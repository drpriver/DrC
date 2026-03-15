//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <string.h>
#include <stddef.h>
#ifdef _WIN32
#include "../Drp/windowsheader.h"
#else
#include <dlfcn.h>
#if defined(__GLIBC__) && !defined(RTLD_DEFAULT)
#define RTLD_DEFAULT ((void *)0)
#endif
#endif
#include "ci_interp.h"
#include "cc_errors.h"
#include "cc_var.h"
#include "cc_expr.h"
#include "cc_target.h"
#include "cpp_preprocessor.h"
#include "native_call.h"
#include "ci_softnum.h"
#include "../Drp/Allocators/allocator.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/MStringBuilder16.h"
#include "../Drp/cmd_run.h"
#include "../Drp/stringview.h"
#include "../Drp/argument_parsing.h"
#include "../Drp/bit_util.h"
#include "../Drp/msb_atomize.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef DEFAULT_UNREACHABLE
#if defined __GNUC__ && !defined __clang__
#define DEFAULT_UNREACHABLE default: __builtin_unreachable()
#elif defined _MSC_VER
#define DEFAULT_UNREACHABLE default: __assume(0)
#else
#define DEFAULT_UNREACHABLE
#endif
#endif

enum {
    CI_NO_ERROR = _cc_no_error,
    CI_OOM_ERROR = _cc_oom_error,
    CI_UNREACHABLE_ERROR = _cc_unreachable_error,
    CI_UNIMPLEMENTED_ERROR = _cc_unimplemented_error,
    CI_RUNTIME_ERROR = _cc_runtime_error,
    CI_INVALID_VALUE_ERROR = _cc_invalid_value_error,
    CI_LIBRARY_NOT_FOUND_ERROR = _cc_file_not_found_error,
    CI_SYMBOL_NOT_FOUND = _cc_symbol_not_found_error,
};
LOG_PRINTF(3, 4) static int ci_error(CiInterpreter*, SrcLoc, const char*, ...);


static char ci_discard_buf[8192];

#define CI_RESULT_TOO_SMALL(ci, loc, sz, size) \
    ci_error((ci), (loc), "interpreter:%s:%d: result buffer too small (need %zu, have %zu)", __FILE__, __LINE__, (size_t)(sz), (size_t)(size))

// SysV x86_64 va_list layout.
typedef struct CiSysvVaListTag CiSysvVaListTag;
struct CiSysvVaListTag {
    unsigned gp_offset;
    unsigned fp_offset;
    void* overflow_arg_area;
    void* reg_save_area;
};

// AAPCS64 (Linux ARM64) va_list layout.
typedef struct CiAapcs64VaList CiAapcs64VaList;
struct CiAapcs64VaList {
    void* __stack;
    void* __gr_top;
    void* __vr_top;
    int __gr_offs; // negative, counts up toward 0
    int __vr_offs;
};
static Allocator ci_allocator(CiInterpreter*);
static Allocator ci_scratch_allocator(CiInterpreter*);

static
void
ci_free_alloca_list(Allocator al, CiAllocaBlock*_Null_unspecified list){
    while(list){
        CiAllocaBlock* next = list->next;
        Allocator_free(al, list, sizeof(CiAllocaBlock) + list->size);
        list = next;
    }
}
static const CcTargetConfig* ci_target(const CiInterpreter*);
static int ci_dlsym(CiInterpreter*, SrcLoc, LongString, const char* what, void*_Nullable*_Nonnull);
static int ci_interp_call(CiInterpreter*, CiInterpFrame* caller, CcFunc*, CcExpr*_Nonnull* _Nonnull args, uint32_t nargs, void* result, size_t size, CiInterpFrame*_Nullable*_Nonnull out_frame);
// re-declare here as I'm not sure if this should be used in the interpreter or not
static int cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_alignof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static _Bool cc_implicit_convertible(CcQualType from, CcQualType to);
static _Bool cc_explicit_castable(CcQualType from, CcQualType to);
// Evaluate an expression as an lvalue, returning pointer to its storage.
static int ci_interp_lvalue(CiInterpreter*, CiInterpFrame*, CcExpr* expr, void*_Nullable*_Nonnull out, size_t* size);
static int cc_parse_expr(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static void cc_release_expr(CcParser* p, CcExpr* e);

static CppFuncMacroFn ci_shell, ci_procmacro_expand;

static
int
ci_error(CiInterpreter* ci, SrcLoc loc, const char* fmt, ...){
    LOCK_T_lock(&ci->error_lock);
    va_list va;
    va_start(va, fmt);
    cpp_msg(&ci->parser.cpp, loc, LOG_PRINT_ERROR, "error", fmt, va);
    va_end(va);
    LOCK_T_unlock(&ci->error_lock);
    return CI_RUNTIME_ERROR;
}
#define ci_unimplemented(p, loc, msg) (ci_error(p, loc, "UNIMPLEMENTED: " msg " at %s:%d", __FILE__, __LINE__), CI_UNIMPLEMENTED_ERROR)

static inline
uint64_t
ci_read_uint(const void* buf, uint32_t sz){
    switch(sz){
        case 1: return *(const uint8_t*)buf;
        case 2: return *(const uint16_t*)buf;
        case 4: return *(const uint32_t*)buf;
        case 8: return *(const uint64_t*)buf;
    }
    uint64_t v = 0;
    memcpy(&v, buf, sz);
    return v;
}

static inline
int64_t
ci_read_int(const void* buf, uint32_t sz){
    switch(sz){
        case 1: return *(const int8_t*)buf;
        case 2: return *(const int16_t*)buf;
        case 4: return *(const int32_t*)buf;
        case 8: return *(const int64_t*)buf;
    }
    uint64_t v = 0;
    memcpy(&v, buf, sz);
    return (int64_t)v;
}

static inline
void
ci_write_uint(void* buf, uint32_t sz, uint64_t val){
    memcpy(buf, &val, sz);
}


static inline
double
ci_read_float(const void* buf, CcBasicTypeKind k){
    if(k == CCBT_float){
        float f;
        memcpy(&f, buf, sizeof f);
        return (double)f;
    }
    double d;
    memcpy(&d, buf, sizeof d);
    return d;
}

static inline
void
ci_write_float(void* buf, CcBasicTypeKind k, double val){
    if(k == CCBT_float){
        float f = (float)val;
        memcpy(buf, &f, sizeof f);
    }
    else {
        memcpy(buf, &val, sizeof val);
    }
}

static
void* _Nullable
ci_var_storage(CiInterpFrame* frame, CcVariable* var){
    if(var->automatic)
        return (char*)(frame + 1) + var->frame_offset;
    return var->interp_val;
}

static
int
ci_ensure_var_storage(CiInterpreter* ci, CcVariable* var){
    if(var->automatic) return 0;
    if(var->interp_val) return 0;
    return ci_error(ci, var->loc, "ICE: variable '%s' storage not resolved before execution", var->name->data);
}

static
int
ci_interp_lvalue(CiInterpreter* ci, CiInterpFrame* frame, CcExpr* expr, void*_Nullable*_Nonnull out, size_t* size){
    uint32_t _type_sz;
    // Incomplete arrays (FLA, zero-length arrays) have unknown size.
    if(ccqt_kind(expr->type) == CC_ARRAY && ccqt_as_array(expr->type)->is_incomplete){
        _type_sz = 0;
    }
    else {
        int _serr = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &_type_sz);
        if(_serr) return _serr;
    }
    *size = _type_sz;
    switch(expr->kind){
        case CC_EXPR_VARIABLE: {
            CcVariable* var = expr->var;
            int err = ci_ensure_var_storage(ci, var);
            if(err) return err;
            void* storage = ci_var_storage(frame, var);
            if(!storage)
                return ci_error(ci, expr->loc, "variable '%s' has no storage", var->name->data);
            *out = storage;
            return 0;
        }
        case CC_EXPR_DEREF: {
            // *ptr: evaluate ptr, return the pointer value
            void* ptr_val = NULL;
            int err = ci_interp_expr(ci, frame,expr->lhs, &ptr_val, sizeof ptr_val);
            if(err) return err;
            *out = ptr_val;
            return 0;
        }
        case CC_EXPR_DOT: {
            // base.member: get lvalue of base, add field offset
            void* base;
            size_t base_size;
            int err = ci_interp_lvalue(ci, frame, expr->values[0], &base, &base_size);
            if(err) return err;
            uint64_t off = expr->field_loc.byte_offset;
            if(off + _type_sz > base_size)
                return ci_error(ci, expr->loc, "field access out of bounds");
            *out = (char*)base + off;
            return 0;
        }
        case CC_EXPR_ARROW: {
            // ptr->member: eval ptr, add field offset
            void* ptr_val = NULL;
            int err = ci_interp_expr(ci, frame,expr->values[0], &ptr_val, sizeof ptr_val);
            if(err) return err;
            *out = (char*)ptr_val + expr->field_loc.byte_offset;
            return 0;
        }
        case CC_EXPR_SUBSCRIPT: {
            // base[index]
            CcExpr* base_expr = expr->lhs;
            CcExpr* idx_expr = expr->values[0];
            CcQualType base_type = base_expr->type;
            uint32_t elem_sz;
            int err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &elem_sz);
            if(err) return err;
            int64_t idx = 0;
            uint32_t idx_sz;
            err = cc_sizeof_as_uint(&ci->parser, idx_expr->type, expr->loc, &idx_sz);
            if(err) return err;
            err = ci_interp_expr(ci, frame,idx_expr, &idx, sizeof idx);
            if(err) return err;
            _Bool idx_unsigned = ccqt_is_basic(idx_expr->type) && ccbt_is_unsigned(idx_expr->type.basic.kind, !ci_target(ci)->char_is_signed);
            if(idx_unsigned)
                idx = (int64_t)ci_read_uint(&idx, idx_sz);
            else
                idx = ci_read_int(&idx, idx_sz);
            if(ccqt_kind(base_type) == CC_ARRAY){
                // Array: get lvalue of base, index into it
                void* base;
                size_t base_size;
                err = ci_interp_lvalue(ci, frame, base_expr, &base, &base_size);
                if(err) return err;
                // Skip bounds check when size is unknown (incomplete/zero-length
                // arrays) or when base is a struct member (FLA patterns).
                _Bool skip_check = !base_size
                    || base_expr->kind == CC_EXPR_ARROW
                    || base_expr->kind == CC_EXPR_DOT;
                if(!skip_check && (idx < 0 || (uint64_t)idx * elem_sz + elem_sz > base_size))
                    return ci_error(ci, expr->loc, "array subscript out of bounds");
                *out = (char*)base + idx * elem_sz;
            }
            else {
                // Pointer: eval base as rvalue
                void* ptr_val = NULL;
                err = ci_interp_expr(ci, frame,base_expr, &ptr_val, sizeof ptr_val);
                if(err) return err;
                *out = (char*)ptr_val + idx * elem_sz;
            }
            return 0;
        }
        case CC_EXPR_COMMA: {
            // Evaluate left side for side effects, then get lvalue of right side.
            // Used by desugared compound literals: (anon = init, anon)
            int err = ci_interp_expr(ci, frame, expr->lhs, ci_discard_buf, sizeof ci_discard_buf);
            if(err) return err;
            return ci_interp_lvalue(ci, frame, expr->values[0], out, size);
        }
        case CC_EXPR_TERNARY: {
            uint64_t cond = 0;
            uint32_t cond_sz;
            int err = cc_sizeof_as_uint(&ci->parser, expr->lhs->type, expr->loc, &cond_sz);
            if(err) return err;
            err = ci_interp_expr(ci, frame, expr->lhs, &cond, sizeof cond);
            if(err) return err;
            cond = ci_read_uint(&cond, cond_sz);
            return ci_interp_lvalue(ci, frame, cond ? expr->values[0] : expr->values[1], out, size);
        }
        case CC_EXPR_VALUE:
            if(ccqt_kind(expr->type) == CC_ARRAY && expr->text){
                *out = (void*)(uintptr_t)expr->text;
                *size = expr->str.length;
                return 0;
            }
            return ci_error(ci, expr->loc, "expression is not an lvalue");
        default:
            return ci_error(ci, expr->loc, "expression is not an lvalue");
    }
}

// Get the address of the storage unit for a bitfield DOT/ARROW expression.
static
int
ci_bitfield_storage_addr(CiInterpreter* ci, CiInterpFrame* frame, CcExpr* expr, void*_Nullable*_Nonnull out){
    if(expr->kind == CC_EXPR_DOT){
        void* base;
        size_t base_size;
        int err = ci_interp_lvalue(ci, frame, expr->values[0], &base, &base_size);
        if(err) return err;
        *out = (char*)base + expr->field_loc.byte_offset;
    }
    else {
        void* ptr_val = NULL;
        int err = ci_interp_expr(ci, frame, expr->values[0], &ptr_val, sizeof ptr_val);
        if(err) return err;
        *out = (char*)ptr_val + expr->field_loc.byte_offset;
    }
    return 0;
}

static inline
uint64_t
ci_bitfield_read(void* storage_addr, uint32_t storage_sz, uint8_t bit_offset, uint8_t bit_width){
    uint64_t storage = 0;
    memcpy(&storage, storage_addr, storage_sz);
    return (storage >> bit_offset) & (((uint64_t)1 << bit_width) - 1);
}

static inline
void
ci_bitfield_write(void* storage_addr, uint32_t storage_sz, uint8_t bit_offset, uint8_t bit_width, uint64_t val){
    uint64_t mask = ((uint64_t)1 << bit_width) - 1;
    uint64_t storage = 0;
    memcpy(&storage, storage_addr, storage_sz);
    storage &= ~(mask << bit_offset);
    storage |= (val & mask) << bit_offset;
    memcpy(storage_addr, &storage, storage_sz);
}

// Userdata for interpreted function closures.
typedef struct CiClosureData CiClosureData;
struct CiClosureData {
    CiInterpreter* ci;
    CcFunc* func;
};

// NativeClosureCallback: called when native code invokes an interpreted function
// through a function pointer (e.g. qsort calling a comparator).
static
void
ci_closure_callback(void* rvalue, void*_Nonnull*_Nonnull args, void* userdata){
    CiClosureData* cd = userdata;
    CiInterpreter* ci = cd->ci;
    CcFunc* func = cd->func;
    CcFunction* ftype = func->type;
    if(!func->parsed){
        ci_error(ci, func->loc, "ICE: Calling function that hasn't been parsed");
        return;
    }
    size_t alloc_size = sizeof(CiInterpFrame) + func->frame_size;
    CiInterpFrame* frame = Allocator_zalloc(ci_allocator(ci), alloc_size);
    if(!frame){
        ci_error(ci, func->loc, "interpreter: OOM allocating frame");
        return;
    }
    uint32_t ret_sz = 0;
    if(!(ccqt_is_basic(ftype->return_type) && ftype->return_type.basic.kind == CCBT_void))
        cc_sizeof_as_uint(&ci->parser, ftype->return_type, func->loc, &ret_sz);
    *frame = (CiInterpFrame){
        .stmts = func->body.data,
        .stmt_count = func->body.count,
        .return_buf = rvalue,
        .return_size = ret_sz,
        .data_length = func->frame_size,
    };
    // Copy raw arg values into param storage.
    for(uint32_t i = 0; i < ftype->param_count; i++){
        CcVariable* var = func->param_vars[i];
        if(!var) continue;
        uint32_t param_sz;
        cc_sizeof_as_uint(&ci->parser, ftype->params[i], func->loc, &param_sz);
        void* storage = (char*)(frame + 1) + var->frame_offset;
        memcpy(storage, args[i], param_sz);
    }
    while(frame->pc < frame->stmt_count){
        int err = ci_interp_step(ci, frame);
        if(err) break;
    }
    ci_free_alloca_list(ci_allocator(ci), frame->alloca_list);
    Allocator_free(ci_allocator(ci), frame, alloc_size);
}

// Create a native closure for an interpreted function, storing the
// resulting function pointer in func->native_func.
static
int
ci_create_closure(CiInterpreter* ci, CcFunc* func){
    #ifdef NO_NATIVE_CALL
        // No native code can call this, so just use the CcFunc* itself as a
        // fake function pointer. The closure_map reverse lookup will catch it
        // at interpreted call sites.
        func->native_func = (void(*)(void))(uintptr_t)func;
    #else
        if(func->type->is_variadic)
            return ci_error(ci, func->loc, "cannot take address of variadic interpreted function");
        CiClosureData* cd = Allocator_zalloc(ci_allocator(ci), sizeof *cd);
        if(!cd) return CI_OOM_ERROR;
        cd->ci = ci;
        cd->func = func;
        NativeClosure* nc = NULL;
        int err = native_closure_create(ci_allocator(ci), func->type, ci_closure_callback, cd, &nc);
        if(err){
            Allocator_free(ci_allocator(ci), cd, sizeof *cd);
            return err;
        }
        func->native_func = native_closure_fn(nc);
        func->native_closure = nc;
    #endif
    int e = BPM_put(&ci->closure_map, ci_allocator(ci), func, (void*)func->native_func);
    if(e) return e == 1 ? CI_OOM_ERROR : CI_RUNTIME_ERROR;
    return 0;
}

