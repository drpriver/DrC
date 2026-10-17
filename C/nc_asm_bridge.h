#ifndef NC_ASM_BRIDGE_H
#define NC_ASM_BRIDGE_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef NO_NATIVE_CALL
// Decls for helpers implemented in asm.
extern void nc_abi_invoke(NcCallState*, void (*fn)(void), size_t stack_size, size_t stack_alignment);
extern const unsigned char nc_capture_slots[];
extern const unsigned char nc_capture_slots_end[];
// This function is not callable from C.
extern void nc_capture_entry(void);
#endif
#endif
