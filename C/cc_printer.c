#ifndef CC_PRINTER_C
#define CC_PRINTER_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "cc_printer.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static const char* _Null_unspecified cc_basic_names[] = {
    [CCBT_INVALID]            = "<invalid>",
    [CCBT_void]               = "void",
    [CCBT_bool]               = "_Bool",
    [CCBT_char]               = "char",
    [CCBT_signed_char]        = "signed char",
    [CCBT_unsigned_char]      = "unsigned char",
    [CCBT_short]              = "short",
    [CCBT_unsigned_short]     = "unsigned short",
    [CCBT_int]                = "int",
    [CCBT_unsigned]           = "unsigned int",
    [CCBT_long]               = "long",
    [CCBT_unsigned_long]      = "unsigned long",
    [CCBT_long_long]          = "long long",
    [CCBT_unsigned_long_long] = "unsigned long long",
    [CCBT_int128]             = "__int128",
    [CCBT_unsigned_int128]    = "unsigned __int128",
    [CCBT_float16]            = "_Float16",
    [CCBT_float]              = "float",
    [CCBT_double]             = "double",
    [CCBT_long_double]        = "long double",
    [CCBT_float128]           = "_Float128",
    [CCBT_float_complex]      = "float _Complex",
    [CCBT_double_complex]     = "double _Complex",
    [CCBT_long_double_complex]= "long double _Complex",
    [CCBT_nullptr_t]          = "nullptr_t",
    [CCBT__Type]              = "_Type",
    [CCBT__Any]               = "_Any",
};

static void cc_print_type_pre(MStringBuilder*, CcQualType t);
static void cc_print_type_post(MStringBuilder*, CcQualType t);

static
_Bool
cc_type_needs_parens(CcQualType t){
    CcTypeKind k = ccqt_kind(t);
    return (k == CC_ARRAY && !ccqt_as_array(t)->is_vector) || k == CC_FUNCTION;
}

static
void
cc_print_type_pre(MStringBuilder* sb, CcQualType t){
    CcTypeKind kind = ccqt_kind(t);
    switch(kind){
        case CC_BASIC:
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcBasicTypeKind k = t.basic.kind;
            msb_sprintf(sb, "%s", k < CCBT_COUNT ? cc_basic_names[k] : "<bad-basic>");
            return;
        case CC_BLOCK_POINTER:
        case CC_POINTER: {
            CcPointer* p = ccqt_as_ptr(t);
            _Bool block = kind == CC_BLOCK_POINTER;
            cc_print_type_pre(sb, p->pointee);
            if(cc_type_needs_parens(p->pointee)){
                if(block) msb_write_literal(sb, " (^");
                else      msb_write_literal(sb, " (*");
            }
            else {
                if(block) msb_write_literal(sb, " ^");
                else      msb_write_literal(sb, " *");
            }
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            if(p->restrict_) msb_write_literal(sb, "restrict ");
            return;
        }
        case CC_SLICE: {
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcSlice* s = ccqt_as_slice(t);
            cc_print_type_pre(sb, s->pointee);
            return;
        }
        case CC_ARRAY: {
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcArray* a = ccqt_as_array(t);
            cc_print_type_pre(sb, a->element);
            return;
        }
        case CC_FUNCTION: {
            CcFunction* f = ccqt_as_function(t);
            cc_print_type_pre(sb, f->return_type);
            return;
        }
        case CC_STRUCT: {
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcStruct* s = ccqt_as_struct(t);
            if(s->name) msb_sprintf(sb, "struct %.*s", s->name->length, s->name->data);
            else msb_write_literal(sb, "struct <anon>");
            return;
        }
        case CC_UNION: {
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcUnion* u = ccqt_as_union(t);
            if(u->name) msb_sprintf(sb, "union %.*s", u->name->length, u->name->data);
            else msb_write_literal(sb, "union <anon>");
            return;
        }
        case CC_ENUM: {
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcEnum* e = ccqt_as_enum(t);
            if(e->name) msb_sprintf(sb, "enum %.*s", e->name->length, e->name->data);
            else msb_write_literal(sb, "enum <anon>");
            return;
        }
    }
    msb_sprintf(sb, "<type:%d>", (int)kind);
}

