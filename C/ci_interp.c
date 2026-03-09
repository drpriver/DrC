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
#include "../Drp/Allocators/allocator.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/MStringBuilder16.h"
#include "../Drp/cmd_run.h"
#include "../Drp/stringview.h"
#include "../Drp/argument_parsing.h"
#include "../Drp/bit_util.h"
#include "native_call.h"
#include "ci_softnum.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
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
static const CcTargetConfig* ci_target(const CiInterpreter*);
static int ci_dlsym(CiInterpreter*, SrcLoc, LongString, const char* what, void*_Nullable*_Nonnull);
static int ci_interp_call(CiInterpreter*, CiInterpFrame* caller, CcFunc*, CcExpr*_Nonnull* _Nonnull args, uint32_t nargs, void* result, size_t size, CiInterpFrame*_Nullable*_Nonnull out_frame);
// re-declare here as I'm not sure if this should be used in the interpreter or not
static int cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);

static CppFuncMacroFn ci_shell;

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
            uint64_t idx = 0;
            uint32_t idx_sz;
            err = cc_sizeof_as_uint(&ci->parser, idx_expr->type, expr->loc, &idx_sz);
            if(err) return err;
            err = ci_interp_expr(ci, frame,idx_expr, &idx, sizeof idx);
            if(err) return err;
            idx = ci_read_uint(&idx, idx_sz);
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
                if(!skip_check && idx * elem_sz + elem_sz > base_size)
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
        // ICE: should have been resolved before execution.
        return;
    }
    size_t alloc_size = sizeof(CiInterpFrame) + func->frame_size;
    CiInterpFrame* frame = Allocator_zalloc(ci_allocator(ci), alloc_size);
    if(!frame) return;
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
    Allocator_free(ci_allocator(ci), frame, alloc_size);
}

