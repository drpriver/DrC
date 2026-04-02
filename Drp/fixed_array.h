//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
// fixed_array.h
// -------------
// Usage:
// ------
//   #define FixedArray_Name FixedArrayInt8
//   #define FixedArray_N 8
//   #define FixedArray_T int
//   #include "fixed_array.h"

//
// This is kind of dumb that you have to write this yourself, but this is for
// a static capacity dynamic array.
//
#ifndef FIXED_ARRAY_H
#define FIXED_ARRAY_H
// size_t
#include <stddef.h>
#include "mem_util.h"
#define fa_push(array, ...) (*(array)).count < sizeof (*(array)).data / sizeof (*(array)).data[0]? (void)((*(array)).data[(*(array)).count++] = (__VA_ARGS__)) :( __builtin_trap(), (void)0)

#define FA_FOR_EACH(type, iter, fixed) \
for(type \
     *iter = (fixed).data, \
     *iter##end__ = (fixed).data+(fixed).count; \
  iter != iter##end__; \
  ++iter)

#define FA_FOR_EACH_VALUE(type, iter, fixed) \
for(type \
     iter = {0}, \
     *iter##iter__ = (fixed).data, \
     *iter##end__ = (fixed).data+(fixed).count; \
  iter##iter__ != iter##end__?((void)(iter = *iter##iter__),1):0; \
  ++iter##iter__)

#define FOR_I_T(type, iter, array) \
    for(type iter = 0; iter < (array).count; iter++)
#define FOR_I(iter, array) \
    for(iter = 0; iter < (array).count; iter++)

#define fa_remove_at(array, idx) \
    (void)(memremove(idx * sizeof *(array)->data, (array)->data, (array)->count * sizeof *(array)->data, sizeof *(array)->data), (void)(array)->count--)

#endif

#ifndef FixedArray_Name
#error "Must define FixedArray_Name"
#endif
#ifndef FixedArray_N
#error "Must define FixedArray_N"
#endif
#ifndef FixedArray_T
#error "Must define FixedArray_T"
#endif


typedef struct FixedArray_Name FixedArray_Name;
struct FixedArray_Name {
    size_t count;
    FixedArray_T data[FixedArray_N];
};

#ifdef GEN_TYPEINFO
#ifndef CONCAT
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#endif
static const TypeInfoFixedArray CONCAT(TI_, FixedArray_Name) = {
    .name = nil_atom,
    .length = FixedArray_N,
    .data_offset = offsetof(FixedArray_Name, data),
    .type = &CONCAT(TI_, FixedArray_T).type_info,
    .size = sizeof(FixedArray_Name),
    .align = _Alignof(FixedArray_Name),
    .kind  = TIK_FARRAY,
};

#endif

#undef FixedArray_Name
#undef FixedArray_N
#undef FixedArray_T
