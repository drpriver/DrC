#ifndef C_CC_TYPE_CHECK_H
#define C_CC_TYPE_CHECK_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "cc_type.h"
#include "cc_target.h"
#include "cc_expr.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
static _Bool cc_type_predicate(const CcTargetConfig* target, CcTypeIntrospectionOp op, CcQualType qt);
// cc_type_predicate()
// -------------------
// Evaluates the boolean type property selected by `op` for `qt`.

static CcQualType cc_parameter_value_type(CcQualType t);
// cc_parameter_value_type()
// -------------------------
// Removes parameter object qualifiers and explicit alignment from a function
// parameter's type.
//
// Returns:
// --------
// The normalized parameter type.
//
// Arguments:
// ----------
// t: Parameter type after array/function adjustment.
//
static CcQualType cc_default_promotion(CcQualType t);
// cc_default_promotion()
// ----------------------
// Applies default argument promotions to `t`.
//
// Returns:
// --------
// The promoted type without object qualifiers or explicit alignment.
//
// Arguments:
// ----------
// t: Argument type after array/function decay.
//
static _Bool cc_requires_default_promotion(CcQualType t);
// cc_requires_default_promotion()
// -------------------------------
// Checks whether default argument promotions change the underlying value type.
//
// Returns:
// --------
// True if promotion changes the type, ignoring enum identity, qualifiers,
// and explicit alignment.
//
// Arguments:
// ----------
// t: Parameter type to inspect.
//
static _Bool cc_is_value_operand_type(CcQualType t);
// cc_is_value_operand_type()
// --------------------------
// Checks whether `t` supplies a value, including through array/function decay.
//
// Returns:
// --------
// True for complete object types, arrays, and functions.
//
// Arguments:
// ----------
// t: Operand type before decay.
//
static int cc_type_sizeof_complete(const CcTargetConfig* target, CcQualType type,
    uint32_t *out, CcQualType *_Nullable failure_type);
// cc_type_sizeof_complete()
// -------------------------
// Computes the constant size of a type in bytes.
//
// Returns:
// --------
// Zero on success and writes the size to `*out`.
// _cc_not_constant_error for incomplete types or VLAs.
// _cc_overflow_error if the size exceeds the 32-bit object layout limit.
// _cc_unreachable_error for function types or invalid type kinds.
// On failure, writes the failing subtype to `*failure_type` if provided.
//
// Arguments:
// ----------
// target: Target configuration.
// type: Type whose size is requested.
// out: Size output.
// failure_type: Failing subtype output, or `NULL` to ignore it.
//

static _Bool cc_vla_bounds_same(const CcVla* a, const CcVla* b);
// cc_vla_bounds_same()
// --------------------
// Checks if two VLAs have the same bounds. This checks that the bounds is
// defined by the same expression, not that the runtime bounds happens to
// be the same. Two VLAs can share a bounds via typedefs or typeof.
//
// A VLA can also have an unspecified bounds ([*]). In that case, this only
// matches if both VLAs are unspecified.
//
// You might wonder how this is different than `a == b`.
// `a == b` would give the wrong result in the face of qualified VLAs:
//
//   typedef int A[n];
//   A a;
//   const A c;
//
//   `A` and `const A` are different VLAs, but have the same bounds.
//
// Returns:
// --------
// True if the bounds denote the same expression.
//
// Arguments:
// ----------
// a, b: VLA types whose bounds are compared.
//

static _Bool cc_types_same(CcQualType a, CcQualType b);
// cc_types_same()
// ---------------
// Compares canonical type identity.
// This is currently just `a.bits == b.bits`, but there is some unclear
// wording in the C standard, so use this function instead of comparing
// bits directly in case that changes.
//
// Unlike compatibility, this preserves record/enum identity, array
// completeness, prototype presence, and explicit alignment. Distinct VLA
// nodes do not acquire identity from bounds.
//
// Returns:
// --------
// True if the canonical types and their qualifiers are identical.
//
// Arguments:
// ----------
// a, b: Qualified types canonicalized in the same type cache.
//

