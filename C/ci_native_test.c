//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#if 1
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#define HEAVY_RECORDING
#endif
#define CI_THREAD_UNSAFE_ALLOCATOR
#include "../Drp/compiler_warnings.h"
#include "../Drp/Allocators/testing_allocator.h"
#include "../Drp/testing.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/env.h"
#include "../Drp/atom_table.h"
#include "../Drp/stdlogger.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/file_cache.h"
#include "../Drp/msb_logger.h"
#include "cc_parser.h"
#include "cc_target.h"
#include "ci_interp.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


// Integer types
static int test_add(int a, int b){ return a + b; }
static int test_sub(int a, int b){ return a - b; }
static long long test_add_ll(long long a, long long b){ return a + b; }
static char test_add_char(char a, char b){ return (char)(a + b); }
static short test_add_short(short a, short b){ return (short)(a + b); }
static unsigned test_add_unsigned(unsigned a, unsigned b){ return a + b; }
static int test_mixed_widths(char a, long long b, short c){ return (int)(a + b + c); }
static int test_char_sign(char c){ return (int)c; }

// Floating point
static double test_dadd(double a, double b){ return a + b; }
static float test_fadd(float a, float b){ return a + b; }
static double test_mixed_int_double(int a, double b, int c, double d){ return a + b + c + d; }
static float test_float_identity(float x){ return x; }
static double test_double_identity(double x){ return x; }
static int test_double_to_int(double x){ return (int)x; }
static double test_int_to_double(int x){ return (double)x; }

// Many arguments (register spill)
static int test_many_ints(int a, int b, int c, int d, int e, int f, int g, int h){
    return a + b + c + d + e + f + g + h;
}
static double test_many_doubles(double a, double b, double c, double d, double e, double f, double g, double h, double i){
    return a + b + c + d + e + f + g + h + i;
}
static int test_many_mixed(int a, double b, int c, double d, int e, double f, int g, double h, int i, double j){
    return (int)(a + b + c + d + e + f + g + h + i + j);
}

// Structs
struct SmallStruct { int x; };
struct TwoIntStruct { int x, y; };
struct IntFloatStruct { int x; float y; };
struct LargeStruct { int a, b, c, d, e; };
struct TwoFloatStruct { float x, y; };
struct TwoDoubleStruct { double x, y; };
struct FourFloatStruct { float a, b, c, d; };
struct FiveFloatStruct { float a, b, c, d, e; };
struct IntDoubleStruct { int x; double y; };
struct FloatFloatIntStruct { float a, b; int c; };

static int test_small_struct(struct SmallStruct s){ return s.x; }
static int test_two_int_struct(struct TwoIntStruct s){ return s.x + s.y; }
static int test_int_float_struct(struct IntFloatStruct s){ return s.x + (int)s.y; }
static int test_large_struct(struct LargeStruct s){ return s.a + s.b + s.c + s.d + s.e; }

static struct SmallStruct test_return_small_struct(int x){ return (struct SmallStruct){x}; }
static struct TwoIntStruct test_return_two_int_struct(int x, int y){ return (struct TwoIntStruct){x, y}; }
static struct LargeStruct test_return_large_struct(int v){
    return (struct LargeStruct){v, v+1, v+2, v+3, v+4};
}

// HFA
static int test_two_float_struct(struct TwoFloatStruct s){ return (int)(s.x + s.y); }
static int test_two_double_struct(struct TwoDoubleStruct s){ return (int)(s.x + s.y); }
static int test_four_float_struct(struct FourFloatStruct s){ return (int)(s.a + s.b + s.c + s.d); }
static int test_five_float_struct(struct FiveFloatStruct s){ return (int)(s.a + s.b + s.c + s.d + s.e); }
static struct TwoFloatStruct test_return_two_float_struct(float a, float b){
    return (struct TwoFloatStruct){a, b};
}
static struct FourFloatStruct test_return_four_float_struct(float a, float b, float c, float d){
    return (struct FourFloatStruct){a, b, c, d};
}

