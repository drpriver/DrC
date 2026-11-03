//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// drc runtime services

#ifndef C_CI_RT_C
#define C_CI_RT_C

#include "ci_rt.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

struct CiTlsSlot {
    void*_Nullable address;
    size_t allocation_size;
    size_t alignment;
};

static int ci_create_closure(CiInterpreter*, CcFunc*);
static int ci_closure_map_put(CiInterpreter*, CcFunc*, void*);
static int ci_lower_func(CiInterpreter*, CcFunc*, CiLowerDeps*);
static int ci_lookup_symbol(CiInterpreter*, SrcLoc, CiModule*, const char*, size_t, CcQualType, void*_Nullable*_Nonnull, CcQualType*_Nullable);
static int ci_compile_module(CiInterpreter*, CcModule*, StringView, StringView, CiModule*_Nullable*_Nonnull);
static int ci_parse_module_type(CiInterpreter*, SrcLoc, CiModule*, StringView, CcQualType*);
static int ci_reflect_func_unlocked(CiInterpreter*, SrcLoc, CcFunc*, CiRtModuleMember*, _Bool);

// The error lock stays held until ci_finish_error flushes the diagnostic.
LOG_PRINTF(3, 4)
static
MStringBuilder*
ci_start_error(CiInterpreter* ci, SrcLoc loc, const char* fmt, ...){
    LOCK_T_lock(&ci->error_lock);
    va_list va;
    va_start(va, fmt);
    cpp_include_backtrace(&ci->parser.cpp, LOG_PRINT_ERROR);
    cpp_msg_preamble(&ci->parser.cpp, loc, "error");
    MStringBuilder* sb = &ci->parser.cpp.logger->buff;
    msb_vsprintf(sb, fmt, va);
    va_end(va);
    return sb;
}

static
int
ci_finish_error(CiInterpreter* ci, SrcLoc loc){
    log_flush(ci->parser.cpp.logger, LOG_PRINT_ERROR);
    cpp_msg_postamble(&ci->parser.cpp, loc, LOG_PRINT_ERROR);
    LOCK_T_unlock(&ci->error_lock);
    return CI_RUNTIME_ERROR;
}

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

static
void
ci_tls_free_thread(CiTlsThread* state){
    Allocator al = state->allocator;
    for(size_t i = 0; i < state->capacity; i++){
        CiTlsSlot* slot = &state->slots[i];
        if(slot->address) Allocator_free(al, slot->address, slot->allocation_size, 1, slot->alignment);
    }
    if(state->slots) Allocator_free(al, state->slots, state->capacity, sizeof *state->slots, _Alignof(CiTlsSlot));
    ci_stack_destroy(&state->stack);
    Allocator_free(al, state, 1, sizeof *state, _Alignof(CiTlsThread));
}

static
THREAD_LOCAL_DESTRUCTOR(ci_tls_thread_exit){
    CiTlsThread* state = value;
    if(!state) return;
    CiInterpreter* ci = state->owner;
    LOCK_T_lock(&ci->tls_lock);
    if(state->prev) state->prev->next = state->next;
    else ci->tls_threads = state->next;
    if(state->next) state->next->prev = state->prev;
    LOCK_T_unlock(&ci->tls_lock);
    ci_tls_free_thread(state);
}

static
int
ci_tls_prepare(CiInterpreter* ci){
    if(ci->tls_key_created) return 0;
    LOCK_T_init(&ci->tls_lock);
    if(thread_local_key_create(&ci->tls_key, ci_tls_thread_exit))
        return ci_error(ci, (SrcLoc){0}, "could not create interpreter TLS key");
    ci->tls_key_created = 1;
    return 0;
}

static
void
ci_tls_cleanup(CiInterpreter* ci){
    ci_closure_cleanup(ci);
    ci_free_toplevel_frame(ci);
    if(!ci->tls_key_created) return;
    thread_local_key_delete(ci->tls_key);
    while(ci->tls_threads){
        CiTlsThread* state = ci->tls_threads;
        ci->tls_threads = state->next;
        ci_tls_free_thread(state);
    }
    ci->tls_key_created = 0;
}

static
int
ci_tls_get(CiInterpreter* ci, CiTlsThread*_Nullable*_Nonnull out){
    if(!ci->tls_key_created)
        return ci_ice(ci, (SrcLoc){0}, "interpreter TLS state not prepared%s", "");
    CiTlsThread* state = thread_local_key_get(ci->tls_key);
    if(!state){
        Allocator al = ci_allocator(ci);
        state = Allocator_zalloc(al, 1, sizeof *state, _Alignof(CiTlsThread));
        if(!state) return CI_OOM_ERROR;
        state->owner = ci;
        state->allocator = al;
        state->stack.allocator = al;
        if(thread_local_key_set(ci->tls_key, state)){
            Allocator_free(al, state, 1, sizeof *state, _Alignof(CiTlsThread));
            return ci_error(ci, (SrcLoc){0}, "could not set interpreter TLS state");
        }
        LOCK_T_lock(&ci->tls_lock);
        state->next = ci->tls_threads;
        if(state->next) state->next->prev = state;
        ci->tls_threads = state;
        LOCK_T_unlock(&ci->tls_lock);
    }
    *out = state;
    return 0;
}

static
int
ci_tls_address(CiInterpreter* ci, CcVariable* var, void*_Nonnull*_Nonnull out){
    if(!var->tls_slot || !var->interp_val)
        return ci_ice(ci, var->loc, "thread_local variable '%s' not prepared", var->name->data);
    CiTlsThread* state;
    int err = ci_tls_get(ci, &state);
    if(err) return err;
    size_t index = var->tls_slot - 1;
    if(index >= state->capacity){
        size_t capacity = 8;
        if(index >= capacity){
            unsigned bits = 64 - clz_64(index);
            if(bits >= sizeof(size_t) * 8) return CI_OOM_ERROR;
            capacity = (size_t)1 << bits;
        }
        CiTlsSlot* slots = Allocator_zalloc(state->allocator, capacity, sizeof *slots, _Alignof(CiTlsSlot));
        if(!slots) return CI_OOM_ERROR;
        if(state->slots){
            memcpy(slots, state->slots, state->capacity * sizeof *slots);
            Allocator_free(state->allocator, state->slots, state->capacity, sizeof *state->slots, _Alignof(CiTlsSlot));
        }
        state->slots = slots;
        state->capacity = capacity;
    }
    CiTlsSlot* slot = &state->slots[index];
    if(!slot->address){
        size_t alignment = var->tls_alignment;
        size_t size = var->tls_size ? var->tls_size : 1;
        void* address = Allocator_alloc(state->allocator, size, 1, alignment);
        if(!address) return CI_OOM_ERROR;
        memcpy(address, var->interp_val, var->tls_size);
        *slot = (CiTlsSlot){.address = address, .allocation_size = size, .alignment = alignment};
    }
    *out = (void*_Nonnull)slot->address;
    return 0;
}

static
int
ci_lookup_type_member(CiInterpreter* ci, CcQualType qt, const CiRtSlice* name, _Bool method, CcField* out){
    int err;
    memset(out, 0, sizeof *out);
    if(!name->count || !name->data) return 0;
    AtomTable* at = ci_lock_atoms(ci);
    Atom atom = AT_atomize(at, name->data, name->count);
    ci_unlock_atoms(ci, at);
    if(!atom) return CI_OOM_ERROR;
    uint64_t offset;
    CcQualType type;
    CcField* field = NULL;
    err = cc_lookup_field_offset(&ci->parser, qt, atom, &offset, &type, &field);
    if(err) return err;
    if(!field || field->is_method != method) return 0;
    *out = *field;
    out->type = type;
    out->offset = (uint32_t)offset;
    return 0;
}

static
void
ci_reflect_field(const CcField* field, CiRtField* result){
    *result = (CiRtField){
        .type = field->type,
        .name.data = (void*)(uintptr_t)(field->name ? field->name->data : ""),
        .name.count = field->name ? field->name->length : 0,
        .offset = field->offset,
        .bitwidth = field->bitwidth,
        .bitoffset = field->bitoffset,
        .is_bitfield = field->is_bitfield,
    };
}

