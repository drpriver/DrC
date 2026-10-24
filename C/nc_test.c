//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#if 1
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#define HEAVY_RECORDING
#endif
#include "../Drp/testing.h"
#include "../Drp/switch_macros.h"
#include "../Drp/Allocators/testing_allocator.h"
#include "../Drp/Allocators/allocator.h"
#include "../Drp/Allocators/mallocator.h"
#include "cc_errors.h"
#include "nc_native_call.h"
#include "asm/nc_capture_config.h"
#include "nc_va_list.h"
#include <stdarg.h>
#include <string.h>
#ifndef unreachable
#if defined(__GNUC__) || defined(__clang__)
#define unreachable() __builtin_unreachable()
#else
#define unreachable() __assume(0)
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct NcTestPair { long long a, b; } NcTestPair;
typedef struct NcTestFloats { double a, b; } NcTestFloats;
typedef struct NcTestLarge { long long a, b, c; } NcTestLarge;

typedef union NcTestResult {
    signed char schar;
    int integer;
    unsigned long long ull;
    long long ll;
    float f;
    double d;
    long double ld;
    int* pointer;
    NcTestPair pair;
    NcTestFloats floats;
    NcTestLarge large;
} NcTestResult;

static int nc_test_constant(void){ return 42; }
static signed char nc_test_signed(signed char a, short b){ return (signed char)(a - b); }
static unsigned long long nc_test_unsigned(unsigned long long a, unsigned long long b){ return a ^ b; }
static float nc_test_float(float a, float b){ return a + 2 * b; }
static long double nc_test_long_double(long double a, long double b){ return 3 * a + b; }
static int* nc_test_pointer(int* p, int offset){ return p + offset; }
static void nc_test_void(int* p, int value){ *p = value; }

static
long long
nc_test_gp_spill(long long a, long long b, long long c, long long d, long long e, long long f, long long g, long long h, long long i, long long j){
    return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h + 9*i + 10*j;
}

static
double
nc_test_fp_spill(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j){
    return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h + 9*i + 10*j;
}

static
double
nc_test_mixed(int a, double b, long long c, float d){
    return 4*a + 2*b + (double)c + d;
}

static
double
nc_test_variadic(int count, ...){
    va_list ap;
    va_start(ap, count);
    double result = 0;
    for(int i = 0; i < count; i++){
        long long integer = va_arg(ap, long long);
        double fraction = va_arg(ap, double);
        result += (i + 1) * (integer + fraction);
    }
    va_end(ap);
    return result;
}

static NcTestPair nc_test_pair(NcTestPair p){ return (NcTestPair){p.b + 1, p.a - 2}; }
static NcTestFloats nc_test_floats(NcTestFloats p){ return (NcTestFloats){2 * p.a, 3 * p.b}; }
static NcTestLarge nc_test_large(NcTestLarge p, long long delta){
    p.a += delta;
    p.b -= delta;
    p.c += delta;
    return p;
}

static
void
nc_test_record_types(CcStruct* pair, CcStruct* floats, CcStruct* large){
    *pair = (CcStruct){.kind = CC_STRUCT, .size = sizeof(NcTestPair), .alignment = _Alignof(NcTestPair)};
    *floats = (CcStruct){.kind = CC_STRUCT, .size = sizeof(NcTestFloats), .alignment = _Alignof(NcTestFloats)};
    *large = (CcStruct){.kind = CC_STRUCT, .size = sizeof(NcTestLarge), .alignment = _Alignof(NcTestLarge)};
    switch(CC_TARGET_NATIVE){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            pair->sysv = (CcSysvAggregate){.eightbyte_count = 2, .gp_count = 2};
            cc_sysv_set_class(&pair->sysv, 0, CC_SYSV_INTEGER);
            cc_sysv_set_class(&pair->sysv, 1, CC_SYSV_INTEGER);
            floats->sysv = (CcSysvAggregate){.eightbyte_count = 2, .fp_count = 2};
            cc_sysv_set_class(&floats->sysv, 0, CC_SYSV_SSE);
            cc_sysv_set_class(&floats->sysv, 1, CC_SYSV_SSE);
            large->sysv = (CcSysvAggregate){.is_memory_class = 1};
            break;
        case CC_TARGET_AARCH64_LINUX:
        case CC_TARGET_AARCH64_MACOS:
            pair->arm64 = (CcArm64Aggregate){.gp_count = 2, .copy_alignment = 8, .natural_alignment = 8};
            floats->arm64 = (CcArm64Aggregate){
                .hfa_type = CCBT_double, .hfa_count = 2, .member_size = 8,
                .copy_alignment = 8, .natural_alignment = 8,
            };
            cc_arm64_set_member_offset(&floats->arm64, 1, 8);
            large->arm64 = (CcArm64Aggregate){.is_indirect = 1, .copy_alignment = 8, .natural_alignment = 8};
            break;
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
            pair->win64 = floats->win64 = large->win64 = (CcWindowsAggregate){.is_indirect = 1};
            break;
        case CC_TARGET_TEST:
        case CC_TARGET_COUNT:
            unreachable();
        DRP_CASES_EXHAUSTED;
    }
}