static _Bool cc_types_compatible(CcQualType a, CcQualType b);
// cc_types_compatible()
// ---------------------
// Checks declaration compatibility, as specified by the C standard.
// Unlike identity, compatibility handles incomplete array vs a complete
// one, a VLA to an array, or an enum to its underlying integer type.
// Explicit alignment wrappers do not affect compatibility, but qualifiers
// must agree.
//
// Returns:
// --------
// True if compatible, false if incompatible or the structural recursion limit
// is reached. Recursive record pairs are recognized to terminate cycles.
//
// Arguments:
// ----------
// a, b: Qualified types to compare.
//

static _Bool cc_types_compatible_unqualified(CcQualType a, CcQualType b);
// cc_types_compatible_unqualified()
// ---------------------------------
// Like cc_types_compatible(), but strips qualifiers.
// This handles nested types like arrays of a qualified type.
//
// Returns:
// --------
// True if compatible after qualifier removal.
//
// Arguments:
// ----------
// a, b: Types to compare.
//

static _Bool cc_types_compatible_ignoring_cv(CcQualType a, CcQualType b);
// cc_types_compatible_ignoring_cv()
// ---------------------------------
// Like cc_types_compatible_unqualified(), but retains `_Atomic`.
//
// Returns:
// --------
// True if compatible under these qualification rules.
//
// Arguments:
// ----------
// a, b: Types to compare
//

static _Bool cc_arrays_copy_compatible(CcQualType a, CcQualType b);
// cc_arrays_copy_compatible()
// ---------------------------
// Checks whether array types have matching bounds and compatible
// types, where compatible is like with cc_types_compatible_ignoring_cv().
// VLA dimensions must share a specified bounds.
//
// Returns:
// --------
// True if both types are arrays satisfying these requirements.
//
// Arguments:
// ----------
// a, b: Source and destination array types.
//

static _Bool cc_qualification_convertible(CcQualType from, CcQualType to);
// cc_qualification_convertible()
// ------------------------------
// Checks that the types are cc_types_compatible_ignoring_cv() and that
// adding the quals from `to` doesn't remove any quals from `from`.
// It is ok to add `const`/`volatile`, but not to change `_Atomic`.
// Handles arrays.
//
// Returns:
// --------
// True if qualification conversion is allowed.
//
// Arguments:
// ----------
// from: The type to convert from
// to: The type to convert to
//

static _Bool cc_plan9_base(CcQualType from, CcQualType to);
// cc_plan9_base()
// ---------------
// Like cc_qualification_convertible(), but allows converting to a Plan9
// anonymous field.
//
// Returns:
// --------
// True if the destination record is reachable with valid qualification.
//
// Arguments:
// ----------
// from: The type to convert from
// to: The type to convert to
//

static _Bool cc_pointer_pointee_convertible(CcQualType from, CcQualType to);
// cc_pointer_pointee_convertible()
// --------------------------------
// Call this function with the `pointee`s of two pointers.
// Given that these two types are being pointed to, is the pointer type
// allowed to be convertible.
// Allows compatible qualificatin, void pointer conversion, Plan9 conversion.
//
// Returns:
// --------
// True if the pointee conversion is allowed by this compiler.
//
// Arguments:
// ----------
// from: The pointed to type to convert from. This should be the pointee.
// to: The ponted to type to convert to. This should be the pointee.
//

static _Bool cc_any_payload_type(const CcTargetConfig* target, CcQualType t);
// cc_any_payload_type()
// ---------------------
// Checks whether an unconverted value fits the runtime _Any payload.
//
// Returns:
// --------
// True if the type has a supported payload representation on the target.
//
// Arguments:
// ----------
// target: Target configuration.
// t: Value type before any decay.
//

static _Bool cc_implicit_convertible(const CcTargetConfig* target,
        CcQualType from, CcQualType to);
// cc_implicit_convertible()
// -------------------------
// Checks type-based implicit conversion, including array/function decay, array
// copies, slices, and _Any.
// Note: this cannot recognize expression-dependent cases such as an integer
// null pointer contants.
//
// Returns:
// --------
// True if the types alone permit the conversion.
//
// Arguments:
// ----------
// target: Target configuration.
// from: Source type
// to: Destination type
//

static int cc_check_cast_types(const CcTargetConfig* target,
        CcQualType from, CcQualType to,
        const char* _Null_unspecified* _Nonnull failure);