static
int
ci_reflect_method(CiInterpreter* ci, SrcLoc loc, const CcField* field, CiRtMethod* result){
    int err;
    CiRtModuleMember info;
    ci_lock_resolver(ci);
    err = ci_reflect_func_unlocked(ci, loc, field->method, &info, 1);
    ci_unlock_resolver(ci);
    if(err) return err;
    *result = (CiRtMethod){
        .type = field->type,
        .name = info.name,
        .offset = field->offset,
        .address = (uintptr_t)info.address,
    };
    return 0;
}

static
int
ci_rt_type_name(CiInterpreter* ci, uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
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
    *(CiRtSlice*)result = (CiRtSlice){
        .count = a->length,
        .data = (void*)(uintptr_t)a->data,
    };
    return 0;
}

static
int
ci_rt_type_tag(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    Atom tag = NULL;
    CcTypeKind k = ccqt_kind(qt);
    if(k == CC_STRUCT)     tag = ccqt_as_struct(qt)->name;
    else if(k == CC_UNION) tag = ccqt_as_union(qt)->name;
    else if(k == CC_ENUM)  tag = ccqt_as_enum(qt)->name;
    if(!tag) tag = nil_atom;
    *(CiRtSlice*)result = (CiRtSlice){
        .count = tag->length,
        .data = (void*)(uintptr_t)tag->data,
    };
    return 0;
}

static
int
ci_rt_type_is_valid(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = qt.bits != 0;
    return 0;
}

static
int
ci_rt_type_is_invalid(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = qt.bits == 0;
    return 0;
}

static
int
ci_rt_type_is_integer(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_is_integer(qt);
    return 0;
}

static
int
ci_rt_type_is_float(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_is_basic(qt) && ccbt_is_float(ccqt_basic_kind(qt));
    return 0;
}

static
int
ci_rt_type_is_arithmetic(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = (ccqt_is_basic(qt) && ccbt_is_arithmetic(ccqt_basic_kind(qt))) || ccqt_kind(qt) == CC_ENUM;
    return 0;
}

static
int
ci_rt_type_is_pointer(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_kind(qt) == CC_POINTER;
    return 0;
}

static
int
ci_rt_type_is_struct(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_kind(qt) == CC_STRUCT;
    return 0;
}

static
int
ci_rt_type_is_union(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_kind(qt) == CC_UNION;
    return 0;
}

static
int
ci_rt_type_is_array(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_is_array(qt);
    return 0;
}

static
int
ci_rt_type_is_vector(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_kind(qt) == CC_VECTOR;
    return 0;
}

static
int
ci_rt_type_is_slice(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_kind(qt) == CC_SLICE;
    return 0;
}

static
int
ci_rt_type_is_function(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_kind(qt) == CC_FUNCTION;
    return 0;
}

static
int
ci_rt_type_is_enum(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_kind(qt) == CC_ENUM;
    return 0;
}

static
int
ci_rt_type_is_const(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_array_object_type(qt).is_const;
    return 0;
}

static
int
ci_rt_type_is_volatile(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_array_object_type(qt).is_volatile;
    return 0;
}

static
int
ci_rt_type_is_atomic(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = qt.is_atomic;
    return 0;
}

static
int
ci_rt_type_is_unsigned(CiInterpreter* ci, uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_is_unsigned(qt, !ci_target(ci)->char_is_signed);
    return 0;
}

static
int
ci_rt_type_is_signed(CiInterpreter* ci, uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_is_integer(qt) && !ccqt_is_unsigned(qt, !ci_target(ci)->char_is_signed);
    return 0;
}

static
int
ci_rt_type_is_callable(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = ccqt_callable_function(qt) != NULL;
    return 0;
}

static
int
ci_rt_type_is_incomplete(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    *(_Bool*)result = qt.bits && ccqt_kind(qt) != CC_FUNCTION && !ccqt_is_complete_object(qt);
    return 0;
}

static
int
ci_rt_type_is_variadic(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    CcFunction* f = ccqt_callable_function(qt);
    *(_Bool*)result = f && f->is_variadic;
    return 0;
}

static
int
ci_rt_type_sizeof(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(ccqt_kind(qt) == CC_VLA)
        return ci_error(ci, loc, "_Type.sizeof_: VLA bounds are unavailable through a runtime _Type value");
    uint32_t sz;
    err = cc_sizeof_as_uint(&ci->parser, qt, loc, &sz);
    if(err) return err;
    *(size_t*)result = sz;
    return 0;
}

static
int
ci_rt_type_alignof(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    uint32_t al;
    err = cc_alignof_as_uint(&ci->parser, qt, loc, &al);
    if(err) return err;
    *(size_t*)result = al;
    return 0;
}

static
int
ci_rt_type_pointee(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(ccqt_kind(qt) != CC_POINTER){
        MStringBuilder* sb = ci_start_error(ci, loc, "_Type.pointee: not a pointer type: ");
        cc_print_type(sb, qt);
        return ci_finish_error(ci, loc);
    }
    CcPointer* ptr = ccqt_as_ptr(qt);
    *(uintptr_t*)result = ptr->pointee.bits;
    return 0;
}

static
int
ci_rt_type_unqual(CiInterpreter* ci, uintptr_t type_bits, void* result){
    int err;
    CcQualType qt = {.bits = type_bits};
    CcQualType uq;
    ci_lock_resolver(ci);
    err = cc_unqual_object_type(&ci->parser, qt, &uq);
    ci_unlock_resolver(ci);
    if(err) return err;
    *(uintptr_t*)result = uq.bits;
    return 0;
}

static
int
ci_rt_type_count(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(ccqt_kind(qt) == CC_VLA)
        return ci_error(ci, loc, "_Type.count: VLA bounds are unavailable through a runtime _Type value");
    if(!ccqt_is_fixed_sequence(qt)){
        MStringBuilder* sb = ci_start_error(ci, loc, "_Type.count: not an array type: ");
        cc_print_type(sb, qt);
        return ci_finish_error(ci, loc);
    }
    *(size_t*)result = ccqt_sequence_length(qt);
    return 0;
}

static
int
ci_rt_type_loc(uintptr_t type_bits, void* result){
    CcQualType qt = {.bits = type_bits};
    SrcLoc* out = result;
    switch(ccqt_kind(qt)){
        case CC_VLA: *out = ccqt_as_vla(qt)->loc; break;
        case CC_ALIGNED: ci_unsafe_unreachable();
        DRP_CASES_EXHAUSTED;
        case CC_ENUM:
            *out = ccqt_as_enum(qt)->loc;
            break;
        case CC_STRUCT:
            *out = ccqt_as_struct(qt)->loc;
            break;
        case CC_UNION:
            *out = ccqt_as_union(qt)->loc;
            break;
        case CC_VECTOR:
        case CC_FUNCTION:
        case CC_BLOCK_POINTER:
        case CC_SLICE:
        case CC_ARRAY:
        case CC_POINTER:
        case CC_BASIC:
            out->bits = 0;
            break;
    }
    return 0;
}

static
int
ci_rt_type_is_callable_with(CiInterpreter* ci, uintptr_t type_bits, uintptr_t arg, void* result){
    CcQualType qt = {.bits = type_bits};
    CiRtSlice types;
    memcpy(&types, (const void*)arg, sizeof types);
    *(_Bool*)result = types.count <= UINT32_MAX && cc_is_callable_with(ci_target(ci), qt, types.data, (uint32_t)types.count);
    return 0;
}

static
int
ci_rt_type_is_callable_through(CiInterpreter* ci, uintptr_t type_bits, uintptr_t arg, void* result){
    CcQualType qt = {.bits = type_bits};
    CcQualType target = {.bits = arg};
    *(_Bool*)result = cc_is_callable_through(ci_target(ci), qt, target);
    return 0;
}

static
int
ci_rt_type_castable_to(CiInterpreter* ci, uintptr_t type_bits, uintptr_t arg, void* result){
    CcQualType qt = {.bits = type_bits};
    CcQualType target = {.bits = arg};
    *(_Bool*)result = cc_explicit_castable(ci_target(ci), qt, target);
    return 0;
}