static
int
ci_interp_expr(CiInterpreter* ci, CiInterpFrame* frame, CcExpr* expr, void* result, size_t size){
    switch(expr->kind){
    case CC_EXPR_VALUE: {
        if(result == ci_discard_buf) return 0;
        uint32_t sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(ccqt_kind(expr->type) == CC_ARRAY){
            if(sz > size)
                return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
            uint32_t len = expr->str.length;
            if(len > sz) len = sz;
            memcpy(result, expr->text, len);
            return 0;
        }
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        memcpy(result, &expr->uinteger, sz);
        return 0;
    }
    case CC_EXPR_VARIABLE: {
        if(result == ci_discard_buf) return 0;
        CcVariable* var = expr->var;
        int err = ci_ensure_var_storage(ci, var);
        if(err) return err;
        void* storage = ci_var_storage(frame, var);
        if(!storage)
            return ci_error(ci, expr->loc, "variable '%s' has no storage", var->name->data);
        CcQualType var_type = var->type;
        // Array variables decay to pointer (but not vectors)
        if(ccqt_kind(var_type) == CC_ARRAY && !ccqt_as_array(var_type)->is_vector){
            if(sizeof storage > size)
                return CI_RESULT_TOO_SMALL(ci, expr->loc, sizeof storage, size);
            memcpy(result, &storage, sizeof storage);
            return 0;
        }
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, var_type, expr->loc, &sz);
        if(err) return err;
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, 0, size);
        memcpy(result, storage, sz);
        return 0;
    }
    case CC_EXPR_FUNCTION: {
        if(result == ci_discard_buf) return 0;
        CcFunc* func = expr->func;
        if(!func->native_func)
            return ci_error(ci, expr->loc, "ICE: function '%s' not resolved before execution",
                func->name ? func->name->data : "<unknown>");
        void (*fn)(void) = func->native_func;
        if(sizeof fn > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, 0, size);
        memcpy(result, &fn, sizeof fn);
        return 0;
    }
    case CC_EXPR_CAST: {
        CcExpr* operand = expr->lhs;
        CcQualType from = operand->type;
        CcQualType to = expr->type;
        if(ccqt_is_basic(to) && to.basic.kind == CCBT_void){
            return ci_interp_expr(ci, frame, operand, ci_discard_buf, sizeof ci_discard_buf);
        }
        // Function-to-pointer decay: the function expression already
        // produces the function pointer value.
        if(ccqt_kind(from) == CC_FUNCTION){
            return ci_interp_expr(ci, frame, operand, result, size);
        }
        // Qualifier-only cast (e.g., const T -> T): pass through directly.
        if((from.bits & ~(uintptr_t)7) == (to.bits & ~(uintptr_t)7)){
            return ci_interp_expr(ci, frame, operand, result, size);
        }
        // Array-to-pointer decay: get address of array data (not vectors).
        if(ccqt_kind(from) == CC_ARRAY && !ccqt_as_array(from)->is_vector){
            if(result == ci_discard_buf) return 0;
            if(sizeof(void*) > size)
                return CI_RESULT_TOO_SMALL(ci, expr->loc, sizeof(void*), size);
            void* ptr;
            size_t lval_size;
            int err = ci_interp_lvalue(ci, frame, operand, &ptr, &lval_size);
            if(err) return err;
            memcpy(result, &ptr, sizeof ptr);
            return 0;
        }
        if(!ccqt_is_basic(from) && ccqt_kind(from) != CC_POINTER && ccqt_kind(from) != CC_ENUM)
            return ci_error(ci, expr->loc, "interpreter:%s:%d: cast from non-scalar type", __FILE__, __LINE__);
        uint32_t from_sz;
        int err = cc_sizeof_as_uint(&ci->parser, from, expr->loc, &from_sz);
        if(err) return err;
        uint32_t to_sz;
        err = cc_sizeof_as_uint(&ci->parser, to, expr->loc, &to_sz);
        if(err) return err;
        CiUint128 val128 = {0};
        void* valp = &val128;
        size_t val_cap = sizeof val128;
        err = ci_interp_expr(ci, frame, operand, valp, val_cap);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        if(to_sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, to_sz, size);
        _Bool from_is_float = ccqt_is_basic(from) && ccbt_is_float(from.basic.kind);
        _Bool to_is_float = ccqt_is_basic(to) && ccbt_is_float(to.basic.kind);
        if(from_is_float && to_is_float){
            double d = ci_read_float(valp, from.basic.kind);
            ci_write_float(result, to.basic.kind, d);
        }
        else if(from_is_float && !to_is_float){
            double d = ci_read_float(valp, from.basic.kind);
            if(to_sz > 8){
                // float to 128-bit int
                CiUint128 v;
                if(ccqt_is_basic(to) && ccbt_is_unsigned(to.basic.kind, !ci_target(ci)->char_is_signed))
                    v = ci_uint128_from_uint64((uint64_t)d);
                else
                    v = ci_uint128_from_int64((int64_t)d);
                ci_uint128_write(result, to_sz, v);
            }
            else {
                if(ccqt_is_basic(to) && ccbt_is_unsigned(to.basic.kind, !ci_target(ci)->char_is_signed))
                    ci_write_uint(result, to_sz, (uint64_t)d);
                else
                    ci_write_uint(result, to_sz, (uint64_t)(int64_t)d);
            }
        }
        else if(!from_is_float && to_is_float){
            _Bool from_unsigned = ccqt_is_basic(from) && ccbt_is_unsigned(from.basic.kind, !ci_target(ci)->char_is_signed);
            double d;
            if(from_sz > 8){
                CiUint128 v;
                ci_uint128_read(&v, valp, from_sz);
                d = (double)ci_uint128_lo(v); // best effort
            }
            else if(from_unsigned)
                d = (double)ci_read_uint(valp, from_sz);
            else
                d = (double)ci_read_int(valp, from_sz);
            ci_write_float(result, to.basic.kind, d);
        }
        else {
            if(from_sz > 8 || to_sz > 8){
                // 128-bit path
                CiUint128 v;
                ci_uint128_read(&v, valp, from_sz);
                if(from_sz <= 8){
                    _Bool from_unsigned = ccqt_is_basic(from) && ccbt_is_unsigned(from.basic.kind, !ci_target(ci)->char_is_signed);
                    if(from_unsigned)
                        v = ci_uint128_from_uint64(ci_read_uint(valp, from_sz));
                    else
                        v = ci_uint128_from_int64(ci_read_int(valp, from_sz));
                }
                if(to_sz <= 8){
                    ci_write_uint(result, to_sz, ci_uint128_lo(v));
                }
                else {
                    ci_uint128_write(result, to_sz, v);
                }
            }
            else {
                _Bool from_unsigned = ccqt_is_basic(from) && ccbt_is_unsigned(from.basic.kind, !ci_target(ci)->char_is_signed);
                if(from_unsigned)
                    ci_write_uint(result, to_sz, ci_read_uint(valp, from_sz));
                else
                    ci_write_uint(result, to_sz, (uint64_t)ci_read_int(valp, from_sz));
            }
        }
        return 0;
    }
    case CC_EXPR_ASSIGN: {
        CcExpr* lhs = expr->lhs;
        // Bitfield assignment: read-modify-write on the storage unit.
        if((lhs->kind == CC_EXPR_DOT || lhs->kind == CC_EXPR_ARROW) && lhs->field_loc.bit_width){
            void* storage_addr;
            int err = ci_bitfield_storage_addr(ci, frame, lhs, &storage_addr);
            if(err) return err;
            uint32_t sz;
            err = cc_sizeof_as_uint(&ci->parser, lhs->type, expr->loc, &sz);
            if(err) return err;
            uint64_t rval = 0;
            err = ci_interp_expr(ci, frame, expr->values[0], &rval, sizeof rval);
            if(err) return err;
            rval = ci_read_uint(&rval, sz);
            ci_bitfield_write(storage_addr, sz, lhs->field_loc.bit_offset, lhs->field_loc.bit_width, rval);
            if(result == ci_discard_buf) return 0;
            uint64_t out = rval & (((uint64_t)1 << lhs->field_loc.bit_width) - 1);
            memset(result, 0, size);
            memcpy(result, &out, sz < size ? sz : size);
            return 0;
        }
        void* lval;
        size_t lval_size;
        int err = ci_interp_lvalue(ci, frame, lhs, &lval, &lval_size);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(sz > lval_size)
            return ci_error(ci, expr->loc, "interpreter: assignment exceeds lvalue storage");
        err = ci_interp_expr(ci, frame,expr->values[0], lval, lval_size);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        memcpy(result, lval, sz);
        return 0;
    }
    case CC_EXPR_COMMA: {
        int err = ci_interp_expr(ci, frame, expr->lhs, ci_discard_buf, sizeof ci_discard_buf);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        return ci_interp_expr(ci, frame,expr->values[0], result, size);
    }
    case CC_EXPR_TERNARY: {
        uint64_t cond = 0;
        uint32_t cond_sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->lhs->type, expr->loc, &cond_sz);
        if(err) return err;
        err = ci_interp_expr(ci, frame,expr->lhs, &cond, sizeof cond);
        if(err) return err;
        if(ci_read_uint(&cond, cond_sz))
            return ci_interp_expr(ci, frame,expr->values[0], result, size);
        else
            return ci_interp_expr(ci, frame,expr->values[1], result, size);
    }
    case CC_EXPR_ADDR: {
        void* lval;
        size_t lval_size;
        int err = ci_interp_lvalue(ci, frame, expr->lhs, &lval, &lval_size);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        if(sizeof lval > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sizeof lval, size);
        memcpy(result, &lval, sizeof lval);
        return 0;
    }
    case CC_EXPR_DEREF: {
        void* ptr_val = NULL;
        int err = ci_interp_expr(ci, frame,expr->lhs, &ptr_val, sizeof ptr_val);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        memcpy(result, ptr_val, sz);
        return 0;
    }
    case CC_EXPR_DOT: {
        if(result == ci_discard_buf) return 0;
        uint64_t off = expr->field_loc.byte_offset;
        uint32_t sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        CcExpr* base_expr = expr->values[0];
        if(!expr->is_lvalue){
            uint32_t base_sz;
            err = cc_sizeof_as_uint(&ci->parser, base_expr->type, expr->loc, &base_sz);
            if(err) return err;
            char* temp = Allocator_alloc(ci_allocator(ci), base_sz);
            if(!temp) return CI_OOM_ERROR;
            err = ci_interp_expr(ci, frame, base_expr, temp, base_sz);
            if(err){ Allocator_free(ci_allocator(ci), temp, base_sz); return err; }
            if(expr->field_loc.bit_width){
                uint64_t val = ci_bitfield_read(temp + off, sz, expr->field_loc.bit_offset, expr->field_loc.bit_width);
                memset(result, 0, size);
                memcpy(result, &val, sz < size ? sz : size);
            }
            else {
                memcpy(result, temp + off, sz);
            }
            Allocator_free(ci_allocator(ci), temp, base_sz);
            return 0;
        }
        void* base;
        size_t base_size;
        err = ci_interp_lvalue(ci, frame, base_expr, &base, &base_size);
        if(err) return err;
        if(off + sz > base_size)
            return ci_error(ci, expr->loc, "interpreter: field access out of bounds");
        if(expr->field_loc.bit_width){
            uint64_t val = ci_bitfield_read((char*)base + off, sz, expr->field_loc.bit_offset, expr->field_loc.bit_width);
            memset(result, 0, size);
            memcpy(result, &val, sz < size ? sz : size);
        }
        else {
            memcpy(result, (char*)base + off, sz);
        }
        return 0;
    }
    case CC_EXPR_ARROW: {
        void* ptr_val = NULL;
        int err = ci_interp_expr(ci, frame,expr->values[0], &ptr_val, sizeof ptr_val);
        if(err) return err;
        uint64_t off = expr->field_loc.byte_offset;
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        if(expr->field_loc.bit_width){
            uint64_t val = ci_bitfield_read((char*)ptr_val + off, sz, expr->field_loc.bit_offset, expr->field_loc.bit_width);
            memset(result, 0, size);
            memcpy(result, &val, sz < size ? sz : size);
        }
        else {
            memcpy(result, (char*)ptr_val + off, sz);
        }
        return 0;
    }
    case CC_EXPR_SUBSCRIPT: {
        void* addr;
        size_t addr_size;
        int err = ci_interp_lvalue(ci, frame, expr, &addr, &addr_size);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        memcpy(result, addr, sz);
        return 0;
    }
    case CC_EXPR_NEG: {
        uint64_t val = 0;
        uint32_t sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        err = ci_interp_expr(ci, frame, expr->lhs, &val, sizeof val);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        if(ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind)){
            double d = -ci_read_float(&val, expr->type.basic.kind);
            ci_write_float(result, expr->type.basic.kind, d);
        }
        else {
            int64_t v = -ci_read_int(&val, sz);
            ci_write_uint(result, sz, (uint64_t)v);
        }
        return 0;
    }
    case CC_EXPR_POS: {
        uint32_t sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        return ci_interp_expr(ci, frame,expr->lhs, result, size);
    }
    case CC_EXPR_BITNOT: {
        uint64_t val = 0;
        uint32_t sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        err = ci_interp_expr(ci, frame,expr->lhs, &val, sizeof val);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        uint64_t v = ~ci_read_uint(&val, sz);
        ci_write_uint(result, sz, v);
        return 0;
    }
    case CC_EXPR_LOGNOT: {
        uint64_t val = 0;
        uint32_t sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->lhs->type, expr->loc, &sz);
        if(err) return err;
        err = ci_interp_expr(ci, frame,expr->lhs, &val, sizeof val);
        if(err) return err;
        _Bool v = !ci_read_uint(&val, sz);
        uint32_t rsz;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &rsz);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        if(rsz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        ci_write_uint(result, rsz, v);
        return 0;
    }
    case CC_EXPR_PREINC:
    case CC_EXPR_PREDEC:
    case CC_EXPR_POSTINC:
    case CC_EXPR_POSTDEC: {
        CcExpr* lhs = expr->lhs;
        _Bool is_pre = (expr->kind == CC_EXPR_PREINC || expr->kind == CC_EXPR_PREDEC);
        _Bool is_inc = (expr->kind == CC_EXPR_PREINC || expr->kind == CC_EXPR_POSTINC);
        // Bitfield inc/dec
        if((lhs->kind == CC_EXPR_DOT || lhs->kind == CC_EXPR_ARROW) && lhs->field_loc.bit_width){
            void* storage_addr;
            int err = ci_bitfield_storage_addr(ci, frame, lhs, &storage_addr);
            if(err) return err;
            uint32_t sz;
            err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
            if(err) return err;
            uint64_t v = ci_bitfield_read(storage_addr, sz, lhs->field_loc.bit_offset, lhs->field_loc.bit_width);
            uint64_t old = v;
            v += is_inc ? 1 : (uint64_t)-1;
            ci_bitfield_write(storage_addr, sz, lhs->field_loc.bit_offset, lhs->field_loc.bit_width, v);
            if(result == ci_discard_buf) return 0;
            uint64_t out = is_pre ? v : old;
            out &= ((uint64_t)1 << lhs->field_loc.bit_width) - 1;
            memset(result, 0, size);
            memcpy(result, &out, sz < size ? sz : size);
            return 0;
        }
        void* lval;
        size_t lval_size;
        int err = ci_interp_lvalue(ci, frame, lhs, &lval, &lval_size);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(sz > lval_size)
            return ci_error(ci, expr->loc, "interpreter: write exceeds lvalue storage");
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        _Bool is_float = ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind);
        if(is_float){
            double d = ci_read_float(lval, expr->type.basic.kind);
            double old = d;
            d += is_inc ? 1.0 : -1.0;
            ci_write_float(lval, expr->type.basic.kind, d);
            if(result == ci_discard_buf) return 0;
            ci_write_float(result, expr->type.basic.kind, is_pre ? d : old);
        }
        else if(ccqt_kind(expr->type) == CC_POINTER){
            CcPointer* pt = ccqt_as_ptr(expr->type);
            uint32_t pointee_sz;
            err = cc_sizeof_as_uint(&ci->parser, pt->pointee, expr->loc, &pointee_sz);
            if(err) return err;
            void* ptr = NULL;
            memcpy(&ptr, lval, sizeof ptr);
            void* old = ptr;
            ptr = (char*)ptr + (is_inc ? (int)pointee_sz : -(int)pointee_sz);
            memcpy(lval, &ptr, sizeof ptr);
            if(result == ci_discard_buf) return 0;
            void* out = is_pre ? ptr : old;
            memcpy(result, &out, sizeof out);
        }
        else {
            uint64_t v = ci_read_uint(lval, sz);
            uint64_t old = v;
            v += is_inc ? 1 : (uint64_t)-1;
            ci_write_uint(lval, sz, v);
            if(result == ci_discard_buf) return 0;
            ci_write_uint(result, sz, is_pre ? v : old);
        }
        return 0;
    }
    case CC_EXPR_ADD: case CC_EXPR_SUB:
    case CC_EXPR_MUL: case CC_EXPR_DIV: case CC_EXPR_MOD:
    case CC_EXPR_BITAND: case CC_EXPR_BITOR: case CC_EXPR_BITXOR:
    case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:
    case CC_EXPR_EQ: case CC_EXPR_NE:
    case CC_EXPR_LT: case CC_EXPR_GT:
    case CC_EXPR_LE: case CC_EXPR_GE:
    case CC_EXPR_LOGAND: case CC_EXPR_LOGOR: {
        CcExpr* lhs = expr->lhs;
        CcExpr* rhs = expr->values[0];
        CiUint128 lbuf128 = {0}, rbuf128 = {0};
        uint32_t lsz, rsz, result_sz;
        int err = cc_sizeof_as_uint(&ci->parser, lhs->type, expr->loc, &lsz);
        if(err) return err;
        err = cc_sizeof_as_uint(&ci->parser, rhs->type, expr->loc, &rsz);
        if(err) return err;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &result_sz);
        if(err) return err;
        if(result_sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, result_sz, size);
        if(expr->kind == CC_EXPR_LOGAND){
            err = ci_interp_expr(ci, frame,lhs, &lbuf128, sizeof lbuf128);
            if(err) return err;
            _Bool lnz;
            if(lsz > 8){ CiUint128 v; ci_uint128_read(&v, &lbuf128, lsz); lnz = ci_uint128_nonzero(v); }
            else lnz = ci_read_uint(&lbuf128, lsz) != 0;
            if(!lnz){
                ci_write_uint(result, result_sz, 0);
                return 0;
            }
            err = ci_interp_expr(ci, frame,rhs, &rbuf128, sizeof rbuf128);
            if(err) return err;
            _Bool rnz;
            if(rsz > 8){ CiUint128 v; ci_uint128_read(&v, &rbuf128, rsz); rnz = ci_uint128_nonzero(v); }
            else rnz = ci_read_uint(&rbuf128, rsz) != 0;
            ci_write_uint(result, result_sz, rnz ? 1 : 0);
            return 0;
        }
        if(expr->kind == CC_EXPR_LOGOR){
            err = ci_interp_expr(ci, frame,lhs, &lbuf128, sizeof lbuf128);
            if(err) return err;
            _Bool lnz;
            if(lsz > 8){ CiUint128 v; ci_uint128_read(&v, &lbuf128, lsz); lnz = ci_uint128_nonzero(v); }
            else lnz = ci_read_uint(&lbuf128, lsz) != 0;
            if(lnz){
                ci_write_uint(result, result_sz, 1);
                return 0;
            }
            err = ci_interp_expr(ci, frame,rhs, &rbuf128, sizeof rbuf128);
            if(err) return err;
            _Bool rnz;
            if(rsz > 8){ CiUint128 v; ci_uint128_read(&v, &rbuf128, rsz); rnz = ci_uint128_nonzero(v); }
            else rnz = ci_read_uint(&rbuf128, rsz) != 0;
            ci_write_uint(result, result_sz, rnz ? 1 : 0);
            return 0;
        }
        err = ci_interp_expr(ci, frame,lhs, &lbuf128, sizeof lbuf128);
        if(err) return err;
        err = ci_interp_expr(ci, frame,rhs, &rbuf128, sizeof rbuf128);
        if(err) return err;
        _Bool lhs_ptr = ccqt_is_pointer_like(lhs->type);
        _Bool rhs_ptr = ccqt_is_pointer_like(rhs->type);
        if(lhs_ptr || rhs_ptr){
            if(expr->kind == CC_EXPR_ADD && lhs_ptr && !rhs_ptr){
                CcQualType pointee;
                if(ccqt_kind(lhs->type) == CC_POINTER)
                    pointee = ccqt_as_ptr(lhs->type)->pointee;
                else
                    pointee = ccqt_as_array(lhs->type)->element;
                uint32_t elem_sz;
                err = cc_sizeof_as_uint(&ci->parser, pointee, expr->loc, &elem_sz);
                if(err) return err;
                char* ptr = NULL;
                memcpy(&ptr, &lbuf128, sizeof ptr);
                int64_t idx = ci_read_int(&rbuf128, rsz);
                ptr += idx * elem_sz;
                memcpy(result, &ptr, sizeof ptr);
                return 0;
            }
            if(expr->kind == CC_EXPR_ADD && !lhs_ptr && rhs_ptr){
                CcQualType pointee;
                if(ccqt_kind(rhs->type) == CC_POINTER)
                    pointee = ccqt_as_ptr(rhs->type)->pointee;
                else
                    pointee = ccqt_as_array(rhs->type)->element;
                uint32_t elem_sz;
                err = cc_sizeof_as_uint(&ci->parser, pointee, expr->loc, &elem_sz);
                if(err) return err;
                char* ptr = NULL;
                memcpy(&ptr, &rbuf128, sizeof ptr);
                int64_t idx = ci_read_int(&lbuf128, lsz);
                ptr += idx * elem_sz;
                memcpy(result, &ptr, sizeof ptr);
                return 0;
            }
            if(expr->kind == CC_EXPR_SUB && lhs_ptr && !rhs_ptr){
                CcQualType pointee;
                if(ccqt_kind(lhs->type) == CC_POINTER)
                    pointee = ccqt_as_ptr(lhs->type)->pointee;
                else
                    pointee = ccqt_as_array(lhs->type)->element;
                uint32_t elem_sz;
                err = cc_sizeof_as_uint(&ci->parser, pointee, expr->loc, &elem_sz);
                if(err) return err;
                char* ptr = NULL;
                memcpy(&ptr, &lbuf128, sizeof ptr);
                int64_t idx = ci_read_int(&rbuf128, rsz);
                ptr -= idx * elem_sz;
                memcpy(result, &ptr, sizeof ptr);
                return 0;
            }
            if(expr->kind == CC_EXPR_SUB && lhs_ptr && rhs_ptr){
                CcQualType pointee;
                if(ccqt_kind(lhs->type) == CC_POINTER)
                    pointee = ccqt_as_ptr(lhs->type)->pointee;
                else
                    pointee = ccqt_as_array(lhs->type)->element;
                uint32_t elem_sz;
                err = cc_sizeof_as_uint(&ci->parser, pointee, expr->loc, &elem_sz);
                if(err) return err;
                char* lp = NULL, *rp = NULL;
                memcpy(&lp, &lbuf128, sizeof lp);
                memcpy(&rp, &rbuf128, sizeof rp);
                int64_t diff = (lp - rp) / (int64_t)elem_sz;
                ci_write_uint(result, result_sz, (uint64_t)diff);
                return 0;
            }
            char* lp = NULL, *rp = NULL;
            memcpy(&lp, &lbuf128, sizeof lp);
            memcpy(&rp, &rbuf128, sizeof rp);
            _Bool cmp;
            switch(expr->kind){
                case CC_EXPR_EQ: cmp = lp == rp; break;
                case CC_EXPR_NE: cmp = lp != rp; break;
                case CC_EXPR_LT: cmp = lp <  rp; break;
                case CC_EXPR_GT: cmp = lp >  rp; break;
                case CC_EXPR_LE: cmp = lp <= rp; break;
                case CC_EXPR_GE: cmp = lp >= rp; break;
                default:
                    return ci_error(ci, expr->loc, "unsupported pointer operation");
            }
            ci_write_uint(result, result_sz, cmp);
            return 0;
        }
        _Bool is_float = ccqt_is_basic(lhs->type) && ccbt_is_float(lhs->type.basic.kind);
        if(is_float){
            double ld = ci_read_float(&lbuf128, lhs->type.basic.kind);
            double rd = ci_read_float(&rbuf128, rhs->type.basic.kind);
            double res;
            switch(expr->kind){
                case CC_EXPR_ADD: res = ld + rd; break;
                case CC_EXPR_SUB: res = ld - rd; break;
                case CC_EXPR_MUL: res = ld * rd; break;
                case CC_EXPR_DIV: res = ld / rd; break;
                default: {
                    _Bool cmp;
                    switch(expr->kind){
                        case CC_EXPR_EQ: cmp = ld == rd; break;
                        case CC_EXPR_NE: cmp = ld != rd; break;
                        case CC_EXPR_LT: cmp = ld <  rd; break;
                        case CC_EXPR_GT: cmp = ld >  rd; break;
                        case CC_EXPR_LE: cmp = ld <= rd; break;
                        case CC_EXPR_GE: cmp = ld >= rd; break;
                        default:
                            return ci_error(ci, expr->loc, "unsupported float operation");
                    }
                    ci_write_uint(result, result_sz, cmp);
                    return 0;
                }
            }
            if(ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind))
                ci_write_float(result, expr->type.basic.kind, res);
            else
                ci_write_uint(result, result_sz, (uint64_t)(int64_t)res);
            return 0;
        }
        _Bool is_unsigned = ccqt_is_basic(lhs->type) && ccbt_is_unsigned(lhs->type.basic.kind, !ci_target(ci)->char_is_signed);
        if(lsz > 8 || rsz > 8 || result_sz > 8){
            // 128-bit integer path
            CiUint128 lu, ru;
            if(is_unsigned){
                ci_uint128_read(&lu, &lbuf128, lsz);
                ci_uint128_read(&ru, &rbuf128, rsz);
            }
            else {
                if(lsz <= 8)
                    lu = ci_uint128_from_int64(ci_read_int(&lbuf128, lsz));
                else
                    ci_uint128_read(&lu, &lbuf128, lsz);
                if(rsz <= 8)
                    ru = ci_uint128_from_int64(ci_read_int(&rbuf128, rsz));
                else
                    ci_uint128_read(&ru, &rbuf128, rsz);
            }
            CiUint128 res;
            switch(expr->kind){
                case CC_EXPR_ADD: res = ci_uint128_add(lu, ru); break;
                case CC_EXPR_SUB: res = ci_uint128_sub(lu, ru); break;
                case CC_EXPR_MUL: res = ci_uint128_mul(lu, ru); break;
                case CC_EXPR_DIV:
                    if(is_unsigned)
                        res = ci_uint128_div(lu, ru);
                    else
                        res = ci_uint128_div(lu, ru); // TODO: signed 128-bit division
                    break;
                case CC_EXPR_MOD:
                    if(is_unsigned)
                        res = ci_uint128_mod(lu, ru);
                    else
                        res = ci_uint128_mod(lu, ru); // TODO: signed 128-bit modulo
                    break;
                case CC_EXPR_BITAND: res = ci_uint128_and(lu, ru); break;
                case CC_EXPR_BITOR:  res = ci_uint128_or(lu, ru); break;
                case CC_EXPR_BITXOR: res = ci_uint128_xor(lu, ru); break;
                case CC_EXPR_LSHIFT: res = ci_uint128_shl(lu, ci_uint128_lo(ru)); break;
                case CC_EXPR_RSHIFT: res = ci_uint128_shr(lu, ci_uint128_lo(ru)); break;
                case CC_EXPR_EQ: ci_write_uint(result, result_sz, ci_uint128_eq(lu, ru)); return 0;
                case CC_EXPR_NE: ci_write_uint(result, result_sz, ci_uint128_ne(lu, ru)); return 0;
                case CC_EXPR_LT: ci_write_uint(result, result_sz, ci_uint128_lt(lu, ru)); return 0;
                case CC_EXPR_GT: ci_write_uint(result, result_sz, ci_uint128_gt(lu, ru)); return 0;
                case CC_EXPR_LE: ci_write_uint(result, result_sz, ci_uint128_le(lu, ru)); return 0;
                case CC_EXPR_GE: ci_write_uint(result, result_sz, ci_uint128_ge(lu, ru)); return 0;
                default: res = ci_uint128_from_uint64(0); break;
            }
            ci_uint128_write(result, result_sz, res);
            return 0;
        }
        uint64_t lu, ru;
        if(is_unsigned){
            lu = ci_read_uint(&lbuf128, lsz);
            ru = ci_read_uint(&rbuf128, rsz);
        }
        else {
            lu = (uint64_t)ci_read_int(&lbuf128, lsz);
            ru = (uint64_t)ci_read_int(&rbuf128, rsz);
        }
        uint64_t res;
        switch(expr->kind){
            case CC_EXPR_ADD: res = lu + ru; break;
            case CC_EXPR_SUB: res = lu - ru; break;
            case CC_EXPR_MUL: res = lu * ru; break;
            case CC_EXPR_DIV:
                if(is_unsigned)
                    res = ru ? lu / ru : 0;
                else
                    res = ru ? (uint64_t)((int64_t)lu / (int64_t)ru) : 0;
                break;
            case CC_EXPR_MOD:
                if(is_unsigned)
                    res = ru ? lu % ru : 0;
                else
                    res = ru ? (uint64_t)((int64_t)lu % (int64_t)ru) : 0;
                break;
            case CC_EXPR_BITAND:  res = lu & ru; break;
            case CC_EXPR_BITOR:   res = lu | ru; break;
            case CC_EXPR_BITXOR:  res = lu ^ ru; break;
            case CC_EXPR_LSHIFT:  res = lu << ru; break;
            case CC_EXPR_RSHIFT:
                if(is_unsigned)
                    res = lu >> ru;
                else
                    res = (uint64_t)((int64_t)lu >> ru);
                break;
            case CC_EXPR_EQ: res = lu == ru; break;
            case CC_EXPR_NE: res = lu != ru; break;
            case CC_EXPR_LT:
                res = is_unsigned ? (lu < ru) : ((int64_t)lu < (int64_t)ru);
                break;
            case CC_EXPR_GT:
                res = is_unsigned ? (lu > ru) : ((int64_t)lu > (int64_t)ru);
                break;
            case CC_EXPR_LE:
                res = is_unsigned ? (lu <= ru) : ((int64_t)lu <= (int64_t)ru);
                break;
            case CC_EXPR_GE:
                res = is_unsigned ? (lu >= ru) : ((int64_t)lu >= (int64_t)ru);
                break;
            default: res = 0; break;
        }
        ci_write_uint(result, result_sz, res);
        return 0;
    }
    case CC_EXPR_ADDASSIGN: case CC_EXPR_SUBASSIGN:
    case CC_EXPR_MULASSIGN: case CC_EXPR_DIVASSIGN: case CC_EXPR_MODASSIGN:
    case CC_EXPR_BITANDASSIGN: case CC_EXPR_BITORASSIGN: case CC_EXPR_BITXORASSIGN:
    case CC_EXPR_LSHIFTASSIGN: case CC_EXPR_RSHIFTASSIGN: {
        CcExpr* lhs = expr->lhs;
        // Bitfield compound assignment
        if((lhs->kind == CC_EXPR_DOT || lhs->kind == CC_EXPR_ARROW) && lhs->field_loc.bit_width){
            void* storage_addr;
            int err = ci_bitfield_storage_addr(ci, frame, lhs, &storage_addr);
            if(err) return err;
            uint32_t sz;
            err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
            if(err) return err;
            uint64_t rbuf = 0;
            uint32_t rsz;
            err = cc_sizeof_as_uint(&ci->parser, expr->values[0]->type, expr->loc, &rsz);
            if(err) return err;
            err = ci_interp_expr(ci, frame, expr->values[0], &rbuf, sizeof rbuf);
            if(err) return err;
            uint64_t lu = ci_bitfield_read(storage_addr, sz, lhs->field_loc.bit_offset, lhs->field_loc.bit_width);
            uint64_t ru = ci_read_uint(&rbuf, rsz);
            uint64_t res;
            switch(expr->kind){
                case CC_EXPR_ADDASSIGN:    res = lu + ru; break;
                case CC_EXPR_SUBASSIGN:    res = lu - ru; break;
                case CC_EXPR_MULASSIGN:    res = lu * ru; break;
                case CC_EXPR_DIVASSIGN:    res = ru ? lu / ru : 0; break;
                case CC_EXPR_MODASSIGN:    res = ru ? lu % ru : 0; break;
                case CC_EXPR_BITANDASSIGN: res = lu & ru; break;
                case CC_EXPR_BITORASSIGN:  res = lu | ru; break;
                case CC_EXPR_BITXORASSIGN: res = lu ^ ru; break;
                case CC_EXPR_LSHIFTASSIGN: res = lu << ru; break;
                case CC_EXPR_RSHIFTASSIGN: res = lu >> ru; break;
                default: res = 0; break;
            }
            ci_bitfield_write(storage_addr, sz, lhs->field_loc.bit_offset, lhs->field_loc.bit_width, res);
            if(result == ci_discard_buf) return 0;
            res &= ((uint64_t)1 << lhs->field_loc.bit_width) - 1;
            memset(result, 0, size);
            memcpy(result, &res, sz < size ? sz : size);
            return 0;
        }
        void* lval;
        size_t lval_size;
        int err = ci_interp_lvalue(ci, frame, lhs, &lval, &lval_size);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(sz > lval_size)
            return ci_error(ci, expr->loc, "interpreter: write exceeds lvalue storage");
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        uint64_t rbuf = 0;
        uint32_t rsz;
        err = cc_sizeof_as_uint(&ci->parser, expr->values[0]->type, expr->loc, &rsz);
        if(err) return err;
        err = ci_interp_expr(ci, frame,expr->values[0], &rbuf, sizeof rbuf);
        if(err) return err;

        _Bool is_float = ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind);
        if(is_float){
            double ld = ci_read_float(lval, expr->type.basic.kind);
            double rd = ci_read_float(&rbuf, expr->values[0]->type.basic.kind);
            double res;
            switch(expr->kind){
                case CC_EXPR_ADDASSIGN: res = ld + rd; break;
                case CC_EXPR_SUBASSIGN: res = ld - rd; break;
                case CC_EXPR_MULASSIGN: res = ld * rd; break;
                case CC_EXPR_DIVASSIGN: res = rd != 0.0 ? ld / rd : 0.0; break;
                default: return ci_error(ci, expr->loc, "unsupported float compound assignment");
            }
            ci_write_float(lval, expr->type.basic.kind, res);
            if(result == ci_discard_buf) return 0;
            ci_write_float(result, expr->type.basic.kind, res);
        }
        else {
            uint64_t lu = ci_read_uint(lval, sz);
            uint64_t ru = ci_read_uint(&rbuf, rsz);
            uint64_t res;
            switch(expr->kind){
                case CC_EXPR_ADDASSIGN:    res = lu + ru; break;
                case CC_EXPR_SUBASSIGN:    res = lu - ru; break;
                case CC_EXPR_MULASSIGN:    res = lu * ru; break;
                case CC_EXPR_DIVASSIGN:    res = ru ? lu / ru : 0; break;
                case CC_EXPR_MODASSIGN:    res = ru ? lu % ru : 0; break;
                case CC_EXPR_BITANDASSIGN: res = lu & ru; break;
                case CC_EXPR_BITORASSIGN:  res = lu | ru; break;
                case CC_EXPR_BITXORASSIGN: res = lu ^ ru; break;
                case CC_EXPR_LSHIFTASSIGN: res = lu << ru; break;
                case CC_EXPR_RSHIFTASSIGN: res = lu >> ru; break;
                default: res = 0; break;
            }
            ci_write_uint(lval, sz, res);
            if(result == ci_discard_buf) return 0;
            ci_write_uint(result, sz, res);
        }
        return 0;
    }
    case CC_EXPR_CALL: {
        CcExpr* callee = expr->lhs;
        uint32_t nargs = expr->call.nargs;
        void (*fn)(void) = NULL;
        CcFunction* ftype;
        // Direct call to a known function.
        if(callee->kind == CC_EXPR_FUNCTION){
            CcFunc* func = callee->func;
            ftype = func->type;
            if(func->defined){
                CiInterpFrame* callee_frame = NULL;
                int err = ci_interp_call(ci, frame, func, expr->values, nargs, result, size, &callee_frame);
                if(err) return err;
                while(callee_frame->pc < callee_frame->stmt_count){
                    err = ci_interp_step(ci, callee_frame);
                    if(err){
                        ci_free_alloca_list(ci_allocator(ci), callee_frame->alloca_list);
                        Allocator_free(ci_allocator(ci), callee_frame, sizeof(CiInterpFrame) + callee_frame->data_length);
                        return err;
                    }
                }
                ci_free_alloca_list(ci_allocator(ci), callee_frame->alloca_list);
                Allocator_free(ci_allocator(ci), callee_frame, sizeof(CiInterpFrame) + callee_frame->data_length);
                return 0;
            }
            fn = func->native_func;
            if(!fn)
                return ci_error(ci, expr->loc, "ICE: function '%s' not resolved before execution",
                    func->name ? func->name->data : "<unknown>");
        }
        else {
            // Indirect call through function pointer.
            int err = ci_interp_expr(ci, frame, callee, &fn, sizeof fn);
            if(err) return err;
            CcQualType callee_type = callee->type;
            if(ccqt_kind(callee_type) == CC_POINTER){
                CcQualType pointee = ccqt_as_ptr(callee_type)->pointee;
                if(ccqt_kind(pointee) != CC_FUNCTION)
                    return ci_error(ci, expr->loc, "Called object is not a function pointer");
                ftype = ccqt_as_function(pointee);
            }
            else if(ccqt_kind(callee_type) == CC_FUNCTION){
                ftype = ccqt_as_function(callee_type);
            }
            else {
                return ci_error(ci, expr->loc, "Called object is not a function pointer");
            }
        }
        // Check if this is a closure wrapping an interpreted function.
        {
            CcFunc* interp_func = BPM_rget(&ci->closure_map, (void*)fn);
            if(interp_func){
                CiInterpFrame* callee_frame = NULL;
                int err = ci_interp_call(ci, frame, interp_func, expr->values, nargs, result, size, &callee_frame);
                if(err) return err;
                while(callee_frame->pc < callee_frame->stmt_count){
                    err = ci_interp_step(ci, callee_frame);
                    if(err){
                        ci_free_alloca_list(ci_allocator(ci), callee_frame->alloca_list);
                        Allocator_free(ci_allocator(ci), callee_frame, sizeof(CiInterpFrame) + callee_frame->data_length);
                        return err;
                    }
                }
                ci_free_alloca_list(ci_allocator(ci), callee_frame->alloca_list);
                Allocator_free(ci_allocator(ci), callee_frame, sizeof(CiInterpFrame) + callee_frame->data_length);
                return 0;
            }
        }
        // Native call path: build args, look up CIF, call.
        uint32_t nvarargs = (ftype->is_variadic && nargs > ftype->param_count) ? nargs - ftype->param_count : 0;
        size_t arg_data_size = 0;
        for(uint32_t i = 0; i < nargs; i++){
            uint32_t arg_sz;
            int err = cc_sizeof_as_uint(&ci->parser, expr->values[i]->type, expr->loc, &arg_sz);
            if(err) return err;
            if(arg_sz < 8) arg_sz = 8;
            arg_data_size += arg_sz;
        }
        size_t total = nargs * sizeof(void*) + arg_data_size;
        char* buf = Allocator_zalloc(ci_allocator(ci), total);
        if(!buf) return CI_OOM_ERROR;
        void** args = (void**)buf;
        char* arg_data = buf + nargs * sizeof(void*);
        for(uint32_t i = 0; i < nargs; i++){
            uint32_t arg_sz;
            int err = cc_sizeof_as_uint(&ci->parser, expr->values[i]->type, expr->loc, &arg_sz);
            if(err) return err;
            if(arg_sz < 8) arg_sz = 8;
            args[i] = arg_data;
            err = ci_interp_expr(ci, frame, expr->values[i], arg_data, arg_sz);
            if(err){
                Allocator_free(ci_allocator(ci), buf, total);
                return err;
            }
            arg_data += arg_sz;
        }
        // Look up pre-built CIF. Non-variadic: keyed by CcFunction*.
        // Variadic: keyed by CcExpr* (the call expression node).
        NativeCallCache* cache;
        if(!nvarargs)
            cache = PM_get(&ci->ffi_cache, ftype);
        else
            cache = PM_get(&ci->ffi_cache, expr);
        if(!cache){
            Allocator_free(ci_allocator(ci), buf, total);
            return ci_error(ci, expr->loc, "ICE: ffi_cache not populated for call type");
        }
        CcQualType ret_type = ftype->return_type;
        if(ccqt_is_basic(ret_type) && ret_type.basic.kind == CCBT_void){
            native_call(cache, fn, args, ci_discard_buf);
            Allocator_free(ci_allocator(ci), buf, total);
            return 0;
        }
        uint32_t ret_sz;
        {
            int err = cc_sizeof_as_uint(&ci->parser, ret_type, expr->loc, &ret_sz);
            if(err){ Allocator_free(ci_allocator(ci), buf, total); return err; }
        }
        if(ret_sz > size){
            Allocator_free(ci_allocator(ci), buf, total);
            return CI_RESULT_TOO_SMALL(ci, expr->loc, ret_sz, size);
        }
        native_call(cache, fn, args, result);
        Allocator_free(ci_allocator(ci), buf, total);
        return 0;
    }
    case CC_EXPR_COMPOUND_LITERAL:
    case CC_EXPR_INIT_LIST: {
        uint32_t sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(sz > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
        memset(result, 0, sz);
        CcInitList* il = expr->init_list;
        for(uint32_t i = 0; i < il->count; i++){
            CcInitEntry* e = &il->entries[i];
            if(!e->value) continue;
            uint32_t esz;
            err = cc_sizeof_as_uint(&ci->parser, e->value->type, expr->loc, &esz);
            if(err) return err;
            uint64_t off = e->field_loc.byte_offset;
            if(off + esz > sz)
                return ci_error(ci, expr->loc, "interpreter: init list entry out of bounds");
            if(esz <= sizeof(uint64_t)){
                uint64_t val = 0;
                err = ci_interp_expr(ci, frame,e->value, &val, sizeof val);
                if(err) return err;
                if(e->field_loc.bit_width){
                    // bitfield
                    uint64_t existing = 0;
                    memcpy(&existing, (char*)result + off, esz);
                    uint64_t mask = ((uint64_t)1 << e->field_loc.bit_width) - 1;
                    existing &= ~(mask << e->field_loc.bit_offset);
                    existing |= (val & mask) << e->field_loc.bit_offset;
                    memcpy((char*)result + off, &existing, esz);
                }
                else {
                    memcpy((char*)result + off, &val, esz);
                }
            }
            else {
                // Nested struct/union: write directly into result at offset
                err = ci_interp_expr(ci, frame,e->value, (char*)result + off, esz);
                if(err) return err;
            }
        }
        return 0;
    }
    case CC_EXPR_ATOMIC: {
        typedef struct { _Alignas(16) char bytes[16]; } Atomic16;
        CcAtomicOp op = expr->atomic.op;
        if(op == CC_ATOMIC_THREAD_FENCE){
            __atomic_thread_fence(__ATOMIC_SEQ_CST);
            return 0;
        }
        if(op == CC_ATOMIC_SIGNAL_FENCE){
            __atomic_signal_fence(__ATOMIC_SEQ_CST);
            return 0;
        }
        // Evaluate pointer argument to get memory address
        void* ptr = NULL;
        int err = ci_interp_expr(ci, frame, expr->lhs, &ptr, sizeof ptr);
        if(err) return err;
        if(!ptr) return ci_error(ci, expr->loc, "null pointer in atomic operation");

        CcQualType pointee = ccqt_as_ptr(expr->lhs->type)->pointee;
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, pointee, expr->loc, &sz);
        if(err) return err;

        #define ATOMIC_LOAD_DISPATCH(dest) \
            switch(sz){ \
                case 1:  __atomic_load(( uint8_t*)ptr, ( uint8_t*)(dest), __ATOMIC_SEQ_CST); break; \
                case 2:  __atomic_load((uint16_t*)ptr, (uint16_t*)(dest), __ATOMIC_SEQ_CST); break; \
                case 4:  __atomic_load((uint32_t*)ptr, (uint32_t*)(dest), __ATOMIC_SEQ_CST); break; \
                case 8:  __atomic_load((uint64_t*)ptr, (uint64_t*)(dest), __ATOMIC_SEQ_CST); break; \
                case 16: __atomic_load((Atomic16*)ptr, (Atomic16*)(dest), __ATOMIC_SEQ_CST); break; \
                default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz); \
            }
        if(op == CC_ATOMIC_LOAD_N){
            ATOMIC_LOAD_DISPATCH(result);
            return 0;
        }
        if(op == CC_ATOMIC_LOAD){
            void* dest = NULL;
            err = ci_interp_expr(ci, frame, expr->values[0], &dest, sizeof dest);
            if(err) return err;
            ATOMIC_LOAD_DISPATCH(dest);
            return 0;
        }
        #undef ATOMIC_LOAD_DISPATCH
        #define ATOMIC_CAS_DISPATCH(desired_ptr) \
            switch(sz){ \
                case 1:  r = __atomic_compare_exchange(( uint8_t*)ptr, ( uint8_t*)expected_ptr, ( uint8_t*)(desired_ptr), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break; \
                case 2:  r = __atomic_compare_exchange((uint16_t*)ptr, (uint16_t*)expected_ptr, (uint16_t*)(desired_ptr), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break; \
                case 4:  r = __atomic_compare_exchange((uint32_t*)ptr, (uint32_t*)expected_ptr, (uint32_t*)(desired_ptr), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break; \
                case 8:  r = __atomic_compare_exchange((uint64_t*)ptr, (uint64_t*)expected_ptr, (uint64_t*)(desired_ptr), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break; \
                case 16: r = __atomic_compare_exchange((Atomic16*)ptr, (Atomic16*)expected_ptr, (Atomic16*)(desired_ptr), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break; \
                default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz); \
            }
        if(op == CC_ATOMIC_COMPARE_EXCHANGE_N || op == CC_ATOMIC_COMPARE_EXCHANGE){
            void* expected_ptr = NULL;
            err = ci_interp_expr(ci, frame, expr->values[0], &expected_ptr, sizeof expected_ptr);
            if(err) return err;
            _Bool r;
            if(op == CC_ATOMIC_COMPARE_EXCHANGE_N){
                _Alignas(16) char desired_buf[16] = {0};
                err = ci_interp_expr(ci, frame, expr->values[1], desired_buf, sz);
                if(err) return err;
                ATOMIC_CAS_DISPATCH(desired_buf);
            }
            else {
                void* desired_ptr = NULL;
                err = ci_interp_expr(ci, frame, expr->values[1], &desired_ptr, sizeof desired_ptr);
                if(err) return err;
                ATOMIC_CAS_DISPATCH(desired_ptr);
            }
            *(_Bool*)result = r;
            return 0;
        }
        #undef ATOMIC_CAS_DISPATCH
        #define ATOMIC_STORE_DISPATCH(val_ptr) \
            switch(sz){ \
                case 1:  __atomic_store(( uint8_t*)ptr, ( uint8_t*)(val_ptr), __ATOMIC_SEQ_CST); break; \
                case 2:  __atomic_store((uint16_t*)ptr, (uint16_t*)(val_ptr), __ATOMIC_SEQ_CST); break; \
                case 4:  __atomic_store((uint32_t*)ptr, (uint32_t*)(val_ptr), __ATOMIC_SEQ_CST); break; \
                case 8:  __atomic_store((uint64_t*)ptr, (uint64_t*)(val_ptr), __ATOMIC_SEQ_CST); break; \
                case 16: __atomic_store((Atomic16*)ptr, (Atomic16*)(val_ptr), __ATOMIC_SEQ_CST); break; \
                default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz); \
            }
        if(op == CC_ATOMIC_STORE){
            void* val_ptr = NULL;
            err = ci_interp_expr(ci, frame, expr->values[0], &val_ptr, sizeof val_ptr);
            if(err) return err;
            ATOMIC_STORE_DISPATCH(val_ptr);
            return 0;
        }
        #undef ATOMIC_STORE_DISPATCH
        if(op == CC_ATOMIC_EXCHANGE){
            void* val_ptr = NULL;
            err = ci_interp_expr(ci, frame, expr->values[0], &val_ptr, sizeof val_ptr);
            if(err) return err;
            void* ret_ptr = NULL;
            err = ci_interp_expr(ci, frame, expr->values[1], &ret_ptr, sizeof ret_ptr);
            if(err) return err;
            switch(sz){
                case 1:  __atomic_exchange(( uint8_t*)ptr, ( uint8_t*)val_ptr, ( uint8_t*)ret_ptr, __ATOMIC_SEQ_CST); break;
                case 2:  __atomic_exchange((uint16_t*)ptr, (uint16_t*)val_ptr, (uint16_t*)ret_ptr, __ATOMIC_SEQ_CST); break;
                case 4:  __atomic_exchange((uint32_t*)ptr, (uint32_t*)val_ptr, (uint32_t*)ret_ptr, __ATOMIC_SEQ_CST); break;
                case 8:  __atomic_exchange((uint64_t*)ptr, (uint64_t*)val_ptr, (uint64_t*)ret_ptr, __ATOMIC_SEQ_CST); break;
                case 16: __atomic_exchange((Atomic16*)ptr, (Atomic16*)val_ptr, (Atomic16*)ret_ptr, __ATOMIC_SEQ_CST); break;
                default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz);
            }
            return 0;
        }
        // fetch_add, fetch_sub, store_n, exchange_n: values[0]=val
        _Alignas(16) char val_buf[16] = {0};
        err = ci_interp_expr(ci, frame, expr->values[0], val_buf, sz);
        if(err) return err;
        #define ATOMIC_DISPATCH(OP) \
            switch(sz){ \
                case 1:  *( uint8_t*)result = OP(( uint8_t*)ptr, *( uint8_t*)val_buf, __ATOMIC_SEQ_CST); break; \
                case 2:  *(uint16_t*)result = OP((uint16_t*)ptr, *(uint16_t*)val_buf, __ATOMIC_SEQ_CST); break; \
                case 4:  *(uint32_t*)result = OP((uint32_t*)ptr, *(uint32_t*)val_buf, __ATOMIC_SEQ_CST); break; \
                case 8:  *(uint64_t*)result = OP((uint64_t*)ptr, *(uint64_t*)val_buf, __ATOMIC_SEQ_CST); break; \
                default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz); \
            }
        switch(op){
            case CC_ATOMIC_FETCH_ADD: ATOMIC_DISPATCH(__atomic_fetch_add); break;
            case CC_ATOMIC_FETCH_SUB: ATOMIC_DISPATCH(__atomic_fetch_sub); break;
            case CC_ATOMIC_EXCHANGE_N:
                switch(sz){
                    case 1:  __atomic_exchange(( uint8_t*)ptr, ( uint8_t*)val_buf, ( uint8_t*)result, __ATOMIC_SEQ_CST); break;
                    case 2:  __atomic_exchange((uint16_t*)ptr, (uint16_t*)val_buf, (uint16_t*)result, __ATOMIC_SEQ_CST); break;
                    case 4:  __atomic_exchange((uint32_t*)ptr, (uint32_t*)val_buf, (uint32_t*)result, __ATOMIC_SEQ_CST); break;
                    case 8:  __atomic_exchange((uint64_t*)ptr, (uint64_t*)val_buf, (uint64_t*)result, __ATOMIC_SEQ_CST); break;
                    case 16: __atomic_exchange((Atomic16*)ptr, (Atomic16*)val_buf, (Atomic16*)result, __ATOMIC_SEQ_CST); break;
                    default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz);
                }
                break;
            case CC_ATOMIC_STORE_N:
                switch(sz){
                    case 1:  __atomic_store(( uint8_t*)ptr, ( uint8_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 2:  __atomic_store((uint16_t*)ptr, (uint16_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 4:  __atomic_store((uint32_t*)ptr, (uint32_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 8:  __atomic_store((uint64_t*)ptr, (uint64_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 16: __atomic_store((Atomic16*)ptr, (Atomic16*)val_buf, __ATOMIC_SEQ_CST); break;
                    default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz);
                }
                break;
            default: return ci_error(ci, expr->loc, "unsupported atomic operation");
        }
        #undef ATOMIC_DISPATCH
        return 0;
    }
    case CC_EXPR_VA: {
        CcVaOp op = expr->va.op;
        switch(op){
        case CC_VA_START: {
            // expr->lhs is a pointer to the va_list data.
            void* ap_ptr = NULL;
            int err = ci_interp_expr(ci, frame, expr->lhs, &ap_ptr, sizeof ap_ptr);
            if(err) return err;
            if(!frame->varargs_buf)
                return ci_error(ci, expr->loc, "va_start used in non-variadic function");
            switch(ci_target(ci)->target){
            case CC_TARGET_AARCH64_MACOS:
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST: {
                void* va_ptr = frame->varargs_buf;
                memcpy(ap_ptr, &va_ptr, sizeof(void*));
                return 0;
            }
            case CC_TARGET_AARCH64_LINUX: {
                CiAapcs64VaList* va = ap_ptr;
                va->__stack = frame->varargs_buf;
                va->__gr_top = NULL;
                va->__vr_top = NULL;
                va->__gr_offs = 0;
                va->__vr_offs = 0;
                return 0;
            }
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS: {
                CiSysvVaListTag* tag = ap_ptr;
                tag->gp_offset = 48;
                tag->fp_offset = 48 + 128;
                tag->overflow_arg_area = frame->varargs_buf;
                tag->reg_save_area = NULL;
                return 0;
            }
            case CC_TARGET_COUNT:
                break;
            }
            return ci_error(ci, expr->loc, "va_start: unsupported target");
        }
        case CC_VA_END:
            return 0;
        case CC_VA_ARG: {
            // expr->lhs is a pointer to the va_list data.
            void* ap_ptr = NULL;
            int err = ci_interp_expr(ci, frame, expr->lhs, &ap_ptr, sizeof ap_ptr);
            if(err) return err;
            uint32_t sz;
            err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
            if(err) return err;
            uint32_t advance = sz < 8 ? 8 : (sz + 7) & ~7u;
            switch(ci_target(ci)->target){
            case CC_TARGET_AARCH64_MACOS:
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST: {
                void* cur;
                memcpy(&cur, ap_ptr, sizeof(void*));
                if(result != ci_discard_buf){
                    if(sz > size)
                        return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
                    memcpy(result, cur, sz);
                }
                cur = (char*)cur + advance;
                memcpy(ap_ptr, &cur, sizeof(void*));
                return 0;
            }
            case CC_TARGET_AARCH64_LINUX: {
                CiAapcs64VaList* va = ap_ptr;
                _Bool is_fp = ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind);
                const void* src;
                if(is_fp){
                    if(va->__vr_offs < 0){
                        src = (char*)va->__vr_top + va->__vr_offs;
                        va->__vr_offs += 16;
                    }
                    else {
                        src = va->__stack;
                        va->__stack = (char*)va->__stack + advance;
                    }
                }
                else {
                    if(va->__gr_offs < 0){
                        src = (char*)va->__gr_top + va->__gr_offs;
                        va->__gr_offs += 8;
                    }
                    else {
                        src = va->__stack;
                        va->__stack = (char*)va->__stack + advance;
                    }
                }
                if(result != ci_discard_buf){
                    if(sz > size)
                        return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
                    memcpy(result, src, sz);
                }
                return 0;
            }
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS: {
                CiSysvVaListTag* tag = ap_ptr;
                _Bool is_fp = ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind);
                const void* src;
                if(is_fp){
                    if(tag->fp_offset < 176){
                        src = (char*)tag->reg_save_area + tag->fp_offset;
                        tag->fp_offset += 16;
                    }
                    else {
                        src = tag->overflow_arg_area;
                        tag->overflow_arg_area = (char*)tag->overflow_arg_area + advance;
                    }
                }
                else {
                    if(tag->gp_offset < 48){
                        src = (char*)tag->reg_save_area + tag->gp_offset;
                        tag->gp_offset += 8;
                    }
                    else {
                        src = tag->overflow_arg_area;
                        tag->overflow_arg_area = (char*)tag->overflow_arg_area + advance;
                    }
                }
                if(result != ci_discard_buf){
                    if(sz > size)
                        return CI_RESULT_TOO_SMALL(ci, expr->loc, sz, size);
                    memcpy(result, src, sz);
                }
                return 0;
            }
            case CC_TARGET_COUNT:
                break;
            }
            return ci_error(ci, expr->loc, "va_arg: unsupported target");
        }
        case CC_VA_COPY: {
            // Both operands are pointers to va_list data.
            void* dest_ptr = NULL;
            int err = ci_interp_expr(ci, frame, expr->lhs, &dest_ptr, sizeof dest_ptr);
            if(err) return err;
            void* src_ptr = NULL;
            err = ci_interp_expr(ci, frame, expr->values[0], &src_ptr, sizeof src_ptr);
            if(err) return err;
            switch(ci_target(ci)->target){
            case CC_TARGET_AARCH64_MACOS:
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST:
                memcpy(dest_ptr, src_ptr, sizeof(void*));
                return 0;
            case CC_TARGET_AARCH64_LINUX:
                *(CiAapcs64VaList*)dest_ptr = *(CiAapcs64VaList*)src_ptr;
                return 0;
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS:
                *(CiSysvVaListTag*)dest_ptr = *(CiSysvVaListTag*)src_ptr;
                return 0;
            case CC_TARGET_COUNT:
                break;
            }
            return ci_error(ci, expr->loc, "va_copy: unsupported target");
        }
        }
        return ci_error(ci, expr->loc, "interpreter: unsupported va operation");
    }
    case CC_EXPR_TYPE_INTROSPECTION: {
        uintptr_t type_bits = 0;
        int err = ci_interp_expr(ci, frame, expr->lhs, &type_bits, sizeof type_bits);
        if(err) return err;
        CcQualType qt = {.bits = type_bits};
        CcTypeIntrospectionOp op = expr->type_introspection.op;
        switch(op){
            case CC_TYPE_NONE:
                return CI_UNREACHABLE_ERROR;
            case CC_TYPE_NAME: {
                MStringBuilder sb = {.allocator = ci_allocator(ci)};
                cc_print_type(&sb, qt);
                Atom a;
                {
                    AtomTable* at = ci_lock_atoms(ci);
                    a = msb_atomize(&sb, at);
                    ci_unlock_atoms(ci, at);
                }
                msb_destroy(&sb);
                if(!a) return CI_OOM_ERROR;
                *(const char**)result = a->data;
                return 0;
            }
            case CC_TYPE_TAG: {
                Atom tag = 0;
                CcTypeKind k = ccqt_kind(qt);
                if(k == CC_STRUCT)     tag = ccqt_as_struct(qt)->name;
                else if(k == CC_UNION) tag = ccqt_as_union(qt)->name;
                else if(k == CC_ENUM)  tag = ccqt_as_enum(qt)->name;
                *(const char**)result = tag ? tag->data : "";
                return 0;
            }
            case CC_TYPE_IS_INTEGER: {
                *(_Bool*)result = ccqt_is_basic(qt) && ccbt_is_integer(qt.basic.kind);
                return 0;
            }
            case CC_TYPE_IS_FLOAT: {
                *(_Bool*)result = ccqt_is_basic(qt) && ccbt_is_float(qt.basic.kind);
                return 0;
            }
            case CC_TYPE_IS_ARITHMETIC: {
                *(_Bool*)result = (ccqt_is_basic(qt) && ccbt_is_arithmetic(qt.basic.kind)) || ccqt_kind(qt) == CC_ENUM;
                return 0;
            }
            case CC_TYPE_IS_POINTER: {
                *(_Bool*)result = ccqt_kind(qt) == CC_POINTER;
                return 0;
            }
            case CC_TYPE_IS_STRUCT: {
                *(_Bool*)result = ccqt_kind(qt) == CC_STRUCT;
                return 0;
            }
            case CC_TYPE_IS_UNION: {
                *(_Bool*)result = ccqt_kind(qt) == CC_UNION;
                return 0;
            }
            case CC_TYPE_IS_ARRAY: {
                *(_Bool*)result = ccqt_kind(qt) == CC_ARRAY;
                return 0;
            }
            case CC_TYPE_IS_FUNCTION: {
                *(_Bool*)result = ccqt_kind(qt) == CC_FUNCTION;
                return 0;
            }
            case CC_TYPE_IS_ENUM: {
                *(_Bool*)result = ccqt_kind(qt) == CC_ENUM;
                return 0;
            }
            case CC_TYPE_IS_CONST: {
                *(_Bool*)result = qt.is_const;
                return 0;
            }
            case CC_TYPE_IS_VOLATILE: {
                *(_Bool*)result = qt.is_volatile;
                return 0;
            }
            case CC_TYPE_IS_ATOMIC: {
                *(_Bool*)result = qt.is_atomic;
                return 0;
            }
            case CC_TYPE_IS_UNSIGNED: {
                *(_Bool*)result = ccqt_is_basic(qt) && ccbt_is_unsigned(qt.basic.kind, !ci_target(ci)->char_is_signed);
                return 0;
            }
            case CC_TYPE_IS_SIGNED: {
                *(_Bool*)result = ccqt_is_basic(qt) && ccbt_is_integer(qt.basic.kind) && !ccbt_is_unsigned(qt.basic.kind, !ci_target(ci)->char_is_signed);
                return 0;
            }
            case CC_TYPE_SIZEOF: {
                uint32_t sz;
                err = cc_sizeof_as_uint(&ci->parser, qt, expr->loc, &sz);
                if(err) return err;
                *(unsigned long*)result = sz;
                return 0;
            }
            case CC_TYPE_ALIGNOF: {
                uint32_t al;
                err = cc_alignof_as_uint(&ci->parser, qt, expr->loc, &al);
                if(err) return err;
                *(unsigned long*)result = al;
                return 0;
            }
            case CC_TYPE_POINTEE: {
                if(ccqt_kind(qt) != CC_POINTER)
                    return ci_error(ci, expr->loc, "_Type.pointee: not a pointer type");
                CcPointer* ptr = ccqt_as_ptr(qt);
                *(uintptr_t*)result = ptr->pointee.bits;
                return 0;
            }
            case CC_TYPE_IS_CALLABLE: {
                CcTypeKind k = ccqt_kind(qt);
                *(_Bool*)result = k == CC_FUNCTION || (k == CC_POINTER && ccqt_kind(ccqt_as_ptr(qt)->pointee) == CC_FUNCTION);
                return 0;
            }
            case CC_TYPE_IS_INCOMPLETE: {
                CcTypeKind k = ccqt_kind(qt);
                switch(k){
                    DEFAULT_UNREACHABLE;
                    case CC_STRUCT:
                        *(_Bool*)result = ccqt_as_struct(qt)->is_incomplete;
                        break;
                    case CC_UNION:
                        *(_Bool*)result = ccqt_as_union(qt)->is_incomplete;
                        break;
                    case CC_ARRAY:
                        *(_Bool*)result = ccqt_as_array(qt)->is_incomplete;
                        break;
                    case CC_ENUM:
                        *(_Bool*)result = ccqt_as_enum(qt)->is_incomplete;
                        break;
                    case CC_FUNCTION:
                    case CC_BASIC:
                    case CC_POINTER:
                        *(_Bool*)result = 0;
                }
                return 0;
            }
            case CC_TYPE_IS_VARIADIC: {
                CcQualType ft = qt;
                if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                *(_Bool*)result = ccqt_kind(ft) == CC_FUNCTION && ccqt_as_function(ft)->is_variadic;
                return 0;
            }
            case CC_TYPE_UNQUAL: {
                CcQualType uq = qt;
                uq.quals = 0;
                *(uintptr_t*)result = uq.bits;
                return 0;
            }
            case CC_TYPE_COUNT: {
                if(ccqt_kind(qt) != CC_ARRAY)
                    return ci_error(ci, expr->loc, "_Type.count: not an array type");
                *(unsigned long*)result = ccqt_as_array(qt)->length;
                return 0;
            }
            case CC_TYPE_IS_CALLABLE_WITH: {
                uintptr_t arg_bits = 0;
                err = ci_interp_expr(ci, frame, expr->values[0], &arg_bits, sizeof arg_bits);
                if(err) return err;
                CcQualType arg_type = {.bits = arg_bits};
                CcQualType ft = qt;
                if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                _Bool v = 0;
                if(ccqt_kind(ft) == CC_FUNCTION){
                    CcFunction* f = ccqt_as_function(ft);
                    if(f->param_count == 1)
                        v = cc_implicit_convertible(arg_type, f->params[0]);
                }
                *(_Bool*)result = v;
                return 0;
            }
            case CC_TYPE_CASTABLE_TO: {
                uintptr_t arg_bits = 0;
                err = ci_interp_expr(ci, frame, expr->values[0], &arg_bits, sizeof arg_bits);
                if(err) return err;
                CcQualType target = {.bits = arg_bits};
                *(_Bool*)result = cc_explicit_castable(qt, target);
                return 0;
            }
            case CC_TYPE_FIELD:{
                CcTypeKind k = ccqt_kind(qt);
                if(k != CC_STRUCT && k != CC_UNION)
                    return ci_error(ci, expr->loc, "_Type.field: not a struct or union type");
                uintptr_t idx = 0;
                err = ci_interp_expr(ci, frame, expr->values[0], &idx, sizeof idx);
                if(err) return err;
                CcField* f;
                if(k == CC_STRUCT){
                    CcStruct* s = ccqt_as_struct(qt);
                    if(idx >= s->field_count)
                        return ci_error(ci, expr->loc, "_Type.field: index out of range");
                    f = &s->fields[idx];
                }
                else {
                    CcUnion* s = ccqt_as_union(qt);
                    if(idx >= s->field_count)
                        return ci_error(ci, expr->loc, "_Type.field: index out of range");
                    f = &s->fields[idx];
                }
                CiRtField* out = (CiRtField*)result;
                if(f->is_method){
                    *out = (CiRtField){
                        .type = f->type,
                        .name = f->method->name ? f->method->name->data : "",
                        .name_length = f->method->name ? f->method->name->length : 0,
                        .offset = 0,
                        .bitwidth = 0,
                        .bitoffset = 0,
                    };
                }
                else {
                    *out = (CiRtField){
                        .type = f->type,
                        .name = f->name ? f->name->data : "",
                        .name_length = f->name ? f->name->length : 0,
                        .offset = f->offset,
                        .bitwidth = f->bitwidth,
                        .bitoffset = f->bitoffset,
                    };
                }
                return 0;
            }
            case CC_TYPE_PUSH_METHOD:
                return ci_error(ci, expr->loc, "push_method should be handled at parse time");
            case CC_TYPE_ENUMERATORS: {
                if(ccqt_kind(qt) != CC_ENUM)
                    return ci_error(ci, expr->loc, "_Type.enumerators: not an enum type");
                CcEnum* en = ccqt_as_enum(qt);
                *(unsigned long*)result = en->enumerator_count;
                return 0;
            }
            case CC_TYPE_ENUMERATOR: {
                if(ccqt_kind(qt) != CC_ENUM)
                    return ci_error(ci, expr->loc, "_Type.enumerator: not an enum type");
                CcEnum* enum_ = ccqt_as_enum(qt);
                uintptr_t idx = 0;
                err = ci_interp_expr(ci, frame, expr->values[0], &idx, sizeof idx);
                if(err) return err;
                if(idx >= enum_->enumerator_count)
                    return ci_error(ci, expr->loc, "_Type.enumerator: index out of range");
                CcEnumerator* enumerator = enum_->enumerators[idx];
                CiRtEnumerator* out = (CiRtEnumerator*)result;
                out->name = enumerator->name ? enumerator->name->data : "";
                out->name_length = enumerator->name ? enumerator->name->length: 0;
                out->value = (long long)enumerator->value;
                return 0;
            }
            case CC_TYPE_RETURN_TYPE: {
                CcQualType ft = qt;
                if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                if(ccqt_kind(ft) != CC_FUNCTION)
                    return ci_error(ci, expr->loc, "_Type.return_type: not a function type");
                *(uintptr_t*)result = ccqt_as_function(ft)->return_type.bits;
                return 0;
            }
            case CC_TYPE_PARAM_COUNT: {
                CcQualType ft = qt;
                if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                if(ccqt_kind(ft) != CC_FUNCTION)
                    return ci_error(ci, expr->loc, "_Type.param_count: not a function type");
                *(unsigned long*)result = ccqt_as_function(ft)->param_count;
                return 0;
            }
            case CC_TYPE_PARAM_TYPE: {
                CcQualType ft = qt;
                if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                if(ccqt_kind(ft) != CC_FUNCTION)
                    return ci_error(ci, expr->loc, "_Type.param_type: not a function type");
                CcFunction* f = ccqt_as_function(ft);
                uintptr_t idx = 0;
                err = ci_interp_expr(ci, frame, expr->values[0], &idx, sizeof idx);
                if(err) return err;
                if(idx >= f->param_count)
                    return ci_error(ci, expr->loc, "_Type.param_type: index out of range");
                *(uintptr_t*)result = f->params[idx].bits;
                return 0;
            }
            case CC_TYPE_ELEMENT_TYPE: {
                if(ccqt_kind(qt) != CC_ARRAY)
                    return ci_error(ci, expr->loc, "_Type.element_type: not an array type");
                *(uintptr_t*)result = ccqt_as_array(qt)->element.bits;
                return 0;
            }
            case CC_TYPE_UNDERLYING_TYPE: {
                if(ccqt_kind(qt) != CC_ENUM)
                    return ci_error(ci, expr->loc, "_Type.underlying_type: not an enum type");
                *(uintptr_t*)result = ccqt_as_enum(qt)->underlying.bits;
                return 0;
            }
            case CC_TYPE_FIELDS:{
                CcTypeKind k = ccqt_kind(qt);
                if(k != CC_STRUCT && k != CC_UNION)
                    return ci_error(ci, expr->loc, "_Type.fields: not a struct or union type");
                CcStruct* s = ccqt_as_struct(qt);
                *(unsigned long*)result = s->field_count;
                return 0;
            }
        }
        return ci_error(ci, expr->loc, "interpreter: unsupported type introspection op");
    }
    case CC_EXPR_BUILTIN: {
        CcBuiltinOp op = expr->builtin.op;
        switch(op){
            case CC_BUILTIN_UNREACHABLE:
                return ci_error(ci, expr->loc, "__builtin_unreachable reached");
            case CC_BUILTIN_TRAP:
                return ci_error(ci, expr->loc, "__builtin_trap");
            case CC_BUILTIN_DEBUGTRAP:
                return 0;
            case CC_BUILTIN_ABORT:
                return ci_error(ci, expr->loc, "__builtin_abort called");
            case CC_BUILTIN_BACKTRACE:
                return ci_backtrace(ci, frame, 0);
        }
        return ci_error(ci, expr->loc, "interpreter: unsupported builtin");
    }
    case CC_EXPR_ALLOCA: {
        uint64_t sz = 0;
        int err = ci_interp_expr(ci, frame, expr->lhs, &sz, sizeof sz);
        if(err) return err;
        sz = ci_read_uint(&sz, sizeof(size_t));
        CiAllocaBlock* block = Allocator_alloc(ci_allocator(ci), sizeof(CiAllocaBlock) + sz);
        if(!block) return CI_OOM_ERROR;
        memset(block + 1, 0, sz);
        block->size = sz;
        block->next = frame->alloca_list;
        frame->alloca_list = block;
        void* ptr = block + 1;
        if(sizeof(void*) > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sizeof(void*), size);
        memcpy(result, &ptr, sizeof(void*));
        return 0;
    }
    case CC_EXPR_INTERN: {
        // Evaluate the argument to get a char pointer.
        void* ptr = NULL;
        int err = ci_interp_expr(ci, frame, expr->lhs, &ptr, sizeof ptr);
        if(err) return err;
        const char* s = ptr;
        if(!s){
            // NULL in, NULL out.
            memset(result, 0, sizeof(void*));
            return 0;
        }
        size_t len = strlen(s);
        Atom a;
        {
            AtomTable* at = ci_lock_atoms(ci);
            a = AT_atomize(at, s, len);
            ci_unlock_atoms(ci, at);
        }
        if(!a) return CI_OOM_ERROR;
        const char* interned = a->data;
        if(sizeof(void*) > size)
            return CI_RESULT_TOO_SMALL(ci, expr->loc, sizeof(void*), size);
        memcpy(result, &interned, sizeof(void*));
        return 0;
    }
    case CC_EXPR_ADD_OVERFLOW:
    case CC_EXPR_SUB_OVERFLOW:
    case CC_EXPR_MUL_OVERFLOW: {
        // Read a
        uint64_t abuf = 0;
        uint32_t asz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->lhs->type, expr->loc, &asz);
        if(err) return err;
        err = ci_interp_expr(ci, frame, expr->lhs, &abuf, sizeof abuf);
        if(err) return err;
        _Bool a_unsigned = ccqt_is_basic(expr->lhs->type) && ccbt_is_unsigned(expr->lhs->type.basic.kind, !ci_target(ci)->char_is_signed);
        CiInt128 a = a_unsigned ? ci_int128_from_uint64(ci_read_uint(&abuf, asz))
                              : ci_int128_from_int64(ci_read_int(&abuf, asz));
        // Read b
        uint64_t bbuf = 0;
        uint32_t bsz;
        err = cc_sizeof_as_uint(&ci->parser, expr->values[0]->type, expr->loc, &bsz);
        if(err) return err;
        err = ci_interp_expr(ci, frame, expr->values[0], &bbuf, sizeof bbuf);
        if(err) return err;
        _Bool b_unsigned = ccqt_is_basic(expr->values[0]->type) && ccbt_is_unsigned(expr->values[0]->type.basic.kind, !ci_target(ci)->char_is_signed);
        CiInt128 b = b_unsigned ? ci_int128_from_uint64(ci_read_uint(&bbuf, bsz)) : ci_int128_from_int64(ci_read_int(&bbuf, bsz));
        // Compute in infinite precision
        CiInt128 r;
        switch(expr->kind){
            case CC_EXPR_ADD_OVERFLOW: r = ci_int128_add(a, b); break;
            case CC_EXPR_SUB_OVERFLOW: r = ci_int128_sub(a, b); break;
            case CC_EXPR_MUL_OVERFLOW: r = ci_int128_mul(a, b); break;
            default: return CI_UNREACHABLE_ERROR;
        }
        // Get result pointer and destination type
        void* res_ptr = NULL;
        err = ci_interp_expr(ci, frame, expr->values[1], &res_ptr, sizeof res_ptr);
        if(err) return err;
        CcQualType dest_type = ccqt_as_ptr(expr->values[1]->type)->pointee;
        uint32_t dsz;
        err = cc_sizeof_as_uint(&ci->parser, dest_type, expr->loc, &dsz);
        if(err) return err;
        // Truncate and store
        uint64_t truncated = ci_int128_lo(r);
        ci_write_uint(res_ptr, dsz, truncated);
        // Check for overflow: sign/zero-extend the truncated value
        // back to CiInt128 and compare with the infinite precision result.
        _Bool dest_unsigned = ccqt_is_basic(dest_type) && ccbt_is_unsigned(dest_type.basic.kind, !ci_target(ci)->char_is_signed);
        CiInt128 back;
        if(dest_unsigned){
            if(dsz >= 8)
                back = ci_int128_from_uint64(truncated);
            else
                back = ci_int128_from_uint64(truncated & (((uint64_t)1 << (dsz * 8)) - 1));
        }
        else {
            int64_t sval;
            switch(dsz){
                case 1: sval = (int8_t)truncated; break;
                case 2: sval = (int16_t)truncated; break;
                case 4: sval = (int32_t)truncated; break;
                default: sval = (int64_t)truncated; break;
            }
            back = ci_int128_from_int64(sval);
        }
        _Bool overflowed = !ci_int128_eq(r, back);
        if(result != ci_discard_buf){
            uint32_t rsz;
            err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &rsz);
            if(err) return err;
            if(rsz > size)
                return CI_RESULT_TOO_SMALL(ci, expr->loc, rsz, size);
            ci_write_uint(result, rsz, overflowed);
        }
        return 0;
    }
    case CC_EXPR_POPCOUNT: {
        uint64_t val = 0;
        uint32_t sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->lhs->type, expr->loc, &sz);
        if(err) return err;
        err = ci_interp_expr(ci, frame, expr->lhs, &val, sizeof val);
        if(err) return err;
        val = ci_read_uint(&val, sz);
        int count = popcount_64(val);
        if(result != ci_discard_buf){
            uint32_t rsz;
            err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &rsz);
            if(err) return err;
            if(rsz > size)
                return CI_RESULT_TOO_SMALL(ci, expr->loc, rsz, size);
            ci_write_uint(result, rsz, (uint64_t)count);
        }
        return 0;
    }
    case CC_EXPR_CTZ:
    case CC_EXPR_CLZ: {
        uint64_t val = 0;
        uint32_t sz;
        int err = cc_sizeof_as_uint(&ci->parser, expr->lhs->type, expr->loc, &sz);
        if(err) return err;
        err = ci_interp_expr(ci, frame, expr->lhs, &val, sizeof val);
        if(err) return err;
        val = ci_read_uint(&val, sz);
        int count;
        if(val == 0){
            // UB per the spec, but common behavior is to return the bit width
            count = (int)(sz * 8);
        }
        else if(expr->kind == CC_EXPR_CTZ)
            count = ctz_64(val);
        else
            count = clz_64(val) - (int)(64 - sz * 8);
        if(result != ci_discard_buf){
            uint32_t rsz;
            err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &rsz);
            if(err) return err;
            if(rsz > size)
                return CI_RESULT_TOO_SMALL(ci, expr->loc, rsz, size);
            ci_write_uint(result, rsz, (uint64_t)count);
        }
        return 0;
    }
    case CC_EXPR_SIZEOF_VMT:
    case CC_EXPR_STATEMENT_EXPRESSION:
        return ci_unimplemented(ci, expr->loc, "interpreter: unsupported expression kind");
    }
    return ci_unimplemented(ci, expr->loc, "interpreter: unsupported expression kind");
}