// cc_check_cast_types()
// ---------------------
// Checks explicit cast admission.
//
// Returns:
// --------
// Zero on success and leaves `*failure` unchanged.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// target: Target configuration.
// from: Source type
// to: Destination type
// failure: on error, this is set to a relevant error message.
//

static _Bool cc_explicit_castable(const CcTargetConfig* target,
        CcQualType from, CcQualType to);
// cc_explicit_castable()
// ----------------------
// Boolean form of `cc_check_cast_types()`, without an error message.
//
// Returns:
// --------
// True if cc_check_cast_types would succeed.
//
// Arguments:
// ----------
// target: Target configuration.
// from: Source type
// to: Destination type
//

static _Bool cc_conversion_preserves_representation(CcQualType from, CcQualType to);
// cc_conversion_preserves_representation()
// ----------------------------------------
// Checks whether an allowed conversion preserves the value's representation.
//
// Returns:
// --------
// True if the conversion can reuse the source representation.
//
// Arguments:
// ----------
// from: Source type of an already validated conversion.
// to: Destination type of that conversion.
//
static _Bool cc_function_accepts_arity(const CcFunction* f, uint32_t count);
// cc_function_accepts_arity()
// ---------------------------
// Checks whether the function is callable with this number of arguments.
// Unprototyped functions accept any number.
// Prototyped functions with a fixed parameter list accept exactly the length
// of that list.
// Variadic functions require at least the number of fixed parameters.
//
// Returns:
// --------
// True if `count` is admitted.
//
// Arguments:
// ----------
// f: Function signature.
// count: Number of supplied arguments.
//

static _Bool cc_is_callable_with(const CcTargetConfig* target, CcQualType type,
        const CcQualType *_Null_unspecified args, uint32_t nargs);
// cc_is_callable_with()
// ---------------------
// Checks whether a callable type accepts the supplied argument types.
// Unprototyped and trailing variadic arguments use default argument promotions.
//
// Returns:
// --------
// True if the argument count and all argument types are accepted.
//
// Arguments:
// ----------
// target: Target configuration.
// type: Function or callable pointer type.
// args: Array of `nargs` argument types. May be `NULL` if `nargs` == 0.
// nargs: Number of arguments.
//

static _Bool cc_is_callable_through(const CcTargetConfig* target,
        CcQualType from, CcQualType through);
// cc_is_callable_through()
// ------------------------
// Checks whether a callable can be invoked through a function signature with
// matching ABI representations. If this is true, then it is defined by
// this compiler to be a valid signature to call the function.
// Calling a function through a different function pointer type is undefined
// by the standard.
// Variadic status and the fixed-parameter boundary must match. Aggregate
// types must match, and `_Bool` cannot substitute for another scalar type.
//
// Returns:
// --------
// True if both signatures have usable value types and matching call ABI.
//
// Arguments:
// ----------
// target: Target configuration.
// from: Function or callable pointer type.
// through: Function type of the proposed call signature.
//

static void cc_promote_bitfield_operand(const CcTargetConfig *target,
        CcQualType original, uint32_t width, CcQualType *out);
// cc_promote_bitfield_operand()
// ---------------------------------
// Promotes eligible integer bitfields to int and removes qualifiers from
// arithmetic types. Other types are unchanged.
// Does not perform ordinary integer promotions.
//
// Arguments:
// ----------
// target: Target configuration.
// original: Operand type.
// width: Bitfield width, or zero for a non-bitfield operand.
// out: Prepared-type output.
//

static int cc_integer_promotion(CcQualType t, CcQualType *out,
        const char *_Null_unspecified *_Nonnull failure);
// cc_integer_promotion()
// ----------------------------
// Applies integer promotion to an arithmetic operand.
// Enum types are replaced by their underlying type and outer
// qualifiers/alignment are removed.
// Floating and complex types pass through unchanged.
// Other types are rejected.
//
// Returns:
// --------
// Zero on success and writes the promoted type to `*out`.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// t: Operand type, with bitfield adjustment already applied if needed.
// out: Type output.
// failure: Diagnostic output.
//

