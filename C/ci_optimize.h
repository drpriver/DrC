#ifndef CI_OPTIMIZE_H
#define CI_OPTIMIZE_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include <stddef.h>
#include "ci_op.h"
#include "../Drp/atom_map.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
typedef struct CiInterpreter CiInterpreter;
typedef struct CcFunc CcFunc;
// resolver lock must be held when calling these (or before execution starts,
// which can spawn threads).
static int ci_optimize_func(CiInterpreter*, CcFunc*);
static int ci_optimize_code(CiInterpreter*, Marray(CiOp)*, uint32_t*, size_t, AtomMap(uintptr_t)*_Nullable, size_t*_Nullable);
static int ci_optimize_toplevel(CiInterpreter*);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