static
int
ci_interp_step(CiInterpreter* ci, CiInterpFrame* frame){
    if(frame->pc >= frame->stmt_count)
        return 0;
    CcStatement* stmt = &frame->stmts[frame->pc];
    switch(stmt->kind){
        case CC_STMT_NULL:
        case CC_STMT_LABEL:
            frame->pc++;
            return 0;
        case CC_STMT_EXPR: {
            int err = ci_interp_expr(ci, frame, stmt->exprs[0], ci_discard_buf, sizeof ci_discard_buf);
            if(err) return err;
            frame->pc++;
            return 0;
        }
        case CC_STMT_GOTO:
            frame->pc = stmt->targets[0];
            return 0;
        case CC_STMT_FOR: {
            if(stmt->exprs[1]){
                uint64_t cond = 0;
                int err = ci_interp_expr(ci, frame,stmt->exprs[1], &cond, sizeof cond);
                if(err) return err;
                if(!cond){
                    frame->pc = stmt->targets[0];
                    return 0;
                }
            }
            frame->pc++;
            return 0;
        }
        case CC_STMT_WHILE: {
            uint64_t cond = 0;
            int err = ci_interp_expr(ci, frame,stmt->exprs[0], &cond, sizeof cond);
            if(err) return err;
            if(!cond){
                frame->pc = stmt->targets[0];
                return 0;
            }
            frame->pc++;
            return 0;
        }
        case CC_STMT_IF: {
            uint64_t cond = 0;
            int err = ci_interp_expr(ci, frame,stmt->exprs[0], &cond, sizeof cond);
            if(err) return err;
            if(!cond)
                frame->pc = stmt->targets[0];
            else
                frame->pc++;
            return 0;
        }
        case CC_STMT_DOWHILE: {
            uint64_t cond = 0;
            int err = ci_interp_expr(ci, frame,stmt->exprs[0], &cond, sizeof cond);
            if(err) return err;
            if(cond)
                frame->pc = stmt->targets[0];
            else
                frame->pc++;
            return 0;
        }
        case CC_STMT_RETURN: {
            if(stmt->exprs[0]){
                int err = ci_interp_expr(ci, frame,stmt->exprs[0], frame->return_buf, frame->return_size);
                if(err) return err;
            }
            frame->pc = frame->stmt_count;
            return 0;
        }
        case CC_STMT_SWITCH: {
            uint64_t val = 0;
            int err = ci_interp_expr(ci, frame,stmt->switch_expr, &val, sizeof val);
            if(err) return err;
            // Sign-extend or zero-extend to 64 bits based on the switch expression type.
            {
                CcQualType st = stmt->switch_expr->type;
                uint32_t ssz;
                err = cc_sizeof_as_uint(&ci->parser, st, stmt->loc, &ssz);
                if(err) return err;
                _Bool is_unsigned = ccqt_is_basic(st) && ccbt_is_unsigned(st.basic.kind, !ci_target(ci)->char_is_signed);
                if(is_unsigned)
                    val = ci_read_uint(&val, ssz);
                else
                    val = (uint64_t)ci_read_int(&val, ssz);
            }
            uint32_t count = stmt->targets[2];
            CcSwitchEntry* table = stmt->switch_table;
            // Binary search for matching case
            uint32_t lo = 0, hi = count;
            while(lo < hi){
                uint32_t mid = lo + (hi - lo) / 2;
                if(table[mid].value < val)
                    lo = mid + 1;
                else if(table[mid].value > val)
                    hi = mid;
                else {
                    frame->pc = table[mid].target;
                    return 0;
                }
            }
            // No match — jump to default or exit
            frame->pc = stmt->targets[1];
            return 0;
        }
        case CC_STMT_CASE:
        case CC_STMT_DEFAULT:
        case CC_STMT_BREAK:
        case CC_STMT_CONTINUE:
            return CI_UNREACHABLE_ERROR;
    }
    return ci_unimplemented(ci, stmt->loc, "unsupported statement kind");
}