static
int
ci_rt_type_make_any(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, uintptr_t arg, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(!cc_any_payload_type(ci_target(ci), qt))
        return ci_error(ci, loc, "_Type.make_any requires a complete, non-array object type fitting its payload");
    CiRtAny* any = result;
    memset(any, 0, sizeof *any);
    uint32_t sz;
    err = cc_sizeof_as_uint(&ci->parser, qt, loc, &sz);
    if(err) return err;
    if(sz && !arg){
        return ci_error(ci, loc, "Null pointer as arg to make_any");
    }
    any->type.unqual = qt.unqual;
    if(sz) memcpy(any->payload, (const void*)arg, sz);
    return 0;
}

static
int
ci_rt_type_field(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, uintptr_t index, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcTypeKind k = ccqt_kind(qt);
    if(k != CC_STRUCT && k != CC_UNION)
        return ci_error(ci, loc, "_Type.field: not a struct or union type");
    CcStruct* s = ccqt_as_struct(qt);
    if(index >= s->field_count)
        return ci_error(ci, loc, "_Type.field: index out of range");
    const CcField* field = &s->fields[index];
    ci_reflect_field(field, result);
    return 0;
}

static
int
ci_rt_type_field_named(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, const CiRtSlice* name, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcTypeKind k = ccqt_kind(qt);
    if(k != CC_STRUCT && k != CC_UNION)
        return ci_error(ci, loc, "_Type.field: not a struct or union type");
    CcField field;
    err = ci_lookup_type_member(ci, qt, name, 0, &field);
    if(err) return err;
    if(!field.type.bits)
        return ci_error(ci, loc, "_Type.field: no field with that name");
    ci_reflect_field(&field, result);
    return 0;
}

static
int
ci_rt_type_has_field_named(CiInterpreter* ci, uintptr_t type_bits, const CiRtSlice* name, void* result){
    int err;
    CcQualType qt = {.bits = type_bits};
    CcTypeKind k = ccqt_kind(qt);
    if(k != CC_STRUCT && k != CC_UNION){
        *(_Bool*)result = 0;
        return 0;
    }
    CcField field;
    err = ci_lookup_type_member(ci, qt, name, 0, &field);
    if(err) return err;
    *(_Bool*)result = field.type.bits != 0;
    return 0;
}

static
int
ci_rt_type_method(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, uintptr_t index, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcTypeKind k = ccqt_kind(qt);
    if(k != CC_STRUCT && k != CC_UNION)
        return ci_error(ci, loc, "_Type.method: not a struct or union type");
    CcStruct* s = ccqt_as_struct(qt);
    if(index >= s->methods.count)
        return ci_error(ci, loc, "_Type.method: index out of range");
    const CcField* field = &s->methods.data[index];
    return ci_reflect_method(ci, loc, field, result);
}

static
int
ci_rt_type_method_named(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, const CiRtSlice* name, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcTypeKind k = ccqt_kind(qt);
    if(k != CC_STRUCT && k != CC_UNION)
        return ci_error(ci, loc, "_Type.method: not a struct or union type");
    CcField field;
    err = ci_lookup_type_member(ci, qt, name, 1, &field);
    if(err) return err;
    if(!field.type.bits)
        return ci_error(ci, loc, "_Type.method: no method with that name");
    return ci_reflect_method(ci, loc, &field, result);
}

static
int
ci_rt_type_has_method_named(CiInterpreter* ci, uintptr_t type_bits, const CiRtSlice* name, void* result){
    int err;
    CcQualType qt = {.bits = type_bits};
    CcTypeKind k = ccqt_kind(qt);
    if(k != CC_STRUCT && k != CC_UNION){
        *(_Bool*)result = 0;
        return 0;
    }
    CcField field;
    err = ci_lookup_type_member(ci, qt, name, 1, &field);
    if(err) return err;
    *(_Bool*)result = field.type.bits != 0;
    return 0;
}

static
int
ci_rt_type_enumerators(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(ccqt_kind(qt) != CC_ENUM)
        return ci_error(ci, loc, "_Type.enumerators: not an enum type");
    CcEnum* en = ccqt_as_enum(qt);
    *(size_t*)result = en->enumerator_count;
    return 0;
}

static
int
ci_rt_type_enumerator(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, uintptr_t arg, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(ccqt_kind(qt) != CC_ENUM)
        return ci_error(ci, loc, "_Type.enumerator: not an enum type");
    CcEnum* enum_ = ccqt_as_enum(qt);
    uintptr_t idx = arg;
    if(idx >= enum_->enumerator_count)
        return ci_error(ci, loc, "_Type.enumerator: index out of range");
    CcEnumerator* enumerator = enum_->enumerators[idx];
    CiRtEnumerator* out = (CiRtEnumerator*)result;
    *out = (CiRtEnumerator){
        .name.data = (void*)(uintptr_t)(enumerator->name ? enumerator->name->data : ""),
        .name.count = enumerator->name ? enumerator->name->length : 0,
        .value = enumerator->value,
        .type = enumerator->type,
    };
    return 0;
}

static
int
ci_rt_type_return_type(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcFunction* f = ccqt_callable_function(qt);
    if(!f)
        return ci_error(ci, loc, "_Type.return_type: not a function type");
    *(uintptr_t*)result = f->return_type.bits;
    return 0;
}

static
int
ci_rt_type_param_count(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcFunction* f = ccqt_callable_function(qt);
    if(!f)
        return ci_error(ci, loc, "_Type.param_count: not a function type");
    *(size_t*)result = f->param_count;
    return 0;
}

static
int
ci_rt_type_param_type(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, uintptr_t arg, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcFunction* f = ccqt_callable_function(qt);
    if(!f)
        return ci_error(ci, loc, "_Type.param_type: not a function type");
    uintptr_t idx = arg;
    if(idx >= f->param_count)
        return ci_error(ci, loc, "_Type.param_type: index out of range");
    *(uintptr_t*)result = f->params[idx].bits;
    return 0;
}

static
int
ci_rt_type_element_type(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcTypeKind k = ccqt_kind(qt);
    if(k == CC_ARRAY || k == CC_VECTOR || k == CC_VLA){
        *(uintptr_t*)result = ccqt_qualified_array_element(qt).bits;
        return 0;
    }
    if(k == CC_SLICE){
        *(uintptr_t*)result = ccqt_as_slice(qt)->pointee.bits;
        return 0;
    }
    return ci_error(ci, loc, "_Type.element_type: not an array, slice or vector type");
}

static
int
ci_rt_type_underlying_type(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(ccqt_kind(qt) != CC_ENUM)
        return ci_error(ci, loc, "_Type.underlying_type: not an enum type");
    *(uintptr_t*)result = ccqt_as_enum(qt)->underlying.bits;
    return 0;
}

static
int
ci_rt_type_fields(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcTypeKind k = ccqt_kind(qt);
    if(k != CC_STRUCT && k != CC_UNION)
        return ci_error(ci, loc, "_Type.fields: not a struct or union type");
    CcStruct* s = ccqt_as_struct(qt);
    *(size_t*)result = s->field_count;
    return 0;
}

static
int
ci_rt_type_methods(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits, void* result){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    CcTypeKind k = ccqt_kind(qt);
    if(k != CC_STRUCT && k != CC_UNION)
        return ci_error(ci, loc, "_Type.methods: not a struct or union type");
    CcStruct* s = ccqt_as_struct(qt);
    *(size_t*)result = s->methods.count;
    return 0;
}

static
int
ci_rt_type_validate_field(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(ccqt_kind(qt) != CC_STRUCT && ccqt_kind(qt) != CC_UNION){
        MStringBuilder* sb = ci_start_error(ci, loc, "_Type.field: not a struct or union type: ");
        cc_print_type(sb, qt);
        return ci_finish_error(ci, loc);
    }
    return 0;
}

static
int
ci_rt_type_validate_method(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(ccqt_kind(qt) != CC_STRUCT && ccqt_kind(qt) != CC_UNION){
        MStringBuilder* sb = ci_start_error(ci, loc, "_Type.method: not a struct or union type: ");
        cc_print_type(sb, qt);
        return ci_finish_error(ci, loc);
    }
    return 0;
}

static
int
ci_rt_type_validate_enumerator(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(ccqt_kind(qt) != CC_ENUM){
        MStringBuilder* sb = ci_start_error(ci, loc, "_Type.enumerator: not an enum type: ");
        cc_print_type(sb, qt);
        return ci_finish_error(ci, loc);
    }
    return 0;
}

