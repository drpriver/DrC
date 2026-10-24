#ifndef C_NC_NATIVE_CALL_H
#define C_NC_NATIVE_CALL_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "cc_target.h"
#include "../Drp/Allocators/allocator.h"
#include "../Drp/typed_enum.h"
#include <stddef.h>
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

enum NcAbiRole TYPED_ENUM(uint32_t) {
    NC_ABI_RETURN,
    NC_ABI_NAMED_ARGUMENT,
    NC_ABI_VARIADIC_ARGUMENT
};
TYPEDEF_ENUM(NcAbiRole, uint32_t);
enum NcAbiMode TYPED_ENUM(uint32_t){
    NC_ABI_IGNORED,
    NC_ABI_REGISTERS,
    NC_ABI_STACK_VALUE,
    NC_ABI_INDIRECT
};
TYPEDEF_ENUM(NcAbiMode, uint32_t);
enum NcAbiBank TYPED_ENUM(uint16_t){
    NC_ABI_GP,
    NC_ABI_FP,
    NC_ABI_X87
};
TYPEDEF_ENUM(NcAbiBank, uint16_t);

typedef struct NcAbiPiece NcAbiPiece;
struct NcAbiPiece {
    uint16_t value_offset: 6, // Byte offset in the C object, 0-63.
             size:         5; // Up to 16 bytes per register piece.
    NcAbiBank bank:        2;
    uint16_t _padding:     3;
};
_Static_assert(sizeof(NcAbiPiece) == 2, "");
_Static_assert(NC_ABI_X87 < 4, "ABI bank fits in two bits");

typedef struct NcAbiType NcAbiType;
struct NcAbiType {
    uint32_t  size,
              align_log2:  5; // Alignment is UINT32_C(1) << align_log2.
    NcAbiMode mode:        2; // NcAbiMode.
    uint32_t  count:       3, // 0-4 register pieces.
              sign_extend: 1,
              zero_extend: 1,
              _padding:    20;
    NcAbiPiece pieces[4];
};
_Static_assert(sizeof(NcAbiType) == 16, "");
_Static_assert(NC_ABI_INDIRECT < 4, "ABI mode fits in two bits");

typedef struct NcAbiArgument NcAbiArgument;
struct NcAbiArgument {
    NcAbiType type;
    uint32_t gp:       4,
             fp:       4, // Register indices, including the exhausted value 8.
             on_stack: 1,
             _padding: 23;
    uint32_t stack_offset,
             copy_offset;
};
_Static_assert(sizeof(NcAbiArgument) == 28, "");
typedef struct NcCallPlan NcCallPlan;
struct NcCallPlan {
    CcTarget target;
    uint32_t count,
             fixed_count: 31,
             is_variadic: 1;
    NcAbiType result;
    unsigned gp, fp, named_gp, named_fp;
    size_t stack_size, named_stack_size, copy_size;
    size_t stack_alignment; // Includes alignment required by indirect copies.
    NcAbiArgument args[];
};

typedef struct NcCaptureArm64 NcCaptureArm64;
struct NcCaptureArm64 {
    uint64_t gp[8];
    void* _Nullable indirect_result;
    unsigned char* stack_args;
    unsigned char fp[8][16];
    uint64_t result_gp[2];
    unsigned char result_fp[4][16];
};
_Static_assert(offsetof(NcCaptureArm64, fp) == 80, "ARM64 FP offset");
_Static_assert(offsetof(NcCaptureArm64, result_gp) == 208, "ARM64 result offset");
_Static_assert(sizeof(NcCaptureArm64) == 288, "ARM64 capture size");

typedef struct NcCaptureWin64 NcCaptureWin64;
struct NcCaptureWin64 {
    uint64_t gp[4];
    unsigned char fp[4][16];
    unsigned char* stack_args; // Fifth argument; first four are homed at -32.
    uint64_t result_gp[1];
    unsigned char result_fp[16];
};
_Static_assert(offsetof(NcCaptureWin64, stack_args) == 96, "Win64 stack offset");
_Static_assert(offsetof(NcCaptureWin64, result_gp) == 104, "Win64 result offset");
_Static_assert(sizeof(NcCaptureWin64) == 128, "Win64 capture size");

