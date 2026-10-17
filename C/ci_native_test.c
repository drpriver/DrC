//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#if defined __linux__ && !defined _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
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
#include <fenv.h>
#include <stdarg.h>

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-empty-struct"
#pragma clang diagnostic ignored "-Wgnu-empty-initializer"
#endif


static CiRtAny test_any_identity(CiRtAny a){ return a; }
static CiRtAny test_any_callback(CiRtAny (*f)(CiRtAny), CiRtAny a){ return f(a); }

#if !(defined __DRC__ && defined __GLIBC__)
static int test_fp_callback(int (*f)(void)){
    fenv_t saved;
    if(feholdexcept(&saved)) return -1;
    if(fesetround(FE_TONEAREST)){
        fesetenv(&saved);
        return -1;
    }
    int result = f();
    fesetenv(&saved);
    return result;
}
static void test_fp_clear(void){ feclearexcept(FE_ALL_EXCEPT); }
static int test_fp_inexact(void){ return fetestexcept(FE_INEXACT) != 0; }
static int test_fp_rounding_modes(int (*f)(int)){
    fenv_t saved;
    if(feholdexcept(&saved)) return -1;
    int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    int result = 1;
    for(size_t i = 0; i < countof(modes); i++){
        if(fesetround(modes[i]) || !f(modes[i] == FE_UPWARD) || fegetround() != modes[i]){
            result = 0;
            break;
        }
    }
    fesetenv(&saved);
    return result;
}
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
static float test_float_callback(float (*f)(float), float x){ return f(x); }
static double test_double_identity(double x){ return x; }
static long double test_long_double_identity(long double x){ return x; }
static long double test_long_double_callback(long double (*f)(long double), long double x){ return f(x); }
struct LongDoubleRecord { long double x; };
static struct LongDoubleRecord test_ld_record(struct LongDoubleRecord x){ x.x += 1.L; return x; }
static struct LongDoubleRecord test_ld_record_callback(struct LongDoubleRecord (*f)(struct LongDoubleRecord), struct LongDoubleRecord x){ return f(x); }
#if defined __GNUC__ || defined __clang__
struct __attribute__((aligned(16))) PaddedDoubleRecord { double x; };
static struct PaddedDoubleRecord test_padded_record(struct PaddedDoubleRecord x, long a, double b){ x.x += a+b; return x; }
static struct PaddedDoubleRecord test_padded_record_callback(struct PaddedDoubleRecord (*f)(struct PaddedDoubleRecord), struct PaddedDoubleRecord x){ return f(x); }
static struct PaddedDoubleRecord test_padded_record_spill(double a, double b, double c, double d, double e, double f, double g, double h, struct PaddedDoubleRecord x, struct PaddedDoubleRecord y, double tail){
    (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
    x.x += a+h+y.x+tail;
    return x;
}
struct __attribute__((aligned(16))) PaddedIntRecord { long long x; };
static struct PaddedIntRecord test_padded_int_record(struct PaddedIntRecord x, long long a, double b){ x.x += a+(long long)b; return x; }
static struct PaddedIntRecord test_padded_int_record_spill(long long a, long long b, long long c, long long d, long long e, long long f, struct PaddedIntRecord x, struct PaddedIntRecord y, long long tail){
    (void)b; (void)c; (void)d; (void)e;
    x.x += a+f+y.x+tail;
    return x;
}
#endif
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
struct FourDoubleStruct { double a, b, c, d; };
struct FiveDoubleStruct { double a, b, c, d, e; };
struct NestedRectStruct { struct { double x, y; } origin; struct { double w, h; } size; };
struct FiveFloatStruct { float a, b, c, d, e; };
struct IntDoubleStruct { int x; double y; };
struct FloatFloatIntStruct { float a, b; int c; };

struct MixRecord { long long i; double d; };
struct ReverseMixRecord { double d; long long i; };
struct BigRecord { long long a,b,c; };
static struct MixRecord test_mix_return(struct MixRecord x){ x.i+=3; x.d+=4.; return x; }
static struct ReverseMixRecord test_reverse_mix_return(struct ReverseMixRecord x){ x.i+=3; x.d+=4.; return x; }
static struct MixRecord test_mix_callback(struct MixRecord (*fn)(struct MixRecord), struct MixRecord x){ return fn(x); }
static struct ReverseMixRecord test_reverse_mix_callback(struct ReverseMixRecord (*fn)(struct ReverseMixRecord), struct ReverseMixRecord x){ return fn(x); }
static double test_mix_gp_spill(long long a,long long b,long long c,long long d,long long e,long long f,struct MixRecord x,long long tail,double fp){
    return a+b+c+d+e+f+x.i+x.d+tail+fp;
}
static double test_mix_fp_spill(double a,double b,double c,double d,double e,double f,double g,double h,struct MixRecord x,long long tail,double fp){
    return a+b+c+d+e+f+g+h+x.i+x.d+tail+fp;
}
static double test_hfa_spill(double a,double b,double c,double d,double e,double f,double g,struct TwoDoubleStruct x,double tail){
    return a+b+c+d+e+f+g+x.x+x.y+tail;
}
static struct BigRecord test_hidden_return(long long a,long long b,long long c,long long d,long long e,struct MixRecord x,long long tail,double fp){
    return (struct BigRecord){a+b+c+d+e+x.i, (long long)(x.d+fp), tail};
}
static struct BigRecord test_hidden_callback(struct BigRecord (*fn)(struct MixRecord),struct MixRecord x){ return fn(x); }
static struct MixRecord test_mix_varargs(int spill,...){
    va_list ap;
    va_start(ap,spill);
    long long sum=0;
    if(spill){
        for(int i=0;i<6;i++) sum+=va_arg(ap,long long);
        for(int i=0;i<8;i++) sum+=(long long)va_arg(ap,double);
    }
    struct MixRecord x=va_arg(ap,struct MixRecord);
    struct ReverseMixRecord y=va_arg(ap,struct ReverseMixRecord);
    long long tail=va_arg(ap,long long);
    double fp=va_arg(ap,double);
    va_end(ap);
    sum+=x.i+(long long)x.d+y.i+(long long)y.d+tail+(long long)fp;
    return (struct MixRecord){sum,2.*sum};
}
#if defined __GNUC__ || defined __clang__
struct __attribute__((packed)) PackedBitsRecord { unsigned long long a:60; unsigned b:8; };
static struct PackedBitsRecord test_packed_bits_record(struct PackedBitsRecord x){ x.a+=1; x.b+=2; return x; }
struct ZeroWidthRecord {char x; int:0; char y;};
static struct ZeroWidthRecord test_zero_width_record(struct ZeroWidthRecord x){ x.x+=1; x.y+=2; return x; }
#endif

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
static int test_four_double_then_ints(struct FourDoubleStruct s, unsigned long u1, unsigned long u2, _Bool b){
    return (int)(s.a + s.b + s.c + s.d) + (int)u1 + (int)u2 + (int)b;
}
static int test_five_double_then_ints(struct FiveDoubleStruct s, unsigned long u1, unsigned long u2, _Bool b){
    return (int)(s.a + s.b + s.c + s.d+s.e) + (int)u1 + (int)u2 + (int)b;
}
static int test_nested_rect_then_ints(struct NestedRectStruct r, unsigned long u1, unsigned long u2, _Bool b){
    return (int)(r.origin.x + r.origin.y + r.size.w + r.size.h) + (int)u1 + (int)u2 + (int)b;
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
struct CharSlice {
    size_t count;
    char* data;
};
_Static_assert(sizeof(struct CharSlice) == sizeof(CiRtSlice), "");
_Static_assert(_Alignof(struct CharSlice) == _Alignof(CiRtSlice), "");
_Static_assert(offsetof(struct CharSlice, count) == offsetof(CiRtSlice, count), "");
_Static_assert(offsetof(struct CharSlice, data) == offsetof(CiRtSlice, data), "");
static struct CharSlice char_slice_rstrip(struct CharSlice s){
    for(;s.count;s.count--){
        char c = s.data[s.count-1];
        if(!(c == '\0' || c == ' ' || c == '\r' || c == '\n' || c == '\t'))
            break;
    }
    return s;
}
#ifdef __SIZEOF_INT128__
struct Bf128 {
    CiUint128 a: 1,
              b: 32,
              c: 53,
              d: 120,
              e: 33;
};
struct Bf128 inc_bf128(struct Bf128 b){
    b.a++;
    b.b++;
    b.c++;
    b.d++;
    b.e++;
    return b;
}
#endif

struct AbiPair { long long x,y; };
static long long abi_pair_five(long long a,long long b,long long c,long long d,long long e,struct AbiPair p,long long tail){
    return a+b+c+d+e+p.x+p.y+tail;
}
static long long abi_pair_seven(long long a,long long b,long long c,long long d,long long e,long long f,long long g,struct AbiPair p,long long tail){
    return a+b+c+d+e+f+g+p.x+p.y+tail;
}
static long long abi_pair_callback(long long (*fn)(long long,long long,long long,long long,long long,struct AbiPair,long long)){
    return fn(1,2,3,4,5,(struct AbiPair){11,13},17);
}
static double abi_mix_gp_callback(double (*fn)(long long,long long,long long,long long,long long,long long,struct MixRecord,long long,double)){
    return fn(1,2,3,4,5,6,(struct MixRecord){7,8.},9,10.);
}
static double abi_mix_fp_callback(double (*fn)(double,double,double,double,double,double,double,double,struct MixRecord,long long,double)){
    return fn(1.,2.,3.,4.,5.,6.,7.,8.,(struct MixRecord){7,8.},9,10.);
}
static struct BigRecord abi_hidden_callback(struct BigRecord (*fn)(long long,long long,long long,long long,long long,struct MixRecord,long long,double)){
    return fn(1,2,3,4,5,(struct MixRecord){6,7.},8,9.);
}
#if defined __GNUC__ || defined __clang__
static struct PaddedIntRecord abi_padding_callback(struct PaddedIntRecord (*fn)(long long,long long,long long,long long,long long,struct PaddedIntRecord,long long)){
    return fn(1,2,3,4,5,(struct PaddedIntRecord){3},7);
}
#endif

#define ABI_ODD_FIXTURE(N) \
    struct AbiOdd##N { unsigned char bytes[N]; }; \
    static struct AbiOdd##N abi_odd##N(struct AbiOdd##N x){ \
        for(unsigned i=0;i<N;i++) x.bytes[i]++; \
        return x; \
    } \
    static struct AbiOdd##N abi_odd_callback##N(struct AbiOdd##N (*fn)(struct AbiOdd##N),struct AbiOdd##N x){return fn(x);}
ABI_ODD_FIXTURE(3)
ABI_ODD_FIXTURE(5)
ABI_ODD_FIXTURE(7)
ABI_ODD_FIXTURE(15)
#undef ABI_ODD_FIXTURE

#ifdef __SIZEOF_INT128__
#define ABI_I128_SYMBOL(name) (void*)name
#define ABI_I128_SKIP 0
struct AbiI128 { unsigned __int128 x; };
static
unsigned __int128
abi_i128_scalar(unsigned __int128 x){
    return x + 3;
}
static
unsigned __int128
abi_i128_scalar_callback(unsigned __int128 (*fn)(unsigned __int128), unsigned __int128 x){
    return fn(x);
}
static long long abi_i128_odd(long long a,struct AbiI128 p,long long tail){
    return a+(long long)(p.x>>64)+(long long)p.x+tail;
}
static long long abi_i128_callback(long long (*fn)(long long,struct AbiI128,long long)){
    return fn(3,(struct AbiI128){((unsigned __int128)1<<100)+17},5);
}
static long long abi_i128_stack(long long a,long long b,long long c,long long d,long long e,long long f,long long g,long long h,long long before,struct AbiI128 p,long long after){
    return a+b+c+d+e+f+g+h+before+(long long)(p.x>>64)+(long long)p.x+after;
}
struct __attribute__((packed)) AbiPacked128 {unsigned a:4; unsigned __int128 b:128;};
static struct AbiPacked128 abi_packed128_callback(struct AbiPacked128 (*fn)(struct AbiPacked128),struct AbiPacked128 x){return fn(x);}
union AbiX87Integer {long double x; unsigned __int128 bits;};
static union AbiX87Integer abi_union_bump(union AbiX87Integer x){x.bits+=3; return x;}
static union AbiX87Integer abi_union_callback(union AbiX87Integer (*fn)(union AbiX87Integer),union AbiX87Integer x){return fn(x);}
#else
#define ABI_I128_SYMBOL(name) NULL
#define ABI_I128_SKIP 1
#endif

#if defined __GNUC__ || defined __clang__
#define ABI_PACKED_SYMBOL(name) (void*)name
#define ABI_PACKED_SKIP 0
struct __attribute__((packed)) AbiPacked64 {unsigned a:4; unsigned long long b:64;};
static struct AbiPacked64 abi_packed64_callback(struct AbiPacked64 (*fn)(struct AbiPacked64),struct AbiPacked64 x){return fn(x);}
#else
#define ABI_PACKED_SYMBOL(name) NULL
#define ABI_PACKED_SKIP 1
#endif
#define ABI_LEADING_SYMBOL(name) (void*)name
struct AbiLeadingPadding {unsigned long long:64; double x;};
static struct AbiLeadingPadding abi_leading_bump(struct AbiLeadingPadding x){x.x+=1.; return x;}
static struct AbiLeadingPadding abi_leading_callback(struct AbiLeadingPadding (*fn)(struct AbiLeadingPadding),struct AbiLeadingPadding x){return fn(x);}
// A named dummy forces GCC's INTEGER classification when Linux fixtures are
// compiled with Clang, which otherwise ignores the unnamed bitfield.
#if (defined __clang__ && !defined __APPLE__) || defined _WIN32
struct AbiOnlyPadding {unsigned char padding:7;};
#define ABI_EMPTY_INIT {0}
#else
struct AbiOnlyPadding {unsigned:7;};
#define ABI_EMPTY_INIT {}
#endif
static long long abi_only_padding(long long a,long long b,long long c,long long d,long long e,struct AbiOnlyPadding p,long long tail){
    (void)p; return a+b+c+d+e+tail;
}
static long long abi_only_padding_callback(long long (*fn)(long long,long long,long long,long long,long long,struct AbiOnlyPadding,long long)){
    return fn(1,2,3,4,5,(struct AbiOnlyPadding)ABI_EMPTY_INIT,6);
}
static int abi_empty_only(struct AbiOnlyPadding p){(void)p; return 9;}
static int abi_empty_only_callback(int (*fn)(struct AbiOnlyPadding)){
    return fn((struct AbiOnlyPadding)ABI_EMPTY_INIT);
}
struct AbiNestedEmpty {struct AbiOnlyPadding e[2]; unsigned:7;};
static long long abi_nested_empty(struct AbiOnlyPadding p,long long a,long long b,long long c,long long d,long long e,long long f,long long g,long long h,long long i,struct AbiNestedEmpty q,long long tail){
    (void)p; (void)q; return a+b+c+d+e+f+g+h+i+tail;
}
static long long abi_nested_empty_callback(long long (*fn)(struct AbiOnlyPadding,long long,long long,long long,long long,long long,long long,long long,long long,long long,struct AbiNestedEmpty,long long)){
    return fn((struct AbiOnlyPadding)ABI_EMPTY_INIT,1,2,3,4,5,6,7,8,9,(struct AbiNestedEmpty)ABI_EMPTY_INIT,10);
}
#undef ABI_EMPTY_INIT
static long long abi_empty_varargs(struct AbiOnlyPadding p,int n,...){
    (void)p;
    va_list ap; va_start(ap,n);
    long long sum = 0;
    for(int i = 0; i < n; i++) sum += va_arg(ap,long long);
    va_end(ap);
    return sum;
}

struct AbiThreeFloat {float x,y,z;};
#define ABI_VARARG_SYMBOL(name) (void*)name
#define ABI_VARARG_SKIP 0
static double abi_hfa_varargs(struct TwoDoubleStruct named,int count,...){
    double sum=named.x+named.y;
    va_list ap; va_start(ap,count);
    for(int i=0;i<count;i++) sum+=va_arg(ap,double);
    struct TwoDoubleStruct p=va_arg(ap,struct TwoDoubleStruct);
    struct AbiThreeFloat q=va_arg(ap,struct AbiThreeFloat);
    int tail=va_arg(ap,int); double fp=va_arg(ap,double);
    va_end(ap);
    return sum+p.x+p.y+(double)q.x+(double)q.y+(double)q.z+tail+fp;
}
static double abi_hfa_variadic_callback(double (*fn)(int,...)){
    return fn(0,(struct TwoDoubleStruct){8.,9.},(struct AbiThreeFloat){1,2,3},11,12.);
}
static
int
abi_read_list(va_list ap){
    int a = va_arg(ap, int);
    double b = va_arg(ap, double);
    struct TwoDoubleStruct c = va_arg(ap, struct TwoDoubleStruct);
    long long d = va_arg(ap, long long);
    return a == 17 && b == 2.5 && c.x == 3.5 && c.y == 4.5 && d == 29;
}
static
int
abi_supply_list(int (*fn)(va_list), int n, ...){
    (void)n;
    va_list ap;
    va_start(ap, n);
    int r = fn(ap);
    va_end(ap);
    return r;
}
static
int
abi_invoke_variadic(int (*fn)(int, ...)){
    return fn(0, 17, 2.5, (struct TwoDoubleStruct){3.5, 4.5}, 29LL);
}

#ifndef _MSC_VER
struct __attribute__((aligned(128))) AbiWideAligned {
    long long x;
};
static
int
abi_wide_aligned(int n, ...){
    (void)n;
    va_list ap;
    va_start(ap, n);
    struct AbiWideAligned a = va_arg(ap, struct AbiWideAligned);
    long long tail = va_arg(ap, long long);
    va_end(ap);
    return a.x == 19 && tail == 23;
}
static
int
abi_wide_aligned_callback(int (*fn)(int, ...)){
    return fn(0, (struct AbiWideAligned){19}, 23LL);
}
#endif

#if defined __GNUC__ || defined __clang__ || defined __DRC__
#define ABI_COMPLEX_RECORD_SYMBOL(name) (void*)name
#define ABI_COMPLEX_RECORD_SKIP 0
#ifndef __DRC__
#define ABI_COMPLEX_SCALAR_SYMBOL(name) (void*)name
#else
#define ABI_COMPLEX_SCALAR_SYMBOL(name) NULL
#endif
#define ABI_COMPLEX_FIXTURE(T,tag) \
    struct AbiComplex##tag {T _Complex z;}; \
    static T _Complex abi_complex_##tag(T _Complex z){return z;} \
    static T _Complex abi_complex_callback_##tag(T _Complex (*fn)(T _Complex),T _Complex z){return fn(z);} \
    static struct AbiComplex##tag abi_complex_record_##tag(struct AbiComplex##tag x){return x;} \
    static struct AbiComplex##tag abi_complex_record_callback_##tag(struct AbiComplex##tag (*fn)(struct AbiComplex##tag),struct AbiComplex##tag x){return fn(x);}
ABI_COMPLEX_FIXTURE(float,f)
ABI_COMPLEX_FIXTURE(double,d)
ABI_COMPLEX_FIXTURE(long double,ld)
#undef ABI_COMPLEX_FIXTURE
#else
#define ABI_COMPLEX_SCALAR_SYMBOL(name) NULL
#define ABI_COMPLEX_RECORD_SYMBOL(name) NULL
#define ABI_COMPLEX_RECORD_SKIP 1
#endif

#if defined __DRC__ || ((defined __GNUC__ && !defined __clang__ && __GNUC__ >= 12) || (defined __clang__ && __clang_major__ >= 15))
#define ABI_HALF_SYMBOL(name) (void*)name
#define ABI_HALF_SKIP 0
struct AbiHalf { _Float16 x,y,z; };
static _Float16 abi_half(_Float16 x){return x;}
static _Float16 abi_half_callback(_Float16 (*fn)(_Float16),_Float16 x){return fn(x);}
static struct AbiHalf abi_half_record(struct AbiHalf x){return x;}
static struct AbiHalf abi_half_record_callback(struct AbiHalf (*fn)(struct AbiHalf),struct AbiHalf x){return fn(x);}
#else
#define ABI_HALF_SYMBOL(name) NULL
#define ABI_HALF_SKIP 1
#endif
#if defined __DRC__ || ((defined __GNUC__ && !defined __clang__ && __GNUC__ >= 13) || (defined __clang__ && __clang_major__ >= 17))
#define ABI_BFLOAT_SYMBOL(name) (void*)name
#define ABI_BFLOAT_SKIP 0
static __bf16 abi_bfloat(__bf16 x){return x;}
static __bf16 abi_bfloat_callback(__bf16 (*fn)(__bf16),__bf16 x){return fn(x);}
#else
#define ABI_BFLOAT_SYMBOL(name) NULL
#define ABI_BFLOAT_SKIP 1
#endif

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
    #ifndef _MSC_VER
    {
        "ABI: over-aligned variadic stack arguments in both directions", __LINE__,
        SVI("struct __attribute__((aligned(128))) S {long long x;};int probe(int,...);int apply(int(*)(int,...));"
            "int f(int n,...){__builtin_va_list a;__builtin_va_start(a,n);struct S s=__builtin_va_arg(a,struct S);"
            "long long tail=__builtin_va_arg(a,long long);__builtin_va_end(a);return s.x==19&&tail==23;}"
            "return probe(0,(struct S){19},23LL)&&apply(f);"),
        {{SV("probe"),ABI_VARARG_SYMBOL(abi_wide_aligned)},{SV("apply"),ABI_VARARG_SYMBOL(abi_wide_aligned_callback)}},
        .skip=ABI_VARARG_SKIP,
        .exit_code=1,
    },
    #endif
    {
        "ABI: native and interpreted va_lists forward in both directions", __LINE__,
        SVI("struct D{double x,y;};int read_list(__builtin_va_list);"
            "int supply_list(int(*)(__builtin_va_list),int,...);int invoke(int(*)(int,...));"
            "int consume(__builtin_va_list ap){return read_list(ap);}"
            "int f(int n,...){__builtin_va_list a,b;__builtin_va_start(a,n);__builtin_va_copy(b,a);"
            "int r=consume(a)&&consume(b);__builtin_va_end(a);__builtin_va_end(b);"
            "__builtin_va_start(a,n);r=r&&consume(a);__builtin_va_end(a);return r;}"
            "return f(0,17,2.5,(struct D){3.5,4.5},29LL)&&invoke(f)"
            "&&supply_list(consume,0,17,2.5,(struct D){3.5,4.5},29LL);"),
        {{SV("read_list"),ABI_VARARG_SYMBOL(abi_read_list)},
         {SV("supply_list"),ABI_VARARG_SYMBOL(abi_supply_list)},
         {SV("invoke"),ABI_VARARG_SYMBOL(abi_invoke_variadic)}},
        .exit_code=1,.skip=ABI_VARARG_SKIP,
    },
    {
        "ABI: two GP words with one register remaining", __LINE__,
        SVI("struct P {long long x,y;}; long long probe(long long,long long,long long,long long,long long,struct P,long long);\n"
            "return probe(1,2,3,4,5,(struct P){11,13},17)==56;\n"),
        {{SV("probe"),(void*)abi_pair_five}}, .exit_code=1,
    },
    {
        "ABI: two GP words at the ARM64 register boundary", __LINE__,
        SVI("struct P {long long x,y;}; long long probe(long long,long long,long long,long long,long long,long long,long long,struct P,long long);\n"
            "return probe(1,2,3,4,5,6,7,(struct P){11,13},17)==69;\n"),
        {{SV("probe"),(void*)abi_pair_seven}}, .exit_code=1,
    },
    {
        "ABI callback: two GP words with one register remaining", __LINE__,
        SVI("struct P {long long x,y;}; long long apply(long long(*)(long long,long long,long long,long long,long long,struct P,long long));\n"
            "long long f(long long a,long long b,long long c,long long d,long long e,struct P p,long long t){return a+b+c+d+e+p.x+p.y+t;}\n"
            "return apply(f)==56;\n"),
        {{SV("apply"),(void*)abi_pair_callback}}, .exit_code=1,
    },
    {
        "ABI callback: mixed record after GP exhaustion", __LINE__,
        SVI("struct S {long long i;double d;}; double apply(double(*)(long long,long long,long long,long long,long long,long long,struct S,long long,double));\n"
            "double f(long long a,long long b,long long c,long long d,long long e,long long g,struct S x,long long t,double y){return a+b+c+d+e+g+x.i+x.d+t+y;}\n"
            "return apply(f)==55.;\n"),
        {{SV("apply"),(void*)abi_mix_gp_callback}}, .exit_code=1,
    },
    {
        "ABI callback: mixed record after FP exhaustion", __LINE__,
        SVI("struct S {long long i;double d;}; double apply(double(*)(double,double,double,double,double,double,double,double,struct S,long long,double));\n"
            "double f(double a,double b,double c,double d,double e,double g,double h,double j,struct S x,long long t,double y){return a+b+c+d+e+g+h+j+x.i+x.d+t+y;}\n"
            "return apply(f)==70.;\n"),
        {{SV("apply"),(void*)abi_mix_fp_callback}}, .exit_code=1,
    },
    {
        "ABI callback: hidden result and register pressure", __LINE__,
        SVI("struct S {long long i;double d;}; struct B {long long a,b,c;};\n"
            "struct B apply(struct B(*)(long long,long long,long long,long long,long long,struct S,long long,double));\n"
            "struct B f(long long a,long long b,long long c,long long d,long long e,struct S x,long long t,double y){return (struct B){a+b+c+d+e+x.i,(long long)(x.d+y),t};}\n"
            "struct B x=apply(f); return x.a==21&&x.b==16&&x.c==8;\n"),
        {{SV("apply"),(void*)abi_hidden_callback}}, .exit_code=1,
    },
    #if defined __GNUC__ || defined __clang__
    {
        "ABI callback: padded integer record under register pressure", __LINE__,
        SVI("struct __attribute__((aligned(16))) S {long long x;};\n"
            "struct S apply(struct S(*)(long long,long long,long long,long long,long long,struct S,long long));\n"
            "struct S f(long long a,long long b,long long c,long long d,long long e,struct S x,long long t){x.x+=a+b+c+d+e+t;return x;}\n"
            "return apply(f).x==25;\n"),
        {{SV("apply"),(void*)abi_padding_callback}}, .exit_code=1,
    },
    #endif
    #define ABI_ODD_CASE(N) { \
        "ABI: " #N "-byte arguments, results and callbacks preserve canaries", __LINE__, \
        SVI("struct S {unsigned char bytes[" #N "];}; struct W {unsigned char before;struct S s;unsigned char after;};\n" \
            "struct S bump(struct S); struct S apply(struct S(*)(struct S),struct S);\n" \
            "struct S f(struct S s){for(unsigned i=0;i<" #N ";i++)s.bytes[i]++;return s;}\n" \
            "struct W w={.before=0xa5,.after=0x5a}; for(unsigned i=0;i<" #N ";i++)w.s.bytes[i]=i+1;\n" \
            "w.s=bump(w.s); for(unsigned i=0;i<" #N ";i++)if(w.s.bytes[i]!=i+2)return 0;\n" \
            "w.s=apply(f,w.s); for(unsigned i=0;i<" #N ";i++)if(w.s.bytes[i]!=i+3)return 0;\n" \
            "return w.before==0xa5&&w.after==0x5a;\n"), \
        {{SV("bump"),(void*)abi_odd##N},{SV("apply"),(void*)abi_odd_callback##N}}, .exit_code=1, \
    },
    ABI_ODD_CASE(3)
    ABI_ODD_CASE(5)
    ABI_ODD_CASE(7)
    ABI_ODD_CASE(15)
    #undef ABI_ODD_CASE
    {
        "ABI: int128 scalar arguments, returns and callbacks", __LINE__,
        SVI("unsigned __int128 bump(unsigned __int128);unsigned __int128 apply(unsigned __int128(*)(unsigned __int128),unsigned __int128);\n"
            "unsigned __int128 f(unsigned __int128 x){return x+5;}\n"
            "unsigned __int128 x=((unsigned __int128)1<<110)+19;return bump(x)==x+3&&apply(f,x)==x+5;\n"),
        {{SV("bump"),ABI_I128_SYMBOL(abi_i128_scalar)},{SV("apply"),ABI_I128_SYMBOL(abi_i128_scalar_callback)}},
        .exit_code=1,.skip=ABI_I128_SKIP,
    },
    {
        "ABI: int128 record after an odd GP argument, including callback", __LINE__,
        SVI("struct S {unsigned __int128 x;}; long long probe(long long,struct S,long long);\n"
            "long long apply(long long(*)(long long,struct S,long long));\n"
            "long long f(long long a,struct S s,long long t){return a+(long long)(s.x>>64)+(long long)s.x+t;}\n"
            "struct S s={((unsigned __int128)1<<100)+17}; long long expected=((long long)1<<36)+25;\n"
            "return probe(3,s,5)==expected&&apply(f)==expected;\n"),
        {{SV("probe"),ABI_I128_SYMBOL(abi_i128_odd)},{SV("apply"),ABI_I128_SYMBOL(abi_i128_callback)}},
        .exit_code=1, .skip=ABI_I128_SKIP,
    },
    {
        "ABI: int128-aligned stack record between ordinary stack arguments", __LINE__,
        SVI("struct S {unsigned __int128 x;};\n"
            "long long probe(long long,long long,long long,long long,long long,long long,long long,long long,long long,struct S,long long);\n"
            "struct S s={((unsigned __int128)1<<100)+17};\n"
            "return probe(1,2,3,4,5,6,7,8,9,s,10)==((long long)1<<36)+72;\n"),
        {{SV("probe"),ABI_I128_SYMBOL(abi_i128_stack)}}, .exit_code=1, .skip=ABI_I128_SKIP,
    },
    {
        "ABI: named and variadic HFAs in registers", __LINE__,
        SVI("struct D {double x,y;};struct F {float x,y,z;};double probe(struct D,int,...);\n"
            "return probe((struct D){2.,3.},0,(struct D){8.,9.},(struct F){1,2,3},11,12.)==51.;\n"),
        {{SV("probe"),ABI_VARARG_SYMBOL(abi_hfa_varargs)}}, .exit_code=1, .skip=ABI_VARARG_SKIP,
    },
    {
        "ABI: variadic HFAs with one FP register remaining", __LINE__,
        SVI("struct D {double x,y;};struct F {float x,y,z;};double probe(struct D,int,...);\n"
            "return probe((struct D){2.,3.},5,1.,2.,3.,4.,5.,(struct D){8.,9.},(struct F){1,2,3},11,12.)==66.;\n"),
        {{SV("probe"),ABI_VARARG_SYMBOL(abi_hfa_varargs)}}, .exit_code=1, .skip=ABI_VARARG_SKIP,
    },
    {
        "ABI: variadic HFAs after FP register exhaustion", __LINE__,
        SVI("struct D {double x,y;};struct F {float x,y,z;};double probe(struct D,int,...);\n"
            "return probe((struct D){2.,3.},6,1.,2.,3.,4.,5.,6.,(struct D){8.,9.},(struct F){1,2,3},11,12.)==72.;\n"),
        {{SV("probe"),ABI_VARARG_SYMBOL(abi_hfa_varargs)}}, .exit_code=1, .skip=ABI_VARARG_SKIP,
    },
    {
        "ABI callback: incoming variadic HFAs", __LINE__,
        SVI("struct D {double x,y;};struct F {float x,y,z;};double apply(double(*)(int,...));\n"
            "double f(int n,...){__builtin_va_list ap;__builtin_va_start(ap,n);\n"
            "struct D d=__builtin_va_arg(ap,struct D);struct F q=__builtin_va_arg(ap,struct F);\n"
            "int t=__builtin_va_arg(ap,int);double y=__builtin_va_arg(ap,double);__builtin_va_end(ap);\n"
            "return d.x+d.y+q.x+q.y+q.z+t+y;}return apply(f)==46.;\n"),
        {{SV("apply"),ABI_VARARG_SYMBOL(abi_hfa_variadic_callback)}}, .exit_code=1,
        .skip=ABI_VARARG_SKIP,
    },
    {
        "ABI callback: packed full-width 64-bit field starting mid-byte", __LINE__,
        SVI("struct __attribute__((packed)) S {unsigned a:4;unsigned long long b:64;};\n"
            "struct S apply(struct S(*)(struct S),struct S);struct S f(struct S s){s.a++;s.b+=2;return s;}\n"
            "struct W {unsigned char before;struct S s;unsigned char after;} w={.before=0xa5,.s={7,0xfedcba9876543210ULL},.after=0x5a};\n"
            "w.s=apply(f,w.s);return w.s.a==8&&w.s.b==0xfedcba9876543212ULL&&w.before==0xa5&&w.after==0x5a;\n"),
        {{SV("apply"),ABI_PACKED_SYMBOL(abi_packed64_callback)}}, .exit_code=1, .skip=ABI_PACKED_SKIP,
    },
    {
        "ABI callback: packed full-width 128-bit field starting mid-byte", __LINE__,
        SVI("struct __attribute__((packed)) S {unsigned a:4;unsigned __int128 b:128;};\n"
            "struct S apply(struct S(*)(struct S),struct S);struct S f(struct S s){s.a++;s.b+=2;return s;}\n"
            "unsigned __int128 v=((unsigned __int128)1<<120)+13;\n"
            "struct W {unsigned char before;struct S s;unsigned char after;} w={.before=0xa5,.s={7,v},.after=0x5a};\n"
            "w.s=apply(f,w.s);return w.s.a==8&&w.s.b==v+2&&w.before==0xa5&&w.after==0x5a;\n"),
        {{SV("apply"),ABI_I128_SYMBOL(abi_packed128_callback)}}, .exit_code=1, .skip=ABI_I128_SKIP,
    },
    {
        "ABI: leading padding eightbyte in native arguments, results and callbacks", __LINE__,
        SVI("struct S {unsigned long long:64;double x;};struct S bump(struct S);struct S apply(struct S(*)(struct S),struct S);\n"
            "struct S f(struct S s){s.x+=2.;return s;}struct S s={9.};s=bump(s);s=apply(f,s);return s.x==12.;\n"),
        {{SV("bump"),ABI_LEADING_SYMBOL(abi_leading_bump)},{SV("apply"),ABI_LEADING_SYMBOL(abi_leading_callback)}}, .exit_code=1,
    },
    {
        "ABI: padding-only record followed by a live GP argument", __LINE__,
        SVI("struct S {unsigned:7;};long long probe(long long,long long,long long,long long,long long,struct S,long long);\n"
            "return probe(1,2,3,4,5,(struct S){},6)==21;\n"),
        {{SV("probe"),(void*)abi_only_padding}}, .exit_code=1,
    },
    {
        "ABI callback: padding-only record followed by a live GP argument", __LINE__,
        SVI("struct S {unsigned:7;};long long apply(long long(*)(long long,long long,long long,long long,long long,struct S,long long));\n"
            "long long f(long long a,long long b,long long c,long long d,long long e,struct S p,long long t){return a+b+c+d+e+t;}\n"
            "return apply(f)==21;\n"),
        {{SV("apply"),(void*)abi_only_padding_callback}}, .exit_code=1,
    },
    {
        "ABI: empty argument side effects survive direct and indirect calls", __LINE__,
        SVI("struct S {unsigned:7;};long long probe(long long,long long,long long,long long,long long,struct S,long long);\n"
            "int count;struct S empty(void){count++;return (struct S){};}\n"
            "long long local(long long a,long long b,long long c,long long d,long long e,struct S p,long long t){return a+b+c+d+e+t;}\n"
            "long long (*fn)(long long,long long,long long,long long,long long,struct S,long long)=probe;\n"
            "if(probe(1,2,3,4,5,empty(),6)!=21||fn(1,2,3,4,5,empty(),6)!=21)return 0;\n"
            "fn=local;if(local(1,2,3,4,5,empty(),6)!=21||fn(1,2,3,4,5,empty(),6)!=21)return 0;\n"
            "return count==4;\n"),
        {{SV("probe"),(void*)abi_only_padding}}, .exit_code=1,
        .skip=CC_TARGET_NATIVE!=CC_TARGET_AARCH64_MACOS,
    },
    {
        "ABI: all arguments omitted in calls and callbacks", __LINE__,
        SVI("struct E {unsigned:7;};int probe(struct E);int apply(int(*)(struct E));\n"
            "int f(struct E p){return 9;}int (*fn)(struct E)=probe;\n"
            "return probe((struct E){})==9&&fn((struct E){})==9&&apply(f)==9;\n"),
        {{SV("probe"),(void*)abi_empty_only},{SV("apply"),(void*)abi_empty_only_callback}}, .exit_code=1,
        .skip=CC_TARGET_NATIVE!=CC_TARGET_AARCH64_MACOS,
    },
    {
        "ABI: nested empty records around register and stack arguments, including callback", __LINE__,
        SVI("struct E {unsigned:7;};struct S {struct E e[2];unsigned:7;};\n"
            "long long probe(struct E,long long,long long,long long,long long,long long,long long,long long,long long,long long,struct S,long long);\n"
            "long long apply(long long(*)(struct E,long long,long long,long long,long long,long long,long long,long long,long long,long long,struct S,long long));\n"
            "long long f(struct E p,long long a,long long b,long long c,long long d,long long e,long long g,long long h,long long i,long long j,struct S q,long long t){return a+b+c+d+e+g+h+i+j+t;}\n"
            "return probe((struct E){},1,2,3,4,5,6,7,8,9,(struct S){},10)==55&&apply(f)==55;\n"),
        {{SV("probe"),(void*)abi_nested_empty},{SV("apply"),(void*)abi_nested_empty_callback}}, .exit_code=1,
        .skip=CC_TARGET_NATIVE!=CC_TARGET_AARCH64_MACOS,
    },
    {
        "ABI: omitted empty fixed argument before variadic arguments", __LINE__,
        SVI("struct E {unsigned:7;};long long probe(struct E,int,...);\n"
            "return probe((struct E){},3,11LL,13LL,17LL)==41;\n"),
        {{SV("probe"),ABI_VARARG_SYMBOL(abi_empty_varargs)}}, .exit_code=1,
        .skip=ABI_VARARG_SKIP || CC_TARGET_NATIVE!=CC_TARGET_AARCH64_MACOS,
    },
    {
        "ABI: x87/integer union arguments, results and callbacks", __LINE__,
        SVI("union U {long double x;unsigned __int128 bits;};union U bump(union U);union U apply(union U(*)(union U),union U);\n"
            "union U f(union U u){u.bits+=4;return u;}unsigned __int128 v=((unsigned __int128)1<<100)+13;\n"
            "union U u={.bits=v};u=bump(u);u=apply(f,u);return u.bits==v+7;\n"),
        {{SV("bump"),ABI_I128_SYMBOL(abi_union_bump)},{SV("apply"),ABI_I128_SYMBOL(abi_union_callback)}}, .exit_code=1,
        .skip=ABI_I128_SKIP,
    },
#define ABI_COMPLEX_CASE(T,tag) { \
        "ABI: " #T " complex scalar arguments, results and callbacks", __LINE__, \
        SVI("typedef " #T " T;typedef T _Complex C;C echo(C);C apply(C(*)(C),C);C f(C z){return z;}\n" \
            "union U {C z;T parts[2];} in={.parts={3,5}},out={.z=echo(in.z)},cb={.z=apply(f,in.z)};\n" \
            "return out.parts[0]==3&&out.parts[1]==5&&cb.parts[0]==3&&cb.parts[1]==5;\n"), \
        {{SV("echo"),ABI_COMPLEX_SCALAR_SYMBOL(abi_complex_##tag)},{SV("apply"),ABI_COMPLEX_SCALAR_SYMBOL(abi_complex_callback_##tag)}}, \
        .exit_code=1, .skip=1, /* FIXME: complex scalar value lowering is not implemented. */ \
    }, { \
        "ABI: " #T " complex record arguments, results and callbacks", __LINE__, \
        SVI("typedef " #T " T;struct S {T _Complex z;};\n" \
            "struct S echo_record(struct S);struct S apply_record(struct S(*)(struct S),struct S);\n" \
            "struct S g(struct S s){return s;}union U {struct S s;T parts[2];} in={.parts={3,5}};\n" \
            "union U out={.s=echo_record(in.s)},cb={.s=apply_record(g,in.s)};\n" \
            "return out.parts[0]==3&&out.parts[1]==5&&cb.parts[0]==3&&cb.parts[1]==5;\n"), \
        {{SV("echo_record"),ABI_COMPLEX_RECORD_SYMBOL(abi_complex_record_##tag)},{SV("apply_record"),ABI_COMPLEX_RECORD_SYMBOL(abi_complex_record_callback_##tag)}}, \
        .exit_code=1, .skip=ABI_COMPLEX_RECORD_SKIP, \
    },
    ABI_COMPLEX_CASE(float,f)
    ABI_COMPLEX_CASE(double,d)
    ABI_COMPLEX_CASE(long double,ld)
#undef ABI_COMPLEX_CASE
    {
        "ABI: half-precision scalars and HFAs, including callbacks", __LINE__,
        SVI("_Float16 echo(_Float16);_Float16 apply(_Float16(*)(_Float16),_Float16);\n"
            "struct S {_Float16 x,y,z;};struct S echo_record(struct S);struct S apply_record(struct S(*)(struct S),struct S);\n"
            "_Float16 f(_Float16 x){return x;}struct S g(struct S s){return s;}\n"
            "union U {_Float16 f;unsigned short u;};union R {struct S s;unsigned short u[3];};\n"
            "union R a={.u={0x3c00,0x4000,0x4200}},b={.u={0x4400,0x4500,0x4600}},r,q;\n"
            "r.s=echo_record(a.s);q.s=apply_record(g,b.s);\n"
            "union U x={.u=0x4000},y={.u=0x4200},z,w;z.f=echo(x.f);w.f=apply(f,y.f);\n"
            "return z.u==x.u&&w.u==y.u&&r.u[0]==a.u[0]&&r.u[1]==a.u[1]&&r.u[2]==a.u[2]&&q.u[0]==b.u[0]&&q.u[1]==b.u[1]&&q.u[2]==b.u[2];\n"),
        {{SV("echo"),ABI_HALF_SYMBOL(abi_half)},{SV("apply"),ABI_HALF_SYMBOL(abi_half_callback)},
         {SV("echo_record"),ABI_HALF_SYMBOL(abi_half_record)},{SV("apply_record"),ABI_HALF_SYMBOL(abi_half_record_callback)}},
        .exit_code=1, .skip=ABI_HALF_SKIP,
    },
    {
        "ABI: bfloat16 arguments, results and callbacks", __LINE__,
        SVI("__bf16 echo(__bf16);__bf16 apply(__bf16(*)(__bf16),__bf16);__bf16 f(__bf16 x){return x;}\n"
            "union U {__bf16 f;unsigned short u;};union U x={.u=0x4000},y={.u=0x4040},z,w;\n"
            "z.f=echo(x.f);w.f=apply(f,y.f);return z.u==x.u&&w.u==y.u;\n"),
        {{SV("echo"),ABI_BFLOAT_SYMBOL(abi_bfloat)},{SV("apply"),ABI_BFLOAT_SYMBOL(abi_bfloat_callback)}},
        .exit_code=1, .skip=ABI_BFLOAT_SKIP,
    },
        {
            "frame stack: reentrant native callbacks preserve parent alloca", __LINE__,
            SV("int apply(int(*)(int,int),int,int);\n"
               "int descend(int n,int value){char* p=__builtin_alloca(40001);p[0]=n;p[40000]=value;\n"
               "int v=n?apply(descend,n-1,value+1):0;return v+(p[0]==n&&p[40000]==value);}\n"
               "return descend(5,7)==6&&descend(5,7)==6;\n"),
            {{SV("apply"), (void*)test_apply}},
            .exit_code = 1,
        },
        {
            "inline: native callbacks preserve inline locals and addresses", __LINE__,
            SV("int apply(int(*)(int,int),int,int);\n"
               "inline int add(int a,int b){return a+b;}\n"
               "__forceinline int wrapper(int x){int local=x;int* p=&local;\n"
               "int r=apply(add,x,3);return *p+r;}\n"
               "int outer(void){return wrapper(7)==17&&wrapper(9)==21;}return outer();\n"),
            {{SV("apply"), (void*)test_apply}},
            .exit_code = 1,
        },
        {
            "memcpy: runtime size and its side effects retain native behavior", __LINE__,
            SV("char src[4]={1,2,3,4};char dst[4]={0};volatile unsigned long n=3;int calls=0;\n"
               "void* result=__builtin_memcpy(dst,src,(calls++,n));\n"
               "return result==dst&&calls==1&&dst[0]==1&&dst[1]==2&&dst[2]==3&&dst[3]==0;\n"),
            .exit_code = 1,
        },
        {
            "memcpy: side effects in an otherwise fixed size are preserved", __LINE__,
            SV("char src=42,dst=0;int calls=0;\n"
               "void* result=__builtin_memcpy(&dst,&src,(calls++,1));\n"
               "return result==&dst&&calls==1&&dst==42;\n"),
            .exit_code = 1,
        },
#if !(defined __DRC__ && defined __GLIBC__)
        {
            "floating expressions honor each rounding mode at runtime", __LINE__,
            SV("int fp_modes(int(*)(int));void fp_clear(void);int fp_inexact(void);\n"
               "int f(int up){fp_clear();float x=(float)16777217;\n"
               "double y=1.0+0x1p-53;float z=(float)0x1.000001p0;\n"
               "return x==(up?16777218.f:16777216.f)"
               " &&y==(up?0x1.0000000000001p0:1.0)"
               " &&z==(up?0x1.000002p0f:1.f)&&fp_inexact();}\n"
               "return fp_modes(f);\n"),
            {{SV("fp_modes"), (void*)test_fp_rounding_modes},
             {SV("fp_clear"), (void*)test_fp_clear},
             {SV("fp_inexact"), (void*)test_fp_inexact}},
            .exit_code = 1,
        },
        {
            "review: runtime aggregate preserves floating exceptions", __LINE__,
            SV("int fp_callback(int(*)(void)); void fp_clear(void); int fp_inexact(void);\n"
               "int f(void){fp_clear(); double a[4]={0.1+0.2,1,2,3};\n"
               "return a[0]>0 && fp_inexact();} return fp_callback(f);\n"),
            {{SV("fp_callback"), (void*)test_fp_callback},
             {SV("fp_clear"), (void*)test_fp_clear},
             {SV("fp_inexact"), (void*)test_fp_inexact}},
            .exit_code = 1,
        },
#endif

        {
            "any: native argument and return", __LINE__,
            SV("_Any identity(_Any);\n"
               "_Any a=identity(42);\n"
               "return a.type==int && a.as(int)==42;\n"),
            {{SV("identity"), (void*)test_any_identity}},
            .exit_code = 1,
        },
        {
            "any: native callback", __LINE__,
            SV("_Any apply(_Any(*)(_Any), _Any);\n"
               "_Any inc(_Any a){return a.as(int)+1;}\n"
               "_Any a=apply(inc, 41);\n"
               "return a.type==int && a.as(int)==42;\n"),
            {{SV("apply"), (void*)test_any_callback}},
            .exit_code = 1,
        },
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
            "mixed integer widths", __LINE__,
            SV("int mixed_widths(char, long long, short);\n"
               "return mixed_widths(1, 2LL, 4);\n"),
            {{SV("mixed_widths"), (void*)test_mixed_widths},},
            .exit_code = 7,
        },
        // ---- Floating point ----
        {
            "flat buffer: indirect native and interpreted targets", __LINE__,
            SV("int mixed_widths(char,long long,short);\n"
               "int local(char a,long long b,short c){return a+b+c;}\n"
               "int run(int (*p)(char,long long,short)){return p(1,2LL,4);}\n"
               "return run(mixed_widths)+run(local);\n"),
            {{SV("mixed_widths"), (void*)test_mixed_widths}},
            .exit_code = 14,
        },
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
            "aligned typedefs: native scalar arguments, results and callbacks", __LINE__,
            SV("typedef float F __attribute__((aligned(1)));\n"
               "typedef int I __attribute__((aligned(32)));\n"
               "F fid(F); I add(I,I); F callback(F (*)(F),F);\n"
               "F local(F x){return x+1;}\n"
               "return (int)fid(3.0f)+add(4,5)+(int)callback(local,6.0f);\n"),
            {{SV("fid"), (void*)test_float_identity},
             {SV("add"), (void*)test_add},
             {SV("callback"), (void*)test_float_callback}},
            .exit_code = 19,
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
            "long double native argument and return", __LINE__,
            SV("long double identity(long double);\n"
               "long double x=__LDBL_MANT_DIG__>53 ? 0x1p60L : 0x1p50L;\n"
               "return identity(x+1.L)-x==1.L;\n"),
            {{SV("identity"), (void*)test_long_double_identity}},
            .exit_code = 1,
        },
        {
            "long double native callback", __LINE__,
            SV("long double apply(long double(*)(long double),long double);\n"
               "long double inc(long double x){return x+1.L;}\n"
               "long double x=__LDBL_MANT_DIG__>53 ? 0x1p60L : 0x1p50L;\n"
               "return apply(inc,x)-x==1.L;\n"),
            {{SV("apply"), (void*)test_long_double_callback}},
            .exit_code = 1,
        },
        {
            "int to double", __LINE__,
            SV("double i2d(int);\n"
               "return (int)i2d(7);\n"),
            {{SV("i2d"), (void*)test_int_to_double},},
            .exit_code = 7,
        },
        {
            "long double record: native argument and result", __LINE__,
            SV("struct S { long double x; }; struct S bump(struct S);\n"
               "struct S x={__LDBL_MANT_DIG__>53 ? 0x1p60L : 0x1p50L};\n"
               "return bump(x).x-x.x==1.L;\n"),
            {{SV("bump"), (void*)test_ld_record}},
            .exit_code = 1,
        },
        {
            "long double record: native callback argument and result", __LINE__,
            SV("struct S { long double x; }; struct S apply(struct S(*)(struct S),struct S);\n"
               "struct S bump(struct S x){x.x+=1.L; return x;}\n"
               "struct S x={__LDBL_MANT_DIG__>53 ? 0x1p60L : 0x1p50L};\n"
               "return apply(bump,x).x-x.x==1.L;\n"),
            {{SV("apply"), (void*)test_ld_record_callback}},
            .exit_code = 1,
        },
        #if defined __GNUC__ || defined __clang__
        {
            "padded FP record: native argument and result", __LINE__,
            SV("struct __attribute__((aligned(16))) S { double x; };\n"
               "struct S bump(struct S,long,double); struct S x={3.};\n"
               "return bump(x,4,5.).x==12.;\n"),
            {{SV("bump"), (void*)test_padded_record}},
            .exit_code = 1,
        },
        {
            "padded FP record: native callback argument and result", __LINE__,
            SV("struct __attribute__((aligned(16))) S { double x; };\n"
               "struct S apply(struct S(*)(struct S),struct S);\n"
               "struct S bump(struct S x){x.x+=2.; return x;} struct S x={3.};\n"
               "return apply(bump,x).x==5.;\n"),
            {{SV("apply"), (void*)test_padded_record_callback}},
            .exit_code = 1,
        },
        {
            "padded FP records: spilled arguments keep their full size", __LINE__,
            SV("struct __attribute__((aligned(16))) S { double x; };\n"
               "struct S bump(double,double,double,double,double,double,double,double,struct S,struct S,double);\n"
               "struct S x={3.}, y={4.}; return bump(1.,2.,3.,4.,5.,6.,7.,8.,x,y,5.).x==21.;\n"),
            {{SV("bump"), (void*)test_padded_record_spill}},
            .exit_code = 1,
        },
        {
            "padded integer record: padding consumes no register", __LINE__,
            SV("struct __attribute__((aligned(16))) S { long long x; };\n"
               "struct S bump(struct S,long long,double); struct S x={3};\n"
               "return bump(x,4,5.).x==12;\n"),
            {{SV("bump"), (void*)test_padded_int_record}},
            .exit_code = 1,
        },
        {
            "padded integer records: spilled arguments keep their full size", __LINE__,
            SV("struct __attribute__((aligned(16))) S { long long x; };\n"
               "struct S bump(long long,long long,long long,long long,long long,long long,struct S,struct S,long long);\n"
               "struct S x={3}, y={4}; return bump(1,2,3,4,5,6,x,y,5).x==19;\n"),
            {{SV("bump"), (void*)test_padded_int_record_spill}},
            .exit_code = 1,
        },
        #endif
        {
            "mixed INTEGER/SSE record: argument and return", __LINE__,
            SV("struct S {long long i; double d;}; struct S bump(struct S);\n"
               "struct S x=bump((struct S){7,8.}); return x.i==10&&x.d==12.;\n"),
            {{SV("bump"), (void*)test_mix_return}}, .exit_code=1,
        },
        {
            "mixed SSE/INTEGER record: argument and return", __LINE__,
            SV("struct S {double d; long long i;}; struct S bump(struct S);\n"
               "struct S x=bump((struct S){8.,7}); return x.i==10&&x.d==12.;\n"),
            {{SV("bump"), (void*)test_reverse_mix_return}}, .exit_code=1,
        },
        {
            "mixed INTEGER/SSE record: callback return", __LINE__,
            SV("struct S {long long i; double d;}; struct S apply(struct S(*)(struct S),struct S);\n"
               "struct S bump(struct S x){x.i+=3; x.d+=4.; return x;}\n"
               "struct S x=apply(bump,(struct S){7,8.}); return x.i==10&&x.d==12.;\n"),
            {{SV("apply"), (void*)test_mix_callback}}, .exit_code=1,
        },
        {
            "mixed SSE/INTEGER record: callback return", __LINE__,
            SV("struct S {double d; long long i;}; struct S apply(struct S(*)(struct S),struct S);\n"
               "struct S bump(struct S x){x.i+=3; x.d+=4.; return x;}\n"
               "struct S x=apply(bump,(struct S){8.,7}); return x.i==10&&x.d==12.;\n"),
            {{SV("apply"), (void*)test_reverse_mix_callback}}, .exit_code=1,
        },
        {
            "mixed record: GP exhaustion rolls back SIMD allocation", __LINE__,
            SV("struct S {long long i; double d;};\n"
               "double probe(long long,long long,long long,long long,long long,long long,struct S,long long,double);\n"
               "return probe(1,2,3,4,5,6,(struct S){7,8.},9,10.)==55.;\n"),
            {{SV("probe"), (void*)test_mix_gp_spill}}, .exit_code=1,
        },
        {
            "mixed record: SIMD exhaustion rolls back GP allocation", __LINE__,
            SV("struct S {long long i; double d;};\n"
               "double probe(double,double,double,double,double,double,double,double,struct S,long long,double);\n"
               "return probe(1.,2.,3.,4.,5.,6.,7.,8.,(struct S){7,8.},9,10.)==70.;\n"),
            {{SV("probe"), (void*)test_mix_fp_spill}}, .exit_code=1,
        },
        {
            "HFA exhaustion: following FP argument also spills", __LINE__,
            SV("struct S {double x,y;};\n"
               "double probe(double,double,double,double,double,double,double,struct S,double);\n"
               "return probe(1.,2.,3.,4.,5.,6.,7.,(struct S){8.,9.},10.)==55.;\n"),
            {{SV("probe"), (void*)test_hfa_spill}}, .exit_code=1,
        },
        {
            "hidden result pointer: register exhaustion with mixed record", __LINE__,
            SV("struct S {long long i; double d;}; struct B {long long a,b,c;};\n"
               "struct B probe(long long,long long,long long,long long,long long,struct S,long long,double);\n"
               "struct B b=probe(1,2,3,4,5,(struct S){6,7.},8,9.); return b.a==21&&b.b==16&&b.c==8;\n"),
            {{SV("probe"), (void*)test_hidden_return}}, .exit_code=1,
        },
        {
            "hidden result pointer: callback with mixed record", __LINE__,
            SV("struct S {long long i; double d;}; struct B {long long a,b,c;};\n"
               "struct B apply(struct B(*)(struct S),struct S);\n"
               "struct B make(struct S x){return (struct B){x.i,(long long)x.d,13};}\n"
               "struct B b=apply(make,(struct S){6,7.}); return b.a==6&&b.b==7&&b.c==13;\n"),
            {{SV("apply"), (void*)test_hidden_callback}}, .exit_code=1,
        },
        {
            "native variadic mixed records in registers", __LINE__,
            SV("struct S {long long i; double d;}; struct R {double d;long long i;}; struct S probe(int,...);\n"
               "struct S x=probe(0,(struct S){7,8.},(struct R){9.,10},11LL,12.); return x.i==57&&x.d==114.;\n"),
            {{SV("probe"), (void*)test_mix_varargs}}, .exit_code=1,
        },
        {
            "native variadic mixed records after register exhaustion", __LINE__,
            SV("struct S {long long i; double d;}; struct R {double d;long long i;}; struct S probe(int,...);\n"
               "struct S x=probe(1,1LL,2LL,3LL,4LL,5LL,6LL,1.,2.,3.,4.,5.,6.,7.,8.,"
               "(struct S){7,8.},(struct R){9.,10},11LL,12.); return x.i==114&&x.d==228.;\n"),
            {{SV("probe"), (void*)test_mix_varargs}}, .exit_code=1,
        },
        #if defined __GNUC__ || defined __clang__
        {
            "packed crossing bitfields: native argument and result", __LINE__,
            #ifdef _WIN32
            SV("struct __attribute__((packed)) S {unsigned long long a:60;unsigned b:8;}; struct S bump(struct S);\n"
               "struct S x=bump((struct S){0x123456789abcdefULL,0xa5});"
               "return sizeof(x)==12&&x.a==0x123456789abcdf0ULL&&x.b==0xa7;\n"),
            #else
            SV("struct __attribute__((packed)) S {unsigned long long a:60;unsigned b:8;}; struct S bump(struct S);\n"
               "struct S x=bump((struct S){0x123456789abcdefULL,0xa5});"
               "return sizeof(x)==9&&x.a==0x123456789abcdf0ULL&&x.b==0xa7;\n"),
            #endif
            {{SV("bump"), (void*)test_packed_bits_record}}, .exit_code=1,
        },
        {
            "zero-width bitfield: native argument and result", __LINE__,
            SV("struct S {char x;int:0;char y;}; struct S bump(struct S);\n"
               "struct S x=bump((struct S){3,4}); return x.x==4&&x.y==6;\n"),
            {{SV("bump"), (void*)test_zero_width_record}}, .exit_code=1,
        },
        #endif
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
        {
            "4-double HFA followed by int args (CGRect-like)", __LINE__,
            SV("struct S { double a, b, c, d; };\n"
               "int hfa4_then_ints(struct S, unsigned long, unsigned long, _Bool);\n"
               "struct S s = {1.0, 2.0, 3.0, 4.0};\n"
               "return hfa4_then_ints(s, 100, 20, 1);\n"),
            {{SV("hfa4_then_ints"), (void*)test_four_double_then_ints},},
            .exit_code = 131,
        },
        {
            "5-double not-HFA followed by int args", __LINE__,
            SV("struct S { double a, b, c, d, e; };\n"
               "int func(struct S, unsigned long, unsigned long, _Bool);\n"
               "struct S s = {1.0, 2.0, 3.0, 4.0, 5.0};\n"
               "return func(s, 100, 20, 1);\n"),
            {{SV("func"), (void*)test_five_double_then_ints},},
            .exit_code = 136,
        },
        {
            "nested HFA followed by int args (actual CGRect layout)", __LINE__,
            SV("struct R { struct { double x, y; } origin; struct { double w, h; } size; };\n"
               "int nested_then_ints(struct R, unsigned long, unsigned long, _Bool);\n"
               "struct R r = {{1.0, 2.0}, {3.0, 4.0}};\n"
               "return nested_then_ints(r, 100, 20, 1);\n"),
            {{SV("nested_then_ints"), (void*)test_nested_rect_then_ints},},
            .exit_code = 131,
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
            "callback: deep interpreted recursion returns to native caller", __LINE__,
            SV("int apply(int (*)(int, int), int, int);\n"
               "int descend(int n, int v){ return n ? descend(n-1, v) + 1 : v; }\n"
               "int outer(void){ return apply(descend, 10000, 7) + 3; }\n"
               "return outer();\n"),
            {{SV("apply"), (void*)test_apply},},
            .exit_code = 10010,
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
        // ---- Indirect calls with native targets ----
        {
            "fn ptr: native target", __LINE__,
            SV("int add(int, int);\n"
               "int (*fp)(int, int) = add;\n"
               "return fp(30, 12);\n"),
            {{SV("add"), (void*)test_add},},
            .exit_code = 42,
        },
        {
            "fn ptr: native target, double args", __LINE__,
            SV("double dadd(double, double);\n"
               "double (*fp)(double, double) = dadd;\n"
               "return (int)fp(1.5, 2.5);\n"),
            {{SV("dadd"), (void*)test_dadd},},
            .exit_code = 4,
        },
        {
            "fn ptr: one call site, native and interpreted targets", __LINE__,
            SV("int add(int, int);\n"
               "int mymul(int a, int b){ return a * b; }\n"
               "int apply(int (*f)(int, int), int a, int b){ return f(a, b); }\n"
               "return apply(add, 4, 5) + apply(mymul, 4, 5);\n"),
            {{SV("add"), (void*)test_add},},
            .exit_code = 29,
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
            .skip = 1, // TODO: this passes on macos, verify on other systems
        },
        {
            "variadic indirect zero varargs", __LINE__,
            SVI("char buf[8] = {0};\n"
                "typeof(snprintf)* p = snprintf;\n"
                "return p(buf, sizeof buf, \"hi\");\n"),
            {{SV("snprintf"), (void*)snprintf}},
            .exit_code = 2,
        },
        {
            "variadic indirect with varargs", __LINE__,
            SVI("char buf[8] = {0};\n"
                "typeof(snprintf)* p = snprintf;\n"
                "return p(buf, sizeof buf, \"%d\", 123);\n"),
            {{SV("snprintf"), (void*)snprintf}},
            .exit_code = 3,
        },
        {
            "char slice", __LINE__,
            SVI("char rstrip(char[:])[:];\n"
                "char s[:] = \"hello \\r\\n\\0\";\n"
                "s = rstrip(s);\n"
                "return (int)s.count;\n"),
            {{SVI("rstrip"), (void*)char_slice_rstrip}},
            .exit_code = 5,
        },
        {
            "128 bit bitfields", __LINE__,
            SVI("struct Bf128 {\n"
                "    __uint128_t a: 1,\n"
                "              b: 32,\n"
                "              c: 53,\n"
                "              d: 120,\n"
                "              e: 33;\n"
                "};\n"
                "struct Bf128 inc(struct Bf128);\n"
                "struct Bf128 b = {0, 12, -1, 44, 17};\n"
                "b = inc(b);\n"
                "if(b.a != 1) return __LINE__;\n"
                "if(b.b != 13) return __LINE__;\n"
                "if(b.c != 0) return __LINE__;\n"
                "if(b.d != 45) return __LINE__;\n"
                "if(b.e != 18) return __LINE__;\n"
                "return 0;\n"),
            #ifdef __SIZEOF_INT128__
            {{SVI("inc"), (void*)inc_bf128}},
            #else
            .skip = 1,
            #endif
            .exit_code = 0,
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
        FileCache* fc = fc_create(al, FC_FLAGS_NONE);
        if(!fc){err = 1; TestReport("setup failure"); goto finally;}
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {0};
        Environment env = {.allocator = al, .at=&at};
        CiInterpreter interp = {
            .poison_frame_slots = 1,
            .exit_code = -1,
            .parser = {
                .cpp = {
                    .allocator = al,
                    .fc = fc,
                    .at = &at,
                    .logger = logger,
                    .env = &env,
                    .target = &cc_target_native,
                },
                .current = &interp.parser.global,
            },
            .top_frame = {
                .return_buf = &interp.exit_code,
                .return_size = sizeof interp.exit_code,
            },
        };
        LOCK_T_init(&interp.error_lock);
        LOCK_T_init(&interp.atom_lock);
        LOCK_T_init(&interp.resolve_lock);
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
        if(err){TestPrintf("%s:%d: %s: failed to parse\n", __FILE__, tc->line, tc->name); goto finally;}
        err = ci_resolve_refs(&interp);
        if(err){TestPrintf("%s:%d: %s: failed to link\n", __FILE__, tc->line, tc->name); goto finally;}

        CiInterpFrame* frame = &interp.top_frame;
        err = ci_lower_toplevel(&interp, &interp.deps);
        if(err) goto finally;
        while(frame->pc < frame->op_count){
            err = ci_interp_step(&interp, frame);
            if(err) goto finally;
        }
        TEST_stats.executed++;
        if(interp.exit_code != tc->exit_code){
            TEST_stats.failures++;
            TestPrintf("%s:%d: %s: expected (%d) != actual (%d)\n", __FILE__, tc->line, tc->name, tc->exit_code, interp.exit_code);
        }

        finally:
        if(log_sb.cursor && !log_sb.errored){
            StringView sv = msb_borrow_sv(&log_sb);
            TestPrintf("%.*s\n", sv_p(sv));
        }
        if(err) TEST_stats.failures++;
        ci_tls_cleanup(&interp);
        ArenaAllocator_free_all(&interp.bt.arena);
        ArenaAllocator_free_all(&at.arena);
        ArenaAllocator_free_all(&arena);
        ArenaAllocator_free_all(&interp.parser.cpp.synth_arena);
        ArenaAllocator_free_all(&interp.parser.scratch_arena);
    }
    TESTEND();
}
TestFunction(test_interp){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    struct tc {
        const char* name; int line;
        StringView program;
        struct { StringView name; void* sym;} symbols[8];
        int exit_code;
        _Bool skip;
        const char*_Nullable parse_error;
    } testcases[] = {
        {
            "procmacro: file-scope static aggregate and scalar initialization", __LINE__,
            SVI("static struct {short pad; char digits[5];} table={0,\"0123\"};\n"
                "static char separator='.';\n"
                "int probe(void){return table.digits[2]+separator;}\n"
                "#pragma procmacro PROBE probe\n"
                "int parsed=PROBE();\n"
                "return parsed==('2'+'.') && probe()==parsed;\n"),
            .exit_code = 1,
        },
        {
            "procmacro: file-scope static initialization runs once", __LINE__,
            SVI("static int count=40;\n"
                "int next(void){return ++count;}\n"
                "#pragma procmacro NEXT next\n"
                "int a=NEXT(); int b=NEXT();\n"
                "return a==41 && b==42 && next()==43;\n"),
            .exit_code = 1,
        },
        {
            "procmacro: address of a previously prepared function", __LINE__,
            SVI("int leaf(int n){return n+1;}\n"
                "int first(int n){return leaf(n);}\n"
                "#pragma procmacro first\n"
                "int second(int n){static int (*p)(int)=leaf; return p(n);}\n"
                "#pragma procmacro second\n"
                "return first(2)+second(3);\n"),
            .exit_code = 7,
        },
        {
            "module: unused eager body does not resolve externs", __LINE__,
            SVI("_Module m=__compile(\"int missing_dvm_dep(void); int unused(void){return missing_dvm_dep();} int used(void){return 9;} return used();\", \"\");\n"
                "return m.run();\n"),
            .exit_code = 9,
        },
        {
            "procmacro: transitive dependencies without resolve", __LINE__,
            SVI("int g;\n"
                "int leaf(int n){return n ? leaf(n-1)+1 : ++g;}\n"
                "int middle(int n){static int (*p)(int)=leaf; return p(n);}\n"
                "int macro(int n){return middle(n);}\n"
                "#pragma procmacro macro\n"
                "return macro(5);\n"),
            .exit_code = 6,
        },
        {
            "procmacro: argument dependencies without resolve", __LINE__,
            SVI("int g; int helper(void){return ++g;}\n"
                "int identity(int n){return n;}\n"
                "#pragma procmacro identity\n"
                "return identity(helper());\n"),
            .exit_code = 1,
        },
        {
            "procmacro: any string literal result", __LINE__,
            SVI(
                "_Any text(void){return \"hello\";}\n"
                "#pragma procmacro text\n"
                "const char* s=text();\n"
                "return s[0]=='h' && s[4]=='o' && s[5]==0;\n"
            ),
            .exit_code = 1,
        },
        {
            "procmacro: any char array result respects its bound", __LINE__,
            SVI(
                "_Any text(void){return *(char(*)[2])\"xyzz\";}\n"
                "#pragma procmacro text\n"
                "const char* s=text();\n"
                "return s[0]=='x' && s[1]=='y' && s[2]==0;\n"
            ),
            .exit_code = 1,
        },
        {
            "procmacro: any const char array result", __LINE__,
            SVI(
                "_Any text(void){return *(const char(*)[3])\"ok\";}\n"
                "#pragma procmacro text\n"
                "const char* s=text();\n"
                "return s[0]=='o' && s[1]=='k' && s[2]==0;\n"
            ),
            .exit_code = 1,
        },
        {
            "procmacro: any result", __LINE__,
            SVI("_Any boxed(int x){return x+1;}\n"
                "#pragma procmacro boxed\n"
                "return boxed(41);\n"),
            .exit_code = 42,
        },
        {
            "procmacro: any result-2", __LINE__,
            SVI("_Any a(void){return \"x\\0y\";}\n" // embedded nul -> short string
                "int x = 42;\n"
                "#pragma procmacro a\n"
                "return __mixin(a());\n"),
            .exit_code = 42,
        },
        {
            "procmacro: aggregate argument", __LINE__,
            SVI("struct S { int a[5]; };\n"
                "int sum(struct S s){ return s.a[0]+s.a[4]; }\n"
                "#pragma procmacro sum\n"
                "return sum(((struct S){{3,0,0,0,4}}));\n"),
            .exit_code = 7,
        },
        {
            "procmacro: void result", __LINE__,
            SVI("void nothing(int x){}\n"
                "#pragma procmacro nothing\n"
                "nothing(3)\n"
                "return 7;\n"),
            .exit_code = 7,
        },
        {
            "procmacro", __LINE__,
            SVI("int add(int x, int y){return x + y;}\n"
                "#pragma procmacro add\n"
                "#if add(1, 2) == 3\n"
                "return add(1, 2);\n"
                "#endif"),
            .exit_code = 3,
        },
        {
            "procmacro with name", __LINE__,
            SVI("int add(int x, int y){return x + y;}\n"
                "#pragma procmacro ADD add\n"
                "#if ADD(1, 2) == 3\n"
                "return add(1, 2);\n"
                "#endif"),
            .exit_code = 3,
        },
        {
            "append lib", __LINE__,
            SVI("#pragma lib_path \"/hope/this/does/not/exist\"\n"
                "return 0;\n"),
            .exit_code = 0,
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
        FileCache* fc = fc_create(al, FC_FLAGS_NONE);
        if(!fc){err = 1; TestReport("setup failure"); goto finally;}
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {0};
        Environment env = {.allocator = al, .at=&at};
        CiInterpreter interp = {
            .poison_frame_slots = 1,
            .exit_code = -1,
            .procedural_macros = 1,
            .parser = {
                .cpp = {
                    .allocator = al,
                    .fc = fc,
                    .at = &at,
                    .logger = logger,
                    .env = &env,
                    .target = &cc_target_native,
                },
                .current = &interp.parser.global,
            },
            .top_frame = {
                .return_buf = &interp.exit_code,
                .return_size = sizeof interp.exit_code,
            },
        };
        LOCK_T_init(&interp.error_lock);
        LOCK_T_init(&interp.atom_lock);
        LOCK_T_init(&interp.resolve_lock);
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
        err = ci_resolve_refs(&interp);
        if(err){TestPrintf("%s:%d: failed to link\n", __FILE__, tc->line); goto finally;}

        CiInterpFrame* frame = &interp.top_frame;
        err = ci_prepare_toplevel(&interp);
        if(err) goto finally;
        while(frame->pc < frame->op_count){
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
        ci_tls_cleanup(&interp);
        ArenaAllocator_free_all(&interp.bt.arena);
        ArenaAllocator_free_all(&at.arena);
        ArenaAllocator_free_all(&arena);
        ArenaAllocator_free_all(&interp.parser.cpp.synth_arena);
        ArenaAllocator_free_all(&interp.parser.scratch_arena);
    }
    TESTEND();
}
TestFunction(test_interp_fail){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    struct tc {
        const char* name; int line;
        StringView program;
        StringView expected_msg;
        _Bool skip;
    } testcases[] = {
        {
            "pragma lib", __LINE__,
            SVI("#pragma lib \"hopethisdoesnotexist\"\n"),
            SVI("(test):1:2: error: failed to load library 'hopethisdoesnotexist'\n"),
        },
        {
            "pragma comment lib", __LINE__,
            SVI("#pragma comment(lib, \"hopethisdoesnotexist\")\n"),
            SVI("(test):1:2: error: failed to load library 'hopethisdoesnotexist'\n"),
        },
        {
            "pragma framework", __LINE__,
            SVI("#pragma framework \"hopethisdoesnotexist\"\n"),
            SVI("(test):1:2: error: failed to load framework 'hopethisdoesnotexist'\n"),
        },
        {
            "pragma pkg_config", __LINE__,
            SVI("#pragma pkg_config \"hopethisdoesnotexist\"\n"),
            // This always fails as we give an Environment without PATH
            SVI("(test):1:2: error: `pkg-config` not found in PATH\n"),
        },


        {
            "__shell", __LINE__,
            SVI("const char* foo = __shell(\"hopethisdoesnotexist\");\n"),
            SVI("(test):1:19: error: __SHELL__: 'hopethisdoesnotexist' not found in PATH\n"),
        },
        {
            "__shell again", __LINE__,
            SVI("const char* foo = __shell(\"./hopethisdoesnotexist\");\n"),
            SVI("(test):1:19: error: __SHELL__: './hopethisdoesnotexist' not found in PATH\n"),
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
        FileCache* fc = fc_create(al, FC_FLAGS_NONE);
        if(!fc){err = 1; TestReport("setup failure"); goto finally;}
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {0};
        Environment env = {.allocator = al, .at=&at};
        CiInterpreter interp = {
            .poison_frame_slots = 1,
            .can_dlopen = 1,
            .exit_code = -1,
            .procedural_macros = 1,
            .parser = {
                .cpp = {
                    .allocator = al,
                    .fc = fc,
                    .at = &at,
                    .logger = logger,
                    .env = &env,
                    .target = &cc_target_native,
                },
                .current = &interp.parser.global,
            },
            .top_frame = {
                .return_buf = &interp.exit_code,
                .return_size = sizeof interp.exit_code,
            },
        };
        LOCK_T_init(&interp.error_lock);
        LOCK_T_init(&interp.atom_lock);
        LOCK_T_init(&interp.resolve_lock);
        fc_write_path(fc, "(test)", sizeof "(test)" - 1);
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

        err = cpp_include_file_via_file_cache(&interp.parser.cpp, SV("(test)"));
        if(err) {TestReport("failed to include"); goto finally;}
        TEST_stats.executed++;
        err = cc_parse_all(&interp.parser);
        if(err) {err = 0; goto finally;}
        err = ci_resolve_refs(&interp);
        if(err){TestPrintf("%s:%d: failed to link\n", __FILE__, tc->line); goto finally;}

        CiInterpFrame* frame = &interp.top_frame;
        err = ci_prepare_toplevel(&interp);
        if(err) goto finally;
        while(frame->pc < frame->op_count){
            err = ci_interp_step(&interp, frame);
            if(err) goto finally;
        }

        finally:;
        StringView sv = SV("");
        if(log_sb.cursor && !log_sb.errored)
            sv = msb_borrow_sv(&log_sb);
        test_expect_equals_sv(tc->expected_msg, sv, "expected error", "actual error", &TEST_stats, __FILE__, __func__, tc->line);
        if(err && sv_equals(tc->expected_msg, sv)) err = 0;
        if(err) TEST_stats.failures++;
        ci_tls_cleanup(&interp);
        ArenaAllocator_free_all(&interp.bt.arena);
        ArenaAllocator_free_all(&at.arena);
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
    RegisterTestFlags(test_interp_fail, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    int err = test_main(argc, argv, NULL);
    #ifdef USE_TESTING_ALLOCATOR
        testing_assert_all_freed();
    #endif
    return err;
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "ci_interp.c"
#include "../Drp/Allocators/allocator.c"
#include "../Drp/file_cache.c"
#include "cpp_preprocessor.c"
#include "cc_parser.c"
#include "nc_native_call.c"

#ifdef __DRC__
#include "../Vendored/softfloat/softfloat_unity.c"
#endif