static
int
ci_rt_type_validate_param_type(CiInterpreter* ci, uint64_t loc_bits, uintptr_t type_bits){
    SrcLoc loc = {.bits = loc_bits};
    CcQualType qt = {.bits = type_bits};
    if(!ccqt_callable_function(qt)){
        MStringBuilder* sb = ci_start_error(ci, loc, "_Type.param_type: not a function type: ");
        cc_print_type(sb, qt);
        return ci_finish_error(ci, loc);
    }
    return 0;
}

static
int
ci_rt_module_symbol(CiInterpreter* ci, uint64_t loc_bits, CiModule* module, const CiRtSlice* name, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    if(name->count && !name->data)
        return ci_error(ci, loc, "_Module.symbol name must not be NULL");
    void* sym = NULL;
    CiRtAny any = {0};
    if(name->count){
        err = ci_lookup_symbol(ci, loc, module, name->data, name->count, (CcQualType){0}, &sym, &any.type);
        if(err) return err;
    }
    memcpy(any.payload, &sym, sizeof sym);
    memcpy(result, &any, sizeof any);
    return 0;
}

static
int
ci_rt_module_symbol_typed(CiInterpreter* ci, uint64_t loc_bits, CiModule* module, const CiRtSlice* name, uintptr_t expected_bits, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    if(name->count && !name->data)
        return ci_error(ci, loc, "_Module.symbol name must not be NULL");
    void* sym = NULL;
    CcQualType expected = {.bits = expected_bits};
    if(name->count){
        err = ci_lookup_symbol(ci, loc, module, name->data, name->count, expected, &sym, NULL);
        if(err) return err;
    }
    memcpy(result, &sym, sizeof sym);
    return 0;
}

static
int
ci_rt_module_parse_type(CiInterpreter* ci, uint64_t loc_bits, CiModule* module, const CiRtSlice* name, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    if(name->count && !name->data)
        return ci_error(ci, loc, "_Module.parse_type name must not be NULL");
    CcQualType type = {0};
    err = ci_parse_module_type(ci, loc, module, (StringView){name->count, name->data}, &type);
    if(err) return err;
    memcpy(result, &type, sizeof type);
    return 0;
}

static
int
ci_rt_srcloc_file(CiInterpreter* ci, uint64_t bits, void* result){
    SrcLoc loc = {.bits = bits};
    size_t value = loc.file_id;
    if(loc.is_actually_a_pointer){
        SrcLocExp* exp = (SrcLocExp*)((uintptr_t)loc.pointer.bits << 1);
        while(exp->parent) exp = exp->parent;
        value = exp->file_id;
    }
    CiRtSlice file = {0};
    ci_lock_resolver(ci);
    FileCache* fc = ci->parser.cpp.fc;
    if(loc.bits && value < fc->map.count){
        CStringView path = fc->map.data[value].path;
        file = (CiRtSlice){.count = path.length, .data = (void*)(uintptr_t)path.text};
    }
    ci_unlock_resolver(ci);
    memcpy(result, &file, sizeof file);
    return 0;
}

static
int
ci_rt_srcloc_line(uint64_t bits, void* result){
    SrcLoc loc = {.bits = bits};
    size_t value = loc.line;
    if(loc.is_actually_a_pointer){
        SrcLocExp* exp = (SrcLocExp*)((uintptr_t)loc.pointer.bits << 1);
        while(exp->parent) exp = exp->parent;
        value = exp->line;
    }
    memcpy(result, &value, sizeof value);
    return 0;
}

static
int
ci_rt_srcloc_col(uint64_t bits, void* result){
    SrcLoc loc = {.bits = bits};
    size_t value = loc.column;
    if(loc.is_actually_a_pointer){
        SrcLocExp* exp = (SrcLocExp*)((uintptr_t)loc.pointer.bits << 1);
        while(exp->parent) exp = exp->parent;
        value = exp->column;
    }
    memcpy(result, &value, sizeof value);
    return 0;
}

static
int
ci_rt_symbolicate(CiInterpreter* ci, uint64_t loc, uintptr_t entry, SrcLoc* dest){
    if(!dest) return ci_error(ci, (SrcLoc){.bits = loc}, "__builtin_symbolicate output must not be NULL");
    _Static_assert(sizeof entry == sizeof *dest, "backtrace entry size");
    memcpy(dest, &entry, sizeof *dest);
    return 0;
}

static
int
ci_rt_intern(CiInterpreter* ci, uint64_t loc, const CiRtSlice* input, void* result){
    CiRtSlice s;
    memcpy(&s, input, sizeof s);
    if(s.count && !s.data)
        return ci_error(ci, (SrcLoc){.bits = loc}, "__builtin_intern data must not be NULL");
    AtomTable* at = ci_lock_atoms(ci);
    Atom a = AT_atomize(at, s.count ? s.data : "", s.count);
    ci_unlock_atoms(ci, at);
    if(!a) return CI_OOM_ERROR;
    CiRtSlice interned = {.count = a->length, .data = (void*)(uintptr_t)a->data};
    memcpy(result, &interned, sizeof interned);
    return 0;
}

static
int
ci_rt_hotswap(CiInterpreter* ci, void (*old_ptr)(void), void (*new_ptr)(void), void* result){
    int ret = 1;
    LOCK_T_lock(&ci->closure_map_lock);
    CcFunc* old_func = BPM_rget(&ci->closure_map, (void*)old_ptr);
    CcFunc* new_func = BPM_rget(&ci->closure_map, (void*)new_ptr);
    LOCK_T_unlock(&ci->closure_map_lock);
    if(old_func && old_func->hotswappable){
        if(old_func == new_func)
            ret = 0;
        else if(new_func && cc_types_same(
            (CcQualType){.bits = (uintptr_t)old_func->type},
            (CcQualType){.bits = (uintptr_t)new_func->type}
        )){
            drp_atomic_ptr_store(&old_func->hotswap, new_func);
            ret = 0;
        }
    }
    memcpy(result, &ret, sizeof ret);
    return 0;
}

static
int
ci_rt_compile(CiInterpreter* ci, uint64_t loc, CcModule* parent, const CiRtSlice* source_arg, const CiRtSlice* path_arg, void* result){
    CiRtSlice source, path;
    memcpy(&source, source_arg, sizeof source);
    memcpy(&path, path_arg, sizeof path);
    if(source.count && !source.data)
        return ci_error(ci, (SrcLoc){.bits = loc}, "__compile source data must not be NULL");
    if(path.count && !path.data)
        return ci_error(ci, (SrcLoc){.bits = loc}, "__compile path data must not be NULL");
    CiModule* module = NULL;
    int err = ci_compile_module(ci, parent, (StringView){source.count, source.count ? source.data : ""}, (StringView){path.count, path.count ? path.data : ""}, &module);
    if(err == CI_OOM_ERROR) return err;
    memcpy(result, &module, sizeof module);
    return 0;
}

static
int
ci_compile_module(CiInterpreter* ci, CcModule* parent, StringView source, StringView path, CiModule*_Nullable*_Nonnull out){
    int err = 0;
    *out = NULL;
    ci_lock_resolver(ci);
    CcParser* p = &ci->parser;
    _Bool eager = p->eager_parsing;
    CcScope* old_current = p->current;
    CcModule* old_module = p->module;

    CiModule* module = Allocator_zalloc(ci_allocator(ci), 1, sizeof *module, _Alignof(CiModule));
    if(!module){
        err = CI_OOM_ERROR;
        goto done;
    }
    module->cc.scope.parent = &parent->scope;
    size_t source_len = source.length;
    char* source_copy = "";
    if(source_len){
        // TODO: maybe this should just go through the file cache?
        source_copy = Allocator_dupe(ci_allocator(ci), source.text, source_len, 1, 1);
        if(!source_copy){
            err = CI_OOM_ERROR;
            goto done;
        }
    }
    StringView src = (StringView){source_len, source_copy};
    p->module = &module->cc;
    p->current = &module->cc.scope;
    cc_parser_discard_input(p);

    if(path.length) fc_write_path(p->cpp.fc, path.text, path.length);
    else fc_write_pathf(p->cpp.fc, "<__compile:%zu>", ci->next_module_id++);
    uint32_t file_id = 0;
    err = fc_intern_path(p->cpp.fc, &file_id);
    if(err) goto done;
    CppFrame frame = {
        .file_id = file_id,
        .txt = src,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CppFrame)(&p->cpp.frames, p->cpp.allocator, frame);
    if(err){ err = CI_OOM_ERROR; goto done; }
    p->eager_parsing = 1;
    err = cc_parse_all(p);
    if(err) goto done;

    err = PM_put(&ci->modules, ci_allocator(ci), module, module);
    if(err){
        err = CI_OOM_ERROR;
        goto done;
    }
    *out = module;
    done:
    p->eager_parsing = eager;
    p->current = old_current;
    p->module = old_module;
    if(err){
        cc_parser_discard_input(p);
        // XXX: cleanup module
        //   probably just its scope, nodes, and its storage itself.
    }
    ci_unlock_resolver(ci);
    return err;
}