// Push a new frame for an interpreted function call.
// Evaluates arguments and sets up parameter storage.
// Does NOT run the step loop — the caller must drive stepping.
static
int
ci_interp_call(CiInterpreter* ci, CiInterpFrame* caller, CcFunc* func, CcExpr*_Nonnull* _Nonnull args, uint32_t nargs, void* result, size_t size, CiInterpFrame*_Nullable*_Nonnull out_frame){
    int err;
    if(!func->parsed)
        return ci_error(ci, func->loc, "ICE: function '%s' not parsed before execution", func->name->data);
    CcFunction* ftype = func->type;
    // Compute varargs buffer size: each vararg gets an 8-byte-aligned slot.
    size_t varargs_size = 0;
    if(ftype->is_variadic && nargs > ftype->param_count){
        for(uint32_t i = ftype->param_count; i < nargs; i++){
            uint32_t arg_sz;
            err = cc_sizeof_as_uint(&ci->parser, args[i]->type, func->loc, &arg_sz);
            if(err) return err;
            if(arg_sz < 8) arg_sz = 8;
            arg_sz = (arg_sz + 7) & ~7u;
            varargs_size += arg_sz;
        }
    }
    size_t alloc_size = sizeof(CiInterpFrame) + func->frame_size + varargs_size;
    CiInterpFrame* frame = Allocator_zalloc(ci_allocator(ci), alloc_size);
    if(!frame) return CI_OOM_ERROR;
    *frame = (CiInterpFrame){
        .parent = caller,
        .stmts = func->body.data,
        .stmt_count = func->body.count,
        .return_buf = result,
        .return_size = size,
        .data_length = func->frame_size + varargs_size,
        .varargs_buf = ftype->is_variadic ? (char*)(frame + 1) + func->frame_size : NULL,
    };
    // Evaluate fixed args into param storage in the frame's trailing data.
    // We must evaluate in the CALLER's frame context.
    for(uint32_t i = 0; i < ftype->param_count && i < nargs; i++){
        CcVariable* var = func->param_vars[i];
        if(!var) continue;
        uint32_t param_sz;
        err = cc_sizeof_as_uint(&ci->parser, ftype->params[i], func->loc, &param_sz);
        if(err) return err;
        void* storage = (char*)(frame + 1) + var->frame_offset;
        err = ci_interp_expr(ci, caller, args[i], storage, param_sz);
        if(err){ Allocator_free(ci_allocator(ci), frame, alloc_size); return err; }
    }
    // Evaluate varargs into the trailing buffer.
    if(varargs_size){
        char* va_buf = frame->varargs_buf;
        for(uint32_t i = ftype->param_count; i < nargs; i++){
            uint32_t arg_sz;
            err = cc_sizeof_as_uint(&ci->parser, args[i]->type, func->loc, &arg_sz);
            if(err){ Allocator_free(ci_allocator(ci), frame, alloc_size); return err; }
            err = ci_interp_expr(ci, caller, args[i], va_buf, arg_sz < 8 ? 8 : arg_sz);
            if(err){ Allocator_free(ci_allocator(ci), frame, alloc_size); return err; }
            uint32_t slot = arg_sz < 8 ? 8 : (arg_sz + 7) & ~7u;
            va_buf += slot;
        }
    }
    *out_frame = frame;
    return 0;
}