static int cc_usual_arithmetic_conversions(const CcTargetConfig *target,
        CcQualType a, CcQualType b,
        CcQualType *out, const char *_Null_unspecified *_Nonnull failure);
// cc_usual_arithmetic_conversions()
// ---------------------------------
// Computes the resulting type of "the usual arithmetic conversions".
//
// Returns:
// --------
// Zero on success and writes the common type to `*out`.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// target: Target configuration.
// a, b: Arithmetic operand types.
// out: Common-type output.
// failure: Diagnostic output
//

static int cc_deref_type(CcQualType t, CcQualType* out,
        _Bool subscript, const char* _Null_unspecified* _Nonnull failure);
// cc_deref_type()
// ---------------
// Gets the result type of dereferencing or subscripting `t`.
// Preserves array element qualifiers.
//
// Returns:
// --------
// Zero on success and writes the result type to `*out`.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// t: Type being dereferenced or subscripted.
// out: Result type of dereferencing or subscripting.
// subscript: True for indexing, false for unary dereference.
// failure: Diagnostic output.
//

static _Bool cc_type_has_size(CcQualType type);
// cc_type_has_size()
// ------------------------
// Checks whether `type` has a defined storage size, possibly determined at
// runtime.
//
// Returns:
// --------
// True if `type` has a size.
//
// Arguments:
// ----------
// type: The type whose size is needed, not a pointer to it.
//

static int cc_check_incdec_operand(CcQualType type,
        const char* _Null_unspecified* _Nonnull failure);
// cc_check_incdec_operand()
// -------------------------
// Checks whether `type` supports increment and decrement (++ and --).
//
// Returns:
// --------
// Zero on success and leaves `*failure` unchanged.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// type: Operand type.
// failure: Diagnostic output.
//

static _Bool cc_is_scalar_operand(CcQualType type);
// cc_is_scalar_operand()
// ----------------------
// Checks whether `type` can be used as a condition or an operand of `!`, `&&`,
// or `||`.
//
// Returns:
// --------
// True if `type` is a scalar operand type.
//
// Arguments:
// ----------
// type: Operand type after any required decay.
//

static _Bool cc_string_array_element(const CcTargetConfig* target,
        CcQualType element);
// cc_string_array_element()
// -------------------------
// Checks whether an array of `element` can hold string-literal characters.
//
// Returns:
// --------
// True if `element` is a supported character type.
//
// Arguments:
// ----------
// target: Target configuration.
// element: Array element type.
//

static _Bool cc_constexpr_forbidden_qualifiers(CcQualType type);
// cc_constexpr_forbidden_qualifiers()
// -----------------------------------
// Checks whether qualification of an object or its subobjects prevents
// declaring it constexpr.
//
// Returns:
// --------
// True if a forbidden qualifier occurs in the stored object.
//
// Arguments:
// ----------
// type: Object type to inspect.
//

static _Bool cc_restrict_qualifiable(CcQualType type);
// cc_restrict_qualifiable()
// -----------------------------
// Checks whether restrict can be applied to `type`.
//
// Returns:
// --------
// True if restrict can be applied to `type`.
//
// Arguments:
// ----------
// type: Type to qualify.
//

static int cc_check_atomic_qualifier(CcQualType type,
        const char *_Null_unspecified *_Nonnull failure);
// cc_check_atomic_qualifier()
// ---------------------------
// Checks whether the `_Atomic` qualifier can be applied to `type`.
//
// Returns:
// --------
// Zero on success and leaves `*failure` unchanged.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// type: Type to qualify.
// failure: Diagnostic output.
//

static int cc_check_derived_type_rules(const CcTargetConfig *target,
        CcQualType type, const char *_Null_unspecified *_Nonnull failure);
// cc_check_derived_type_rules()
// -----------------------------
// Checks that pointer, array, slice, and function types are legally combined
// in a declaration, such as rejecting a function returning an array.
//
// Returns:
// --------
// Zero on success and leaves `*failure` unchanged.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// target: Target configuration.
// type: Constructed type to validate.
// failure: Diagnostic output.
//

static int cc_check_function_definition_types(const CcTargetConfig *target,
        const CcFunction *type, const char *_Null_unspecified *_Nonnull failure);