static
void
cc_print_type_post(MStringBuilder* sb, CcQualType t){
    switch(ccqt_kind(t)){
        case CC_BLOCK_POINTER:
        case CC_POINTER: {
            CcPointer* p = ccqt_as_ptr(t);
            if(cc_type_needs_parens(p->pointee))
                msb_write_char(sb, ')');
            cc_print_type_post(sb, p->pointee);
            return;
        }
        case CC_SLICE:{
            CcSlice* s = ccqt_as_slice(t);
            msb_write_literal(sb, "[:]");
            cc_print_type_post(sb, s->pointee);
            return;
        }
        case CC_ARRAY: {
            CcArray* a = ccqt_as_array(t);
            if(a->is_vector)
                msb_sprintf(sb, " __attribute__((vector_size(%u)))", a->vector_size);
            else if(a->is_incomplete)
                msb_write_literal(sb, "[]");
            else
                msb_sprintf(sb, "[%zu]", a->length);
            cc_print_type_post(sb, a->element);
            return;
        }
        case CC_FUNCTION: {
            CcFunction* f = ccqt_as_function(t);
            msb_write_char(sb, '(');
            for(uint32_t i = 0; i < f->param_count; i++){
                if(i) msb_write_literal(sb, ", ");
                cc_print_type_pre(sb, f->params[i]);
                cc_print_type_post(sb, f->params[i]);
            }
            if(f->is_variadic){
                if(f->param_count) msb_write_literal(sb, ", ");
                msb_write_literal(sb, "...");
            }
            if(!f->param_count && !f->is_variadic){
                if(f->no_prototype) {}
                else msb_write_literal(sb, "void");
            }
            msb_write_char(sb, ')');
            cc_print_type_post(sb, f->return_type);
            return;
        }
        case CC_STRUCT:
        case CC_UNION:
        case CC_ENUM:
        case CC_BASIC:
            return;
    }
}

static
void
cc_print_type(MStringBuilder* sb, CcQualType t){
    cc_print_type_pre(sb, t);
    cc_print_type_post(sb, t);
}

