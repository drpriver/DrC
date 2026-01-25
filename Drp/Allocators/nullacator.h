//
// Copyright © 2022-2023, David Priver <david@davidpriver.com>
//
#ifndef NULLACATOR_H
#define NULLACATOR_H

#include "allocator.h"

#ifndef NULLACATOR
// Useful for types that expect to manage a resizable array but you don't want them to alloc
// (and you don't want to abort() like with ALLOCATOR_UNSET).
#define NULLACATOR ((Allocator){.type=ALLOCATOR_NULL})
#endif

#endif
