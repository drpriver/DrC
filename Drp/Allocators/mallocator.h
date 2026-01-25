//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef MALLOCATOR_H
#define MALLOCATOR_H
#include "allocator.h"

// This can be overriden
#ifndef MALLOCATOR
#define MALLOCATOR ((Allocator){.type=ALLOCATOR_MALLOC})
#endif

#ifdef MALLOCATOR_UNSAFE_TRACK_STATS
static struct {
    size_t bytes;
    size_t count;
} malloc_usage;
#endif

#endif