static
void
cc_print_runtime_value(CcParser* p, CcQualType type, const void* data, MStringBuilder* sb, int indent){
    const CcTargetConfig* tgt = cc_target(p);
    SrcLoc loc = {0};
    switch(ccqt_kind(type)){
        case CC_BASIC: {
            CcBasicTypeKind k = type.basic.kind;
            switch(k){
                case CCBT_void:
                    msb_write_literal(sb, "void");
                    return;
                case CCBT_bool:
                    if(*(const uint8_t*)data) msb_write_literal(sb, "true");
                    else msb_write_literal(sb, "false");
                    return;
                case CCBT_char: case CCBT_signed_char: case CCBT_unsigned_char: {
                    unsigned char c = *(const unsigned char*)data;
                    if(c >= 0x20 && c < 0x7f)
                        msb_sprintf(sb, "'%c' (%u)", c, c);
                    else
                        msb_sprintf(sb, "%u", c);
                    return;
                }
                case CCBT_float:
                    msb_sprintf(sb, "%g", (double)*(const float*)data);
                    return;
                case CCBT_double: case CCBT_long_double:
                    msb_sprintf(sb, "%g", *(const double*)data);
                    return;
                case CCBT__Any:{
                    msb_write_literal(sb, "(Any)");
                    CiRtAny any = *(const CiRtAny*)data;
                    if(any.type.bits && cc_any_payload_type(p, any.type))
                        cc_print_runtime_value(p, any.type, any.payload, sb, indent);
                    else msb_write_literal(sb, "<empty or invalid>");
                    return;
                };
                case CCBT__Type:
                    msb_write_char(sb, '(');
                    cc_print_type(sb, (CcQualType){.bits = *(const uintptr_t*)data});
                    msb_write_char(sb, ')');
                    return;
                case CCBT_nullptr_t:
                    msb_write_literal(sb, "nullptr");
                    return;
                case CCBT_short: case CCBT_int: case CCBT_long: case CCBT_long_long: {
                    int64_t v = 0;
                    memcpy(&v, data, tgt->sizeof_[k]);
                    unsigned sz = tgt->sizeof_[k];
                    if(sz < 8 && (v & ((int64_t)1 << (sz*8-1))))
                        v |= ~(((int64_t)1 << (sz*8)) - 1);
                    msb_sprintf(sb, "%lld", (long long)v);
                    return;
                }
                case CCBT_unsigned_short: case CCBT_unsigned: case CCBT_unsigned_long:
                case CCBT_unsigned_long_long: {
                    uint64_t v = 0;
                    memcpy(&v, data, tgt->sizeof_[k]);
                    msb_sprintf(sb, "%llu", (unsigned long long)v);
                    return;
                }
                case CCBT_COUNT:
                case CCBT_INVALID:
                case CCBT_double_complex:
                case CCBT_float128:
                case CCBT_float16:
                case CCBT_float_complex:
                case CCBT_int128:
                case CCBT_long_double_complex:
                case CCBT_unsigned_int128:
                    msb_write_literal(sb, "<unknown basic>");
                    return;
                DRP_CASES_EXHAUSTED;
            }
        }
        case CC_SLICE:{
            CiRtSlice slice;
            memcpy(&slice, data, sizeof slice);
            msb_sprintf(sb, "{%zu, %p}", (size_t)slice.count, slice.data);
            return;
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER: {
            void* ptr;
            memcpy(&ptr, data, tgt->sizeof_[CCBT_nullptr_t]);
            msb_sprintf(sb, "%p", ptr);
            return;
        }
        case CC_ENUM: {
            CcEnum* e = ccqt_as_enum(type);
            int64_t v = 0;
            uint32_t sz = tgt->sizeof_[e->underlying.basic.kind];
            memcpy(&v, data, sz);
            if(sz < 8 && (v & ((int64_t)1 << (sz*8-1))))
                v |= ~(((int64_t)1 << (sz*8)) - 1);
            for(size_t i = 0; i < e->enumerator_count; i++){
                if(e->enumerators[i]->value == v){
                    msb_sprintf(sb, "%.*s (%lld)", (int)e->enumerators[i]->name->length, e->enumerators[i]->name->data, (long long)v);
                    return;
                }
            }
            msb_sprintf(sb, "%lld", (long long)v);
            return;
        }
        case CC_ARRAY: {
            CcArray* arr = ccqt_as_array(type);
            if(arr->is_incomplete || arr->is_vla){ msb_write_literal(sb, "[...]"); return; }
            uint32_t elem_sz;
            if(cc_sizeof_as_uint(p, arr->element, loc, &elem_sz)){ msb_write_literal(sb, "[...]"); return; }
            msb_write_char(sb, '{');
            for(size_t i = 0; i < arr->length; i++){
                if(i) msb_write_literal(sb, ", ");
                if(i >= 16){ msb_write_literal(sb, "..."); break; }
                cc_print_runtime_value(p, arr->element, (const char*)data + i * elem_sz, sb, indent);
            }
            msb_write_char(sb, '}');
            return;
        }
        case CC_STRUCT: {
            CcStruct* s = ccqt_as_struct(type);
            if(s->is_incomplete){ msb_write_literal(sb, "{<incomplete>}"); return; }
            msb_write_literal(sb, "{\n");
            for(uint32_t i = 0; i < s->field_count; i++){
                CcField* f = &s->fields[i];
                if(f->is_method) continue;
                for(int j = 0; j < indent + 1; j++) msb_write_literal(sb, "  ");
                if(f->name)
                    msb_sprintf(sb, ".%.*s = ", (int)f->name->length, f->name->data);
                else
                    msb_write_literal(sb, "<anon> = ");
                if(f->is_bitfield){
                    uint32_t storage_sz = cc_type_sizeof_assume_complete(cc_target(p), f->type);
                    uint64_t storage = 0;
                    memcpy(&storage, (const char*)data + f->offset, storage_sz);
                    uint64_t mask = f->bitwidth >= 64 ? ~(uint64_t)0 : ((uint64_t)1 << f->bitwidth) - 1;
                    uint64_t val = (storage >> f->bitoffset) & mask;
                    msb_sprintf(sb, "%llu", (unsigned long long)val);
                }
                else {
                    cc_print_runtime_value(p, f->type, (const char*)data + f->offset, sb, indent + 1);
                }
                msb_write_literal(sb, ",\n");
            }
            for(int j = 0; j < indent; j++) msb_write_literal(sb, "  ");
            msb_write_char(sb, '}');
            return;
        }
        case CC_UNION: {
            CcUnion* u = ccqt_as_union(type);
            if(u->is_incomplete){ msb_write_literal(sb, "{<incomplete>}"); return; }
            msb_write_literal(sb, "{ /* union, ");
            msb_sprintf(sb, "%u bytes: ", u->size);
            for(uint32_t i = 0; i < u->size && i < 32; i++){
                msb_sprintf(sb, "%02x", ((const unsigned char*)data)[i]);
                if(i + 1 < u->size && i + 1 < 32) msb_write_char(sb, ' ');
            }
            if(u->size > 32) msb_write_literal(sb, " ...");
            msb_write_literal(sb, " */ }");
            return;
        }
        case CC_FUNCTION:
            msb_write_literal(sb, "<function>");
            return;
    }
}

static
void
cc_print_expr(MStringBuilder*sb, CcExpr* e){
    switch(e->kind){
        case CC_EXPR_VALUE:
            if(e->str.length && e->text){
                uint32_t sz = 1;
                if(ccqt_kind(e->type) == CC_ARRAY){
                    CcQualType pointee = ccqt_as_array(e->type)->element;
                    if(ccqt_is_basic(pointee)){
                        // FIXME: adhoc
                        switch(pointee.basic.kind){
                            case CCBT_char:
                            case CCBT_signed_char:
                            case CCBT_unsigned_char:
                                sz = 1;
                                break;
                            case CCBT_short:
                            case CCBT_unsigned_short:
                                sz = 2;
                                break;
                            case CCBT_int:
                            case CCBT_unsigned:
                                sz = 4;
                                break;
                            default:
                                break;
                        }
                    }
                }
                size_t before = sb->cursor;
                switch(sz){
                    case 1:
                        msb_write_literal(sb, "\"");
                        before = sb->cursor;
                        msb_write_str(sb, (const char*)e->text, e->str.length-1);
                        msb_write_literal(sb, "\"");
                        goto fixup;
                    case 2:
                        msb_write_literal(sb, "u\"");
                        before = sb->cursor;
                        msb_write_utf16(sb, (const uint16_t*)e->text, e->str.length-1);
                        msb_write_literal(sb, "\"");
                        goto fixup;
                    case 4:
                        msb_write_literal(sb, "U\"");
                        before = sb->cursor;
                        msb_write_utf32(sb, ((const uint32_t*)e->text), e->str.length);
                        msb_write_literal(sb, "\"");
                        break;
                        goto fixup;
                    default:
                        msb_sprintf(sb, "(??""?)\"...\"");
                        break;
                }
                if(0){
                    fixup:
                    for(size_t i = before; i < sb->cursor-1; i++){
                        unsigned char c = (unsigned char)sb->data[i];
                        char buff[8];
                        char* repl; size_t len;
                        if(c == '\\'){
                            msb_replace_range(sb, i, i+1, "\\\\", 2);
                            i += 1;
                        }
                        else if(c == '"'){
                            msb_replace_range(sb, i, i+1, "\\\"", 2);
                            i += 1;
                        }
                        if(c < 32){
                            switch(c){
                                case '\0':
                                    repl = "\\0";
                                    len = 2;
                                    break;
                                case '\r':
                                    repl = "\\r";
                                    len = 2;
                                    break;
                                case '\n':
                                    repl = "\\n";
                                    len = 2;
                                    break;
                                case '\t':
                                    repl = "\\t";
                                    len = 2;
                                    break;
                                case '\f':
                                    repl = "\\f";
                                    len = 2;
                                    break;
                                default:
                                    len = stbsp_snprintf(buff, sizeof buff, "\\x%x", c);
                                    repl = buff;
                                    break;
                            }
                            msb_replace_range(sb, i, i+1, repl, len);
                            i += len-1;
                        }
                    }
                }
            }
            else if(ccqt_is_basic(e->type) && e->type.basic.kind == CCBT__Type){
                cc_print_type(sb, (CcQualType){.bits = e->uinteger});
            }
            else if(ccqt_is_basic(e->type) && ccbt_is_float(e->type.basic.kind)){
                if(e->type.basic.kind == CCBT_float)
                    msb_sprintf(sb, "%gf", (double)e->float_);
                else
                    msb_sprintf(sb, "%g", e->double_);
            }
            else {
                msb_sprintf(sb, "%llu", (unsigned long long)e->uinteger);
            }
            return;
        case CC_EXPR_VARIABLE:
            if(e->var->name->length)
                msb_sprintf(sb, "%.*s", e->var->name->length, e->var->name->data);
            else if(e->var->automatic)
                msb_sprintf(sb, "<anon>@[%zd]", e->var->frame_offset);
            else
                msb_sprintf(sb, "<anon>");
            return;
        case CC_EXPR_FUNCTION:
            if(e->func->name)
                msb_sprintf(sb, "%.*s", e->func->name->length, e->func->name->data);
            else
                msb_write_literal(sb, "<lambda>");
            return;
        case CC_EXPR_BUILTIN:
            switch(e->builtin.op){
                case CC_BUILTIN_UNREACHABLE: msb_write_literal(sb, "unreachable"); break;
                case CC_BUILTIN_TRAP: msb_write_literal(sb, "trap"); break;
                case CC_BUILTIN_DEBUGTRAP: msb_write_literal(sb, "debugtrap()"); break;
                case CC_BUILTIN_ABORT: msb_write_literal(sb, "abort()"); break;
                case CC_BUILTIN_BACKTRACE: msb_write_literal(sb, "backtrace()"); break;
            }
            return;
        case CC_EXPR_VA:
            switch(e->va.op){
                case CC_VA_ARG:
                    msb_write_literal(sb, "va_arg(");
                    cc_print_expr(sb, e->lhs);
                    msb_write_literal(sb, ", ");
                    cc_print_type(sb, e->type);
                    msb_write_literal(sb, ")");
                    return;
                case CC_VA_END:
                    msb_write_literal(sb, "va_end(");
                    cc_print_expr(sb, e->lhs);
                    msb_write_literal(sb, ")");
                    return;
                case CC_VA_COPY:
                    msb_write_literal(sb, "va_copy(");
                    cc_print_expr(sb, e->lhs);
                    msb_write_literal(sb, ", ");
                    cc_print_expr(sb, e->values[0]);
                    msb_write_literal(sb, ")");
                    return;
                case CC_VA_START:
                    msb_write_literal(sb, "va_start(");
                    cc_print_expr(sb, e->lhs);
                    msb_write_literal(sb, ")");
                    return;
                DRP_CASES_EXHAUSTED;
            }
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_STATEMENT_EXPRESSION:
        case CC_EXPR_ATOMIC:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
        case CC_EXPR_ALLOCA:
        case CC_EXPR_INTERN:
        case CC_EXPR_HOTSWAP:
        case CC_EXPR_SRCLOC_REFLECT:
        case CC_EXPR_COMPILE:
        case CC_EXPR_MODULE_REFLECT:
        case CC_EXPR_UMUL128:
            msb_write_literal(sb, "<unimpl>");
            return;
        case CC_EXPR_POPCOUNT:
            msb_write_literal(sb, "popcount(");
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, ")");
            return;
        case CC_EXPR_CTZ:
            msb_write_literal(sb, "ctz(");
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, ")");
            return;
        case CC_EXPR_CLZ:
            msb_write_literal(sb, "clz(");
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, ")");
            return;
        case CC_EXPR_BSWAP:
            msb_write_literal(sb, "bswap(");
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, ")");
            return;
        case CC_EXPR_COMPOUND_LITERAL:
            msb_write_char(sb, '(');
            cc_print_type(sb, e->type);
            msb_write_char(sb, ')');
            goto print_init_list;
        case CC_EXPR_INIT_LIST:
        print_init_list: {
            CcInitList* il = e->init_list;
            msb_write_char(sb, '{');
            for(uint32_t i = 0; i < il->count; i++){
                if(i) msb_write_literal(sb, ", ");
                CcInitEntry* ent = &il->entries[i];
                _Bool show_offset = ent->field_loc.byte_offset || ent->field_loc.bit_width || il->count > 1;
                if(show_offset){
                    msb_sprintf(sb, "@%llu", (unsigned long long)ent->field_loc.byte_offset);
                    if(ent->field_loc.bit_width)
                        msb_sprintf(sb, ":%llu:%llu", (unsigned long long)ent->field_loc.bit_offset, (unsigned long long)ent->field_loc.bit_width);
                    msb_write_literal(sb, " = ");
                }
                if(ent->value)
                    cc_print_expr(sb, ent->value);
            }
            msb_write_char(sb, '}');
            return;
        }
        #define UNOP(K, S) case K: msb_write_literal(sb, S); cc_print_expr(sb, e->lhs); return;
        UNOP(CC_EXPR_NEG,    "-")
        UNOP(CC_EXPR_POS,    "+")
        UNOP(CC_EXPR_BITNOT, "~")
        UNOP(CC_EXPR_LOGNOT, "!")
        UNOP(CC_EXPR_DEREF,  "*")
        UNOP(CC_EXPR_ADDR,   "&")
        UNOP(CC_EXPR_PREINC, "++")
        UNOP(CC_EXPR_PREDEC, "--")
        #undef UNOP
        case CC_EXPR_POSTINC: cc_print_expr(sb, e->lhs); msb_write_literal(sb, "++"); return;
        case CC_EXPR_POSTDEC: cc_print_expr(sb, e->lhs); msb_write_literal(sb, "--"); return;
        #define BINOP(K, S) case K: msb_write_char(sb, '('); cc_print_expr(sb, e->lhs); msb_write_literal(sb, " " S " "); cc_print_expr(sb, e->values[0]); msb_write_char(sb, ')'); return;
        BINOP(CC_EXPR_ADD,    "+")
        BINOP(CC_EXPR_SUB,    "-")
        BINOP(CC_EXPR_MUL,    "*")
        BINOP(CC_EXPR_DIV,    "/")
        BINOP(CC_EXPR_MOD,    "%")
        BINOP(CC_EXPR_BITAND, "&")
        BINOP(CC_EXPR_BITOR,  "|")
        BINOP(CC_EXPR_BITXOR, "^")
        BINOP(CC_EXPR_LSHIFT, "<<")
        BINOP(CC_EXPR_RSHIFT, ">>")
        BINOP(CC_EXPR_LOGAND, "&&")
        BINOP(CC_EXPR_LOGOR,  "||")
        BINOP(CC_EXPR_EQ,     "==")
        BINOP(CC_EXPR_NE,     "!=")
        BINOP(CC_EXPR_LT,     "<")
        BINOP(CC_EXPR_GT,     ">")
        BINOP(CC_EXPR_LE,     "<=")
        BINOP(CC_EXPR_GE,     ">=")
        BINOP(CC_EXPR_ASSIGN,       "=")
        BINOP(CC_EXPR_ADDASSIGN,    "+=")
        BINOP(CC_EXPR_SUBASSIGN,    "-=")
        BINOP(CC_EXPR_MULASSIGN,    "*=")
        BINOP(CC_EXPR_DIVASSIGN,    "/=")
        BINOP(CC_EXPR_MODASSIGN,    "%=")
        BINOP(CC_EXPR_BITANDASSIGN, "&=")
        BINOP(CC_EXPR_BITORASSIGN,  "|=")
        BINOP(CC_EXPR_BITXORASSIGN, "^=")
        BINOP(CC_EXPR_LSHIFTASSIGN, "<<=")
        BINOP(CC_EXPR_RSHIFTASSIGN, ">>=")
        BINOP(CC_EXPR_COMMA,        ",")
        #undef BINOP
        case CC_EXPR_SUBSCRIPT:
            cc_print_expr(sb, e->lhs);
            msb_write_char(sb, '[');
            cc_print_expr(sb, e->values[0]);
            msb_write_char(sb, ']');
            return;
        case CC_EXPR_SLICE_ALL:
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, "[:]");
            return;
        case CC_EXPR_SLICE_LO:
            cc_print_expr(sb, e->lhs);
            msb_write_char(sb, '[');
            cc_print_expr(sb, e->values[0]);
            msb_write_literal(sb, ":]");
            return;
        case CC_EXPR_SLICE_HI:
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, "[:");
            cc_print_expr(sb, e->values[0]);
            msb_write_char(sb, ']');
            return;
        case CC_EXPR_SLICE:
            cc_print_expr(sb, e->lhs);
            msb_write_char(sb, '[');
            cc_print_expr(sb, e->values[0]);
            msb_write_char(sb, ':');
            cc_print_expr(sb, e->values[1]);
            msb_write_char(sb, ']');
            return;
        case CC_EXPR_TERNARY:
            msb_write_char(sb, '(');
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, " ? ");
            cc_print_expr(sb, e->values[0]);
            msb_write_literal(sb, " : ");
            cc_print_expr(sb, e->values[1]);
            msb_write_char(sb, ')');
            return;
        case CC_EXPR_CAST:
            msb_write_char(sb, '(');
            cc_print_type(sb, e->type);
            msb_write_char(sb, ')');
            cc_print_expr(sb, e->lhs);
            return;
        case CC_EXPR_DOT:
            cc_print_expr(sb, e->values[0]);
            msb_sprintf(sb, ".@%llu", (unsigned long long)e->field_loc.byte_offset);
            return;
        case CC_EXPR_ARROW:
            cc_print_expr(sb, e->values[0]);
            msb_sprintf(sb, "->@%llu", (unsigned long long)e->field_loc.byte_offset);
            return;
        case CC_EXPR_CALL:
            cc_print_expr(sb, e->lhs);
            msb_write_char(sb, '(');
            for(uint32_t i = 0; i < e->call.nargs; i++){
                if(i) msb_write_literal(sb, ", ");
                cc_print_expr(sb, e->values[i]);
            }
            msb_write_char(sb, ')');
            return;
        case CC_EXPR_TYPE_INTROSPECTION:
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, ".<type_introspection>()");
            return;
    }
    msb_write_literal(sb, "<unknown>");
}

