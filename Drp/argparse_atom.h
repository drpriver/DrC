#ifndef DRP_ARGPARSE_ATOM_H
#define DRP_ARGPARSE_ATOM_H
#include "argument_parsing.h"
#include "atom.h"
#include "atom_table.h"


#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
int 
ap_atom_converter(void* ud, const char* txt, size_t len, void* dst){
    AtomTable* at = ud;
    Atom a = AT_atomize(at, txt, len);
    if(!a) return 1;
    *(Atom*)dst = a;
    return 0;
}

static
void
ap_atom_printer(const ArgParser* ap, void* p){
    Atom* pa = p;
    Atom a = *pa;
    ap->print(ap->hout, " = '%s'", a->data);
}



static inline
ArgParseDestination
ArgAtomDest(Atom _Nullable*_Nonnull dst, AtomTable* at){
    static ArgParseUserDefinedType apudt;
    if(apudt.user_data && apudt.user_data != at)
        __builtin_trap();
    if(!apudt.user_data){
        apudt.converter = ap_atom_converter;
        apudt.default_printer = ap_atom_printer;
        apudt.type_name = SV("string");
        apudt.type_size = sizeof(Atom);
        apudt.user_data = at;
    }
    return (ArgParseDestination){
        .type = ARG_USER_DEFINED,
        .pointer = dst,
        .user_pointer = &apudt,
    };
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