TestFunction(test_native_calls){
    TESTBEGIN();
    CcStruct pair, floats, large;
    nc_test_record_types(&pair, &floats, &large);
    CcQualType pair_type = {.bits = (uintptr_t)&pair};
    CcQualType floats_type = {.bits = (uintptr_t)&floats};
    CcQualType large_type = {.bits = (uintptr_t)&large};
    CcPointer pointer = {.kind = CC_POINTER, .pointee = ccqt_basic(CCBT_int)};
    CcQualType pointer_type = {.bits = (uintptr_t)&pointer};
    int array[] = {3, 5, 7};
    int* array_pointer = array;
    int side_effect = 0;
    int* side_effect_pointer = &side_effect;
    NcTestLarge large_arg = {11, 22, 33};
    struct {
        const char* name;
        int line;
        void (*fn)(void);
        CcQualType result_type;
        unsigned count, fixed_count;
        _Bool variadic, long_double_result;
        struct { CcQualType type; const void* value; } args[20];
        size_t result_size;
        NcTestResult expected;
        struct {
            const void* _Nullable actual;
            const void* _Nullable expected;
            size_t size;
        } memory_check;
    } cases[] = {
#define NC_ARG(kind, ...) {ccqt_basic(CCBT_##kind), &(__VA_ARGS__)}
        {
            "no arguments", __LINE__,
            .fn = (void (*)(void))nc_test_constant,
            .result_type = ccqt_basic(CCBT_int),
            .result_size = sizeof(int),
            .expected.integer = 42,
        },
        {
            "signed narrow arguments and result", __LINE__,
            .fn = (void (*)(void))nc_test_signed,
            .result_type = ccqt_basic(CCBT_signed_char),
            .count = 2,
            .args = {NC_ARG(signed_char, (signed char){-100}), NC_ARG(short, (short){20})},
            .result_size = sizeof(signed char),
            .expected.schar = -120,
        },
        {
            "unsigned high bits", __LINE__,
            .fn = (void (*)(void))nc_test_unsigned,
            .result_type = ccqt_basic(CCBT_unsigned_long_long),
            .count = 2,
            .args = {
                NC_ARG(unsigned_long_long, (unsigned long long){0xffffffffffffffffULL}),
                NC_ARG(unsigned_long_long, (unsigned long long){0x7ffffffffffffffeULL})
            },
            .result_size = sizeof(unsigned long long),
            .expected.ull = 0x8000000000000001ULL,
        },
        {
            "float arguments and result", __LINE__,
            .fn = (void (*)(void))nc_test_float,
            .result_type = ccqt_basic(CCBT_float),
            .count = 2,
            .args = {
                NC_ARG(float, (float){1.5f}),
                NC_ARG(float, (float){-2.0f}),
            },
            .result_size = sizeof(float),
            .expected.f = -2.5f
        },
        {
            "long double arguments and result", __LINE__,
            .fn = (void (*)(void))nc_test_long_double,
            .result_type = ccqt_basic(CCBT_long_double),
            .count = 2,
            .long_double_result = 1,
            .args = {
                NC_ARG(long_double, (long double){1.25L}),
                NC_ARG(long_double, (long double){-0.5L}),
            },
            .result_size = sizeof(long double),
            .expected.ld = 3.25L,
        },
        {
            "pointer argument and result", __LINE__,
            .fn = (void (*)(void))nc_test_pointer,
            .result_type = pointer_type,
            .count = 2,
            .args = {{pointer_type, &array_pointer}, NC_ARG(int, (int){2})},
            .result_size = sizeof(int*),
            .expected.pointer = array + 2,
        },
        {
            "void return with side effect", __LINE__,
            .fn = (void (*)(void))nc_test_void,
            .result_type = ccqt_basic(CCBT_void),
            .count = 2,
            .args = {{pointer_type, &side_effect_pointer}, NC_ARG(int, (int){37})},
            .memory_check = {&side_effect, &(int){37}, sizeof side_effect},
        },
        {
            "integer register exhaustion", __LINE__,
            .fn = (void (*)(void))nc_test_gp_spill,
            .result_type = ccqt_basic(CCBT_long_long), .count = 10,
            .args = {
                NC_ARG(long_long, (long long){1}),
                NC_ARG(long_long, (long long){2}),
                NC_ARG(long_long, (long long){3}),
                NC_ARG(long_long, (long long){4}),
                NC_ARG(long_long, (long long){5}),
                NC_ARG(long_long, (long long){6}),
                NC_ARG(long_long, (long long){7}),
                NC_ARG(long_long, (long long){8}),
                NC_ARG(long_long, (long long){9}),
                NC_ARG(long_long, (long long){10}),
            },
            .result_size = sizeof(long long),
            .expected.ll = 385,
        },
        {
            "FP register exhaustion", __LINE__,
            .fn = (void (*)(void))nc_test_fp_spill,
            .result_type = ccqt_basic(CCBT_double),
            .count = 10,
            .args = {
                NC_ARG(double, (double){0.5}),
                NC_ARG(double, (double){1.5}),
                NC_ARG(double, (double){2.5}),
                NC_ARG(double, (double){3.5}),
                NC_ARG(double, (double){4.5}),
                NC_ARG(double, (double){5.5}),
                NC_ARG(double, (double){6.5}),
                NC_ARG(double, (double){7.5}),
                NC_ARG(double, (double){8.5}),
                NC_ARG(double, (double){9.5}),
            },
            .result_size = sizeof(double),
            .expected.d = 357.5,
        },
        {
            "mixed register classes", __LINE__,
            .fn = (void (*)(void))nc_test_mixed,
            .result_type = ccqt_basic(CCBT_double),
            .count = 4,
            .args = {
                NC_ARG(int, (int){-9}),
                NC_ARG(double, (double){1.5}),
                NC_ARG(long_long, (long long){10000000000LL}),
                NC_ARG(float, (float){0.25f}),
            },
            .result_size = sizeof(double),
            .expected.d = 9999999967.25,
        },
        {
            "variadic register exhaustion", __LINE__,
            .fn = (void (*)(void))nc_test_variadic,
            .result_type = ccqt_basic(CCBT_double),
            .count = 19,
            .fixed_count = 1,
            .variadic = 1,
            .args = {
                NC_ARG(int, (int){9}),
                NC_ARG(long_long, (long long){1}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){2}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){3}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){4}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){5}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){6}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){7}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){8}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){9}),
                NC_ARG(double, (double){0.5}),
            },
            .result_size = sizeof(double),
            .expected.d = 307.5,
        },
        {
            "integer aggregate", __LINE__,
            .fn = (void (*)(void))nc_test_pair,
            .result_type = pair_type,
            .count = 1,
            .args = {{pair_type, &(NcTestPair){7, 11}}},
            .result_size = sizeof(NcTestPair),
            .expected.pair = {12, 5},
        },
        {
            "floating-point aggregate", __LINE__,
            .fn = (void (*)(void))nc_test_floats,
            .result_type = floats_type,
            .count = 1,
            .args = {{floats_type, &(NcTestFloats){1.5, 2.5}}},
            .result_size = sizeof(NcTestFloats),
            .expected.floats = {3.0, 7.5},
        },
        {
            "large aggregate argument and indirect result", __LINE__,
            .fn = (void (*)(void))nc_test_large,
            .result_type = large_type,
            .count = 2,
            .args = {
                {large_type, &large_arg},
                NC_ARG(long_long, (long long){5}),
            },
            .result_size = sizeof(NcTestLarge),
            .expected.large = {16, 17, 38},
            .memory_check = {&large_arg, &(NcTestLarge){11, 22, 33}, sizeof large_arg},
        },
