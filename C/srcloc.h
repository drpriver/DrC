#ifndef C_SRC_LOC_H
#define C_SRC_LOC_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
_Static_assert(sizeof(uint64_t) == sizeof(uintptr_t), "");
typedef struct SrcLoc SrcLoc;
struct SrcLoc {
    union {
        uint64_t bits; // 0 is invalid
        struct {
            uint64_t is_actually_a_pointer: 1;
            uint64_t line: 31;
            uint64_t file_id: 16;
            uint64_t column: 16;
        };
        _Static_assert(sizeof(uint64_t) >= sizeof(void*), "");
        struct {
            uint64_t is_actually_a_pointer: 1;
            uint64_t bits: 63; // (SrcLocExp*)(pointer.bits<<1)
        } pointer;
    };
};
// Should be allocated in an arena
typedef struct SrcLocExp SrcLocExp;
struct SrcLocExp {
    uint64_t line: 32;
    uint64_t file_id: 16;
    uint64_t column: 16;
    SrcLocExp*_Nullable parent;
};
#endif
