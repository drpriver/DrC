#ifndef DRP_TYPEINFO_JSON_DESER_H
#define DRP_TYPEINFO_JSON_DESER_H
#include "typeinfo.h"
#include "drjson.h"
#include "long_string.h"
#include "bitfields.h"
#include "atom_map.h"
#include "atom_set.h"
#include "Allocators/allocator.h"
#include "Allocators/nullacator.h"
#include <stdint.h>

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
int
struct_from_json(void* dst, const TypeInfoStruct* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al);
static
int
tuple_from_json(void* dst, const TypeInfoStruct* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al);
static
int
enum_from_json(void* dst, const TypeInfoEnum* ti, DrJsonContext* jsctx, DrJsonValue v);

static
int
atommap_from_json(void* dst, const TypeInfoAtomMap* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al);
static
int
atomset_from_json(AtomSet* dst, DrJsonContext* jsctx, DrJsonValue v, Allocator al);
static
int
marray_from_json(void* dst, const TypeInfoMarray* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al);
static
int
fixed_array_from_json(void* dst, const TypeInfoFixedArray* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al);
static
int
any_from_json(void* dst, const TypeInfo* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al){
    switch(ti->kind){
        case TIK_CLASS: return 1; // TODO FIXME
        case TIK_POINTER: return 1;
        case TIK_UNSET: return 1;
        case TIK_UINT8:{
            if(v.kind != DRJSON_UINTEGER) return 1;
            if(v.uinteger > UINT8_MAX) return 1;
            *(uint8_t*)dst = (uint8_t)v.uinteger;
            break;
        }break;
        case TIK_INT8:{
            if(v.kind == DRJSON_UINTEGER){
                if(v.uinteger > INT8_MAX) return 1;
                *(int8_t*)dst = (int8_t)v.uinteger;
                break;
            }
            if(v.kind == DRJSON_INTEGER){
                if(v.integer > INT8_MAX) return 1;
                if(v.integer < INT8_MIN) return 1;
                *(int8_t*)dst = (int8_t)v.integer;
                break;
            }
            return 1;
        }break;
        case TIK_UINT16:{
            if(v.kind != DRJSON_UINTEGER) return 1;
            if(v.uinteger > UINT16_MAX) return 1;
            *(uint16_t*)dst = (uint16_t)v.uinteger;
            break;
        }break;
        case TIK_INT16:{
            if(v.kind == DRJSON_UINTEGER){
                if(v.uinteger > INT16_MAX) return 1;
                *(int16_t*)dst = (int16_t)v.uinteger;
                break;
            }
            if(v.kind == DRJSON_INTEGER){
                if(v.integer > INT16_MAX) return 1;
                if(v.integer < INT16_MIN) return 1;
                *(int16_t*)dst = (int16_t)v.integer;
                break;
            }
            return 1;
        }break;
        case TIK_UINT32:{
            if(v.kind != DRJSON_UINTEGER) return 1;
            if(v.uinteger > UINT32_MAX) return 1;
            *(uint32_t*)dst = (uint32_t)v.uinteger;
            break;
        }break;
        case TIK_INT32:{
            if(v.kind == DRJSON_UINTEGER){
                if(v.uinteger > INT32_MAX) return 1;
                *(int32_t*)dst = (int32_t)v.uinteger;
                break;
            }
            if(v.kind == DRJSON_INTEGER){
                if(v.integer > INT32_MAX) return 1;
                if(v.integer < INT32_MIN) return 1;
                *(int32_t*)dst = (int32_t)v.integer;
                break;
            }
            return 1;
        }break;
        case TIK_UINT64:{
            if(v.kind != DRJSON_UINTEGER) return 1;
            *(uint64_t*)dst = v.uinteger;
            break;
        }break;
        case TIK_INT64:{
            if(v.kind == DRJSON_UINTEGER){
                if(v.uinteger > (uint64_t)INT64_MAX) return 1;
                *(int64_t*)dst = (int64_t)v.uinteger;
                break;
            }
            if(v.kind == DRJSON_INTEGER){
                *(int64_t*)dst = v.integer;
                break;
            }
            return 1;
        }break;
        case TIK_FLOAT32:
            if(v.kind == DRJSON_NUMBER){
                *(float*)dst = (float)v.number;
                break;
            }
            if(v.kind == DRJSON_INTEGER){
                *(float*)dst = (float)v.integer;
                break;
            }
            if(v.kind == DRJSON_UINTEGER){
                *(float*)dst = (float)v.uinteger;
                break;
            }
            return -1;
        case TIK_FLOAT64:{
            if(v.kind == DRJSON_NUMBER){
                *(double*)dst = v.number;
                break;
            }
            if(v.kind == DRJSON_INTEGER){
                *(double*)dst = (double)v.integer;
                break;
            }
            if(v.kind == DRJSON_UINTEGER){
                *(double*)dst = (double)v.uinteger;
                break;
            }
            return 1;
        }break;
        case TIK_ENUM:{
            int err = enum_from_json(dst, (const TypeInfoEnum*)ti, jsctx, v);
            if(err) return err;
            break;
        }
        case TIK_STRUCT:{
            int err = struct_from_json(dst, (const TypeInfoStruct*)ti, jsctx, v, al);
            if(err) return err;
            break;
        }
        case TIK_TUPLE:{
            int err = tuple_from_json(dst, (const TypeInfoStruct*)ti, jsctx, v, al);
            if(err) return err;
            break;
        }
        case TIK_ATOM_ENUM:
        case TIK_ATOM:{
            Atom* a = dst;
            if(v.kind == DRJSON_STRING){
                *a = v.atom;
                break;
            }
            return 1;
        }
        case TIK_BOOL:{
            _Bool* f = dst;
            if(v.kind == DRJSON_BOOL){
                *f = v.boolean;
                break;
            }
            if(v.kind == DRJSON_INTEGER){
                if(v.integer == 1){
                    *f = 1;
                    break;
                }
                if(v.integer == 0){
                    *f = 0;
                    break;
                }
                return 1;
            }
            if(v.kind == DRJSON_UINTEGER){
                if(v.uinteger == 1){
                    *f = 1;
                    break;
                }
                if(v.uinteger == 0){
                    *f = 0;
                    break;
                }
                return 1;
            }
            return 1;
        }break;
        case TIK_MARRAY:{
            int err = marray_from_json(dst, (const TypeInfoMarray*)ti, jsctx, v, al);
            if(err) return err;
        }break;
        case TIK_FARRAY:{
            int err = fixed_array_from_json(dst, (const TypeInfoFixedArray*)ti, jsctx, v, al);
            if(err) return err;
        }break;
        case TIK_ATOM_MAP:{
            int err = atommap_from_json(dst, (const TypeInfoAtomMap*)ti, jsctx, v, al);
            if(err) return err;
        }break;
        case TIK_ATOM_SET:{
            int err = atomset_from_json(dst, jsctx, v, al);
            if(err) return err;
        }break;
        case TIK_SV:{
            if(v.kind != DRJSON_STRING)
                return 1;
            StringView* sv = dst;
            *sv = (StringView){v.atom->length, v.atom->data};
        }break;
        case TIK_DRJSON_VALUE:
            *(DrJsonValue*)dst = v;
            return 0;
    }
    return 0;
}