#undef NC_ARG
    };
    static int idx = 0;
    Allocator al = MALLOCATOR;
    for(size_t i = test_atomic_increment(&idx); i < countof(cases); i = test_atomic_increment(&idx)){
        size_t signature_size = sizeof(CcFunction) + cases[i].count * sizeof(CcQualType);
        CcFunction* signature = Allocator_zalloc(al, signature_size);
        TestAssert(signature != NULL);
        *signature = (CcFunction){
            .kind = CC_FUNCTION, .return_type = cases[i].result_type,
            .param_count = cases[i].count,
            .fixed_param_count = cases[i].variadic ? cases[i].fixed_count : cases[i].count,
            .is_variadic = cases[i].variadic,
        };
        for(unsigned arg = 0; arg < cases[i].count; arg++)
            signature->params[arg] = cases[i].args[arg].type;
        NcCallPlan* plan = NULL;
        int err = nc_plan_create(al, &cc_target_native, signature, &plan);
        Allocator_free(al, signature, signature_size);
        if(err)
            TestPrintf("%s:%d: %s: plan creation failed: %d\n", __FILE__, cases[i].line, cases[i].name, err);
        TestAssertEquals(int, err, 0);
        TestAssert(plan != NULL);
        size_t allocation_size = nc_state_size(plan) + _Alignof(NcCallState) - 1;
        void* allocation = Allocator_alloc(al, allocation_size);
        TestAssert(allocation != NULL);
        NcCallState* state = (void*)(((uintptr_t)allocation + _Alignof(NcCallState) - 1) & ~(uintptr_t)(_Alignof(NcCallState) - 1));
        NcTestResult result;
        memset(&result, 0xa5, sizeof result);
        nc_state_init(state, plan, &result);
        for(unsigned arg = 0; arg < cases[i].count; arg++)
            nc_arg_write(state, plan, arg, cases[i].args[arg].value);
        if(plan->target==CC_TARGET_X86_64_LINUX || plan->target==CC_TARGET_X86_64_MACOS)
            state->sysv.result_x87_count=cases[i].long_double_result?0:1;
        nc_invoke(plan, cases[i].fn, state);
        nc_result_read(state, plan, &result);
        _Bool equal = cases[i].long_double_result
            ? result.ld == cases[i].expected.ld
            : memcmp(&result, &cases[i].expected, cases[i].result_size) == 0;
        if(!equal)
            TestPrintf("%s:%d %s: incorrect native result\n", __FILE__, cases[i].line, cases[i].name);
        TestExpect(_Bool, equal, ==, 1);
        if(cases[i].memory_check.size){
            _Bool memory_equal = memcmp(cases[i].memory_check.actual, cases[i].memory_check.expected, cases[i].memory_check.size) == 0;
            if(!memory_equal)
                TestPrintf("%s:%d: %s: incorrect memory after native call\n", __FILE__, cases[i].line, cases[i].name);
            TestExpect(_Bool, memory_equal, ==, 1);
        }
        Allocator_free(al, allocation, allocation_size);
        nc_plan_destroy(al, plan);
    }
    TESTEND();
}