static
int
ci_parse_module_type(CiInterpreter* ci, SrcLoc loc, CiModule* module, StringView source, CcQualType* out){
    *out = (CcQualType){0};
    CcParser* p = &ci->parser;
    int err = 0;

    ci_lock_resolver(ci);
    CcModule* previous = p->module;
    p->module = &module->cc;
    err = cc_parse_type_string(p, &module->cc.scope, loc, source, out);
    p->module = previous;
    if(err && err != CI_OOM_ERROR){
        *out = (CcQualType){0};
        err = 0;
    }
    ci_unlock_resolver(ci);
    (void)loc;
    return err;
}

static
size_t
ci_count_atom_items(AtomMapItems items){
    size_t count = 0;
    for(size_t i = 0; i < items.count; i++)
        count += items.data[i].atom != nil_atom && items.data[i].p != NULL;
    return count;
}

static
size_t
ci_count_atom_items16(AtomMap16Items items){
    size_t count = 0;
    for(size_t i = 0; i < items.count; i++)
        count += items.data[i].payload[0] != 0;
    return count;
}

static
int
ci_reflect_func_unlocked(CiInterpreter* ci, SrcLoc loc, CcFunc* func, CiRtModuleMember* out, _Bool define){
    void* address = NULL;
    if(define){
        if(!func->defined){
            if(!func->native_func){
                Atom fsym = func->mangle?func->mangle:func->name;
                int err = ci_try_dlsym(ci, fsym, &address);
                if(err) return err;
                if(address)
                    func->native_func = (void(*)(void))address;
            }
            address = (void*)func->native_func;
        }
        else {
            int err = ci_deps_add_func(&ci->deps, ci_allocator(ci), func, CC_FUNC_DEP_USED | CC_FUNC_DEP_ADDR_TAKEN);
            if(err) return CI_OOM_ERROR;
            err = ci_resolve_deps(ci, &ci->deps);
            if(err) return err;
            if(!func->native_func){
                err = ci_create_closure(ci, func);
                if(err) return err;
            }
            address = (void*)func->native_func;
        }
    }
    *out = (CiRtModuleMember){
        .type = (CcQualType){.bits = (uintptr_t)func->type},
        .name = {
            .count = func->name ? func->name->length : 0,
            .data = (void*)(uintptr_t)(func->name ? func->name->data : ""),
        },
        .address = address,
        .loc = func->loc,
    };
    (void)loc;
    return 0;
}

static
int
ci_reflect_var_unlocked(CiInterpreter* ci, SrcLoc loc, CcVariable* var, CiRtModuleMember* out){
    void* address = NULL;
    if(!var->automatic){
        if(var->thread_local_){
            int err = ci_deps_add_var(&ci->deps, ci_allocator(ci), var, CI_VAR_DEP_USED);
            if(err) return CI_OOM_ERROR;
            err = ci_resolve_deps(ci, &ci->deps);
            if(err) return err;
            err = ci_tls_address(ci, var, &address);
            if(err) return err;
        }
        else if(var->extern_ && !var->initializer){
            if(!var->interp_val){
                Atom vsym = var->mangle?var->mangle:var->name;
                int err = ci_try_dlsym(ci, vsym, &address);
                if(err) return err;
                if(address)
                    var->interp_val = address;
            }
            address = var->interp_val;
        }
        else {
            int err = ci_deps_add_var(&ci->deps, ci_allocator(ci), var, CI_VAR_DEP_USED);
            if(err) return CI_OOM_ERROR;
            err = ci_resolve_deps(ci, &ci->deps);
            if(err) return err;
            address = var->interp_val;
        }
    }
    *out = (CiRtModuleMember){
        .type = var->type,
        .name = {
            .count = var->name ? var->name->length : 0,
            .data = (void*)(uintptr_t)(var->name ? var->name->data : ""),
        },
        .address = address,
        .loc = var->loc,
    };
    (void)loc;
    return 0;
}

static
int
ci_reflect_type_from_maps(CiRtModuleMember* out, CcScope* scope, size_t idx){
    AtomMap16Items typedefs = AM16_items(&scope->typedefs);
    for(size_t i = 0; i < typedefs.count; i++){
        AtomMap16Item item = typedefs.data[i];
        CcTypedef* td = (CcTypedef*)item.payload;
        if(!td->type.bits) continue;
        if(idx--) continue;
        *out = (CiRtModuleMember){
            .type = td->type,
            .name = {
                .count = item.atom?item.atom->length:0,
                .data = (void*)(uintptr_t)(item.atom?item.atom->data:""),
            },
            .address = NULL,
            .loc = cc_typedef_loc(td),
        };
        return 0;
    }
    AtomMapItems structs = AM_items(&scope->structs);
    for(size_t i = 0; i < structs.count; i++){
        if(!structs.data[i].p) continue;
        if(idx--) continue;
        CcStruct* s = structs.data[i].p;
        *out = (CiRtModuleMember){
            .type = (CcQualType){.bits = (uintptr_t)s},
            .name = {
                .count = s->name ? s->name->length : 0,
                .data = (void*)(uintptr_t)(s->name ? s->name->data : ""),
            },
            .address = NULL,
            .loc = s->loc,
        };
        return 0;
    }
    AtomMapItems unions = AM_items(&scope->unions);
    for(size_t i = 0; i < unions.count; i++){
        if(!unions.data[i].p) continue;
        if(idx--) continue;
        CcUnion* u = unions.data[i].p;
        *out = (CiRtModuleMember){
            .type = (CcQualType){.bits = (uintptr_t)u},
            .name = {
                .count = u->name ? u->name->length : 0,
                .data = (void*)(uintptr_t)(u->name ? u->name->data : ""),
            },
            .address = NULL,
            .loc = u->loc,
        };
        return 0;
    }
    AtomMapItems enums = AM_items(&scope->enums);
    for(size_t i = 0; i < enums.count; i++){
        if(!enums.data[i].p) continue;
        if(idx--) continue;
        CcEnum* e = enums.data[i].p;
        *out = (CiRtModuleMember){
            .type = (CcQualType){.bits = (uintptr_t)e},
            .name = {
                .count = e->name ? e->name->length : 0,
                .data = (void*)(uintptr_t)(e->name ? e->name->data : ""),
            },
            .address = NULL,
            .loc = e->loc,
        };
        return 0;
    }
    return CI_SYMBOL_NOT_FOUND;
}

static
int
ci_rt_module_func_count(CiInterpreter* ci, CiModule* module, void* result){
    CcScope* scope = &module->cc.scope;
    ci_lock_resolver(ci);
    size_t count = ci_count_atom_items(AM_items(&scope->functions));
    ci_unlock_resolver(ci);
    memcpy(result, &count, sizeof count);
    return 0;
}

static
int
ci_rt_module_var_count(CiInterpreter* ci, CiModule* module, void* result){
    CcScope* scope = &module->cc.scope;
    ci_lock_resolver(ci);
    size_t count = ci_count_atom_items(CcAnonAM_items(&scope->variables));
    ci_unlock_resolver(ci);
    memcpy(result, &count, sizeof count);
    return 0;
}

static
int
ci_rt_module_type_count(CiInterpreter* ci, CiModule* module, void* result){
    CcScope* scope = &module->cc.scope;
    ci_lock_resolver(ci);
    size_t count = ci_count_atom_items16(AM16_items(&scope->typedefs))
        + ci_count_atom_items(AM_items(&scope->structs))
        + ci_count_atom_items(AM_items(&scope->unions))
        + ci_count_atom_items(AM_items(&scope->enums));
    ci_unlock_resolver(ci);
    memcpy(result, &count, sizeof count);
    return 0;
}