static
int
struct_from_json(void* dst, const TypeInfoStruct* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al){
    if(v.kind != DRJSON_OBJECT) return 1;
    for(size_t i = 0; i < ti->length; i++){
        const MemberInfo* mi = &ti->members[i];
        if(mi->nodeser) continue;
        const TypeInfo* mt = mi->type;
        DrJsonValue val = drjson_object_get_item(jsctx, v, mi->name);
        if(val.kind == DRJSON_ERROR) continue;
        void* field = (char*)dst + mi->offset;
        switch((MemberKind)mi->kind){
            case MK_NORMAL:{
                int err = any_from_json(field, mt, jsctx, val, al);
                if(err) return err;
            }break;
            case MK_BITFIELD:{
                uint64_t bits = 0;
                int err = any_from_json(&bits, mt, jsctx, val, al);
                if(err) return err;
                write_bitfield(field, bits, mt->size, mi->bitfield.bitsize, mi->bitfield.bitoffset);
            }break;
            case MK_ARRAY:{
                if(val.kind != DRJSON_ARRAY) return 1;
                int64_t _len = drjson_len(jsctx, val);
                size_t len = (size_t)_len;
                if(len > mi->array.length)
                    len = mi->array.length;
                for(size_t j = 0; j < len; j++){
                    DrJsonValue aval = drjson_get_by_index(jsctx, val, j);
                    void* f = (char*)field + j * mt->size;
                    int err = any_from_json(f, mt, jsctx, aval, al);
                    if(err) return err;
                }
            }break;
            case MK_FLEXIBLE_ARRAY:{
                return 1; // unimplemented
            }break;
        }
    }
    return 0;
}

