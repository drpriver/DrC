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
#ifndef MALLOCATORI
#define MALLOCATORI {.type=ALLOCATOR_MALLOC}
#endif

#endif
