//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <string.h>
#include <stddef.h>
#include <dlfcn.h>
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static inline
uint64_t
interp_read_uint(const void* buf, uint32_t sz){
    uint64_t v = 0;
    memcpy(&v, buf, sz);
    return v;
}

static inline
int64_t
interp_read_int(const void* buf, uint32_t sz){
    switch(sz){
        case 1: return *(const int8_t*)buf;
        case 2: return *(const int16_t*)buf;
        case 4: return *(const int32_t*)buf;
        case 8: return *(const int64_t*)buf;
    }
    // fallback
    uint64_t v = 0;
    memcpy(&v, buf, sz);
    return (int64_t)v;
}

static inline
void
interp_write_uint(void* buf, uint32_t sz, uint64_t val){
    memcpy(buf, &val, sz);
}

static inline
double
interp_read_float(const void* buf, CcBasicTypeKind k){
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
interp_write_float(void* buf, CcBasicTypeKind k, double val){
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
interp_var_storage(CcParser* p, CcVariable* var){
    for(CcInterpFrame* f = p->current_frame; f; f = f->parent){
        void* storage = PM_get(&f->locals, var);
        if(storage) return storage;
    }
    return var->interp_val;
}

static
int
interp_ensure_global_storage(CcParser* p, CcVariable* var){
    if(var->interp_val) return 0;
    uint32_t sz;
    int err = cc_sizeof_as_uint(p, var->type, var->loc, &sz);
    if(err) return err;
    var->interp_val = Allocator_zalloc(cc_allocator(p), sz);
    if(!var->interp_val) return CC_OOM_ERROR;
    return 0;
}

static inline
Atom
interp_recover_atom(const char* text){
    return (Atom)(text - offsetof(Atom_, data));
}

static
CcField* _Nullable
interp_find_field(CcExpr* expr, CcQualType agg_type){
    Atom name = interp_recover_atom(expr->text);
    CcTypeKind tk = ccqt_kind(agg_type);
    if(tk == CC_STRUCT){
        CcStruct* s = ccqt_as_struct(agg_type);
        return cc_lookup_field(s->fields, s->field_count, name);
    }
    if(tk == CC_UNION){
        CcUnion* u = ccqt_as_union(agg_type);
        return cc_lookup_field(u->fields, u->field_count, name);
    }
    return NULL;
}

static
int
cc_interp_lvalue(CcParser* p, CcExpr* expr, void*_Nullable*_Nonnull out){
    switch(expr->kind){
        case CC_EXPR_VARIABLE: {
            CcVariable* var = expr->var;
            int err = interp_ensure_global_storage(p, var);
            if(err) return err;
            void* storage = interp_var_storage(p, var);
            if(!storage)
                return cc_error(p, expr->loc, "variable '%s' has no storage", var->name->data);
            *out = storage;
            return 0;
        }
        case CC_EXPR_DEREF: {
            // *ptr: evaluate ptr, return the pointer value
            void* ptr_val = NULL;
            int err = cc_interp_expr(p, expr->value0, &ptr_val);
            if(err) return err;
            *out = ptr_val;
            return 0;
        }
        case CC_EXPR_DOT: {
            // base.member: get lvalue of base, add field offset
            void* base;
            int err = cc_interp_lvalue(p, expr->values[0], &base);
            if(err) return err;
            CcQualType agg_type = expr->values[0]->type;
            CcField* f = interp_find_field(expr, agg_type);
            if(!f)
                return cc_error(p, expr->loc, "no member named '%.*s'", expr->extra, expr->text);
            *out = (char*)base + f->offset;
            return 0;
        }
        case CC_EXPR_ARROW: {
            // ptr->member: eval ptr, add field offset
            void* ptr_val = NULL;
            int err = cc_interp_expr(p, expr->values[0], &ptr_val);
            if(err) return err;
            CcQualType ptr_type = expr->values[0]->type;
            CcPointer* pt = ccqt_as_ptr(ptr_type);
            CcField* f = interp_find_field(expr, pt->pointee);
            if(!f)
                return cc_error(p, expr->loc, "no member named '%.*s'", expr->extra, expr->text);
            *out = (char*)ptr_val + f->offset;
            return 0;
        }
        case CC_EXPR_SUBSCRIPT: {
            // base[index]
            CcExpr* base_expr = expr->value0;
            CcExpr* idx_expr = expr->values[0];
            CcQualType base_type = base_expr->type;
            uint32_t elem_sz;
            int err = cc_sizeof_as_uint(p, expr->type, expr->loc, &elem_sz);
            if(err) return err;
            uint64_t idx = 0;
            uint32_t idx_sz;
            err = cc_sizeof_as_uint(p, idx_expr->type, expr->loc, &idx_sz);
            if(err) return err;
            err = cc_interp_expr(p, idx_expr, &idx);
            if(err) return err;
            idx = interp_read_uint(&idx, idx_sz);
            if(ccqt_kind(base_type) == CC_ARRAY){
                // Array: get lvalue of base, index into it
                void* base;
                err = cc_interp_lvalue(p, base_expr, &base);
                if(err) return err;
                *out = (char*)base + idx * elem_sz;
            }
            else {
                // Pointer: eval base as rvalue
                void* ptr_val = NULL;
                err = cc_interp_expr(p, base_expr, &ptr_val);
                if(err) return err;
                *out = (char*)ptr_val + idx * elem_sz;
            }
            return 0;
        }
        default:
            return cc_error(p, expr->loc, "expression is not an lvalue");
    }
}

static
int
cc_interp_expr(CcParser* p, CcExpr* expr, void* result){
    switch(expr->kind){
    case CC_EXPR_VALUE: {
        uint32_t sz;
        int err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        if(ccqt_kind(expr->type) == CC_ARRAY){
            const void* ptr = expr->text;
            memcpy(result, &ptr, sizeof ptr);
            return 0;
        }
        memcpy(result, &expr->uinteger, sz);
        return 0;
    }
    case CC_EXPR_VARIABLE: {
        CcVariable* var = expr->var;
        int err = interp_ensure_global_storage(p, var);
        if(err) return err;
        void* storage = interp_var_storage(p, var);
        if(!storage)
            return cc_error(p, expr->loc, "variable '%s' has no storage", var->name->data);
        CcQualType var_type = var->type;
        // Array variables decay to pointer
        if(ccqt_kind(var_type) == CC_ARRAY){
            memcpy(result, &storage, sizeof storage);
            return 0;
        }
        uint32_t sz;
        err = cc_sizeof_as_uint(p, var_type, expr->loc, &sz);
        if(err) return err;
        memcpy(result, storage, sz);
        return 0;
    }
    case CC_EXPR_FUNCTION: {
        CcFunc* func = expr->func;
        void (*fn)(void) = func->native_func;
        memcpy(result, &fn, sizeof fn);
        return 0;
    }
    case CC_EXPR_CAST: {
        CcExpr* operand = expr->value0;
        CcQualType from = operand->type;
        CcQualType to = expr->type;
        if(ccqt_is_basic(to) && to.basic.kind == CCBT_void){
            uint64_t discard;
            return cc_interp_expr(p, operand, &discard);
        }
        uint64_t val = 0;
        uint32_t from_sz;
        int err = cc_sizeof_as_uint(p, from, expr->loc, &from_sz);
        if(err) return err;
        err = cc_interp_expr(p, operand, &val);
        if(err) return err;
        uint32_t to_sz;
        err = cc_sizeof_as_uint(p, to, expr->loc, &to_sz);
        if(err) return err;
        _Bool from_is_float = ccqt_is_basic(from) && ccbt_is_float(from.basic.kind);
        _Bool to_is_float = ccqt_is_basic(to) && ccbt_is_float(to.basic.kind);
        if(from_is_float && to_is_float){
            double d = interp_read_float(&val, from.basic.kind);
            interp_write_float(result, to.basic.kind, d);
        }
        else if(from_is_float && !to_is_float){
            double d = interp_read_float(&val, from.basic.kind);
            if(ccqt_is_basic(to) && ccbt_is_unsigned(to.basic.kind))
                interp_write_uint(result, to_sz, (uint64_t)d);
            else
                interp_write_uint(result, to_sz, (uint64_t)(int64_t)d);
        }
        else if(!from_is_float && to_is_float){
            _Bool from_unsigned = ccqt_is_basic(from) && ccbt_is_unsigned(from.basic.kind);
            double d;
            if(from_unsigned)
                d = (double)interp_read_uint(&val, from_sz);
            else
                d = (double)interp_read_int(&val, from_sz);
            interp_write_float(result, to.basic.kind, d);
        }
        else {
            interp_write_uint(result, to_sz, interp_read_uint(&val, from_sz));
        }
        return 0;
    }
    case CC_EXPR_ASSIGN: {
        void* lval;
        int err = cc_interp_lvalue(p, expr->value0, &lval);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        err = cc_interp_expr(p, expr->values[0], lval);
        if(err) return err;
        memcpy(result, lval, sz);
        return 0;
    }
    case CC_EXPR_COMMA: {
        uint64_t discard;
        int err = cc_interp_expr(p, expr->value0, &discard);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        return cc_interp_expr(p, expr->values[0], result);
    }
    case CC_EXPR_TERNARY: {
        uint64_t cond = 0;
        uint32_t cond_sz;
        int err = cc_sizeof_as_uint(p, expr->value0->type, expr->loc, &cond_sz);
        if(err) return err;
        err = cc_interp_expr(p, expr->value0, &cond);
        if(err) return err;
        if(interp_read_uint(&cond, cond_sz))
            return cc_interp_expr(p, expr->values[0], result);
        else
            return cc_interp_expr(p, expr->values[1], result);
    }
    case CC_EXPR_ADDR: {
        void* lval;
        int err = cc_interp_lvalue(p, expr->value0, &lval);
        if(err) return err;
        memcpy(result, &lval, sizeof lval);
        return 0;
    }
    case CC_EXPR_DEREF: {
        void* ptr_val = NULL;
        int err = cc_interp_expr(p, expr->value0, &ptr_val);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        memcpy(result, ptr_val, sz);
        return 0;
    }
    case CC_EXPR_DOT: {
        void* base;
        int err = cc_interp_lvalue(p, expr->values[0], &base);
        if(err) return err;
        CcField* f = interp_find_field(expr, expr->values[0]->type);
        if(!f)
            return cc_error(p, expr->loc, "no member named '%.*s'", expr->extra, expr->text);
        uint32_t sz;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        memcpy(result, (char*)base + f->offset, sz);
        return 0;
    }
    case CC_EXPR_ARROW: {
        void* ptr_val = NULL;
        int err = cc_interp_expr(p, expr->values[0], &ptr_val);
        if(err) return err;
        CcPointer* pt = ccqt_as_ptr(expr->values[0]->type);
        CcField* f = interp_find_field(expr, pt->pointee);
        if(!f)
            return cc_error(p, expr->loc, "no member named '%.*s'", expr->extra, expr->text);
        uint32_t sz;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        memcpy(result, (char*)ptr_val + f->offset, sz);
        return 0;
    }
    case CC_EXPR_SUBSCRIPT: {
        void* addr;
        int err = cc_interp_lvalue(p, expr, &addr);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        memcpy(result, addr, sz);
        return 0;
    }
    case CC_EXPR_NEG: {
        uint64_t val = 0;
        uint32_t sz;
        int err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        err = cc_interp_expr(p, expr->value0, &val);
        if(err) return err;
        if(ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind)){
            double d = -interp_read_float(&val, expr->type.basic.kind);
            interp_write_float(result, expr->type.basic.kind, d);
        }
        else {
            int64_t v = -interp_read_int(&val, sz);
            interp_write_uint(result, sz, (uint64_t)v);
        }
        return 0;
    }
    case CC_EXPR_POS: {
        uint32_t sz;
        int err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        return cc_interp_expr(p, expr->value0, result);
    }
    case CC_EXPR_BITNOT: {
        uint64_t val = 0;
        uint32_t sz;
        int err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        err = cc_interp_expr(p, expr->value0, &val);
        if(err) return err;
        uint64_t v = ~interp_read_uint(&val, sz);
        interp_write_uint(result, sz, v);
        return 0;
    }
    case CC_EXPR_LOGNOT: {
        uint64_t val = 0;
        uint32_t sz;
        int err = cc_sizeof_as_uint(p, expr->value0->type, expr->loc, &sz);
        if(err) return err;
        err = cc_interp_expr(p, expr->value0, &val);
        if(err) return err;
        _Bool v = !interp_read_uint(&val, sz);
        uint32_t rsz;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &rsz);
        if(err) return err;
        interp_write_uint(result, rsz, v);
        return 0;
    }
    case CC_EXPR_PREINC:
    case CC_EXPR_PREDEC:
    case CC_EXPR_POSTINC:
    case CC_EXPR_POSTDEC: {
        void* lval;
        int err = cc_interp_lvalue(p, expr->value0, &lval);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        _Bool is_float = ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind);
        _Bool is_pre = (expr->kind == CC_EXPR_PREINC || expr->kind == CC_EXPR_PREDEC);
        _Bool is_inc = (expr->kind == CC_EXPR_PREINC || expr->kind == CC_EXPR_POSTINC);
        if(is_float){
            double d = interp_read_float(lval, expr->type.basic.kind);
            double old = d;
            d += is_inc ? 1.0 : -1.0;
            interp_write_float(lval, expr->type.basic.kind, d);
            interp_write_float(result, expr->type.basic.kind, is_pre ? d : old);
        }
        else if(ccqt_kind(expr->type) == CC_POINTER){
            // pointer increment: add sizeof(pointee)
            CcPointer* pt = ccqt_as_ptr(expr->type);
            uint32_t pointee_sz;
            err = cc_sizeof_as_uint(p, pt->pointee, expr->loc, &pointee_sz);
            if(err) return err;
            void* ptr = NULL;
            memcpy(&ptr, lval, sizeof ptr);
            void* old = ptr;
            ptr = (char*)ptr + (is_inc ? (int)pointee_sz : -(int)pointee_sz);
            memcpy(lval, &ptr, sizeof ptr);
            void* out = is_pre ? ptr : old;
            memcpy(result, &out, sizeof out);
        }
        else {
            uint64_t v = interp_read_uint(lval, sz);
            uint64_t old = v;
            v += is_inc ? 1 : (uint64_t)-1;
            interp_write_uint(lval, sz, v);
            interp_write_uint(result, sz, is_pre ? v : old);
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
        CcExpr* lhs = expr->value0;
        CcExpr* rhs = expr->values[0];
        uint64_t lbuf = 0, rbuf = 0;
        uint32_t lsz, rsz, result_sz;
        int err = cc_sizeof_as_uint(p, lhs->type, expr->loc, &lsz);
        if(err) return err;
        err = cc_sizeof_as_uint(p, rhs->type, expr->loc, &rsz);
        if(err) return err;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &result_sz);
        if(err) return err;
        if(expr->kind == CC_EXPR_LOGAND){
            err = cc_interp_expr(p, lhs, &lbuf);
            if(err) return err;
            if(!interp_read_uint(&lbuf, lsz)){
                interp_write_uint(result, result_sz, 0);
                return 0;
            }
            err = cc_interp_expr(p, rhs, &rbuf);
            if(err) return err;
            interp_write_uint(result, result_sz, interp_read_uint(&rbuf, rsz) ? 1 : 0);
            return 0;
        }
        if(expr->kind == CC_EXPR_LOGOR){
            err = cc_interp_expr(p, lhs, &lbuf);
            if(err) return err;
            if(interp_read_uint(&lbuf, lsz)){
                interp_write_uint(result, result_sz, 1);
                return 0;
            }
            err = cc_interp_expr(p, rhs, &rbuf);
            if(err) return err;
            interp_write_uint(result, result_sz, interp_read_uint(&rbuf, rsz) ? 1 : 0);
            return 0;
        }
        err = cc_interp_expr(p, lhs, &lbuf);
        if(err) return err;
        err = cc_interp_expr(p, rhs, &rbuf);
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
                err = cc_sizeof_as_uint(p, pointee, expr->loc, &elem_sz);
                if(err) return err;
                char* ptr = NULL;
                memcpy(&ptr, &lbuf, sizeof ptr);
                int64_t idx = interp_read_int(&rbuf, rsz);
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
                err = cc_sizeof_as_uint(p, pointee, expr->loc, &elem_sz);
                if(err) return err;
                char* ptr = NULL;
                memcpy(&ptr, &rbuf, sizeof ptr);
                int64_t idx = interp_read_int(&lbuf, lsz);
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
                err = cc_sizeof_as_uint(p, pointee, expr->loc, &elem_sz);
                if(err) return err;
                char* ptr = NULL;
                memcpy(&ptr, &lbuf, sizeof ptr);
                int64_t idx = interp_read_int(&rbuf, rsz);
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
                err = cc_sizeof_as_uint(p, pointee, expr->loc, &elem_sz);
                if(err) return err;
                char* lp = NULL, *rp = NULL;
                memcpy(&lp, &lbuf, sizeof lp);
                memcpy(&rp, &rbuf, sizeof rp);
                int64_t diff = (lp - rp) / (int64_t)elem_sz;
                interp_write_uint(result, result_sz, (uint64_t)diff);
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
                    return cc_error(p, expr->loc, "unsupported pointer operation");
            }
            interp_write_uint(result, result_sz, cmp);
            return 0;
        }
        _Bool is_float = ccqt_is_basic(lhs->type) && ccbt_is_float(lhs->type.basic.kind);
        if(is_float){
            double ld = interp_read_float(&lbuf, lhs->type.basic.kind);
            double rd = interp_read_float(&rbuf, rhs->type.basic.kind);
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
                            return cc_error(p, expr->loc, "unsupported float operation");
                    }
                    interp_write_uint(result, result_sz, cmp);
                    return 0;
                }
            }
            if(ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind))
                interp_write_float(result, expr->type.basic.kind, res);
            else
                interp_write_uint(result, result_sz, (uint64_t)(int64_t)res);
            return 0;
        }
        _Bool is_unsigned = ccqt_is_basic(lhs->type) && ccbt_is_unsigned(lhs->type.basic.kind);
        uint64_t lu = interp_read_uint(&lbuf, lsz);
        uint64_t ru = interp_read_uint(&rbuf, rsz);
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
        interp_write_uint(result, result_sz, res);
        return 0;
    }
    case CC_EXPR_ADDASSIGN: case CC_EXPR_SUBASSIGN:
    case CC_EXPR_MULASSIGN: case CC_EXPR_DIVASSIGN: case CC_EXPR_MODASSIGN:
    case CC_EXPR_BITANDASSIGN: case CC_EXPR_BITORASSIGN: case CC_EXPR_BITXORASSIGN:
    case CC_EXPR_LSHIFTASSIGN: case CC_EXPR_RSHIFTASSIGN: {
        void* lval;
        int err = cc_interp_lvalue(p, expr->value0, &lval);
        if(err) return err;
        uint32_t sz;
        err = cc_sizeof_as_uint(p, expr->type, expr->loc, &sz);
        if(err) return err;
        uint64_t rbuf = 0;
        uint32_t rsz;
        err = cc_sizeof_as_uint(p, expr->values[0]->type, expr->loc, &rsz);
        if(err) return err;
        err = cc_interp_expr(p, expr->values[0], &rbuf);
        if(err) return err;

        _Bool is_float = ccqt_is_basic(expr->type) && ccbt_is_float(expr->type.basic.kind);
        if(is_float){
            double ld = interp_read_float(lval, expr->type.basic.kind);
            double rd = interp_read_float(&rbuf, expr->values[0]->type.basic.kind);
            double res;
            switch(expr->kind){
                case CC_EXPR_ADDASSIGN: res = ld + rd; break;
                case CC_EXPR_SUBASSIGN: res = ld - rd; break;
                case CC_EXPR_MULASSIGN: res = ld * rd; break;
                case CC_EXPR_DIVASSIGN: res = rd != 0.0 ? ld / rd : 0.0; break;
                default: return cc_error(p, expr->loc, "unsupported float compound assignment");
            }
            interp_write_float(lval, expr->type.basic.kind, res);
            interp_write_float(result, expr->type.basic.kind, res);
        }
        else {
            uint64_t lu = interp_read_uint(lval, sz);
            uint64_t ru = interp_read_uint(&rbuf, rsz);
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
            interp_write_uint(lval, sz, res);
            interp_write_uint(result, sz, res);
        }
        return 0;
    }
    case CC_EXPR_CALL: {
        CcExpr* callee = expr->value0;
        uint32_t nargs = expr->call.nargs;
        if(callee->kind != CC_EXPR_FUNCTION)
            return cc_error(p, expr->loc, "indirect function calls not yet supported in interpreter");
        CcFunc* func = callee->func;
        if(!func->native_func){
            const char* sym = func->mangle ? func->mangle->data : func->name->data;
            void* addr = dlsym(RTLD_DEFAULT, sym);
            if(!addr)
                return cc_error(p, expr->loc, "function '%s' not found: %s", sym, dlerror());
            func->native_func = (void(*)(void))addr;
        }
        CcFunction* ftype = func->type;
        void* args[64];
        uint64_t arg_storage[64];
        void* decay_ptrs[64];
        for(uint32_t i = 0; i < nargs; i++){
            CcExpr* arg = expr->values[i];
            CcQualType arg_type = arg->type;
            // Array-to-pointer decay
            if(ccqt_kind(arg_type) == CC_ARRAY){
                arg_storage[i] = 0;
                int err = cc_interp_expr(p, arg, &arg_storage[i]);
                if(err) return err;
                // arg_storage[i] now holds the pointer (from VALUE or VARIABLE decay)
                memcpy(&decay_ptrs[i], &arg_storage[i], sizeof(void*));
                args[i] = &decay_ptrs[i];
            }
            else {
                arg_storage[i] = 0;
                int err = cc_interp_expr(p, arg, &arg_storage[i]);
                if(err) return err;
                args[i] = &arg_storage[i];
            }
        }
        CcQualType vararg_types[64];
        const CcQualType* vt = NULL;
        if(ftype->is_variadic && nargs > ftype->param_count){
            for(uint32_t i = ftype->param_count; i < nargs; i++){
                CcQualType at = expr->values[i]->type;
                // Array decay type for varargs
                if(ccqt_kind(at) == CC_ARRAY){
                    CcArray* arr = ccqt_as_array(at);
                    CcPointer* ptr = Allocator_zalloc(cc_allocator(p), sizeof *ptr);
                    if(!ptr) return CC_OOM_ERROR;
                    ptr->kind = CC_POINTER;
                    ptr->pointee = arr->element;
                    vararg_types[i - ftype->param_count] = (CcQualType){.bits = (uintptr_t)ptr};
                }
                else {
                    vararg_types[i - ftype->param_count] = at;
                }
            }
            vt = vararg_types;
        }
        uint64_t rval = 0;
        int err = native_call(cc_allocator(p), func, args, (int)nargs, vt, &rval);
        if(err) return err;
        CcQualType ret_type = ftype->return_type;
        if(ccqt_is_basic(ret_type) && ret_type.basic.kind == CCBT_void)
            return 0;
        uint32_t ret_sz;
        err = cc_sizeof_as_uint(p, ret_type, expr->loc, &ret_sz);
        if(err) return err;
        memcpy(result, &rval, ret_sz);
        return 0;
    }
    default:
        return cc_unimplemented(p, expr->loc, "interpreter: unsupported expression kind");
    }
}

static
int
cc_interp_step(CcParser* p){
    CcInterpFrame* frame = p->current_frame;
    if(frame->pc >= frame->stmt_count)
        return 0;
    CcStatement* stmt = &frame->stmts[frame->pc];
    switch(stmt->kind){
        case CC_STMT_NULL:
        case CC_STMT_LABEL:
            frame->pc++;
            return 0;
        case CC_STMT_EXPR: {
            uint64_t discard;
            int err = cc_interp_expr(p, stmt->exprs[0], &discard);
            if(err) return err;
            frame->pc++;
            return 0;
        }
        case CC_STMT_GOTO:
            frame->pc = stmt->targets[0];
            return 0;
        default:
            return cc_unimplemented(p, stmt->loc, "interpreter: unsupported statement kind");
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
