#ifndef DRP_PARRAY_H
#define DRP_PARRAY_H
// Marray but for void*
typedef void* _void_pointer;
#define MARRAY_T _void_pointer
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include "Marray.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
typedef Marray(_void_pointer) Parray;
#define pa_push ma_push(_void_pointer)
#define pa_pop ma_pop(_void_pointer)
#define pa_cleanup ma_cleanup(_void_pointer)
#define pa_ensure_total ma_ensure_total(_void_pointer)
#define pa_ensure_additional ma_ensure_additional(_void_pointer)
#define pa_extend ma_extend(_void_pointer)
#define pa_insert ma_insert(_void_pointer)
#define pa_remove_at ma_remove_at(_void_pointer)
#define pa_alloc ma_alloc(_void_pointer)
#define pa_zalloc ma_zalloc(_void_pointer)
#define pa_alloc_index ma_alloc_index(_void_pointer)
#define pa_shrink_to_size ma_shrink_to_size(_void_pointer)
#endif
