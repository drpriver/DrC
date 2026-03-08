#ifndef C_CC_MEMORY_ORDER_H
#define C_CC_MEMORY_ORDER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "../Drp/typed_enum.h"

enum CcMemoryOrder TYPED_ENUM(uint32_t) {
    CC_MO_RELAXED,
    CC_MO_CONSUME,
    CC_MO_ACQUIRE,
    CC_MO_RELEASE,
    CC_MO_ACQ_REL,
    CC_MO_SEQ_CST,
    CC_MO_COUNT,
};
TYPEDEF_ENUM(CcMemoryOrder, uint32_t);

#endif