// Eightbyte splitting
static int test_int_double_struct(struct IntDoubleStruct s){ return s.x + (int)s.y; }
static int test_float_float_int_struct(struct FloatFloatIntStruct s){ return (int)(s.a + s.b) + s.c; }

// Void return, pointer args, bool
static void test_write_ptr(int* out, int val){ *out = val; }
static int test_deref(int* p){ return *p; }
static int* test_return_ptr(int* p){ return p; }
static _Bool test_is_positive(int x){ return x > 0; }

// Bitfields
struct BF_Basic { unsigned a : 3; unsigned b : 5; unsigned c : 8; };
struct BF_Signed { int a : 4; int b : 4; };
struct BF_CrossByte { unsigned a : 7; unsigned b : 10; unsigned c : 15; };
struct BF_Mixed { int x; unsigned a : 4; unsigned b : 4; float y; };
struct BF_MultiUnit { unsigned a : 16; unsigned b : 16; unsigned c : 16; unsigned d : 16; };

static int test_bf_basic(struct BF_Basic s){ return (int)(s.a + s.b + s.c); }
static struct BF_Basic test_bf_basic_return(unsigned a, unsigned b, unsigned c){
    return (struct BF_Basic){.a = a, .b = b, .c = c};
}
static int test_bf_signed(struct BF_Signed s){ return s.a + s.b; }
static int test_bf_cross_byte(struct BF_CrossByte s){ return (int)(s.a + s.b + s.c); }
static int test_bf_mixed(struct BF_Mixed s){ return s.x + (int)s.a + (int)s.b + (int)s.y; }
static int test_bf_multi_unit(struct BF_MultiUnit s){ return (int)(s.a + s.b + s.c + s.d); }

// Weird bitfields
struct BF_Anon { unsigned a : 4; unsigned : 4; unsigned b : 4; };
struct BF_ZeroWidth { unsigned a : 8; unsigned : 0; unsigned b : 8; };
struct BF_SingleBit { unsigned a : 1; unsigned b : 1; unsigned c : 1; };
struct BF_FullWidth { unsigned a : 32; };
struct BF_Bool { _Bool a : 1; _Bool b : 1; };
struct BF_DiffTypes { unsigned a : 4; unsigned short b : 4; unsigned char c : 4; };
struct BF_AnonPad { unsigned a : 3; unsigned : 13; unsigned b : 16; };

static int test_bf_anon(struct BF_Anon s){ return (int)(s.a + s.b); }
static int test_bf_zero_width(struct BF_ZeroWidth s){ return (int)(s.a + s.b); }
static int test_bf_single_bit(struct BF_SingleBit s){ return (int)(s.a + s.b + s.c); }
static int test_bf_full_width(struct BF_FullWidth s){ return (int)s.a; }
static int test_bf_bool(struct BF_Bool s){ return s.a + s.b; }
static int test_bf_diff_types(struct BF_DiffTypes s){ return (int)(s.a + s.b + s.c); }
static int test_bf_anon_pad(struct BF_AnonPad s){ return (int)(s.a + s.b); }

// Callback
static int test_apply(int (*fn)(int, int), int a, int b){ return fn(a, b); }
static void test_sort_ints(int* arr, int n, int (*cmp)(const void*, const void*)){
    for(int i = 0; i < n; i++)
        for(int j = i+1; j < n; j++)
            if(cmp(&arr[i], &arr[j]) > 0){
                int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
            }
}
union U1 { int a, b, c, d, e, f, g;};
static int test_u1(union U1 u, int x){return u.a + x;}
union U2 {long a; int x;};
static long test_u2(union U2 u, union U2 u2){return u.a + u2.a;}
struct v2f { union {float v[2]; struct {float x, y;};};};
int test_v2f_add(struct v2f v){ return (int)(v.x+v.y);}
struct v2fi { union {float v[2]; struct {float x, y;}; int i[2];};};
int test_v2fi_add(struct v2fi v){ return (int)(v.i[0]+v.i[1]);}
struct v2fd { struct { union {float f; double d;} a, b;};};
int test_v2fd_add(struct v2fd v){ return (int)(v.a.f+v.b.f);}
struct HasUnion {
    union { int a, b, c;};
    int x, y;
};
static int test_has_union(struct HasUnion hs){ return hs.a+hs.x+hs.y; }

