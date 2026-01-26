#ifndef C_PP_PREPROCESSOR_H
#define C_PP_PREPROCESSOR_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

#include "../Drp/atom_map.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CPreprocessor CPreprocessor;
struct CPreprocessor {
    AtomMap(CMacro) macros;
    struct 

};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