static
int
tuple_from_json(void* dst, const TypeInfoStruct* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al){
    if(v.kind != DRJSON_ARRAY) return 1;
    if(drjson_len(jsctx, v) != ti->length) return 1;
    for(size_t i = 0; i < ti->length; i++){
        const MemberInfo* mi = &ti->members[i];
        if(mi->nodeser) continue;
        const TypeInfo* mt = mi->type;
        DrJsonValue val = drjson_get_by_index(jsctx, v, i);
        if(val.kind == DRJSON_ERROR) continue;
        void* field = (char*)dst + mi->offset;
        switch((MemberKind)mi->kind){
            case MK_NORMAL:{
                int err = any_from_json(field, mt, jsctx, val, al);
                if(err) return err;
            }break;
            case MK_BITFIELD:{
                uint64_t bits = 0;
                int err = any_from_json(&bits, mt, jsctx, val, al);
                if(err) return err;
                write_bitfield(field, bits, mt->size, mi->bitfield.bitsize, mi->bitfield.bitoffset);
            }break;
            case MK_ARRAY:{
                if(val.kind != DRJSON_ARRAY) return 1;
                int64_t _len = drjson_len(jsctx, val);
                size_t len = (size_t)_len;
                if(len > mi->array.length)
                    len = mi->array.length;
                for(size_t j = 0; j < len; j++){
                    DrJsonValue aval = drjson_get_by_index(jsctx, val, j);
                    void* f = (char*)field + j * mt->size;
                    int err = any_from_json(f, mt, jsctx, aval, al);
                    if(err) return err;
                }
            }break;
            case MK_FLEXIBLE_ARRAY:{
                return 1; // unimplemented
            }break;
        }
    }
    return 0;
}