static
int
ci_call_by_name(CiInterpreter* ci, StringView name, const CiArg* _Nullable args, uint32_t nargs, void* result, size_t size){
    AtomTable* at = ci_lock_atoms(ci);
    Atom atom = AT_atomize(at, name.text, name.length);
    ci_unlock_atoms(ci, at);
    if(!atom) return CI_OOM_ERROR;
    CcFunc* func = cc_scope_lookup_func(&ci->parser.global, atom, CC_SCOPE_NO_WALK);
    if(!func || !func->defined)
        return CI_SYMBOL_NOT_FOUND;
    if(!func->parsed)
        return ci_error(ci, func->loc, "ICE: function '%s' not parsed before execution", func->name->data);
    CcFunction* ftype = func->type;
    // Check arg count.
    if(nargs != ftype->param_count)
        return ci_error(ci, func->loc, "ci_call_by_name '%.*s': expected %u args, got %u",
            (int)name.length, name.text, ftype->param_count, nargs);
    // Type-check args against parameter types.
    for(uint32_t i = 0; i < nargs; i++){
        if(args[i].type.bits != ftype->params[i].bits)
            return ci_error(ci, func->loc, "ci_call_by_name '%.*s': arg %u type mismatch",
                (int)name.length, name.text, i);
    }
    size_t alloc_size = sizeof(CiInterpFrame) + func->frame_size;
    CiInterpFrame* frame = Allocator_zalloc(ci_allocator(ci), alloc_size);
    if(!frame) return CI_OOM_ERROR;
    *frame = (CiInterpFrame){
        .stmts = func->body.data,
        .stmt_count = func->body.count,
        .return_buf = result,
        .return_size = size,
        .data_length = func->frame_size,
    };
    // Copy args into param storage.
    for(uint32_t i = 0; i < nargs; i++){
        CcVariable* var = func->param_vars[i];
        if(!var) continue;
        void* storage = (char*)(frame + 1) + var->frame_offset;
        uint32_t param_sz;
        int err = cc_sizeof_as_uint(&ci->parser, ftype->params[i], func->loc, &param_sz);
        if(err){ Allocator_free(ci_allocator(ci), frame, alloc_size); return err; }
        if(args[i].size < param_sz){
            Allocator_free(ci_allocator(ci), frame, alloc_size);
            return ci_error(ci, func->loc, "ci_call_by_name '%.*s': arg %u buffer too small",
                (int)name.length, name.text, i);
        }
        memcpy(storage, args[i].data, param_sz);
    }
    int err = 0;
    while(frame->pc < frame->stmt_count){
        err = ci_interp_step(ci, frame);
        if(err) break;
    }
    ci_free_alloca_list(ci_allocator(ci), frame->alloca_list);
    Allocator_free(ci_allocator(ci), frame, alloc_size);
    return err;
}