static
int
ci_rt_module_func(CiInterpreter* ci, uint64_t loc_bits, CiModule* module, uintptr_t arg, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    CcScope* scope = &module->cc.scope;
    CiRtModuleMember member = {0};
    ci_lock_resolver(ci);
    AtomMapItems items = AM_items(&scope->functions);
    CcFunc* func = NULL;
    for(size_t i = 0; i < items.count; i++){
        if(!items.data[i].p) continue;
        if(arg--) continue;
        func = items.data[i].p;
        break;
    }
    if(!func){
        err = ci_error(ci, loc, "_Module.func index out of range");
        goto finally;
    }
    err = ci_reflect_func_unlocked(ci, loc, func, &member, 1);
    finally:
    ci_unlock_resolver(ci);
    if(err) return err;
    memcpy(result, &member, sizeof member);
    return 0;
}

static
int
ci_rt_module_func_info(CiInterpreter* ci, uint64_t loc_bits, CiModule* module, uintptr_t arg, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    CcScope* scope = &module->cc.scope;
    CiRtModuleMember member = {0};
    ci_lock_resolver(ci);
    AtomMapItems items = AM_items(&scope->functions);
    CcFunc* func = NULL;
    for(size_t i = 0; i < items.count; i++){
        if(!items.data[i].p) continue;
        if(arg--) continue;
        func = items.data[i].p;
        break;
    }
    if(!func){
        err = ci_error(ci, loc, "_Module.func index out of range");
        goto finally;
    }
    err = ci_reflect_func_unlocked(ci, loc, func, &member, 0);
    finally:
    ci_unlock_resolver(ci);
    if(err) return err;
    memcpy(result, &member, sizeof member);
    return 0;
}

static
int
ci_rt_module_var(CiInterpreter* ci, uint64_t loc_bits, CiModule* module, uintptr_t arg, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    CcScope* scope = &module->cc.scope;
    CiRtModuleMember member = {0};
    ci_lock_resolver(ci);
    AtomMapItems items = CcAnonAM_items(&scope->variables);
    CcVariable* var = NULL;
    for(size_t i = 0; i < items.count; i++){
        if(items.data[i].atom == nil_atom) continue;
        if(!items.data[i].p) continue;
        if(arg--) continue;
        var = items.data[i].p;
        break;
    }
    if(!var){
        err = ci_error(ci, loc, "_Module.var index out of range");
        goto finally;
    }
    err = ci_reflect_var_unlocked(ci, loc, var, &member);
    finally:
    ci_unlock_resolver(ci);
    if(err) return err;
    memcpy(result, &member, sizeof member);
    return 0;
}

static
int
ci_rt_module_type(CiInterpreter* ci, uint64_t loc_bits, CiModule* module, uintptr_t arg, void* result){
    int err;
    SrcLoc loc = {.bits = loc_bits};
    CcScope* scope = &module->cc.scope;
    CiRtModuleMember member = {0};
    ci_lock_resolver(ci);
    err = ci_reflect_type_from_maps(&member, scope, arg);
    if(err == CI_SYMBOL_NOT_FOUND)
        err = ci_error(ci, loc, "_Module.type index out of range");
    ci_unlock_resolver(ci);
    if(err) return err;
    memcpy(result, &member, sizeof member);
    return 0;
}

static
int
ci_lookup_symbol(CiInterpreter* ci, SrcLoc loc, CiModule* module, const char* name, size_t len, CcQualType expected, void*_Nullable*_Nonnull out, CcQualType*_Nullable pointer_type){
    *out = NULL;
    AtomTable* at = ci_lock_atoms(ci);
    Atom atom = AT_atomize(at, name, len);
    ci_unlock_atoms(ci, at);
    if(!atom) return CI_OOM_ERROR;
    name = atom->data;

    int ret = 0;
    CcQualType symbol_type = {0};
    ci_lock_resolver(ci);
    CcSymbol sym;
    CcScope* scope = &module->cc.scope;
    int walk = CC_SCOPE_WALK_CHAIN;
    if(cc_scope_lookup_symbol(scope, atom, walk, &sym)){
        switch(sym.kind){
            case CC_SYM_FUNC: {
                CcFunc* func = sym.func;
                CcQualType func_type = {.bits = (uintptr_t)func->type};
                symbol_type = func_type;
                if(expected.bits && !cc_types_same(func_type, expected))
                    goto done;
                CiRtModuleMember member;
                ret = ci_reflect_func_unlocked(ci, loc, func, &member, 1);
                if(ret) goto done;
                if(func->defined && !member.address)
                    { ret = ci_error(ci, loc, "_Module.symbol: function '%s' has no address", name); goto done; }
                *out = member.address;
                goto done;
            }
            case CC_SYM_VAR: {
                CcVariable* var = sym.var;
                symbol_type = var->type;
                if(expected.bits && !cc_types_same(var->type, expected))
                    goto done;
                CiRtModuleMember member;
                ret = ci_reflect_var_unlocked(ci, loc, var, &member);
                if(ret) goto done;
                if(!var->automatic && !var->thread_local_ && !(var->extern_ && !var->initializer) && !member.address)
                    { ret = ci_error(ci, loc, "_Module.symbol: variable '%s' has no storage", name); goto done; }
                *out = member.address;
                goto done;
            }
            case CC_SYM_TYPEDEF:
            case CC_SYM_ENUMERATOR:
                goto done;
        }
    }
done:
    if(!ret && *out && pointer_type){
        CcPointer* ptr = cc_intern_pointer(&ci->parser.type_cache, cc_allocator(&ci->parser), symbol_type, 0, 0);
        if(!ptr) ret = CI_OOM_ERROR;
        else *pointer_type = (CcQualType){.bits = (uintptr_t)ptr};
    }
    ci_unlock_resolver(ci);
    return ret;
}

static
int
ci_rt_prepare_module(CiInterpreter* ci, uint64_t loc_bits, CiModule* module, void* result){
    SrcLoc loc = {.bits = loc_bits};
    int err;
    ci_lock_resolver(ci);
    if(module->callable && module->callable_node_count == module->cc.nodes.count){
        memcpy(result, &module->callable->native_func, sizeof module->callable->native_func);
        ci_unlock_resolver(ci);
        return 0;
    }
    Allocator al = cc_allocator(&ci->parser);
    size_t count = module->cc.nodes.count;
    if(count >= UINT32_MAX){
        err = _cc_overflow_error;
        goto cleanup;
    }
    CcFunc* f = Allocator_zalloc(al, 1, sizeof *f, _Alignof(CcFunc));
    CcStmtNode* body = Allocator_zalloc_extra(al, count + 1, sizeof *body->stmts, sizeof *body, _Alignof(CcStmtNode));
    CcStmtNode* ret = Allocator_zalloc(al, 1, sizeof *ret, _Alignof(CcStmtNode));
    CcExpr* zero = Allocator_zalloc(al, 1, sizeof *zero, _Alignof(CcExpr));
    if(!f || !body || !ret || !zero){
        err = CI_OOM_ERROR;
        goto cleanup;
    }
    CcFunction* type = cc_intern_function(&ci->parser.type_cache, al, ccqt_basic(CCBT_int), NULL, 0, 0, 0, 0);
    if(!type){
        err = CI_OOM_ERROR;
        goto cleanup;
    }
    *zero = (CcExpr){.kind = CC_EXPR_VALUE, .type = ccqt_basic(CCBT_int), .loc = loc};
    *ret = (CcStmtNode){.kind = CC_STMT_RETURN, .no_fallthrough = 1, .return_.expr = zero, .loc = loc};
    body->kind = CC_STMT_COMPOUND;
    body->count = (uint32_t)count + 1;
    body->loc = loc;
    if(count) memcpy(body->stmts, module->cc.nodes.data, count * sizeof *body->stmts);
    body->stmts[count] = ret;
    f->type = type;
    f->parsed = f->defined = 1;
    f->body_tree = body;
    f->module = &module->cc;
    f->loc = loc;
    err = ci_lower_func(ci, f, &ci->deps);
    if(err) goto cleanup;
    err = ci_resolve_deps(ci, &ci->deps);
    if(err) goto cleanup;
    void (*entry)(void) = (void(*)(void))(uintptr_t)f;
    err = ci_closure_map_put(ci, f, (void*)entry);
    if(err){
        err = CI_OOM_ERROR;
        goto cleanup;
    }
    f->native_func = entry;
    module->callable = f;
    module->callable_node_count = count;
    memcpy(result, &entry, sizeof entry);
    cleanup:
    ci_unlock_resolver(ci);
    return err;
}

