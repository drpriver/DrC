#ifndef TYPEINFO_PRINT_H
#define TYPEINFO_PRINT_H
#ifndef TIK_NO_STDIO
#include <stdio.h>
#endif
#include "typeinfo.h"
#include "bitfields.h"
#include "atom_map.h"

typedef struct TiPrinter TiPrinter;
struct TiPrinter {
    int (*printer)(void*, const char*, ...);
    void* ctx;
    int indent;
};


#ifndef TIK_NO_STDIO
static
void
ti_print_fp(const void* src, const TypeInfo* ti, FILE* fp);
#endif

static
void
ti_print_struct(const void* src, const TypeInfoStruct* ti, TiPrinter* printer);
static
void
ti_print_marray(const void* src, const TypeInfoMarray* ti, TiPrinter* printer);
static
void
ti_print_fixed_array(const void* src, const TypeInfoFixedArray* ti, TiPrinter* printer);
static
void
ti_print_enum(const void* src, const TypeInfoEnum* ti, TiPrinter* printer);
static
void
ti_print_atom_map(const void* src, const TypeInfoAtomMap* ti, TiPrinter* printer);
static
void
ti_print_class(const void* src, const TypeInfoClass* ti, TiPrinter* printer);

static
void
ti_print_any(const void* src, const TypeInfo* ti, TiPrinter* printer){
    switch(ti->kind){
        case TIK_UNSET:
            return;
        case TIK_UINT8:
            printer->printer(printer->ctx, "%u", (unsigned)*(const uint8_t*)src);
            return;
        case TIK_INT8:
            printer->printer(printer->ctx, "%d", (int)*(const int8_t*)src);
            return;
        case TIK_UINT16:
            printer->printer(printer->ctx, "%u", (unsigned)*(const uint16_t*)src);
            return;
        case TIK_INT16:
            printer->printer(printer->ctx, "%d", (int)*(const int16_t*)src);
            return;
        case TIK_UINT32:
            printer->printer(printer->ctx, "%u", (unsigned)*(const uint32_t*)src);
            return;
        case TIK_INT32:
            printer->printer(printer->ctx, "%d", (int)*(const int32_t*)src);
            return;
        case TIK_UINT64:
            printer->printer(printer->ctx, "%llu", (unsigned long long)*(const uint64_t*)src);
            return;
        case TIK_INT64:
            printer->printer(printer->ctx, "%lld", (long long)*(const int64_t*)src);
            return;
        case TIK_FLOAT32:
            printer->printer(printer->ctx, "%f", (double)*(const float*)src);
            return;
        case TIK_FLOAT64:
            printer->printer(printer->ctx, "%f", *(const double*)src);
            return;
        case TIK_BOOL:
            printer->printer(printer->ctx, "%s", *(const _Bool*)src?"true":"false");
            return;
        case TIK_ATOM_ENUM:
        case TIK_ATOM:
            if(*(const Atom*)src)
                printer->printer(printer->ctx, "\"%s\"", (*(const Atom*)src)->data);
            else
                printer->printer(printer->ctx, "null");
            return;
        case TIK_SV:
            printer->printer(printer->ctx, "\"%.*s\"", (int)((const StringView*)src)->length, ((const StringView*)src)->text);
            return;
        case TIK_TUPLE:
        case TIK_STRUCT:
            ti_print_struct(src, (const TypeInfoStruct*)ti, printer);
            return;
        case TIK_ENUM:
            ti_print_enum(src, (const TypeInfoEnum*)ti, printer);
            return;
        case TIK_FARRAY:
            ti_print_fixed_array(src, (const TypeInfoFixedArray*)ti, printer);
            return;
        case TIK_MARRAY:
            ti_print_marray(src, (const TypeInfoMarray*)ti, printer);
            return;
        case TIK_ATOM_MAP:
            ti_print_atom_map(src, (const TypeInfoAtomMap*)ti, printer);
            return;
        case TIK_ATOM_SET:{
            AtomSetItems items = AS_items(src);
            _Bool newline = 0;
            printer->printer(printer->ctx, "[");
            for(size_t i = 0; i < items.count; i++){
                Atom a = items.data[i];
                if(!a) continue;
                if(newline) printer->printer(printer->ctx, ",\n");
                printer->printer(printer->ctx, "%*s", 2*printer->indent+2, "");
                printer->printer(printer->ctx, "\"%s\"", a->data);
                newline = 1;
            }
            if(newline) printer->printer(printer->ctx, "\n");
            printer->printer(printer->ctx, "%*s]", 2*printer->indent, "");
        }break;
        case TIK_DRJSON_VALUE:
            printer->printer(printer->ctx, "(DrJsonValue)");
            return;
        case TIK_POINTER:{
            const void* p = *(const void*const*)src;
            if(!p) printer->printer(printer->ctx, "null");
            else{
                printer->printer(printer->ctx, "*");
                ti_print_any(p, ((const TypeInfoPointer*)ti)->type, printer);
            }
        } return;
        case TIK_CLASS:
            ti_print_class(src, (const TypeInfoClass*)ti, printer);
            return;
    }
}