static
int
enum_from_json(void* dst, const TypeInfoEnum* ti, DrJsonContext* jsctx, DrJsonValue v){
    if(ti->named){
        if(v.kind != DRJSON_STRING) return 1;
        for(size_t i = 0; i < ti->length; i++){
            if(v.atom == ti->names[i]){
                switch(ti->size){
                    case 1:
                        *(uint8_t*)dst = (uint8_t)i;
                        break;
                    case 2:
                        *(uint16_t*)dst = (uint16_t)i;
                        break;
                    case 4:
                        *(uint32_t*)dst = (uint32_t)i;
                        break;
                    case 8:
                        *(uint64_t*)dst = (uint64_t)i;
                        break;
                    default:
                        return 1;
                }
                return 0;
            }
        }
        return 1;
    }
    switch(ti->size){
        case 1:
            return any_from_json(dst, basic_type_infos[TIK_UINT8], jsctx, v, NULLACATOR);
        case 2:
            return any_from_json(dst, basic_type_infos[TIK_UINT16], jsctx, v, NULLACATOR);
        case 4:
            return any_from_json(dst, basic_type_infos[TIK_UINT32], jsctx, v, NULLACATOR);
        case 8:
            return any_from_json(dst, basic_type_infos[TIK_UINT64], jsctx, v, NULLACATOR);
        default:
            return 1;
    }
}
static
int
atommap_from_json(void* dst, const TypeInfoAtomMap* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al){
    if(v.kind != DRJSON_OBJECT)
        return 1;
    DrJsonValue items = drjson_object_items(v);
    AM_clear(dst);
    int64_t len = drjson_len(jsctx, items);
    for(int64_t i = 0; i < len; i+= 2){
        DrJsonValue k = drjson_get_by_index(jsctx, items, i);
        DrJsonValue val = drjson_get_by_index(jsctx, items, i+1);
        Atom a = k.atom;
        void* pval = Allocator_zalloc(al, ti->type->size);
        if(!pval) return 1;
        int err;
        err = any_from_json(pval, ti->type, jsctx, val, al);
        if(err) return err;
        err = AM_put(dst, al, a, pval);
        if(err) return err;
    }
    return 0;
}
static
int
atomset_from_json(AtomSet* dst, DrJsonContext* jsctx, DrJsonValue v, Allocator al){
    if(v.kind != DRJSON_ARRAY)
        return 1;
    AS_clear(dst);
    int64_t len = drjson_len(jsctx, v);
    for(int64_t i = 0; i < len; i++){
        DrJsonValue k = drjson_get_by_index(jsctx, v, i);
        if(k.kind != DRJSON_STRING)
            return 1;
        int err = AS_add(dst, al, k.atom);
        if(err) return DRJSON_ERROR_ALLOC_FAILURE;
    }
    return 0;
}
static
int
marray_from_json(void* dst, const TypeInfoMarray* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al){
    if(v.kind != DRJSON_ARRAY)
        return 1;
    size_t* pcount = dst;
    *pcount = 0;
    int64_t len = drjson_len(jsctx, v);
    size_t* pcap = pcount+1;
    void** pdata = (void**)(pcap+1);
    void* data = *pdata;
    if(*pcap < (size_t)len){
        size_t oldsz = *pcap * ti->type->size;
        size_t newsz = (size_t)len * ti->type->size;
        data = Allocator_realloc(al, data, oldsz, newsz);
        if(!data) return 1;
        *pdata = data;
        *pcap = (size_t)len;
    }
    for(int64_t i = 0; i < len; i++){
        void* p = (char*)data + ti->type->size*i;
        DrJsonValue it = drjson_get_by_index(jsctx, v, i);
        int err = any_from_json(p, ti->type, jsctx, it, al);
        if(err) return err;
    }
    *pcount = (size_t)len;
    return 0;
}
static
int
fixed_array_from_json(void* dst, const TypeInfoFixedArray* ti, DrJsonContext* jsctx, DrJsonValue v, Allocator al){
    if(v.kind != DRJSON_ARRAY)
        return 1;
    size_t* count = (size_t*)dst;
    *count = 0;
    int64_t len = drjson_len(jsctx, v);
    if((size_t)len > ti->length) return 1;
    void* data = (char*)dst + ti->data_offset;
    for(int64_t i = 0; i < len; i++){
        void* p = (char*)data + ti->type->size*i;
        DrJsonValue it = drjson_get_by_index(jsctx, v, i);
        int err = any_from_json(p, ti->type, jsctx, it, al);
        if(err) return err;
    }
    *count = (size_t)len;
    return 0;
}

static
int
struct_from_config_txt(void* dst, const TypeInfoStruct* ti, LongString txt, AtomTable* at, Allocator tmp, Allocator al){
    DrJsonContext* jsctx = NULL;
    int result = 1;
    jsctx = drjson_create_ctx(tmp, at);
    if(!jsctx) goto finally;
    DrJsonValue v = drjson_parse_string(jsctx, txt.text, txt.length, DRJSON_PARSE_FLAG_BRACELESS_OBJECT);
    if(v.kind != DRJSON_OBJECT) goto finally;
    result = struct_from_json(dst, ti, jsctx, v, al);
    finally:
    if(jsctx) drjson_ctx_free_all(jsctx);
    return result;
}