static
void
_cc_print_statement(MStringBuilder* sb, CcStmtNode* s, int indent){
    msb_write_nchar(sb, ' ', indent*2);
    switch(s->kind){
    case CC_STMT_NULL:       // ;
        msb_write_char(sb, ';');
        return;
    case CC_STMT_EXPR:       // expr;
        cc_print_expr(sb, s->exprs[0]);
        return;
    case CC_STMT_COMPOUND:   // { ... } (tree form only, flattened away by lowering)
        msb_write_literal(sb, "{\n");
        for(size_t i = 0; i < s->count; i++){
            _cc_print_statement(sb, s->stmts[i], indent+1);
            msb_write_char(sb, '\n');
        }
        msb_write_nchar(sb, ' ', indent*2);
        msb_write_literal(sb, "}");
        return;
    case CC_STMT_IF:         // if (cond) then [else]
        msb_write_literal(sb, "if(");
        cc_print_expr(sb, s->exprs[0]);
        msb_write_literal(sb, ")\n");
        _cc_print_statement(sb, s->stmts[0], indent+1);
        if(s->stmts[1]){
            msb_write_char(sb, '\n');
            msb_write_nchar(sb, ' ', indent*2);
            msb_write_literal(sb, "else\n");
            _cc_print_statement(sb, s->stmts[1], indent+1);
        }
        return;
    case CC_STMT_WHILE:      // while (cond) body
        msb_write_literal(sb, "while(");
        cc_print_expr(sb, s->exprs[0]);
        msb_write_literal(sb, ")\n");
        _cc_print_statement(sb, s->stmts[0], indent+1);
        return;
    case CC_STMT_DOWHILE:    // do body while (cond);
        msb_write_literal(sb, "do\n");
        _cc_print_statement(sb, s->stmts[0], indent+1);
        msb_write_char(sb, '\n');
        msb_write_nchar(sb, ' ', indent*2);
        msb_write_literal(sb, "while(");
        cc_print_expr(sb, s->exprs[0]);
        msb_write_literal(sb, ");");
        return;
    case CC_STMT_FOR:        // for (init; cond; inc) body
        msb_write_literal(sb, "for(");
        if(s->stmts[0]) _cc_print_statement(sb, s->stmts[0], 0);
        msb_write_literal(sb, "; ");
        if(s->exprs[0]) cc_print_expr(sb, s->exprs[0]);
        msb_write_literal(sb, "; ");
        if(s->exprs[1]) cc_print_expr(sb, s->exprs[1]);
        msb_write_literal(sb, ")\n");
        _cc_print_statement(sb, s->stmts[1], indent+1);
        return;
    case CC_STMT_SWITCH:     // switch (expr) { ... }
        msb_write_literal(sb, "switch(");
        cc_print_expr(sb, s->exprs[0]);
        msb_write_literal(sb, ")\n");
        _cc_print_statement(sb, s->stmts[0], indent+1);
        return;
    case CC_STMT_CASE:       // case expr:
        msb_sprintf(sb, "case %llu:\n", (unsigned long long)s->case_value);
        _cc_print_statement(sb, s->stmts[0], indent+1);
        return;
    case CC_STMT_DEFAULT:    // default:
        msb_write_literal(sb, "default:\n");
        _cc_print_statement(sb, s->stmts[0], indent+1);
        return;
    case CC_STMT_RETURN:     // return [expr];
        msb_write_literal(sb, "return");
        if(s->exprs[0]){
            msb_write_char(sb, ' ');
            cc_print_expr(sb, s->exprs[0]);
        }
        msb_write_char(sb, ';');
        return;
    case CC_STMT_BREAK:      // break;
        msb_write_literal(sb, "break;");
        return;
    case CC_STMT_CONTINUE:   // continue;
        msb_write_literal(sb, "continue;");
        return;
    case CC_STMT_GOTO:       // goto label;
        msb_sprintf(sb, "goto %.*s;", (int)s->label->length, s->label->data);
        return;
    case CC_STMT_LABEL:      // label:
        msb_sprintf(sb, "%.*s:\n", (int)s->label->length, s->label->data);
        _cc_print_statement(sb, s->stmts[0], indent+1);
        return;
    }

}

static
void
cc_print_statement(MStringBuilder*sb, CcStmtNode* s){
    _cc_print_statement(sb, s, 1);
    msb_write_char(sb, '\n');
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