static
int
ti_get_size_t_from_member(const void* src, const MemberInfo* mi, size_t* length){
    switch((MemberKind)mi->kind){
        case MK_NORMAL:{
            const void* field = (const char*)src + mi->offset;
            switch(mi->type->kind){
            case TIK_UNSET: return 1;
            case TIK_INT8:
                *length = (size_t)*(const int8_t*)field;
                return 0;
            case TIK_INT16:
                *length = (size_t)*(const int16_t*)field;
                return 0;
            case TIK_INT32:
                *length = (size_t)*(const int32_t*)field;
                return 0;
            case TIK_INT64:
                *length = (size_t)*(const int64_t*)field;
                return 0;
            case TIK_UINT8:
                *length = (size_t)*(const uint8_t*)field;
                return 0;
            case TIK_UINT16:
                *length = (size_t)*(const uint16_t*)field;
                return 0;
            case TIK_UINT32:
                *length = (size_t)*(const uint32_t*)field;
                return 0;
            case TIK_UINT64:
                *length = (size_t)*(const uint64_t*)field;
                return 0;
            case TIK_FLOAT32:
            case TIK_FLOAT64:
                return 1;
            case TIK_BOOL:
                *length = (size_t)*(const _Bool*)field;
                return 0;
            case TIK_ATOM:
            case TIK_SV:
            case TIK_ATOM_SET:
            case TIK_STRUCT:
            case TIK_TUPLE:
            case TIK_ENUM:
            case TIK_ATOM_ENUM:
            case TIK_MARRAY:
            case TIK_FARRAY:
            case TIK_ATOM_MAP:
            case TIK_DRJSON_VALUE:
            case TIK_POINTER:
            case TIK_CLASS:
                return 1;
            }
        }break;
        case MK_ARRAY:
            return 1;
        case MK_BITFIELD:{
            uint64_t val = 0;
            const void* field = (const char*)src + mi->offset;
            val = read_bitfield(field, mi->type->size, mi->bitfield.bitsize, mi->bitfield.bitoffset, mi->type->kind <= TIK_INT64);
            *length = (size_t)val;
            return 0;
        }break;
        case MK_FLEXIBLE_ARRAY:
            return 1;
    }
}

static int ti_get_end_of_member(const void* src, const TypeInfoStruct* ti, const MemberInfo* mi, const void** end);
static
int
ti_get_start_of_member(const void* src, const TypeInfoStruct* ti, const MemberInfo* mi, const void** end){
    const char* base;
    if(mi->flexible.after_mi != TI_AFTER_MI_NONE){
        if(mi->flexible.after_mi >= ti->length) return 1;
        int err = ti_get_end_of_member(src, ti, &ti->members[mi->flexible.after_mi], (const void**)&base);
        if(err) return err;
    }
    else
        base = (const char*)src + mi->offset;
    *end = base;
    return 0;
}
static
int
ti_get_end_of_member(const void* src, const TypeInfoStruct* ti, const MemberInfo* mi, const void** end){
    switch(mi->kind){
        case MK_NORMAL:
            *end = (const char*)src + mi->offset + mi->type->size;
            return 0;
        case MK_ARRAY:
            *end = (const char*)src + mi->offset + mi->type->size * mi->array.length;
            return 0;
        case MK_FLEXIBLE_ARRAY:{
            const char* base;
            int err = ti_get_start_of_member(src, ti, mi, (const void**)&base);
            if(err) return err;
            if(mi->flexible.length_mi >= ti->length) return 1;
            size_t length;
            err = ti_get_size_t_from_member(src, &ti->members[mi->flexible.length_mi], &length);
            *end = base + length*mi->type->size;
            return 0;
        };
        case MK_BITFIELD:
            *end = (const char*)src + mi->offset + mi->type->size;
            return 0;
    }
    return 1;
}