static
int
any_from_json_txt(void* dst, const TypeInfo* ti, LongString txt, AtomTable* at, Allocator tmp, Allocator al){
    DrJsonContext* jsctx = NULL;
    int result = 1;
    jsctx = drjson_create_ctx(tmp, at);
    if(!jsctx) goto finally;
    DrJsonValue v = drjson_parse_string(jsctx, txt.text, txt.length, DRJSON_PARSE_FLAG_NONE);
    if(v.kind == DRJSON_ERROR) goto finally;
    result = any_from_json(dst, ti, jsctx, v, al);
    finally:
    if(jsctx) drjson_ctx_free_all(jsctx);
    return result;
}

static
DrJsonValue
any_to_json(const void* src, const TypeInfo* ti, DrJsonContext* jsctx);

static
DrJsonValue
enum_to_json(const void* src, const TypeInfoEnum* ti, DrJsonContext* jsctx);

static
DrJsonValue
marray_to_json(const void* src, const TypeInfoMarray* ti, DrJsonContext* jsctx);

static
DrJsonValue
fixed_array_to_json(const void* src, const TypeInfoFixedArray* ti, DrJsonContext* jsctx);

static
DrJsonValue
atommap_to_json(const void* src, const TypeInfoAtomMap* ti, DrJsonContext* jsctx);

static
DrJsonValue
atomset_to_json(const AtomSet* src, DrJsonContext* jsctx);

static
DrJsonValue
tuple_to_json(const void* src, const TypeInfoStruct* ti, DrJsonContext* jsctx);

static
DrJsonValue
struct_to_json(const void* src, const TypeInfoStruct* ti, DrJsonContext* jsctx){
    DrJsonValue result = drjson_make_object(jsctx);
    if(result.kind == DRJSON_ERROR) return result;
    for(size_t i = 0; i < ti->length; i++){
        const MemberInfo* mi = &ti->members[i];
        if(mi->noser) continue;
        const TypeInfo* mt = mi->type;
        DrJsonValue val = {0};
        const void* field = (const char*)src + mi->offset;
        switch((MemberKind)mi->kind){
            case MK_NORMAL:{
                val = any_to_json(field, mt, jsctx);
            }break;
            case MK_ARRAY:{
                val = drjson_make_array(jsctx);
                if(val.kind == DRJSON_ERROR) goto fail;
                for(size_t j = 0; j < mi->array.length; j++){
                    const void* it = (const char*)field + j * mt->size;
                    DrJsonValue v = any_to_json(it, mt, jsctx);
                    if(v.kind == DRJSON_ERROR) goto fail;
                    int err = drjson_array_push_item(jsctx, val, v);
                    if(err) goto fail;
                }
            }break;
            case MK_BITFIELD:{
                uint64_t bits = 0;
                bits = read_bitfield(field, mt->size, mi->bitfield.bitsize, mi->bitfield.bitoffset, mt->kind <= TIK_INT64);
                val = any_to_json(&bits, mt, jsctx);
            }break;
            case MK_FLEXIBLE_ARRAY:{
                goto fail;
            }break;
        }
        if(val.kind == DRJSON_ERROR)
            goto fail;
        int err = drjson_object_set_item(jsctx, result, mi->name, val);
        if(err) goto fail;
    }
    return result;
    fail:
    result = drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "idk");
    return result;
}