TestFunction(test_interop){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    struct tc {
        const char* name; int line;
        StringView program;
        struct { StringView name; void* sym;} symbols[8];
        int exit_code;
        _Bool skip;
    } testcases[] = {
        // ---- Integer types ----
        {
            "int add", __LINE__,
            SV("int add(int, int);\n"
               "return add(3,4);\n"),
            {{SV("add"), (void*)test_add},},
            .exit_code = 7,
        },
        {
            "int sub", __LINE__,
            SV("int sub(int, int);\n"
               "return sub(10,3);\n"),
            {{SV("sub"), (void*)test_sub},},
            .exit_code = 7,
        },
        {
            "long long add", __LINE__,
            SV("long long add_ll(long long, long long);\n"
               "return (int)add_ll(3LL, 4LL);\n"),
            {{SV("add_ll"), (void*)test_add_ll},},
            .exit_code = 7,
        },
        {
            "char add", __LINE__,
            SV("char add_char(char, char);\n"
               "return add_char(3, 4);\n"),
            {{SV("add_char"), (void*)test_add_char},},
            .exit_code = 7,
        },
        {
            "short add", __LINE__,
            SV("short add_short(short, short);\n"
               "return add_short(3, 4);\n"),
            {{SV("add_short"), (void*)test_add_short},},
            .exit_code = 7,
        },
        {
            "unsigned add", __LINE__,
            SV("unsigned add_unsigned(unsigned, unsigned);\n"
               "return (int)add_unsigned(3u, 4u);\n"),
            {{SV("add_unsigned"), (void*)test_add_unsigned},},
            .exit_code = 7,
        },
        {
            "char signedness", __LINE__,
            SV("char c = (char)200;\n"
               "return c < 0;\n"),
            .exit_code = cc_target_funcs[CC_TARGET_NATIVE]().char_is_signed,
        },
        {
            "mixed integer widths", __LINE__,
            SV("int mixed_widths(char, long long, short);\n"
               "return mixed_widths(1, 2LL, 4);\n"),
            {{SV("mixed_widths"), (void*)test_mixed_widths},},
            .exit_code = 7,
        },
        // ---- Floating point ----
        {
            "double add", __LINE__,
            SV("double dadd(double, double);\n"
               "return (int)dadd(3.0, 4.0);\n"),
            {{SV("dadd"), (void*)test_dadd},},
            .exit_code = 7,
        },
        {
            "float add", __LINE__,
            SV("float fadd(float, float);\n"
               "return (int)fadd(3.0f, 4.0f);\n"),
            {{SV("fadd"), (void*)test_fadd},},
            .exit_code = 7,
        },
        {
            "mixed int/double args", __LINE__,
            SV("double mixed(int, double, int, double);\n"
               "return (int)mixed(1, 2.0, 3, 4.0);\n"),
            {{SV("mixed"), (void*)test_mixed_int_double},},
            .exit_code = 10,
        },
        {
            "float identity", __LINE__,
            SV("float fid(float);\n"
               "return (int)fid(42.0f);\n"),
            {{SV("fid"), (void*)test_float_identity},},
            .exit_code = 42,
        },
        {
            "double identity", __LINE__,
            SV("double did(double);\n"
               "return (int)did(42.0);\n"),
            {{SV("did"), (void*)test_double_identity},},
            .exit_code = 42,
        },
        {
            "double to int", __LINE__,
            SV("int d2i(double);\n"
               "return d2i(7.9);\n"),
            {{SV("d2i"), (void*)test_double_to_int},},
            .exit_code = 7,
        },
        {
            "int to double", __LINE__,
            SV("double i2d(int);\n"
               "return (int)i2d(7);\n"),
            {{SV("i2d"), (void*)test_int_to_double},},
            .exit_code = 7,
        },
        // ---- Many arguments (register spill) ----
        {
            "8 int args", __LINE__,
            SV("int many_ints(int,int,int,int,int,int,int,int);\n"
               "return many_ints(1,2,3,4,5,6,7,8);\n"),
            {{SV("many_ints"), (void*)test_many_ints},},
            .exit_code = 36,
        },
        {
            "9 double args", __LINE__,
            SV("double many_doubles(double,double,double,double,double,double,double,double,double);\n"
               "return (int)many_doubles(1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0);\n"),
            {{SV("many_doubles"), (void*)test_many_doubles},},
            .exit_code = 45,
        },
        {
            "many mixed args", __LINE__,
            SV("int many_mixed(int,double,int,double,int,double,int,double,int,double);\n"
               "return many_mixed(1,2.0,3,4.0,5,6.0,7,8.0,9,10.0);\n"),
            {{SV("many_mixed"), (void*)test_many_mixed},},
            .exit_code = 55,
        },
        // ---- Struct passing ----
        {
            "small struct arg", __LINE__,
            SV("struct S { int x; };\n"
               "int small_struct(struct S);\n"
               "struct S s = {42};\n"
               "return small_struct(s);\n"),
            {{SV("small_struct"), (void*)test_small_struct},},
            .exit_code = 42,
        },
        {
            "two int struct arg", __LINE__,
            SV("struct S { int x, y; };\n"
               "int two_int_struct(struct S);\n"
               "struct S s = {3, 4};\n"
               "return two_int_struct(s);\n"),
            {{SV("two_int_struct"), (void*)test_two_int_struct},},
            .exit_code = 7,
        },
        {
            "int+float struct arg", __LINE__,
            SV("struct S { int x; float y; };\n"
               "int int_float_struct(struct S);\n"
               "struct S s = {3, 4.0f};\n"
               "return int_float_struct(s);\n"),
            {{SV("int_float_struct"), (void*)test_int_float_struct},},
            .exit_code = 7,
        },
        {
            "large struct arg", __LINE__,
            SV("struct S { int a, b, c, d, e; };\n"
               "int large_struct(struct S);\n"
               "struct S s = {1, 2, 3, 4, 5};\n"
               "return large_struct(s);\n"),
            {{SV("large_struct"), (void*)test_large_struct},},
            .exit_code = 15,
        },
        // ---- Struct returns ----
        {
            "small struct return", __LINE__,
            SV("struct S { int x; };\n"
               "struct S ret_small(int);\n"
               "struct S s = ret_small(42);\n"
               "return s.x;\n"),
            {{SV("ret_small"), (void*)test_return_small_struct},},
            .exit_code = 42,
        },
        {
            "two int struct return", __LINE__,
            SV("struct S { int x, y; };\n"
               "struct S ret_two(int, int);\n"
               "struct S s = ret_two(3, 4);\n"
               "return s.x + s.y;\n"),
            {{SV("ret_two"), (void*)test_return_two_int_struct},},
            .exit_code = 7,
        },
        {
            "large struct return", __LINE__,
            SV("struct S { int a, b, c, d, e; };\n"
               "struct S ret_large(int);\n"
               "struct S s = ret_large(10);\n"
               "return s.a + s.b + s.c + s.d + s.e;\n"),
            {{SV("ret_large"), (void*)test_return_large_struct},},
            .exit_code = 60,
        },
        // ---- HFA (homogeneous float aggregates) ----
        {
            "two float struct (HFA)", __LINE__,
            SV("struct S { float x, y; };\n"
               "int two_float_struct(struct S);\n"
               "struct S s = {3.0f, 4.0f};\n"
               "return two_float_struct(s);\n"),
            {{SV("two_float_struct"), (void*)test_two_float_struct},},
            .exit_code = 7,
        },
        {
            "two double struct (HFA)", __LINE__,
            SV("struct S { double x, y; };\n"
               "int two_double_struct(struct S);\n"
               "struct S s = {3.0, 4.0};\n"
               "return two_double_struct(s);\n"),
            {{SV("two_double_struct"), (void*)test_two_double_struct},},
            .exit_code = 7,
        },
        {
            "four float struct (HFA)", __LINE__,
            SV("struct S { float a, b, c, d; };\n"
               "int four_float_struct(struct S);\n"
               "struct S s = {1.0f, 2.0f, 3.0f, 4.0f};\n"
               "return four_float_struct(s);\n"),
            {{SV("four_float_struct"), (void*)test_four_float_struct},},
            .exit_code = 10,
        },
        {
            "five float struct (not HFA)", __LINE__,
            SV("struct S { float a, b, c, d, e; };\n"
               "int five_float_struct(struct S);\n"
               "struct S s = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};\n"
               "return five_float_struct(s);\n"),
            {{SV("five_float_struct"), (void*)test_five_float_struct},},
            .exit_code = 15,
        },
        {
            "return two float struct (HFA)", __LINE__,
            SV("struct S { float x, y; };\n"
               "struct S ret_two_float(float, float);\n"
               "struct S s = ret_two_float(3.0f, 4.0f);\n"
               "return (int)(s.x + s.y);\n"),
            {{SV("ret_two_float"), (void*)test_return_two_float_struct},},
            .exit_code = 7,
        },
        {
            "return four float struct (HFA)", __LINE__,
            SV("struct S { float a, b, c, d; };\n"
               "struct S ret_four_float(float, float, float, float);\n"
               "struct S s = ret_four_float(1.0f, 2.0f, 3.0f, 4.0f);\n"
               "return (int)(s.a + s.b + s.c + s.d);\n"),
            {{SV("ret_four_float"), (void*)test_return_four_float_struct},},
            .exit_code = 10,
        },
        // ---- Eightbyte splitting ----
        {
            "int+double struct (split eightbyte)", __LINE__,
            SV("struct S { int x; double y; };\n"
               "int int_double_struct(struct S);\n"
               "struct S s = {3, 4.0};\n"
               "return int_double_struct(s);\n"),
            {{SV("int_double_struct"), (void*)test_int_double_struct},},
            .exit_code = 7,
        },
        {
            "float+float+int struct", __LINE__,
            SV("struct S { float a, b; int c; };\n"
               "int ffi_struct(struct S);\n"
               "struct S s = {1.0f, 2.0f, 4};\n"
               "return ffi_struct(s);\n"),
            {{SV("ffi_struct"), (void*)test_float_float_int_struct},},
            .exit_code = 7,
        },
        // ---- Void return, pointers, bool ----
        {
            "void return with pointer write", __LINE__,
            SV("void write_ptr(int*, int);\n"
               "int x = 0;\n"
               "write_ptr(&x, 42);\n"
               "return x;\n"),
            {{SV("write_ptr"), (void*)test_write_ptr},},
            .exit_code = 42,
        },
        {
            "pointer arg", __LINE__,
            SV("int deref(int*);\n"
               "int x = 42;\n"
               "return deref(&x);\n"),
            {{SV("deref"), (void*)test_deref},},
            .exit_code = 42,
        },
        {
            "pointer return", __LINE__,
            SV("int* ret_ptr(int*);\n"
               "int x = 42;\n"
               "int* p = ret_ptr(&x);\n"
               "return *p;\n"),
            {{SV("ret_ptr"), (void*)test_return_ptr},},
            .exit_code = 42,
        },
        {
            "bool return", __LINE__,
            SV("_Bool is_positive(int);\n"
               "return is_positive(5) + is_positive(-1);\n"),
            {{SV("is_positive"), (void*)test_is_positive},},
            .exit_code = 1,
        },
        // ---- Bitfields ----
        {
            "bitfield basic arg", __LINE__,
            SV("struct S { unsigned a : 3; unsigned b : 5; unsigned c : 8; };\n"
               "int bf_basic(struct S);\n"
               "struct S s = {.a = 5, .b = 17, .c = 200};\n"
               "return bf_basic(s);\n"),
            {{SV("bf_basic"), (void*)test_bf_basic},},
            .exit_code = 222,
        },
        {
            "bitfield basic return", __LINE__,
            SV("struct S { unsigned a : 3; unsigned b : 5; unsigned c : 8; };\n"
               "struct S bf_ret(unsigned, unsigned, unsigned);\n"
               "struct S s = bf_ret(7, 31, 100);\n"
               "return s.a + s.b + s.c;\n"),
            {{SV("bf_ret"), (void*)test_bf_basic_return},},
            .exit_code = 138,
        },
        {
            "bitfield signed", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "int bf_signed(struct S);\n"
               "struct S s = {.a = -3, .b = 5};\n"
               "return bf_signed(s);\n"),
            {{SV("bf_signed"), (void*)test_bf_signed},},
            .exit_code = 2,
        },
        {
            "bitfield cross byte boundary", __LINE__,
            SV("struct S { unsigned a : 7; unsigned b : 10; unsigned c : 15; };\n"
               "int bf_cross(struct S);\n"
               "struct S s = {.a = 100, .b = 500, .c = 10000};\n"
               "return bf_cross(s);\n"),
            {{SV("bf_cross"), (void*)test_bf_cross_byte},},
            .exit_code = 10600,
        },
        {
            "bitfield mixed with regular fields", __LINE__,
            SV("struct S { int x; unsigned a : 4; unsigned b : 4; float y; };\n"
               "int bf_mixed(struct S);\n"
               "struct S s = {.x = 10, .a = 5, .b = 3, .y = 2.0f};\n"
               "return bf_mixed(s);\n"),
            {{SV("bf_mixed"), (void*)test_bf_mixed},},
            .exit_code = 20,
        },
        {
            "bitfield multi storage unit", __LINE__,
            SV("struct S { unsigned a : 16; unsigned b : 16; unsigned c : 16; unsigned d : 16; };\n"
               "int bf_multi(struct S);\n"
               "struct S s = {.a = 1000, .b = 2000, .c = 3000, .d = 4000};\n"
               "return bf_multi(s);\n"),
            {{SV("bf_multi"), (void*)test_bf_multi_unit},},
            .exit_code = 10000,
        },
        // ---- Weird/anonymous bitfields ----
        {
            "anonymous bitfield padding", __LINE__,
            SV("struct S { unsigned a : 4; unsigned : 4; unsigned b : 4; };\n"
               "int bf_anon(struct S);\n"
               "struct S s = {.a = 9, .b = 6};\n"
               "return bf_anon(s);\n"),
            {{SV("bf_anon"), (void*)test_bf_anon},},
            .exit_code = 15,
        },
        {
            "zero-width bitfield", __LINE__,
            SV("struct S { unsigned a : 8; unsigned : 0; unsigned b : 8; };\n"
               "int bf_zero(struct S);\n"
               "struct S s = {.a = 100, .b = 55};\n"
               "return bf_zero(s);\n"),
            {{SV("bf_zero"), (void*)test_bf_zero_width},},
            .exit_code = 155,
        },
        {
            "single-bit bitfields", __LINE__,
            SV("struct S { unsigned a : 1; unsigned b : 1; unsigned c : 1; };\n"
               "int bf_single(struct S);\n"
               "struct S s = {.a = 1, .b = 0, .c = 1};\n"
               "return bf_single(s);\n"),
            {{SV("bf_single"), (void*)test_bf_single_bit},},
            .exit_code = 2,
        },
        {
            "full-width bitfield", __LINE__,
            SV("struct S { unsigned a : 32; };\n"
               "int bf_full(struct S);\n"
               "struct S s = {.a = 42};\n"
               "return bf_full(s);\n"),
            {{SV("bf_full"), (void*)test_bf_full_width},},
            .exit_code = 42,
        },
        {
            "bool bitfields", __LINE__,
            SV("struct S { _Bool a : 1; _Bool b : 1; };\n"
               "int bf_bool(struct S);\n"
               "struct S s = {.a = 1, .b = 1};\n"
               "return bf_bool(s);\n"),
            {{SV("bf_bool"), (void*)test_bf_bool},},
            .exit_code = 2,
        },
        {
            "different underlying types", __LINE__,
            SV("struct S { unsigned a : 4; unsigned short b : 4; unsigned char c : 4; };\n"
               "int bf_diff(struct S);\n"
               "struct S s = {.a = 5, .b = 7, .c = 3};\n"
               "return bf_diff(s);\n"),
            {{SV("bf_diff"), (void*)test_bf_diff_types},},
            .exit_code = 15,
        },
        {
            "anonymous bitfield large pad", __LINE__,
            SV("struct S { unsigned a : 3; unsigned : 13; unsigned b : 16; };\n"
               "int bf_apad(struct S);\n"
               "struct S s = {.a = 7, .b = 1000};\n"
               "return bf_apad(s);\n"),
            {{SV("bf_apad"), (void*)test_bf_anon_pad},},
            .exit_code = 1007,
        },
        // ---- Callbacks (native calling interpreted) ----
        {
            "callback: native calls interpreted fn ptr", __LINE__,
            SV("int apply(int (*)(int, int), int, int);\n"
               "int mul(int a, int b){ return a * b; }\n"
               "return apply(mul, 6, 7);\n"),
            {{SV("apply"), (void*)test_apply},},
            .exit_code = 42,
        },
        {
            "callback: sort", __LINE__,
            SV("void sort_ints(int*, int, int (*)(const void*, const void*));\n"
               "int cmp(const void* a, const void* b){\n"
               "    return *(int*)a - *(int*)b;\n"
               "}\n"
               "int arr[] = {5, 3, 1, 4, 2};\n"
               "sort_ints(arr, 5, cmp);\n"
               "return arr[0]*10000 + arr[1]*1000 + arr[2]*100 + arr[3]*10 + arr[4];\n"),
            {{SV("sort_ints"), (void*)test_sort_ints},},
            .exit_code = 12345,
        },
        {
            "HasUnion", __LINE__,
            SV("struct HasUnion {\n"
               "  union { int a, b, c;};\n"
               "  int x, y;\n"
               "};\n"
               "int has_union(struct HasUnion);\n"
               "struct HasUnion h = {\n"
               "  .a=1, .x=2, .y=3,\n"
               "};\n"
               "return has_union(h);\n"),
            {{SV("has_union"), (void*)test_has_union}},
            .exit_code = 6,
        },
        {
            "U1", __LINE__,
            SV("union U1 { int a, b, c, d, e, f, g;};\n"
               "static int u(union U1, long);\n"
               "return u((union U1){.a=4}, 4);\n"),
            {{SV("u"), (void*)test_u1}},
            .exit_code=8,
        },
        {
            "U2", __LINE__,
            SV("union U { long a; int x;};\n"
               "static int u(union U, union U);\n"
               "return u((union U){.a=4}, (union U){.x=3});\n"),
            {{SV("u"), (void*)test_u2}},
            .exit_code=7,
        },
        {
            "v2f", __LINE__,
            SV("struct v2f { union {float v[2]; struct {float x, y;};};};\n"
               "int add(struct v2f v);\n"
               "return add((struct v2f){1,2});\n"),
            {{SV("add"), (void*)test_v2f_add}},
            .exit_code=3,
        },
        {
            "v2fi", __LINE__,
            SV("struct v2fi { union {float v[2]; struct {float x, y;}; int i[2];};};\n"
               "int add(struct v2fi v);\n"
               "return add((struct v2fi){.i={1,2}});\n"),
            {{SV("add"), (void*)test_v2fi_add}},
            .exit_code=3,
        },
        {
            "v2fd", __LINE__,
            SV("struct v2fd { struct { union {float f; double d;} a, b;};};\n"
               "int add(struct v2fd v);\n"
               "return add((struct v2fd){.a.f=1, .b.f=2});\n"),
            {{SV("add"), (void*)test_v2fd_add}},
            .exit_code=3,
            .skip = 1,
        },
    };
    int err;
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < sizeof testcases/sizeof testcases[0]; i = test_atomic_increment(&idx)){
        struct tc* tc = &testcases[i];
        if(tc->skip){
            TEST_stats.skipped++;
            continue;
        }
        err = 0;
        TEST_stats.executed++;
        FileCache* fc = fc_create(al);
        if(!fc){err = 1; TestReport("setup failure"); goto finally;}
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {.allocator = al};
        Environment env = {.allocator = al, .at=&at};
        CiInterpreter interp = {
            .exit_code = -1,
            .parser = {
                .cpp = {
                    .allocator = al,
                    .fc = fc,
                    .at = &at,
                    .logger = logger,
                    .env = &env,
                    .target = cc_target_funcs[CC_TARGET_NATIVE](),
                },
                .current = &interp.parser.global,
            },
            .top_frame = {
                .return_buf = &interp.exit_code,
                .return_size = sizeof interp.exit_code,
            },
        };
        LOCK_T_init(&interp.error_lock);
        fc_write_path(fc, __FILE__, sizeof __FILE__ - 1);
        err = fc_cache_file(fc, tc->program);
        if(err){TestReport("setup failure"); goto finally;}
        err = cpp_define_builtin_macros(&interp.parser.cpp);
        if(err){TestReport("setup failure"); goto finally;}
        err = cc_define_builtin_types(&interp.parser);
        if(err){TestReport("setup failure"); goto finally;}
        err = cc_register_pragmas(&interp.parser);
        if(err){TestReport("setup failure"); goto finally;}
        err = ci_register_pragmas(&interp);
        if(err){TestReport("setup failure"); goto finally;}
        err = ci_register_macros(&interp);
        if(err){TestReport("setup failure"); goto finally;}

        for(size_t s = 0; s < sizeof tc->symbols / sizeof tc->symbols[0]; s++){
            if(!tc->symbols[s].sym) break;
            err = ci_register_sym(&interp, SV("builtins"), tc->symbols[s].name, tc->symbols[s].sym);
            if(err){TestReport("register sym failure"); goto finally;}
        }

        err = cpp_include_file_via_file_cache(&interp.parser.cpp, SV(__FILE__));
        if(err) {TestReport("failed to include"); goto finally;}
        ma_tail(interp.parser.cpp.frames).line = tc->line+1;

        err = cc_parse_all(&interp.parser);
        if(err){TestPrintf("%s:%d: failed to parse\n", __FILE__, tc->line); goto finally;}
        err = ci_resolve_refs(&interp, 0);
        if(err){TestPrintf("%s:%d: failed to link\n", __FILE__, tc->line); goto finally;}

        CiInterpFrame* frame = &interp.top_frame;
        frame->stmts = interp.parser.toplevel_statements.data;
        frame->stmt_count = interp.parser.toplevel_statements.count;
        while(frame->pc < frame->stmt_count){
            err = ci_interp_step(&interp, frame);
            if(err) goto finally;
        }
        TEST_stats.executed++;
        if(interp.exit_code != tc->exit_code){
            TEST_stats.failures++;
            TestPrintf("%s:%d: expected (%d) != actual (%d)\n", __FILE__, tc->line, tc->exit_code, interp.exit_code);
        }

        finally:
        if(log_sb.cursor && !log_sb.errored){
            StringView sv = msb_borrow_sv(&log_sb);
            TestPrintf("%.*s\n", sv_p(sv));
        }
        if(err) TEST_stats.failures++;
        ArenaAllocator_free_all(&arena);
        ArenaAllocator_free_all(&interp.parser.cpp.synth_arena);
        ArenaAllocator_free_all(&interp.parser.scratch_arena);
    }
    TESTEND();
}

int main(int argc, char** argv){
    #ifdef USE_TESTING_ALLOCATOR
        testing_allocator_init();
    #endif
    RegisterTestFlags(test_interop, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    int err = test_main(argc, argv, NULL);
    #ifdef USE_TESTING_ALLOCATOR
        testing_assert_all_freed();
    #endif
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "ci_interp.c"
#include "../Drp/Allocators/allocator.c"
#include "../Drp/file_cache.c"
#include "cpp_preprocessor.c"
#include "cc_parser.c"
#include "native_call.c"