typedef struct NcCaptureSysV64 NcCaptureSysV64;
struct NcCaptureSysV64 {
    // Contiguous SysV va_list register save area: GP at 0, SIMD at 48.
    uint64_t gp[6]; // rdi, rsi, rdx, rcx, r8, r9
    unsigned char fp[8][16];
    uint64_t incoming_rax;     // Low byte: variadic vector-register count.
    unsigned char* stack_args; // Immediately after caller's return address.
    uint64_t result_gp[2];
    unsigned char result_fp[2][16];
    unsigned char result_x87[2][16];
    uint64_t result_x87_count;
    uint64_t reserved;
};
_Static_assert(offsetof(NcCaptureSysV64, gp) == 0, "SysV GP offset");
_Static_assert(offsetof(NcCaptureSysV64, fp) == 48, "SysV FP offset");
_Static_assert(offsetof(NcCaptureSysV64, incoming_rax) == 176, "SysV RAX offset");
_Static_assert(offsetof(NcCaptureSysV64, stack_args) == 184, "SysV stack offset");
_Static_assert(offsetof(NcCaptureSysV64, result_gp) == 192, "SysV result offset");
_Static_assert(offsetof(NcCaptureSysV64, result_x87) == 240, "SysV x87 offset");
_Static_assert(offsetof(NcCaptureSysV64, result_x87_count) == 272, "SysV x87 count offset");
_Static_assert(sizeof(NcCaptureSysV64) == 288, "SysV capture size");

// TEST has stack-only arguments, two GP/FP result cells, and a separate
// pointer for indirect results.
typedef struct NcCaptureTest NcCaptureTest;
struct NcCaptureTest {
    unsigned char* stack_args;
    void* _Nullable indirect_result;
    uint64_t result_gp[2];
    unsigned char result_fp[2][16];
};

typedef union NcCallState NcCallState;
union NcCallState {
    _Alignas(16) unsigned char bytes[320];
    NcCaptureArm64 arm;
    NcCaptureSysV64 sysv;
    NcCaptureWin64 win;
    NcCaptureTest test;
};



static
int
nc_abi_type(const CcTargetConfig *tc, CcQualType qt, NcAbiRole role, NcAbiType *out);
// nc_abi_type()
// -------------
// Classify how the target ABI passes or returns a value of this type, as
// determined by `role`. Usually this is called internally when creating a call
// plan, but you might need it for classifying a variadic argument.
//
// Returns:
// --------
//    0 on success or a non-zero error code on failure. On failure `out` is
//    unchanged.  `_cc_unimplemented_error` means the type is unsupported.
//    Other values indicate actual failure.
//
// Arguments:
// ----------
// tc:
//    The target to classify the type for.
//
// qt:
//    The type to classify.
//
// role:
//    The parameter's role in the call.
//
// out:
//    On success, the calculated type is written here. It is otherwise
//    unchanged.
//

static
int
nc_plan_create(Allocator al, const CcTargetConfig *tc, CcFunction *f,
        NcCallPlan *_Nullable *_Nonnull out);
// nc_plan_create()
// ----------------
// Classify a complete signature, including promoted variadic argument types.
//
// Returns:
// --------
//    0 on success, error code on failure. On failure `out` is unchanged.
//
// Arguments:
// ----------
// al:
//    Allocator to allocate the NcCallPlan with.
//
// tc:
//    Target to create a plan for. Pass `&cc_target_native` for a native plan.
//    This is not retained.
//
// f:
//    The complete signature, including promoted variadic argument types.
//    If the call is variadic, this must be in the signature as it
//    matters on some ABI. This is not retained.
//
// out:
//    The resulting call plan is written here on success.
//    Release it with `nc_plan_destroy`.
//



static
void
nc_plan_destroy(Allocator al, NcCallPlan *p);
// nc_plan_destroy()
// -----------------
// Free a call pla created with `nc_plan_create`.
//
// Arguments:
// ----------
// al:
//    The Allocator used with `nc_plan_create`.
//
// p:
//    The plan to release.
//

static
size_t
nc_state_size(const NcCallPlan *p);
// nc_state_size()
// ---------------
// Compute the allocation size for the `NcCallState` corresponding to this plan.
//
// Returns:
// --------
//   The required allocation size to invoke the call plan, including stack
//   arguments and indirect argument copies after the state.
//   Your allocation should be aligned to 16 bytes.
//
// Arguments:
// ----------
// p:
//   The call plan to compute the allocation size for.
//

static
void
nc_state_init(NcCallState *s, const NcCallPlan *p, void *_Nullable result);
// nc_state_init()
// -------------
// Initialize an allocation of nc_state_size(p) bytes.
//
// Arguments:
// ----------
// s:
//   Setups up a call state for use. The buffer pointed to by `s` should be
//   aligned and sized as according to `nc_state_size(p)`.
//
// p:
//   The call plan for this state.
//
// result:
//   For an indirect return, this pointer should point to a suitably sized and
//   aligned buffer. Otherwise it can be NULL.
//

static
size_t
nc_stack_storage_size(const NcCallPlan *p);
// nc_stack_storage_size()
// -----------------------
// Compute the stack storage size when registers are allocated separately.
//
// Returns:
// --------
//   Bytes required for stack arguments, indirect argument copies, and any
//   prefix. Allocate this storage with alignment `p->stack_alignment`.
//
// Arguments:
// ----------
// p:
//   The call plan to compute the allocation size for.
//