typedef struct NcTestClosureCase NcTestClosureCase;
struct NcTestClosureCase {
    const char* name;
    int line;
    void (*call)(void (*fn)(void), const NcTestClosureCase*, NcTestResult*);
    CcQualType result_type;
    unsigned count, fixed_count;
    _Bool variadic, long_double_result, store_int;
    struct { CcQualType type; const void* value; size_t size; } args[20];
    size_t result_size;
    NcTestResult expected;
    struct {
        const void* _Nullable actual;
        const void* _Nullable expected;
        size_t size;
    } memory_check;
    unsigned copies;
    int expected_error;
};

typedef struct NcTestClosureContext NcTestClosureContext;
struct NcTestClosureContext {
    const NcTestClosureCase* test;
    NcTestResult returned;
    unsigned calls;
    unsigned bad_arguments;
};

// Typed C calls supply the independent native ABI side of these tests.
static
void
nc_test_call_constant(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef int Fn(void);
    (void)tc;
    result->integer = ((Fn*)entry)();
}

static
void
nc_test_call_signed(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef signed char Fn(signed char, short);
    result->schar = ((Fn*)entry)(*(signed char const*)tc->args[0].value,
        *(short const*)tc->args[1].value);
}

static
void
nc_test_call_unsigned(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef unsigned long long Fn(unsigned long long, unsigned long long);
    result->ull = ((Fn*)entry)(*(unsigned long long const*)tc->args[0].value,
        *(unsigned long long const*)tc->args[1].value);
}

static
void
nc_test_call_float(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef float Fn(float, float);
    result->f = ((Fn*)entry)(*(float const*)tc->args[0].value,
        *(float const*)tc->args[1].value);
}

