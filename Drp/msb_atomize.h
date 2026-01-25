#ifndef DRP_MSB_ATOMIZE_H
#define DRP_MSB_ATOMIZE_H
#include "MStringBuilder.h"
#include "atom.h"
#include "atom_table.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
Atom _Nullable
msb_atomize(MStringBuilder* sb, AtomTable* at){
    if(sb->errored) return NULL;
    StringView sv = msb_borrow_sv(sb);
    Atom a = AT_atomize(at, sv.text, sv.length);
    return a;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