static
void
ti_print_struct(const void* src, const TypeInfoStruct* ti, TiPrinter* printer){
    printer->printer(printer->ctx, "(%s){\n", ti->name->data);
    for(size_t i = 0; i < ti->length; i++){
        const MemberInfo* mi = &ti->members[i];
        if(mi->noprint) continue;
        const TypeInfo* mt = mi->type;
        printer->printer(printer->ctx, "%*s.%s = ", 2*printer->indent+2, "", mi->name->data);
        switch((MemberKind)mi->kind){
            case MK_NORMAL:{
                const void* field = (const char*)src + mi->offset;
                printer->indent++;
                ti_print_any(field, mt, printer);
                printer->indent--;
            }break;
            case MK_ARRAY:{
                printer->printer(printer->ctx, "[");
                printer->indent++;
                for(size_t j = 0; j < mi->array.length; j++){
                    const void* field = (const char*)src + mi->offset + j * mt->size;
                    ti_print_any(field, mt, printer);
                    printer->printer(printer->ctx, ", ");
                }
                printer->indent--;
                printer->printer(printer->ctx, "]");
            }break;
            case MK_BITFIELD:{
                uint64_t val = 0;
                const void* field = (const char*)src + mi->offset;
                val = read_bitfield(field, mt->size, mi->bitfield.bitsize, mi->bitfield.bitoffset, mt->kind <= TIK_INT64);
                ti_print_any(&val, mt, printer);
            }break;
            case MK_FLEXIBLE_ARRAY:{
                const char* base;
                int err = ti_get_start_of_member(src, ti, mi, (const void**)&base);
                size_t length;
                if(mi->flexible.length_mi < ti->length){
                    err = ti_get_size_t_from_member(src, &ti->members[mi->flexible.length_mi], &length);
                    if(!err){
                        printer->printer(printer->ctx, "[");
                        printer->indent++;
                        for(size_t j = 0; j < length; j++){
                            const void* field = base + j * mt->size;
                            ti_print_any(field, mt, printer);
                            printer->printer(printer->ctx, ", ");
                        }
                        printer->indent--;
                        printer->printer(printer->ctx, "]");
                    }
                }
            }break;
        }
        printer->printer(printer->ctx, ",\n");
    }
    printer->printer(printer->ctx, "%*s}", 2*printer->indent, "");
}

static
void
ti_print_marray(const void* src, const TypeInfoMarray* ti, TiPrinter* printer){
    const TypeInfo* type = ti->type;
    const size_t* pcount = src;
    const void* const* pdata = (const void*const*)((const char*)src + 2*sizeof(size_t));
    size_t count = *pcount;
    if(count){
        printer->printer(printer->ctx, "%s[\n", ti->name->data);
        const void* data = *pdata;
        for(size_t i = 0; i < count; i++){
            printer->printer(printer->ctx, "%*s", 2*printer->indent+2, "");
            const void* field = (const char*)data + i*type->size;
            printer->indent++;
            ti_print_any(field, type, printer);
            printer->indent--;
            printer->printer(printer->ctx, ",\n");
        }
        printer->printer(printer->ctx, "%*s]", 2*printer->indent, "");
    }
    else {
        printer->printer(printer->ctx, "%s[]", ti->name->data);
    }
}