static
void
nc_test_call_long_double(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef long double Fn(long double, long double);
    result->ld = ((Fn*)entry)(*(long double const*)tc->args[0].value,
        *(long double const*)tc->args[1].value);
}

static
void
nc_test_call_pointer(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef int* Fn(int*, int);
    result->pointer = ((Fn*)entry)(*(int* const*)tc->args[0].value,
        *(int const*)tc->args[1].value);
}

static
void
nc_test_call_void(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef void Fn(int*, int);
    (void)result;
    ((Fn*)entry)(*(int* const*)tc->args[0].value,
        *(int const*)tc->args[1].value);
}

static
void
nc_test_call_gp_spill(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef long long Fn(long long, long long, long long, long long, long long, long long, long long, long long, long long, long long);
    result->ll = ((Fn*)entry)(*(long long const*)tc->args[0].value,
        *(long long const*)tc->args[1].value,
        *(long long const*)tc->args[2].value,
        *(long long const*)tc->args[3].value,
        *(long long const*)tc->args[4].value,
        *(long long const*)tc->args[5].value,
        *(long long const*)tc->args[6].value,
        *(long long const*)tc->args[7].value,
        *(long long const*)tc->args[8].value,
        *(long long const*)tc->args[9].value);
}

static
void
nc_test_call_fp_spill(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef double Fn(double, double, double, double, double, double, double, double, double, double);
    result->d = ((Fn*)entry)(*(double const*)tc->args[0].value,
        *(double const*)tc->args[1].value,
        *(double const*)tc->args[2].value,
        *(double const*)tc->args[3].value,
        *(double const*)tc->args[4].value,
        *(double const*)tc->args[5].value,
        *(double const*)tc->args[6].value,
        *(double const*)tc->args[7].value,
        *(double const*)tc->args[8].value,
        *(double const*)tc->args[9].value);
}

static
void
nc_test_call_mixed(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef double Fn(int, double, long long, float);
    result->d = ((Fn*)entry)(*(int const*)tc->args[0].value,
        *(double const*)tc->args[1].value,
        *(long long const*)tc->args[2].value,
        *(float const*)tc->args[3].value);
}

static
void
nc_test_call_variadic(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef double Fn(int, ...);
    result->d = ((Fn*)entry)(*(int const*)tc->args[0].value,
        *(long long const*)tc->args[1].value,
        *(double const*)tc->args[2].value,
        *(long long const*)tc->args[3].value,
        *(double const*)tc->args[4].value,
        *(long long const*)tc->args[5].value,
        *(double const*)tc->args[6].value,
        *(long long const*)tc->args[7].value,
        *(double const*)tc->args[8].value,
        *(long long const*)tc->args[9].value,
        *(double const*)tc->args[10].value,
        *(long long const*)tc->args[11].value,
        *(double const*)tc->args[12].value,
        *(long long const*)tc->args[13].value,
        *(double const*)tc->args[14].value,
        *(long long const*)tc->args[15].value,
        *(double const*)tc->args[16].value,
        *(long long const*)tc->args[17].value,
        *(double const*)tc->args[18].value);
}

static
void
nc_test_call_pair(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef NcTestPair Fn(NcTestPair);
    result->pair = ((Fn*)entry)(*(NcTestPair const*)tc->args[0].value);
}

static
void
nc_test_call_floats(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef NcTestFloats Fn(NcTestFloats);
    result->floats = ((Fn*)entry)(*(NcTestFloats const*)tc->args[0].value);
}

static
void
nc_test_call_large(void (*entry)(void), const NcTestClosureCase* tc, NcTestResult* result){
    typedef NcTestLarge Fn(NcTestLarge, long long);
    result->large = ((Fn*)entry)(*(NcTestLarge const*)tc->args[0].value,
        *(long long const*)tc->args[1].value);
}