static
size_t
nc_stack_storage_prefix(const NcCallPlan *p);
// nc_stack_storage_prefix()
// -------------------------
// Locate the stack arguments within separately allocated stack storage.
//
// Returns:
// --------
//   Byte offset from the allocation to the stack arguments.
//   On Windows this reserves the home area and alignment padding.
//   Otherwise it is zero.
//
// Arguments:
// ----------
// p:
//   The call plan describing the stack layout.
//

static
size_t
nc_capture_size(CcTarget target);
// nc_capture_size()
// -----------------
// Compute the size of the native bridge's register capture for a target.
//
// Returns:
// --------
//   The capture size in bytes, excluding stack arguments and indirect copies.
//
// Arguments:
// ----------
// target:
//   The target whose capture representation is used.
//

static
size_t
nc_capture_result_offset(CcTarget target);
// nc_capture_result_offset()
// --------------------------
// Locate the result register storage within a native bridge capture.
//
// Returns:
// --------
//   Byte offset of the first GP result register in the capture.
//
// Arguments:
// ----------
// target:
//   The target whose capture representation is used.
//

static
void
nc_registers_init(NcCallState *s, const NcCallPlan *p,
        void *stack_storage, void *_Nullable result);
// nc_registers_init()
// -------------------
// Clear the register buffer and initialize the stack-argument pointer,
// indirect-return pointer, and target-specific call metadata.
// Stack-argument storage is supplied separately and is not cleared.
//
// Arguments:
// ----------
// s:
//   Register storage to clear and initialize, sized and aligned for
//   `NcCallState`.
//
// p:
//   The call plan for this state.
//
// stack_storage:
//   Storage of `nc_stack_storage_size(p)` bytes, aligned to
//   `p->stack_alignment`.
//   This allocation must remain live while the callee uses its arguments.
//
// result:
//   For an indirect return, a suitably sized and aligned destination buffer.
//   Otherwise it can be `NULL`.
//

static
void *_Nullable
nc_indirect_result(const NcCallState *s, const NcCallPlan *p);
// nc_indirect_result()
// --------------------
// Read the indirect-result destination from the ABI register storage.
//
// Returns:
// --------
//   The destination pointer, or `NULL` if the plan returns directly.
//
// Arguments:
// ----------
// s:
//   Call state containing the incoming result destination.
//
// p:
//   The call plan describing the result ABI.
//

static
void
nc_set_result_destination(NcCallState *s, const NcCallPlan *p, void *_Nullable result);
// nc_set_result_destination()
// ---------------------------
// Set the indirect-result destination. Does nothing for direct returns.
//
// Arguments:
// ----------
// s:
//   Call state to update.
//
// p:
//   The call plan describing the result ABI.
//
// result:
//   The destination pointer to store in the ABI register storage.
//

static
void
nc_arg_write(NcCallState *s, const NcCallPlan *p, unsigned index, const void *source);
// nc_arg_write()
// --------------
// Pack argument index from its C object representation into ABI storage.
//
// Arguments:
// ----------
// s:
//    Call state to write into.
//
// p:
//    Call plan describing the ABI of the call.
//
// index:
//    Which arg in the plan to write to.
//
// source:
//    Pointer to the value to write. It should be suitably sized and aligned.
//

static
void
nc_arg_read(NcCallState *s, const NcCallPlan *p, unsigned index, void *dest);
// nc_arg_read()
// -------------
// Unpack argument index from ABI storage into its C object representation.
//
// Arguments:
// ----------
// s:
//   Call state to read from
//
// p:
//   Call plan describing the ABI of the call.
//
// index:
//   Which arg in the plan to read.
//
// dest:
//   Pointer to the buffer to write to. It should be suitably sized and
//   aligned.
//

static
void
nc_result_write(NcCallState *s, const NcCallPlan *p, const void *value);
// nc_result_write()
// -----------------
// Pack a C return value into ABI result storage.
//
// Arguments:
// ----------
// s:
//   Call state to write into.
//
// p:
//   Call plan describing the ABI of the call.
//
// value:
//   The value to write. It should be suitably sized and aligned.
//

static
void
nc_result_read(const NcCallState *s, const NcCallPlan *p, void *value);
// nc_result_read()
// ---------------------------------
// Extract a completed call's ABI result into a C object.
//
// Arguments:
// ----------
// s:
//   Call state to read from.
//
// p:
//   Call plan describing the ABI of the call.
//
// value:
//   Pointer to the buffer to write to. It should be suitably sized and
//   aligned.
//