static
DrJsonValue
tuple_to_json(const void* src, const TypeInfoStruct* ti, DrJsonContext* jsctx){
    DrJsonValue result = drjson_make_array(jsctx);
    if(result.kind == DRJSON_ERROR) return result;
    for(size_t i = 0; i < ti->length; i++){
        const MemberInfo* mi = &ti->members[i];
        if(mi->noser) continue;
        const TypeInfo* mt = mi->type;
        DrJsonValue val = {0};
        const void* field = (const char*)src + mi->offset;
        switch((MemberKind)mi->kind){
            case MK_NORMAL:{
                val = any_to_json(field, mt, jsctx);
            }break;
            case MK_ARRAY:{
                val = drjson_make_array(jsctx);
                if(val.kind == DRJSON_ERROR) goto fail;
                for(size_t j = 0; j < mi->array.length; j++){
                    const void* it = (const char*)field + j * mt->size;
                    DrJsonValue v = any_to_json(it, mt, jsctx);
                    if(v.kind == DRJSON_ERROR) goto fail;
                    int err = drjson_array_push_item(jsctx, val, v);
                    if(err) goto fail;
                }
            }break;
            case MK_BITFIELD:{
                uint64_t bits = 0;
                bits = read_bitfield(field, mt->size, mi->bitfield.bitsize, mi->bitfield.bitoffset, mt->kind <= TIK_INT64);
                val = any_to_json(&bits, mt, jsctx);
            }break;
            case MK_FLEXIBLE_ARRAY:{
                goto fail;
            }break;
        }
        if(val.kind == DRJSON_ERROR)
            goto fail;
        int err = drjson_array_push_item(jsctx, result, val);
        if(err) goto fail;
    }
    return result;
    fail:
    result = drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "idk");
    return result;
}

static
DrJsonValue
any_to_json(const void* src, const TypeInfo* ti, DrJsonContext* jsctx){
    DrJsonValue result = {0};
    switch(ti->kind){
        case TIK_CLASS: // TODO: FIXME
        case TIK_POINTER:
        case TIK_UNSET:
            goto fail;
        case TIK_UINT8:
            result = drjson_make_uint(*(const uint8_t*)src);
            break;
        case TIK_INT8:
            result = drjson_make_int(*(const int8_t*)src);
            break;
        case TIK_UINT16:
            result = drjson_make_uint(*(const uint16_t*)src);
            break;
        case TIK_INT16:
            result = drjson_make_int(*(const int16_t*)src);
            break;
        case TIK_UINT32:
            result = drjson_make_uint(*(const uint32_t*)src);
            break;
        case TIK_INT32:
            result = drjson_make_int(*(const int32_t*)src);
            break;
        case TIK_UINT64:
            result = drjson_make_uint(*(const uint64_t*)src);
            break;
        case TIK_INT64:
            result = drjson_make_int(*(const int64_t*)src);
            break;
        case TIK_FLOAT32:
            result = drjson_make_number((double)*(const float*)src);
            break;
        case TIK_FLOAT64:
            result = drjson_make_number(*(const double*)src);
            break;
        case TIK_ENUM:
            result = enum_to_json(src, (const TypeInfoEnum*)ti, jsctx);
            break;
        case TIK_STRUCT:
            result = struct_to_json(src, (const TypeInfoStruct*)ti, jsctx);
            break;
        case TIK_TUPLE:
            result = tuple_to_json(src, (const TypeInfoStruct*)ti, jsctx);
            break;
        case TIK_ATOM_ENUM:
        case TIK_ATOM:
            result = drjson_atom_to_value(*(const Atom*)src);
            break;
        case TIK_BOOL:
            result = drjson_make_bool(*(const _Bool*)src);
            break;
        case TIK_MARRAY:
            result = marray_to_json(src, (const TypeInfoMarray*)ti, jsctx);
            break;
        case TIK_FARRAY:
            result = fixed_array_to_json(src, (const TypeInfoFixedArray*)ti, jsctx);
            break;
        case TIK_ATOM_MAP:
            result = atommap_to_json(src, (const TypeInfoAtomMap*)ti, jsctx);
            break;
        case TIK_ATOM_SET:
            result = atomset_to_json(src, jsctx);
            break;
        case TIK_SV:
            result = drjson_make_string(jsctx, ((const StringView*)src)->text, ((const StringView*)src)->length);
            break;
        case TIK_DRJSON_VALUE:
            result = *(const DrJsonValue*)src;
            break;
    }
    return result;
    fail:
    result = drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "idk");
    return result;
}