static
void
nc_test_closure_callback(const NcCallPlan* plan, NcCallState* state, void* userdata){
    NcTestClosureContext* context = userdata;
    const NcTestClosureCase* tc = context->test;
    context->calls++;
    unsigned named = tc->variadic ? tc->fixed_count : tc->count;
    for(unsigned i = 0; i < named; i++){
        NcTestResult actual;
        memset(&actual, 0xa5, sizeof actual);
        nc_arg_read(state, plan, i, &actual);
        _Bool equal = ccqt_bt_eq(tc->args[i].type, CCBT_long_double)
            ? actual.ld == *(const long double*)tc->args[i].value
            : memcmp(&actual, tc->args[i].value, tc->args[i].size) == 0;
        if(!equal) context->bad_arguments |= 1u << i;
    }
    if(tc->variadic){
        // Read the captured arguments with the host compiler's va_arg, not
        // with the same argument-decoding helper under test.
        va_list ap;
        nc_va_start(state, plan, &ap);
        for(unsigned i = named; i < tc->count; i++){
            NcTestResult actual;
            if(ccqt_bt_eq(tc->args[i].type, CCBT_long_long))
                actual.ll = va_arg(ap, long long);
            else
                actual.d = va_arg(ap, double);
            if(memcmp(&actual, tc->args[i].value, tc->args[i].size))
                context->bad_arguments |= 1u << i;
        }
        va_end(ap);
    }
    if(tc->store_int){
        int* dest;
        int value;
        nc_arg_read(state, plan, 0, &dest);
        nc_arg_read(state, plan, 1, &value);
        *dest = value;
    }
    nc_result_write(state, plan, &context->returned);
    if(plan->target==CC_TARGET_X86_64_LINUX || plan->target==CC_TARGET_X86_64_MACOS){
        unsigned x87=0;
        for(unsigned i=0;i<plan->result.count;i++) x87+=plan->result.pieces[i].bank==NC_ABI_X87;
        state->sysv.result_x87_count=x87?0:1;
    }
}