// These helpers mean we can always call runtime funcs
// even if the generic native call bridge is not enabled.
static
int
ci_invoke_int2(const NcCallPlan* plan, void (*fn)(void), NcCallState* state){
    uintptr_t args[2];
    for(unsigned i = 0; i < 2; i++)
        nc_arg_read(state, plan, i, &args[i]);
    int result = ((int(*)(uintptr_t, uintptr_t))fn)(args[0], args[1]);
    nc_result_write(state, plan, &result);
    return 0;
}

static
int
ci_invoke_int3(const NcCallPlan* plan, void (*fn)(void), NcCallState* state){
    uintptr_t args[3];
    for(unsigned i = 0; i < 3; i++)
        nc_arg_read(state, plan, i, &args[i]);
    int result = ((int(*)(uintptr_t, uintptr_t, uintptr_t))fn)(args[0], args[1], args[2]);
    nc_result_write(state, plan, &result);
    return 0;
}

static
int
ci_invoke_int4(const NcCallPlan* plan, void (*fn)(void), NcCallState* state){
    uintptr_t args[4];
    for(unsigned i = 0; i < 4; i++)
        nc_arg_read(state, plan, i, &args[i]);
    int result = ((int(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))fn)(args[0], args[1], args[2], args[3]);
    nc_result_write(state, plan, &result);
    return 0;
}

static
int
ci_invoke_int5(const NcCallPlan* plan, void (*fn)(void), NcCallState* state){
    uintptr_t args[5];
    for(unsigned i = 0; i < 5; i++)
        nc_arg_read(state, plan, i, &args[i]);
    int result = ((int(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))fn)(args[0], args[1], args[2], args[3], args[4]);
    nc_result_write(state, plan, &result);
    return 0;
}

static
int
ci_invoke_int6(const NcCallPlan* plan, void (*fn)(void), NcCallState* state){
    uintptr_t args[6];
    for(unsigned i = 0; i < 6; i++)
        nc_arg_read(state, plan, i, &args[i]);
    int result = ((int(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))fn)(args[0], args[1], args[2], args[3], args[4], args[5]);
    nc_result_write(state, plan, &result);
    return 0;
}

typedef union CiRuntimeSignature CiRuntimeSignature;
union CiRuntimeSignature {
    CcFunction type;
    struct {
        union {
            uint32_t _bits;
            struct {
                CcTypeKind kind: 4;
                uint32_t is_variadic: 1,
                         no_prototype: 1,
                         _padding: 10,
                         fixed_param_count: 16;
            };
        };
        uint32_t param_count;
        CcQualType return_type;
        CcQualType params[6];
    } storage;
};
_Static_assert(offsetof(CiRuntimeSignature, storage.params) == offsetof(CcFunction, params), "runtime signature layout");
_Static_assert(_Alignof(CiRuntimeSignature) == _Alignof(CcFunction), "runtime signature alignment");
static const CcPointer ci_rt_pointer = {
    .kind = CC_POINTER,
    .pointee = {.basic.kind = CCBT_void},
};
#define CI_RT_PTR {.bits = (uintptr_t)&ci_rt_pointer}
#define CI_RT_U64 {.basic.kind = CCBT_unsigned_long_long}
#define CI_RT_SIGNATURE(name, count, ...) \
    static const CiRuntimeSignature ci_rt_signature_##name = {.storage = { \
        .kind = CC_FUNCTION, \
        .fixed_param_count = count, \
        .param_count = count, \
        .return_type = {.basic.kind = CCBT_int}, \
        .params = {__VA_ARGS__}, \
    }}
CI_RT_SIGNATURE(pupp, 4, CI_RT_PTR, CI_RT_U64, CI_RT_PTR, CI_RT_PTR);
CI_RT_SIGNATURE(puup, 4, CI_RT_PTR, CI_RT_U64, CI_RT_U64, CI_RT_PTR);
CI_RT_SIGNATURE(pppp, 4, CI_RT_PTR, CI_RT_PTR, CI_RT_PTR, CI_RT_PTR);
CI_RT_SIGNATURE(pupppp, 6, CI_RT_PTR, CI_RT_U64, CI_RT_PTR, CI_RT_PTR, CI_RT_PTR, CI_RT_PTR);
CI_RT_SIGNATURE(pup, 3, CI_RT_PTR, CI_RT_U64, CI_RT_PTR);
CI_RT_SIGNATURE(up, 2, CI_RT_U64, CI_RT_PTR);
CI_RT_SIGNATURE(puuup, 5, CI_RT_PTR, CI_RT_U64, CI_RT_U64, CI_RT_U64, CI_RT_PTR);
CI_RT_SIGNATURE(puupp, 5, CI_RT_PTR, CI_RT_U64, CI_RT_U64, CI_RT_PTR, CI_RT_PTR);
CI_RT_SIGNATURE(puu, 3, CI_RT_PTR, CI_RT_U64, CI_RT_U64);
CI_RT_SIGNATURE(ppp, 3, CI_RT_PTR, CI_RT_PTR, CI_RT_PTR);
CI_RT_SIGNATURE(pupup, 5, CI_RT_PTR, CI_RT_U64, CI_RT_PTR, CI_RT_U64, CI_RT_PTR);
CI_RT_SIGNATURE(puppp, 5, CI_RT_PTR, CI_RT_U64, CI_RT_PTR, CI_RT_PTR, CI_RT_PTR);
CI_RT_SIGNATURE(puppup, 6, CI_RT_PTR, CI_RT_U64, CI_RT_PTR, CI_RT_PTR, CI_RT_U64, CI_RT_PTR);
#undef CI_RT_SIGNATURE
#undef CI_RT_PTR
#undef CI_RT_U64
static const CiRuntimeSignature ci_rt_signature_none = {.storage = {
    .kind = CC_FUNCTION,
    .return_type = {.basic.kind = CCBT_int},
}};

enum {
    CI_RT_SIG_pupp,
    CI_RT_SIG_puup,
    CI_RT_SIG_pppp,
    CI_RT_SIG_pupppp,
    CI_RT_SIG_pup,
    CI_RT_SIG_up,
    CI_RT_SIG_puuup,
    CI_RT_SIG_puupp,
    CI_RT_SIG_puu,
    CI_RT_SIG_ppp,
    CI_RT_SIG_pupup,
    CI_RT_SIG_puppp,
    CI_RT_SIG_puppup,
    CI_RT_SIG_none,
    CI_RT_SIG_COUNT,
};
static const CcFunction* _Nonnull const ci_rt_signatures[CI_RT_SIG_COUNT] = {
    [CI_RT_SIG_pupp]   = &ci_rt_signature_pupp.type,
    [CI_RT_SIG_puup]   = &ci_rt_signature_puup.type,
    [CI_RT_SIG_pppp]   = &ci_rt_signature_pppp.type,
    [CI_RT_SIG_pupppp] = &ci_rt_signature_pupppp.type,
    [CI_RT_SIG_pup]    = &ci_rt_signature_pup.type,
    [CI_RT_SIG_up]     = &ci_rt_signature_up.type,
    [CI_RT_SIG_puuup]  = &ci_rt_signature_puuup.type,
    [CI_RT_SIG_puupp]  = &ci_rt_signature_puupp.type,
    [CI_RT_SIG_puu]    = &ci_rt_signature_puu.type,
    [CI_RT_SIG_ppp]    = &ci_rt_signature_ppp.type,
    [CI_RT_SIG_pupup]  = &ci_rt_signature_pupup.type,
    [CI_RT_SIG_puppp]  = &ci_rt_signature_puppp.type,
    [CI_RT_SIG_puppup] = &ci_rt_signature_puppup.type,
    [CI_RT_SIG_none]   = &ci_rt_signature_none.type,
};