static
int
ci_call_main(CiInterpreter* ci, int argc, char*_Null_unspecified*_Null_unspecified argv, char*_Null_unspecified*_Null_unspecified envp, int* out_ret){
    AtomTable* at = ci_lock_atoms(ci);
    Atom atom = AT_atomize(at, "main", 4);
    ci_unlock_atoms(ci, at);
    if(!atom) return CI_OOM_ERROR;
    CcFunc* func = cc_scope_lookup_func(&ci->parser.global, atom, CC_SCOPE_NO_WALK);
    if(!func || !func->defined)
        return CI_SYMBOL_NOT_FOUND;
    CcFunction* ftype = func->type;
    uint32_t nparams = ftype->param_count;
    CiArg args[3];
    switch(nparams){
        case 0:
            break;
        case 3:
            args[2] = (CiArg){.data = &envp, .size = sizeof envp, .type = ftype->params[2]};
            goto argv;
        case 2:
            argv:
            args[0] = (CiArg){.data = &argc, .size = sizeof argc, .type = ftype->params[0]};
            args[1] = (CiArg){.data = &argv, .size = sizeof argv, .type = ftype->params[1]};
            break;
        default:
            return ci_error(ci, func->loc, "main has unsupported signature (%u params)", nparams);
    }
    int ret = 0;
    int err = ci_call_by_name(ci, SV("main"), args, nparams, &ret, sizeof ret);
    if(err) return err;
    *out_ret = ret;
    return 0;
}

static
int
ci_resolve_refs(CiInterpreter* ci, _Bool libc_only){
    CcParser* p = &ci->parser;
    Allocator al = ci_allocator(ci);
    if(!libc_only){
        // Add main() as a root if it exists.
        {
            Atom main_atom = AT_atomize(p->cpp.at, "main", 4);
            if(!main_atom) return CI_OOM_ERROR;
            CcFunc* main_func = cc_scope_lookup_func(&p->global, main_atom, CC_SCOPE_NO_WALK);
            if(main_func && main_func->defined){
                int err = PM_put(&p->used_funcs, al, main_func, main_func);
                if(err) return CI_OOM_ERROR;
            }
        }
    }
    // Resolve functions. When libc_only, only resolve libc builtins.
    // Otherwise resolve all (parsing bodies, dlsym, closures).
    for(size_t i = libc_only ? ci->resolved_libc : ci->resolved_funcs; i < p->used_funcs.count; i++){
        PointerMapItems items = PM_items(&p->used_funcs);
        CcFunc* func = (CcFunc*)(uintptr_t)items.data[i].key;
        if(libc_only && !func->libc_builtin) continue;
        if(!func->defined){
            if(!func->native_func && func->name){
                LongString sym = func->mangle
                    ? (LongString){func->mangle->length, func->mangle->data}
                    : (LongString){func->name->length, func->name->data};
                void* addr;
                int err = ci_dlsym(ci, func->loc, sym, "function", &addr);
                if(err) return err;
                func->native_func = (void(*)(void))addr;
            }
            continue;
        }
        if(!libc_only){
            if(!func->parsed){
                int err = cc_parse_func_body(p, func);
                if(err) return err;
            }
            if(!func->native_func && func->addr_taken){
                int err = ci_create_closure(ci, func);
                if(err) return err;
            }
        }
    }
    if(libc_only)
        ci->resolved_libc = p->used_funcs.count;
    else
        ci->resolved_funcs = p->used_funcs.count;
    // Resolve non-automatic variable storage.
    if(!libc_only){
        {
            PointerMapItems items = PM_items(&p->used_vars);
            for(size_t i = ci->resolved_vars; i < items.count; i++){
                CcVariable* var = (CcVariable*)(uintptr_t)items.data[i].key;
                if(var->interp_val) continue;
                if(var->extern_ && !var->initializer){
                    LongString sym = var->mangle
                        ? (LongString){var->mangle->length, var->mangle->data}
                        : (LongString){var->name->length, var->name->data};
                    void* addr;
                    int err = ci_dlsym(ci, var->loc, sym, "extern variable", &addr);
                    if(err) return err;
                    var->interp_val = addr;
                }
                else {
                    uint32_t sz;
                    int err = cc_sizeof_as_uint(p, var->type, var->loc, &sz);
                    if(err) return err;
                    void* storage = Allocator_zalloc(al, sz);
                    if(!storage) return CI_OOM_ERROR;
                    var->interp_val = storage;
                }
            }
            ci->resolved_vars = p->used_vars.count;
        }
        // Evaluate initializers for non-automatic variables.
        // This must happen after all storage is allocated (initializers may
        // reference other globals). Running this here (single-threaded, before
        // execution) avoids re-initialization and thread races.
        {
            PointerMapItems items = PM_items(&p->used_vars);
            CiInterpFrame dummy_frame = {0};
            for(size_t i = 0; i < items.count; i++){
                CcVariable* var = (CcVariable*)(uintptr_t)items.data[i].key;
                if(!var->interp_preinit) continue;
                if(!var->initializer) continue;
                if(!var->interp_val) continue;
                if(var->interp_initialized) continue;
                uint32_t sz;
                int err = cc_sizeof_as_uint(p, var->type, var->loc, &sz);
                if(err) return err;
                err = ci_interp_expr(ci, &dummy_frame, var->initializer, var->interp_val, sz);
                if(err) return err;
                var->interp_initialized = 1;
            }
        }
    }
    #ifndef NO_NATIVE_CALL
    // Pre-populate ffi_cache for non-variadic call types.
    {
        PointerMapItems funcs = PM_items(&p->used_funcs);
        for(size_t i = 0; i < funcs.count; i++){
            CcFunc* func = (CcFunc*)(uintptr_t)funcs.data[i].key;
            if(func->defined) continue;
            CcFunction* ftype = func->type;
            if(PM_get(&ci->ffi_cache, ftype)) continue;
            NativeCallCache* cache = NULL;
            int err = native_call_cache_create(al, ftype, 0, NULL, &cache);
            if(err) return err;
            err = PM_put(&ci->ffi_cache, al, ftype, cache);
            if(err) return CI_OOM_ERROR;
        }
        PointerMapItems ctypes = PM_items(&p->used_call_types);
        for(size_t i = 0; i < ctypes.count; i++){
            CcFunction* ftype = (CcFunction*)(uintptr_t)ctypes.data[i].key;
            if(PM_get(&ci->ffi_cache, ftype)) continue;
            NativeCallCache* cache = NULL;
            int err = native_call_cache_create(al, ftype, 0, NULL, &cache);
            if(err) return err;
            err = PM_put(&ci->ffi_cache, al, ftype, cache);
            if(err) return CI_OOM_ERROR;
        }
    }
    // Pre-populate ffi_cache for variadic call expressions.
    {
        PointerMapItems vcalls = PM_items(&p->used_var_calls);
        for(size_t i = ci->resolved_variadic; i < vcalls.count; i++){
            CcExpr* call_expr = (CcExpr*)(uintptr_t)vcalls.data[i].key;
            if(call_expr->lhs->kind == CC_EXPR_FUNCTION && call_expr->lhs->func->defined)
                continue;
            CcQualType ct = call_expr->lhs->type;
            CcFunction* ftype;
            if(ccqt_kind(ct) == CC_POINTER)
                ftype = ccqt_as_function(ccqt_as_ptr(ct)->pointee);
            else
                ftype = ccqt_as_function(ct);
            uint32_t nvarargs = call_expr->call.nargs - ftype->param_count;
            CcQualType* vararg_types = Allocator_alloc(al, nvarargs * sizeof(CcQualType));
            if(!vararg_types) return CI_OOM_ERROR;
            for(uint32_t j = 0; j < nvarargs; j++)
                vararg_types[j] = call_expr->values[ftype->param_count + j]->type;
            NativeCallCache* cache = NULL;
            int err = native_call_cache_create(al, ftype, nvarargs, vararg_types, &cache);
            Allocator_free(al, vararg_types, nvarargs * sizeof(CcQualType));
            if(err) return err;
            err = PM_put(&ci->ffi_cache, al, call_expr, cache);
            if(err) return CI_OOM_ERROR;
        }
        ci->resolved_variadic = p->used_var_calls.count;
    }
    #endif
    return 0;
}