TestFunction(test_native_closures){
    TESTBEGIN();
    CcStruct pair, floats, large;
    nc_test_record_types(&pair, &floats, &large);
    CcQualType pair_type = {.bits = (uintptr_t)&pair};
    CcQualType floats_type = {.bits = (uintptr_t)&floats};
    CcQualType large_type = {.bits = (uintptr_t)&large};
    CcPointer pointer = {.kind = CC_POINTER, .pointee = ccqt_basic(CCBT_int)};
    CcQualType pointer_type = {.bits = (uintptr_t)&pointer};
    int array[] = {3, 5, 7};
    int* array_pointer = array;
    int side_effect = 0;
    int* side_effect_pointer = &side_effect;
    NcTestLarge large_arg = {11, 22, 33};
    NcTestClosureCase cases[] = {
#define NC_ARG(kind, ...) {ccqt_basic(CCBT_##kind), &(__VA_ARGS__), sizeof(__VA_ARGS__)}
        {
            "no arguments", __LINE__,
            .call = nc_test_call_constant,
            .result_type = ccqt_basic(CCBT_int),
            .result_size = sizeof(int),
            .expected.integer = 42,
            .copies = 42,
        },
        {
            "many live closures", __LINE__,
            .call = nc_test_call_constant,
            .result_type = ccqt_basic(CCBT_int),
            .result_size = sizeof(int),
            .expected.integer = 42,
            .copies = 130,
            // .copies = NC_CAPTURE_SLOT_COUNT + 2,
        },
        {
            "signed narrow arguments and result", __LINE__,
            .call = nc_test_call_signed,
            .result_type = ccqt_basic(CCBT_signed_char),
            .count = 2,
            .args = {
                NC_ARG(signed_char, (signed char){-100}),
                NC_ARG(short, (short){20})
            },
            .result_size = sizeof(signed char),
            .expected.schar = -120,
        },
        {
            "unsigned high bits", __LINE__,
            .call = nc_test_call_unsigned,
            .result_type = ccqt_basic(CCBT_unsigned_long_long),
            .count = 2,
            .args = {
                NC_ARG(unsigned_long_long, (unsigned long long){0xffffffffffffffffULL}),
                NC_ARG(unsigned_long_long, (unsigned long long){0x7ffffffffffffffeULL})
            },
            .result_size = sizeof(unsigned long long),
            .expected.ull = 0x8000000000000001ULL,
        },
        {
            "float arguments and result", __LINE__,
            .call = nc_test_call_float,
            .result_type = ccqt_basic(CCBT_float),
            .count = 2,
            .args = {
                NC_ARG(float, (float){1.5f}),
                NC_ARG(float, (float){-2.0f}),
            },
            .result_size = sizeof(float),
            .expected.f = -2.5f
        },
        {
            "long double arguments and result", __LINE__,
            .call = nc_test_call_long_double,
            .result_type = ccqt_basic(CCBT_long_double),
            .count = 2,
            .long_double_result = 1,
            .args = {
                NC_ARG(long_double, (long double){1.25L}),
                NC_ARG(long_double, (long double){-0.5L}),
            },
            .result_size = sizeof(long double),
            .expected.ld = 3.25L,
        },
        {
            "pointer argument and result", __LINE__,
            .call = nc_test_call_pointer,
            .result_type = pointer_type,
            .count = 2,
            .args = {
                {pointer_type, &array_pointer, sizeof array_pointer},
                NC_ARG(int, (int){2})
            },
            .result_size = sizeof(int*),
            .expected.pointer = array + 2,
        },
        {
            "void return with side effect", __LINE__,
            .call = nc_test_call_void,
            .store_int = 1,
            .result_type = ccqt_basic(CCBT_void),
            .count = 2,
            .args = {
                {pointer_type, &side_effect_pointer, sizeof side_effect_pointer},
                NC_ARG(int, (int){37})
            },
            .memory_check = {&side_effect, &(int){37}, sizeof side_effect},
        },
        {
            "integer register exhaustion", __LINE__,
            .call = nc_test_call_gp_spill,
            .result_type = ccqt_basic(CCBT_long_long), .count = 10,
            .args = {
                NC_ARG(long_long, (long long){1}),
                NC_ARG(long_long, (long long){2}),
                NC_ARG(long_long, (long long){3}),
                NC_ARG(long_long, (long long){4}),
                NC_ARG(long_long, (long long){5}),
                NC_ARG(long_long, (long long){6}),
                NC_ARG(long_long, (long long){7}),
                NC_ARG(long_long, (long long){8}),
                NC_ARG(long_long, (long long){9}),
                NC_ARG(long_long, (long long){10}),
            },
            .result_size = sizeof(long long),
            .expected.ll = 385,
        },
        {
            "FP register exhaustion", __LINE__,
            .call = nc_test_call_fp_spill,
            .result_type = ccqt_basic(CCBT_double),
            .count = 10,
            .args = {
                NC_ARG(double, (double){0.5}),
                NC_ARG(double, (double){1.5}),
                NC_ARG(double, (double){2.5}),
                NC_ARG(double, (double){3.5}),
                NC_ARG(double, (double){4.5}),
                NC_ARG(double, (double){5.5}),
                NC_ARG(double, (double){6.5}),
                NC_ARG(double, (double){7.5}),
                NC_ARG(double, (double){8.5}),
                NC_ARG(double, (double){9.5}),
            },
            .result_size = sizeof(double),
            .expected.d = 357.5,
        },
        {
            "mixed register classes", __LINE__,
            .call = nc_test_call_mixed,
            .result_type = ccqt_basic(CCBT_double),
            .count = 4,
            .args = {
                NC_ARG(int, (int){-9}),
                NC_ARG(double, (double){1.5}),
                NC_ARG(long_long, (long long){10000000000LL}),
                NC_ARG(float, (float){0.25f}),
            },
            .result_size = sizeof(double),
            .expected.d = 9999999967.25,
        },
        {
            "variadic register exhaustion", __LINE__,
            .call = nc_test_call_variadic,
            .result_type = ccqt_basic(CCBT_double),
            .count = 19,
            .fixed_count = 1,
            .variadic = 1,
            .args = {
                NC_ARG(int, (int){9}),
                NC_ARG(long_long, (long long){1}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){2}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){3}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){4}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){5}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){6}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){7}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){8}),
                NC_ARG(double, (double){0.5}),
                NC_ARG(long_long, (long long){9}),
                NC_ARG(double, (double){0.5}),
            },
            .result_size = sizeof(double),
            .expected.d = 307.5,
        },
        {
            "integer aggregate", __LINE__,
            .call = nc_test_call_pair,
            .result_type = pair_type,
            .count = 1,
            .args = {{pair_type, &(NcTestPair){7, 11}, sizeof(NcTestPair)}},
            .result_size = sizeof(NcTestPair),
            .expected.pair = {12, 5},
        },
        {
            "floating-point aggregate", __LINE__,
            .call = nc_test_call_floats,
            .result_type = floats_type,
            .count = 1,
            .args = {{floats_type, &(NcTestFloats){1.5, 2.5}, sizeof(NcTestFloats)}},
            .result_size = sizeof(NcTestFloats),
            .expected.floats = {3.0, 7.5},
        },
        {
            "large aggregate argument and indirect result", __LINE__,
            .call = nc_test_call_large,
            .result_type = large_type,
            .count = 2,
            .args = {
                {large_type, &large_arg, sizeof large_arg},
                NC_ARG(long_long, (long long){5}),
            },
            .result_size = sizeof(NcTestLarge),
            .expected.large = {16, 17, 38},
            .memory_check = {&large_arg, &(NcTestLarge){11, 22, 33}, sizeof large_arg},
        },
        {
            "unsupported signature", __LINE__,
            .call = nc_test_call_constant,
            .result_type = ccqt_basic(CCBT_INVALID),
            .expected_error = _cc_unimplemented_error,
        },
