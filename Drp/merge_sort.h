#ifndef DRP_MERGE_SORT_H
#define DRP_MERGE_SORT_H
//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
// License: MIT
// Retrieved from: https://github.com/swenson/sort
// Retrieved on: Sep 26, 2024
// Git commit hash: 24f5b8b13810ad130109c7b56daf8e99ab0fe1b8
//
/* Copyright (c) 2010-2024 Christopher Swenson. */
/* Copyright (c) 2012 Vojtech Fried. */
/* Copyright (c) 2012 Google Inc. All Rights Reserved. */
// Based on above file, but changed to be libc style generic with callback
//
#include <stddef.h> // size_t

#ifndef drp_memmove
#if defined(__GNUC__) || defined(__clang__)
#define drp_memmove __builtin_memmove
#else
#include <string.h>
#define drp_memmove memmove
#endif
#endif

#ifndef drp_memcpy
#if defined(__GNUC__) || defined(__clang__)
#define drp_memcpy __builtin_memcpy
#else
#include <string.h>
#define drp_memcpy memcpy
#endif
#endif

#ifndef drp_alloc
#if defined(__GNUC__) || defined(__clang__)
#define drp_alloca __builtin_alloca
#elif defined(_MSC_VER)
#include <malloc.h>
#define drp_alloca _alloca
#else
#error "Don't know alloca for this compiler"
#endif
#endif


#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

#ifndef DRP_SORT_API
#define DRP_SORT_API static
#endif

typedef int drp_cmp_func(void*_Null_unspecified ctx, const void* a, const void* b);

force_inline
void*
drp_elem_n(void* buff, size_t width, size_t idx){
    return (char*)buff+width*idx;
}

DRP_SORT_API void drp_binary_insertion_sort(void* dst, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp);
DRP_SORT_API void drp_merge_sort(void* scratch, void* dst, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp);
static inline size_t drp_binary_insertion_find(void *dst, const void* x, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp);
static void drp_binary_insertion_sort_start(void *dst, size_t start, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp);
static void drp_merge_sort_recursive(void* restrict newdst, void* restrict dst, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp);

DRP_SORT_API
void
drp_binary_insertion_sort(void* dst, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp){
    if(nel <= 1) return;
    drp_binary_insertion_sort_start(dst, 1, nel, width, ctx, cmp);
}

static inline
size_t
drp_binary_insertion_find(void *dst, const void* x, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp){
    size_t l = 0;
    size_t r = nel - 1;
    size_t c = r >> 1;

    if(cmp(ctx, x, drp_elem_n(dst, width, 0)) < 0)
        return 0;
    if(cmp(ctx, x, drp_elem_n(dst, width, r)) > 0)
        return r;

    void* cx = drp_elem_n(dst, width, c);

    for(;;){
        const int val = cmp(ctx, x, cx);
        if(val < 0) {
            if(c - l <= 1) return c;
            r = c;
        }
        else {
            if(r - c <= 1) return c + 1;
            l = c;
        }

        c = l + ((r - l) >> 1);
        cx = drp_elem_n(dst, width, c);
    }
}

static
void
drp_binary_insertion_sort_start(void *dst, size_t start, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp){
    for(size_t i = start; i < nel; i++) {
        if(cmp(ctx, drp_elem_n(dst, width, i - 1), drp_elem_n(dst, width, i)) <= 0)
            continue;

        void* x = drp_alloca(width);
        drp_memcpy(x, drp_elem_n(dst, width, i), width);
        size_t location = drp_binary_insertion_find(dst, x, i, width, ctx, cmp);
        for(size_t j = i - 1; j >= location; j--) {
            drp_memcpy(drp_elem_n(dst, width, j+1), drp_elem_n(dst, width, j), width);
            if(!j) break;
        }
        drp_memcpy(drp_elem_n(dst, width, location), x, width);
    }
}

DRP_SORT_API
void
drp_merge_sort(void* restrict scratch, void* restrict dst, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp){
    if(nel <= 1) return;
    if(nel <= 16){
        drp_binary_insertion_sort(dst, nel, width, ctx, cmp);
        return;
    }
    drp_merge_sort_recursive(scratch, dst, nel, width, ctx, cmp);
}

static
void
drp_merge_sort_recursive(void* restrict newdst, void* restrict dst, size_t nel, size_t width, void*_Null_unspecified ctx, drp_cmp_func* cmp){
    const size_t middle = nel / 2;
    size_t out = 0;
    size_t i = 0;
    size_t j = middle;

    if(nel <= 1) return;
    if(nel <= 16){
        drp_binary_insertion_sort(dst, nel, width, ctx, cmp);
        return;
    }

    drp_merge_sort_recursive(newdst, dst, middle, width, ctx, cmp);
    drp_merge_sort_recursive(newdst, drp_elem_n(dst, width, middle), nel - middle, width, ctx, cmp);

    while(out != nel) {
        if(i < middle) {
            if(j < nel) {
                if(cmp(ctx, drp_elem_n(dst, width, i), drp_elem_n(dst, width, j)) <= 0){
                    drp_memcpy(drp_elem_n(newdst, width, out), drp_elem_n(dst, width, i++), width);
                }
                else {
                    drp_memcpy(drp_elem_n(newdst, width, out), drp_elem_n(dst, width, j++), width);
                }
            }
            else {
                drp_memcpy(drp_elem_n(newdst, width, out), drp_elem_n(dst, width, i++), width);
            }
        }
        else {
            drp_memcpy(drp_elem_n(newdst, width, out), drp_elem_n(dst, width, j++), width);
        }

        out++;
    }

    drp_memcpy(dst, newdst, nel*width);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