static
Allocator
ci_allocator(CiInterpreter* ci){
#ifndef CI_THREAD_UNSAFE_ALLOCATOR
    (void)ci;
    return MALLOCATOR;
#else
    return ci->parser.cpp.allocator;
#endif
}
static
Allocator
ci_scratch_allocator(CiInterpreter* ci){
    return allocator_from_arena(&ci->parser.scratch_arena);
}

static
int
ci_append_lib_path(CiInterpreter* ci, StringView sv){
    if(!sv.length) return CI_INVALID_VALUE_ERROR;
    Atom a = AT_atomize(ci->parser.cpp.at, sv.text, sv.length);
    if(!a) return CI_OOM_ERROR;
    int err = AM_put(&ci->lib_paths, ci_allocator(ci), a, (void*)(uintptr_t)1);
    if(err) return CI_OOM_ERROR;
    return 0;
}

static CppPragmaFn ci_pragma_lib, ci_pragma_lib_path, ci_pragma_pkg_config, ci_pragma_procmacro;
static
int
ci_register_pragmas(CiInterpreter*ci){
    int err = 0;
    err = cpp_register_pragma(&ci->parser.cpp, SV("lib"), ci_pragma_lib, ci);
    if(err) return err;
    err = cpp_register_pragma(&ci->parser.cpp, SV("lib_path"), ci_pragma_lib_path, ci);
    if(err) return err;
    err = cpp_register_pragma(&ci->parser.cpp, SV("pkg_config"), ci_pragma_pkg_config, ci);
    if(err) return err;
    if(ci->procedural_macros){
        err = cpp_register_pragma(&ci->parser.cpp, SV("procmacro"), ci_pragma_procmacro, ci);
        if(err) return err;
    }
    return 0;
}

static
int
ci_register_macros(CiInterpreter* ci){
    int err = 0;
    err = cpp_define_builtin_func_macro(&ci->parser.cpp, SV("__shell"), ci_shell, ci, 1, 1, 0);
    if(err) return err;
    err = cpp_define_builtin_func_macro(&ci->parser.cpp, SV("__SHELL__"), ci_shell, ci, 1, 1, 0);
    if(err) return err;
    return err;
}

static
int
ci_try_load_library(CiInterpreter* ci, LongString lib, _Bool* success){
    if(!ci->can_dlopen) return CI_RUNTIME_ERROR;
    #ifdef NO_NATIVE_CALL
        (void)ci; (void)lib;
        *success = 0;
        return 0;
    #else
        Atom a = AT_atomize(ci->parser.cpp.at, lib.text, lib.length);
        if(!a) return CI_OOM_ERROR;
        void* handle = AM_get(&ci->opened_libs, a);
        if(handle) {*success = 1; return 0;}
        #ifdef _WIN32
        MStringBuilder16 sb = {.allocator = ci_scratch_allocator(ci)};
        msb16_write_utf8(&sb, a->data, a->length);
        msb16_nul_terminate(&sb);
        if(sb.errored){
            msb16_destroy(&sb);
            return CI_OOM_ERROR;
        }
        LongStringUtf16 wlib = msb16_borrow_ls(&sb);
        handle = LoadLibraryW((const wchar_t*)wlib.text);
        msb16_destroy(&sb);
        if(!handle) {*success = 0; return 0;}
        #else
        handle = dlopen(a->data, RTLD_GLOBAL | RTLD_LAZY);
        if(!handle) {*success = 0; return 0;}
        #endif
        int err = AM_put(&ci->opened_libs, ci_allocator(ci), a, handle);
        if(err) {
            #ifdef _WIN32
            FreeLibrary(handle);
            #else
            dlclose(handle);
            #endif
            return CI_OOM_ERROR;
        }
        *success = 1;
        return 0;
    #endif
}

static
int
ci_load_library(CiInterpreter* ci, StringView sv){
    if(!ci->can_dlopen) return CI_RUNTIME_ERROR;
    MStringBuilder sb = {.allocator=ci_scratch_allocator(ci)};
    int err = 0;
    _Bool success = 0;
    StringView prefix = SV("lib");
    StringView suffixes[2] = {{0}, {0}};
    size_t nsuffixes = 0;
    switch(ci_target(ci)->os){
        case CC_OS_MACOS:
            suffixes[nsuffixes++] = SV(".dylib");
            suffixes[nsuffixes++] = SV(".so");
            break;
        case CC_OS_LINUX:
        case CC_OS_TEST:
            suffixes[nsuffixes++] = SV(".so");
            break;
        case CC_OS_WINDOWS:
            prefix = SV("");
            suffixes[nsuffixes++] = SV(".dll");
            break;
    }
    // Search lib_paths for {prefix}{name}{suffix}
    AtomMapItems items = AM_items(&ci->lib_paths);
    for(size_t i = 0; i < items.count; i++){
        if(!items.data[i].p) continue;
        Atom path = items.data[i].atom;
        for(size_t s = 0; s < nsuffixes; s++){
            // Try {path}/{prefix}{name}{suffix}
            msb_reset(&sb);
            msb_write_str(&sb, path->data, path->length);
            if(msb_peek(&sb) != '/') msb_write_char(&sb, '/');
            msb_write_str(&sb, prefix.text, prefix.length);
            msb_write_str(&sb, sv.text, sv.length);
            msb_write_str(&sb, suffixes[s].text, suffixes[s].length);
            msb_nul_terminate(&sb);
            if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
            err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
            if(err) goto finally;
            if(success) goto finally;
        }
        // Try {path}/{name} verbatim
        msb_reset(&sb);
        msb_write_str(&sb, path->data, path->length);
        if(msb_peek(&sb) != '/') msb_write_char(&sb, '/');
        msb_write_str(&sb, sv.text, sv.length);
        msb_nul_terminate(&sb);
        if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
        err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
        if(err) goto finally;
        if(success) goto finally;
    }
    // Search framework paths for {path}/{name}.framework/{name} (macOS)
    if(ci_target(ci)->os == CC_OS_MACOS){
        for(size_t i = 0; !success && i < ci->parser.cpp.framework_paths.count; i++){
            StringView fp = ci->parser.cpp.framework_paths.data[i];
            msb_reset(&sb);
            msb_write_str(&sb, fp.text, fp.length);
            if(msb_peek(&sb) != '/') msb_write_char(&sb, '/');
            msb_write_str(&sb, sv.text, sv.length);
            msb_write_literal(&sb, ".framework/");
            msb_write_str(&sb, sv.text, sv.length);
            msb_nul_terminate(&sb);
            if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
            err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
            if(err) goto finally;
            if(success) goto finally;
        }
    }
    // Fallback: {prefix}{name}{suffix} (let system search handle it)
    for(size_t s = 0; s < nsuffixes; s++){
        msb_reset(&sb);
        msb_write_str(&sb, prefix.text, prefix.length);
        msb_write_str(&sb, sv.text, sv.length);
        msb_write_str(&sb, suffixes[s].text, suffixes[s].length);
        msb_nul_terminate(&sb);
        if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
        err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
        if(err) goto finally;
        if(success) goto finally;
    }
    // Last resort: name verbatim (for absolute paths or full filenames)
    {
        msb_reset(&sb);
        msb_write_str(&sb, sv.text, sv.length);
        msb_nul_terminate(&sb);
        if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
        err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
        if(err) goto finally;
        if(success) goto finally;
    }
    err = CI_LIBRARY_NOT_FOUND_ERROR;
    finally:
    msb_destroy(&sb);
    return err;
}

static
int
ci_pragma_lib(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CiInterpreter* ci = ctx;
    if(!ci->can_dlopen)
        return cpp_error(cpp, loc, "Loading libraries is disabled");
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(!expanded) return CI_OOM_ERROR;
    int err = 0;
    err = cpp_expand_argument(cpp, toks, ntoks, expanded);
    if(err) goto finally;
    toks = expanded->data;
    ntoks = expanded->count;
    while(ntoks && toks->type == CPP_WHITESPACE){
        toks++;
        ntoks--;
    }
    while(ntoks && toks[ntoks-1].type == CPP_WHITESPACE)
        ntoks--;
    if(!ntoks){
        err = cpp_error(cpp, loc, "#pragma lib without any arguments");
        goto finally;
    }
    if(toks->type != CPP_STRING){
        err = cpp_error(cpp, loc, "#pragma lib requires a string literal library name");
        goto finally;
    }
    StringView name = {toks->txt.length-2, toks->txt.text+1};
    err = ci_load_library(ci, name);
    if(err == CI_LIBRARY_NOT_FOUND_ERROR)
        err = cpp_error(cpp, loc, "failed to load library '%.*s'", (int)name.length, name.text);
    finally:
    cpp_release_scratch(cpp, expanded);
    return err;
}
static
int
ci_pragma_lib_path(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CiInterpreter* ci = ctx;
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(!expanded) return CI_OOM_ERROR;
    int err = 0;
    toks = expanded->data;
    ntoks = expanded->count;
    while(ntoks && toks->type == CPP_WHITESPACE){
        toks++;
        ntoks--;
    }
    while(ntoks && toks[ntoks-1].type == CPP_WHITESPACE)
        ntoks--;
    if(!ntoks){
        err = cpp_error(cpp, loc, "#pragma lib_path without any arguments");
        goto finally;
    }
    if(toks->type != CPP_STRING){
        err = cpp_error(cpp, loc, "#pragma lib_path requires a string literal library name");
        goto finally;
    }
    StringView name = {toks->txt.length-2, toks->txt.text+1};
    err = ci_append_lib_path(ci, name);
    finally:
    cpp_release_scratch(cpp, expanded);
    return err;
}
static
_Bool
ci_file_exists(void* _Null_unspecified ctx, const char* path, size_t length){
    (void)ctx;
    (void)length;
    #ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    #else
    return access(path, F_OK) == 0;
    #endif
}

static
int
ci_pragma_pkg_config(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CiInterpreter* ci = ctx;
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(!expanded) return CI_OOM_ERROR;
    int err = 0;
    LongString output = {0};
    Allocator scratch = ci_scratch_allocator(ci);
    err = cpp_expand_argument(cpp, toks, ntoks, expanded);
    if(err) goto finally;
    toks = expanded->data;
    ntoks = expanded->count;
    while(ntoks && toks->type == CPP_WHITESPACE){
        toks++;
        ntoks--;
    }
    while(ntoks && toks[ntoks-1].type == CPP_WHITESPACE)
        ntoks--;
    if(!ntoks){
        err = cpp_error(cpp, loc, "#pragma pkg_config without any arguments");
        goto finally;
    }
    _Bool optional = 0;
    if(toks->type == CPP_IDENTIFIER){
        if(sv_equals(toks->txt, SV("optional"))){
            optional = 1;
            do {
                toks++;
                ntoks--;
            } while(ntoks && toks->type == CPP_WHITESPACE);
        }
    }
    if(toks->type != CPP_STRING){
        err = cpp_error(cpp, loc, "#pragma pkg_config requires a string literal package name");
        goto finally;
    }
    {
        StringView pkg_name = {toks->txt.length-2, toks->txt.text+1};
        CmdBuilder cmd = {.allocator = scratch};
        cmd_prog(&cmd, LS("pkg-config"));
        cmd_resolve_prog_path(&cmd, cpp->env, ci_file_exists, NULL);
        if(cmd.errored){
            if(optional){
                cpp_warn(cpp, loc, "`pkg-config` not found in PATH");
                err = 0;
                goto finally;
            }
            err = cpp_error(cpp, loc, "'pkg-config' not found in PATH");
            cmd_destroy(&cmd);
            goto finally;
        }
        cmd_carg(&cmd, "--cflags");
        cmd_carg(&cmd, "--libs");
        {
            Atom a = AT_atomize(ci->parser.cpp.at, pkg_name.text, pkg_name.length);
            if(!a){
                err = CI_OOM_ERROR;
                goto finally;
            }
            cmd_aarg(&cmd, a);
        }
        size_t envp_size = 0;
        void* envp = env_to_envp(cpp->env, scratch, &envp_size);
        if(!envp) {
            err = CI_OOM_ERROR;
            goto finally;
        }
        int run_err = cmd_run_capture(&cmd, envp, scratch, &output);
        cmd_destroy(&cmd);
        Allocator_free(scratch, envp, envp_size);
        if(run_err){
            if(optional) {
                err = 0;
                goto finally;
            }
            err = cpp_error(cpp, loc, "pkg-config failed for '%.*s'", (int)pkg_name.length, pkg_name.text);
            goto finally;
        }
        #if defined(__clang__) || defined(__GNUC__)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wcast-qual"
            #if !defined(__clang__)
                #pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
            #endif
        #elif defined(_MSC_VER)
            #pragma warning(push)
            #pragma warning(disable: 4090)
        #else
        #endif
        // The cast is safe: we own the buffer from cmd_run_capture.
        char* cmdline = (char*)output.text;
        #if defined(__clang__) || defined(__GNUC__)
            #pragma GCC diagnostic pop
        #elif defined(_MSC_VER)
            #pragma warning(pop)
        #endif
        enum { MAX_PKG_FLAGS = 64 };
        StringView pkg_I[MAX_PKG_FLAGS], pkg_L[MAX_PKG_FLAGS], pkg_l[MAX_PKG_FLAGS];
        StringView pkg_F[MAX_PKG_FLAGS], pkg_D[MAX_PKG_FLAGS], pkg_fw[MAX_PKG_FLAGS];
        size_t npkg_I=0, npkg_L=0, npkg_l=0, npkg_F=0, npkg_D=0, npkg_fw=0;
        #define PKG_KW(flag_, name_) { \
            .name = SV(flag_), \
            .max_num = MAX_PKG_FLAGS, \
            .one_at_a_time = 1, \
            .space_sep_is_optional = 1, \
            .dest = {.type = ARG_STRING, .pointer = name_}, \
            .pnum_parsed = &n##name_, \
        }
        ArgToParse pkg_kwargs[] = {
            PKG_KW("-I", pkg_I),
            PKG_KW("-L", pkg_L),
            PKG_KW("-l", pkg_l),
            PKG_KW("-F", pkg_F),
            PKG_KW("-D", pkg_D),
            {
                .name = SV("-framework"),
                .max_num = MAX_PKG_FLAGS,
                .one_at_a_time = 1,
                .dest = {.type = ARG_STRING, .pointer = pkg_fw},
                .pnum_parsed = &npkg_fw,
            },
        };
        #undef PKG_KW
        ArgParser pkg_parser = {
            .keyword = {.args = pkg_kwargs, .count = arrlen(pkg_kwargs)},
        };
        enum ArgParseError ap_err = parse_args_cmdline(&pkg_parser, cmdline,
            ARGPARSE_FLAGS_UNKNOWN_KWARGS_AS_ARGS
            | ARGPARSE_FLAGS_ALLOW_KWARG_SEP_TO_BE_OPTIONAL
            | ARGPARSE_FLAGS_IGNORE_EXCESS_ARGS);
        if(ap_err){
            err = cpp_error(cpp, loc, "pkg-config: failed to parse flags for '%.*s'", (int)pkg_name.length, pkg_name.text);
            goto finally;
        }
        for(size_t i = 0; i < npkg_I; i++){
            Atom a = AT_atomize(ci->parser.cpp.at, pkg_I[i].text, pkg_I[i].length);
            if(!a){ err = CI_OOM_ERROR; goto finally; }
            err = cpp_add_default_includea(cpp, &cpp->Ipaths, a);
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_L; i++){
            err = ci_append_lib_path(ci, pkg_L[i]);
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_l; i++){
            err = ci_load_library(ci, pkg_l[i]);
            if(err == CI_LIBRARY_NOT_FOUND_ERROR)
                err = cpp_error(cpp, loc, "pkg-config: failed to load library '%.*s'", (int)pkg_l[i].length, pkg_l[i].text);
            if(err == CI_RUNTIME_ERROR)
                err = cpp_error(cpp, loc, "pkg-config: Loading libraries is disabled");
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_F; i++){
            Atom a = AT_atomize(ci->parser.cpp.at, pkg_F[i].text, pkg_F[i].length);
            if(!a){ err = CI_OOM_ERROR; goto finally; }
            err = cpp_add_default_includea(cpp, &cpp->framework_paths, a);
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_D; i++){
            StringView val = pkg_D[i];
            const char* eq = memchr(val.text, '=', val.length);
            if(eq){
                StringView mname = {(size_t)(eq - val.text), val.text};
                StringView mval = {val.length - mname.length - 1, eq + 1};
                CppToken valtok = {.type = CPP_NUMBER, .txt = {mval.length, mval.text}};
                err = cpp_define_obj_macro(cpp, mname, &valtok, 1);
            }
            else {
                CppToken onetok = {.type = CPP_NUMBER, .txt = {1, "1"}};
                err = cpp_define_obj_macro(cpp, val, &onetok, 1);
            }
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_fw; i++){
            err = ci_load_library(ci, pkg_fw[i]);
            if(err == CI_LIBRARY_NOT_FOUND_ERROR)
                err = cpp_error(cpp, loc, "pkg-config: failed to load framework '%.*s'", (int)pkg_fw[i].length, pkg_fw[i].text);
            if(err == CI_RUNTIME_ERROR)
                err = cpp_error(cpp, loc, "pkg-config: Loading libraries is disabled");
            if(err) goto finally;
        }
    }
    finally:
    if(output.text) Allocator_free(scratch, output.text, output.length+1);
    cpp_release_scratch(cpp, expanded);
    return err;
}
static CiInterpreter*
ci_from_cpp(CppPreprocessor* cpp){
    return (CiInterpreter*)((char*)cpp - offsetof(CcParser, cpp) - offsetof(CiInterpreter, parser));
}