static
DrJsonValue
enum_to_json(const void* src, const TypeInfoEnum* ti, DrJsonContext* jsctx){
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
            default:
                return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "enum invalid size");
        }
        if(idx >= ti->length) return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "enum out of bounds");
        Atom name = ti->names[idx];
        return drjson_atom_to_value(name);
    }
    switch(ti->size){
        case 1:
            return any_to_json(src, basic_type_infos[TIK_UINT8], jsctx);
        case 2:
            return any_to_json(src, basic_type_infos[TIK_UINT16], jsctx);
        case 4:
            return any_to_json(src, basic_type_infos[TIK_UINT32], jsctx);
        case 8:
            return any_to_json(src, basic_type_infos[TIK_UINT64], jsctx);
        default:
            return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "invalid enum size");
    }
}
static
DrJsonValue
fixed_array_to_json(const void* src, const TypeInfoFixedArray* ti, DrJsonContext* jsctx){
    DrJsonValue result = drjson_make_array(jsctx);
    if(result.kind == DRJSON_ERROR) return result;
    size_t count = *(const size_t*)src;
    const void* data = (const char*)src + ti->data_offset;
    const TypeInfo* et = ti->type;
    for(size_t i = 0; i < count; i++){
        const void* field = (const char*)data + et->size*i;
        DrJsonValue v = any_to_json(field, et, jsctx);
        if(v.kind == DRJSON_ERROR) return v;
        int err = drjson_array_push_item(jsctx, result, v);
        if(err) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "buy more ram");
    }
    return result;
}
static
DrJsonValue
marray_to_json(const void* src, const TypeInfoMarray* ti, DrJsonContext* jsctx){
    DrJsonValue result = drjson_make_array(jsctx);
    if(result.kind == DRJSON_ERROR) return result;
    const size_t* pcount = src;
    size_t count = *pcount;
    const void* const* pdata = (const void*const*)((const char*)src + sizeof(size_t)*2);
    const void* data = *pdata;
    const TypeInfo* et = ti->type;
    for(size_t i = 0; i < count; i++){
        const void* field = (const char*)data + et->size*i;
        DrJsonValue v = any_to_json(field, et, jsctx);
        if(v.kind == DRJSON_ERROR) return v;
        int err = drjson_array_push_item(jsctx, result, v);
        if(err) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "buy more ram");
    }
    return result;
}

static
DrJsonValue
atommap_to_json(const void* src, const TypeInfoAtomMap* ti, DrJsonContext* jsctx){
    DrJsonValue result = drjson_make_object(jsctx);
    if(result.kind == DRJSON_ERROR) return result;
    AtomMapItems items = AM_items(src);
    for(size_t i = 0; i < items.count; i++){
        Atom key = items.data[i].atom;
        void* p = items.data[i].p;
        DrJsonValue v = any_to_json(p, ti->type, jsctx);
        if(v.kind == DRJSON_ERROR) return v;
        int err = drjson_object_set_item(jsctx, result, key, v);
        if(err) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "buy more ram");
    }
    return result;
}

static
DrJsonValue
atomset_to_json(const AtomSet* src, DrJsonContext* jsctx){
    DrJsonValue result = drjson_make_array(jsctx);
    if(result.kind == DRJSON_ERROR) return result;
    AtomSetItems items = AS_items(src);
    for(size_t i = 0; i < items.count; i++){
        Atom key = items.data[i];
        if(!key) continue;
        DrJsonValue v = drjson_atom_to_value(key);
        if(v.kind == DRJSON_ERROR) return v;
        int err = drjson_array_push_item(jsctx, result, v);
        if(err) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "buy more ram");
    }
    return result;
}



#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