static
void
ti_print_fixed_array(const void* src, const TypeInfoFixedArray* ti, TiPrinter* printer){
    const TypeInfo* type = ti->type;
    size_t count = *(const size_t*)src;
    const void* data = (const char*)src + ti->data_offset;
    if(count){
        printer->printer(printer->ctx, "%s[\n", ti->name->data);
        for(size_t i = 0; i < count; i++){
            if(type->kind <= TIK_BOOL){
                if((i & 15 )== 0)
                    printer->printer(printer->ctx, "%*s", 2*printer->indent+2, "");
            }
            else
                printer->printer(printer->ctx, "%*s", 2*printer->indent+2, "");
            const void* field = (const char*)data + i*type->size;
            printer->indent++;
            ti_print_any(field, type, printer);
            printer->indent--;
            if(type->kind <= TIK_BOOL){
                if((i &15) == 15)
                    printer->printer(printer->ctx, ",\n");
                else
                    printer->printer(printer->ctx, ",");
            }
            else
                printer->printer(printer->ctx, ",\n");
        }
        printer->printer(printer->ctx, "%*s]", 2*printer->indent, "");
    }
    else {
        printer->printer(printer->ctx, "%s[]", ti->name->data);
    }
}

static
void
ti_print_atom_map(const void* src, const TypeInfoAtomMap* ti, TiPrinter* printer){
    const TypeInfo* type = ti->type;
    AtomMapItems items = AM_items(src);
    if(items.count){
        printer->printer(printer->ctx, "%s{\n", ti->name->data);
        for(size_t i = 0; i < items.count; i++){
            AtomMapItem* it = &items.data[i];
            printer->printer(printer->ctx, "%*s\"%s\": ", 2*printer->indent+2, "", it->atom->data);
            printer->indent++;
            ti_print_any(it->p, type, printer);
            printer->indent--;
            printer->printer(printer->ctx, ",\n");
        }
        printer->printer(printer->ctx, "%*s}", 2*printer->indent, "");
    }
    else {
        printer->printer(printer->ctx, "%s{}", ti->name->data);
    }

}

static
void
ti_print_enum(const void* src, const TypeInfoEnum* ti, TiPrinter* printer){
    if(ti->named){
        size_t idx = 0;
        switch(ti->size){
            case 1:
                idx = (size_t)*(const uint8_t*)src;
                break;
            case 2:
                idx = (size_t)*(const uint16_t*)src;
                break;
            case 4:
                idx = (size_t)*(const uint32_t*)src;
                break;
            case 8:
                idx = (size_t)*(const uint64_t*)src;
                break;
            default: return;
        }
        Atom name = ti->names[idx];
        printer->printer(printer->ctx, "%s", name->data);
        return;
    }
    uint64_t val = 0;
    switch(ti->size){
        case 1:
            val = (uint64_t)*(const uint8_t*)src;
            break;
        case 2:
            val = (uint64_t)*(const uint16_t*)src;
            break;
        case 4:
            val = (uint64_t)*(const uint32_t*)src;
            break;
        case 8:
            val = (uint64_t)*(const uint64_t*)src;
            break;
        default: return;
    }
    printer->printer(printer->ctx, "%llu", val);
}

static
void
ti_print_class(const void* src, const TypeInfoClass* ti, TiPrinter* printer){
    const TypeInfo* t = NULL;
    if(ti->atom_tag){
        Atom const * tag = src;
        if(!*tag) return;
        if(ti->vtable_func)
            t = ti->func(*tag);
        else
            t = AM_get(ti->_vtable, *tag);
        if(!t) return;
        ti_print_any(src, t, printer);
    }
    if(!t) __builtin_debugtrap(); // TODO: unimplemented
}

#ifndef TIK_NO_STDIO
static
void
ti_print_fp(const void* src, const TypeInfo* ti, FILE* fp){
    TiPrinter printer = {
        .printer = (int(*)(void*, const char*, ...))fprintf,
        .ctx = fp,
    };
    ti_print_any(src, ti, &printer);
    printer.printer(printer.ctx, "\n");
}
#endif

#endif