// Create a native closure for an interpreted function, storing the
// resulting function pointer in func->native_func.
static
int
ci_create_closure(CiInterpreter* ci, CcFunc* func){
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
                return ci_error(ci, expr->loc, "interpreter: result buffer too small");
            uint32_t len = expr->str.length;
            if(len > sz) len = sz;
            memcpy(result, expr->text, len);
            return 0;
        }
        if(sz > size)
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
        // Array variables decay to pointer
        if(ccqt_kind(var_type) == CC_ARRAY){
            if(sizeof storage > size)
                return ci_error(ci, expr->loc, "interpreter: result buffer too small");
            memcpy(result, &storage, sizeof storage);
            return 0;
        }
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, var_type, expr->loc, &sz);
        if(err) return err;
        if(sz > size)
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
        // Array-to-pointer decay: get address of array data.
        if(ccqt_kind(from) == CC_ARRAY){
            if(result == ci_discard_buf) return 0;
            if(sizeof(void*) > size)
                return ci_error(ci, expr->loc, "interpreter: result buffer too small");
            void* ptr;
            size_t lval_size;
            int err = ci_interp_lvalue(ci, frame, operand, &ptr, &lval_size);
            if(err) return err;
            memcpy(result, &ptr, sizeof ptr);
            return 0;
        }
        uint64_t val = 0;
        int err = ci_interp_expr(ci, frame, operand, &val, sizeof val);
        if(err) return err;
        if(result == ci_discard_buf) return 0;
        uint32_t from_sz;
        err = cc_sizeof_as_uint(&ci->parser, from, expr->loc, &from_sz);
        if(err) return err;
        uint32_t to_sz;
        err = cc_sizeof_as_uint(&ci->parser, to, expr->loc, &to_sz);
        if(err) return err;
        if(to_sz > size)
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
        _Bool from_is_float = ccqt_is_basic(from) && ccbt_is_float(from.basic.kind);
        _Bool to_is_float = ccqt_is_basic(to) && ccbt_is_float(to.basic.kind);
        if(from_is_float && to_is_float){
            double d = ci_read_float(&val, from.basic.kind);
            ci_write_float(result, to.basic.kind, d);
        }
        else if(from_is_float && !to_is_float){
            double d = ci_read_float(&val, from.basic.kind);
            if(ccqt_is_basic(to) && ccbt_is_unsigned(to.basic.kind))
                ci_write_uint(result, to_sz, (uint64_t)d);
            else
                ci_write_uint(result, to_sz, (uint64_t)(int64_t)d);
        }
        else if(!from_is_float && to_is_float){
            _Bool from_unsigned = ccqt_is_basic(from) && ccbt_is_unsigned(from.basic.kind);
            double d;
            if(from_unsigned)
                d = (double)ci_read_uint(&val, from_sz);
            else
                d = (double)ci_read_int(&val, from_sz);
            ci_write_float(result, to.basic.kind, d);
        }
        else {
            _Bool from_unsigned = ccqt_is_basic(from) && ccbt_is_unsigned(from.basic.kind);
            if(from_unsigned)
                ci_write_uint(result, to_sz, ci_read_uint(&val, from_sz));
            else
                ci_write_uint(result, to_sz, (uint64_t)ci_read_int(&val, from_sz));
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
        memcpy(result, ptr_val, sz);
        return 0;
    }
    case CC_EXPR_DOT: {
        if(result == ci_discard_buf) return 0;
        void* base;
        size_t base_size;
        int err = ci_interp_lvalue(ci, frame, expr->values[0], &base, &base_size);
        if(err) return err;
        uint64_t off = expr->field_loc.byte_offset;
        uint32_t sz;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
        if(err) return err;
        if(off + sz > base_size)
            return ci_error(ci, expr->loc, "interpreter: field access out of bounds");
        if(sz > size)
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
        uint64_t lbuf = 0, rbuf = 0;
        uint32_t lsz, rsz, result_sz;
        int err = cc_sizeof_as_uint(&ci->parser, lhs->type, expr->loc, &lsz);
        if(err) return err;
        err = cc_sizeof_as_uint(&ci->parser, rhs->type, expr->loc, &rsz);
        if(err) return err;
        err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &result_sz);
        if(err) return err;
        if(result_sz > size)
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
        if(expr->kind == CC_EXPR_LOGAND){
            err = ci_interp_expr(ci, frame,lhs, &lbuf, sizeof lbuf);
            if(err) return err;
            if(!ci_read_uint(&lbuf, lsz)){
                ci_write_uint(result, result_sz, 0);
                return 0;
            }
            err = ci_interp_expr(ci, frame,rhs, &rbuf, sizeof rbuf);
            if(err) return err;
            ci_write_uint(result, result_sz, ci_read_uint(&rbuf, rsz) ? 1 : 0);
            return 0;
        }
        if(expr->kind == CC_EXPR_LOGOR){
            err = ci_interp_expr(ci, frame,lhs, &lbuf, sizeof lbuf);
            if(err) return err;
            if(ci_read_uint(&lbuf, lsz)){
                ci_write_uint(result, result_sz, 1);
                return 0;
            }
            err = ci_interp_expr(ci, frame,rhs, &rbuf, sizeof rbuf);
            if(err) return err;
            ci_write_uint(result, result_sz, ci_read_uint(&rbuf, rsz) ? 1 : 0);
            return 0;
        }
        err = ci_interp_expr(ci, frame,lhs, &lbuf, sizeof lbuf);
        if(err) return err;
        err = ci_interp_expr(ci, frame,rhs, &rbuf, sizeof rbuf);
        if(err) return err;
        _Bool lhs_ptr = ccqt_kind(lhs->type) == CC_POINTER || ccqt_kind(lhs->type) == CC_ARRAY;
        _Bool rhs_ptr = ccqt_kind(rhs->type) == CC_POINTER || ccqt_kind(rhs->type) == CC_ARRAY;
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
                memcpy(&ptr, &lbuf, sizeof ptr);
                int64_t idx = ci_read_int(&rbuf, rsz);
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
                memcpy(&ptr, &rbuf, sizeof ptr);
                int64_t idx = ci_read_int(&lbuf, lsz);
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
                memcpy(&ptr, &lbuf, sizeof ptr);
                int64_t idx = ci_read_int(&rbuf, rsz);
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
                memcpy(&lp, &lbuf, sizeof lp);
                memcpy(&rp, &rbuf, sizeof rp);
                int64_t diff = (lp - rp) / (int64_t)elem_sz;
                ci_write_uint(result, result_sz, (uint64_t)diff);
                return 0;
            }
            char* lp = NULL, *rp = NULL;
            memcpy(&lp, &lbuf, sizeof lp);
            memcpy(&rp, &rbuf, sizeof rp);
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
            double ld = ci_read_float(&lbuf, lhs->type.basic.kind);
            double rd = ci_read_float(&rbuf, rhs->type.basic.kind);
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
        _Bool is_unsigned = ccqt_is_basic(lhs->type) && ccbt_is_unsigned(lhs->type.basic.kind);
        uint64_t lu, ru;
        if(is_unsigned){
            lu = ci_read_uint(&lbuf, lsz);
            ru = ci_read_uint(&rbuf, rsz);
        }
        else {
            lu = (uint64_t)ci_read_int(&lbuf, lsz);
            ru = (uint64_t)ci_read_int(&rbuf, rsz);
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
                        Allocator_free(ci_allocator(ci), callee_frame, sizeof(CiInterpFrame) + callee_frame->data_length);
                        return err;
                    }
                }
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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

        if(op == CC_ATOMIC_LOAD){
            switch(sz){
                case 1:  *( uint8_t*)result = __atomic_load_n(( uint8_t*)ptr, __ATOMIC_SEQ_CST); break;
                case 2:  *(uint16_t*)result = __atomic_load_n((uint16_t*)ptr, __ATOMIC_SEQ_CST); break;
                case 4:  *(uint32_t*)result = __atomic_load_n((uint32_t*)ptr, __ATOMIC_SEQ_CST); break;
                case 8:  *(uint64_t*)result = __atomic_load_n((uint64_t*)ptr, __ATOMIC_SEQ_CST); break;
                case 16: __atomic_load((Atomic16*)ptr, (Atomic16*)result, __ATOMIC_SEQ_CST); break;
                default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz);
            }
            return 0;
        }
        if(op == CC_ATOMIC_COMPARE_EXCHANGE){
            void* expected_ptr = NULL;
            err = ci_interp_expr(ci, frame, expr->values[0], &expected_ptr, sizeof expected_ptr);
            if(err) return err;
            _Alignas(16) char desired_buf[16] = {0};
            err = ci_interp_expr(ci, frame, expr->values[1], desired_buf, sz);
            if(err) return err;
            _Bool r;
            switch(sz){
                case 1:  r = __atomic_compare_exchange_n(( uint8_t*)ptr, ( uint8_t*)expected_ptr, *( uint8_t*)desired_buf, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                case 2:  r = __atomic_compare_exchange_n((uint16_t*)ptr, (uint16_t*)expected_ptr, *(uint16_t*)desired_buf, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                case 4:  r = __atomic_compare_exchange_n((uint32_t*)ptr, (uint32_t*)expected_ptr, *(uint32_t*)desired_buf, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                case 8:  r = __atomic_compare_exchange_n((uint64_t*)ptr, (uint64_t*)expected_ptr, *(uint64_t*)desired_buf, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                case 16: r = __atomic_compare_exchange((Atomic16*)ptr, (Atomic16*)expected_ptr, (Atomic16*)desired_buf, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz);
            }
            *(_Bool*)result = r;
            return 0;
        }
        // fetch_add, fetch_sub, store, exchange: values[0]=val
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
            case CC_ATOMIC_EXCHANGE:
                switch(sz){
                    case 1:  *( uint8_t*)result = __atomic_exchange_n(( uint8_t*)ptr, *( uint8_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 2:  *(uint16_t*)result = __atomic_exchange_n((uint16_t*)ptr, *(uint16_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 4:  *(uint32_t*)result = __atomic_exchange_n((uint32_t*)ptr, *(uint32_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 8:  *(uint64_t*)result = __atomic_exchange_n((uint64_t*)ptr, *(uint64_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 16: __atomic_exchange((Atomic16*)ptr, (Atomic16*)val_buf, (Atomic16*)result, __ATOMIC_SEQ_CST); break;
                    default: return ci_error(ci, expr->loc, "unsupported atomic operand size %u", sz);
                }
                break;
            case CC_ATOMIC_STORE:
                switch(sz){
                    case 1:  __atomic_store_n(( uint8_t*)ptr, *( uint8_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 2:  __atomic_store_n((uint16_t*)ptr, *(uint16_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 4:  __atomic_store_n((uint32_t*)ptr, *(uint32_t*)val_buf, __ATOMIC_SEQ_CST); break;
                    case 8:  __atomic_store_n((uint64_t*)ptr, *(uint64_t*)val_buf, __ATOMIC_SEQ_CST); break;
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
            void* ap_addr;
            size_t ap_size;
            int err = ci_interp_lvalue(ci, frame, expr->lhs, &ap_addr, &ap_size);
            if(err) return err;
            if(!frame->varargs_buf)
                return ci_error(ci, expr->loc, "va_start used in non-variadic function");
            switch(ci_target(ci)->target){
            case CC_TARGET_AARCH64_MACOS:
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST: {
                // va_list is a pointer.
                void* va_ptr = frame->varargs_buf;
                memcpy(ap_addr, &va_ptr, sizeof(void*));
                return 0;
            }
            case CC_TARGET_AARCH64_LINUX: {
                // Mark all register slots as used so va_arg always reads from __stack.
                CiAapcs64VaList* va = ap_addr;
                va->__stack = frame->varargs_buf;
                va->__gr_top = NULL;
                va->__vr_top = NULL;
                va->__gr_offs = 0;
                va->__vr_offs = 0;
                return 0;
            }
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS: {
                // Mark all register slots as used so va_arg always reads from overflow.
                CiSysvVaListTag* tag = ap_addr;
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
            void* ap_addr;
            size_t ap_size;
            int err = ci_interp_lvalue(ci, frame, expr->lhs, &ap_addr, &ap_size);
            if(err) return err;
            uint32_t sz;
            err = cc_sizeof_as_uint(&ci->parser, expr->type, expr->loc, &sz);
            if(err) return err;
            uint32_t advance = sz < 8 ? 8 : (sz + 7) & ~7u;
            switch(ci_target(ci)->target){
            case CC_TARGET_AARCH64_MACOS:
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST: {
                // va_list is a pointer.
                void* cur;
                memcpy(&cur, ap_addr, sizeof(void*));
                if(result != ci_discard_buf){
                    if(sz > size)
                        return ci_error(ci, expr->loc, "interpreter: result buffer too small");
                    memcpy(result, cur, sz);
                }
                cur = (char*)cur + advance;
                memcpy(ap_addr, &cur, sizeof(void*));
                return 0;
            }
            case CC_TARGET_AARCH64_LINUX: {
                CiAapcs64VaList* va = ap_addr;
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
                        return ci_error(ci, expr->loc, "interpreter: result buffer too small");
                    memcpy(result, src, sz);
                }
                return 0;
            }
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS: {
                CiSysvVaListTag* tag = ap_addr;
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
                        return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
            void* dest_addr;
            size_t dest_size;
            int err = ci_interp_lvalue(ci, frame, expr->lhs, &dest_addr, &dest_size);
            if(err) return err;
            switch(ci_target(ci)->target){
            case CC_TARGET_AARCH64_MACOS:
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST: {
                // va_list is a pointer.
                void* src_val;
                err = ci_interp_expr(ci, frame, expr->values[0], &src_val, sizeof src_val);
                if(err) return err;
                memcpy(dest_addr, &src_val, sizeof(void*));
                return 0;
            }
            case CC_TARGET_AARCH64_LINUX: {
                void* src_addr;
                size_t src_size;
                err = ci_interp_lvalue(ci, frame, expr->values[0], &src_addr, &src_size);
                if(err) return err;
                *(CiAapcs64VaList*)dest_addr = *(CiAapcs64VaList*)src_addr;
                return 0;
            }
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS: {
                void* src_addr;
                size_t src_size;
                err = ci_interp_lvalue(ci, frame, expr->values[0], &src_addr, &src_size);
                if(err) return err;
                *(CiSysvVaListTag*)dest_addr = *(CiSysvVaListTag*)src_addr;
                return 0;
            }
            case CC_TARGET_COUNT:
                break;
            }
            return ci_error(ci, expr->loc, "va_copy: unsupported target");
        }
        }
        return ci_error(ci, expr->loc, "interpreter: unsupported va operation");
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
        }
        return ci_error(ci, expr->loc, "interpreter: unsupported builtin");
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
        _Bool a_unsigned = ccqt_is_basic(expr->lhs->type) && ccbt_is_unsigned(expr->lhs->type.basic.kind);
        CiInt128 a = a_unsigned ? ci_int128_from_uint64(ci_read_uint(&abuf, asz))
                              : ci_int128_from_int64(ci_read_int(&abuf, asz));
        // Read b
        uint64_t bbuf = 0;
        uint32_t bsz;
        err = cc_sizeof_as_uint(&ci->parser, expr->values[0]->type, expr->loc, &bsz);
        if(err) return err;
        err = ci_interp_expr(ci, frame, expr->values[0], &bbuf, sizeof bbuf);
        if(err) return err;
        _Bool b_unsigned = ccqt_is_basic(expr->values[0]->type) && ccbt_is_unsigned(expr->values[0]->type.basic.kind);
        CiInt128 b = b_unsigned ? ci_int128_from_uint64(ci_read_uint(&bbuf, bsz))
                              : ci_int128_from_int64(ci_read_int(&bbuf, bsz));
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
        _Bool dest_unsigned = ccqt_is_basic(dest_type) && ccbt_is_unsigned(dest_type.basic.kind);
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
                return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
                return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
                return ci_error(ci, expr->loc, "interpreter: result buffer too small");
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
    Atom atom = AT_atomize(ci->parser.cpp.at, name.text, name.length);
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
    Allocator_free(ci_allocator(ci), frame, alloc_size);
    return err;
}

static
int
ci_call_main(CiInterpreter* ci, int argc, char*_Null_unspecified*_Null_unspecified argv, char*_Null_unspecified*_Null_unspecified envp, int* out_ret){
    Atom atom = AT_atomize(ci->parser.cpp.at, "main", 4);
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
ci_resolve_refs(CiInterpreter* ci){
    CcParser* p = &ci->parser;
    Allocator al = ci_allocator(ci);
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
    // Worklist: iterate used_funcs. Parsing bodies may add new entries,
    // so we re-check count each iteration.
    for(size_t i = 0; i < p->used_funcs.count; i++){
        PointerMapItems items = PM_items(&p->used_funcs);
        CcFunc* func = (CcFunc*)(uintptr_t)items.data[i].key;
        if(!func->defined) {
            // Extern: resolve via dlsym.
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
        if(!func->parsed){
            int err = cc_parse_func_body(p, func);
            if(err) return err;
            // Parsing may have grown used_funcs/used_vars, loop will pick them up.
        }
        // Create a native closure only if the function's address is taken.
        if(!func->native_func && func->addr_taken){
            if(func->type->is_variadic)
                return ci_error(ci, func->loc, "cannot take address of variadic interpreted function");
            int err = ci_create_closure(ci, func);
            if(err) return err;
        }
    }
    // Resolve non-automatic variable storage.
    {
        PointerMapItems items = PM_items(&p->used_vars);
        for(size_t i = 0; i < items.count; i++){
            CcVariable* var = (CcVariable*)(uintptr_t)items.data[i].key;
            if(var->interp_val) continue;
            if(var->extern_){
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
    }
    // Pre-populate ffi_cache for non-variadic call types.
    // Collect all CcFunction* that need a CIF: extern funcs + indirect call types.
    {
        // From extern functions.
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
        // From indirect call types.
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
    // Pre-populate ffi_cache for variadic call expressions, keyed by CcExpr*.
    // Skip calls to interpreted (defined) functions — they go through ci_interp_call.
    {
        PointerMapItems vcalls = PM_items(&p->used_var_calls);
        for(size_t i = 0; i < vcalls.count; i++){
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
    }
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

static CppPragmaFn ci_pragma_lib, ci_pragma_lib_path, ci_pragma_pkg_config;
static
int
ci_register_pragmas(CiInterpreter*ci){
    int err = 0;
    err = cpp_register_pragma(&ci->parser.cpp, SV("lib"), ci_pragma_lib, ci);
    if(err) return err;
    err = cpp_register_pragma(&ci->parser.cpp, SV("lib_path"), ci_pragma_lib_path, ci);
    if(err) return err;
    err = cpp_register_pragma(&ci->parser.cpp, SV("pkg_config"), ci_pragma_pkg_config, ci);
    return err;
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
            if(err) goto finally;
        }
    }
    finally:
    if(output.text) Allocator_free(scratch, output.text, output.length+1);
    cpp_release_scratch(cpp, expanded);
    return err;
}

static
const CcTargetConfig*
ci_target(const CiInterpreter* ci){
    return &ci->parser.cpp.target;
}
static
int
ci_dlsym(CiInterpreter* ci, SrcLoc loc, LongString sym, const char* what, void*_Nullable*_Nonnull out){
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
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