// cc_check_function_definition_types()
// ------------------------------------
// Checks that parameter and return types are complete enough to define the
// function.
//
// Returns:
// --------
// Zero on success and leaves `*failure` unchanged.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// target: Target configuration.
// type: Function signature being defined.
// failure: Diagnostic output.
//

static _Bool cc_string_array_elements_compatible(CcQualType target,
        CcQualType source);
// cc_string_array_elements_compatible()
// --------------------------------
// Checks whether a string literal's element type can initialize the array's
// element type.
//
// Returns:
// --------
// True if the element types permit string-literal initialization.
//
// Arguments:
// ----------
// target: Destination array element type
// source: String literal element type.
//

static int cc_vector_element_count(const CcTargetConfig* target,
        CcQualType base, uint32_t size, uint32_t* length,
        const char *_Null_unspecified *_Nonnull failure);
// cc_vector_element_count()
// -------------------------
// Computes how many elements of `base` fit in the vector_size attribute's byte
// size.
//
// Returns:
// --------
// Zero on success and writes the element count to `*length`.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// target: Target configuration.
// base: Proposed element type.
// size: Requested vector size in bytes.
// length: Element-count output.
// failure: Diagnostic output.
//

static int cc_check_member_type(CcQualType type, _Bool bitfield,
        const char *_Null_unspecified *_Nonnull failure);
// cc_check_member_type()
// ----------------------
// Checks whether `type` can be used for a struct or union member.
//
// Returns:
// --------
// Zero on success and leaves `*failure` unchanged.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// type: Member type.
// bitfield: Whether the member is a bitfield.
// failure: Diagnostic output.
//

static _Bool cc_type_is_initializable(CcQualType type);
// cc_type_is_initializable()
// ---------------------------
// Checks whether `type` can have an initializer.
//
// Returns:
// --------
// True if `type` can have an initializer.
//
// Arguments:
// ----------
// type: Destination type.
//

static int cc_check_atomic_specifier(CcQualType type,
        const char* _Null_unspecified* _Nonnull failure);
// cc_check_atomic_specifier()
// ---------------------------
// Checks whether `type` can be used in `_Atomic(type)`.
//
// Returns:
// --------
// Zero on success and leaves `*failure` unchanged.
// `_cc_syntax_error` on failure and writes a diagnostic string to `*failure`.
//
// Arguments:
// ----------
// type: Type inside `_Atomic(...)`, before adding atomic qualification.
// failure: Diagnostic output.
//

static _Bool cc_type_supports_atomic_rmw(CcQualType type);
// cc_type_supports_atomic_rmw()
// -----------------------------
// Checks the `_Atomic` operand restrictions for compound assignment and ++/--.
//
// Returns:
// --------
// True if `type` satisfies the atomic read-modify-write restrictions.
//
// Arguments:
// ----------
// type: Expression's object type, retaining atomic qualification.
//

static unsigned cc_array_bound_information(CcQualType type);
// cc_array_bound_information()
// ----------------------------
// Ranks array bounds by how much information they contribute to a composite
// type.
//
// Returns:
// --------
// Increasing ranks for unknown, unspecified VLA, specified VLA, and constant
// bounds.
//
// Arguments:
// ----------
// type: Array or VLA type.
//


static _Bool cc_record_members_match(CcStruct* a, CcStruct* b, _Bool same);
// cc_record_members_match()
// -------------------------
// Checks whether two record definitions have matching members and layout.
//
// Returns:
// --------
// True if members/layout match under the selected identity/compatibility mode.
//
// Arguments:
// ----------
// a, b: Record definitions to compare.
// same: True for member type identity, false for compatibility.
//

static _Bool cc_method_types_compatible(CcQualType a, CcQualType b,
        CcQualType owner_a, CcQualType owner_b);
// cc_method_types_compatible()
// ----------------------------
// Checks whether declarations of the same method have compatible signatures,
// treating their corresponding receiver types as compatible for this
// comparison.  These may be distinct type nodes from separate translation
// units.
//
// Returns:
// --------
// True if the signatures are compatible.
//
// Arguments:
// ----------
// a, b: Method signatures to compare.
// owner_a: Receiver type corresponding to `a`.
// owner_b: Receiver type corresponding to `b`.
//

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