#define CI_RUNTIME_HELPERS(X) \
    X(intern,                   pupp,   4, ci_rt_intern)                   \
    X(symbolicate,              puup,   4, ci_rt_symbolicate)              \
    X(hotswap,                  pppp,   4, ci_rt_hotswap)                  \
    X(compile,                  pupppp, 6, ci_rt_compile)                  \
    X(module_entry,             pupp,   4, ci_rt_prepare_module)           \
    X(type_name,                pup,    3, ci_rt_type_name)                \
    X(type_tag,                 up,     2, ci_rt_type_tag)                 \
    X(type_is_valid,            up,     2, ci_rt_type_is_valid)            \
    X(type_is_invalid,          up,     2, ci_rt_type_is_invalid)          \
    X(type_is_integer,          up,     2, ci_rt_type_is_integer)          \
    X(type_is_float,            up,     2, ci_rt_type_is_float)            \
    X(type_is_arithmetic,       up,     2, ci_rt_type_is_arithmetic)       \
    X(type_is_pointer,          up,     2, ci_rt_type_is_pointer)          \
    X(type_is_struct,           up,     2, ci_rt_type_is_struct)           \
    X(type_is_union,            up,     2, ci_rt_type_is_union)            \
    X(type_is_array,            up,     2, ci_rt_type_is_array)            \
    X(type_is_vector,           up,     2, ci_rt_type_is_vector)           \
    X(type_is_slice,            up,     2, ci_rt_type_is_slice)            \
    X(type_is_function,         up,     2, ci_rt_type_is_function)         \
    X(type_is_enum,             up,     2, ci_rt_type_is_enum)             \
    X(type_is_const,            up,     2, ci_rt_type_is_const)            \
    X(type_is_volatile,         up,     2, ci_rt_type_is_volatile)         \
    X(type_is_atomic,           up,     2, ci_rt_type_is_atomic)           \
    X(type_is_unsigned,         pup,    3, ci_rt_type_is_unsigned)         \
    X(type_is_signed,           pup,    3, ci_rt_type_is_signed)           \
    X(type_is_callable,         up,     2, ci_rt_type_is_callable)         \
    X(type_is_incomplete,       up,     2, ci_rt_type_is_incomplete)       \
    X(type_is_variadic,         up,     2, ci_rt_type_is_variadic)         \
    X(type_sizeof,              puup,   4, ci_rt_type_sizeof)              \
    X(type_alignof,             puup,   4, ci_rt_type_alignof)             \
    X(type_pointee,             puup,   4, ci_rt_type_pointee)             \
    X(type_unqual,              pup,    3, ci_rt_type_unqual)              \
    X(type_count,               puup,   4, ci_rt_type_count)               \
    X(type_loc,                 up,     2, ci_rt_type_loc)                 \
    X(type_is_callable_with,    puup,   4, ci_rt_type_is_callable_with)    \
    X(type_is_callable_through, puup,   4, ci_rt_type_is_callable_through) \
    X(type_castable_to,         puup,   4, ci_rt_type_castable_to)         \
    X(type_make_any,            puuup,  5, ci_rt_type_make_any)            \
    X(type_field,               puuup,  5, ci_rt_type_field)               \
    X(type_field_named,         puupp,  5, ci_rt_type_field_named)         \
    X(type_method,              puuup,  5, ci_rt_type_method)              \
    X(type_method_named,        puupp,  5, ci_rt_type_method_named)        \
    X(type_has_field_named,     pupp,   4, ci_rt_type_has_field_named)     \
    X(type_has_method_named,    pupp,   4, ci_rt_type_has_method_named)    \
    X(type_enumerators,         puup,   4, ci_rt_type_enumerators)         \
    X(type_enumerator,          puuup,  5, ci_rt_type_enumerator)          \
    X(type_return_type,         puup,   4, ci_rt_type_return_type)         \
    X(type_param_count,         puup,   4, ci_rt_type_param_count)         \
    X(type_param_type,          puuup,  5, ci_rt_type_param_type)          \
    X(type_element_type,        puup,   4, ci_rt_type_element_type)        \
    X(type_underlying_type,     puup,   4, ci_rt_type_underlying_type)     \
    X(type_fields,              puup,   4, ci_rt_type_fields)              \
    X(type_methods,             puup,   4, ci_rt_type_methods)             \
    X(type_validate_field,      puu,    3, ci_rt_type_validate_field)      \
    X(type_validate_method,     puu,    3, ci_rt_type_validate_method)     \
    X(type_validate_enumerator, puu,    3, ci_rt_type_validate_enumerator) \
    X(type_validate_param_type, puu,    3, ci_rt_type_validate_param_type) \
    X(module_func_count,        ppp,    3, ci_rt_module_func_count)        \
    X(module_var_count,         ppp,    3, ci_rt_module_var_count)         \
    X(module_type_count,        ppp,    3, ci_rt_module_type_count)        \
    X(module_func,              pupup,  5, ci_rt_module_func)              \
    X(module_func_info,         pupup,  5, ci_rt_module_func_info)         \
    X(module_var,               pupup,  5, ci_rt_module_var)               \
    X(module_type,              pupup,  5, ci_rt_module_type)              \
    X(module_symbol,            puppp,  5, ci_rt_module_symbol)            \
    X(module_symbol_typed,      puppup, 6, ci_rt_module_symbol_typed)      \
    X(module_parse_type,        puppp,  5, ci_rt_module_parse_type)        \
    X(srcloc_file,              pup,    3, ci_rt_srcloc_file)              \
    X(srcloc_line,              up,     2, ci_rt_srcloc_line)              \
    X(srcloc_col,               up,     2, ci_rt_srcloc_col)

#define CI_RT_FUNCTION(name_, signature_, count_, function_) \
    static const union { \
        Atom_ atom; \
        struct { \
            uint32_t length: 31, flags: 1; \
            uint32_t hash; \
            char data[sizeof("runtime." #name_)]; \
        } storage; \
    } ci_rt_name_##name_ = {.storage = { \
        .length = sizeof("runtime." #name_) - 1, \
        .data = "runtime." #name_, \
    }}; \
    static const CcFunc ci_rt_function_##name_ = { \
        .name = &ci_rt_name_##name_.atom, \
        .type = &ci_rt_signature_##signature_.type, \
        .native_func = (void(*)(void))function_, \
    };
CI_RUNTIME_HELPERS(CI_RT_FUNCTION)
#undef CI_RT_FUNCTION

static const struct {
    size_t offset;
    unsigned signature;
    CiCallDescriptor descriptor;
} ci_rt_templates[] = {
    #define CI_RT_TEMPLATE(name_, signature_, count_, function_) { \
        .offset = offsetof(CiInterpreter, runtime_calls.name_), \
        .signature = CI_RT_SIG_##signature_, \
        .descriptor = { \
            .func = &ci_rt_function_##name_, \
            .invoke = ci_invoke_int##count_, \
            .call_type = &ci_rt_signature_##signature_.type, \
            .nargs = count_, \
        }, \
    },
    CI_RUNTIME_HELPERS(CI_RT_TEMPLATE)
    #undef CI_RT_TEMPLATE
    {
        .offset = offsetof(CiInterpreter, runtime_calls.module_run),
        .signature = CI_RT_SIG_none,
        .descriptor = {
            .invoke = nc_invoke,
            .call_type = &ci_rt_signature_none.type,
        },
    },
};
#undef CI_RUNTIME_HELPERS

static
int
ci_init_runtime_calls(CiInterpreter* ci){
    Allocator al = cc_allocator(&ci->parser);
    const CcTargetConfig* target = ci_target(ci);
    NcCallPlan* plans[CI_RT_SIG_COUNT];
    for(size_t i = 0; i < countof(plans); i++){
        int err = nc_plan_create(al, target, ci_rt_signatures[i], &plans[i]);
        if(err) return err;
    }
    CiCallDescriptor* descriptors = Allocator_alloc(al, countof(ci_rt_templates), sizeof *descriptors, _Alignof(CiCallDescriptor));
    if(!descriptors) return CI_OOM_ERROR;
    for(size_t i = 0; i < countof(ci_rt_templates); i++){
        CiCallDescriptor* runtime = &descriptors[i];
        *runtime = ci_rt_templates[i].descriptor;
        runtime->abi = plans[ci_rt_templates[i].signature];
        CiCallDescriptor** field = (CiCallDescriptor**)((char*)ci + ci_rt_templates[i].offset);
        *field = runtime;
    }
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
