//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Quick test for native_call.
// Build: cc -g -lffi -o native_call_test C/native_call_test.c
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>
#include "native_call.h"
#include "native_call.c"

static int failures = 0;

#define CHECK(cond, ...) do { \
    if(!(cond)){ \
        printf("FAIL: " __VA_ARGS__); \
        printf("\n"); \
        failures++; \
    } \
} while(0)

// Helper to build a CcFunction on the stack with N params.
// Uses the GNU extension of a flexible array member not at the end.
#define MAKE_FUNC(name, ret, nparams, variadic, ...) \
    struct { CcFunction f; CcQualType p[nparams ? nparams : 1]; } name##_storage = { \
        .f = { ._bits = CC_FUNCTION | ((variadic) << 4), \
               .return_type = (ret), .param_count = (nparams) }, \
        .p = { __VA_ARGS__ } \
    }; \
    CcFunction* name = &name##_storage.f

#define QT_BASIC(k) ((CcQualType){.basic = {._quals = 0, .kind = (k)}})
#define QT_PTR(ptr_var) ((CcQualType){.bits = (uintptr_t)(ptr_var)})

static void
test_strlen(void){
    // size_t strlen(const char*)
    static CcPointer char_ptr = {._bits = CC_POINTER, .pointee = {.basic = {.kind = CCBT_char}}};
    MAKE_FUNC(ft, QT_BASIC(CCBT_unsigned_long), 1, 0,
        QT_PTR(&char_ptr)
    );
    void (*fn)(void) = (void(*)(void))dlsym(RTLD_DEFAULT, "strlen");
    CHECK(fn, "dlsym strlen failed");

    const char* str = "hello";
    void* args[1] = { &str };
    unsigned long result = 0;
    int err = native_call(ft, fn, args, 1, NULL, &result);
    CHECK(!err, "native_call for strlen failed: %d", err);
    CHECK(result == 5, "strlen(\"hello\") = %lu, expected 5", result);
}

static void
test_sqrt(void){
    // double sqrt(double)
    MAKE_FUNC(ft, QT_BASIC(CCBT_double), 1, 0,
        QT_BASIC(CCBT_double)
    );
    void (*fn)(void) = (void(*)(void))dlsym(RTLD_DEFAULT, "sqrt");
    CHECK(fn, "dlsym sqrt failed");

    double arg = 2.0;
    void* args[1] = { &arg };
    double result = 0;
    int err = native_call(ft, fn, args, 1, NULL, &result);
    CHECK(!err, "native_call for sqrt failed: %d", err);
    CHECK(fabs(result - 1.41421356) < 0.0001, "sqrt(2.0) = %f, expected ~1.414", result);
}

static void
test_printf_variadic(void){
    // int printf(const char*, ...)
    // Call with: printf("test %d %f\n", 42, 3.14)
    static CcPointer char_ptr = {._bits = CC_POINTER, .pointee = {.basic = {.kind = CCBT_char}}};
    MAKE_FUNC(ft, QT_BASIC(CCBT_int), 1, 1,
        QT_PTR(&char_ptr)
    );
    void (*fn)(void) = (void(*)(void))dlsym(RTLD_DEFAULT, "printf");
    CHECK(fn, "dlsym printf failed");

    const char* fmt = "test %d %f\n";
    int ival = 42;
    double dval = 3.14;
    void* args[3] = { &fmt, &ival, &dval };
    CcQualType vararg_types[2] = { QT_BASIC(CCBT_int), QT_BASIC(CCBT_double) };
    int result = 0;
    int err = native_call(ft, fn, args, 3, vararg_types, &result);
    CHECK(!err, "native_call for printf failed: %d", err);
    CHECK(result > 0, "printf returned %d, expected > 0", result);
}

static void
test_div_struct_return(void){
    // div_t div(int, int)
    static CcField div_fields[2] = {
        {.type = {.basic = {._quals = 0, .kind = CCBT_int}}, .name = 0, .offset = 0},
        {.type = {.basic = {._quals = 0, .kind = CCBT_int}}, .name = 0, .offset = 4},
    };
    static CcStruct div_struct = {
        ._bits = CC_STRUCT,
        .name = 0,
        .size = 8,
        .alignment = 4,
        .field_count = 2,
        .fields = div_fields,
    };
    CcQualType div_qt = QT_PTR(&div_struct);

    MAKE_FUNC(ft, div_qt, 2, 0,
        QT_BASIC(CCBT_int), QT_BASIC(CCBT_int)
    );
    void (*fn)(void) = (void(*)(void))dlsym(RTLD_DEFAULT, "div");
    CHECK(fn, "dlsym div failed");

    int a = 10, b = 3;
    void* args[2] = { &a, &b };
    div_t result;
    int err = native_call(ft, fn, args, 2, NULL, &result);
    CHECK(!err, "native_call for div failed: %d", err);
    CHECK(result.quot == 3, "div(10,3).quot = %d, expected 3", result.quot);
    CHECK(result.rem == 1, "div(10,3).rem = %d, expected 1", result.rem);
}

// Simulate an interpreted comparator: sort ints descending.
static void
descending_cmp_callback(void* rvalue, void** args, void* userdata){
    (void)userdata;
    const int* a = *(const int**)args[0];
    const int* b = *(const int**)args[1];
    *(int*)rvalue = (*b > *a) - (*b < *a);
}

static void
test_qsort_closure(void){
    // int (*compar)(const void*, const void*)
    static CcPointer void_ptr = {._bits = CC_POINTER, .pointee = {.basic = {.kind = CCBT_void}}};
    MAKE_FUNC(ft, QT_BASIC(CCBT_int), 2, 0,
        QT_PTR(&void_ptr), QT_PTR(&void_ptr)
    );
    NativeClosure* closure = native_closure_create(ft, descending_cmp_callback, NULL);
    CHECK(closure, "native_closure_create failed");
    if(!closure) return;

    int arr[] = {3, 1, 4, 1, 5, 9, 2, 6};
    int n = sizeof arr / sizeof arr[0];
    int (*compar)(const void*, const void*) = (int(*)(const void*, const void*))native_closure_fn(closure);
    qsort(arr, n, sizeof(int), compar);

    CHECK(arr[0] == 9, "arr[0] = %d, expected 9", arr[0]);
    CHECK(arr[1] == 6, "arr[1] = %d, expected 6", arr[1]);
    CHECK(arr[n-1] == 1, "arr[%d] = %d, expected 1", n-1, arr[n-1]);

    native_closure_destroy(closure);
}

int main(void){
    test_strlen();
    test_sqrt();
    test_printf_variadic();
    test_div_struct_return();
    test_qsort_closure();

    if(failures)
        printf("%d test(s) FAILED\n", failures);
    else
        printf("All tests passed.\n");
    return failures ? 1 : 0;
}
