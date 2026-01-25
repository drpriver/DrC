//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef DRP_ATOM_DECL_H
#define DRP_ATOM_DECL_H

//
// AT() macro for declaring atoms inline
//
// Usage:
//   AT(identifier)              -> ATOM_AT_identifier (text = "identifier")
//   AT(identifier, "text")      -> ATOM_AT_identifier (text = "text")
//
// The atom_decl_gen tool scans for AT() usage and generates atom declarations.
//
#define AT(...) AT_(__VA_ARGS__, )
#define AT_(identifier, ...) ATOM_AT_##identifier

#endif