typedef struct NcClosure NcClosure;
// NcClosure *
// -----------
// Opaque handle owning a native-callable thunk and its callback data.
// Created by `nc_create_closure` and freed by `nc_destroy_closure`.
//

typedef void NcClosureCallback(const NcCallPlan *plan, NcCallState *state,
                               void *_Null_unspecified userdata);
// NcClosureCallback
// -----------------
// The type of the function called when native code invokes a closure.
//
// Use `nc_arg_read` and `nc_result_write` to read arguments
// and write results.
//
// It is the implementer of this function's responsibility to ensure
// they write the return value into `state` using `nc_result_write`,
// or else native callers can read garbage.
//
// Arguments:
// ----------
// plan:
//    The closure-owned host ABI plan. Use it with `nc_arg_read` and
//    `nc_result_write`.
//
//    For variadic signatures it describes the declared parameters; unnamed
//    arguments are accessed through va_list.
//
//    Valid until the closure is destroyed.
//
// state:
//    Captured arguments in the host ABI layout.
//    Write the ABI return value into this state before returning.
//
// userdata:
//    The pointer passed to nc_create_closure.
//

static
int
nc_invoke(const NcCallPlan *plan, void (*fn)(void), NcCallState *state);
// nc_invoke()
// -----------
// Call a native function using prepared host ABI arguments and capture its
// result. The caller owns the plan, state, and referenced storage.
//
// Returns:
// --------
//    If NO_NATIVE_CALL is defined, this return `_cc_unreachable_error`.
//    Otherwise it always returns 0.
//
// Arguments:
// ----------
// plan:
//    The call's host ABI plan, including stack size and alignment.
//
// fn:
//    Native function pointer whose actual signature matches the plan.
//
// state:
//    Prepared argument registers and stack pointer. On return, contains the
//    result registers; indirect results are written to the supplied buffer.
//
// With NO_NATIVE_CALL defined, calling this function aborts.

static
int
nc_create_closure(Allocator al, CcFunction *func_type, NcClosureCallback *cb,
    void *_Null_unspecified userdata, NcClosure *_Nullable *_Nonnull out);
// nc_create_closure()
// -------------------
// Create a native-callable closure backed by a static assembly thunk.
//
// Returns:
// --------
//    0 on success or a `cc_error` code on failure. No cleanup is needed
//    if this function fails. Check for `_cc_unimplemented_error` if the
//    function signature is not supported.
//
//    If NO_NATIVE_CALL is defined, always returns `_cc_unimplemented_error`.
//
// Arguments:
// ----------
// al:
//    Allocator for the closure's internal data.
//
// func_type:
//    Function signature used to compute the closure's host ABI plan.
//    For variadic functions, provide only the declared parameters,
//    but the function must be marked as variadic as this distinction
//    matters on some ABIs.
//
//    The signature is only used to compute the call plan and is not
//    retained.
//
// cb:
//    Callback invoked whenever the thunk is called.
//
// userdata:
//    Borrowed pointer passed unchanged to cb.
//
// out:
//    On success, the new closure will be written through this
//    pointer. On failure, nothing is written.
//
//    Release it with `nc_destroy_closure` using the same allocator.
//

static void (*nc_closure_get_fn(NcClosure *closure))(void);
// nc_closure_get_fn()
// -------------------
// Retrieves the C-callable function pointer associated with a
// closure.
//
// Returns:
// --------
//    The closure's native function pointer. Cast it to the intended
//    function pointer type before calling.
//
//    With NO_NATIVE_CALL defined, this produces only an opaque token,
//    not a callable native function.
//
// Arguments:
// ----------
// closure:
//    The closure to retrieve the native function pointer from.
//

static
void
nc_destroy_closure(Allocator al, NcClosure *closure);
// nc_destroy_closure()
// --------------------
// Releases the thunk and releases the memory used to allocate `closure`.
//
// This is only safe to call if no calls using the closure are currently in
// progress and the closure's function pointer will not be called again.
//
// With NO_NATIVE_CALL defined this is a no-op.
//
// Arguments:
// ----------
// al:
//    The allocator used with `nc_create_closure`.
//
// closure:
//   The closure to destroy. This does not free the userdata associated
//   with the closure.
//



// shared inline helpers, not really part of the API
static inline
size_t
_nc_align_up(size_t size, size_t alignment){
    return (size + alignment - 1) & ~(alignment - 1);
}

static inline
unsigned
_nc_abi_bank_count(NcAbiType t, NcAbiBank b){
    unsigned n = 0;
    for(unsigned i = 0; i < t.count; i++)
        n += t.pieces[i].bank == b;
    return n;
}

static inline
unsigned
_nc_abi_fp_count(NcAbiType t){
    return _nc_abi_bank_count(t, NC_ABI_FP);
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
