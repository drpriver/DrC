#ifndef C_CI_INTERP_LAYOUT_H
#define C_CI_INTERP_LAYOUT_H

#define CI_ASM_code 16
#define CI_ASM_code_ops 32
#define CI_ASM_code_count 24
#define CI_ASM_pc 8
#define CI_ASM_incoming 24
#define CI_ASM_parent 0
#define CI_ASM_frame_size 32
#define CI_ASM_stack_top 344
#define CI_ASM_stack_marks 360
#define CI_ASM_stack_mark_count 368
#define CI_ASM_stack_mark_capacity 376
#define CI_ASM_stack_boundary 384
#define CI_ASM_var_value 64
#define CI_ASM_native_func 176
#define CI_ASM_func_flags 56
#define CI_ASM_atom_data 8
#define CI_ASM_switch_count 0
#define CI_ASM_switch_data 8
#define CI_ASM_entry_size 24
#define CI_ASM_entry_target 16
#define CI_ASM_op_count 1157
#define CI_ASM_func_code 80
#define CI_ASM_func_hotswap 168
#define CI_ASM_plan_stack_size 48
#define CI_ASM_plan_stack_alignment 72

#define CI_ASM_HELPERS(X) \
    X(float16_add, ci_float16_add) \
    X(float16_sub, ci_float16_sub) \
    X(float16_mul, ci_float16_mul) \
    X(float16_div, ci_float16_div) \
    X(float16_eq, ci_float16_eq) \
    X(float16_lt, ci_float16_lt) \
    X(float16_le, ci_float16_le) \
    X(float80_add, ci_float80_add) \
    X(float80_sub, ci_float80_sub) \
    X(float80_mul, ci_float80_mul) \
    X(float80_div, ci_float80_div) \
    X(float80_eq, ci_float80_eq) \
    X(float80_lt, ci_float80_lt) \
    X(float80_le, ci_float80_le) \
    X(float128_add, ci_float128_add) \
    X(float128_sub, ci_float128_sub) \
    X(float128_mul, ci_float128_mul) \
    X(float128_div, ci_float128_div) \
    X(float128_eq, ci_float128_eq) \
    X(float128_lt, ci_float128_lt) \
    X(float128_le, ci_float128_le) \
    X(uint128_div, ci_uint128_div) \
    X(uint128_mod, ci_uint128_mod) \
    X(int128_div, ci_int128_div) \
    X(int128_mod, ci_int128_mod) \
    X(float16_from_int32, ci_float16_from_int32) \
    X(float16_from_uint32, ci_float16_from_uint32) \
    X(float16_from_int64, ci_float16_from_int64) \
    X(float16_from_uint64, ci_float16_from_uint64) \
    X(float16_from_uint128, ci_float16_from_uint128) \
    X(uint128_to_float, ci_uint128_to_float) \
    X(uint128_to_double, ci_uint128_to_double) \
    X(float80_from_uint128, ci_float80_from_uint128) \
    X(float128_from_uint128, ci_float128_from_uint128) \
    X(float16_to_int32, ci_float16_to_int32) \
    X(float16_to_uint32, ci_float16_to_uint32) \
    X(float16_to_int64, ci_float16_to_int64) \
    X(float16_to_uint64, ci_float16_to_uint64) \
    X(float16_to_uint128, ci_float16_to_uint128) \
    X(uint128_from_double, ci_uint128_from_double) \
    X(float80_to_uint128, ci_float80_to_uint128) \
    X(float128_to_uint128, ci_float128_to_uint128) \
    X(float16_to_float, ci_float16_to_float) \
    X(float16_from_float, ci_float16_from_float) \
    X(float16_to_double, ci_float16_to_double) \
    X(float16_to_float80, ci_float16_to_float80) \
    X(float16_to_quad, ci_float16_to_quad) \
    X(float80_from_float, ci_float80_from_float) \
    X(float128_from_float, ci_float128_from_float) \
    X(float16_from_double, ci_float16_from_double) \
    X(float80_from_double, ci_float80_from_double) \
    X(float128_from_double, ci_float128_from_double) \
    X(float16_from_float80, ci_float16_from_float80) \
    X(float80_to_float, ci_float80_to_float) \
    X(float80_to_double, ci_float80_to_double) \
    X(float128_from_float80, ci_float128_from_float80) \
    X(float16_from_quad, ci_float16_from_quad) \
    X(float128_to_float, ci_float128_to_float) \
    X(float128_to_double, ci_float128_to_double) \
    X(float80_from_float128, ci_float80_from_float128) \
    X(backtrace, ci_backtrace) \
    X(cyclecounter, performance_counter) \
    X(closure_lookup, ci_closure_map_get) \
    X(new_frame, ci_alloc_frame) \
    X(stack_save, ci_stack_save) \
    X(callback_stack, ci_prepare_callback) \
    X(ensure_var, ci_ensure_var_storage) \
    X(tls_address, ci_tls_address) \
    X(stack_alloc, ci_stack_alloc) \
    X(stack_zalloc, ci_stack_zalloc) \
    X(error, ci_error) \
    X(clear_cache, CI_CACHE_SERVICE) \
    X(memmove, memmove) \
    X(memset, memset) \
    X(crc, drp_crc32c)

#ifdef __ASSEMBLER__
.set ci_helper_index, 0
#define CI_HELPER_INDEX(name, function) name,
.irp helper, CI_ASM_HELPERS(CI_HELPER_INDEX) COUNT
    .set CI_ASM_HELPER_\helper, ci_helper_index
    .set ci_helper_index, ci_helper_index + 1
.endr
#undef CI_HELPER_INDEX
#else
enum {
#define CI_HELPER_INDEX(name, function) CI_ASM_HELPER_##name,
    CI_ASM_HELPERS(CI_HELPER_INDEX)
#undef CI_HELPER_INDEX
    CI_ASM_HELPER_COUNT,
};
#endif
#endif