static Marray(CcToken)* cc_get_scratch(CcParser* p);
static void cc_release_scratch(CcParser* p, Marray(CcToken)*);

static
int
ci_procmacro_expand(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps){
    (void)arg_seps;
    CcFunc* func = ctx;
    CiInterpreter* ci = ci_from_cpp(cpp);
    CcParser* p = &ci->parser;
    CcFunction* ftype = func->type;
    CcExpr* expr = NULL;
    int err = 0;
    Allocator al = p->cpp.allocator;
    Marray(CcToken)* scratch = cc_get_scratch(p);
    Marray(CcToken) pending = p->pending;
    // Build tokens: EOF funcname ( arg0 , arg1 , ... argN ) in reverse onto pending.
    CcToken eof = {.type = CC_EOF, .loc = loc};
    err = ma_push(CcToken)(scratch, al, eof);
    if(err) goto restore;
    // Push ')'.
    CcToken rparen = {.punct = {.type = CC_PUNCTUATOR, .punct = CC_rparen}, .loc = loc};
    err = ma_push(CcToken)(scratch, al, rparen);
    if(err) goto restore;
    size_t idx = scratch->count;
    {
        const CppToken* arg_toks = args->data;
        const CppToken* end = args->data + args->count;
        while(arg_toks < end){
            CcToken tok;
            err = cpp_next_c_token_array(cpp, &arg_toks, end, &tok);
            if(err) goto restore;
            if(tok.type == CC_EOF)
                break;
            err = ma_push(CcToken)(scratch, al, tok);
            if(err) goto restore;
        }
    }
    // reverse
    for(size_t i = idx, j = scratch->count - 1; i < j; i++, j--){
        CcToken tok = scratch->data[j];
        scratch->data[j] = scratch->data[i];
        scratch->data[i] = tok;
    }
    // Push '('.
    CcToken lparen = {.punct = {.type = CC_PUNCTUATOR, .punct = CC_lparen}, .loc = loc};
    err = ma_push(CcToken)(scratch, al, lparen);
    if(err) goto restore;
    // Push function name identifier.
    CcToken name_tok = {.ident = {.type = CC_IDENTIFIER, .ident = func->name}, .loc = loc};
    err = ma_push(CcToken)(scratch, al, name_tok);
    if(err)goto restore;
    // Parse as expression — the parser will type-check the call.
    p->pending = *scratch;
    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &expr);
    {
        restore:
        *scratch = p->pending;
        p->pending = pending;
        cc_release_scratch(p, scratch);
        if(err) return err;
    }
    uint32_t result_sz;
    err = cc_sizeof_as_uint(p, ftype->return_type, loc, &result_sz);
    if(err) return err;
    _Alignas(16) char result_buf[16];
    char* result = result_buf;
    if(result_sz > 16){
        result = Allocator_zalloc(ci_scratch_allocator(ci), result_sz);
        if(!result){
            return CI_OOM_ERROR;
        }
    }
    err = ci_interp_expr(ci, &ci->top_frame, expr, result, result_sz);
    if(err) goto cleanup;
    CcQualType rt = ftype->return_type;
    Atom a = NULL;
    int tok_type = CPP_NUMBER;
    while(ccqt_kind(rt) == CC_ENUM)
        rt = ccqt_as_enum(rt)->underlying;
    switch(ccqt_kind(rt)){
        case CC_BASIC:
            switch(rt.basic.kind){
                case CCBT_COUNT:
                case CCBT_INVALID:
                case CCBT_nullptr_t:
                case CCBT__Type:
                    err = ci_error(ci, loc, "Invalid return type");
                    goto cleanup;
                case CCBT_void:
                    goto cleanup; // output nothing
                case CCBT_bool:
                    a = *(_Bool*)result?AT_ATOMIZE(cpp->at, "true"):AT_ATOMIZE(cpp->at, "false");
                    tok_type = CPP_IDENTIFIER;
                    break;
                case CCBT_char:
                    if(ci_target(ci)->char_is_signed)
                        goto signed_char;
                    else goto unsigned_char;
                case CCBT_signed_char:
                    signed_char:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_int(result, 1));
                    break;
                case CCBT_unsigned_char:
                    unsigned_char:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_uint(result, 1));
                    break;
                case CCBT_short:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_int(result, ci_target(ci)->sizeof_[CCBT_short]));
                    break;
                case CCBT_unsigned_short:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_uint(result, ci_target(ci)->sizeof_[CCBT_unsigned_short]));
                    break;
                case CCBT_int:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_int(result, ci_target(ci)->sizeof_[CCBT_int]));
                    break;
                case CCBT_unsigned:
                    a = cpp_atomizef(cpp, "%uu", (unsigned)ci_read_uint(result, ci_target(ci)->sizeof_[CCBT_unsigned]));
                    break;
                case CCBT_long:
                    a = cpp_atomizef(cpp, "%lldl", (long long)ci_read_int(result, ci_target(ci)->sizeof_[CCBT_long]));
                    break;
                case CCBT_unsigned_long:
                    a = cpp_atomizef(cpp, "%llulu", (unsigned long long)ci_read_uint(result, ci_target(ci)->sizeof_[CCBT_unsigned_long]));
                    break;
                case CCBT_long_long:
                    a = cpp_atomizef(cpp, "%lldll", (long long)ci_read_int(result, ci_target(ci)->sizeof_[CCBT_long_long]));
                    break;
                case CCBT_unsigned_long_long:
                    a = cpp_atomizef(cpp, "%llullu", (unsigned long long)ci_read_uint(result, ci_target(ci)->sizeof_[CCBT_unsigned_long_long]));
                    break;
                case CCBT_int128:
                case CCBT_unsigned_int128:
                    err = ci_error(ci, loc, "Invalid return type");
                    goto cleanup;
                case CCBT_float16:
                    err = ci_unimplemented(ci, loc, "TODO");
                    goto cleanup;
                case CCBT_float:
                    a = cpp_atomizef(cpp, "%.9gf", (double)*(float*)result);
                    break;
                case CCBT_double:
                    a = cpp_atomizef(cpp, "%.17g", (double)*(double*)result);
                    break;
                case CCBT_long_double:
                case CCBT_float128:
                case CCBT_float_complex:
                case CCBT_double_complex:
                case CCBT_long_double_complex:
                    err = ci_error(ci, loc, "Invalid return type");
                    goto cleanup;
            }
            break;
        case CC_ENUM:
            return CI_UNREACHABLE_ERROR;
        case CC_POINTER:{
            CcPointer* ptr = ccqt_as_ptr(rt);
            if(ccqt_is_basic(ptr->pointee) && ptr->pointee.basic.kind == CCBT_char){
                // string literal
                const char* s = *(const char**)result;
                if(!s){
                    a = AT_ATOMIZE(cpp->at, "NULL");
                    tok_type = CPP_IDENTIFIER;
                }
                else {
                    // Escape the string so cpp_mixin_string's
                    // escape processing reconstructs the original bytes.
                    MStringBuilder sb = {.allocator = ci_scratch_allocator(ci)};
                    msb_write_char(&sb, '"');
                    for(size_t i = 0, slen = strlen(s); i < slen; i++){
                        unsigned char c = (unsigned char)s[i];
                        switch(c){
                            case '\\': msb_write_literal(&sb, "\\\\"); break;
                            case '"':  msb_write_literal(&sb, "\\\""); break;
                            case '\n': msb_write_literal(&sb, "\\n"); break;
                            case '\t': msb_write_literal(&sb, "\\t"); break;
                            case '\r': msb_write_literal(&sb, "\\r"); break;
                            case '\a': msb_write_literal(&sb, "\\a"); break;
                            case '\b': msb_write_literal(&sb, "\\b"); break;
                            case '\f': msb_write_literal(&sb, "\\f"); break;
                            case '\v': msb_write_literal(&sb, "\\v"); break;
                            case '\0': msb_write_literal(&sb, "\\0"); break;
                            default:   msb_write_char(&sb, c); break;
                        }
                    }
                    msb_write_char(&sb, '"');
                    a = msb_atomize(&sb, cpp->at);
                    msb_destroy(&sb);
                    tok_type = CPP_STRING;
                }
                if(!a){
                    err = CI_OOM_ERROR;
                    goto cleanup;
                }
                break;
            }
            err = ci_unimplemented(ci, loc, "Unsupported return type pointer");
            goto cleanup;
        }
        case CC_STRUCT:
        case CC_UNION:
        case CC_FUNCTION:
        case CC_ARRAY:
            err = ci_unimplemented(ci, loc, "Unsupported return type");
            goto cleanup;
    }
    if(!a) {err = CI_OOM_ERROR; goto cleanup;}
    CppToken tok = {
        .type = tok_type,
        .txt = {a->length, a->data},
        .loc = loc,
    };
    err = cpp_push_tok(cpp, outtoks, tok);
    goto cleanup;
    cleanup:
    if(expr) cc_release_expr(&ci->parser, expr);
    if(result != result_buf) Allocator_free(ci_scratch_allocator(ci), result, result_sz);
    return err;
}

static
int
ci_pragma_procmacro(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    int err;
    CiInterpreter* ci = ctx;
    // Skip whitespace.
    while(ntoks && toks->type == CPP_WHITESPACE){ toks++; ntoks--; }
    if(!ntoks || toks->type != CPP_IDENTIFIER)
        return cpp_error(cpp, loc, "#pragma procmacro: expected function name");
    StringView name = toks->txt;
    // Look up the function.
    Atom atom = AT_get_atom(cpp->at, name.text, name.length);
    if(!atom)
        return cpp_error(cpp, loc, "#pragma procmacro: unknown function '%.*s'", (int)name.length, name.text);
    CcFunc* func = cc_scope_lookup_func(&ci->parser.global, atom, CC_SCOPE_NO_WALK);
    if(!func || !func->defined)
        return cpp_error(cpp, loc, "#pragma procmacro: function '%.*s' not defined", (int)name.length, name.text);
    if(!func->parsed){
        err = cc_parse_func_body(&ci->parser, func);
        if(err) return err;
    }
    err = ci_resolve_refs(ci, 1);
    if(err) return err;
    return cpp_define_builtin_func_macro(cpp, name, ci_procmacro_expand, func, func->type->param_count, 0, 0);
}

static
const CcTargetConfig*
ci_target(const CiInterpreter* ci){
    return &ci->parser.cpp.target;
}
static
int
ci_dlsym(CiInterpreter* ci, SrcLoc loc, LongString sym, const char* what, void*_Nullable*_Nonnull out){
    // Try virtual libs first.
    Atom a = AT_get_atom(ci->parser.cpp.at, sym.text, sym.length);
    if(a){
        AtomMapItems vlibs = AM_items(&ci->virtual_libs);
        for(size_t i = 0; i < vlibs.count; i++){
            AtomMap(void*)* symbols = vlibs.data[i].p;
            void* p = AM_get(symbols, a);
            if(p){
                *out = p;
                return 0;
            }
        }
    }
    if(ci_target(ci)->target != CC_TARGET_NATIVE)
        return ci_error(ci, loc, "%s '%s' not found (cross-interpreting)", what, sym.text);
    #ifdef NO_NATIVE_CALL
        (void)out;
        return ci_error(ci, loc, "%s '%s' not found (native calls disabled)", what, sym.text);
    #elif defined _WIN32
        // Try the exe module first, then each dlopen'd library.
        void* p = (void*)GetProcAddress(GetModuleHandleW(NULL), sym.text);
        if(!p){
            AtomMapItems items = AM_items(&ci->opened_libs);
            for(size_t i = 0; !p && i < items.count; i++){
                if(!items.data[i].p) continue;
                p = (void*)GetProcAddress(items.data[i].p, sym.text);
            }
        }
        if(!p) return ci_error(ci, loc, "%s '%s' not found", what, sym.text);
        *out = p;
        return 0;
    #else
        void* p = dlsym(RTLD_DEFAULT, sym.text);
        if(!p && sym.text[0] == '_')
            p = dlsym(RTLD_DEFAULT, sym.text+1);
        if(!p) return ci_error(ci, loc, "%s '%s' not found: %s", what, sym.text, dlerror());
        *out = p;
        return 0;
    #endif
}
static
int
ci_shell(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps){
    (void)arg_seps;
    int err = 0;
    CiInterpreter* ci = ctx;
    Allocator scratch = ci_scratch_allocator(ci);
    LongString output = {0};
    if(!cpp->env){
        return cpp_error(cpp, loc, "__SHELL__: no environment available");
    }
    // Each string token across all args becomes a separate command argument.
    // The first string is the program.
    CmdBuilder cmd = {.allocator = scratch};
    for(size_t i = 0; i < args->count; i++){
        CppToken t = args->data[i];
        if(t.type == CPP_WHITESPACE || t.type == CPP_NEWLINE || t.type == CPP_PUNCTUATOR) continue;
        if(t.type != CPP_STRING || t.txt.length < 2){
            err = cpp_error(cpp, t.loc, "__SHELL__: arguments must be string literals");
            cmd_destroy(&cmd);
            return err;
        }
        Atom a = AT_atomize(cpp->at, t.txt.text + 1, t.txt.length - 2);
        if(!a){ cmd_destroy(&cmd); return CI_OOM_ERROR; }
        if(!cmd.args.count){
            cmd_prog(&cmd, (LongString){a->length, a->data});
            cmd_resolve_prog_path(&cmd, cpp->env, ci_file_exists, NULL);
            if(cmd.errored){
                err = cpp_error(cpp, loc, "__SHELL__: '%s' not found in PATH", a->data);
                cmd_destroy(&cmd);
                return err;
            }
        }
        else {
            cmd_aarg(&cmd, a);
        }
    }
    if(!cmd.args.count){
        return cpp_error(cpp, loc, "__SHELL__: requires at least one argument");
    }
    size_t envp_size = 0;
    void* envp = env_to_envp(cpp->env, scratch, &envp_size);
    int run_err = cmd_run_capture(&cmd, envp, scratch, &output);
    cmd_destroy(&cmd);
    if(envp) Allocator_free(scratch, envp, envp_size);
    if(run_err){
        return cpp_error(cpp, loc, "__SHELL__: command failed");
    }
    // Strip trailing newlines.
    while(output.length > 0 && (output.text[output.length-1] == '\n' || output.text[output.length-1] == '\r'))
        output.length--;
    Atom v = cpp_atomizef(cpp, "\"%.*s\"", (int)output.length, output.text);
    Allocator_free(scratch, output.text, output.length);
    if(!v) return CI_OOM_ERROR;
    CppToken result = {
        .txt = {v->length, v->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    err = cpp_push_tok(cpp, outtoks, result);
    return err;
}

static
int
ci_backtrace(CiInterpreter* ci, CiInterpFrame* f, int level){
    if(!f) return 1;
    if(!f->stmts) return 1;
    if(f->pc >= f->stmt_count) return 1;
    CcStatement* stmt = &f->stmts[f->pc];
    ci_error(ci, stmt->loc, "%d", level);
    if(!f->parent) return 0;
    return ci_backtrace(ci, f->parent, level+1);
}
static
int
ci_register_sym(CiInterpreter*ci, StringView libname, StringView symname, void* sym){
    int err;
    Atom lname = AT_atomize(ci->parser.cpp.at, libname.text, libname.length);
    if(!lname) return CI_OOM_ERROR;
    Atom name = AT_atomize(ci->parser.cpp.at, symname.text, symname.length);
    if(!name) return CI_OOM_ERROR;
    CiVirtualLib* lib = AM_get(&ci->virtual_libs, lname);
    if(!lib){
        lib = Allocator_zalloc(ci_allocator(ci), sizeof *lib);
        if(!lib) return CI_OOM_ERROR;
        err = AM_put(&ci->virtual_libs, ci_allocator(ci), lname, lib);
        if(err){
            Allocator_free(ci_allocator(ci), lib, sizeof *lib);
            return CI_OOM_ERROR;
        }
    }
    err = AM_put(&lib->symbols, ci_allocator(ci), name, sym);
    if(err) return CI_OOM_ERROR;
    return 0;
}
static
AtomTable*
ci_lock_atoms(CiInterpreter* ci){
    LOCK_T_lock(&ci->atom_lock);
    AtomTable* at = ci->parser.cpp.at;
    ci->parser.cpp.at = NULL;
    return at;
}
static
void
ci_unlock_atoms(CiInterpreter* ci, AtomTable* at){
    ci->parser.cpp.at = at;
    LOCK_T_unlock(&ci->atom_lock);
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