#undef NC_ARG
    };
    static int idx = 0;
    Allocator al = MALLOCATOR;
    for(size_t i = test_atomic_increment(&idx); i < countof(cases); i = test_atomic_increment(&idx)){
        const NcTestClosureCase* tc = &cases[i];
        unsigned named = tc->variadic ? tc->fixed_count : tc->count;
        size_t signature_size = sizeof(CcFunction) + named * sizeof(CcQualType);
        CcFunction* signature = Allocator_zalloc(al, signature_size);
        TestAssert(signature != NULL);
        *signature = (CcFunction){
            .kind = CC_FUNCTION,
            .return_type = tc->result_type,
            .param_count = named,
            .fixed_param_count = named,
            .is_variadic = tc->variadic,
        };
        for(unsigned arg = 0; arg < named; arg++)
            signature->params[arg] = tc->args[arg].type;
        unsigned copies = tc->copies ? tc->copies : 1;
        NcClosure** closures = Allocator_zalloc(al, copies * sizeof *closures);
        NcTestClosureContext* contexts = Allocator_zalloc(al, copies * sizeof *contexts);
        TestAssert(closures != NULL);
        TestAssert(contexts != NULL);
        for(unsigned j = 0; j < copies; j++){
            contexts[j].test = tc;
            contexts[j].returned = tc->expected;
            if(tc->copies)
                contexts[j].returned.integer += (int)j;
            int err = nc_create_closure(al, signature, nc_test_closure_callback, &contexts[j], &closures[j]);
            if(err != tc->expected_error)
                TestPrintf("%s:%d: %s: closure creation returned %d\n", __FILE__, tc->line, tc->name, err);
            TestExpect(int, err, ==, tc->expected_error);
            TestExpect(_Bool, closures[j] != NULL, ==, err == 0);
        }
        Allocator_free(al, signature, signature_size);
        for(unsigned j = 0; j < copies; j++){
            if(!closures[j]) continue;
            if(!tc->expected_error){
                NcTestResult result;
                memset(&result, 0xa5, sizeof result);
                tc->call(nc_closure_get_fn(closures[j]), tc, &result);
                if(contexts[j].bad_arguments)
                    TestPrintf("%s:%d: %s: incorrect captured arguments (mask %x)\n", __FILE__, tc->line, tc->name, contexts[j].bad_arguments);
                TestExpect(unsigned, contexts[j].calls, ==, 1);
                TestExpect(unsigned, contexts[j].bad_arguments, ==, 0);
                _Bool equal = tc->long_double_result ?
                    result.ld == contexts[j].returned.ld
                    : memcmp(&result, &contexts[j].returned, tc->result_size) == 0;
                if(!equal)
                    TestPrintf("%s:%d: %s: incorrect closure result\n", __FILE__, tc->line, tc->name);
                TestExpect(_Bool, equal, ==, 1);
                if(tc->memory_check.size){
                    _Bool equal_memory = memcmp(tc->memory_check.actual, tc->memory_check.expected, tc->memory_check.size) == 0;
                    if(!equal_memory)
                        TestPrintf("%s:%d: %s: incorrect memory after closure call\n", __FILE__, tc->line, tc->name);
                    TestExpect(_Bool, equal_memory, ==, 1);
                }
            }
            nc_destroy_closure(al, closures[j]);
        }
        Allocator_free(al, contexts, copies * sizeof *contexts);
        Allocator_free(al, closures, copies * sizeof *closures);
    }
    TESTEND();
}

int
main(int argc, char** argv){
#ifdef USE_TESTING_ALLOCATOR
    testing_allocator_init();
#endif
    RegisterTestFlags(test_native_calls, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_native_closures, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    int err = test_main(argc, argv, NULL);
#ifdef USE_TESTING_ALLOCATOR
    testing_assert_all_freed();
#endif
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "nc_native_call.c"
#include "../Drp/Allocators/allocator.c"
