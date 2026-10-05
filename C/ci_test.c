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
#define NO_NATIVE_CALL
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
#include "cc_errors.h"
#include "ci_interp.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


TestFunction(test_interpreter){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    static struct tc {
        const char* name; int line;
        StringView program;
        int exit_code;
        _Bool skip;
        _Bool expect_template;
        uint32_t expect_runtime_stores;
    } testcases[] = {
        {
            "frame reuse: partial aggregate and bitfield initializers clear omitted fields", __LINE__,
            SVI("struct S { unsigned a:3,b:5; int values[3]; };\n"
                "int f(int n){struct S s={.a=n};int a[4]={[2]=n};\n"
                "return s.a==n&&s.b==0&&s.values[0]==0&&s.values[1]==0&&s.values[2]==0\n"
                "&&a[0]==0&&a[1]==0&&a[2]==n&&a[3]==0;}\n"
                "for(int i=0;i<100;i++)if(!f(i%8))return 0;return 1;\n"),
            .exit_code = 1,
        },
        {
            "frame stack: recursive alloca survives segment boundaries and repeated calls", __LINE__,
            SVI("int descend(int n){unsigned char* a=__builtin_alloca(4097);unsigned char* b=__builtin_alloca(32);\n"
                "a[0]=n;a[4096]=n+1;b[0]=n+2;int v=n?descend(n-1):0;\n"
                "return v+(a[0]==n&&a[4096]==n+1&&b[0]==n+2&&a!=b);}\n"
                "for(int i=0;i<4;i++)if(descend(24)!=25)return 0;return 1;\n"),
            .exit_code = 1,
        },
        {
            "frame stack: large alloca preserves earlier allocations and alignment", __LINE__,
            SVI("int f(void){char* a=__builtin_alloca(17);char* b=__builtin_alloca(300001);char* c=__builtin_alloca(33);\n"
                "a[0]=1;a[16]=2;b[0]=3;b[300000]=4;c[0]=5;c[32]=6;\n"
                "return a[0]+a[16]+b[0]+b[300000]+c[0]+c[32]==21\n"
                "&&((unsigned long long)a%16)==0&&((unsigned long long)b%16)==0&&((unsigned long long)c%16)==0;}\n"
                "return f()&&f();\n"),
            .exit_code = 1,
        },
        {
            "frame stack: large frames preserve caller locals", __LINE__,
            SVI("int inner(int n){int a[20000];a[0]=n;a[19999]=n+1;return a[0]+a[19999];}\n"
                "int outer(void){int value=7;return inner(value)==15&&inner(9)==19&&value==7;}return outer();\n"),
            .exit_code = 1,
        },
        {
            "frame stack: top-level alloca survives child calls", __LINE__,
            SVI("int f(void){char* p=__builtin_alloca(80000);p[0]=7;p[79999]=9;return p[0]+p[79999];}\n"
                "char* p=__builtin_alloca(19);p[0]=42;p[18]=7;return f()==16&&p[0]==42&&p[18]==7;\n"),
            .exit_code = 1,
        },
        {
            "memcpy: fixed-size builtin preserves bytes and returns destination", __LINE__,
            SVI("unsigned char src[9]={1,2,3,4,5,6,7,8,9}; unsigned char dst[11]={0};\n"
                "void* result=__builtin_memcpy(dst+1,src,sizeof src);\n"
                "for(int i=0;i<9;i++)if(dst[i+1]!=src[i])return 0;\n"
                "return result==dst+1 && dst[0]==0 && dst[10]==0;\n"),
            .exit_code = 1,
        },
        {
            "memcpy: ordinary libc name and folded size", __LINE__,
            SVI("int src[2]={42,7}; int dst[2]={0};\n"
                "memcpy(dst,src,2*sizeof(int));return dst[0]==42&&dst[1]==7;\n"),
            .exit_code = 1,
        },
        {
            "memcpy: pointer arguments are captured once before copying", __LINE__,
            SVI("char src[2]={42,7}; char dst[2]={0}; char* p=dst; int calls=0;\n"
                "char* source(void){calls++;p=dst+1;return src;}\n"
                "void* result=__builtin_memcpy(p,source(),1);\n"
                "return result==dst&&p==dst+1&&calls==1&&dst[0]==42&&dst[1]==0;\n"),
            .exit_code = 1,
        },
        {
            "memcpy: zero size still evaluates pointer arguments", __LINE__,
            SVI("char src[2]={42,7}; char dst[2]={3,4};char* s=src;char* d=dst;\n"
                "void* result=__builtin_memcpy(d++,s++,0);\n"
                "return result==dst&&d==dst+1&&s==src+1&&dst[0]==3&&dst[1]==4;\n"),
            .exit_code = 1,
        },
        {
            "memcpy: assignment result retains the original destination", __LINE__,
            SVI("char src[2]={42,7};char dst[2]={0};char* p=dst;\n"
                "p=__builtin_memcpy(p,src+1,1);return p==dst&&dst[0]==7&&dst[1]==0;\n"),
            .exit_code = 1,
        },
        {
            "memcpy: copy between local objects", __LINE__,
            SVI("int f(void){int x=2,y=3;__builtin_memcpy(&x,&y,sizeof x);return x+y;}return f();\n"),
            .exit_code = 6,
        },
        {
            "memcpy: local destination reads source after argument side effects", __LINE__,
            SVI("int* source(int* p){*p=7;return p;}int f(void){int x=2,y=3;\n"
                "void* result=__builtin_memcpy(&x,source(&y),sizeof x);return result==&x&&x==7&&y==7;}return f();\n"),
            .exit_code = 1,
        },
        {
            "memcpy: local source and pointer destination retain return value", __LINE__,
            SVI("int f(int* dst){int src=42;return __builtin_memcpy(dst,&src,sizeof src)==dst;}\n"
                "int x=0;return f(&x)&&x==42;\n"),
            .exit_code = 1,
        },
        {
            "memcpy: local aggregate fields preserve neighboring bytes", __LINE__,
            SVI("int f(void){struct S{int a,b,c;}dst={1,2,3},src={4,5,6};\n"
                "void* result=__builtin_memcpy(&dst.b,&src.b,sizeof dst.b);\n"
                "return result==&dst.b&&dst.a==1&&dst.b==5&&dst.c==3;}return f();\n"),
            .exit_code = 1,
        },
        {
            "memcpy: local array decay copies exact bytes", __LINE__,
            SVI("int f(void){char src[3]={1,2,3},dst[4]={0};\n"
                "void* result=__builtin_memcpy(dst,src,sizeof src);\n"
                "return result==dst&&dst[0]==1&&dst[1]==2&&dst[2]==3&&dst[3]==0;}return f();\n"),
            .exit_code = 1,
        },
        {
            "memcpy: a user-defined function keeps its behavior", __LINE__,
            SVI("void* memcpy(void* d,const void* s,unsigned long n){*(char*)d=99;return (void*)s;}\n"
                "char src=42,dst=0;void* result=memcpy(&dst,&src,1);return result==&src&&dst==99;\n"),
            .exit_code = 1,
        },
        {
            "TLS aggregate initialization, alignment and static persistence", __LINE__,
            SVI("int target = 6;\n"
                "_Alignas(64) _Thread_local struct S {int a[3]; int* p;} x = {{1,2,3}, &target};\n"
                "_Thread_local int zero;\n"
                "int next(void){static _Thread_local int n = 10; return ++n;}\n"
                "int* saved = &x.a[1];\n"
                "x.a[1] += 5;\n"
                "return zero == 0 && x.a[0] == 1 && *saved == 7 && x.a[2] == 3\n"
                "&& *x.p == 6 && ((unsigned long)&x % 64) == 0 && next() == 11 && next() == 12;\n"),
            .exit_code = 1,
        },
        {
            "TLS block extern uses the interpreted definition", __LINE__,
            SVI("_Thread_local int x = 17;\n"
                "int next(void){extern _Thread_local int x; return ++x;}\n"
                "return next() == 18 && x == 18;\n"),
            .exit_code = 1,
        },
        {
            "TLS reflection returns the current instance", __LINE__,
            SVI("_Thread_local int x = 17;\n"
                "int* p = __root_module().symbol(\"x\", int);\n"
                "*p = 42;\n"
                "return p == &x && x == 42;\n"),
            .exit_code = 1,
        },
        {
            "integer relocations preserve nested addends and scalar initializer storage", __LINE__,
            SVI("static int target; static const long base={(long)&target};\n"
                "constexpr struct S {long address;} s={(long)&target};\n"
                "static long braced=base+4; static long field=s.address+4;\n"
                "static long nested=3+(2+((long)&target-1));\n"
                "static long text=(long)\"ab\"+1;\n"
                "static short absolute=(short)(int*)0xffff; static unsigned __int128 wide=(unsigned __int128)(void*)6;\n"
                "return braced==(long)&target+4&&field==braced&&nested==braced&&*(const char*)text=='b'&&absolute==-1&&wide==6;\n"),
            .exit_code = 1,
        },
        {
            "constexpr bool conversions agree with static storage", __LINE__,
            SVI("constexpr _Bool a=256,b=0.5,c=-0.5,d=-0.0;\n"
                "static _Bool x=a,y=b,z=c,w=d; static int target;\n"
                "constexpr _Bool pointer=(_Bool)&target; static _Bool p=pointer;\n"
                "constexpr union U {unsigned char byte; struct B {unsigned char pad:7; _Bool flag:1;} b;} bits={.byte=128};\n"
                "static _Bool packed=bits.b.flag;\n"
                "return x==1&&y==1&&z==1&&w==0&&p==1&&packed==1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr enums with bool storage agree with static casts", __LINE__,
            SVI("enum E:_Bool {ZERO,ONE}; constexpr enum E x=(enum E)2;\n"
                "static enum E copy=x; static enum E direct=(enum E)256;\n"
                "return (int)copy==1&&(int)direct==1&&!copy==0;\n"),
            .exit_code = 1,
        },
        {
            "runtime casts to enums with bool storage normalize their values", __LINE__,
            SVI("enum E:_Bool {ZERO,ONE}; enum E convert(int n){return (enum E)n;}\n"
                "return convert(2)==ONE&&convert(256)==ONE&&convert(0)==ZERO;\n"),
            .exit_code = 1,
        },
        {
            "static enum storage preserves signed maximum and explicit resets", __LINE__,
            SVI("enum E:long long {MAX=9223372036854775807LL,RESET=0,NEXT};\n"
                "static enum E value=MAX; static enum E reset=RESET;\n"
                "return value==9223372036854775807LL&&reset==0&&NEXT==1;\n"),
            .exit_code = 1,
        },
        {
            "static arithmetic preserves wrapping, skipped branches and address addends", __LINE__,
            SVI("static int target; static long address=(long)&target+4;\n"
                "static long reversed=4+(long)&target; static long before=(long)&target-4;\n"
                "static int a=1?7:(1/0); static int b=0&&(1/0);\n"
                "static unsigned wrapped=~0U*2U;\n"
                "static __int128 wide=((__int128)1<<100)*2;\n"
                "return address==(long)&target+4&&reversed==address&&before==(long)&target-4\n"
                "&&a==7&&b==0&&wrapped==~1U&&wide==((__int128)1<<101);\n"),
            .exit_code = 1,
        },
        {
            "constant byte swaps agree with static storage and initializer paths", __LINE__,
            SVI("constexpr unsigned short a=__builtin_bswap16(0x1234);\n"
                "constexpr unsigned b=__builtin_bswap32(0x12345678U);\n"
                "constexpr unsigned long long c=__builtin_bswap64(0x0102030405060708ULL);\n"
                "static unsigned short x=a; static unsigned y=b; static unsigned long long z=c;\n"
                "static int values[3]={[__builtin_bswap16(0x0100)]=7};\n"
                "return x==0x3412&&y==0x78563412U&&z==0x0807060504030201ULL&&values[0]==0&&values[1]==7&&values[2]==0;\n"),
            .exit_code = 1,
        },
        {
            "all new bit builtin names support constant evaluation and correct result types", __LINE__,
            SVI("_Static_assert(__builtin_ffs(8)==4);\n"
                "_Static_assert(__builtin_ffsl(8)==4);\n"
                "_Static_assert(__builtin_ffsll(8)==4);\n"
                "_Static_assert(__builtin_clrsb(8)==sizeof(int)*8-5);\n"
                "_Static_assert(__builtin_clrsbl(8)==sizeof(long)*8-5);\n"
                "_Static_assert(__builtin_clrsbll(8)==sizeof(long long)*8-5);\n"
                "_Static_assert(__builtin_parity(8)==1);\n"
                "_Static_assert(__builtin_parityl(8)==1);\n"
                "_Static_assert(__builtin_parityll(8)==1);\n"
                "_Static_assert(__builtin_ffsg((signed char)8)==4);\n"
                "_Static_assert(__builtin_clzg((unsigned char)65)==1);\n"
                "_Static_assert(__builtin_ctzg((unsigned char)65)==0);\n"
                "_Static_assert(__builtin_clrsbg((signed char)8)==3);\n"
                "_Static_assert(__builtin_popcountg((unsigned char)65)==2);\n"
                "_Static_assert(__builtin_parityg((unsigned char)65)==0);\n"
                "_Static_assert(__builtin_stdc_bit_ceil((unsigned char)65)==128);\n"
                "_Static_assert(__builtin_stdc_bit_floor((unsigned char)65)==64);\n"
                "_Static_assert(__builtin_stdc_bit_width((unsigned char)65)==7);\n"
                "_Static_assert(__builtin_stdc_count_ones((unsigned char)65)==2);\n"
                "_Static_assert(__builtin_stdc_count_zeros((unsigned char)65)==6);\n"
                "_Static_assert(__builtin_stdc_first_leading_one((unsigned char)65)==2);\n"
                "_Static_assert(__builtin_stdc_first_leading_zero((unsigned char)65)==1);\n"
                "_Static_assert(__builtin_stdc_first_trailing_one((unsigned char)65)==1);\n"
                "_Static_assert(__builtin_stdc_first_trailing_zero((unsigned char)65)==2);\n"
                "_Static_assert(__builtin_stdc_has_single_bit((unsigned char)65)==0);\n"
                "_Static_assert(__builtin_stdc_leading_ones((unsigned char)65)==0);\n"
                "_Static_assert(__builtin_stdc_leading_zeros((unsigned char)65)==1);\n"
                "_Static_assert(__builtin_stdc_trailing_ones((unsigned char)65)==1);\n"
                "_Static_assert(__builtin_stdc_trailing_zeros((unsigned char)65)==0);\n"
                "_Static_assert(__builtin_stdc_rotate_left((unsigned char)65,1)==130);\n"
                "_Static_assert(__builtin_stdc_rotate_right((unsigned char)65,1)==160);\n"
                "_Static_assert(_Generic(__builtin_stdc_bit_ceil((unsigned char)1),unsigned char:1,default:0));\n"
                "_Static_assert(_Generic(__builtin_stdc_bit_floor((unsigned short)1),unsigned short:1,default:0));\n"
                "_Static_assert(_Generic(__builtin_stdc_rotate_left((unsigned __int128)1,0),unsigned __int128:1,default:0));\n"
                "_Static_assert(_Generic(__builtin_stdc_has_single_bit(1u),unsigned int:1,default:0));\n"
                "_Static_assert(_Generic(__builtin_popcountg(1u),int:1,default:0));\n"
                "static int fallback=__builtin_ctzg((unsigned char)0,-7);\n"
                "_Static_assert(__builtin_clrsbg((signed char)-128)==0);\n"
                "_Static_assert(__builtin_ffsg((signed char)-128)==8);\n"
                "_Static_assert(__builtin_clrsbg((signed char)0)==7);\n"
                "_Static_assert(__builtin_clrsbg((__int128)1<<126)==0);\n"
                "_Static_assert(__builtin_stdc_bit_ceil((unsigned __int128)1<<127)==((unsigned __int128)1<<127));\n"
                "_Static_assert(__builtin_stdc_bit_floor(~(unsigned __int128)0)==((unsigned __int128)1<<127));\n"
                "unsigned __int128 count=((unsigned __int128)1<<100)+129;\n"
                "unsigned __int128 value=((unsigned __int128)1<<127)|1;\n"
                "unsigned short narrow=0x8001;\n"
                "signed char neg=-128;\n"
                "int i=0,j=0;\n"
                "__builtin_stdc_rotate_left((i++,(unsigned char)1),j++);\n"
                "__builtin_clzg((i++,(unsigned char)0),j++);\n"
                "return fallback==-7&&i==2&&j==2&&__builtin_ffsg(neg)==8&&__builtin_clrsbg(neg)==0\n"
                "&&__builtin_stdc_rotate_left(value,count)==3\n"
                "&&__builtin_stdc_rotate_right(value,count)==(((unsigned __int128)1<<126)|((unsigned __int128)1<<127))\n"
                "&&__builtin_stdc_rotate_left(narrow,17)==3\n"
                "&&__builtin_stdc_rotate_right(narrow,17)==0xc000;\n"),
            .exit_code = 1,
        },
        {
            "generic bit builtins preserve all supported integer widths at runtime", __LINE__,
            SVI("#define CHECK(T,N) \\\n"
                "int N(T hi,T ones){unsigned w=sizeof(T)*8; \\\n"
                "return __builtin_clzg(hi)==0&&__builtin_ctzg(hi)==w-1 \\\n"
                "&&__builtin_popcountg(ones)==w&&__builtin_parityg(ones)==0 \\\n"
                "&&__builtin_stdc_bit_ceil(hi)==hi&&__builtin_stdc_bit_floor(ones)==hi \\\n"
                "&&__builtin_stdc_bit_width(hi)==w&&__builtin_stdc_count_zeros(hi)==w-1 \\\n"
                "&&__builtin_stdc_first_leading_one(hi)==1&&__builtin_stdc_first_trailing_one(hi)==w \\\n"
                "&&__builtin_stdc_first_leading_zero(hi)==2&&__builtin_stdc_first_trailing_zero(hi)==1 \\\n"
                "&&__builtin_stdc_leading_ones(hi)==1&&__builtin_stdc_trailing_zeros(hi)==w-1 \\\n"
                "&&__builtin_stdc_rotate_left(hi,1)==1&&__builtin_stdc_rotate_right((T)1,1)==hi;}\n"
                "CHECK(unsigned char,f8)\n"
                "CHECK(unsigned short,f16)\n"
                "CHECK(unsigned int,fi)\n"
                "CHECK(unsigned long,fl)\n"
                "CHECK(unsigned long long,fll)\n"
                "CHECK(unsigned __int128,f128)\n"
                "#undef CHECK\n"
                "int s(signed char x,short y,int z,long l,long long ll,__int128 wide){\n"
                "return __builtin_ffsg(x)==8&&__builtin_clrsbg(x)==0\n"
                "&&__builtin_ffsg(y)==sizeof(short)*8&&__builtin_clrsbg(y)==0\n"
                "&&__builtin_ffsg(z)==sizeof(int)*8&&__builtin_clrsbg(z)==0\n"
                "&&__builtin_ffsg(l)==sizeof(long)*8&&__builtin_clrsbg(l)==0\n"
                "&&__builtin_ffsg(ll)==sizeof(long long)*8&&__builtin_clrsbg(ll)==0\n"
                "&&__builtin_ffsg(wide)==128&&__builtin_clrsbg(wide)==0;\n"
                "}\n"
                "return f8(128,255)&&f16(0x8000,0xffff)\n"
                "&&fi(1u<<(sizeof(unsigned)*8-1),~0u)\n"
                "&&fl(1ul<<(sizeof(unsigned long)*8-1),~0ul)\n"
                "&&fll(1ull<<(sizeof(unsigned long long)*8-1),~0ull)\n"
                "&&f128((unsigned __int128)1<<127,~(unsigned __int128)0)\n"
                "&&s(-128,(short)(1u<<(sizeof(short)*8-1)),1u<<(sizeof(int)*8-1),\n"
                "1ul<<(sizeof(long)*8-1),1ull<<(sizeof(long long)*8-1),(unsigned __int128)1<<127);\n"),
            .exit_code = 1,
        },
        {
            "generic bit count fallbacks are evaluated only for zero", __LINE__,
            SVI("_Static_assert(__builtin_clzg((unsigned char)1,1/0)==7);\n"
                "_Static_assert(__builtin_ctzg((unsigned __int128)8,1/0)==3);\n"
                "int f(unsigned __int128 x){int n=0;\n"
                "int a=__builtin_clzg(x,++n); int b=__builtin_ctzg(x,++n);\n"
                "__builtin_clzg(x,++n); __builtin_ctzg(x,++n);\n"
                "return x? a==124&&b==3&&n==0 : a==1&&b==2&&n==4;}\n"
                "return f(8)&&f(0);\n"),
            .exit_code = 1,
        },
        {
            "new bit builtins preserve widths and evaluate arguments once", __LINE__,
            SVI("constexpr int a=__builtin_clzg((unsigned char)1);\n"
                "static unsigned char b=__builtin_stdc_bit_ceil((unsigned char)65);\n"
                "static unsigned __int128 c=__builtin_stdc_rotate_left((unsigned __int128)1,127);\n"
                "int f(unsigned char x,unsigned __int128 y){\n"
                "return __builtin_ffsg((__int128)y)==101&&__builtin_clzg(y)==27\n"
                "&&__builtin_ctzg(y)==100&&__builtin_popcountg(y)==1&&__builtin_parityg(y)==1\n"
                "&&__builtin_clrsbg((signed char)-1)==7&&__builtin_clrsbg((__int128)-1)==127\n"
                "&&__builtin_stdc_bit_floor(x)==64&&__builtin_stdc_bit_width(x)==7\n"
                "&&__builtin_stdc_count_ones(x)==2&&__builtin_stdc_count_zeros(x)==6\n"
                "&&__builtin_stdc_first_leading_one(x)==2&&__builtin_stdc_first_leading_zero(x)==1\n"
                "&&__builtin_stdc_first_trailing_one(x)==1&&__builtin_stdc_first_trailing_zero(x)==2\n"
                "&&__builtin_stdc_has_single_bit(y)&&__builtin_stdc_leading_ones(x)==0\n"
                "&&__builtin_stdc_leading_zeros(x)==1&&__builtin_stdc_trailing_ones(x)==1\n"
                "&&__builtin_stdc_trailing_zeros(x)==0&&__builtin_stdc_rotate_right(x,8)==x;}\n"
                "unsigned char x=1; int n=0; unsigned char r=__builtin_stdc_rotate_left(x++,++n);\n"
                "return a==7&&b==128&&c==((unsigned __int128)1<<127)&&f(65,(unsigned __int128)1<<100)\n"
                "&&r==2&&x==2&&n==1&&__builtin_clzg((unsigned char)0,-7)==-7\n"
                "&&__builtin_ctzg((unsigned __int128)0,123)==123;\n"),
            .exit_code = 1,
        },
        {
            "fixed bit builtins and generic zero boundaries", __LINE__,
            SVI("int f(int x,long l,long long ll){return __builtin_ffs(x)==4\n"
                "&&__builtin_ffsl(l)==4&&__builtin_ffsll(ll)==4\n"
                "&&__builtin_clrsb(x)==sizeof(int)*8-5&&__builtin_clrsbl(l)==sizeof(long)*8-5\n"
                "&&__builtin_clrsbll(ll)==sizeof(long long)*8-5\n"
                "&&__builtin_parity(x)==1&&__builtin_parityl(l)==1&&__builtin_parityll(ll)==1;}\n"
                "static int a=__builtin_ffs(0),b=__builtin_clrsb(-1),c=__builtin_parity(3);\n"
                "unsigned char z=0,ones=255; unsigned __int128 wide=~(unsigned __int128)0;\n"
                "return f(8,8,8)&&a==0&&b==sizeof(int)*8-1&&c==0\n"
                "&&__builtin_ffsg((signed char)0)==0&&__builtin_popcountg(wide)==128\n"
                "&&__builtin_stdc_bit_ceil(z)==1&&__builtin_stdc_bit_floor(z)==0\n"
                "&&__builtin_stdc_bit_width(z)==0&&__builtin_stdc_leading_zeros(z)==8\n"
                "&&__builtin_stdc_trailing_zeros(z)==8&&__builtin_stdc_leading_ones(ones)==8\n"
                "&&__builtin_stdc_trailing_ones(ones)==8&&__builtin_stdc_first_leading_one(z)==0\n"
                "&&__builtin_stdc_first_trailing_one(z)==0&&__builtin_stdc_first_leading_zero(ones)==0\n"
                "&&__builtin_stdc_first_trailing_zero(ones)==0&&!__builtin_stdc_has_single_bit(z)\n"
                "&&__builtin_stdc_rotate_left(ones,0)==ones;\n"),
            .exit_code = 1,
        },
        {
            "bit builtin word boundaries and rotations agree with integer arithmetic", __LINE__,
            SVI("_Static_assert(__builtin_clzg((unsigned __int128)1<<64)==63);\n"
                "_Static_assert(__builtin_ctzg((unsigned __int128)1<<64)==64);\n"
                "_Static_assert(__builtin_stdc_bit_ceil(((unsigned __int128)1<<64)+1)==((unsigned __int128)1<<65));\n"
                "_Static_assert(__builtin_stdc_rotate_left((unsigned __int128)1,64)==((unsigned __int128)1<<64));\n"
                "int f(unsigned __int128 x){\n"
                "for(unsigned n=0;n<256;n++){unsigned k=n%128;\n"
                "unsigned __int128 l=k?(x<<k)|(x>>(128-k)):x;\n"
                "unsigned __int128 r=k?(x>>k)|(x<<(128-k)):x;\n"
                "if(__builtin_stdc_rotate_left(x,n)!=l||__builtin_stdc_rotate_right(x,n)!=r)return 0;}\n"
                "return __builtin_popcountg(x)==64&&__builtin_clzg(x)==0&&__builtin_ctzg(x)==0\n"
                "&&__builtin_stdc_leading_ones(x)==1&&__builtin_stdc_trailing_ones(x)==1;}\n"
                "int g(unsigned long long x){for(unsigned n=0;n<128;n++){unsigned k=n%64;\n"
                "unsigned long long l=k?(x<<k)|(x>>(64-k)):x;\n"
                "unsigned long long r=k?(x>>k)|(x<<(64-k)):x;\n"
                "if(__builtin_stdc_rotate_left(x,n)!=l||__builtin_stdc_rotate_right(x,n)!=r)return 0;}return 1;}\n"
                "unsigned __int128 halves=(unsigned __int128)0xaaaaaaaaaaaaaaaaULL<<64|0x5555555555555555ULL;\n"
                "unsigned __int128 middle=(unsigned __int128)1<<64;\n"
                "return f(halves)&&g(0x8123456789abcdefULL)\n"
                "&&__builtin_stdc_bit_ceil(middle-1)==middle&&__builtin_stdc_bit_ceil(middle+1)==middle*2\n"
                "&&__builtin_stdc_bit_floor(middle-1)==middle/2&&__builtin_stdc_bit_floor(middle+1)==middle\n"
                "&&__builtin_clrsbg((__int128)(~(unsigned __int128)0<<64))==63;\n"),
            .exit_code = 1,
        },
        {
            "fixed bit counts share generic operations after parameter conversion", __LINE__,
            SVI("int f(unsigned __int128 x){unsigned a=x; unsigned long b=x; unsigned long long c=x;\n"
                "return __builtin_popcount(x)==__builtin_popcountg(a)\n"
                "&&__builtin_popcountl(x)==__builtin_popcountg(b)&&__builtin_popcountll(x)==__builtin_popcountg(c)\n"
                "&&__builtin_clz(x)==__builtin_clzg(a)&&__builtin_clzl(x)==__builtin_clzg(b)\n"
                "&&__builtin_clzll(x)==__builtin_clzg(c)&&__builtin_ctz(x)==__builtin_ctzg(a)\n"
                "&&__builtin_ctzl(x)==__builtin_ctzg(b)&&__builtin_ctzll(x)==__builtin_ctzg(c);}\n"
                "_Static_assert(__builtin_popcount(-1)==sizeof(unsigned)*8);\n"
                "_Static_assert(__builtin_clz((unsigned char)1)==sizeof(unsigned)*8-1);\n"
                "int i=0; __builtin_popcount(i++); __builtin_clzl(++i); __builtin_ctzll(++i);\n"
                "return f(((unsigned __int128)1<<100)|0x100000010ULL)&&i==3;\n"),
            .exit_code = 1,
        },
        {
            "constant bit counts use builtin parameter widths", __LINE__,
            SVI("constexpr int a=__builtin_clz((unsigned char)1);\n"
                "constexpr int b=__builtin_clzll(1); constexpr int c=__builtin_clzl(1);\n"
                "constexpr int d=__builtin_popcount(-1); constexpr int e=__builtin_clz(-1);\n"
                "static int values[]={a,b,c,d,e};\n"
                "return values[0]==sizeof(unsigned)*8-1&&values[1]==sizeof(unsigned long long)*8-1\n"
                "&&values[2]==sizeof(unsigned long)*8-1&&values[3]==sizeof(unsigned)*8&&values[4]==0;\n"),
            .exit_code = 1,
        },
        {
            "runtime bit counts convert arguments before counting", __LINE__,
            SVI("int f(unsigned char c,long long x,signed char n){\n"
                "return __builtin_clzll(c)==sizeof(unsigned long long)*8-1\n"
                "&&__builtin_popcount(x)==1&&__builtin_ctz(x)==0\n"
                "&&__builtin_popcountll(n)==sizeof(unsigned long long)*8;}\n"
                "return f(1,0x100000001LL,-1);\n"),
            .exit_code = 1,
        },
        {
            "bit counts accept enum and wide integer conversions", __LINE__,
            SVI("enum E:unsigned __int128 {BITS=((unsigned __int128)1<<100)|7};\n"
                "constexpr int n=__builtin_popcount(BITS);\n"
                "constexpr int z=__builtin_clzll((unsigned __int128)1<<32);\n"
                "int f(unsigned __int128 x){return __builtin_popcountll(x)==3&&__builtin_ctzll(x)==0;}\n"
                "return n==3&&z==sizeof(unsigned long long)*8-33&&f(BITS);\n"),
            .exit_code = 1,
        },
        {
            "bit counts apply ordinary floating argument conversion", __LINE__,
            SVI("constexpr int n=__builtin_popcount(7.75);\n"
                "int f(double x){return __builtin_clzll(x);} return n==3&&f(1.5)==sizeof(unsigned long long)*8-1;\n"),
            .exit_code = 1,
        },
        {
            "static shifts validate full counts and skip unselected branches", __LINE__,
            SVI("static unsigned a=1U<<31ULL;\n"
                "static unsigned b=0x80000000U>>(unsigned __int128)31;\n"
                "static unsigned long long c=1ULL<<63U;\n"
                "static unsigned __int128 d=(unsigned __int128)1<<127ULL;\n"
                "static unsigned e=1?7:(1U<<0x100000000ULL);\n"
                "static int f=0&&(1U<<0x100000000ULL);\n"
                "return a==0x80000000U&&b==1&&c==0x8000000000000000ULL&&(d>>127U)==1&&e==7&&f==0;\n"),
            .exit_code = 1,
        },
        {
            "nested union defaults retain numeric storage in static copies", __LINE__,
            SVI("constexpr union O {struct S {union U {void* p; struct B {unsigned low, high;} b;} u;} s; unsigned long bits;} v={.s.u.b.low=7};\n"
                "static unsigned long bits=v.bits; static struct S copy=v.s;\n"
                "return bits==7&&copy.u.b.low==7&&copy.u.b.high==0;\n"),
            .exit_code = 1,
        },
        {
            "constant enum casts preserve width and sign in static storage", __LINE__,
            SVI("enum E:unsigned char {ZERO}; constexpr enum E x={(enum E)256};\n"
                "enum S:signed char {S_ZERO}; constexpr enum S y={(enum S)255};\n"
                "static unsigned a=x; static int b=y;\n"
                "enum W:__int128 {NEG=-1}; constexpr enum W w={NEG};\n"
                "static __int128 c=w; return a==0&&b==-1&&c==-1;\n"),
            .exit_code = 1,
        },
        {
            "constant pointer negation and braced symbolic addresses", __LINE__,
            SVI("static int target=7; constexpr int* zero={nullptr};\n"
                "constexpr int* symbol={&target}; static int a=!zero;\n"
                "static int b=!symbol; static const int* copy=symbol;\n"
                "return a==1&&b==0&&copy==&target&&*copy==7;\n"),
            .exit_code = 1,
        },
        {
            "constant flexible member views copy only actual storage", __LINE__,
            SVI("struct I {double n; char c; char tail[];}; struct J {double n; char c;};\n"
                "constexpr union U {struct I i; struct J j;} u={.i={3,7}};\n"
                "constexpr struct J x=u.j; static struct J copy=x;\n"
                "static unsigned char padding=((const unsigned char*)&u.i)[15];\n"
                "return copy.n==3&&copy.c==7&&padding==0;\n"),
            .exit_code = 1,
        },
        {
            "wide bitfields retain all static and constexpr bits", __LINE__,
            SVI("struct B {unsigned __int128 x:128;};\n"
                "constexpr struct B b={.x=~(unsigned __int128)0};\n"
                "static struct B copy=b; static unsigned __int128 value=b.x;\n"
                "struct B local={.x=value}; local.x=copy.x;\n"
                "return sizeof(copy)==16&&copy.x==value&&local.x==value;\n"),
            .exit_code = 1,
        },
        {
            "numeric reinterpretation: typed copies retain omitted opaque values", __LINE__,
            SVI("constexpr union U {struct P {void* p; _Type t;} p; struct B {void* p; _Type t;} b;} u={.p={}};\n"
                "constexpr struct B value=u.b; static struct B copy=value;\n"
                "_Static_assert(value.p==nullptr&&value.t.is_invalid);\n"
                "return copy.p==nullptr&&copy.t.is_invalid;\n"),
            .exit_code = 1,
        },
        {
            "numeric reinterpretation: overwritten metadata no longer supplies bytes", __LINE__,
            SVI("constexpr union U {_Type t; unsigned long bits; struct B {unsigned low;} b;}\n"
                "u={.t=int,.b.low=7}; static struct B copy=u.b;\n"
                "_Static_assert(u.b.low==7); constexpr union U whole={.t=int,.bits=9};\n"
                "_Static_assert(whole.bits==9); return copy.low==7;\n"),
            .exit_code = 1,
        },
        {
            "numeric reinterpretation: overwritten bits do not read opaque metadata", __LINE__,
            SVI("constexpr union U {_Type t; unsigned bit:1;} u={.t=int,.bit=1};\n"
                "_Static_assert(u.bit==1); static unsigned bit=u.bit; return bit==1;\n"),
            .exit_code = 1,
        },
        {
            "numeric reinterpretation: Any views preserve typed metadata and numeric payloads", __LINE__,
            SVI("constexpr union U {_Any a; struct B {_Type tag; int value;} b;} u={.a=7};\n"
                "_Static_assert(u.b.tag==int&&u.b.value==7); static struct B copy=u.b;\n"
                "return copy.tag==int&&copy.value==7;\n"),
            .exit_code = 1,
        },
        {
            "numeric reinterpretation: typed copies retain absolute pointer values", __LINE__,
            SVI("constexpr union U {int* p; struct B {int* p;} b;} u={.p=(int*)7};\n"
                "constexpr struct B value=u.b; static struct B copy=value;\n"
                "_Static_assert(value.p==(int*)7); return copy.p==(int*)7;\n"),
            .exit_code = 1,
        },
        {
            "constant slices: evaluated aggregates lower count and relocated data", __LINE__,
            SVI("static int a[4]={3,5,7,9}; constexpr const int part[:]=a[1:4];\n"
                "static const int tail[:]=part[1:]; static const int head[:]=part[:1];\n"
                "static const int empty[:]=a[4:4]; static const int whole[:]=a;\n"
                "return tail.count==2&&tail.data==a+2&&tail[1]==9&&head[0]==5\n"
                "&&empty.count==0&&empty.data==a+4&&whole.count==4&&whole.data==a;\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: direct calls pack slices including empty packs", __LINE__,
            SVI("int sum(int args...){int r=0; for(size_t i=0;i<_Countof args;i++) r+=args[i]; return r;}\n"
                "return sum(1,2,3)==6&&sum(1,2,3,4,3)==13&&sum()==0;\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: fixed and named arguments with element conversions", __LINE__,
            SVI("long sum(int start, long args...){long r=start; for(size_t i=0;i<args.count;i++) r+=args[i]; return r;}\n"
                "return sum(4,1,2)==7&&sum(.start=5,2,3)==10&&sum(9)==9;\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: named parameters after packs", __LINE__,
            SVI("int sum(int start=10, int args..., int scale=2, int extra=0){int r=start; for(size_t i=0;i<args.count;i++)r+=args[i]; return r*scale+extra;}\n"
                "return sum()==20&&sum(1,2,3)==12&&sum(1,2,3,.scale=3,.extra=4)==22\n"
                "&&sum(.scale=3,1,2,3)==18&&sum(.args=(int[]){2,3},.start=1,.extra=4)==16\n"
                "&&sum([2]=3,1,2,3)==18;\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: Any pack followed by a default output parameter", __LINE__,
            SVI("struct File {int id;}; struct File out={1},err={2};\n"
                "int print(_Any args..., struct File* file=&out){return args.count==2&&args[1].as(int)==1?file->id:0;}\n"
                "return print(\"hello\",1)==1&&print(\"hello\",1,.file=&err)==2;\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: required named parameters and function pointers", __LINE__,
            SVI("int f(int args..., int scale=2, int extra){return (int)args.count*scale+extra;}\n"
                "int (*p)(int[:],int,int)=f;\n"
                "return f(7,8,.extra=3)==7&&f(.extra=3)==3&&p((int[]){1,2},4,5)==13;\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: trailing parameter defaults survive redeclarations", __LINE__,
            SVI("int f(int values...,int scale=2);\n"
                "int f(int args...,int factor){return (int)args.count*factor;}\n"
                "int f(int renamed...,int renamed_factor);\n"
                "return f(1,2)==4&&f(1,2,.factor=3)==6&&f(.args=(int[]){1,2})==4;\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: Any boxing preserves float and aggregate types", __LINE__,
            SVI("struct S {int x;};\n"
                "int check(_Any args...){return args.count==4&&args[0].type==int&&args[0].as(int)==1\n"
                "&&args[1].type==float&&args[1].as(float)==2.f\n"
                "&&args[2].type==double&&args[2].as(double)==3.\n"
                "&&args[3].type==struct S&&args[3].as(struct S).x==4;}\n"
                "return check(1,2.f,3.,(struct S){4});\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: function pointers take ordinary slices", __LINE__,
            SVI("int sum(int args...){int r=0; for(size_t i=0;i<args.count;i++) r+=args[i]; return r;}\n"
                "int (*p)(int[:])=sum; int a[]={2,3};\n"
                "return p(a)==5&&p((int[]){4,5}[:])==9&&typeof(sum)==typeof(p).pointee;\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: arguments evaluated once and storage survives call", __LINE__,
            SVI("int calls; int next(void){return ++calls;}\n"
                "int retain(int args...)[:]{return args;}\n"
                "int test(void){int s[:]=retain(next(),next()); return calls==2&&s.count==2&&s[0]+s[1]==3;}\n"
                "return test();\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: explicit slices, aggregates and nested packs", __LINE__,
            SVI("struct S {int x,y;}; int sum(struct S args...){int r=0; for(size_t i=0;i<args.count;i++) r+=args[i].x+args[i].y; return r;}\n"
                "struct S a[]={{1,2},{3,4}};\n"
                "int nested(int args...){return args[0]+args[1];}\n"
                "return sum({1,2},{3,4})==10&&sum(.args=a)==10&&sum([0]=a)==10\n"
                "&&nested(nested(1,2),nested(3,4))==10;\n"),
            .exit_code = 1,
        },
        {
            "typed varargs: prototypes and definitions retain pack metadata", __LINE__,
            SVI("int sum(int values...); int sum(int args...){int r=0; for(size_t i=0;i<args.count;i++) r+=args[i]; return r;}\n"
                "int sum(int renamed...); int sum();\n"
                "return sum(2,3)==5&&sum(.args=(int[]){7})==7;\n"),
            .exit_code = 1,
        },
        {
            "constant slices: runtime bounds retain their effects", __LINE__,
            SVI("int calls; int low(void){calls++;return 1;} static int a[4]={3,5,7,9};\n"
                "struct S {int part[:]; int marker;} s={a[low():3],7};\n"
                "return calls==1&&s.part.count==2&&s.part.data==a+1&&s.marker==7;\n"),
            .exit_code = 1,
        },
        {
            "constant slices: runtime numeric bases retain their effects", __LINE__,
            SVI("int calls; int* base(void){calls++;return (int*)16;}\n"
                "struct S {int part[:]; int marker;} s={base()[1:3],7};\n"
                "return calls==1&&s.part.count==2&&s.part.data==(int*)20&&s.marker==7;\n"),
            .exit_code = 1,
        },
        {
            "static subobject ranges: overwritten arithmetic is not evaluated", __LINE__,
            SVI("if(0){struct I {int a,b;}; const struct S {struct I i; int unrelated;}\n"
                "s={{1/0,2},1/0,.i.a=9}; static struct I copy=s.i;\n"
                "if(copy.a!=9||copy.b!=2) return 0;} return 1;\n"),
            .exit_code = 1,
        },
        {
            "static subobject ranges: partially overwritten numeric storage", __LINE__,
            SVI("constexpr union U {unsigned long raw; struct B {unsigned a:3; unsigned b:5;} b;}\n"
                "u={.raw=255,.b.a=2}; static unsigned long copy=u.raw;\n"
                "return copy==250;\n"),
            .exit_code = 1,
        },
        {
            "static subobject ranges: Any payload retains symbols and metadata", __LINE__,
            SVI("static int target; struct P {int* p;}; struct M {_Type t;};\n"
                "constexpr _Any pointer=(struct P){&target}, metadata=(struct M){long};\n"
                "static struct P copy=pointer.as(struct P); static struct M tag=metadata.as(struct M);\n"
                "return tag.t==long&&copy.p==&target;\n"),
            .exit_code = 1,
        },
        {
            "static subobject ranges: unrelated arithmetic is not evaluated", __LINE__,
            SVI("if(0){const struct S {int value; int unrelated;} s={7,1/0};\n"
                "static int copy=s.value; if(copy!=7) return 0;} return 1;\n"),
            .exit_code = 1,
        },
        {
            "static subobject ranges: sparse array selection retains relocations", __LINE__,
            SVI("static int target; struct I {int n; int* p;};\n"
                "constexpr struct I a[16385]={[16384]={9,&target}};\n"
                "static struct I copy=a[16384]; return copy.n==9&&copy.p==&target;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: bitfield reads preserve static float rounding", __LINE__,
            SVI("struct B {unsigned n:25;};\n"
                "static const struct B bits={(unsigned)((16777216.f+1.f)-16777216.f)};\n"
                "static int direct=bits.n; static int indirect=(&bits)->n;\n"
                "return bits.n==0&&direct==0&&indirect==0;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: direct and indirect bitfield reads agree", __LINE__,
            SVI("struct B {unsigned a:3; int b:5;}; constexpr struct B bits[2]={{1,2},{5,-3}};\n"
                "constexpr const struct B* p=bits+1;\n"
                "static int direct=bits[1].b; static int indirect=p->b;\n"
                "static int sum=bits[1].b+p->b;\n"
                "return direct==-3&&indirect==-3&&sum==-6;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: repeated representation copies retain padding", __LINE__,
            SVI("struct I {unsigned char c; int n;};\n"
                "constexpr union U {unsigned char bytes[8]; struct I i;} u={.bytes={1,2,3,4,5,6,7,8}};\n"
                "constexpr struct I first=u.i; constexpr struct I copies[2]={first,first};\n"
                "constexpr const struct I* p=copies+1; constexpr struct I second=*p;\n"
                "static union U result={.i=second};\n"
                "return result.bytes[0]==1&&result.bytes[1]==2&&result.bytes[3]==4&&result.bytes[7]==8;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: recovered paths retain large and deep indices", __LINE__,
            SVI("struct I {int a,b;}; constexpr struct I a[1025]={[1024]={3,4}};\n"
                "constexpr const struct I* p=a+1024; constexpr struct I copy=p[0];\n"
                "constexpr int deep[1][1][1][1][1][1][2]={{{{{{{5,6}}}}}}};\n"
                "constexpr const int* q=&deep[0][0][0][0][0][0][1];\n"
                "_Static_assert(p->b==4&&copy.a==3&&q[-1]==5);\n"
                "static struct I stored=copy; static int x=q[0]; return stored.b==4&&x==6;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: pointer arrays preserve symbolic elements", __LINE__,
            SVI("static int target; constexpr int* a[1025]={[1024]=&target};\n"
                "constexpr int* const* p=a+1024; _Static_assert(p[0]==&target);\n"
                "static const int* q=p[0]; return q==&target;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: arrow bitfields retain width and sign", __LINE__,
            SVI("struct B {unsigned a:3; int b:5;}; constexpr struct B bits={5,-3};\n"
                "constexpr const struct B* p=&bits; _Static_assert(p->a==5&&p->b==-3);\n"
                "static int b=p->b; static unsigned a=p->a; return a==5&&b==-3;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: pointer casts read numeric representation", __LINE__,
            SVI("constexpr unsigned x=0x04030201; constexpr const unsigned char* p=(const unsigned char*)&x;\n"
                "_Static_assert(p[0]==1&&p[3]==4); static unsigned char b=p[2]; return b==3;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: pointer and slice reads agree with runtime", __LINE__,
            SVI("constexpr int a[4]={3,5,7,9}; constexpr const int* p=a+2;\n"
                "constexpr const int part[:]=a[1:3]; constexpr int x=p[-1]+part[1]+*p;\n"
                "_Static_assert(x==19); static int copy=x; return copy==19&&part[0]==5;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: pointer selected aggregates preserve metadata and relocations", __LINE__,
            SVI("static int target; struct I {_Type t; int* p; int n;};\n"
                "constexpr struct I a[2]={{int,&target,3},{long,&target,7}};\n"
                "constexpr const struct I* p=a+1; constexpr struct I copy=p[0];\n"
                "_Static_assert(p->n==7&&copy.t==long&&copy.p==&target);\n"
                "static struct I stored=copy; return stored.n==7&&stored.t==long&&stored.p==&target;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: reflection names survive union views", __LINE__,
            SVI("struct S {int hello;};\n"
                "constexpr union U {struct __builtin_Field a,b;} u={.a=(struct S).field(0)};\n"
                "constexpr struct __builtin_Field f=(struct S).field(u.b.name);\n"
                "_Static_assert(f.type==int); return f.name.count==5&&f.offset==0;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: union padding survives static and runtime copies", __LINE__,
            SVI("constexpr union U {unsigned char bytes[8]; struct I {unsigned char c; int n;} i;}\n"
                "u={.bytes={1,2,3,4,5,6,7,8}}; constexpr struct I x=u.i;\n"
                "constexpr union U alias={.i=x}; static union U copy=alias; union U local={.i=x};\n"
                "_Static_assert(alias.bytes[1]==2&&alias.bytes[2]==3&&alias.bytes[3]==4);\n"
                "return copy.bytes[7]==8&&local.bytes[1]==2&&local.bytes[2]==3&&local.bytes[3]==4&&local.bytes[7]==8;\n"),
            .exit_code = 1,
        },
        {
            "constant object views: comma preserves metadata and symbolic siblings", __LINE__,
            SVI("static int target; struct I {_Type t; int* p;};\n"
                "constexpr union U {struct I a,b;} u={.a={int,&target}};\n"
                "constexpr struct I x=(0,u).b; static struct I copy=x;\n"
                "_Static_assert(x.t==int&&x.p==&target); return copy.t==int&&copy.p==&target;\n"),
            .exit_code = 1,
        },
        {
            "initializer paths: partial templates preserve adjacent bitfield bytes", __LINE__,
            SVI("struct S {unsigned a:3,b:5; int c,d,e,f,x;};\n"
                "int test(unsigned n){struct S s={.a=5,.b=n,.c=1,.d=2,.e=3,.f=4,.x=n};\n"
                "return s.a==5&&s.b==n&&s.c==1&&s.d==2&&s.e==3&&s.f==4&&s.x==n;}\n"
                "return test(7)&&test(17);\n"),
            .exit_code = 1,
        },
        {
            "initializer paths: selected aggregate preserves updates and symbolic siblings", __LINE__,
            SVI("static int target; struct I {int a,b; int* p;}; struct S {struct I x; int z;};\n"
                "constexpr struct S s={1,2,&target,3,.x.b=7}; constexpr struct I x=s.x;\n"
                "_Static_assert(x.a==1&&x.b==7&&x.p==&target); static struct I copy=x;\n"
                "return copy.a==1&&copy.b==7&&copy.p==&target;\n"),
            .exit_code = 1,
        },
        {
            "field paths: extended members, base conversions and method receivers", __LINE__,
            SVI("struct L {int value; int bump(_Self* s){return ++s.value;}};\n"
                "struct S {int pad; struct {int pad2; struct {struct {struct {struct {struct {struct L;};};};};};};};\n"
                "static struct S global={.value=7}; static int *p=&global.value;\n"
                "int read(struct L* l){return l.value;} struct S local={.value=4};\n"
                "int r=local.bump(); return r==5&&read(&local)==5&&*p==7;\n"),
            .exit_code = 1,
        },
        {
            "field paths: nested bitfield layout in constexpr and runtime storage", __LINE__,
            SVI("struct S {int pad; union {unsigned raw; struct {unsigned a:3; signed b:5;};};};\n"
                "constexpr struct S c={.a=5,.b=-3}; _Static_assert(c.a==5&&c.b==-3);\n"
                "static struct S copy=c; struct S s={.a=2,.b=-4}; s.a+=3; ++s.b;\n"
                "return copy.a==5&&copy.b==-3&&s.a==5&&s.b==-3;\n"),
            .exit_code = 1,
        },
        {
            "initializer paths: whole-subobject boundaries survive evaluation and lowering", __LINE__,
            SVI("struct S {struct I {int a,b;} x; int z;};\n"
                "constexpr struct S s={.x.b=7,.x={.a=1},.z=9};\n"
                "_Static_assert(s.x.a==1&&s.x.b==0&&s.z==9); static struct S copy=s;\n"
                "int f(int n){struct S r={.x={.a=n},.x.b=n+1,.x={},.z=n}; return r.x.a==0&&r.x.b==0&&r.z==n;}\n"
                "return copy.x.a==1&&copy.x.b==0&&copy.z==9&&f(3);\n"),
            .exit_code = 1,
        },
        {
            "initializer paths: extended residual path survives partial template", __LINE__,
            SVI("int f(int x){int a[1025]={[0]=1,[1]=2,[2]=3,[3]=4,[1024]=x}; return a[0]+a[1]+a[2]+a[3]+a[1024];}\nreturn f(7)==17;\n"),
            .exit_code = 1, .expect_template = 1, .expect_runtime_stores = 1,
        },
        {
            "initializer paths: nested template composes extended paths", __LINE__,
            SVI("int f(int x){int a[1][1][1][1][1][1][5]={{{{{{{1,2,x,4,5}}}}}}}; return a[0][0][0][0][0][0][2];}\nreturn f(7)==7;\n"),
            .exit_code = 1, .expect_template = 1, .expect_runtime_stores = 1,
        },
        {
            "constexpr union: slice element addresses retain array identity", __LINE__,
            SVI("static int a[4]; constexpr union U {int s[:];} u={.s=a[1:3]};\n"
                "_Static_assert(&u.s[1]==&a[2]&&&u.s[2]-&u.s[0]==2);\n"
                "static const int* p=&u.s[1]; return p==&a[2];\n"),
            .exit_code = 1,
        },
        {
            "static reflection: aggregate results and resliced names", __LINE__,
            SVI("struct S {int x;}; constexpr _Type T=struct S;\n"
                "static const char name[:]=T.name; static const char tail[:]=T.name[1:];\n"
                "static struct __builtin_Field field=T.field(0);\n"
                "return name.count==8&&tail.count==7&&field.name.count==1&&field.name[0]=='x'&&field.type==int;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: reflection slice retains literal identity", __LINE__,
            SVI("constexpr union U {const char name[:]; struct V {unsigned long count; const char* p;} v;} u={.name=int.name};\n"
                "_Static_assert(u.v.count==3&&u.v.p==u.name.data);\n"
                "static const char* p=u.v.p; return p[0]=='i'&&p[1]=='n'&&p[2]=='t';\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: slice counts and pointers have constant views", __LINE__,
            SVI("static int a[3]; constexpr union U {int s[:]; struct V {unsigned long count; int* p;} v;} u={.s=a[1:3]};\n"
                "_Static_assert(u.v.count==2&&u.v.p==&a[1]);\n"
                "static unsigned long count=u.v.count; static int* p=u.v.p; return count==2&&p==&a[1];\n"),
            .exit_code = 1,
        },
        {
            "constexpr slices: count and data survive reslicing", __LINE__,
            SVI("static int a[4]; constexpr int s[:]=(a[:])[1:3];\n"
                "_Static_assert(s.count==2&&s.data==&a[1]);\n"
                "constexpr int t[:]=s[1:]; _Static_assert(t.count==1&&t.data==&a[2]);\n"
                "static unsigned long count=s.count; static const int* p=s.data; return count==2&&p==&a[1];\n"),
            .exit_code = 1,
        },
        {
            "constexpr slices: absolute pointer and array cast fields", __LINE__,
            SVI("static int a[3]; constexpr int s[:]=(int[:])a;\n"
                "_Static_assert(s.count==3&&s.data==a);\n"
                "constexpr int n[:]=((int*)6)[:2]; _Static_assert(n.count==2&&n.data==(int*)6);\n"
                "static const int* p=n.data; return p==(int*)6;\n"),
            .exit_code = 1,
        },
        {
            "static union: complete overwrite replaces a slice pointer", __LINE__,
            SVI("constexpr union U {int s[:]; struct V {unsigned long count; int* p;} v;} u={.s=((int*)6)[:2],.v.p=(int*)7};\n"
                "static struct V v=u.v; return v.count==2&&v.p==(int*)7;\n"),
            .exit_code = 1,
        },
        {
            "static union: complete slice representation remains copyable", __LINE__,
            SVI("static int a[3]; constexpr union U {int s[:]; struct V {unsigned long count; int* p;} v;} u={.s=a[1:3]};\n"
                "static struct V v=u.v; return v.count==2&&v.p==&a[1];\n"),
            .exit_code = 1,
        },
        {
            "static union: large view accepts complete relocations", __LINE__,
            SVI("static int a,b; constexpr union U {struct A {int* p; unsigned long x; int* q;} a; struct B {int* p; unsigned x[2]; int* q;} b;} u={.a={&a,7,&b}};\n"
                "static struct B copy=u.b; return copy.p==&a&&copy.x[0]==7&&copy.x[1]==0&&copy.q==&b;\n"),
            .exit_code = 1,
        },
        {
            "static union: later writes make a clipped relocation concrete", __LINE__,
            SVI("static int a; constexpr union U {struct A {unsigned long x; int* p;} a; struct B {unsigned x[3];} b;} u={.a={7,&a},.b.x[2]=9};\n"
                "static struct B copy=u.b; return copy.x[0]==7&&copy.x[1]==0&&copy.x[2]==9;\n"),
            .exit_code = 1,
        },
        {
            "static union: containing overwrite replaces an earlier relocation", __LINE__,
            SVI("static int a; union U {struct A {unsigned long x; int* p;} a; struct B {unsigned x[3];} b;};\n"
                "constexpr struct H {union U u;} h={.u={.a={7,&a}},.u={.b={{3,4,5}}}};\n"
                "static struct B copy=h.u.b; return copy.x[0]==3&&copy.x[1]==4&&copy.x[2]==5;\n"),
            .exit_code = 1,
        },
        {
            "static union: packed aggregate preserves an unaligned relocation", __LINE__,
            SVI("static int a; struct __attribute__((packed)) A {char x; int* p;};\n"
                "constexpr union U {struct A a; char bytes[9];} u={.a={7,&a}};\n"
                "static struct A copy=u.a; return copy.x==7&&copy.p==&a;\n"),
            .exit_code = 1,
        },
        {
            "static union: selecting a bitfield does not copy its aggregate", __LINE__,
            SVI("static int a; constexpr union U {int* p; struct B {unsigned pad:3; int x:5;} b;} u={.p=&a,.b.x=-3,.b.pad=5};\n"
                "static int x=(0,u.b).x; static unsigned pad=(1?u.b:u.b).pad;\n"
                "return x==-3&&pad==5;\n"),
            .exit_code = 1,
        },
        {
            "static union: small numeric aggregate view remains copyable", __LINE__,
            SVI("constexpr union U {unsigned long raw; struct B {unsigned low;} b;} u={.raw=7};\n"
                "static struct B b=u.b; constexpr struct B c=u.b;\n"
                "_Static_assert(c.low==7); return b.low==7;\n"),
            .exit_code = 1,
        },
        {
            "static union: complete relocation survives aggregate views", __LINE__,
            SVI("static int a; constexpr union U {int* p; struct B {int* p;} b;} u={.p=&a};\n"
                "static struct B b=u.b; constexpr struct B c=u.b;\n"
                "_Static_assert(c.p==&a); return b.p==&a;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: high numeric pointer agrees with direct cast", __LINE__,
            SVI("constexpr union U {void* p; unsigned long bits;} u={.bits=~0ul};\n"
                "_Static_assert(u.p==(void*)~0ul); _Static_assert((void*)~0ul!=(void*)0);\n"
                "_Static_assert((void*)~0ul==(void*)(unsigned __int128)~0ul);\n"
                "static int same=u.p==(void*)~0ul; return same;\n"),
            .exit_code = 1,
        },
        {
            "union initializer: masked reads preserve remaining numeric bits", __LINE__,
            SVI("constexpr union U {unsigned long raw; struct B {unsigned a:3,b:5;} bits;} u={.raw=255,.bits.a=2};\n"
                "_Static_assert(u.bits.a==2&&u.bits.b==31); static unsigned b=u.bits.b;\n"
                "volatile unsigned v=255; union U local={.raw=v,.bits.a=2};\n"
                "return b==31&&local.bits.a==2&&local.bits.b==31;\n"),
            .exit_code = 1,
        },
        {
            "static union: floating views reinterpret numeric storage", __LINE__,
            SVI("constexpr union U {unsigned long bits; double d;} u={.bits=0x3ff0000000000000ul};\n"
                "static const double d=u.d; static union U copy={.d=d}; return d==1.0&&copy.bits==u.bits;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: bit masks survive nesting and Any views", __LINE__,
            SVI("static int a; union U {int* p; struct B {unsigned pad:3; int x:5;} b;};\n"
                "constexpr struct H {union U u[2];} h={.u[1]={.p=&a,.b.x=-3,.b.pad=5}};\n"
                "_Static_assert(h.u[1].b.x==-3&&h.u[1].b.pad==5);\n"
                "constexpr _Any boxed=h.u[1]; _Static_assert(boxed.as(union U).b.x==-3);\n"
                "static int x=boxed.as(union U).b.x; static unsigned pad=h.u[1].b.pad;\n"
                "return x==-3&&pad==5;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: overwritten bitfield does not read older relocation", __LINE__,
            SVI("static int a; constexpr union U {int* p; unsigned bit:1;} u={.p=&a,.bit=1};\n"
                "_Static_assert(u.bit==1); static unsigned bit=u.bit; return bit==1;\n"),
            .exit_code = 1,
        },
        {
            "union initializer: partial numeric overwrite forms a pointer", __LINE__,
            SVI("constexpr union U {int* p; unsigned long bits; unsigned bit:1;} u={.bits=6,.bit=1};\n"
                "_Static_assert(u.p==(int*)7); static int* p=u.p; return p==(int*)7;\n"),
            .exit_code = 1,
        },
        {
            "union initializer: positional value followed by designator", __LINE__,
            SVI("constexpr union U {int x; long y;} u={3,.y=7};\n"
                "_Static_assert(u.y==7); static union U s={3,.y=9};\n"
                "union U local={3,.y=11}; return u.y==7&&s.y==9&&local.y==11;\n"),
            .exit_code = 1,
        },
        {
            "union initializer: nested designators retain adjacent fields", __LINE__,
            SVI("constexpr union U {struct S {int x,y;} s; long bits;} u={.s.x=3,.s.y=7,.s.x=5};\n"
                "_Static_assert(u.s.x==5&&u.s.y==7); static union U copy=u;\n"
                "union U local={.s.x=3,.s.y=7,.s.x=5}; return copy.s.x==5&&copy.s.y==7&&local.s.y==7;\n"),
            .exit_code = 1,
        },
        {
            "union initializer: final pointer replaces earlier relocation", __LINE__,
            SVI("static int a,b; constexpr union U {int* p; int* q;} u={.p=&a,.q=&b};\n"
                "_Static_assert(u.p==&b); static union U copy=u; static int* p=u.p;\n"
                "return copy.p==&b&&p==&b;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: genuine type metadata survives aggregate views", __LINE__,
            SVI("struct S {int a,b;}; constexpr union U {_Type t; unsigned long bits;} u={.t=struct S};\n"
                "_Static_assert(u.t.sizeof_==sizeof(struct S)); static _Type t=u.t;\n"
                "constexpr struct H {_Type types[2];} h={{int,struct S}};\n"
                "_Static_assert(h.types[1].sizeof_==sizeof(struct S));\n"
                "constexpr _Any boxed=u.t; _Static_assert(boxed.type==_Type);\n"
                "_Static_assert(boxed.as(_Type).sizeof_==sizeof(struct S)); return t==struct S;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: zero metadata and final type overwrite", __LINE__,
            SVI("constexpr union U {_Type t; unsigned long bits;} zero={.bits=0};\n"
                "_Static_assert(zero.t.is_invalid); static _Type none=zero.t;\n"
                "constexpr union U u={.bits=4096,.t=int}; _Static_assert(u.t==int);\n"
                "constexpr union U v={.t=int,.bits=0}; _Static_assert(v.t.is_invalid);\n"
                "constexpr _Any empty={}; _Static_assert(empty.type.is_invalid);\n"
                "return none.is_invalid&&u.t==int;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: absolute pointer casts preserve pointer width", __LINE__,
            SVI("constexpr union U {void* p; unsigned long bits;} u={.bits=~0ul};\n"
                "_Static_assert((unsigned long)u.p==~0ul);\n"
                "_Static_assert((unsigned __int128)u.p==(unsigned __int128)~0ul);\n"
                "_Static_assert((unsigned long)(void*)-1==~0ul); return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: explicit pointer conversion uses the requested unsigned type", __LINE__,
            SVI("constexpr union U {void* p; unsigned long bits;} u={.bits=(unsigned long)(void*)-1};\n"
                "_Static_assert(u.bits>1ul); _Static_assert(u.bits==~0ul);\n"
                "static unsigned long bits=u.bits; return bits>1ul;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: signed and full width bitfields", __LINE__,
            SVI("constexpr union S {__int128 bits:100; unsigned __int128 raw;} s={.bits=(1ui128<<99)|7};\n"
                "_Static_assert(s.bits==-((1i128<<99)-7));\n"
                "constexpr union U {unsigned __int128 bits:128; unsigned __int128 raw;} u={.bits=~0ui128};\n"
                "_Static_assert(u.bits==~0ui128&&u.raw==~0ui128);\n"
                "static __int128 n=s.bits; return n==s.bits;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: bitfield crossing the 64 bit boundary", __LINE__,
            SVI("constexpr union U {struct B {unsigned __int128 a:65,b:63;} b; unsigned __int128 raw;} u={.b={1ui128<<64,7}};\n"
                "_Static_assert(u.b.a==(1ui128<<64)&&u.b.b==7);\n"
                "_Static_assert(u.raw==((7ui128<<65)|(1ui128<<64))); static unsigned __int128 n=u.b.b; return n==7;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: explicit numeric conversion produces numeric storage", __LINE__,
            SVI("constexpr union U {int* p; unsigned long bits;} u={.bits=(unsigned long)(int*)7};\n"
                "_Static_assert(u.bits==7); static unsigned long bits=u.bits; return bits==7;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: integer bytes form an absolute pointer", __LINE__,
            SVI("constexpr union U {void* p; unsigned long bits;} u={.bits=7};\n"
                "_Static_assert(u.p==(void*)7); static void* p=u.p; return p==(void*)7;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: float punning agrees with static reads", __LINE__,
            SVI("constexpr union U {float f; unsigned bits;} u={.f=1.0f};\n"
                "_Static_assert(u.bits==0x3f800000u); static unsigned bits=u.bits;\n"
                "constexpr union U v={.bits=0x3f800000u}; _Static_assert(v.f==1.0f); return bits==v.bits;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: nested aggregate views agree with static reads", __LINE__,
            SVI("constexpr union U {struct A {int x,y;} a; struct B {unsigned x,y;} b;} u={.a={3,4}};\n"
                "_Static_assert(u.b.x==3&&u.b.y==4); static struct B b=u.b;\n"
                "constexpr _Any boxed=u.b; _Static_assert(boxed.as(struct B).y==4); return b.x==3&&b.y==4;\n"),
            .exit_code = 1,
        },
        {
            "constexpr union: wide bitfield retains upper bits", __LINE__,
            SVI("constexpr union U {unsigned __int128 bits:100; unsigned __int128 raw;} u={.bits=(1ui128<<120)|(1ui128<<90)|7};\n"
                "_Static_assert(u.bits==((1ui128<<90)|7)); _Static_assert(u.raw==((1ui128<<90)|7));\n"
                "static unsigned __int128 bits=u.bits; return bits==u.raw;\n"),
            .exit_code = 1,
        },
        {
            "static Any: overwritten bitfields preserve adjacent fields", __LINE__,
            SVI("constexpr struct B {unsigned x:3,y:5;} b={.x=1,.y=17,.x=5};\n"
                "_Static_assert(b.x==5&&b.y==17); constexpr _Any boxed=b;\n"
                "_Static_assert(boxed.as(struct B).x==5&&boxed.as(struct B).y==17);\n"
                "static unsigned n=boxed.as(struct B).x+boxed.as(struct B).y; return n;\n"),
            .exit_code = 22,
        },
        {
            "static Any: later aggregate zeroes earlier bytes", __LINE__,
            SVI("constexpr struct S {struct P {int x,y;} p;} s={.p={3,4},.p={7}};\n"
                "_Static_assert(s.p.x==7&&s.p.y==0); constexpr _Any boxed=s.p;\n"
                "_Static_assert(boxed.as(struct P).x==7&&boxed.as(struct P).y==0);\n"
                "static struct P p=boxed.as(struct P); return p.x==7&&p.y==0;\n"),
            .exit_code = 1,
        },
        {
            "static Any: overwritten symbolic union payload agrees with constexpr", __LINE__,
            SVI("static int a; constexpr struct S {union U {int* p; unsigned long bits;} u;} s={.u={.p=&a},.u={.bits=0}};\n"
                "_Static_assert(s.u.bits==0); constexpr _Any boxed=s.u;\n"
                "_Static_assert(boxed.as(union U).bits==0);\n"
                "static unsigned long n=boxed.as(union U).bits; return n==0;\n"),
            .exit_code = 1,
        },
        {
            "static Any: overwritten boxed pointer becomes scalar bytes", __LINE__,
            SVI("static int a; constexpr struct S {_Any x;} s={.x=&a,.x=7};\n"
                "_Static_assert(s.x.as(int)==7); static int n=s.x.as(int); return n==7;\n"),
            .exit_code = 1,
        },
        {
            "static Any: overwritten pointer becomes null", __LINE__,
            SVI("static int a; constexpr struct S {int* p;} s={.p=&a,.p=nullptr};\n"
                "_Static_assert(s.p==nullptr); constexpr _Any boxed=s;\n"
                "_Static_assert(boxed.as(struct S).p==nullptr); return 1;\n"),
            .exit_code = 1,
        },
        {
            "static Any: scalar compound literal payload agrees with constexpr", __LINE__,
            SVI("struct P {int x,y;}; constexpr _Any boxed=(struct P){3,4};\n"
                "_Static_assert(boxed.as(struct P).x==3&&boxed.as(struct P).y==4);\n"
                "static struct P p=boxed.as(struct P); return p.x==3&&p.y==4;\n"),
            .exit_code = 1,
        },
        {
            "static Any: qualifiers and unselected views", __LINE__,
            SVI("const _Any boxed=7; static int x=boxed.as(const int);\n"
                "static long y=1?7:boxed.as(long); return x==7&&y==7;\n"),
            .exit_code = 1,
        },
        {
            "static Any: tag reflection folds into static scalars", __LINE__,
            SVI("const _Any boxed=7; static _Type tag=boxed.type;\n"
                "static unsigned long size=boxed.type.sizeof_; static int integer=boxed.type.is_integer;\n"
                "return tag==int&&size==sizeof(int)&&integer;\n"),
            .exit_code = 1,
        },
        {
            "static Any: payload addresses preserve storage identity", __LINE__,
            SVI("constexpr _Any boxed=7; static const void* p=boxed.payload;\n"
                "constexpr const void* q=boxed.payload;\n"
                "_Static_assert((const char*)q-(const char*)&boxed==8);\n"
                "return p==q&&*(const int*)p==7;\n"),
            .exit_code = 1,
        },
        {
            "static Any: scalar null enum and type payloads", __LINE__,
            SVI("enum E {e=42}; static _Any i=7,f=1.5f,n=nullptr,t=int,v=(enum E)e,empty={};\n"
                "return i.type==int&&i.as(int)==7&&f.type==float&&f.as(float)==1.5f\n"
                "&&n.type==typeof(nullptr)&&n.as(typeof(nullptr))==nullptr\n"
                "&&t.type==_Type&&t.as(_Type)==int&&v.type==enum E&&v.as(enum E)==e&&empty.type.is_invalid;\n"),
            .exit_code = 1,
        },
        {
            "static Any: pointer string and function relocations", __LINE__,
            SVI("static int a[2]={3,4}; int f(void){return 7;}\n"
                "static _Any p=&a[1],text=\"hello\"+1,fn=f;\n"
                "return p.type==int*&&p.as(int*)==&a[1]&&*p.as(int*)==4\n"
                "&&text.as(char*)[0]=='e'&&fn.as(int(*)(void))()==7;\n"),
            .exit_code = 1,
        },
        {
            "static Any: const copies nested arrays and overwritten relocations", __LINE__,
            SVI("static int a[2]={3,4}; const _Any boxed=&a[1];\n"
                "static _Any copy=boxed; static struct S {_Any x[3];} s={.x={boxed,7,\"hi\"},.x[0]={}};\n"
                "return copy.as(int*)==&a[1]&&s.x[0].type.is_invalid&&s.x[1].as(int)==7&&s.x[2].as(char*)[1]=='i';\n"),
            .exit_code = 1,
        },
        {
            "static Any: aggregate payload and constant view extraction", __LINE__,
            SVI("struct P {int* p;}; static int a[2]; constexpr _Any boxed=(struct P){&a[1]};\n"
                "static _Any copy=boxed; static int* p=boxed.as(struct P).p;\n"
                "constexpr long d=boxed.as(struct P).p-&a[0];\n"
                "return copy.type==struct P&&copy.as(struct P).p==p&&p==a+1&&d==1;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: qualified member addresses require no reads", __LINE__,
            SVI("struct V {volatile int a,b;} v; struct A {_Atomic(int) a,b;} a;\n"
                "constexpr long vd=&v.b-&v.a, ad=&a.b-&a.a;\n"
                "_Static_assert(vd==1&&ad==1);\n"
                "static volatile int* vp=&v.b; static _Atomic(int)* ap=&a.b;\n"
                "return vp==&v.b&&ap==&a.b&&vd==ad;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: Any payload retains address identity", __LINE__,
            SVI("static int a[4]; constexpr _Any box=&a[3];\n"
                "constexpr long d=box.as(int*)-&a[0]; _Static_assert(d==3);\n"
                "static int* p=box.as(int*); return p==a+3&&d==3;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: unsigned integer addresses retain all bits", __LINE__,
            SVI("static void* p=(void*)0xffffffffffffffffull;\n"
                "constexpr void* q=(void*)0xffffffffffffffffull;\n"
                "_Static_assert(q==(void*)-1); return p==q;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: wide integer addresses truncate to pointer bits", __LINE__,
            SVI("static void* p=(void*)((1ui128<<100)|0xffffffffffffffffui128);\n"
                "constexpr void* q=(void*)((1ui128<<100)|0xffffffffffffffffui128);\n"
                "_Static_assert(q==(void*)-1); return p==q;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: runtime template retains high address bits", __LINE__,
            SVI("int f(void){struct S {void* p; int a,b,c,d;} s={(void*)0xffffffffffffffffull,1,2,3,4};\n"
                "return s.p==(void*)-1&&s.a==1&&s.d==4;} return f();\n"),
            .exit_code = 1, .expect_template = 1,
        },
        {
            "symbolic pointers: nullptr aggregate members and array elements", __LINE__,
            SVI("constexpr struct S {typeof(nullptr) n;} s={nullptr};\n"
                "constexpr typeof(nullptr) a[2]={nullptr,nullptr};\n"
                "_Static_assert(s.n==nullptr); _Static_assert(a[1]==nullptr); return 1;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: unselected branches need no relocation", __LINE__,
            SVI("static int a,b;\n"
                "static long d=1?0:&b-&a;\n"
                "static int no=0&&(&a==&b); static int yes=1||(&b-&a);\n"
                "static int same=0?(&a==&b):1; return !d&&!no&&yes&&same;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: nested overwritten and zero aggregate members", __LINE__,
            SVI("static int a[4];\n"
                "constexpr struct P {struct {int* p;} v[2]; int* zero;} s={.v[0].p=&a[0],.v[1].p=&a[1],.v[1].p=&a[3]};\n"
                "constexpr long d=s.v[1].p-s.v[0].p; _Static_assert(d==3);\n"
                "_Static_assert(s.zero==nullptr); return d;\n"),
            .exit_code = 3,
        },
        {
            "symbolic pointers: numeric address ordering uses unsigned pointer bits", __LINE__,
            SVI("_Static_assert((void*)-1>(void*)0);\n"
                "_Static_assert(!((void*)-1<(void*)0));\n"
                "_Static_assert((void*)-1>=(void*)0);\n"
                "_Static_assert(!((void*)-1<=(void*)0));\n"
                "static int greater=(void*)-1>(void*)0; return greater;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: aggregate pointer members retain their symbols", __LINE__,
            SVI("struct S {int a,b;} s;\n"
                "constexpr struct P {int* p; int* q;} aliases={&s.a,&s.b};\n"
                "constexpr long d=aliases.q-aliases.p; _Static_assert(d==1);\n"
                "static int ordered=aliases.p<aliases.q; return ordered && d==1;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: pointer array elements retain their symbols", __LINE__,
            SVI("static int a[4]; constexpr int* aliases[2]={a,a+3};\n"
                "constexpr long d=aliases[1]-aliases[0]; _Static_assert(d==3);\n"
                "static long n=aliases[1]-aliases[0]; return n;\n"),
            .exit_code = 3,
        },
        {
            "symbolic pointers: all comparisons at equal offsets", __LINE__,
            SVI("union U {int a,b;} u;\n"
                "_Static_assert(&u.a==&u.b); _Static_assert(!(&u.a!=&u.b));\n"
                "_Static_assert(!(&u.a<&u.b)); _Static_assert(&u.a<=&u.b);\n"
                "_Static_assert(!(&u.a>&u.b)); _Static_assert(&u.a>=&u.b);\n"
                "static int flags[6]={&u.a==&u.b,&u.a!=&u.b,&u.a<&u.b,\n"
                "&u.a<=&u.b,&u.a>&u.b,&u.a>=&u.b};\n"
                "return flags[0] && !flags[1] && !flags[2] && flags[3] && !flags[4] && flags[5];\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: all comparisons at increasing offsets", __LINE__,
            SVI("extern struct S {int a,b;} s;\n"
                "_Static_assert(!(&s.a==&s.b)); _Static_assert(&s.a!=&s.b);\n"
                "_Static_assert(&s.a<&s.b); _Static_assert(&s.a<=&s.b);\n"
                "_Static_assert(!(&s.a>&s.b)); _Static_assert(!(&s.a>=&s.b));\n"
                "static int flags[6]={&s.a==&s.b,&s.a!=&s.b,&s.a<&s.b,\n"
                "&s.a<=&s.b,&s.a>&s.b,&s.a>=&s.b};\n"
                "return !flags[0] && flags[1] && flags[2] && flags[3] && !flags[4] && !flags[5];\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: all comparisons at decreasing offsets", __LINE__,
            SVI("extern struct S {int a,b;} s;\n"
                "_Static_assert(!(&s.b==&s.a)); _Static_assert(&s.b!=&s.a);\n"
                "_Static_assert(!(&s.b<&s.a)); _Static_assert(!(&s.b<=&s.a));\n"
                "_Static_assert(&s.b>&s.a); _Static_assert(&s.b>=&s.a);\n"
                "static int flags[6]={&s.b==&s.a,&s.b!=&s.a,&s.b<&s.a,\n"
                "&s.b<=&s.a,&s.b>&s.a,&s.b>=&s.a};\n"
                "return !flags[0] && flags[1] && !flags[2] && !flags[3] && flags[4] && flags[5];\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: const aliases arrow dereference and reversed addition", __LINE__,
            SVI("extern struct S {int a[4];} s;\n"
                "static struct S* const p=&s; static int* const q=&s.a[0];\n"
                "static long d=&p->a[3]-(2+q); static long zero=&*q-q;\n"
                "return d==1 && zero==0;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: string alias subtraction and comparisons", __LINE__,
            SVI("constexpr const char* p=\"hello\";\n"
                "constexpr long n=(p+5)-(p+1); _Static_assert(n==4);\n"
                "_Static_assert(p+5>p); static int same=p==p; return same && n==4;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: function alias equality", __LINE__,
            SVI("int target(void){return 7;} constexpr int (*p)(void)=target;\n"
                "_Static_assert(p==p); _Static_assert(!(p!=p));\n"
                "static int same=target==target; return same;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: nullptr and numeric pointer equality", __LINE__,
            SVI("_Static_assert((int*)nullptr==(int*)0);\n"
                "_Static_assert((int*)17==(int*)17); _Static_assert((int*)17!=(int*)18);\n"
                "static int same=(int*)nullptr==(int*)0; return same;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: known object and function addresses differ from null", __LINE__,
            SVI("extern int a; int target(void);\n"
                "_Static_assert(&a!=nullptr); _Static_assert(nullptr!=&a);\n"
                "_Static_assert(!(&a==nullptr)); _Static_assert(!(nullptr==&a));\n"
                "_Static_assert(target!=nullptr); _Static_assert(!(nullptr==target));\n"
                "static int object=&a!=nullptr; static int function=target!=nullptr;\n"
                "return object && function;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: ternary selects the evaluable address", __LINE__,
            SVI("extern int a[4]; extern int* unknown;\n"
                "constexpr long n=(1 ? &a[3] : &a[0])-&a[0];\n"
                "static long d=(0 ? unknown : a+2)-a; return n==3 && d==2;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: mutable alias reads keep runtime changes", __LINE__,
            SVI("int a[4]; int* p=a; p=a+3; return p-a;\n"),
            .exit_code = 3,
        },
        {
            "symbolic pointers: volatile alias reads keep runtime changes", __LINE__,
            SVI("int a[4]; int* volatile p=a; p=a+2; return p-a;\n"),
            .exit_code = 2,
        },
        {
            "symbolic pointers: atomic alias reads keep runtime changes", __LINE__,
            SVI("int a[4]; _Atomic(int*) p=a; p=a+1; return p-a;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: static differences and comparisons", __LINE__,
            SVI("struct S {int a,b; int x[4];}; static struct S s;\n"
                "static long d=&s.b-&s.a; static long reverse=&s.a-&s.b;\n"
                "static long span=(s.x+4)-(s.x+1);\n"
                "static int equal=&s.a==&s.a; static int ordered=&s.a<&s.b;\n"
                "return d==1 && reverse==-1 && span==3 && equal && ordered;\n"),
            .exit_code = 1,
        },
        {
            "symbolic pointers: constexpr differences compose with arithmetic", __LINE__,
            SVI("struct S {int a,b;}; extern struct S s;\n"
                "constexpr long d=&s.b-&s.a; static long n=2*d+1;\n"
                "_Static_assert(d==1); _Static_assert((&s.a==&s.a) && (&s.a!=&s.b));\n"
                "return n;\n"),
            .exit_code = 3,
        },
        {
            "symbolic pointers: constexpr pointer aliases and nested members", __LINE__,
            SVI("struct S {int a[2][3];}; static struct S s;\n"
                "constexpr int* p=&s.a[1][2]; constexpr int* q=&s.a[0][1];\n"
                "constexpr long d=p-q; _Static_assert(d==4);\n"
                "static long n=(1 ? p : q)-(q+1); return n;\n"),
            .exit_code = 3,
        },
        {
            "symbolic pointers: casts change subtraction scale", __LINE__,
            SVI("struct S {int a,b;}; extern struct S s;\n"
                "constexpr long bytes=(char*)&s.b-(char*)&s.a;\n"
                "_Static_assert(bytes==sizeof(int));\n"
                "static long n=(&s.b+2)-(&s.a+1); return n;\n"),
            .exit_code = 2,
        },
        {
            "symbolic pointers: unknown runtime pointers still execute", __LINE__,
            SVI("int f(int* p,int* q){return p-q;} int a[4];\n"
                "return f(a+3,a)==3 && f(a,a+3)==-3;\n"),
            .exit_code = 1,
        },
        {
            "static data: ordinary const scalar initializes a static object", __LINE__,
            SVI("const int source=7;\nstatic int value=source;\nreturn value;\n"),
            .exit_code = 7,
        },
        {
            "static data: parser-accepted const objects fold during lowering", __LINE__,
            SVI("static const int n=7; static const double d=1.5;\n"
                "static const int a[3]={2,4,6};\n"
                "static const struct S {int x;unsigned b:4;} s={9,11};\n"
                "static const int* const p=&a[1];\n"
                "static int x=n+2; static double y=d*2;\n"
                "static int z=a[2]+s.x+s.b; static const int* q=p;\n"
                "return x==9 && y==3 && z==26 && q==&a[1] && *q==4;\n"),
            .exit_code = 1,
        },
        {
            "review: braced string initializes an entire character array", __LINE__,
            SVI("char a[4]={\"abc\"}; signed char b[]={\"xy\",};\n"
                "static unsigned short c[3]={u\"ab\"};\n"
                "return a[0]=='a' && a[2]=='c' && a[3]==0 && sizeof(b)==3\n"
                " && b[0]=='x' && b[1]=='y' && b[2]==0 && c[0]=='a' && c[1]=='b' && c[2]==0;\n"),
            .exit_code = 1,
        },
        {
            "review: string-based scalar expressions remain array elements", __LINE__,
            SVI("char a[1]={\"xyz\"[1]}; int b[1]={\"xyz\"[2]};\n"
                "struct S {char c[1];} s={\"xyz\"[0]};\n"
                "return a[0]=='y' && b[0]=='z' && s.c[0]=='x';\n"),
            .exit_code = 1,
        },
        {
            "review: static nullptr comparisons", __LINE__,
            SVI("static int equal=(nullptr==nullptr); static int different=(nullptr!=nullptr);\n"
                "static int converted=((void*)nullptr==(void*)0);\n"
                "static int nonzero=((void*)1!=(void*)0);\n"
                "return equal && !different && converted && nonzero;\n"),
            .exit_code = 1,
        },
        {
            "review: address comma expression nested in static arithmetic", __LINE__,
            SVI("static int g; static int a=(&g,7); static int b=(&g,7)+1; return a==7 && b==8;\n"),
            .exit_code = 1,
        },
        {
            "review: constexpr bool bitfield retains canonical value", __LINE__,
            SVI("constexpr struct S {_Bool b:1;} s={1}; static int n=s.b; return n==1;\n"),
            .exit_code = 1,
        },
        {
            "review: symbolic ternary nested in static arithmetic", __LINE__,
            SVI("static int g; static int x=(&g ? 7 : 9)+1; return x==8;\n"),
            .exit_code = 1,
        },
        {
            "review: static signed conversion checks the truncated value", __LINE__,
            SVI("static signed char x=(signed char)-128.5;\n"
                "static short y=(short)-32768.5f;\n"
                "static int z=(int)-2147483648.75;\n"
                "static signed char minimum=(signed char)-128.0;\n"
                "return x==-128 && y==-32768 && z==(-2147483647-1) && minimum==-128;\n"),
            .exit_code = 1,
        },
        {
            "review: uint128 to float avoids double rounding", __LINE__,
            SVI("static float x=(float)((1ui128<<100)+(1ui128<<76)+1); return x>0x1p100f;\n"),
            .exit_code = 1,
        },
        {
            "review: runtime uint128 to float avoids double rounding", __LINE__,
            SVI("volatile unsigned __int128 n=(1ui128<<100)+(1ui128<<76)+1;\n"
                "float x=(float)n; return x>0x1p100f;\n"),
            .exit_code = 1,
        },
        {
            "review: symbolic pointer under logical operators", __LINE__,
            SVI("static int g; static int x=!!&g; static int y=(&g && 1); return x && y;\n"),
            .exit_code = 1,
        },
        {
            "review: invalid runtime slice in an unexecuted aggregate", __LINE__,
            SVI("static int a[3]; struct S {int s[:]; int pad[4];};\n"
                "int f(int n){if(n){struct S s={a[2:1],{1,2,3,4}};return s.pad[0];}return 7;}\n"
                "return f(0)==7;\n"),
            .exit_code = 1,
        },
        {
            "review: static rounded arithmetic under unary minus", __LINE__,
            SVI("static double x=-(0.1+0.2); return x < -0.29 && x > -0.31;\n"),
            .exit_code = 1,
        },
        {
            "review: static rounded arithmetic under logical not", __LINE__,
            SVI("static int x=!(0.1+0.2); return x==0;\n"),
            .exit_code = 1,
        },
        {
            "review: constexpr bitfield static read", __LINE__,
            SVI("constexpr struct S {unsigned n:3;} s={5}; static int x=s.n; return x==5;\n"),
            .exit_code = 1,
        },
        {
            "static pointer read through arrow", __LINE__,
            SVI("static int target[3]={3,5,7}; constexpr struct S {int* p;} s={&target[1]};\n"
                "static int* p=(&s)->p; return p==&target[1]&&*p==5;\n"),
            .exit_code = 1,
        },
        {
            "static pointer read through dereference", __LINE__,
            SVI("static int target[3]={3,5,7}; constexpr int* p=&target[2];\n"
                "static int* q=*(&p); return q==&target[2]&&*q==7;\n"),
            .exit_code = 1,
        },
        {
            "static pointer read through indirect subscript", __LINE__,
            SVI("static int target[3]={3,5,7}; constexpr int* a[2]={target,&target[2]};\n"
                "constexpr int* const* p=a; static int* q=p[1];\n"
                "return q==&target[2]&&*q==7;\n"),
            .exit_code = 1,
        },
        {
            "static boolean and difference from indirect pointer reads", __LINE__,
            SVI("static int target[3]; constexpr struct S {int* p;} s={&target[1]};\n"
                "constexpr int* p=&target[2]; static _Bool b=(&s)->p;\n"
                "static long d=*(&p)-(&s)->p; return b&&d==1;\n"),
            .exit_code = 1,
        },
        {
            "static indirect function and null pointer reads", __LINE__,
            SVI("int f(void){return 7;} constexpr struct S {int (*f)(void); int* p;} s={f,nullptr};\n"
                "_Static_assert((&s)->f==f&&*(&s.p)==nullptr);\n"
                "static _Bool b=(&s)->f; static int (*g)(void)=(&s)->f;\n"
                "static int* p=*(&s.p); return b&&g()==7&&p==nullptr;\n"),
            .exit_code = 1,
        },
        {
            "type metadata explicit predicates work in scalar contexts", __LINE__,
            SVI("constexpr _Type t=int; constexpr int valid=!t.is_invalid;\n"
                "_Static_assert(valid&&t.is_valid&&t==int);\n"
                "_Type empty={}; _Type runtime=long; if(empty.is_valid) return 0;\n"
                "if(!runtime.is_invalid&&runtime==long) return valid; return 0;\n"),
            .exit_code = 1,
        },
        {
            "unprototyped declaration accepts matching return definition", __LINE__,
            SVI("int f(); int f(void){return 7;} return f();\n"),
            .exit_code = 7,
        },
        {
            "unprototyped redeclaration preserves defined prototype", __LINE__,
            SVI("int f(int x){return x+1;} int f(); return f(6);\n"),
            .exit_code = 7,
        },
        {
            "unprototyped redeclaration preserves prototype before definition", __LINE__,
            SVI("long f(long); long f(); _Static_assert(typeof(f).param_count==1);\n"
                "long g(void){return f(7);} long f(long x){return x;} return g();\n"),
            .exit_code = 7,
        },
        {
            "empty-list definition agrees with empty prototype", __LINE__,
            SVI("int f(void); int f(){return 7;} int f(void); return f();\n"),
            .exit_code = 7,
        },
        {
            "function redeclaration accepts callback parameter qualifiers", __LINE__,
            SVI("int f(int (*cb)(const int));\n"
                "int f(int (*cb)(int)){return cb(6);}\n"
                "int g(int x){return x+1;} return f(g);\n"),
            .exit_code = 7,
        },
        {
            "function redeclaration accepts compatible array pointer parameters", __LINE__,
            SVI("int f(int (*p)[]);\n"
                "int f(int (*p)[3]){return (*p)[2];}\n"
                "int a[3]={3,5,7}; return f(&a);\n"),
            .exit_code = 7,
        },
        {
            "function redeclaration accepts compatible array pointer returns", __LINE__,
            SVI("int (*f(void))[];\n"
                "int (*f(void))[3]{static int a[3]={3,5,7}; return &a;}\n"
                "return (*f())[2];\n"),
            .exit_code = 7,
        },
        {
            "function redeclaration preserves complete return array bounds", __LINE__,
            SVI("int (*f(void))[3]{static int a[3]={3,5,7}; return &a;}\n"
                "int (*f(void))[]; _Static_assert(sizeof(*f())==3*sizeof(int));\n"
                "return (*f())[2];\n"),
            .exit_code = 7,
        },
        {
            "function definition preserves earlier complete return array bounds", __LINE__,
            SVI("int (*f(void))[3];\n"
                "int (*f(void))[]{static int a[3]={3,5,7}; return &a;}\n"
                "_Static_assert(sizeof(*f())==3*sizeof(int)); return (*f())[2];\n"),
            .exit_code = 7,
        },
        {
            "function redeclaration preserves nested callback prototypes", __LINE__,
            SVI("int f(int (*cb)(int)); int f(int (*cb)());\n"
                "_Static_assert(typeof(f).param_type(0).pointee.param_count==1);\n"
                "int f(int (*cb)(int)){return cb(6);} int g(int x){return x+1;} return f(g);\n"),
            .exit_code = 7,
        },
        {
            "unprototyped redeclaration completes return without erasing prototype", __LINE__,
            SVI("int (*f(void))[]; int (*f())[3];\n"
                "_Static_assert(sizeof(*f())==3*sizeof(int));\n"
                "int (*f(void))[3]{static int a[3]={3,5,7}; return &a;} return (*f())[2];\n"),
            .exit_code = 7,
        },
        {
            "composite function type retains definition parameter qualifiers", __LINE__,
            SVI("int f(int); int f(const int x){return x+1;}\n"
                "_Static_assert(typeof(f).param_type(0).is_const); return f(6);\n"),
            .exit_code = 7,
        },
        {
            "review: pointer constant converted to bool", __LINE__,
            SVI("static int g; static _Bool x=&g; return x;\n"),
            .exit_code = 1,
        },
        {
            "review: static constexpr any unboxing", __LINE__,
            SVI("constexpr _Any a=7; static int x=a.as(int); return x==7;\n"),
            .exit_code = 1,
        },
        {
            "review: template fallback preserves compound literal metadata", __LINE__,
            SVI("int n=7; struct S {int* p; int x[4];};\n"
                "struct S s={(int[]){n},{1,2,3,n}};\n"
                "return s.p[0]==7 && s.x[3]==7;\n"),
            .exit_code = 1,
        },
        {
            "review: static float128 integer conversion", __LINE__,
            SVI("static _Float128 x=1; return x==1;\n"),
            .exit_code = 1,
        },
        {
            "static data: signed bitfields and fields spanning bytes", __LINE__,
            SVI("constexpr struct S {unsigned pad:3; signed int n:6; unsigned tail:9;} s={1,-7,301};\n"
                "static int n=s.n; static unsigned tail=s.tail;\n"
                "return n==-7 && tail==301;\n"),
            .exit_code = 1,
        },
        {
            "local aggregate: runtime compound literal is reset each evaluation", __LINE__,
            SVI("struct S {int* p; int x[4];}; int total=0;\n"
                "for(int i=0;i<2;i++){struct S s={(int[]){7},{1,2,3,4}};\n"
                "total+=s.p[0]; s.p[0]=99;} return total==14;\n"),
            .exit_code = 1,
        },
        {
            "local aggregate: partial template leaves one runtime initializer", __LINE__,
            SVI("struct S {int a,b,c,d,e,f,g,h,i,j,k;};\n"
                "int f(int x){struct S s={1,2,x,4,5,6,7,8,9,10,11};\n"
                "return s.b==2 && s.c==x && s.k==11;} return f(3) && f(17);\n"),
            .exit_code = 1,
            .expect_template = 1,
            .expect_runtime_stores = 1,
        },
        {
            "local aggregate: partial template preserves nested side effects and relocations", __LINE__,
            SVI("static int g=9; int next(void){static int n;return ++n;}\n"
                "struct S {int* p;int a[6];};\n"
                "int f(void){struct S s={&g,{1,next(),3,next(),5,6}};\n"
                "return *s.p+s.a[0]+s.a[1]+s.a[2]+s.a[3]+s.a[4]+s.a[5];}\n"
                "return f()==27 && f()==31;\n"),
            .exit_code = 1,
            .expect_template = 1,
        },
        {
            "local aggregate: overlapping dynamic initializers preserve final writes", __LINE__,
            SVI("int f(int x){int a[6]={[0]=x,1,2,3,4,5,[0]=7};\n"
                "return a[0]==7 && a[1]==1 && a[5]==5;} return f(19);\n"),
            .exit_code = 1,
        },
        {
            "local aggregate: constant template is copied each invocation", __LINE__,
            SVI("struct S {int a[8]; int tail;};\n"
                "int f(int n){struct S s={{1,2,3,4,5,6,7,8},9};\n"
                " s.a[0]=n; return s.a[0]+s.a[7]+s.tail;}\n"
                "return f(10)==27 && f(20)==37;\n"),
            .exit_code = 1,
            .expect_template = 1,
        },
        {
            "local aggregate: template handles bitfields and overwritten designators", __LINE__,
            SVI("struct S {unsigned a:3; unsigned b:5; int x[4];};\n"
                "int f(void){struct S s={.a=1,.b=17,.x={2,4,6,8},.a=5};\n"
                " return s.a==5 && s.b==17 && s.x[0]==2 && s.x[3]==8;}\n"
                "return f() && f();\n"),
            .exit_code = 1,
            .expect_template = 1,
        },
        {
            "local aggregate: template includes symbol relocations", __LINE__,
            SVI("static int g[2]={7,8}; int target(void){return 11;}\n"
                "struct S {int* p; int (*fn)(void); const char* text; int x[4];};\n"
                "int f(void){struct S s={&g[1],target,\"hello\"+1,{1,2,3,4}};\n"
                " return *s.p==8 && s.fn()==11 && s.text[0]=='e' && s.x[3]==4;}\n"
                "return f() && f();\n"),
            .exit_code = 1,
            .expect_template = 1,
        },
        {
            "local aggregate: overwritten relocations are omitted", __LINE__,
            SVI("int missing(void);\n"
                "struct S {int (*fn)(void); void* address; int x[4];};\n"
                "int f(void){struct S s={.fn=missing,.fn=nullptr,.address=(void*)17,\n"
                " .x={1,2,3,4}}; return s.fn==nullptr && (unsigned long)s.address==17\n"
                " && s.x[0]==1 && s.x[3]==4;}\n"
                "return f();\n"),
            .exit_code = 1,
            .expect_template = 1,
        },
        {
            "local aggregate: dynamic call falls back to field stores", __LINE__,
            SVI("int next(void){static int n; return ++n;}\n"
                "int f(void){int a[5]={1,2,next(),4,5}; return a[2];}\n"
                "return f()==1 && f()==2;\n"),
            .exit_code = 1,
        },
        {
            "local aggregate: bitfield read falls back to field stores", __LINE__,
            SVI("struct Bits {unsigned n:3;};\n"
                "int f(int x){struct Bits b={x}; int a[5]={b.n,2,3,4,5}; return a[0];}\n"
                "return f(3)==3 && f(6)==6;\n"),
            .exit_code = 1,
        },
        {
            "local aggregate: const local is evaluated at runtime", __LINE__,
            SVI("int f(int n){const int x=n; int a[5]={x,x+1,x+2,x+3,x+4};\n"
                " return a[0]+a[4];}\n"
                "return f(2)==8 && f(7)==18;\n"),
            .exit_code = 1,
        },
        {
            "static data: interpreter constant expressions", __LINE__,
            SVI("static int a[4]={1,2,3,4};\n"
                "static int i=(1+2)*3; static unsigned u=(unsigned)(1<<12);\n"
                "static double d=1.25*2; static int choice=(0 ? 99 : 7);\n"
                "static const char text[4]=\"abc\";\n"
                "static int* p=&a[1+1];\n"
                "return i==9 && u==4096 && d==2.5 && choice==7\n"
                " && text[2]=='c' && *p==3;\n"),
            .exit_code = 1,
        },
        {
            "static data: rounded floating constant conversion", __LINE__,
            SVI("static float f=1.1; static int i=(int)1.5;\n"
                "static float sum=0.1f+0.2f;\n"
                "static long double ld=1.25L*2;\n"
                "static int equal=(1.25L*2==2.5L);\n"
                "static int selected=(1.0L ? 2:3)+1;\n"
                "static float rounded=(float)((1ull<<63)+(1ull<<39)+1);\n"
                "return f>1.09f && f<1.11f && sum>0.29f && sum<0.31f\n"
                " && i==1 && ld==2.5L && equal && selected==3\n"
                " && rounded==(float)(1ull<<63)+(float)(1ull<<40);\n"),
            .exit_code = 1,
        },
        {
            "static data: slices retain counts and relocated addresses", __LINE__,
            SVI("static int a[3]={2,4,6}; static int all[:]=a[:];\n"
                "static int lo[:]=a[1:]; static int hi[:]=a[:2];\n"
                "static int mid[:]=a[1:2]; static int empty[:]=a[3:3];\n"
                "static int cast[:]=(int[:])a; static int ptr[:]=(&a[1])[:2];\n"
                "return all.count==3 && all[1]==4 && lo.count==2 && lo[0]==4\n"
                " && hi.count==2 && hi[1]==4 && mid.count==1 && mid[0]==4\n"
                " && empty.count==0 && empty.data==a+3 && cast.data==a\n"
                " && ptr.count==2 && ptr[1]==6;\n"),
            .exit_code = 1,
        },
        {
            "static data: slices of constant slices", __LINE__,
            SVI("static int a[3]={2,4,6};\n"
                "static const int part[:]=(a[:])[1:];\n"
                "return part.count==2 && part.data==a+1 && part[1]==6;\n"),
            .exit_code = 1,
        },
        {
            "static data: pointer from constexpr aggregate", __LINE__,
            SVI("constexpr struct S {const char* p;} s={\"abc\"};\n"
                "static const char* p=s.p; return p[1]=='b';\n"),
            .exit_code = 1,
        },
        {
            "static data: constexpr subobjects preserve relocations", __LINE__,
            SVI("constexpr struct S {const char* p[2];} s={{\"ab\",\"cd\"}};\n"
                "static const char* p=s.p[1]+1;\n"
                "static struct S copy=s;\n"
                "return *p=='d' && copy.p[1][0]=='c';\n"),
            .exit_code = 1,
        },
        {
            "static data: commuted pointer offsets and integer casts", __LINE__,
            SVI("static const char* text=2+\"abcd\";\n"
                "static void* address=(void*)(unsigned char)257;\n"
                "return text[0]=='c' && (unsigned long)address==1;\n"),
            .exit_code = 1,
        },
        {
            "static data: file-scope compound literal addresses", __LINE__,
            SVI("struct S {int n;};\n"
                "static int* p=(int[]){3,4};\n"
                "static struct S* s=&(struct S){7};\n"
                "return p[0]==3 && p[1]==4 && s->n==7;\n"),
            .exit_code = 1,
        },
        {
            "static data: aggregate constants and address relocations", __LINE__,
            SVI("struct S {unsigned a:3; unsigned b:5; int values[3]; const char* text;};\n"
                "static struct S s={5,17,{2,4,6},\"hello\"+1};\n"
                "static int* p=&s.values[1]; static int* end=s.values+3;\n"
                "static double d=1.25*2; static unsigned __int128 big=(unsigned __int128)1<<100;\n"
                "return s.a==5 && s.b==17 && *p==4 && end-p==2 && s.text[0]=='e' && d==2.5 && (big>>100)==1;\n"),
            .exit_code = 1,
        },
        {
            "static data: cyclic address relocations", __LINE__,
            SVI("struct Node {struct Node* next; int value;};\n"
                "static struct Node a; static struct Node b={&a,2}; static struct Node a={&b,1};\n"
                "return a.next==&b && b.next==&a && a.next->value==2;\n"),
            .exit_code = 1,
        },
        {
            "static data: overwritten relocations", __LINE__,
            SVI("int missing_static_dep(void); struct S {int (*p)(void); int n;};\n"
                "static struct S s={.p=missing_static_dep,.p=nullptr,.n=7};\n"
                "return s.p==nullptr && s.n==7;\n"),
            .exit_code = 1,
        },
        {
            "static data: bitfield overwrites a relocation", __LINE__,
            SVI("int missing_static_dep(void); union U {int (*p)(void); unsigned bits:3;};\n"
                "struct S {union U u;}; static struct S s={.u.p=missing_static_dep,.u.bits=5};\n"
                "return s.u.bits==5;\n"),
            .exit_code = 1,
        },
        {
            "lower deps: unevaluated undefined externs", __LINE__,
            SVI("extern int missing_var; int missing_func(void);\n"
                "return sizeof(missing_var) + sizeof(missing_func());\n"),
            .exit_code = 8,
        },
        {
            "sizeof statement expression discards statements and dependencies", __LINE__,
            SVI("int missing_func(void);\n"
                "int probe(void){ int effects=0;\n"
                "  int size=sizeof(({ int local=missing_func(); effects++; local; }));\n"
                "  return size==sizeof(int) && effects==0;\n"
                "}\n"
                "return probe();\n"),
            .exit_code = 1,
        },
        {
            "lower deps: discarded branch does not resolve externs", __LINE__,
            SVI("extern int missing_var; int missing_func(void);\n"
                "return 0 ? missing_func()+missing_var : 7;\n"),
            .exit_code = 7,
        },
        {
            "lower deps: static initializer discovers function and storage", __LINE__,
            SVI("int g; int target(void){return ++g;}\n"
                "int run(void){static int (*p)(void)=target; return p();}\n"
                "return run()+run();\n"),
            .exit_code = 3,
        },
        {
            "named arguments retain prototype and definition names", __LINE__,
            SVI("int f(int first, int second);\n"
                "int x=f(.second=4,.first=3);\n"
                "int f(int a,int b){return 10*a+b;}\n"
                "return x+f(.b=6,.a=5);\n"),
            .exit_code = 90,
        },
        {
            "call: flat mixed arguments and unnamed parameters", __LINE__,
            SVI("struct S {long a; char b;};\n"
                "int f(char a, double, struct S s, short z){return a+s.a+s.b+z;}\n"
                "int (*p)(char,double,struct S,short)=f;\n"
                "return f(1,2.0,(struct S){3,4},5)+p(1,2.0,(struct S){3,4},5);\n"),
            .exit_code = 26,
        },
        {
            "call: nested arguments preserve captured values", __LINE__,
            SVI("int x=3; int change(void){x=9; return 4;}\n"
                "int f(int a,int b){return a*10+b;}\nreturn f(x,change());\n"),
            .exit_code = 34,
        },
        {
            "call: flat variadic buffer with mixed sizes", __LINE__,
            SVI("struct S {char s[11];};\n"
                "int f(char c,...){__builtin_va_list ap; __builtin_va_start(ap,c);\n"
                "int i=__builtin_va_arg(ap,int); double d=__builtin_va_arg(ap,double);\n"
                "struct S s=__builtin_va_arg(ap,struct S); int z=__builtin_va_arg(ap,int);\n"
                "__builtin_va_end(ap); return c+i+(int)d+s.s[10]+z;}\n"
                "int (*p)(char,...)=f;\n"
                "return f(1,2,3.0,(struct S){.s[10]=4},5)+p(1,2,3.0,(struct S){.s[10]=4},5);\n"),
            .exit_code = 30,
        },
        {
            "call: hotswap preserves unnamed parameter slots", __LINE__,
            SVI("int f(char, double, int z){return z;}\n"
                "int g(char c,double d,int z){return c+(int)d+z;}\n"
                "int (*p)(char,double,int)=f; __hotswap(f,g);\n"
                "return f(1,2.0,3)+p(4,5.0,6);\n"),
            .exit_code = 21,
        },
        {
            "varargs: wide fixed parameter remains supported", __LINE__,
            SVI("int f(__int128 n,...){__builtin_va_list ap; __builtin_va_start(ap,n);\n"
                "double d=__builtin_va_arg(ap,double); __builtin_va_end(ap);\n"
                "return (int)n+(int)d;}\nreturn f(40,2.0);\n"),
            .exit_code = 42,
        },
        {
            "call: borrowed varargs survive recursion and va_copy", __LINE__,
            SVI("int f(int n,...){__builtin_va_list ap,copy; __builtin_va_start(ap,n);\n"
                "__builtin_va_copy(copy,ap); int first=__builtin_va_arg(ap,int);\n"
                "int nested=n?f(n-1,10,20):0;\n"
                "int second=__builtin_va_arg(ap,int);\n"
                "int again=__builtin_va_arg(copy,int); again+=__builtin_va_arg(copy,int);\n"
                "__builtin_va_end(copy); __builtin_va_end(ap);\n"
                "return first+second+again+nested;}\nreturn f(2,1,2);\n"),
            .exit_code = 126,
        },
        {
            "auto: const array members preserve pointee qualifiers", __LINE__,
            SVI("struct S {int a[2];}; const struct S s={{1,2}};\n"
                "auto p=s.a;\n"
                "const _Any a=(struct S){{3,4}}; auto q=a.as(struct S).a;\n"
                "return _Generic(p,const int*:1,default:0)\n"
                " && _Generic(q,const int*:1,default:0)\n"
                " && p[0]==1 && p[1]==2 && q[0]==3 && q[1]==4;\n"),
            .exit_code = 1,
        },
        {
            "init: shorter string replaces whole array", __LINE__,
            SVI(
                "struct S {char s[4]; int sentinel;};\n"
                "struct S s={.s=\"abc\",.sentinel=42,.s=\"x\"};\n"
                "return s.s[0]==120 && s.s[1]==0 && s.s[2]==0 && s.s[3]==0 && s.sentinel==42;\n"
            ),
            .exit_code = 1,
        },
        {
            "init: wide string replacement clears omitted elements", __LINE__,
            SVI(
                "struct S {unsigned short s[4];};\n"
                "struct S s={.s=u\"abc\",.s=u\"x\"};\n"
                "return s.s[0]==120 && s.s[1]==0 && s.s[2]==0 && s.s[3]==0;\n"
            ),
            .exit_code = 1,
        },
        {
            "init: disjoint strings avoid extra zeroing", __LINE__,
            SVI(
                "struct S {char a[4]; char b[4];};\n"
                "struct S s={.b=\"bc\",.a=\"a\"};\n"
                "return s.a[0]==97 && s.a[3]==0 && s.b[1]==99 && s.b[3]==0;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: const array permits qualified pointer and slice", __LINE__,
            SVI(
                "struct S {int a[2];}; const _Any a=(struct S){{1,2}};\n"
                "const int* p=a.as(struct S).a;\n"
                "const int s[:]=a.as(struct S).a;\n"
                "return p[0]==1 && s[1]==2;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: conditional aggregate agrees with constexpr", __LINE__,
            SVI(
                "struct S {int x,y;}; constexpr struct S ss[2]={{3,4},{5,6}};\n"
                "constexpr _Any a=1?ss[0]:ss[1];\n"
                "_Static_assert(a.as(struct S).y==4);\n"
                "return a.as(struct S).y==4;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: boxed array element agrees with constexpr", __LINE__,
            SVI("struct S {int x,y;};\n"
                "constexpr struct S values[2]={{1,2},{3,4}};\n"
                "constexpr _Any a=values[1];\n"
                "constexpr int x=a.as(struct S).x, y=a.as(struct S).y;\n"
                "return x==3 && y==4 && a.as(struct S).x==x && a.as(struct S).y==y;"),
            .exit_code = 1,
        },
        {
            "any: vector boxes a value and preserves its tag", __LINE__,
            SVI(
                "typedef int V __attribute__((vector_size(8)));\n"
                "V v={3,4}; _Any a=v;\n"
                "if(a.type!=V) return 0;\n"
                "v[0]=99;\n"
                "if(a.as(V)[0]!=3 || a.as(V)[1]!=4) return 0;\n"
                "a.as(V)[1]=7;\n"
                "return a.type==V && a.as(V)[1]==7 && v[1]==4;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: small vector payload view is writable", __LINE__,
            SVI(
                "typedef int V __attribute__((vector_size(8)));\n"
                "_Any a=0ull;\n"
                "a.as(V)[0]=3; a.as(V)[1]=4;\n"
                "return a.type==unsigned long long && a.as(V)[0]==3 && a.as(V)[1]==4;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: empty array initializer overwrites tag and payload", __LINE__,
            SVI(
                "_Any values[1]={[0]=0xffffffffffffffffull,[0]={}};\n"
                "return values[0].type.is_invalid && values[0].as(unsigned long long)==0;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: empty struct member initializer overwrites tag and payload", __LINE__,
            SVI(
                "struct S {_Any a; int sentinel;};\n"
                "struct S s={.a=0xffffffffffffffffull,.sentinel=42,.a={}};\n"
                "return s.a.type.is_invalid && s.a.as(unsigned long long)==0 && s.sentinel==42;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: empty assignment clears an existing value", __LINE__,
            SVI(
                "_Any a=0xffffffffffffffffull; a={};\n"
                "return a.type.is_invalid && a.as(unsigned long long)==0;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: const aggregate preserves payload pointer qualifiers", __LINE__,
            SVI(
                "struct S {_Any a;}; const struct S s={3};\n"
                "return _Generic(s.a.payload, const void*:1, default:0)\n"
                " && _Generic(&s.a.as(int), const int*:1, default:0)\n"
                " && _Generic(&s.a.type, const _Type*:1, default:0);\n"
            ),
            .exit_code = 1,
        },
        {
            "any: empty elements in print-style macro", __LINE__,
            SVI("int check(unsigned long count, _Any* args){\n"
                " return count==4 && args[0].type==char(*)[6]\n"
                " && args[1].type.is_invalid && args[2].type==int\n"
                " && args[2].as(int)==0 && args[3].type.is_invalid;\n"
                "}\n"
                "#define check(...) check(__VA_COUNT__, (_Any[]){__VA_ARGS__})\n"
                "return check(\"empty\", {}, {0}, {{}});\n"),
            .exit_code = 1,
        },
        {
            "empty scalar members and nested braces", __LINE__,
            SVI("struct S {int x; _Any a; _Type t;};\n"
                "struct S s={{}, {}, {}}; int values[2]={{}, {{}}};\n"
                "_Any a={{}};\n"
                "return s.x==0 && s.a.type.is_invalid && s.t.is_invalid\n"
                " && values[0]==0 && values[1]==0 && a.type.is_invalid;\n"),
            .exit_code = 1,
        },
        {
            "any: bitfield views", __LINE__,
            SVI("struct Bits {unsigned a:3; signed b:3;};\n"
                "constexpr struct Bits bits={3,-1}; constexpr _Any a=bits;\n"
                "_Static_assert(a.as(struct Bits).a==3);\n"
                "_Static_assert(a.as(struct Bits).b==-1);\n"
                "return a.as(struct Bits).a==3 && a.as(struct Bits).b==-1;\n"),
            .exit_code = 1,
        },
        {
            "any: conditional copies and type payload", __LINE__,
            SVI("_Any a=3, b=4; int flag=1; _Any c=flag?a:b;\n"
                "if(c.as(int)!=3) return 0;\n"
                "a=int; return a.type==_Type && a.as(_Type)==int;\n"),
            .exit_code = 1,
        },
        {
            "any: constant pointer tags", __LINE__,
            SVI("constexpr _Any a=\"hello\";\n"
                "_Static_assert(a.type==char(*)[6]);\n"
                "constexpr _Any b=nullptr;\n"
                "_Static_assert(b.type==typeof(nullptr));\n"
                "return a.as(char*)[0]=='h' && b.as(unsigned long long)==0;\n"),
            .exit_code = 1,
        },
        {
            "any: enum and small union", __LINE__,
            SVI("enum E {value=42}; enum E e=value; _Any a=e;\n"
                "if(a.type!=enum E || a.as(enum E)!=value) return 0;\n"
                "union U {int i; float f;}; union U u={.f=1.f}; a=u;\n"
                "return a.type==union U && a.as(union U).i==0x3f800000;\n"),
            .exit_code = 1,
        },
        {
            "any: empty, zero, and layout", __LINE__,
            SVI(
                "_Any empty = {}; _Any zero = {0}; static _Any global;\n"
                "struct Holder {_Any a[2];}; struct Holder h = {};\n"
                "return sizeof(_Any)==16 && alignof(_Any)==8 && __ANY_PAYLOAD_SIZE__==8\n"
                " && empty.type.is_invalid && !empty.type.is_valid && global.type.is_invalid\n"
                " && h.a[0].type.is_invalid && h.a[1].type.is_invalid\n"
                " && zero.type==int && zero.as(int)==0 && empty.as(unsigned long long)==0;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: copy, retag, and aliasing", __LINE__,
            SVI(
                "int f(void){\n"
                "    _Any a = 3; a = a.as(int) + 4;\n"
                "    if(a.type != int || a.as(int)!=7) return 0;\n"
                "    a.as(int) = 9.f;\n"
                "    a.type = unsigned;\n"
                "    const _Any b = a; a = b; a = a;\n"
                "    _Any* p = &a; p->as(unsigned) += 2;\n"
                "    return a.type == unsigned && b.as(unsigned)==9 && a.as(unsigned)==11;\n"
                "}\n"
                "return f();\n"
            ),
            .exit_code = 1,
        },
        {
            "any: wrapped function", __LINE__,
            SVI(
            "int f(void){\n"
            "    return 42;\n"
            "}\n"
            "_Any a = f;\n"
            "return a.as(int(*)(void))();\n"
            ),
            .exit_code = 42,
        },
        {
            "any: wrapped function: convert function type to pointer", __LINE__,
            SVI(
            "int f(void){\n"
            "    return 42;\n"
            "}\n"
            "_Any a = f;\n"
            "return a.as(int(void))();\n"
            ),
            .exit_code = 42,
        },
        {
            "any: zero-fill versus typed writes", __LINE__,
            SVI(
                "_Any a = 0xffffffffffffffffull;\n"
                "a = (unsigned char)5;\n"
                "if(a.as(unsigned long long)!=5) return 0;\n"
                "a = 0xffffffffffffffffull; a.as(unsigned)=0;\n"
                "return a.type==unsigned long long && a.as(unsigned long long)==0xffffffff00000000ull;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: qualifiers and payload address", __LINE__,
            SVI(
                "int x=3; const int* ptr=&x; const float f=2.f;\n"
                "_Any a=f; if(a.type!=float) return 0;\n"
                "a=ptr; if(a.type!=const int*) return 0;\n"
                "const _Any c=3;\n"
                "if(!_Generic(c.payload, const void*:1, default:0)) return 0;\n"
                "if(!_Generic(&c.as(int), const int*:1, default:0)) return 0;\n"
                "if(!_Generic(&c.type, const _Type*:1, default:0)) return 0;\n"
                "_Any b=2; int* ip=b.payload; *ip=7;\n"
                "return b.as(int)==7 && (char*)b.payload-(char*)&b==8;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: array and function addresses", __LINE__,
            SVI(
                "int data[2]={3,4}; const int cs[3]={1,2,3};\n"
                "int f(int x){return x+1;}\n"
                "_Any a=data;\n"
                "if(a.type!=int(*)[2]) return 0;\n"
                "a.as(int(*)[2])[0][1]=8;\n"
                "a=cs; if(a.type!=const int(*)[3]) return 0;\n"
                "a=\"hello\"; if(a.type!=char(*)[6] || a.as(char*)[1]!='e') return 0;\n"
                "a=f; return a.type==int(*)(int) && a.as(int(*)(int))(data[1])==9;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: calls, returns, and temporary receivers", __LINE__,
            SVI(
                "_Any identity(_Any a){return a;}\n"
                "_Any make(int x){return x;}\n"
                "int read(_Any a){return a.type==int ? a.as(int):0;}\n"
                "return read(42)==42 && identity(3.f).type==float && make(7).as(int)==7;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: small aggregates and destination pointers", __LINE__,
            SVI(
                "struct S {int x,y;}; struct S s={3,4};\n"
                "_Any a=s; s.x=99;\n"
                "if(a.type!=struct S || a.as(struct S).x!=3) return 0;\n"
                "a.as(struct S).y=6;\n"
                "int parse(_Any dst){\n"
                " if(dst.type.pointee!=struct S) return 1;\n"
                " dst.as(struct S*).x=42;\n"
                " return 0;\n"
                "}\n"
                "return parse(&s)==0 && s.x==42 && a.as(struct S).y==6;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: compound literal storage", __LINE__,
            SVI( "int f(void){\n"
                " _Any a=&(struct S {int x,y;}){3,4};\n"
                " int* p=((_Any){7}).payload;\n"
                " _Any b=99;\n"
                " return *p==7 && a.as(struct S*).y==4 && b.as(int)==99;\n"
                "} return f();\n"
            ),
            .exit_code = 1,
        },
        {
            "compound literals are assignment targets", __LINE__,
            SVI("int f(void){\n"
                " (int){3}=4;\n"
                " int n=0;\n"
                " if(((int){++n}=4)!=4 || n!=1) return 91;\n"
                " if((((int){3})+=4)!=7) return 92;\n"
                " struct S {int x;};\n"
                " if(((struct S){3}=(struct S){4}).x!=4) return 93;\n"
                " return 7;\n"
                "} return f();\n"),
            .exit_code = 7,
        },
        {
            "any: constexpr representations", __LINE__,
            SVI(
                "constexpr _Any e={}; constexpr _Any z={0}; constexpr _Any f=1.f;\n"
                "constexpr _Any c=f;\n"
                "_Static_assert(e.type.is_invalid && !e.type.is_valid);\n"
                "_Static_assert(z.type==int && z.as(int)==0);\n"
                "_Static_assert(f.type==float && f.as(float)==1.f);\n"
                "_Static_assert(c.type==float && c.as(const float)==1.f);\n"
                "constexpr _Any neg=-3;\n"
                "_Static_assert(neg.as(int)==-3);\n"
                "return 1;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: explicit boxing and type reflection", __LINE__,
            SVI(
                "_Any a=(_Any)4;\n"
                "_Type t=int; _Type none={};\n"
                "int accepts(_Any a){return a.as(int);}\n"
                "return a.as(int)==4 && t.is_valid && none.is_invalid\n"
                " && t.is_castable_to(_Any) && (int(int)).is_callable_with(int)\n"
                " && (int(_Any)).is_callable_with(int) && !(int(_Any)).is_callable_with(int[:]);\n"
            ),
            .exit_code = 1,
        },
        {
            "any: payload of temporary", __LINE__,
            SVI(
                "_Any f(void){return 42;}\n"
                "int read(const void* p){return *(const int*)p;}\n"
                "return read(f().payload)==42;\n"
            ),
            .exit_code = 1,
        },
        {
            "any: arrays of boxed values", __LINE__,
            SVI(
                "_Any a[3]={1,2.f,\"hello\"};\n"
                "return a[0].type==int && a[1].type==float && a[2].type==char(*)[6]\n"
                " && a[0].as(int)==1 && a[1].as(float)==2.f && a[2].as(char*)[0]=='h';\n"
            ),
            .exit_code = 1,
        },
        {
            "any: dynamic any", __LINE__,
            SVI(
                "int x = 3;\n"
                "_Any a = x;\n"
                "_Type T = typeof(x);\n"
                "_Any b = T.make_any(&x);\n"
                "return a.as(int) == b.as(int);\n"
            ),
            .exit_code = 1,
        },
        {
            "fold: constant pointer casts", __LINE__,
            SVI("return (int)(unsigned long long)(void*)42;\n"),
            .exit_code = 42,
        },
        {
            "fold: null pointer casts", __LINE__,
            SVI("return !(int*)(void*)0;\n"),
            .exit_code = 1,
        },
        {
            "fold: fixed array displacement", __LINE__,
            SVI("int a[4] = {10,20,30,42}; return a[1+2];\n"),
            .exit_code = 42,
        },
        {
            "fold: pointer displacement", __LINE__,
            SVI("int a[4] = {10,20,30,42}; int *p = a; return p[3];\n"),
            .exit_code = 42
        },
        {
            "fold: negative pointer displacement", __LINE__,
            SVI("int a[4] = {42,20,30,40}; int *p = a+3; return p[-3];\n"),
            .exit_code = 42,
        },
        {
            "fold: one past array address", __LINE__,
            SVI("int a[4]; return &a[4] == a+4;\n"),
            .exit_code = 1,
        },
        {
            "fold: pointer subscript snapshots address", __LINE__,
            SVI("int f(void){int a[2]={0,0}; int *p=a; p[0]=(p=a+1,42); return a[0]+a[1]*2;} return f();\n"),
            .exit_code = 42,
        },
        {
            "fold: array member snapshots base", __LINE__,
            SVI("int f(void){struct S {int a[2];}; struct S s={0}, t={0}; struct S *p=&s; p->a[1]=(p=&t,42); return s.a[1]+t.a[1]*2;} return f();\n"),
            .exit_code = 42,
        },
        {
            "fold: member address immediate", __LINE__,
            SVI("struct S {int a,b;}; struct S s={0,42}; struct S *p=&s; int *q=&p->b; return *q;\n"),
            .exit_code = 42,
        },
        {
            "fold: selected runtime ternary", __LINE__,
            SVI("int f(void){return 42;} int g(void){return 99;} return 1 ? f() : g();\n"),
            .exit_code = 42,
        },
        {
            "fold: constant branch chain", __LINE__,
            SVI("int x=0; if(0) x=99; if(1) x=42; while(0) x++; return x;\n"),
            .exit_code = 42,
        },
        {
            "fold: goto into constant false branch", __LINE__,
            SVI("int x=0; goto L; if(0){L: x=42;} return x;\n"),
            .exit_code = 42,
        },
        {
            "fold: float exact arithmetic", __LINE__,
            SVI("return (int)((800 / 2.0 + 20.0) / 10.0);\n"),
            .exit_code = 42,
        },
        {
            "fold: float exact multiply subtract", __LINE__,
            SVI("return (int)(8.0f * 6.0f - 6.0f);\n"),
            .exit_code = 42,
        },
        {
            "fold: float constant comparisons", __LINE__,
            SVI("return (-3.0 < -2.0) && (2.0 <= 3.0) && (4.0 > 3.0) && (2.0 >= 2.0) && (-0.0 == 0.0) && (2.0 != 3.0);\n"),
            .exit_code = 1,
        },
        {
            "fold: float inexact division retained", __LINE__,
            SVI("return (1.0 / 3.0) > 0.0;\n"),
            .exit_code = 1,
        },
        {
            "fold: float cancellation retained", __LINE__,
            SVI("return (1.0 - 1.0) == 0.0;\n"),
            .exit_code = 1,
        },
        {
            "fold: pointer plus constant", __LINE__,
            SVI("int a[4] = {10, 20, 30, 42}; int *p = a; return *(p + (1 + 2));\n"),
            .exit_code = 42,
        },
        {
            "fold: constant plus pointer", __LINE__,
            SVI("int a[4] = {10, 20, 30, 42}; int *p = a; return *((1 + 2) + p);\n"),
            .exit_code = 42,
        },
        {
            "fold: pointer plus negative", __LINE__,
            SVI("int a[4] = {42, 20, 30, 40}; int *p = a + 3; return *(p + -3);\n"),
            .exit_code = 42,
        },
        {
            "fold: pointer minus negative", __LINE__,
            SVI("int a[4] = {10, 20, 30, 42}; int *p = a; return *(p - -3);\n"),
            .exit_code = 42,
        },
        {
            "fold: pointer compound scaled", __LINE__,
            SVI("int a[4] = {10, 20, 42, 40}; int *p = a; p += (1 + 2); p -= 1; return *p;\n"),
            .exit_code = 42,
        },
        {
            "fold: pointer compound negative", __LINE__,
            SVI("int a[4] = {10, 42, 30, 40}; int *p = a; p -= -3; p += -2; return *p;\n"),
            .exit_code = 42,
        },
        {
            "fold: byte pointer immediate", __LINE__,
            SVI("char a[2] = {0, 42}; char *p = a; p += 1; return *p;\n"),
            .exit_code = 42,
        },
        {
            "fold: pointer lvalue evaluated once", __LINE__,
            SVI("int a[2] = {10, 42}; int *p[2] = {a, a}; int **q = p; *q++ += 1; return **p + (q == p + 1);\n"),
            .exit_code = 43,
        },
        {
            "fold: pointer index side effects", __LINE__,
            SVI("int a[4] = {10, 20, 30, 42}; int *p = a; int n = 0; p += (n++, 3); return *p + n;\n"),
            .exit_code = 43,
        },
        {
            "fold: pointer expression evaluated once", __LINE__,
            SVI("int a[3] = {10, 42, 30}; int *p = a; int *q = 1 + p++; return *q + (p == a + 1);\n"),
            .exit_code = 43,
        },
        {
            "fold: exact floating cast chain", __LINE__,
            SVI("return (int)(double)(float)42;\n"),
            .exit_code = 42,
        },
        {
            "fold: negative floating cast", __LINE__,
            SVI("return (int)(float)-42.0;\n"),
            .exit_code = -42,
        },
        {
            "fold: fractional conversion stays at runtime", __LINE__,
            SVI("return (int)42.5;\n"),
            .exit_code = 42,
        },
        {
            "fold: inexact integer conversion stays at runtime", __LINE__,
            SVI("return (int)(float)16777217;\n"),
            .exit_code = 16777216,
        },
        {
            "fold: inexact float narrowing stays at runtime", __LINE__,
            SVI("return (int)(float)1.000000059604644775390625;\n"),
            .exit_code = 1,
        },
        {
            "fold: nested integer expression", __LINE__,
            SVI("return (3 + 4) * 6;\n"),
            .exit_code = 42,
        },
        {
            "fold: binary immediate from expression", __LINE__,
            SVI("int x = 35; return x + (3 + 4);\n"),
            .exit_code = 42,
        },
        {
            "fold: compound immediate from expression", __LINE__,
            SVI("int x = 35; x += (3 + 4); return x;\n"),
            .exit_code = 42,
        },
        {
            "fold: store immediate from expression", __LINE__,
            SVI("int x; x = (3 + 4) * 6; return x;\n"),
            .exit_code = 42,
        },
        {
            "fold: narrow signed casts", __LINE__,
            SVI("return (int)(signed char)255 + (int)(unsigned char)257 + 42;\n"),
            .exit_code = 42,
        },
        {
            "fold: wide casts arithmetic and truth", __LINE__,
            SVI("return (((unsigned __int128)1 << 100) != 0) &&\n"
                "  (((__int128)-7 / 2) == -3) &&\n"
                "  (((unsigned __int128)1 << 100) >> 100) == 1;\n"),
            .exit_code = 1,
        },
        {
            "fold: short circuit unevaluated effects", __LINE__,
            SVI("int f(void){return 99;}\n"
                "return (0 && f()) + (1 || f()) + (1 ? 41 : f());\n"),
            .exit_code = 42,
        },
        {
            "fold: comma retains evaluated effects", __LINE__,
            SVI("int x = 0; return (x++, 41) + x;\n"),
            .exit_code = 42,
        },
        {
            "fold: negative zero condition", __LINE__,
            SVI("return -0.0 ? 0 : 42;\n"),
            .exit_code = 42,
        },
        {
            "fold: integer bit builtins", __LINE__,
            SVI("return __builtin_popcount(7) + __builtin_clz(1u) + __builtin_ctz(256u);\n"),
            .exit_code = 42,
        },
        {
            "fold: unselected invalid arithmetic", __LINE__,
            SVI("return (1 ? 42 : 1 / 0) + (0 && (1 << 99));\n"),
            .exit_code = 42,
        },
        {
            "fold: division by zero uses runtime behavior", __LINE__,
            SVI("return 42 / 0;\n"),
            .exit_code = 0,
        },
        {
            "fold: constexpr variable", __LINE__,
            SVI("constexpr int n = 6; return n * 7;\n"),
            .exit_code = 42,
        },
        {
            "fold: mutable const is runtime storage", __LINE__,
            SVI("int x = 1; const int *p = &x; x = 42; return *p;\n"),
            .exit_code = 42,
        },

        {
            "reflection: discarded module run", __LINE__,
            SVI("_Module m = __compile(\"int x; x = 42;\", \"\");\n"
                "m.run();\n"
                "return *m.symbol(\"x\", int);\n"),
            .exit_code = 42,
        },
        {
            "reflection: dynamic introspection in function", __LINE__,
            SVI("int f(_Type t){ return t.is_integer; }\n"
                "return f(int) && !f(float);\n"),
            .exit_code = 1,
        },
        {
            "reflection: receiver snapshot", __LINE__,
            SVI("struct S { int x; };\n"
                "_Type t = struct S;\n"
                "struct __builtin_Field f = t.field((t = int, 0));\n"
                "return f.type == int && t == int;\n"),
            .exit_code = 1,
        },
        {
            "reflection: discarded type evaluates operands", __LINE__,
            SVI("struct S { int x; };\n"
                "_Type t = struct S;\n"
                "int n = 0;\n"
                "t.field(n++);\n"
                "return n;\n"),
            .exit_code = 1,
        },
        {
            "reflection: discarded module skips operands", __LINE__,
            SVI("int n = 0;\n"
                "_Module m = __root_module();\n"
                "(n++, m).symbol((n++, \"missing\"), int);\n"
                "return n;\n"),
            .exit_code = 0,
        },
        {
            "reflection: module receiver snapshot", __LINE__,
            SVI("_Module m = __compile(\"typedef int T;\", \"\");\n"
                "_ModuleMember t = m.type((m = __root_module(), 0));\n"
                "return t.type == int;\n"),
            .exit_code = 1,
        },
        {
            "reflection: local result survives later operand", __LINE__,
            SVI("int f(void){ _Type t = int; return t.is_integer + (t = float).is_float; }\n"
                "return f();\n"),
            .exit_code = 2,
        },
        {
            "basic", __LINE__,
            SVI("return 13;\n"),
            .exit_code = 13,
        },
        {
            "loops: for", __LINE__,
            SVI("int result = 0;\n"
               "for(int i = 0; i < 10; i++) result += i;\n"
               "return result;\n"),
            .exit_code = 45,
        },
        // Arithmetic
        {
            "arith: add", __LINE__,
            SVI("return 3 + 4;\n"),
            .exit_code = 7,
        },
        {
            "arith: sub", __LINE__,
            SVI("return 10 - 3;\n"),
            .exit_code = 7,
        },
        {
            "arith: mul", __LINE__,
            SVI("return 6 * 7;\n"),
            .exit_code = 42,
        },
        {
            "arith: div", __LINE__,
            SVI("return 42 / 6;\n"),
            .exit_code = 7,
        },
        {
            "arith: mod", __LINE__,
            SVI("return 17 % 5;\n"),
            .exit_code = 2,
        },
        {
            "arith: negative div", __LINE__,
            SVI("return -7 / 2;\n"),
            .exit_code = -3,
        },
        {
            "arith: negative mod", __LINE__,
            SVI("return -7 % 3;\n"),
            .exit_code = -1,
        },
        {
            "arith: precedence", __LINE__,
            SVI("return 2 + 3 * 4;\n"),
            .exit_code = 14,
        },
        {
            "arith: parens", __LINE__,
            SVI("return (2 + 3) * 4;\n"),
            .exit_code = 20,
        },
        {
            "arith: unary minus", __LINE__,
            SVI("return -(-5);\n"),
            .exit_code = 5,
        },
        // Bitwise
        {
            "bitwise: and", __LINE__,
            SVI("return 0xFF & 0x0F;\n"),
            .exit_code = 15,
        },
        {
            "bitwise: or", __LINE__,
            SVI("return 0xF0 | 0x0F;\n"),
            .exit_code = 255,
        },
        {
            "bitwise: xor", __LINE__,
            SVI("return 0xFF ^ 0x0F;\n"),
            .exit_code = 240,
        },
        {
            "bitwise: not", __LINE__,
            SVI("return ~0 & 0xFF;\n"),
            .exit_code = 255,
        },
        {
            "bitwise: lshift", __LINE__,
            SVI("return 1 << 4;\n"),
            .exit_code = 16,
        },
        {
            "bitwise: rshift", __LINE__,
            SVI("return 256 >> 4;\n"),
            .exit_code = 16,
        },
        {
            "bitwise: u64 and", __LINE__,
            SVI("union { double d; unsigned long long i; } u;\n"
               "u.d = 0.5;\n"
               "unsigned long long expmask = 0x7FF0000000000000ULL;\n"
               "unsigned long long result = u.i & expmask;\n"
               "return result == 0x3FE0000000000000ULL;\n"),
            .exit_code = 1,
        },
        {
            "bitwise: u64 and sign", __LINE__,
            SVI("union { double d; unsigned long long i; } u;\n"
               "u.d = -1.0;\n"
               "unsigned long long signmask = 0x8000000000000000ULL;\n"
               "return (u.i & signmask) != 0;\n"),
            .exit_code = 1,
        },
        {
            "bitwise: u64 and frac", __LINE__,
            SVI("union { double d; unsigned long long i; } u;\n"
               "u.d = 1.0;\n"
               "unsigned long long fracmask = 0x000FFFFFFFFFFFFFULL;\n"
               "return (u.i & fracmask) == 0;\n"),
            .exit_code = 1,
        },
        {
            "hex literal U promotion", __LINE__,
            SVI("unsigned long long x = 0x7FF0000000000000U;\n"
               "return x != 0;\n"),
            .exit_code = 1,
        },
        {
            "hex literal no suffix promotion", __LINE__,
            SVI("unsigned long long x = 0x8000000000000000;\n"
               "return x != 0;\n"),
            .exit_code = 1,
        },
        // Comparison
        {
            "cmp: eq true", __LINE__,
            SVI("return 5 == 5;\n"),
            .exit_code = 1,
        },
        {
            "cmp: eq false", __LINE__,
            SVI("return 5 == 6;\n"),
            .exit_code = 0,
        },
        {
            "cmp: neq", __LINE__,
            SVI("return 5 != 6;\n"),
            .exit_code = 1,
        },
        {
            "cmp: lt", __LINE__,
            SVI("return 3 < 5;\n"),
            .exit_code = 1,
        },
        {
            "cmp: gt", __LINE__,
            SVI("return 5 > 3;\n"),
            .exit_code = 1,
        },
        {
            "cmp: le", __LINE__,
            SVI("return 5 <= 5;\n"),
            .exit_code = 1,
        },
        {
            "cmp: ge", __LINE__,
            SVI("return 5 >= 6;\n"),
            .exit_code = 0,
        },
        // Logical
        {
            "logical: and true", __LINE__,
            SVI("return 1 && 2;\n"),
            .exit_code = 1,
        },
        {
            "logical: and false", __LINE__,
            SVI("return 1 && 0;\n"),
            .exit_code = 0,
        },
        {
            "logical: or", __LINE__,
            SVI("return 0 || 5;\n"),
            .exit_code = 1,
        },
        {
            "logical: not", __LINE__,
            SVI("return !0;\n"),
            .exit_code = 1,
        },
        {
            "logical: not truthy", __LINE__,
            SVI("return !42;\n"),
            .exit_code = 0,
        },
        {
            "logical: short circuit and", __LINE__,
            SVI("int x = 0;\n"
               "0 && (x = 1);\n"
               "return x;\n"),
            .exit_code = 0,
        },
        {
            "logical: short circuit or", __LINE__,
            SVI("int x = 0;\n"
               "1 || (x = 1);\n"
               "return x;\n"),
            .exit_code = 0,
        },
        // Ternary
        {
            "ternary: true", __LINE__,
            SVI("return 1 ? 10 : 20;\n"),
            .exit_code = 10,
        },
        {
            "ternary: false", __LINE__,
            SVI("return 0 ? 10 : 20;\n"),
            .exit_code = 20,
        },
        // Comma
        {
            "comma", __LINE__,
            SVI("return (1, 2, 3);\n"),
            .exit_code = 3,
        },
        // Assignment operators
        {
            "assign: plus_eq", __LINE__,
            SVI("int x = 10;\n"
               "x += 5;\n"
               "return x;\n"),
            .exit_code = 15,
        },
        {
            "assign: minus_eq", __LINE__,
            SVI("int x = 10;\n"
               "x -= 3;\n"
               "return x;\n"),
            .exit_code = 7,
        },
        {
            "assign: mul_eq", __LINE__,
            SVI("int x = 6;\n"
               "x *= 7;\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "assign: div_eq", __LINE__,
            SVI("int x = 42;\n"
               "x /= 6;\n"
               "return x;\n"),
            .exit_code = 7,
        },
        {
            "assign: mod_eq", __LINE__,
            SVI("int x = 17;\n"
               "x %= 5;\n"
               "return x;\n"),
            .exit_code = 2,
        },
        {
            "assign: and_eq", __LINE__,
            SVI("int x = 0xFF;\n"
               "x &= 0x0F;\n"
               "return x;\n"),
            .exit_code = 15,
        },
        {
            "assign: or_eq", __LINE__,
            SVI("int x = 0xF0;\n"
               "x |= 0x0F;\n"
               "return x;\n"),
            .exit_code = 255,
        },
        {
            "assign: xor_eq", __LINE__,
            SVI("int x = 0xFF;\n"
               "x ^= 0x0F;\n"
               "return x;\n"),
            .exit_code = 240,
        },
        {
            "assign: lshift_eq", __LINE__,
            SVI("int x = 1;\n"
               "x <<= 4;\n"
               "return x;\n"),
            .exit_code = 16,
        },
        {
            "assign: rshift_eq", __LINE__,
            SVI("int x = 256;\n"
               "x >>= 4;\n"
               "return x;\n"),
            .exit_code = 16,
        },
        {
            "assign: signed div_eq", __LINE__,
            SVI("int x = -6;\n"
               "x /= 2;\n"
               "return x + 100;\n"),
            .exit_code = 97,
        },
        {
            "assign: signed mod_eq", __LINE__,
            SVI("int x = -7;\n"
               "x %= 3;\n"
               "return x + 100;\n"),
            .exit_code = 99,
        },
        {
            "assign: signed rshift_eq", __LINE__,
            SVI("int x = -8;\n"
               "x >>= 1;\n"
               "return x + 100;\n"),
            .exit_code = 96,
        },
        {
            "assign: unsigned enum div_eq", __LINE__,
            SVI("typedef enum : unsigned { BIG = 3000000000u } E;\n"
               "E e = BIG;\n"
               "e /= 1000000000u;\n"
               "return (int)e;\n"),
            .exit_code = 3,
        },
        // Increment / Decrement
        {
            "prefix increment", __LINE__,
            SVI("int x = 5;\n"
               "return ++x;\n"),
            .exit_code = 6,
        },
        {
            "postfix increment", __LINE__,
            SVI("int x = 5;\n"
               "int y = x++;\n"
               "return y * 10 + x;\n"),
            .exit_code = 56,
        },
        {
            "prefix decrement", __LINE__,
            SVI("int x = 5;\n"
               "return --x;\n"),
            .exit_code = 4,
        },
        {
            "postfix decrement", __LINE__,
            SVI("int x = 5;\n"
               "int y = x--;\n"
               "return y * 10 + x;\n"),
            .exit_code = 54,
        },
        // Cast
        {
            "cast: int to char truncation", __LINE__,
            SVI("int x = 257;\n"
               "char c = (char)x;\n"
               "return c;\n"),
            .exit_code = 1,
        },
        // _Bool must canonicalize any nonzero value to exactly 1, not
        // truncate to the low byte.
        {
            "cast: bool from nonzero low byte", __LINE__,
            SVI("int x = 3;\n"
               "_Bool b = x;\n"
               "return b;\n"),
            .exit_code = 1,
        },
        {
            "cast: bool from value with zero low byte", __LINE__,
            SVI("int x = 256;\n"
               "_Bool b = x;\n"
               "return b;\n"),
            .exit_code = 1,
        },
        {
            "cast: bool bits are 0 or 1 (nonzero)", __LINE__,
            SVI("int x = 256;\n"
               "_Bool b = x;\n"
               "return *(unsigned char*)&b;\n"),
            .exit_code = 1,
        },
        {
            "cast: bool bits are 0 or 1 (large)", __LINE__,
            SVI("long x = 0x10000;\n"
               "_Bool b = x;\n"
               "return *(unsigned char*)&b;\n"),
            .exit_code = 1,
        },
        {
            "cast: bool bits are 0 or 1 (zero)", __LINE__,
            SVI("int x = 0;\n"
               "_Bool b = x;\n"
               "return *(unsigned char*)&b;\n"),
            .exit_code = 0,
        },
        {
            "cast: bool from pointer canonicalizes", __LINE__,
            SVI("int obj = 0;\n"
               "int* p = &obj;\n"
               "_Bool b = p;\n"
               "return *(unsigned char*)&b;\n"),
            .exit_code = 1,
        },
        // sizeof
        {
            "sizeof int", __LINE__,
            SVI("return sizeof(int);\n"),
            .exit_code = 4,
        },
        {
            "sizeof char", __LINE__,
            SVI("return sizeof(char);\n"),
            .exit_code = 1,
        },
        {
            "sizeof expr", __LINE__,
            SVI("int x = 0;\n"
               "return sizeof x;\n"),
            .exit_code = 4,
        },
        // If/else
        {
            "if: true branch", __LINE__,
            SVI("if(1) return 10;\n"
               "return 20;\n"),
            .exit_code = 10,
        },
        {
            "if: false branch", __LINE__,
            SVI("if(0) return 10;\n"
               "return 20;\n"),
            .exit_code = 20,
        },
        {
            "if: else", __LINE__,
            SVI("if(0) return 10;\n"
               "else return 20;\n"),
            .exit_code = 20,
        },
        {
            "if: else if chain", __LINE__,
            SVI("int x = 2;\n"
               "if(x == 0) return 0;\n"
               "else if(x == 1) return 1;\n"
               "else if(x == 2) return 2;\n"
               "else return 3;\n"),
            .exit_code = 2,
        },
        // While
        {
            "while", __LINE__,
            SVI("int i = 0;\n"
               "int s = 0;\n"
               "while(i < 5){ s += i; i++; }\n"
               "return s;\n"),
            .exit_code = 10,
        },
        // Do-while
        {
            "do while", __LINE__,
            SVI("int i = 0;\n"
               "do { i++; } while(i < 5);\n"
               "return i;\n"),
            .exit_code = 5,
        },
        {
            "do while: executes at least once", __LINE__,
            SVI("int i = 0;\n"
               "do { i++; } while(0);\n"
               "return i;\n"),
            .exit_code = 1,
        },
        // For loop variants
        {
            "for: empty body", __LINE__,
            SVI("int i;\n"
               "for(i = 0; i < 10; i++);\n"
               "return i;\n"),
            .exit_code = 10,
        },
        {
            "for: no init", __LINE__,
            SVI("int i = 5;\n"
               "for(; i < 10; i++);\n"
               "return i;\n"),
            .exit_code = 10,
        },
        // Break / Continue
        {
            "break", __LINE__,
            SVI("int i = 0;\n"
               "while(1){ if(i == 5) break; i++; }\n"
               "return i;\n"),
            .exit_code = 5,
        },
        {
            "continue", __LINE__,
            SVI("int s = 0;\n"
               "for(int i = 0; i < 10; i++){\n"
               "  if(i % 2 == 0) continue;\n"
               "  s += i;\n"
               "}\n"
               "return s;\n"),
            .exit_code = 25,
        },
        {
            "break: nested loops", __LINE__,
            SVI("int s = 0;\n"
               "for(int i = 0; i < 5; i++){\n"
               "  for(int j = 0; j < 5; j++){\n"
               "    if(j == 2) break;\n"
               "    s++;\n"
               "  }\n"
               "}\n"
               "return s;\n"),
            .exit_code = 10,
        },
        // Switch
        {
            "switch: match", __LINE__,
            SVI("int x = 2;\n"
               "switch(x){\n"
               "  case 1: return 10;\n"
               "  case 2: return 20;\n"
               "  case 3: return 30;\n"
               "}\n"
               "return 0;\n"),
            .exit_code = 20,
        },
        {
            "switch: default", __LINE__,
            SVI("int x = 99;\n"
               "switch(x){\n"
               "  case 1: return 10;\n"
               "  default: return 42;\n"
               "}\n"
               "return 0;\n"),
            .exit_code = 42,
        },
        {
            "switch: fallthrough", __LINE__,
            SVI("int x = 1;\n"
               "int r = 0;\n"
               "switch(x){\n"
               "  case 1: r += 1;\n"
               "  case 2: r += 2;\n"
               "  case 3: r += 3; break;\n"
               "  case 4: r += 4;\n"
               "}\n"
               "return r;\n"),
            .exit_code = 6,
        },
        {
            "switch: unsigned enum", __LINE__,
            SVI("typedef enum : unsigned { A = 3000000000u, B = 100u } E;\n"
               "E e = A;\n"
               "switch(e){\n"
               "  case 100u: return 1;\n"
               "  case 3000000000u: return 2;\n"
               "  default: return 3;\n"
               "}\n"),
            .exit_code = 2,
        },
        {
            "switch: negative case converts to unsigned controlling type", __LINE__,
            SVI("unsigned x=4294967295u;\n"
                "switch(x){case -1: return 1; default: return 0;}\n"),
            .exit_code = 1,
        },
        {
            "switch: unsigned case converts to signed controlling type", __LINE__,
            SVI("int x=-1;\n"
                "switch(x){case 4294967295u: return 1; default: return 0;}\n"),
            .exit_code = 1,
        },
        {
            "switch: wide enum case converts to controlling width", __LINE__,
            SVI("enum W:unsigned __int128 {HIGH=((unsigned __int128)1<<100)+7};\n"
                "int x=7; switch(x){case HIGH: return 1; default: return 0;}\n"),
            .exit_code = 1,
        },
        {
            "switch: packed enum is promoted before converting cases", __LINE__,
            SVI("enum __attribute__((packed)) P {LAST=255}; enum P x=LAST;\n"
                "switch(x){case -1: return 0; case 511: return 0; case 255: return 1;}\n"
                "return 0;\n"),
            .exit_code = 1,
        },
        {
            "bitfield arithmetic uses the promoted value width", __LINE__,
            SVI("enum E:unsigned {ONE=1};\n"
                "struct B {unsigned small:3,full:32; enum E bits:3;} b={1,4294967295u,ONE};\n"
                "static constexpr struct B c={1,4294967295u,ONE};\n"
                "static int complement=~c.bits, comparison=c.small < -1;\n"
                "_Any boxed=+b.small;\n"
                "return !(b.small < -1)&&-b.small<0&&~b.small==-2&&~b.bits==-2"
                    "&&b.full==4294967295u&&complement==-2&&comparison==0"
                    "&&boxed.type==int&&boxed.as(int)==1;\n"),
            .exit_code = 1,
        },
        {
            "compound assignments compute before converting to the object type", __LINE__,
            SVI("int x=3; x*=0.5; if(x!=1) return 0;\n"
                "unsigned char n=200; n/=300; if(n!=0) return 0;\n"
                "int y=100000; y/=4294967297ll; if(y!=0) return 0;\n"
                "_Bool b=1; b+=1; if(*(unsigned char*)&b!=1) return 0;\n"
                "return 1;\n"),
            .exit_code = 1,
        },
        {
            "bitfield compound assignments use promoted arithmetic", __LINE__,
            SVI("struct S {unsigned n:3; signed s:3;} a={1,-1};\n"
                "unsigned r=(a.n/=-1); if(r!=7||a.n!=7) return 0;\n"
                "a.n=5; a.n%= -2; if(a.n!=1) return 0;\n"
                "a.n=3; a.n*=0.5; if(a.n!=1) return 0;\n"
                "a.s/=2u; if(a.s!=-1) return 0; return 1;\n"),
            .exit_code = 1,
        },
        {
            "compound assignments evaluate the destination once", __LINE__,
            SVI("struct S {unsigned n:3;} a[2]={{3},{5}}; int i=0,j=0;\n"
                "unsigned r=(a[i++].n*= (j++,0.5));\n"
                "return i==1&&j==1&&r==1&&a[0].n==1&&a[1].n==5;\n"),
            .exit_code = 1,
        },
        {
            "atomic compound assignments convert after computing", __LINE__,
            SVI("_Atomic int x=3; x*=0.5; if(x!=1) return 0;\n"
                "_Atomic unsigned char n=200; n/=300; if(n!=0) return 0;\n"
                "_Atomic _Bool b=1; b+=1; return b;\n"),
            .exit_code = 1,
        },
        {
            "compound conversions preserve wide integers and enum storage", __LINE__,
            SVI("enum __attribute__((packed)) E {LAST=255}; enum E e=LAST; e/=300;\n"
                "unsigned __int128 wide=(unsigned __int128)1<<100; int n=100; n/=wide;\n"
                "float f=3; f*=0.5; double d=4; d/=2;\n"
                "return e==0&&n==0&&f==1.5&&d==2;\n"),
            .exit_code = 1,
        },
        {
            "bool increments store canonical values and preserve pre/post results", __LINE__,
            SVI("_Bool b=1; int old=b++; if(old!=1||*(unsigned char*)&b!=1) return 0;\n"
                "if(++b!=1||*(unsigned char*)&b!=1) return 0;\n"
                "b=0; if(b--!=0||*(unsigned char*)&b!=1) return 0;\n"
                "if(--b!=0||b!=0) return 0;\n"
                "_Bool* p=&b; ++*p; ++*p; if(*(unsigned char*)p!=1) return 0;\n"
                "return 1;\n"),
            .exit_code = 1,
        },
        {
            "bool bitfield increments convert before truncating", __LINE__,
            SVI("struct S {_Bool b:1; unsigned neighbor:2;} a[2]={{1,3},{0,2}}; int i=0;\n"
                "if(a[i++].b++!=1||i!=1||a[0].b!=1||a[0].neighbor!=3) return 0;\n"
                "if(++a[0].b!=1||a[0].b!=1) return 0;\n"
                "if(a[1].b--!=0||a[1].b!=1||a[1].neighbor!=2) return 0;\n"
                "return --a[1].b==0&&a[1].b==0;\n"),
            .exit_code = 1,
        },
        {
            "atomic bool increments store canonical values", __LINE__,
            SVI("_Atomic _Bool b=1; if(b++!=1||b!=1||++b!=1||b!=1) return 0;\n"
                "b=0; if(b--!=0||b!=1||--b!=0||b!=0) return 0;\n"
                "++b; ++b; return b==1;\n"),
            .exit_code = 1,
        },
        {
            "bool enum increments use boolean storage semantics", __LINE__,
            SVI("enum E:_Bool {ZERO=0,ONE=1}; enum E e=ONE;\n"
                "if(e++!=ONE||*(unsigned char*)&e!=1) return 0;\n"
                "e=ZERO; if(--e!=ONE||*(unsigned char*)&e!=1) return 0;\n"
                "struct S {enum E e:1;} s={ONE};\n"
                "return ++s.e==ONE&&s.e==ONE;\n"),
            .exit_code = 1,
        },
        {
            "unqualified update results preserve atomic and volatile accesses", __LINE__,
            SVI("_Atomic int a=0; volatile int v=0;\n"
                "int x=(a=3),y=++a,z=a++; if(x!=3||y!=4||z!=4||a!=5) return 0;\n"
                "x=(a+=2); y=--a; z=a--; if(x!=7||y!=6||z!=6||a!=5) return 0;\n"
                "x=(v=3); y=++v; z=v++; if(x!=3||y!=4||z!=4||v!=5) return 0;\n"
                "x=(v+=2); y=--v; z=v--; return x==7&&y==6&&z==6&&v==5;\n"),
            .exit_code = 1,
        },
        {
            "conditional array and function decay with null operands", __LINE__,
            SVI("int f(void){return 7;} int choose(int n){\n"
                "const int a[2]={3,4}; const int* p=n?a:0; const int* q=n?nullptr:a;\n"
                "int (*g)(void)=n?f:0; int (*h)(void)=n?0:f;\n"
                "return n?(p[1]==4&&q==nullptr&&g()==7&&h==nullptr)"
                ":(p==nullptr&&q[0]==3&&g==nullptr&&h()==7);}\n"
                "return choose(0)&&choose(1);\n"),
            .exit_code = 1,
        },
        {
            "array decay to embedded base adjusts the address", __LINE__,
            SVI("struct B {int value;}; struct D {int pad; struct B;};\n"
                "struct D a[2]={{11,{7}},{12,{8}}}; struct B* b=a;\n"
                "if((void*)b!=(void*)&a[0].value||b->value!=7) return 0;\n"
                "static struct D data[1]={{13,{9}}}; static struct B* stored=data;\n"
                "return (void*)stored==(void*)&data[0].value&&stored->value==9;\n"),
            .exit_code = 1,
        },
        {
            "static pointer casts preserve symbolic subobject values", __LINE__,
            SVI("static int target; constexpr struct P {int* p;} s={&target};\n"
                "static const int* qualified=s.p; static void* opaque=s.p;\n"
                "constexpr int* a[1]={&target}; static const int* element=a[0];\n"
                "return qualified==&target&&opaque==&target&&element==&target;\n"),
            .exit_code = 1,
        },
        {
            "compatible function pointer conversions and conditional calls", __LINE__,
            SVI("int f(const int x){return x+1;} int (*p)(int)=f; int (*old)()=f;\n"
                "int (*qualified)(const int)=p; int (*converted)(int)=old;\n"
                "int n=0; int a=(n?old:p)(6); n=1; int b=(n?old:p)(7);\n"
                "return p==qualified&&p==old&&qualified(4)==5&&converted(5)==6&&a==7&&b==8;\n"),
            .exit_code = 1,
        },
        {
            "evaluated zero initializes and compares null pointers", __LINE__,
            SVI("enum E {ZERO=0}; constexpr int zero=0; static int* a=1-1;\n"
                "static int* b=(int)0; static int* c=ZERO; static int* d=zero;\n"
                "int value=7; int* p=&value; int choose=1;\n"
                "int* q=choose?p:(2-2); int* r=choose?(3-3):p;\n"
                "_Bool flag=nullptr; _Any boxed=nullptr; int* wide=0ui128;\n"
                "return a==0&&b==0&&c==0&&d==0&&q==p&&r==(4-4)&&!flag"
                "&&boxed.type==typeof(nullptr)&&wide==0;\n"),
            .exit_code = 1,
        },
        {
            "pointer arithmetic retains qualified accesses and scales complete types", __LINE__,
            SVI("int a[3]={3,5,7}; int* volatile p=a; _Atomic(int*) q=a;\n"
                "int* x=p+1; int* y=1+p; int* z=q+2;\n"
                "if(*x!=5||y!=x||*z!=7||p!=a||q!=a) return 0;\n"
                "enum E:unsigned __int128; enum E wide[2]={1,2};\n"
                "enum E* e=wide; ++e; e-=1; e++;\n"
                "return e==wide+1&&*e==2&&e-wide==1;\n"),
            .exit_code = 1,
        },
        {
            "function designator boolean conversions and conditions use its address", __LINE__,
            SVI("int f(void){return 7;} _Bool b=f; static _Bool stored=f;\n"
                "if(!f||!b||!stored) return 0; int n=f?3:0;\n"
                "if(!(f&&1)||!(0||f)||n!=3) return 0;\n"
                "int (*p)(void)=f; if(!(*p)||!(_Bool)*p) return 0; return 1;\n"),
            .exit_code = 1,
        },
        {
            "function-valued comma and statement expressions preserve side effects", __LINE__,
            SVI("int f(void){return 7;} int i=0; int (*p)(void)=f;\n"
                "_Bool a=(i++,f); _Bool b=(i++,*p);\n"
                "_Bool c=({i++; *p;}); return a&&b&&c&&i==3;\n"),
            .exit_code = 1,
        },
        {
            "calls through function-valued expressions preserve side effects", __LINE__,
            SVI("int f(int x){return x+1;} int i=0; int (*p)(int)=f;\n"
                "int a=(i++,f)(4); int b=({i++; f;})(5); int c=(i++,*p)(6);\n"
                "return a==5&&b==6&&c==7&&i==3;\n"),
            .exit_code = 1,
        },
        // Goto
        {
            "goto: forward", __LINE__,
            SVI("goto end;\n"
               "return 1;\n"
               "end:\n"
               "return 0;\n"),
            .exit_code = 0,
        },
        {
            "goto: backward (loop)", __LINE__,
            SVI("int i = 0;\n"
               "top:\n"
               "if(i >= 5) return i;\n"
               "i++;\n"
               "goto top;\n"),
            .exit_code = 5,
        },
        {
            "goto: forward within function", __LINE__,
            SVI("int f(void){\n"
               "    int x = 1;\n"
               "    goto skip;\n"
               "    x = 99;\n"
               "skip:\n"
               "    x += 2;\n"
               "    return x;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 3,
        },
        {
            "goto: same label name in two functions", __LINE__,
            SVI("int f(void){ goto out; out: return 1; }\n"
               "int g(void){ goto out; out: return 2; }\n"
               "return f() * 10 + g();\n"),
            .exit_code = 12,
        },
        {
            "flat expr: && || normalize to 0/1", __LINE__,
            SVI("int r = (5 && 7) + (0 || 3) * 10;\n"
               "return r;\n"),
            .exit_code = 11,
        },
        {
            "flat expr: && short circuits side effect", __LINE__,
            SVI("int n = 0;\n"
               "int r = 0 && (n = 5);\n"
               "return n * 10 + r;\n"),
            .exit_code = 0,
        },
        {
            "flat expr: || short circuits side effect", __LINE__,
            SVI("int n = 0;\n"
               "int r = 1 || (n = 5);\n"
               "return n * 10 + r;\n"),
            .exit_code = 1,
        },
        {
            "flat expr: && short circuits in if condition", __LINE__,
            SVI("int n = 0;\n"
               "if(0 && (n = 5)) return 9;\n"
               "if(1 && (n = 3)) return n;\n"
               "return 9;\n"),
            .exit_code = 3,
        },
        {
            "flat expr: || short circuits in if condition", __LINE__,
            SVI("int n = 0;\n"
               "if(1 || (n = 5)) return n;\n"
               "return 9;\n"),
            .exit_code = 0,
        },
        {
            "flat expr: && value is canonical in switch", __LINE__,
            SVI("switch(5 && 7){\n"
               "  case 0: return 10;\n"
               "  case 1: return 11;\n"
               "  default: return 12;\n"
               "}\n"),
            .exit_code = 11,
        },
        {
            "flat expr: negative zero is falsy", __LINE__,
            SVI("double z = -0.0;\n"
               "if(z) return 1;\n"
               "while(z) return 2;\n"
               "return z ? 3 : 4;\n"),
            .exit_code = 4,
        },
        {
            "flat expr: ternary in condition", __LINE__,
            SVI("int f(int a){\n"
               "    int n = 0;\n"
               "    if(a ? a - 1 : a + 1) n += 1;\n"
               "    while(n < (a ? 3 : 1)) n++;\n"
               "    return n;\n"
               "}\n"
               "return f(0) * 10 + f(2);\n"),
            .exit_code = 13,
        },
        {
            "flat expr: comma in condition", __LINE__,
            SVI("int n = 0;\n"
               "int x = (n = 4, n + 1);\n"
               "if((n = 7, 0)) return 1;\n"
               "return x * 10 + n;\n"),
            .exit_code = 57,
        },
        {
            "flat expr: nested short circuit in loop cond", __LINE__,
            SVI("int i = 0, evals = 0;\n"
               "int bump(void){ evals++; return 1; }\n"
               "while(i < 3 && bump()){ i++; }\n"
               "return i * 10 + evals;\n"),
            .exit_code = 33,
        },
        {
            "flat expr: var condition re-read each iteration", __LINE__,
            SVI("int f(void){\n"
               "    int x = 3, n = 0;\n"
               "    while(x){ x--; n++; }\n"
               "    if(x) return 99;\n"
               "    return n + 40;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 43,
        },
        {
            "flat expr: ternary selects var into return", __LINE__,
            SVI("int f(int c){ int b = 7, d = 9; return c ? b : d; }\n"
               "return f(1) * 10 + f(0);\n"),
            .exit_code = 79,
        },
        {
            "flat expr: struct ternary return", __LINE__,
            SVI("struct P { int a, b; };\n"
               "struct P f(int c){ struct P x = {1,2}, y = {3,4}; return c ? x : y; }\n"
               "return f(1).a * 10 + f(0).b;\n"),
            .exit_code = 14,
        },
        {
            "flat expr: discarded ternary side effects", __LINE__,
            SVI("int n = 0;\n"
               "1 ? (void)(n = 5) : (void)(n = 6);\n"
               "0 ? (void)(n += 100) : (void)(n += 10);\n"
               "return n;\n"),
            .exit_code = 15,
        },
        {
            "flat expr: comma statement", __LINE__,
            SVI("int n = 0;\n"
               "n = 3, n++;\n"
               "return n;\n"),
            .exit_code = 4,
        },
        {
            "flat expr: return short circuit", __LINE__,
            SVI("int f(int a, int b){ return a && b; }\n"
               "int g(int a, int b){ return a || b; }\n"
               "return f(2,0)*100 + f(2,3)*10 + g(0,0);\n"),
            .exit_code = 10,
        },
        {
            "flat expr: assignment chains and arithmetic", __LINE__,
            SVI("int f(void){\n"
               "    int x = 0, y = 0;\n"
               "    x = x + 5;\n"
               "    y = x * 3 - 4;\n"
               "    x = y = y / 2;\n"
               "    return x * 10 + y;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 55,
        },
        {
            "flat expr: unsigned wraparound compare", __LINE__,
            SVI("int f(void){\n"
               "    unsigned a = 0;\n"
               "    if(a - 1 > 100) return 1;\n"
               "    return 2;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 1,
        },
        {
            "flat expr: signed compare and division", __LINE__,
            SVI("int f(int a, int b){\n"
               "    int q = 0;\n"
               "    q = a / b;\n"
               "    if(-1 < q) return q + 10;\n"
               "    return -q;\n"
               "}\n"
               "return f(-7, 2) * 10 + f(9, 2);\n"),
            .exit_code = 44,
        },
        {
            "flat expr: assignment value used in condition", __LINE__,
            SVI("int f(void){\n"
               "    int x = 0, n = 0;\n"
               "    while((x = x + 2) < 10) n = n + 1;\n"
               "    return x * 10 + n;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 104,
        },
        {
            "flat expr: logical rhs reads assigned var", __LINE__,
            SVI("int seen = 0;\n"
               "int probe(int v){ seen = v; return 1; }\n"
               "int f(void){\n"
               "    int x = 7;\n"
               "    x = x && probe(x);\n"
               "    return x;\n"
               "}\n"
               "return f() * 10 + seen;\n"),
            .exit_code = 17,
        },
        {
            "flat expr: bit ops", __LINE__,
            SVI("int f(void){\n"
               "    int a = 0;\n"
               "    a = (5 << 2) | 3;\n"
               "    a = a ^ (a & 6);\n"
               "    a = a % 7;\n"
               "    return a + (1 << 4);\n"
               "}\n"
               "return f();\n"),
            .exit_code = 19,
        },
        {
            "flat expr: mixed width arithmetic", __LINE__,
            SVI("int f(void){\n"
               "    unsigned char c = 200;\n"
               "    short s = -5;\n"
               "    int r = 0;\n"
               "    r = c + 100;\n"
               "    if(s < 0) r = r + 1;\n"
               "    return r - 250;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 51,
        },
        {
            "flat expr: casts truncate and extend", __LINE__,
            SVI("int f(void){\n"
               "    int big = 0x1234;\n"
               "    char c = 0;\n"
               "    c = (char)big;\n"
               "    signed char s = -1;\n"
               "    unsigned u = 0;\n"
               "    u = (unsigned)s;\n"
               "    int r = 0;\n"
               "    r = (int)(u >> 24);\n"
               "    return c + (r == 255);\n"
               "}\n"
               "return f();\n"),
            .exit_code = 53,
        },
        {
            "flat expr: unary ops", __LINE__,
            SVI("int f(void){\n"
               "    int a = 5;\n"
               "    int r = 0;\n"
               "    r = -a + 10;\n"
               "    r = r + (~a & 15);\n"
               "    if(!a) return 99;\n"
               "    if(!!a) r = r + 1;\n"
               "    return r + !0 * 10;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 26,
        },
        {
            "flat expr: lognot of double", __LINE__,
            SVI("int f(void){\n"
               "    double z = -0.0;\n"
               "    double v = 0.5;\n"
               "    return !z * 10 + !v;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 10,
        },
        {
            "flat expr: classic for loop with compound assign", __LINE__,
            SVI("int f(void){\n"
               "    int s = 0;\n"
               "    for(int i = 0; i < 5; i++) s += i;\n"
               "    for(int i = 10; i > 0; --i) s -= 1;\n"
               "    return s;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 0,
        },
        {
            "flat expr: post and pre value semantics", __LINE__,
            SVI("int f(void){\n"
               "    int i = 3, j = 0, k = 0;\n"
               "    j = i++;\n"
               "    k = --i;\n"
               "    return j * 100 + k * 10 + i;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 333,
        },
        {
            "flat expr: pointer inc dec", __LINE__,
            SVI("int f(void){\n"
               "    int a[3];\n"
               "    a[0] = 5; a[1] = 6; a[2] = 7;\n"
               "    int* p = a;\n"
               "    p++;\n"
               "    ++p;\n"
               "    int r = *p;\n"
               "    p--;\n"
               "    return r * 10 + *p;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 76,
        },
        {
            "flat expr: compound assign chain", __LINE__,
            SVI("int f(void){\n"
               "    int x = 2;\n"
               "    x *= 3;\n"
               "    x <<= 2;\n"
               "    x |= 1;\n"
               "    x %= 100;\n"
               "    x -= 5;\n"
               "    x ^= 3;\n"
               "    x &= 0xff;\n"
               "    unsigned u = 0x80000000u;\n"
               "    u >>= 24;\n"
               "    return x + (u == 0x80);\n"
               "}\n"
               "return f();\n"),
            .exit_code = 24,
        },
        {
            "flat expr: compound assign value used", __LINE__,
            SVI("int f(void){\n"
               "    int x = 5, y = 0;\n"
               "    y = (x += 3);\n"
               "    while((x -= 2) > 0) y++;\n"
               "    return y * 10 + (x == 0);\n"
               "}\n"
               "return f();\n"),
            .exit_code = 111,
        },
        {
            "flat expr: float arithmetic", __LINE__,
            SVI("int f(void){\n"
               "    float x = 1.5f;\n"
               "    float y = 0;\n"
               "    y = x * 2.0f + 1.0f;\n"
               "    y = y - 0.5f;\n"
               "    y = -y;\n"
               "    return (int)(y * -2.0f);\n"
               "}\n"
               "return f();\n"),
            .exit_code = 7,
        },
        {
            "flat expr: float compound assignment", __LINE__,
            SVI("int f(void){\n"
               "    float x = 10.0f;\n"
               "    x += 5.0f;\n"
               "    x -= 3.0f;\n"
               "    x *= 2.0f;\n"
               "    x /= 4.0f;\n"
               "    x += 1;\n"     // int rhs, parser casts to float
               "    double d = 2.0;\n"
               "    d *= 1.5;\n"
               "    d += x;\n"     // float rhs, parser casts to double
               "    return (int)(x * 10.0f) + (int)d;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 80,
        },
        {
            "flat expr: double arithmetic and compares", __LINE__,
            SVI("int f(void){\n"
               "    double a = 1.5, b = 2.5;\n"
               "    int r = 0;\n"
               "    double c = 0;\n"
               "    c = a * b + 0.25;\n"
               "    if(a < b) r += 1;\n"
               "    if(c == 4.0) r += 2;\n"
               "    if(a >= b) r += 100;\n"
               "    if(b != b) r += 100;\n"
               "    return r + (int)c;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 7,
        },
        {
            "flat expr: int float conversions", __LINE__,
            SVI("int f(void){\n"
               "    int i = 7;\n"
               "    double d = 0;\n"
               "    d = (double)i / 2.0;\n"
               "    float g = 0;\n"
               "    g = (float)(d + 0.25);\n"
               "    int r = 0;\n"
               "    r = (int)(g * 4.0f);\n"
               "    unsigned u = 3000000000u;\n"
               "    d = (double)u;\n"
               "    if(d > 2000000000.0) r += 10;\n"
               "    return r + (int)-1.5;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 24,
        },
        {
            "flat expr: deref load and store", __LINE__,
            SVI("int f(void){\n"
               "    int x = 5;\n"
               "    int* p = &x;\n"
               "    *p = 7;\n"
               "    *p = *p + 1;\n"
               "    return *p + x;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 16,
        },
        {
            "flat expr: struct through pointer", __LINE__,
            SVI("struct P { int a, b; };\n"
               "int f(void){\n"
               "    struct P s;\n"
               "    s.a = 1; s.b = 2;\n"
               "    struct P* q = &s;\n"
               "    q->a = 3;\n"
               "    q->b = q->a + 4;\n"
               "    return s.a * 10 + s.b;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 37,
        },
        {
            "flat expr: nested member of local", __LINE__,
            SVI("struct Inner { int v; };\n"
               "struct Outer { int pad; struct Inner in; };\n"
               "int f(void){\n"
               "    struct Outer o;\n"
               "    o.pad = 1;\n"
               "    o.in.v = 5;\n"
               "    o.in.v += 2;\n"
               "    o.in.v++;\n"
               "    return o.in.v * 10 + o.pad;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 81,
        },
        {
            "flat expr: pointer subscript", __LINE__,
            SVI("int f(void){\n"
               "    int a[4];\n"
               "    a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4;\n"
               "    int* p = a;\n"
               "    p[2] = 9;\n"
               "    int s = 0;\n"
               "    for(int i = 0; i < 4; i++) s += p[i];\n"
               "    int* q = &p[1];\n"
               "    return s + *q;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 18,
        },
        {
            "flat expr: struct copy through pointer", __LINE__,
            SVI("struct P { int a, b; };\n"
               "int f(void){\n"
               "    struct P s, t;\n"
               "    s.a = 3; s.b = 4;\n"
               "    t.a = 0; t.b = 0;\n"
               "    struct P* q = &t;\n"
               "    *q = s;\n"
               "    return t.a * 10 + t.b;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 34,
        },
        {
            "flat expr: recursive call", __LINE__,
            SVI("int fib(int n){\n"
               "    if(n < 2) return n;\n"
               "    return fib(n - 1) + fib(n - 2);\n"
               "}\n"
               "return fib(10);\n"),
            .exit_code = 55,
        },
        {
            "flat expr: compound assign through memory", __LINE__,
            SVI("struct P { int a, b; };\n"
               "int g = 100;\n"
               "int f(void){\n"
               "    g += 5;\n"                 // static: 105
               "    struct P s; s.a = 2; s.b = 3;\n"
               "    struct P* q = &s;\n"
               "    q->a *= 10;\n"             // through pointer: a=20
               "    int arr[3]; arr[0]=1; arr[1]=4; arr[2]=9;\n"
               "    int* p = arr;\n"
               "    p[1] += 6;\n"              // subscript: arr[1]=10
               "    int r = (q->b <<= 2);\n"   // value used: b=12, r=12
               "    return g + q->a + p[1] + r;\n"  // 105+20+10+12 = 147
               "}\n"
               "return f();\n"),
            .exit_code = 147,
        },
        {
            "flat expr: incdec through memory", __LINE__,
            SVI("int g = 10;\n"
               "struct P { int a, b; };\n"
               "int f(void){\n"
               "    ++g;\n"               // static pre: g=11
               "    struct P s; s.a=5; s.b=8;\n"
               "    struct P* q = &s;\n"
               "    int x = q->a++;\n"     // post through ptr: x=5, s.a=6
               "    int arr[3]; arr[0]=1; arr[1]=2; arr[2]=3;\n"
               "    int* p = arr;\n"
               "    p[2]--;\n"             // post through subscript, discarded: arr[2]=2
               "    int y = --p[0];\n"     // pre through subscript: y=0, arr[0]=0
               "    return g*100 + x*10 + q->a + arr[2] + y;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 1158,
        },
        {
            "flat expr: float compound assign through pointer", __LINE__,
            SVI("int f(void){\n"
               "    double d = 4.0;\n"
               "    double* p = &d;\n"
               "    *p += 1.0;\n"
               "    *p *= 3.0;\n"
               "    return (int)*p;\n"          // (4+1)*3 = 15
               "}\n"
               "return f();\n"),
            .exit_code = 15,
        },
        {
            "flat expr: float incdec pre and post value", __LINE__,
            SVI("int f(void){\n"
               "    double d = 5.0;\n"
               "    double a = d++;\n"          // post: a=5, d=6
               "    double b = ++d;\n"          // pre:  b=7, d=7
               "    float g = 2.0f;\n"
               "    float c = g--;\n"           // post: c=2, g=1
               "    return (int)(a*100 + b*10 + d + c);\n"  // 500+70+7+2=579
               "}\n"
               "return f();\n"),
            .exit_code = 579,
        },
        {
            "flat expr: float incdec through pointer", __LINE__,
            SVI("int f(void){\n"
               "    double d = 10.0;\n"
               "    double* p = &d;\n"
               "    ++*p;\n"                     // d=11
               "    (*p)--;\n"                   // d=10, discarded
               "    double x = (*p)++;\n"        // x=10, d=11
               "    return (int)(x*10 + d);\n"   // 100+11=111
               "}\n"
               "return f();\n"),
            .exit_code = 111,
        },
        {
            "flat expr: atomic float incdec", __LINE__,
            SVI("int f(void){\n"
               "    _Atomic double d = 3.0;\n"
               "    double a = d++;\n"           // post: a=3, d=4
               "    double b = ++d;\n"           // pre:  b=5, d=5
               "    d--;\n"                       // d=4, discarded
               "    return (int)(a*100 + b*10 + d);\n"  // 300+50+4=354
               "}\n"
               "return f();\n"),
            .exit_code = 354,
        },
        {
            "flat expr: atomic float compound assign", __LINE__,
            SVI("int f(void){\n"
               "    _Atomic double d = 4.0;\n"
               "    d += 1.0;\n"                  // 5, discarded
               "    d *= 3.0;\n"                  // 15, discarded
               "    double v = (d -= 5.0);\n"     // 10, value used
               "    _Atomic float g = 8.0f;\n"
               "    g /= 2.0f;\n"                 // 4
               "    return (int)(v*10 + g);\n"    // 100+4=104
               "}\n"
               "return f();\n"),
            .exit_code = 104,
        },
        {
            "flat expr: atomic 128-bit incdec", __LINE__,
            SVI("int f(void){\n"
               "    _Atomic __int128 x = 100;\n"
               "    __int128 a = x++;\n"          // post: a=100, x=101
               "    __int128 b = ++x;\n"          // pre:  b=102, x=102
               "    x--;\n"                        // x=101, discarded
               "    return (int)(a + b + x);\n"   // 100+102+101=303
               "}\n"
               "return f();\n"),
            .exit_code = 303,
        },
        {
            "flat expr: atomic 128-bit compound assign", __LINE__,
            SVI("int f(void){\n"
               "    _Atomic __int128 x = 6;\n"
               "    x += 4;\n"                     // 10, discarded
               "    x *= 5;\n"                     // 50, discarded (rmw can't; cas loop)
               "    __int128 v = (x -= 2);\n"      // 48, value used
               "    return (int)v;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 48,
        },
        {
            "flat expr: add overflow builtin", __LINE__,
            SVI("int f(void){\n"
               "    int r;\n"
               "    int of1 = __builtin_add_overflow(2000000000, 2000000000, &r);\n" // overflows int
               "    int r2;\n"
               "    int of2 = __builtin_add_overflow(3, 4, &r2);\n"                  // r2=7, of2=0
               "    return of1*1000 + of2*100 + r2;\n"                               // 1000+0+7
               "}\n"
               "return f();\n"),
            .exit_code = 1007,
        },
        {
            "flat expr: mul overflow into narrow type", __LINE__,
            SVI("int f(void){\n"
               "    unsigned char r;\n"
               "    int of = __builtin_mul_overflow(20, 20, &r);\n"    // 400 & 255 = 144, of=1
               "    unsigned char r2;\n"
               "    int of2 = __builtin_mul_overflow(10, 10, &r2);\n"  // 100 fits, of2=0
               "    return of*1000 + r + of2*10 + r2;\n"              // 1000+144+0+100
               "}\n"
               "return f();\n"),
            .exit_code = 1244,
        },
        {
            "flat expr: sub overflow mixed signedness", __LINE__,
            SVI("int f(void){\n"
               "    unsigned u;\n"
               "    int of = __builtin_sub_overflow(3, 5, &u);\n"   // -2 doesn't fit unsigned: of=1
               "    int s;\n"
               "    int of2 = __builtin_sub_overflow(5, 3, &s);\n"  // s=2, of2=0
               "    return of*100 + of2*10 + s + (u == 4294967294u ? 5 : 0);\n" // 100+0+2+5
               "}\n"
               "return f();\n"),
            .exit_code = 107,
        },
        {
            "flat expr: overflow builtin in statement context", __LINE__,
            SVI("int f(void){\n"
               "    long r = 0;\n"
               "    __builtin_add_overflow(1000, 2000, &r);\n"  // value discarded, r=3000
               "    return (int)r;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 3000,
        },
        {
            "flat expr: umul128", __LINE__,
            SVI("int f(void){\n"
               "    unsigned long long hi;\n"
               "    unsigned long long lo = _umul128(0x100000000ULL, 0x100000000ULL, &hi);\n" // 2^64
               "    return (int)(hi*10 + lo);\n"  // hi=1, lo=0 -> 10
               "}\n"
               "return f();\n"),
            .exit_code = 10,
        },
        {
            "flat expr: umul128 nonzero low", __LINE__,
            SVI("int f(void){\n"
               "    unsigned long long hi;\n"
               "    unsigned long long lo = _umul128(0xFFFFFFFFFFFFFFFFULL, 2ULL, &hi);\n" // 2^65-2
               "    return (int)(hi*100 + (lo == 0xFFFFFFFFFFFFFFFEULL ? 42 : 0));\n"       // 100+42
               "}\n"
               "return f();\n"),
            .exit_code = 142,
        },
        {
            "flat expr: umul128 statement context", __LINE__,
            SVI("int f(void){\n"
               "    unsigned long long hi = 0;\n"
               "    _umul128(0x100000000ULL, 0x100000000ULL, &hi);\n" // low discarded, hi=1
               "    return (int)hi;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 1,
        },
        {
            "flat expr: popcount clz ctz", __LINE__,
            SVI("int f(void){\n"
               "    int a = __builtin_popcount(0xF0u);\n"          // 4
               "    int b = __builtin_ctz(0x10u);\n"               // 4
               "    int c = __builtin_clz(1u);\n"                  // 31 (32-bit operand)
               "    int d = __builtin_clzll(1ull);\n"             // 63 (64-bit operand)
               "    int e = __builtin_popcountll(0xFFFFFFFFFFFFFFFFull);\n" // 64
               "    return a*10000 + b*1000 + c*10 + d + e;\n"     // 40000+4000+310+63+64
               "}\n"
               "return f();\n"),
            .exit_code = 44437,
        },
        {
            "flat expr: clz ctz of zero", __LINE__,
            SVI("int f(void){\n"
               "    unsigned x = 0;\n"
               "    int c = __builtin_clz(x);\n"    // width: 32
               "    int t = __builtin_ctz(x);\n"    // width: 32
               "    return c + t;\n"                 // 64
               "}\n"
               "return f();\n"),
            .exit_code = 64,
        },
        {
            "flat expr: array subscript in bounds", __LINE__,
            SVI("int f(void){\n"
               "    int a[4];\n"
               "    for(int i = 0; i < 4; i++) a[i] = i * i;\n"
               "    int s = 0;\n"
               "    for(int i = 0; i < 4; i++) s += a[i];\n"
               "    a[3] += 10;\n"
               "    return s + a[3];\n"       // 0+1+4+9=14, a[3]=9+10=19 -> 33
               "}\n"
               "return f();\n"),
            .exit_code = 33,
        },
        {
            "flat expr: one-past-end address is legal", __LINE__,
            SVI("int f(void){\n"
               "    int a[3];\n"
               "    int* p = a;\n"
               "    int* end = &a[3];\n"       // forming one-past pointer is allowed
               "    int n = 0;\n"
               "    while(p != end){ *p = 7; p++; n++; }\n"
               "    return n * 10 + a[2];\n"   // 3 iterations, a[2]=7 -> 37
               "}\n"
               "return f();\n"),
            .exit_code = 37,
        },
        {
            "flat expr: array to pointer decay", __LINE__,
            SVI("void fill(int* a, int v){ a[0] = v; a[1] = v + 1; }\n"
               "struct S { int arr[3]; };\n"
               "int g[2];\n"
               "int f(void){\n"
               "    int local[3];\n"
               "    int* p;\n"
               "    p = local;\n"          // local array decay (assignment)
               "    p[0] = 1; p[1] = 2; p[2] = 3;\n"
               "    fill(local, 10);\n"     // array arg decays: local[0]=10, local[1]=11
               "    struct S s;\n"
               "    int* q = s.arr;\n"      // member array decay (initializer)
               "    q[0] = 7;\n"
               "    int* r = g;\n"          // global array decay
               "    r[1] = 4;\n"
               "    return local[0]+local[1]+local[2] + s.arr[0] + g[1];\n" // 10+11+3+7+4
               "}\n"
               "return f();\n"),
            .exit_code = 35,
        },
        {
            "flat expr: bitfield load/store", __LINE__,
            SVI("struct B { unsigned a: 3; int b: 4; unsigned c: 9; };\n"
               "struct B g;\n"
               "int f(void){\n"
               "    struct B x;\n"
               "    x.a = 9;\n"                       // truncates to 1
               "    x.b = -3;\n"
               "    x.c = 100;\n"
               "    int r = (x.a == 1) + (x.b == -3) + (x.c == 100);\n"
               "    struct B* p = &x;\n"
               "    p->b = 7;\n"
               "    r += p->b == 7;\n"
               "    r += (x.c = 512) == 0;\n"         // 512 truncates to 0 in 9 bits
               "    g.b = 5;\n"                       // global struct bitfield
               "    r += g.b == 5;\n"
               "    return r;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 6,
        },
        {
            "flat expr: bitfield compound assign", __LINE__,
            SVI("struct B { int s: 4; unsigned u: 3; };\n"
               "int f(void){\n"
               "    struct B x;\n"
               "    x.s = 7; x.u = 6;\n"
               "    int r = (x.s += 2) == -7;\n"      // 7+2=9 wraps to -7 in 4 signed bits
               "    r += x.s == -7;\n"
               "    x.u += 3;\n"                      // 6+3=9 wraps to 1 in 3 bits
               "    r += x.u == 1;\n"
               "    struct B* p = &x;\n"
               "    p->s &= 6;\n"                     // 1001 & 0110 == 0
               "    r += x.s == 0;\n"
               "    return r;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 4,
        },
        {
            "flat expr: bitfield inc/dec", __LINE__,
            SVI("struct B { int s: 3; unsigned u: 2; };\n"
               "int f(void){\n"
               "    struct B x;\n"
               "    x.s = 3; x.u = 3;\n"
               "    int r = x.s++ == 3;\n"            // post yields old; s wraps to -4
               "    r += x.s == -4;\n"
               "    r += --x.s == 3;\n"               // -5 wraps to 3 in 3 signed bits
               "    r += x.u++ == 3;\n"
               "    r += x.u == 0;\n"                 // wraps
               "    return r;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 5,
        },
        {
            "flat expr: struct-hack trailing array (BITMAPINFO idiom)", __LINE__,
            // a length-1 array as a struct's last member is over-allocated and
            // indexed past its declared bound; the ABC must be elided
            SVI("struct Info { int n; int colors[1]; };\n"
               "int f(void){\n"
               "    char buf[64];\n"
               "    struct Info* p = (struct Info*)buf;\n"
               "    p->n = 3;\n"
               "    p->colors[0] = 10;\n"
               "    p->colors[1] = 20;\n"
               "    p->colors[2] = 30;\n"
               "    return p->n + p->colors[1] + p->colors[2];\n" // 3+20+30
               "}\n"
               "return f();\n"),
            .exit_code = 53,
        },
        {
            "flat expr: 2d array subscript", __LINE__,
            SVI("int f(void){\n"
               "    int m[2][3];\n"
               "    for(int i = 0; i < 2; i++)\n"
               "        for(int j = 0; j < 3; j++)\n"
               "            m[i][j] = i * 10 + j;\n"
               "    return m[1][2] + m[0][1];\n" // 12 + 1 = 13
               "}\n"
               "return f();\n"),
            .exit_code = 13,
        },
        {
            "flat expr: pointer int casts", __LINE__,
            SVI("int f(void){\n"
               "    int x = 42;\n"
               "    int* p = &x;\n"
               "    __SIZE_TYPE__ n = (__SIZE_TYPE__)p;\n"  // ptr -> int
               "    int* q = (int*)n;\n"                     // int -> ptr
               "    char* c = (char*)p;\n"                   // ptr -> ptr
               "    return (*q == 42) + (c == (char*)p) * 2;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 3,
        },
        {
            "flat expr: global read write and address", __LINE__,
            SVI("int g = 5;\n"
               "int* gp;\n"
               "void bump(void){ g += 10; }\n"
               "void store(int v){ *gp = v; }\n"
               "int f(void){\n"
               "    gp = &g;\n"
               "    bump();\n"        // g = 15
               "    store(g + 1);\n"  // g = 16 via *gp
               "    return g;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 16,
        },
        {
            "flat expr: global struct member", __LINE__,
            SVI("struct P { int a, b; };\n"
               "struct P gp;\n"
               "int f(void){\n"
               "    gp.a = 3;\n"
               "    gp.b = gp.a + 4;\n"
               "    struct P* q = &gp;\n"
               "    q->a += 10;\n"
               "    return gp.a * 100 + gp.b;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 1307,
        },
        {
            "flat expr: void call with out param", __LINE__,
            SVI("void set(int* p, int v){ *p = v; }\n"
               "int f(void){\n"
               "    int x = 0;\n"
               "    set(&x, 42);\n"
               "    return x;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 42,
        },
        {
            "flat expr: struct args and return", __LINE__,
            SVI("struct P { int a, b; };\n"
               "struct P mk(int a, int b){ struct P r; r.a = a; r.b = b; return r; }\n"
               "int sum(struct P p){ return p.a + p.b; }\n"
               "int f(void){\n"
               "    struct P s;\n"
               "    s = mk(3, 4);\n"
               "    return s.a * 10 + sum(s);\n"
               "}\n"
               "return f();\n"),
            .exit_code = 37,
        },
        {
            "rvalue dot: members of call results", __LINE__,
            SVI("struct S { int x, y; };\n"
               "struct S mk(int a, int b){ struct S r; r.x = a; r.y = b; return r; }\n"
               "return mk(1, 2).x + mk(3, 4).y;\n"),
            .exit_code = 5,
        },
        {
            "rvalue dot: nested member of call result", __LINE__,
            SVI("struct S { int x, y; };\n"
               "struct T { struct S s; };\n"
               "struct T mk(void){ struct T t; t.s.x = 6; t.s.y = 7; return t; }\n"
               "return mk().s.y;\n"),
            .exit_code = 7,
        },
        {
            "rvalue dot: 8-byte members of call result", __LINE__,
            SVI("struct L { long a, b; };\n"
               "struct L mk(void){ struct L r; r.a = 30; r.b = 12; return r; }\n"
               "return (int)(mk().a + mk().b);\n"),
            .exit_code = 42,
        },
        {
            "rvalue dot: member of conditional", __LINE__,
            SVI("struct S { int x, y; };\n"
               "int f(int c){\n"
               "    struct S a = {1, 2};\n"
               "    struct S b = {30, 40};\n"
               "    return (c ? a : b).y;\n"
               "}\n"
               "return f(0) + f(1);\n"),
            .exit_code = 42,
        },
        {
            "rvalue dot: member of statement expression", __LINE__,
            SVI("struct S { int x, y; };\n"
               "struct S mk(int a, int b){ struct S r; r.x = a; r.y = b; return r; }\n"
               "return ({ mk(40, 2); }).y;\n"),
            .exit_code = 2,
        },
        {
            "rvalue dot: bitfield members of call results", __LINE__,
            SVI("struct S { int x:3, y:5; };\n"
               "struct S mk(int a, int b){ struct S r; r.x = a; r.y = b; return r; }\n"
               "return mk(1, 2).x + mk(3, 4).y;\n"),
            .exit_code = 5,
        },
        {
            "rvalue dot: bitfield sign extension", __LINE__,
            SVI("struct S { int v:4; };\n"
               "struct S mk(int v){ struct S r; r.v = v; return r; }\n"
               "return mk(-3).v == -3 ? 42 : 1;\n"),
            .exit_code = 42,
        },
        {
            "rvalue dot: unsigned bitfield masking", __LINE__,
            SVI("struct U { unsigned pad:7, v:4; };\n"
               "struct U mk(unsigned v){ struct U r; r.pad = 127; r.v = v; return r; }\n"
               "return mk(255).v;\n"),
            .exit_code = 15,
        },
        {
            "rvalue dot: bitfield of nested member of call result", __LINE__,
            SVI("struct S { int x:3, y:5; };\n"
               "struct T { int pad; struct S s; };\n"
               "struct T mk(void){ struct T t; t.pad = 9; t.s.x = 2; t.s.y = 12; return t; }\n"
               "return mk().s.y;\n"),
            .exit_code = 12,
        },
        {
            "rvalue dot: subscript of array member of call result", __LINE__,
            SVI("struct S { int arr[4]; };\n"
               "struct S mk(void){ return (struct S){{1, 2, 3, 4}}; }\n"
               "int i = 2;\n"
               "return mk().arr[i] * 10 + mk().arr[3];\n"),
            .exit_code = 34,
        },
        {
            "subscript: __int128 index", __LINE__,
            SVI("int a[4] = {5, 6, 7, 8};\n"
               "__int128 i = 2;\n"
               "return a[i];\n"),
            .exit_code = 7,
        },
        {
            "subscript: unsigned __int128 index", __LINE__,
            SVI("int a[4] = {5, 6, 7, 8};\n"
               "unsigned __int128 i = 3;\n"
               "return a[i];\n"),
            .exit_code = 8,
        },
        {
            "compound literal: subscript at toplevel", __LINE__,
            SVI("int i = 1;\n"
               "int x = (int[]){10, 20, 30}[i];\n"
               "return x + (int[]){1, 2, 3}[2];\n"),
            .exit_code = 23,
        },
        {
            "compound literal: subscript in function", __LINE__,
            SVI("int f(int i){ return (int[]){10, 20, 30}[i]; }\n"
               "return f(2);\n"),
            .exit_code = 30,
        },
        {
            "compound literal: store through subscript", __LINE__,
            SVI("return ((int[]){1, 2, 3}[0] = 5);\n"),
            .exit_code = 5,
        },
        {
            "compound literal: member access", __LINE__,
            SVI("struct S { int x, y; };\n"
               "int f(void){ return (struct S){1, 2}.y; }\n"
               "return f() + (struct S){39, 3}.x;\n"),
            .exit_code = 41,
        },
        {
            "flat expr: nested calls as arguments", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int twice(int x){ return x * 2; }\n"
               "return add(twice(3), add(twice(2), 1));\n"),
            .exit_code = 11,
        },
        {
            "flat expr: discarded call return", __LINE__,
            SVI("int calls = 0;\n"
               "int bump(void){ calls++; return 99; }\n"
               "int f(void){ bump(); bump(); return 1; }\n"
               "return f() + calls;\n"),
            .exit_code = 3,
        },
        {
            "lowering: temp slot recycling stress", __LINE__,
            SVI("int f(void){\n"
               "    int total = 0;\n"
               "    for(int i = 0; i < 3; i++){\n"
               "        int j = 0;\n"
               "        while(j < 4){\n"
               "            if(j % 2 == 0){\n"
               "                switch(j){\n"
               "                    case 0: total += 1; break;\n"
               "                    case 2: total += 10; break;\n"
               "                }\n"
               "            }\n"
               "            if(j == 3) total += 100;\n"
               "            j++;\n"
               "        }\n"
               "        do { total += 1000; } while(0);\n"
               "    }\n"
               "    return total == 3333 ? 42 : 1;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 42,
        },
        // Statement expressions
        {
            "stmt expr: basic value", __LINE__,
            SVI("int x = ({ int a = 2; a * 3; });\n"
               "return x;\n"),
            .exit_code = 6,
        },
        {
            "stmt expr: goto out through call args", __LINE__,
            SVI("int calls = 0;\n"
               "int foo(int x){ calls++; return x; }\n"
               "int goto_out(void){\n"
               "    foo(({ goto hello; 1; }));\n"
               "    return 100;\n"
               "hello:\n"
               "    return 42;\n"
               "}\n"
               "return goto_out() + calls;\n"),
            .exit_code = 42,
        },
        {
            "stmt expr: return inside, not taken", __LINE__,
            SVI("int f(int n){\n"
               "    int r = 10 + ({ if(n > 3) return -n; n * 2; });\n"
               "    return r;\n"
               "}\n"
               "return f(2);\n"),
            .exit_code = 14,
        },
        {
            "stmt expr: return inside, taken", __LINE__,
            SVI("int f(int n){\n"
               "    int r = 10 + ({ if(n > 3) return -n; n * 2; });\n"
               "    return r;\n"
               "}\n"
               "return -f(5);\n"),
            .exit_code = 5,
        },
        {
            "stmt expr: loop condition re-evaluated", __LINE__,
            SVI("int i = 0, total = 0;\n"
               "while(({ int t = i < 5; i++; t; })){\n"
               "    total += i;\n"
               "}\n"
               "return total;\n"),
            .exit_code = 15,
        },
        {
            "stmt expr: break from body binds enclosing loop", __LINE__,
            SVI("int total = 0;\n"
               "for(int i = 0;; i++){\n"
               "    total += ({ if(i == 4) break; i * 10; });\n"
               "}\n"
               "return total;\n"),
            .exit_code = 60,
        },
        {
            "stmt expr: if condition with goto out", __LINE__,
            SVI("if(({ goto hello; 1; })) return 1;\n"
               "return 2;\n"
               "hello:\n"
               "return 7;\n"),
            .exit_code = 7,
        },
        {
            "stmt expr: && short circuits body", __LINE__,
            SVI("int n = 0;\n"
               "int r = 0 && ({ n = 1; 1; });\n"
               "return n * 10 + r;\n"),
            .exit_code = 0,
        },
        {
            "stmt expr: && normalizes to 0/1", __LINE__,
            SVI("return 1 && ({ 5; });\n"),
            .exit_code = 1,
        },
        {
            "stmt expr: || short circuits body", __LINE__,
            SVI("int n = 0;\n"
               "int r = 1 || ({ n = 1; 0; });\n"
               "return n * 10 + r;\n"),
            .exit_code = 1,
        },
        {
            "stmt expr: ternary arms", __LINE__,
            SVI("int f(int a){\n"
               "    int x = a && ({ int y = ({ a + 1; }); y * 2; });\n"
               "    int z = a ? ({ x + 3; }) : ({ x + 4; });\n"
               "    return x + z;\n"
               "}\n"
               "return f(0) * 10 + f(2);\n"),
            .exit_code = 45,
        },
        {
            "stmt expr: labels are function scoped", __LINE__,
            SVI("int f(void){\n"
               "    int acc = 0;\n"
               "    acc += ({ goto fwd; 999; });\n"
               "fwd:\n"
               "    acc += 7;\n"
               "    if(acc < 20) goto back;\n"
               "    return acc;\n"
               "back:\n"
               "    acc += ({ int q = 6; back2: q += 1; if(q < 10) goto back2; q; });\n"
               "    goto fwd;\n"
               "}\n"
               "return f();\n"),
            .exit_code = 24,
        },
        {
            "stmt expr: struct value", __LINE__,
            SVI("struct Pair { int a, b; };\n"
               "struct Pair p = ({ struct Pair t = {3, 4}; t; });\n"
               "return p.a * 10 + p.b;\n"),
            .exit_code = 34,
        },
        {
            "stmt expr: void as expression statement", __LINE__,
            SVI("int n = 0;\n"
               "({ n += 5; (void)0; });\n"
               "({ n += 6; });\n"
               "return n;\n"),
            .exit_code = 11,
        },
        {
            "stmt expr: toplevel ternary arm", __LINE__,
            SVI("int total = 0;\n"
               "for(int i = 0;; i++){\n"
               "    total += ({ if(i == 4) break; i * 10; });\n"
               "}\n"
               "int z = total > 50 ? ({ total + 1; }) : 0;\n"
               "return z;\n"),
            .exit_code = 61,
        },
        // Blocks / scoping
        {
            "block: variable shadowing", __LINE__,
            SVI("int x = 1;\n"
               "{\n"
               "  int x = 2;\n"
               "  x = x + 10;\n"
               "}\n"
               "return x;\n"),
            .exit_code = 1,
        },
        // Pointers
        {
            "pointer: address and deref", __LINE__,
            SVI("int x = 42;\n"
               "int *p = &x;\n"
               "return *p;\n"),
            .exit_code = 42,
        },
        {
            "pointer: write through", __LINE__,
            SVI("int x = 0;\n"
               "int *p = &x;\n"
               "*p = 42;\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "pointer: arithmetic", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = arr;\n"
               "return *(p + 2);\n"),
            .exit_code = 30,
        },
        {
            "pointer: subscript", __LINE__,
            SVI("int arr[3] = {5, 10, 15};\n"
               "int *p = arr;\n"
               "return p[1];\n"),
            .exit_code = 10,
        },
        {
            "pointer: += compound assign", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = arr;\n"
               "p += 2;\n"
               "return *p;\n"),
            .exit_code = 30,
        },
        {
            "pointer: -= compound assign", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = arr + 3;\n"
               "p -= 2;\n"
               "return *p;\n"),
            .exit_code = 20,
        },
        {
            "pointer: difference", __LINE__,
            SVI("int arr[5] = {0};\n"
               "int *a = &arr[1];\n"
               "int *b = &arr[4];\n"
               "return (int)(b - a);\n"),
            .exit_code = 3,
        },
        {
            "pointer: add negative signed char index", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = &arr[3];\n"
               "signed char i = -2;\n"
               "return *(p + i);\n"),
            .exit_code = 20,
        },
        {
            "pointer: int + pointer", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = arr;\n"
               "return *(3 + p);\n"),
            .exit_code = 40,
        },
        {
            "pointer: sub unsigned index", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = &arr[3];\n"
               "unsigned i = 2;\n"
               "return *(p - i);\n"),
            .exit_code = 20,
        },
        {
            "pointer: long long and unsigned long long index", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = arr;\n"
               "long long i = 3;\n"
               "unsigned long long u = 1;\n"
               "return *(p + i) + *(p + u);\n"),
            .exit_code = 60,
        },
        {
            "pointer: enum index", __LINE__,
            SVI("enum E { TWO = 2 };\n"
               "int arr[4] = {10, 20, 30, 40};\n"
               "int *p = arr;\n"
               "enum E e = TWO;\n"
               "return *(p + e);\n"),
            .exit_code = 30,
        },
        {
            "pointer: difference negative", __LINE__,
            SVI("int arr[5] = {0};\n"
               "int *a = &arr[4];\n"
               "int *b = &arr[1];\n"
               "return (int)(b - a) + 4;\n"),
            .exit_code = 1,
        },
        {
            "pointer: difference odd-sized element", __LINE__,
            SVI("struct S { char c[12]; };\n"
               "struct S arr[5];\n"
               "struct S *a = &arr[1];\n"
               "struct S *b = &arr[4];\n"
               "return (int)(b - a);\n"),
            .exit_code = 3,
        },
        {
            "pointer: char difference", __LINE__,
            SVI("char buf[10];\n"
               "char *a = buf + 2;\n"
               "char *b = buf + 9;\n"
               "return (int)(b - a);\n"),
            .exit_code = 7,
        },
        {
            "pointer: comparisons", __LINE__,
            SVI("int arr[4];\n"
               "int *a = arr + 1;\n"
               "int *b = arr + 3;\n"
               "return (a < b) + (b > a)*2 + (a <= a)*4 + (b >= b)*8 + (a == a)*16 + (a != b)*32;\n"),
            .exit_code = 63,
        },
        {
            "pointer: compare with null", __LINE__,
            SVI("int x = 0;\n"
               "int *p = &x;\n"
               "int *q = 0;\n"
               "return (p != 0) + (q == 0)*2 + (0 == q)*4;\n"),
            .exit_code = 7,
        },
        {
            "pointer: += negative signed char", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = &arr[3];\n"
               "signed char i = -3;\n"
               "p += i;\n"
               "return *p;\n"),
            .exit_code = 10,
        },
        {
            "pointer: += through pointer to pointer", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = arr;\n"
               "int **pp = &p;\n"
               "*pp += 2;\n"
               "return *p;\n"),
            .exit_code = 30,
        },
        {
            "pointer: compound assign value used", __LINE__,
            SVI("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = arr;\n"
               "int *q = (p += 2);\n"
               "return (*q == 30) + (p == q)*2;\n"),
            .exit_code = 3,
        },
        {
            "pointer: sub array decay", __LINE__,
            SVI("typedef struct Foo { char data[10000]; } Foo;\n"
               "Foo f;\n"
               "char* p = f.data;\n"
               "char* end = f.data + sizeof f.data;\n"
               "long diff = end - p;\n"
               "long diff2 = end - f.data;\n"
               "return diff != diff2;\n"),
            .exit_code = 0,
        },
        {
            "pointer: ternary array decay cond", __LINE__,
            SVI("typedef struct Foo { char data[10000]; } Foo;\n"
               "Foo f;\n"
               "char* c = f.data ? f.data : (void*)0;\n"
               "return c != f.data;\n"),
            .exit_code = 0,
        },
        // Arrays
        {
            "array: basic", __LINE__,
            SVI("int arr[3] = {10, 20, 30};\n"
               "return arr[0] + arr[1] + arr[2];\n"),
            .exit_code = 60,
        },
        {
            "array: zero init", __LINE__,
            SVI("int arr[5] = {0};\n"
               "return arr[0] + arr[1] + arr[4];\n"),
            .exit_code = 0,
        },
        {
            "array: partial init", __LINE__,
            SVI("int arr[5] = {1, 2};\n"
               "return arr[0] + arr[1] + arr[2];\n"),
            .exit_code = 3,
        },
        {
            "array: assign local to local", __LINE__,
            SVI("int a[3] = {1, 2, 3};\n"
               "int b[3] = {0};\n"
               "b = a;\n"
               "a[0] = 9;\n"
               "return b[0] * 100 + b[1] * 10 + b[2];\n"),
            .exit_code = 123,
        },
        {
            "array: assign global to global", __LINE__,
            SVI("int p[100];\n"
               "int q[100];\n"
               "void doit(void){ p = q; }\n"
               "q[0] = 7;\n"
               "q[99] = 8;\n"
               "doit();\n"
               "return p[0] * 10 + p[99];\n"),
            .exit_code = 78,
        },
        {
            "array: assign global to local", __LINE__,
            SVI("int g[4] = {4, 3, 2, 1};\n"
               "int f(void){\n"
               "    int l[4] = {0};\n"
               "    l = g;\n"
               "    return l[0] * 1000 + l[1] * 100 + l[2] * 10 + l[3];\n"
               "}\n"
               "return f();\n"),
            .exit_code = 4321,
        },
        {
            "array: assign local to global", __LINE__,
            SVI("int g[4];\n"
               "void f(void){\n"
               "    int l[4] = {5, 6, 7, 8};\n"
               "    g = l;\n"
               "}\n"
               "f();\n"
               "return g[0] * 1000 + g[1] * 100 + g[2] * 10 + g[3];\n"),
            .exit_code = 5678,
        },
        {
            "array: assign struct member arrays", __LINE__,
            SVI("struct S { int a[3]; int b[3]; };\n"
               "struct S s = {{1, 2, 3}, {0}};\n"
               "s.b = s.a;\n"
               "return s.b[0] * 100 + s.b[1] * 10 + s.b[2];\n"),
            .exit_code = 123,
        },
        {
            "array: assign member array through pointer", __LINE__,
            SVI("struct S { int a[3]; };\n"
               "int f(struct S* dst, struct S* src){\n"
               "    dst->a = src->a;\n"
               "    return dst->a[0] * 100 + dst->a[1] * 10 + dst->a[2];\n"
               "}\n"
               "struct S x = {{0}};\n"
               "struct S y = {{3, 2, 1}};\n"
               "return f(&x, &y);\n"),
            .exit_code = 321,
        },
        {
            "array: assign row of 2d array", __LINE__,
            SVI("int m[2][3] = {{1, 2, 3}, {0}};\n"
               "m[1] = m[0];\n"
               "return m[1][0] * 100 + m[1][1] * 10 + m[1][2];\n"),
            .exit_code = 123,
        },
        {
            "array: assign through pointer to array", __LINE__,
            SVI("int f(int (*d)[3], int (*s)[3]){\n"
               "    *d = *s;\n"
               "    return (*d)[0] * 100 + (*d)[1] * 10 + (*d)[2];\n"
               "}\n"
               "int a[3] = {0};\n"
               "int b[3] = {4, 5, 6};\n"
               "return f(&a, &b);\n"),
            .exit_code = 456,
        },
        {
            "array: init local from array", __LINE__,
            SVI("int a[3] = {1, 2, 3};\n"
               "int b[3] = a;\n"
               "a[1] = 9;\n"
               "return b[0] * 100 + b[1] * 10 + b[2];\n"),
            .exit_code = 123,
        },
        {
            "array: self assignment", __LINE__,
            SVI("int a[3] = {1, 2, 3};\n"
               "a = a;\n"
               "return a[0] * 100 + a[1] * 10 + a[2];\n"),
            .exit_code = 123,
        },
        // Struct
        {
            "struct: basic", __LINE__,
            SVI("struct point { int x; int y; };\n"
               "struct point p = {3, 4};\n"
               "return p.x * 10 + p.y;\n"),
            .exit_code = 34,
        },
        {
            "struct: designated init", __LINE__,
            SVI("struct point { int x; int y; };\n"
               "struct point p = {.y = 7, .x = 3};\n"
               "return p.x * 10 + p.y;\n"),
            .exit_code = 37,
        },
        {
            "struct: pointer arrow", __LINE__,
            SVI("struct point { int x; int y; };\n"
               "struct point p = {5, 6};\n"
               "struct point *pp = &p;\n"
               "return pp->x * 10 + pp->y;\n"),
            .exit_code = 56,
        },
        {
            "struct: nested", __LINE__,
            SVI("struct inner { int val; };\n"
               "struct outer { struct inner a; int b; };\n"
               "struct outer o = {{42}, 7};\n"
               "return o.a.val + o.b;\n"),
            .exit_code = 49,
        },
        // Union
        {
            "union: basic", __LINE__,
            SVI("union u { int i; char c; };\n"
               "union u v;\n"
               "v.i = 0;\n"
               "v.c = 5;\n"
               "return v.c;\n"),
            .exit_code = 5,
        },
        // Enum
        {
            "enum", __LINE__,
            SVI("enum color { RED, GREEN, BLUE };\n"
               "enum color c = BLUE;\n"
               "return c;\n"),
            .exit_code = 2,
        },
        {
            "enum: explicit values", __LINE__,
            SVI("enum { A = 10, B, C = 20 };\n"
               "return A + B + C;\n"),
            .exit_code = 41,
        },
        // Typedef
        {
            "typedef", __LINE__,
            SVI("typedef int myint;\n"
               "myint x = 42;\n"
               "return x;\n"),
            .exit_code = 42,
        },
        // Functions
        {
            "function: basic call", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "return add(3, 4);\n"),
            .exit_code = 7,
        },
        {
            "function: recursion (factorial)", __LINE__,
            SVI("int fact(int n){\n"
               "  if(n <= 1) return 1;\n"
               "  return n * fact(n - 1);\n"
               "}\n"
               "return fact(5);\n"),
            .exit_code = 120,
        },
        {
            "function: mutual recursion", __LINE__,
            SVI("int is_even(int n);\n"
               "int is_odd(int n);\n"
               "int is_even(int n){ if(n == 0) return 1; return is_odd(n - 1); }\n"
               "int is_odd(int n){ if(n == 0) return 0; return is_even(n - 1); }\n"
               "return is_even(10);\n"),
            .exit_code = 1,
        },
        // Function pointers
        {
            "function pointer", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int (*fp)(int, int) = add;\n"
               "return fp(3, 4);\n"),
            .exit_code = 7,
        },
        {
            "function pointer: call inside function body", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int mul(int a, int b){ return a * b; }\n"
               "int apply(int (*f)(int, int), int a, int b){ return f(a, b); }\n"
               "return apply(add, 2, 3) + apply(mul, 2, 3);\n"),
            .exit_code = 11,
        },
        {
            "function pointer: struct arg and return", __LINE__,
            SVI("typedef struct P { int x, y; } P;\n"
               "P mkp(int x, int y){ return (P){x, y}; }\n"
               "int psum(P p){ return p.x + p.y; }\n"
               "P (*mk)(int, int) = mkp;\n"
               "int (*sum)(P) = psum;\n"
               "return sum(mk(3, 4));\n"),
            .exit_code = 7,
        },
        {
            "function pointer: discarded result still runs", __LINE__,
            SVI("int store(int* p, int v){ *p = v; return v; }\n"
               "int (*fp)(int*, int) = store;\n"
               "int x = 0;\n"
               "fp(&x, 42);\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "function pointer: call through explicit deref", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int (*fp)(int, int) = add;\n"
               "return (*fp)(3, 4);\n"),
            .exit_code = 7,
        },
        {
            "function pointer: deref decays back to pointer", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int (*fp)(int, int) = add;\n"
               "int (*fp2)(int, int) = *fp;\n"
               "return fp2(3, 4);\n"),
            .exit_code = 7,
        },
        {
            "function pointer: repeated deref", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int (*fp)(int, int) = add;\n"
               "return (***fp)(3, 4);\n"),
            .exit_code = 7,
        },
        {
            "function pointer: address of deref", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int (*fp)(int, int) = add;\n"
               "int (*fp2)(int, int) = &*fp;\n"
               "return fp2(3, 4);\n"),
            .exit_code = 7,
        },
        {
            "function pointer: comma rhs decays", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int x = 0;\n"
               "int (*fp)(int, int) = (x = 1, add);\n"
               "return fp(3, 4) + x;\n"),
            .exit_code = 8,
        },
        {
            "call: argument to unnamed parameter is evaluated", __LINE__,
            SVI("int second(int, int b){ return b; }\n"
               "int counter = 0;\n"
               "return second(counter = 40, 2) + counter;\n"),
            .exit_code = 42,
        },
        {
            "_Module.symbol Any bounded slice", __LINE__,
            SVI("int target = 42;\n"
                "char name[6] = {'t', 'a', 'r', 'g', 'e', 't'};\n"
                "const char slice[:] = name;\n"
                "_Any a = __root_module().symbol(slice);\n"
                "_Any b = __root_module().symbol(\"!target?\"[1:7]);\n"
                "if(a.type != int* || b.type != int*) return 99;\n"
                "return *a.as(int*) == 42 && a.as(int*) == b.as(int*);\n"),
            .exit_code = 1,
        },
        {
            "_Module.symbol Any empty and embedded null slices", __LINE__,
            SVI("int target = 42;\n"
                "const char empty[:] = {};\n"
                "_Any a = __root_module().symbol(empty);\n"
                "_Any b = __root_module().symbol(\"target\\0suffix\");\n"
                "return a.type.is_invalid && b.type.is_invalid;\n"),
            .exit_code = 1,
        },
        {
            "_Module.symbol Any function", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
                "_Any a = __root_module().symbol(\"add\");\n"
                "if(a.type != typeof(&add)) return 99;\n"
                "return a.as(typeof(&add))(3, 4);\n"),
            .exit_code = 7,
        },
        {
            "_Module.symbol Any compiled module", __LINE__,
            SVI("_Module m = __compile(\"int f(void){ return 42; } int x = 7;\", \"\");\n"
                "_Any f = m.symbol(\"f\"), x = m.symbol(\"x\");\n"
                "if(f.type != typeof(int(*)(void)) || x.type != int*) return 99;\n"
                "*x.as(int*) = 8;\n"
                "return f.as(int(*)(void))() + *m.symbol(\"x\").as(int*);\n"),
            .exit_code = 50,
        },
        {
            "_Module.symbol Any variable types", __LINE__,
            SVI("const int c = 3; int values[2] = {4, 5}; int g = 10;\n"
                "_Any a = __root_module().symbol(\"g\");\n"
                "_Any cptr = __root_module().symbol(\"c\");\n"
                "_Any arr = __root_module().symbol(\"values\");\n"
                "if(a.type != int* || cptr.type != const int* || arr.type != typeof(&values)) return 99;\n"
                "*a.as(int*) = 42;\n"
                "return g + *cptr.as(const int*) + (*arr.as(typeof(&values)))[1];\n"),
            .exit_code = 50,
        },
        {
            "_Module.symbol Any missing and non-addressable symbols", __LINE__,
            SVI("typedef int T; enum { E = 1 }; extern int missing_extern_symbol;\n"
                "int missing_function(void);\n"
                "_Module m = __root_module();\n"
                "_Any a = m.symbol(\"absent\"), b = m.symbol(\"T\"), c = m.symbol(\"E\");\n"
                "_Any d = m.symbol(\"missing_extern_symbol\"), e = m.symbol(\"missing_function\");\n"
                "return a.type.is_invalid && b.type.is_invalid && c.type.is_invalid\n"
                "    && d.type.is_invalid && e.type.is_invalid\n"
                "    && *(void**)a.payload == nullptr && *(void**)d.payload == nullptr;\n"),
            .exit_code = 1,
        },
        {
            "_Module.symbol typed bounded slices", __LINE__,
            SVI("_Module m = __compile(\"int target = 42; int add(int a, int b){return a + b;}\", \"\");\n"
                "char name[6] = {'t', 'a', 'r', 'g', 'e', 't'};\n"
                "const char slice[:] = name;\n"
                "int* p = m.symbol(slice, int);\n"
                "const char* nameptr = name;\n"
                "int* q = m.symbol(nameptr[:6], int);\n"
                "int (*f)(int, int) = m.symbol(\"!add?\"[1:4], typeof(*f));\n"
                "if(!p || q != p || !f || m.symbol(slice, long) != nullptr) return 99;\n"
                "*p = 35; return f(*p, 7);\n"),
            .exit_code = 42,
        },
        {
            "_Module.symbol typed empty and embedded null slices", __LINE__,
            SVI("int target = 42; const char empty[:] = {};\n"
                "_Module m = __root_module();\n"
                "return m.symbol(empty, int) == nullptr\n"
                "    && m.symbol(\"target\\0suffix\", int) == nullptr\n"
                "    && m.symbol(\"target\"[0:0], int) == nullptr;\n"),
            .exit_code = 1,
        },
        {
            "_Module.symbol function", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int (*fp)(int, int) = __root_module().symbol(\"add\", typeof(*fp));\n"
               "return fp ? fp(3, 4) : 99;\n"),
            .exit_code = 7,
        },
        {
            "_Module.symbol function type mismatch", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int (*fp)(void) = __root_module().symbol(\"add\", typeof(*fp));\n"
               "return fp == 0;\n"),
            .exit_code = 1,
        },
        {
            "_Module.symbol missing function", __LINE__,
            SVI("int (*fp)(void) = __root_module().symbol(\"missing_function\", typeof(*fp));\n"
               "return fp == 0;\n"),
            .exit_code = 1,
        },
        {
            "__compile module symbol", __LINE__,
            SVI("_Module m = __compile(\"int f(void){ return 42; }\", \"\");\n"
               "if(!m) return 99;\n"
               "int (*fp)(void) = m.symbol(\"f\", typeof(*fp));\n"
               "return fp ? fp() : 98;\n"),
            .exit_code = 42,
        },
        {
            "__compile module method symbol", __LINE__,
            SVI("_Module m = __compile(\"int f(void){ return 7; }\", \"\");\n"
               "if(!m) return 99;\n"
               "int (*fp)(void) = m.symbol(\"f\", typeof(*fp));\n"
               "return fp ? fp() : 98;\n"),
            .exit_code = 7,
        },
        {
            "__compile module sees global", __LINE__,
            SVI("int g(void){ return 5; }\n"
               "_Module m = __compile(\"int f(void){ return g() + 2; }\", \"\");\n"
               "if(!m) return 99;\n"
               "int (*fp)(void) = m.symbol(\"f\", typeof(*fp));\n"
               "return fp ? fp() : 98;\n"),
            .exit_code = 7,
        },
        {
            "__compile module shadows global", __LINE__,
            SVI("int f(void){ return 1; }\n"
               "_Module m = __compile(\"int f(void){ return 7; }\", \"\");\n"
               "if(!m) return 99;\n"
               "int (*mf)(void) = m.symbol(\"f\", typeof(*mf));\n"
               "int (*rf)(void) = __root_module().symbol(\"f\", typeof(*rf));\n"
               "return mf() * 10 + rf();\n"),
            .exit_code = 71,
        },
        {
            "__compile module run", __LINE__,
            SVI("_Module m = __compile(\"int x = 1; x = x + 41;\", \"\");\n"
               "if(!m) return 99;\n"
               "if(m.run()) return 98;\n"
               "int* x = m.symbol(\"x\", typeof(*x));\n"
               "return x ? *x : 97;\n"),
            .exit_code = 42,
        },
        {
            "__compile module run returns top-level value", __LINE__,
            SVI("_Module m = __compile(\"int x; x++; return -17; x = 99;\", \"\");\n"
               "if(!m) return 99;\n"
               "int* x = m.symbol(\"x\", int);\n"
               "int first = m.run();\n"
               "m.run();\n"
               "return first == -17 && *x == 2;\n"),
            .exit_code = 1,
        },
        {
            "__compile module run recursion", __LINE__,
            SVI("_Module m = __compile(\"_Module self; int n; if(n){ n--; self.run(); }\", \"\");\n"
                "_Module* self = m.symbol(\"self\", _Module);\n"
                "int* n = m.symbol(\"n\", int);\n"
                "*self = m; *n = 100;\n"
                "int r = m.run();\n"
                "return r + *n;\n"),
            .exit_code = 0,
        },
        {
            "__compile owns source", __LINE__,
            SVI("char src[] = \"const char* f(void){ return \\\"ok\\\"; }\";\n"
               "_Module m = __compile(src[:sizeof(src)-1], \"\");\n"
               "if(!m) return 99;\n"
               "for(int i = 0; src[i]; i++) src[i] = '?';\n"
               "const char* (*fp)(void) = m.symbol(\"f\", typeof(*fp));\n"
               "if(!fp) return 98;\n"
               "const char* s = fp();\n"
               "return s[0] == 'o' && s[1] == 'k' && s[2] == 0 ? 7 : 97;\n"),
            .exit_code = 7,
        },
        {
            "__compile synthetic file", __LINE__,
            SVI("_Module m = __compile(\"const char* file(void){ return __FILE__; }\", \"\");\n"
               "if(!m) return 99;\n"
               "const char* (*file)(void) = m.symbol(\"file\", typeof(*file));\n"
               "if(!file) return 98;\n"
               "const char* s = file();\n"
               "return s[0] == '<' && s[1] == '_' && s[2] == '_' && s[3] == 'c' ? 7 : 97;\n"),
            .exit_code = 7,
        },
        {
            "__compile bounded source and path slices", __LINE__,
            SVI("char source[] = {'i','n','t',' ','x','=','4','2',';'};\n"
                "_Module m = __compile(source[:9], \"!bounded.c?\"[1:10]);\n"
                "if(!m) return 99;\n"
                "if(m.run()) return 98;\n"
                "_ModuleMember x = m.var(0); const char path[:] = x.srcloc.file;\n"
                "if(*m.symbol(\"x\", int) != 42) return 91;\n"
                "if(path.count < 9) return 92;\n"
                "if(path[path.count-9] != 'b' || path[path.count-1] != 'c') return 93;\n"
                "return 1;\n"),
            .exit_code = 1,
        },
        {
            "__compile empty slices", __LINE__,
            SVI("const char empty[:] = {};\n"
                "_Module m = __compile(empty, empty);\n"
                "return m != nullptr && m.var_count == 0 && m.func_count == 0 && m.run() == 0;\n"),
            .exit_code = 1,
        },
        {
            "_Module.parse_type bounded and empty slices", __LINE__,
            SVI("char name[3] = {'i','n','t'}; const char empty[:] = {};\n"
                "_Module m = __root_module();\n"
                "return m.parse_type(name[:3]) == int\n"
                "    && m.parse_type(\"!int*?\"[1:5]) == int*\n"
                "    && m.parse_type(empty).is_invalid;\n"),
            .exit_code = 1,
        },
        {
            "_Module.parse_type invalid input and recovery", __LINE__,
            SVI("_Module m = __compile(\"typedef int MyInt;\", \"\");\n"
               "if(!m) return 99;\n"
               "if(!m.parse_type(\"int name\").is_invalid) return 98;\n"
               "if(!m.parse_type(\"int;\").is_invalid) return 97;\n"
               "if(!m.parse_type(\"\").is_invalid) return 96;\n"
               "if(!m.parse_type(\"missing_type\").is_invalid) return 95;\n"
               "return m.parse_type(\"MyInt\") == int && __root_module().parse_type(\"int\") == int;\n"),
            .exit_code = 1,
        },
        {
            "_Module.parse_type root", __LINE__,
            SVI("_Type T = __root_module().parse_type(\"int*\");\n"
               "return T.is_pointer && T.pointee == int ? 7 : 99;\n"),
            .exit_code = 7,
        },
        {
            "_Module.parse_type module typedef", __LINE__,
            SVI("_Module m = __compile(\"typedef int MyInt;\", \"\");\n"
               "if(!m) return 99;\n"
               "_Type T = m.parse_type(\"MyInt\");\n"
               "return T == int ? 7 : 98;\n"),
            .exit_code = 7,
        },
        {
            "_Module.parse_type module struct", __LINE__,
            SVI("_Module m = __compile(\"typedef int MyInt; struct S { MyInt x; };\", \"\");\n"
               "if(!m) return 99;\n"
               "_Type T = m.parse_type(\"struct S\");\n"
               "return T.is_struct && T.fields == 1 ? 7 : 98;\n"),
            .exit_code = 7,
        },
        {
            "_Module reflection counts", __LINE__,
            SVI("_Module m = __compile(\"typedef int T; struct S{int x;}; int f(void){return 1;} int x;\", \"\");\n"
               "if(!m) return 99;\n"
               "return m.func_count == 1 && m.var_count == 1 && m.type_count == 2 ? 7 : 98;\n"),
            .exit_code = 7,
        },
        {
            "_Module reflection excludes compound literal backing variables", __LINE__,
            SVI("_Module m = __compile(\"(int){3} = 4; int *p = &(int){3};\", \"\");\n"
                "if(!m) return 99;\n"
                "if(m.var_count != 1) return 98;\n"
                "_ModuleMember v = m.var(0);\n"
                "return v.name.count == 1 && v.name[0] == 'p' ? 7 : 97;\n"),
            .exit_code = 7,
        },
        {
            "_Module reflection entries", __LINE__,
            SVI("_Module m = __compile(\"typedef int T; int f(void){return 3;} int x;\", \"\");\n"
               "if(!m) return 99;\n"
               "_ModuleMember f = m.func(0);\n"
               "_ModuleMember fd = m.func_decl(0);\n"
               "_ModuleMember v = m.var(0);\n"
               "_ModuleMember t = m.type(0);\n"
               "if(f.name.count != 1 || fd.name.count != 1|| v.name.count != 1 || t.name.count != 1) return 95;\n"
               "if(f.name[0] != 'f' || fd.name[0] != 'f' || v.name[0] != 'x' || t.name[0] != 'T') return 94;\n"
               "const char name[:] = f.name;\n"
               "if(name.count != 1 || name.data[0] != 'f') return 93;\n"
               "if(!f.address || !v.address || t.address) return 98;\n"
               "return f.type.is_function && v.type == int && t.type == int ? 7 : 96;\n"),
            .exit_code = 7,
        },
        {
            "_SrcLoc module properties", __LINE__,
            SVI("_Module m = __compile(\"int f(void){return 1;}\\n  int x;\\nstruct S {int x;};\", \"\");\n"
               "_SrcLoc f = m.func(0).srcloc;\n"
               "_SrcLoc v = m.var(0).srcloc;\n"
               "_SrcLoc t = m.type(0).srcloc;\n"
               "const char file[:] = f.file;\n"
               "if(file.count < 4 || file[0] != '<' || file[1] != '_') return 91;\n"
               "if(f.line != 1 || f.col != 5) return 92;\n"
               "if(v.line != 2 || v.col != 7) return 93;\n"
               "if(t.line != 3 || t.col != 1) return 94;\n"
               "return 7;\n"),
            .exit_code = 7,
        },
        {
            "_SrcLoc definition locations survive redeclarations", __LINE__,
            SVI("_Module m = __compile(\"int f(void); extern int x; struct S; union U; enum E;\\n"
                "  int f(void){return 1;}\\n"
                "  int x;\\n"
                "  struct S {int a;};\\n"
                "  union U {int a;};\\n"
                "  enum E {A};\\n"
                "int f(void); extern int x; struct S; union U; enum E;\", \"\");\n"
                "if(!m) return 90;\n"
                "if(m.func(0).srcloc.line != 2 || m.func(0).srcloc.col != 7) return 91;\n"
                "if(m.var(0).srcloc.line != 3 || m.var(0).srcloc.col != 7) return 92;\n"
                "for(size_t i = 0; i < m.type_count; i++){\n"
                "_ModuleMember t = m.type(i);\n"
                "size_t line = t.name[0] == 'S' ? 4 : t.name[0] == 'U' ? 5 : 6;\n"
                "if(t.srcloc.line != line || t.srcloc.col != 3) return 93;\n"
                "}\n"
                "int* x = m.symbol(\"x\", int);\n"
                "return x && *x == 0 ? 7 : 94;\n"),
            .exit_code = 7,
        },
        {
            "_SrcLoc root and macro properties", __LINE__,
            SVI("#define DECL int x;\n"
               "DECL int expected_line = __LINE__;\n"
               "_Module m = __root_module();\n"
               "_SrcLoc loc = nullptr;\n"
               "for(size_t i = 0; i < m.var_count; i++){\n"
               "_ModuleMember v = m.var(i);\n"
               "if(v.name.count == 1 && v.name[0] == 'x') loc = v.srcloc;\n"
               "}\n"
               "const char file[:] = loc.file;\n"
               "const char* expected_file = __FILE__;\n"
               "for(size_t i = 0; i < file.count; i++) if(file[i] != expected_file[i]) return 91;\n"
               "if(expected_file[file.count]) return 93;\n"
               "return loc.line == expected_line && loc.col == 1 ? 7 : 92;\n"),
            .exit_code = 7,
        },
        {
            "_SrcLoc typedef properties", __LINE__,
            SVI("_Module m = __compile(\"typedef int T;\", \"\");\n"
               "_SrcLoc loc = m.type(0).srcloc;\n"
               "return loc.line == 1 && loc.col == 13 && loc.file.count > 0 ? 7 : 91;\n"),
            .exit_code = 7,
        },
        {
            "_SrcLoc builtin typedef has no location", __LINE__,
            SVI("_Module m = __root_module();\n"
                "for(size_t i = 0; i < m.type_count; i++){\n"
                "_ModuleMember t = m.type(i);\n"
                "if(t.name.count != 6 || t.name[0] != 's' || t.name[1] != 'i') continue;\n"
                "return t.srcloc.line == 0 && t.srcloc.col == 0 && t.srcloc.file.count == 0 ? 7 : 91;\n"
                "}\nreturn 92;\n"),
            .exit_code = 7,
        },
        {
            "_SrcLoc receiver evaluated once", __LINE__,
            SVI("int x; int expected_line = __LINE__;\n"
               "int calls;\n"
               "_SrcLoc get(void){ calls++; _Module m = __root_module(); for(size_t i = 0; i < m.var_count; i++){ _ModuleMember v = m.var(i); if(v.name.count == 1 && v.name[0] == 'x') return v.srcloc; } return nullptr; }\n"
               "int line = get().line;\n"
               "get().file;\n"
               "return calls == 2 && line == expected_line ? 7 : 91;\n"),
            .exit_code = 7,
        },
        {
            "__hotswap direct call", __LINE__,
            SVI("int f(void){ return 1; }\n"
               "int g(void){ return 2; }\n"
               "int err = __hotswap(f, g);\n"
               "return err ? 99 : f();\n"),
            .exit_code = 2,
        },
        {
            "__hotswap indirect call", __LINE__,
            SVI("int f(void){ return 1; }\n"
               "int g(void){ return 2; }\n"
               "int (*p)(void) = f;\n"
               "int err = __hotswap(f, g);\n"
               "return err ? 99 : p();\n"),
            .exit_code = 2,
        },
        {
            "__hotswap chain", __LINE__,
            SVI("int f(void){ return 1; }\n"
               "int g(void){ return 2; }\n"
               "int h(void){ return 3; }\n"
               "__hotswap(f, g);\n"
               "__hotswap(g, h);\n"
               "return f();\n"),
            .exit_code = 3,
        },
        {
            "__hotswap null replacement fails", __LINE__,
            SVI("int f(void){ return 1; }\n"
               "int (*p)(void) = 0;\n"
               "return __hotswap(f, p) != 0;\n"),
            .exit_code = 1,
        },
        {
            "static local: aggregate preinitialization", __LINE__,
            SVI("struct S { int a[3]; unsigned b:5; const char* s; };\n"
                "int f(void){ static struct S x = {{3,4,5}, 17, \"ok\"}; return x.a[1]++ + x.b + x.s[0]; }\n"
                "return f() + f();\n"),
            .exit_code = 265,
        },
        {
            "static local: pointer preinitialization", __LINE__,
            SVI("int g;\n"
                "int f(void){ static int* p = &g; return ++*p; }\n"
                "return f() + f();\n"),
            .exit_code = 3,
        },
        // Static locals
        {
            "static local", __LINE__,
            SVI("int counter(void){\n"
               "  static int n = 0;\n"
               "  return n++;\n"
               "}\n"
               "counter();\n"
               "counter();\n"
               "return counter();\n"),
            .exit_code = 2,
        },
        // Global variables
        {
            "global variable", __LINE__,
            SVI("int g = 10;\n"
               "int get(void){ return g; }\n"
               "void set(int v){ g = v; }\n"
               "set(42);\n"
               "return get();\n"),
            .exit_code = 42,
        },
        {
            "_Module.symbol global variable", __LINE__,
            SVI("int g = 10;\n"
               "int* p = __root_module().symbol(\"g\", typeof(*p));\n"
               "if(!p) return 99;\n"
               "*p = 42;\n"
               "return g;\n"),
            .exit_code = 42,
        },
        {
            "_Module.symbol variable type mismatch", __LINE__,
            SVI("int g = 10;\n"
               "long* p = __root_module().symbol(\"g\", typeof(*p));\n"
               "return p == 0;\n"),
            .exit_code = 1,
        },
        {
            "_Module.symbol missing extern", __LINE__,
            SVI("extern int missing_extern_symbol;\n"
               "int* p = __root_module().symbol(\"missing_extern_symbol\", typeof(*p));\n"
               "return p == 0;\n"),
            .exit_code = 1,
        },
        {
            "index address: signed, unsigned, narrowing and scale", __LINE__,
            SVI("struct triple { unsigned char x[3]; };\n"
                "struct triple a[260] = {0};\n"
                "struct triple* p = a + 2;\n"
                "int neg = -2; unsigned int u = 255; int n = 257;\n"
                "p[neg].x[1] = 11; p[u].x[2] = 19;\n"
                "p[(unsigned char)n].x[0] = 23;\n"
                "signed char sc = -2; short sh = -1; long l = -2;\n"
                "p[sh].x[0] = 29;\n"
                "if (p[sc].x[1] != 11 || p[l].x[1] != 11) return 1;\n"
                "if (a[257].x[2] != 19 || a[3].x[0] != 23) return 2;\n"
                "return a[1].x[0] == 29 ? 0 : 3;\n"),
            .exit_code = 0,
        },
        {
            "index address: evaluate base and index once", __LINE__,
            SVI("int a[4] = {10,20,30,40}; int calls = 0;\n"
                "int* base(void) { calls++; return a; }\n"
                "int index(void) { calls++; return 2; }\n"
                "int v = base()[index()];\n"
                "return v == 30 && calls == 2 ? 0 : 1;\n"),
            .exit_code = 0,
        },
        {
            "immediate increment: wrap, pre/post values and pointer scale", __LINE__,
            SVI("unsigned int u = ~0u; unsigned long long w = ~0ull;\n"
                "unsigned int old = u++;\n"
                "if (u != 0 || old != ~0u) return 1;\n"
                "if (--u != ~0u) return 2;\n"
                "unsigned long long wide = w++;\n"
                "if (w != 0 || wide != ~0ull || --w != ~0ull) return 3;\n"
                "int a[3] = {4,5,6}; int* p = a; int* q = p++;\n"
                "if (*q != 4 || *p != 5 || *--p != 4) return 4;\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "branch conditions: widths, signedness and materialized values", __LINE__,
            SVI("int s = -1; unsigned int u = 0x80000000u;\n"
                "long long l = -4294967296ll; unsigned long long w = 0x8000000000000000ull;\n"
                "if (s >= 0) return 1; if (u <= 1u) return 2;\n"
                "if (l > 0ll) return 3; if (w < 1ull) return 4;\n"
                "if (s == 0) return 5; if (l != -4294967296ll) return 6;\n"
                "int v = 0; if (v = (s < 0)) { if (v != 1) return 7; }\n"
                "else return 8;\n"
                "int n = 0; do { n++; if (n == 2) continue; } while (n < 3);\n"
                "if (n != 3) return 9;\n"
                "unsigned long long high = 1ull << 40; if (high) {} else return 10;\n"
                "double zero = -0.0; if (zero) return 11;\n"
                "__int128 big = (__int128)1 << 100; if (big) {} else return 12;\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "immediate ALU: int", __LINE__,
            SVI("int x = -37;\n"
                "if ((x + 5) != -32) return 1;\n"
                "{ typeof(x) y = x; y += 5; if (y != -32) return 11; }\n"
                "if ((x - 5) != -42) return 2;\n"
                "{ typeof(x) y = x; y -= 5; if (y != -42) return 12; }\n"
                "if ((x * 3) != -111) return 3;\n"
                "{ typeof(x) y = x; y *= 3; if (y != -111) return 13; }\n"
                "if ((x / 5) != -7) return 4;\n"
                "{ typeof(x) y = x; y /= 5; if (y != -7) return 14; }\n"
                "if ((x % 5) != -2) return 5;\n"
                "{ typeof(x) y = x; y %= 5; if (y != -2) return 15; }\n"
                "if ((x & 15) != 11) return 6;\n"
                "{ typeof(x) y = x; y &= 15; if (y != 11) return 16; }\n"
                "if ((x | 15) != -33) return 7;\n"
                "{ typeof(x) y = x; y |= 15; if (y != -33) return 17; }\n"
                "if ((x ^ 15) != -44) return 8;\n"
                "{ typeof(x) y = x; y ^= 15; if (y != -44) return 18; }\n"
                "if ((x >> 3) != -5) return 9;\n"
                "{ typeof(x) y = x; y >>= 3; if (y != -5) return 19; }\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "immediate ALU: unsigned int", __LINE__,
            SVI("unsigned int x = 4026531877u;\n"
                "if ((x + 5u) != 4026531882u) return 1;\n"
                "{ typeof(x) y = x; y += 5u; if (y != 4026531882u) return 11; }\n"
                "if ((x - 5u) != 4026531872u) return 2;\n"
                "{ typeof(x) y = x; y -= 5u; if (y != 4026531872u) return 12; }\n"
                "if ((x * 3u) != 3489661039u) return 3;\n"
                "{ typeof(x) y = x; y *= 3u; if (y != 3489661039u) return 13; }\n"
                "if ((x / 5u) != 805306375u) return 4;\n"
                "{ typeof(x) y = x; y /= 5u; if (y != 805306375u) return 14; }\n"
                "if ((x % 5u) != 2u) return 5;\n"
                "{ typeof(x) y = x; y %= 5u; if (y != 2u) return 15; }\n"
                "if ((x & 15u) != 5u) return 6;\n"
                "{ typeof(x) y = x; y &= 15u; if (y != 5u) return 16; }\n"
                "if ((x | 15u) != 4026531887u) return 7;\n"
                "{ typeof(x) y = x; y |= 15u; if (y != 4026531887u) return 17; }\n"
                "if ((x ^ 15u) != 4026531882u) return 8;\n"
                "{ typeof(x) y = x; y ^= 15u; if (y != 4026531882u) return 18; }\n"
                "if ((x >> 3u) != 503316484u) return 9;\n"
                "{ typeof(x) y = x; y >>= 3u; if (y != 503316484u) return 19; }\n"
                "if ((x << 3u) != 2147483944u) return 10;\n"
                "{ typeof(x) y = x; y <<= 3u; if (y != 2147483944u) return 20; }\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "immediate ALU: long long", __LINE__,
            SVI("long long x = -37ll;\n"
                "if ((x + 5ll) != -32ll) return 1;\n"
                "{ typeof(x) y = x; y += 5ll; if (y != -32ll) return 11; }\n"
                "if ((x - 5ll) != -42ll) return 2;\n"
                "{ typeof(x) y = x; y -= 5ll; if (y != -42ll) return 12; }\n"
                "if ((x * 3ll) != -111ll) return 3;\n"
                "{ typeof(x) y = x; y *= 3ll; if (y != -111ll) return 13; }\n"
                "if ((x / 5ll) != -7ll) return 4;\n"
                "{ typeof(x) y = x; y /= 5ll; if (y != -7ll) return 14; }\n"
                "if ((x % 5ll) != -2ll) return 5;\n"
                "{ typeof(x) y = x; y %= 5ll; if (y != -2ll) return 15; }\n"
                "if ((x & 15ll) != 11ll) return 6;\n"
                "{ typeof(x) y = x; y &= 15ll; if (y != 11ll) return 16; }\n"
                "if ((x | 15ll) != -33ll) return 7;\n"
                "{ typeof(x) y = x; y |= 15ll; if (y != -33ll) return 17; }\n"
                "if ((x ^ 15ll) != -44ll) return 8;\n"
                "{ typeof(x) y = x; y ^= 15ll; if (y != -44ll) return 18; }\n"
                "if ((x >> 3ll) != -5ll) return 9;\n"
                "{ typeof(x) y = x; y >>= 3ll; if (y != -5ll) return 19; }\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "immediate ALU: unsigned long long", __LINE__,
            SVI("unsigned long long x = 17293822569371140133ull;\n"
                "if ((x + 5ull) != 17293822569371140138ull) return 1;\n"
                "{ typeof(x) y = x; y += 5ull; if (y != 17293822569371140138ull) return 11; }\n"
                "if ((x - 5ull) != 17293822569371140128ull) return 2;\n"
                "{ typeof(x) y = x; y -= 5ull; if (y != 17293822569371140128ull) return 12; }\n"
                "if ((x * 3ull) != 14987979560694317167ull) return 3;\n"
                "{ typeof(x) y = x; y *= 3ull; if (y != 14987979560694317167ull) return 13; }\n"
                "if ((x / 5ull) != 3458764513874228026ull) return 4;\n"
                "{ typeof(x) y = x; y /= 5ull; if (y != 3458764513874228026ull) return 14; }\n"
                "if ((x % 5ull) != 3ull) return 5;\n"
                "{ typeof(x) y = x; y %= 5ull; if (y != 3ull) return 15; }\n"
                "if ((x & 15ull) != 5ull) return 6;\n"
                "{ typeof(x) y = x; y &= 15ull; if (y != 5ull) return 16; }\n"
                "if ((x | 15ull) != 17293822569371140143ull) return 7;\n"
                "{ typeof(x) y = x; y |= 15ull; if (y != 17293822569371140143ull) return 17; }\n"
                "if ((x ^ 15ull) != 17293822569371140138ull) return 8;\n"
                "{ typeof(x) y = x; y ^= 15ull; if (y != 17293822569371140138ull) return 18; }\n"
                "if ((x >> 3ull) != 2161727821171392516ull) return 9;\n"
                "{ typeof(x) y = x; y >>= 3ull; if (y != 2161727821171392516ull) return 19; }\n"
                "if ((x << 3ull) != 9223372039002259752ull) return 10;\n"
                "{ typeof(x) y = x; y <<= 3ull; if (y != 9223372039002259752ull) return 20; }\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "store immediate: scalar widths and member offsets", __LINE__,
            SVI("struct S { unsigned char guard0, a, guard1; unsigned short b; unsigned int c; unsigned long long d; float f; double g; };\n"
                "struct S s = {.guard0=41, .guard1=43}; struct S* p = &s;\n"
                "p->a = 255; p->b = 65535; p->c = 0x89abcdefu; p->d = 0xfedcba9876543210ull;\n"
                "p->f = 1.5f; p->g = -0.0;\n"
                "if (p->guard0 != 41 || p->guard1 != 43) return 1;\n"
                "if (p->a != 255 || p->b != 65535 || p->c != 0x89abcdefu) return 2;\n"
                "if (p->d != 0xfedcba9876543210ull || p->f != 1.5f) return 3;\n"
                "if (1.0 / p->g > 0.0) return 4; return 0;\n"),
            .exit_code = 0,
        },
        {
            "store immediate: evaluate address once, preserve assignment result", __LINE__,
            SVI("int a[3] = {3,4,5}; int calls = 0;\n"
                "int* get(void) { calls++; return a; }\n"
                "int i = 0; get()[i++] = 17;\n"
                "if (calls != 1 || i != 1 || a[0] != 17 || a[1] != 4) return 1;\n"
                "int v = (get()[i++] = 19);\n"
                "if (calls != 2 || i != 2 || v != 19 || a[1] != 19) return 2;\n"
                "int* p = a; *p = (a[2] = 23);\n"
                "return a[0] == 23 && a[2] == 23 ? 0 : 3;\n"),
            .exit_code = 0,
        },
        {
            "store immediate: bitfield, atomic and volatile assignments", __LINE__,
            SVI("struct S { unsigned int a:3; unsigned int b:5; };\n"
                "struct S s = {0}; struct S* p = &s; p->b = 17; p->a = 7;\n"
                "_Atomic int a = 0; _Atomic int* q = &a; *q = 31;\n"
                "volatile int v = 0; volatile int* r = &v; *r = 37;\n"
                "return s.a == 7 && s.b == 17 && a == 31 && v == 37 ? 0 : 1;\n"),
            .exit_code = 0,
        },
        {
            "branch context: nested short circuit and evaluation order", __LINE__,
            SVI("int log = 0; int mark(int v, int id) { log = log*10 + id; return v; }\n"
                "int logs[8] = {13,123,13,12,13,123,13,12};\n"
                "int neglogs[8] = {123,1,12,1,123,1,12,1};\n"
                "for (int i=0; i<8; i++) {\n"
                "  int a=i&1, b=i&2, c=i&4, got=0; int want=(a&&b)||c;\n"
                "  log=0; if ((mark(a,1)&&mark(b,2))||mark(c,3)) got=1; else got=0;\n"
                "  if (got != want || log != logs[i]) return 1;\n"
                "  log=0; if (!(mark(a,1)||mark(b,2)) && mark(c,3)) got=1; else got=0;\n"
                "  if (got != (i==4) || log != neglogs[i]) return 2;\n"
                "  log=0; int v=((mark(a,1)&&mark(b,2))||mark(c,3)) ? mark(11,4) : mark(22,5);\n"
                "  if (v != (want?11:22) || log != logs[i]*10+(want?4:5)) return 3;\n"
                "  log=0; ((mark(a,1)&&mark(b,2))||mark(c,3)) && mark(1,4);\n"
                "  if (log != (want?logs[i]*10+4:logs[i])) return 4;\n"
                "} return 0;\n"),
            .exit_code = 0,
        },
        {
            "branch context: loop backedges, comma and value results", __LINE__,
            SVI("int n=0; do { n++; if(n==2) continue; } while(n<3 || (n<5 && n!=4));\n"
                "if(n!=4) return 1;\n"
                "while(n<5 && (n==4 || n==0)) n++;\n"
                "if(n!=5) return 2;\n"
                "int calls=0; for(int i=0; (calls++, i<3 && n>0); i++) { if(i==1) continue; n--; }\n"
                "if(calls!=4 || n!=3) return 3;\n"
                "int v=0; if(v=(n && calls)) {} else return 4;\n"
                "if(v!=1) return 5; double z=-0.0;\n"
                "if(z && ++calls) return 6; if(calls!=4) return 7;\n"
                "if(!(z || n)) return 8; return 0;\n"),
            .exit_code = 0,
        },
        {
            "constant conversions: signedness, truncation, exact and inexact floats", __LINE__,
            SVI("unsigned char a=511; signed char b=255; long long c=(signed char)255;\n"
                "unsigned long long u=(unsigned int)4294967295u; _Bool yes=256, no=0;\n"
                "if(a!=255 || b!=-1 || c!=-1 || u!=4294967295ull || !yes || no) return 1;\n"
                "float f=16777216, rounded=16777217; double d=9007199254740992ll;\n"
                "double dr=9007199254740993ll; double neg=-37;\n"
                "if(f!=16777216.0f || rounded!=16777216.0f || d!=9007199254740992.0) return 2;\n"
                "if(dr!=9007199254740992.0 || neg!=-37.0) return 3;\n"
                "int calls=0; double once=(calls++, 2);\n"
                "return once==2.0 && calls==1 ? 0 : 4;\n"),
            .exit_code = 0,
        },
        {
            "float constant conversion: negation", __LINE__,
            SVI("double a = -1.f;\n"
                "return a == -1.;\n"),
            .exit_code = 1,
        },
        {
            "float constant conversion: exact narrowing and signed zero", __LINE__,
            SVI("float a=2.5, b=-2.5, zero=0.0, negzero=-0.0;\n"
                "float normal=0x1p-126, sub=0x1p-149, max=0x1.fffffep127;\n"
                "float next=0x1.000002p0;\n"
                "if(a!=2.5f || b!=-2.5f || zero!=0.0f || negzero!=0.0f) return 1;\n"
                "if(1.0f/zero<0.0f || 1.0f/negzero>0.0f) return 2;\n"
                "if(normal!=0x1p-126f || sub!=0x1p-149f || max!=0x1.fffffep127f) return 3;\n"
                "return next==0x1.000002p0f ? 0 : 4;\n"),
            .exit_code = 0,
        },
        {
            "float constant conversion: finite widening", __LINE__,
            SVI("double a=2.5f, b=-2.5f, sub=0x1p-149f, zero=-0.0f;\n"
                "if(a!=2.5 || b!=-2.5 || sub!=0x1p-149 || zero!=0.0) return 1;\n"
                "return 1.0/zero<0.0 ? 0 : 2;\n"),
            .exit_code = 0,
        },
        {
            "float constant conversion: inexact narrowing stays runtime", __LINE__,
            SVI("float decimal=0.1, halfway=0x1.000001p0, tiny=0x1p-150;\n"
                "if(decimal!=0.1f || halfway!=1.0f || tiny!=0.0f) return 1;\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "float constant conversion: preserve side effects and dynamic casts", __LINE__,
            SVI("int calls=0; float a=(calls++, -2.5);\n"
                "float b=2.5f; double dynamic=b;\n"
                "return calls==1 && a==-2.5f && dynamic==2.5 ? 0 : 1;\n"),
            .exit_code = 0,
        },
        // Type conversions
        {
            "unsigned wrap", __LINE__,
            SVI("unsigned int x = 0;\n"
               "x = x - 1;\n"
               "return (x > 1000) ? 1 : 0;\n"),
            .exit_code = 1,
        },
        // Compound literals
        {
            "compound literal", __LINE__,
            SVI("struct pt { int x; int y; };\n"
               "struct pt p = (struct pt){.x=3, .y=4};\n"
               "return p.x + p.y;\n"),
            .exit_code = 7,
        },
        {
            "compound literal parens", __LINE__,
            SVI("struct pt { int x; int y; };\n"
               "struct pt p = ((struct pt){.x=3, .y=4});\n"
               "return p.x + p.y;\n"),
            .exit_code = 7,
        },
        {
            "compound literal lvalue", __LINE__,
            SVI("struct pt { int x; int y; };\n"
               "struct pt* p = &(struct pt){.x=3, .y=4};\n"
               "return p->x + p->y;\n"),
            .exit_code = 7,
        },
        {
            "compound literal lvalue parens", __LINE__,
            SVI("struct pt { int x; int y; };\n"
               "struct pt* p = &((struct pt){.x=3, .y=4});\n"
               "return p->x + p->y;\n"),
            .exit_code = 7,
        },
        {
            "compound literal member access", __LINE__,
            SVI("struct pt { int x; int y; };\n"
               "return (struct pt){.x=10, .y=20}.y;\n"),
            .exit_code = 20,
        },
        {
            "compound literal array subscript", __LINE__,
            SVI("return (int[]){10, 20, 30}[1];\n"),
            .exit_code = 20,
        },
        {
            "compound literal assignment", __LINE__,
            SVI("struct pt { int x; int y; };\n"
               "struct pt p = {0, 0};\n"
               "p = (struct pt){.x=5, .y=6};\n"
               "return p.x + p.y;\n"),
            .exit_code = 11,
        },
        {
            "compound literal in struct init", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer o = { 1, (struct Inner){2, 3} };\n"
               "return o.x + o.inner.a + o.inner.b;\n"),
            .exit_code = 6,
        },
        {
            "compound literal array lvalue", __LINE__,
            SVI("int *p = (int[]){10, 20, 30};\n"
               "p[1] = 99;\n"
               "return p[1];\n"),
            .exit_code = 99,
        },
        {
            "compound literal member lvalue", __LINE__,
            SVI("struct pt { int x; int y; };\n"
               "(struct pt){.x=3, .y=4}.x = 10;\n"
               "return 0;\n"),
            .exit_code = 0,
        },
        {
            "compound literal deep lvalue", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer *p = &(struct Outer){.x=1, .inner={.a=2, .b=3}};\n"
               "p->inner.a = 42;\n"
               "return p->inner.a + p->inner.b;\n"),
            .exit_code = 45,
        },
        {
            "compound literal array element lvalue", __LINE__,
            SVI("struct pt { int x; int y; };\n"
               "struct pt *p = (struct pt[]){ {1, 2}, {3, 4} };\n"
               "p[1].x = 50;\n"
               "return p[0].x + p[1].x + p[1].y;\n"),
            .exit_code = 55,
        },
        {
            "compound literal member array decay", __LINE__,
            SVI("struct SmallStr { char txt[8]; };\n"
               "char *p = (struct SmallStr){\"hello\"}.txt;\n"
               "return p[1];\n"),
            .exit_code = 'e',
        },
        {
            "compound literal as function arg", __LINE__,
            SVI("struct pt { int x; int y; };\n"
               "int* bump(int* p) { *p += 10; return p; }\n"
               "int* q = bump(&(struct pt){3, 4}.x);\n"
               "return *q;\n"),
            .exit_code = 13,
        },
        // String literals
        {
            "string literal indexing", __LINE__,
            SVI("const char *s = \"hello\";\n"
               "return s[1];\n"),
            .exit_code = 'e',
        },
        {
            "deref array variable", __LINE__,
            SVI("int arr[] = {10, 20, 30};\n"
               "return *arr;\n"),
            .exit_code = 10,
        },
        {
            "reversed subscript string literal", __LINE__,
            SVI("return 1[\"hello\"];\n"),
            .exit_code = 'e',
        },
        {
            "reversed subscript array", __LINE__,
            SVI("int arr[] = {10, 20, 30};\n"
               "return 2[arr];\n"),
            .exit_code = 30,
        },
        {
            "ternary string literal subscript", __LINE__,
            SVI("int x = 1;\n"
               "return (x ? \"abc\" : \"def\")[1];\n"),
            .exit_code = 'b',
        },
        {
            "ternary string literal subscript false", __LINE__,
            SVI("int x = 0;\n"
               "return (x ? \"abc\" : \"def\")[1];\n"),
            .exit_code = 'e',
        },
        {
            "deref string literal", __LINE__,
            SVI("return *\"hello\";\n"),
            .exit_code = 'h',
        },
        {
            "deref L string literal", __LINE__,
            SVI("return *L\"hello\";\n"),
            .exit_code = 'h',
        },
        {
            "deref u string literal", __LINE__,
            SVI("return *u\"hello\";\n"),
            .exit_code = 'h',
        },
        {
            "deref U string literal", __LINE__,
            SVI("return *U\"hello\";\n"),
            .exit_code = 'h',
        },
        {
            "deref u8 string literal", __LINE__,
            SVI("return *u8\"hello\";\n"),
            .exit_code = 'h',
        },
        // Wide string literals
        {
            "L string indexing", __LINE__,
            SVI("return L\"hello\"[1];\n"),
            .exit_code = 'e',
        },
        {
            "u string indexing", __LINE__,
            SVI("return u\"hello\"[0];\n"),
            .exit_code = 'h',
        },
        {
            "U string indexing", __LINE__,
            SVI("return U\"hello\"[2];\n"),
            .exit_code = 'l',
        },
        {
            "u8 string indexing", __LINE__,
            SVI("return u8\"hello\"[4];\n"),
            .exit_code = 'o',
        },
        {
            "L string null terminator", __LINE__,
            SVI("return L\"hi\"[2];\n"),
            .exit_code = 0,
        },
        {
            "u string null terminator", __LINE__,
            SVI("return u\"hi\"[2];\n"),
            .exit_code = 0,
        },
        {
            "U string null terminator", __LINE__,
            SVI("return U\"hi\"[2];\n"),
            .exit_code = 0,
        },
        {
            "u8 string null terminator", __LINE__,
            SVI("return u8\"hi\"[2];\n"),
            .exit_code = 0,
        },
        {
            "sizeof L string element", __LINE__,
            SVI("return sizeof(L\"x\"[0]);\n"),
            .exit_code = 4, // sizeof(int)
        },
        {
            "sizeof u string element", __LINE__,
            SVI("return sizeof(u\"x\"[0]);\n"),
            .exit_code = 2, // sizeof(unsigned short)
        },
        {
            "sizeof U string element", __LINE__,
            SVI("return sizeof(U\"x\"[0]);\n"),
            .exit_code = 4, // sizeof(unsigned int)
        },
        {
            "sizeof u8 string element", __LINE__,
            SVI("return sizeof(u8\"x\"[0]);\n"),
            .exit_code = 1, // sizeof(unsigned char)
        },
        {
            "sizeof L string", __LINE__,
            SVI("return sizeof(L\"hello\");\n"),
            .exit_code = 24, // 6 * 4
        },
        {
            "sizeof u string", __LINE__,
            SVI("return sizeof(u\"hello\");\n"),
            .exit_code = 12, // 6 * 2
        },
        {
            "sizeof U string", __LINE__,
            SVI("return sizeof(U\"hello\");\n"),
            .exit_code = 24, // 6 * 4
        },
        {
            "sizeof u8 string", __LINE__,
            SVI("return sizeof(u8\"hello\");\n"),
            .exit_code = 6, // 6 * 1
        },
        {
            "L string escape", __LINE__,
            SVI("return L\"a\\nb\"[1];\n"),
            .exit_code = '\n',
        },
        {
            "L string UCN", __LINE__,
            SVI("return L\"\\u0041\"[0];\n"),
            .exit_code = 'A',
        },
        {
            "u string UCN", __LINE__,
            SVI("return u\"\\u00E9\"[0];\n"),
            .exit_code = 233, // 0xE9
        },
        {
            "U string UCN above BMP", __LINE__,
            SVI("return U\"\\U0001F600\"[0] == 0x1F600;\n"),
            .exit_code = 1,
        },
        {
            "L string array init", __LINE__,
            SVI("int arr[] = L\"AB\";\n"
               "return arr[0] + arr[1];\n"),
            .exit_code = 'A' + 'B',
        },
        {
            "u string array init", __LINE__,
            SVI("unsigned short arr[] = u\"AB\";\n"
               "return arr[0] + arr[1];\n"),
            .exit_code = 131, // 65 + 66
        },
        {
            "U string array init", __LINE__,
            SVI("unsigned arr[] = U\"AB\";\n"
               "return arr[0] + arr[1];\n"),
            .exit_code = 131, // 65 + 66
        },
        {
            "u8 string array init", __LINE__,
            SVI("unsigned char arr[] = u8\"AB\";\n"
               "return arr[0] + arr[1];\n"),
            .exit_code = 131, // 65 + 66
        },
        {
            "char array truncation", __LINE__,
            SVI("char t[3] = \"abc\";\n"
               "return t[2];\n"),
            .exit_code = 'c',
        },
        {
            "nested char array truncation", __LINE__,
            SVI("char t[1][3] = {\"abc\"};\n"
               "return t[0][2];\n"),
            .exit_code = 'c',
        },
        {
            "designated nested char array truncation", __LINE__,
            SVI("char t[1][3] = {[0] = \"abc\"};\n"
               "return t[0][2];\n"),
            .exit_code = 'c',
        },
        {
            "static char array truncation", __LINE__,
            SVI("static char t[3] = \"abc\";\n"
               "return t[2];\n"),
            .exit_code = 'c',
        },
        {
            "nested static char array truncation", __LINE__,
            SVI("char t[1][3] = {\"abc\"};\n"
               "return t[0][1];\n"),
            .exit_code = 'b',
        },
        {
            "L string array truncation", __LINE__,
            SVI("int t[3] = L\"abc\";\n"
               "return t[2];\n"),
            .exit_code = 'c',
        },
        {
            "u string array truncation", __LINE__,
            SVI("unsigned short t[3] = u\"abc\";\n"
               "return t[2];\n"),
            .exit_code = 'c',
        },
        {
            "U string array truncation", __LINE__,
            SVI("unsigned t[3] = U\"abc\";\n"
               "return t[2];\n"),
            .exit_code = 'c',
        },
        // Multidimensional array
        {
            "2d array", __LINE__,
            SVI("int m[2][3] = {{1,2,3},{4,5,6}};\n"
               "return m[1][2];\n"),
            .exit_code = 6,
        },
        // Deeply nested expressions
        {
            "nested ternary", __LINE__,
            SVI("int x = 3;\n"
               "return x == 1 ? 10 : x == 2 ? 20 : x == 3 ? 30 : 40;\n"),
            .exit_code = 30,
        },
        // Zero iteration
        {
            "zero iteration for", __LINE__,
            SVI("int s = 5;\n"
               "for(int i = 0; i < 0; i++) s += i;\n"
               "return s;\n"),
            .exit_code = 5,
        },
        {
            "zero iteration while", __LINE__,
            SVI("int s = 5;\n"
               "while(0) s = 0;\n"
               "return s;\n"),
            .exit_code = 5,
        },
        // Unary plus
        {
            "unary plus", __LINE__,
            SVI("int x = -5;\n"
               "return +x;\n"),
            .exit_code = -5,
        },
        // Ternary: side effects only in taken branch
        {
            "ternary: no side effect in untaken", __LINE__,
            SVI("int x = 0;\n"
               "int y = 1 ? (x = 10) : (x = 20);\n"
               "return x;\n"),
            .exit_code = 10,
        },
        // Cast: sign extension
        {
            "cast: sign extend char to int", __LINE__,
            SVI("char c = -1;\n"
               "int x = (int)c;\n"
               "return x;\n"),
            .exit_code = -1,
        },
        {
            "cast: unsigned enum to float", __LINE__,
            SVI("typedef enum : unsigned { V = 3000000000u } E;\n"
               "E e = V;\n"
               "double d = (double)e;\n"
               "return (int)(d / 1000000000.0);\n"),
            .exit_code = 3,
        },
        {
            "cast: float to unsigned enum", __LINE__,
            SVI("typedef enum : unsigned { V = 0 } E;\n"
               "E e = (E)3000000000.0;\n"
               "return (int)(e / 1000000000u);\n"),
            .exit_code = 3,
        },
        {
            "cast: float to unsigned long long enum", __LINE__,
            SVI("typedef enum : unsigned long long { V = 0 } E;\n"
               "E e = (E)1e19;\n"
               "return (int)(e / (unsigned long long)1e18);\n"),
            .exit_code = 10,
        },
        // Subscript equivalence *(a+i)
        {
            "subscript: *(a+i)", __LINE__,
            SVI("int arr[3] = {10, 20, 30};\n"
               "return *(arr + 2);\n"),
            .exit_code = 30,
        },
        // Return without value (void function)
        {
            "return void", __LINE__,
            SVI("void noop(void){ return; }\n"
               "noop();\n"
               "return 42;\n"),
            .exit_code = 42,
        },
        // for(;;) infinite loop with break
        {
            "for: infinite with break", __LINE__,
            SVI("int i = 0;\n"
               "for(;;){ if(i == 7) break; i++; }\n"
               "return i;\n"),
            .exit_code = 7,
        },
        // Empty statement
        {
            "empty statement", __LINE__,
            SVI(";;;\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        // Integer types: short
        {
            "type: short", __LINE__,
            SVI("short s = 127;\n"
               "return s;\n"),
            .exit_code = 127,
        },
        // Integer types: long
        {
            "type: long", __LINE__,
            SVI("long l = 100;\n"
               "return (int)l;\n"),
            .exit_code = 100,
        },
        // Integer types: unsigned char
        {
            "type: unsigned char", __LINE__,
            SVI("unsigned char c = 200;\n"
               "return c;\n"),
            .exit_code = 200,
        },
        // Multi-level pointer
        {
            "pointer: multi-level", __LINE__,
            SVI("int x = 42;\n"
               "int *p = &x;\n"
               "int **pp = &p;\n"
               "return **pp;\n"),
            .exit_code = 42,
        },
        // Null pointer check
        {
            "pointer: null check", __LINE__,
            SVI("int *p = 0;\n"
               "return p == 0;\n"),
            .exit_code = 1,
        },
        // Array decay to pointer
        {
            "array: decay to pointer", __LINE__,
            SVI("int arr[3] = {10, 20, 30};\n"
               "int *p = arr;\n"
               "return *p;\n"),
            .exit_code = 10,
        },
        // Pointer comparison
        {
            "pointer: comparison", __LINE__,
            SVI("int arr[3] = {0};\n"
               "int *a = &arr[0];\n"
               "int *b = &arr[2];\n"
               "return a < b;\n"),
            .exit_code = 1,
        },
        // void* as generic pointer
        {
            "pointer: void star", __LINE__,
            SVI("int x = 42;\n"
               "void *v = &x;\n"
               "int *p = (int*)v;\n"
               "return *p;\n"),
            .exit_code = 42,
        },
        // Local variable without initializer
        {
            "decl: uninitialized local", __LINE__,
            SVI("int x;\n"
               "x = 7;\n"
               "return x;\n"),
            .exit_code = 7,
        },
        // Adjacent string literal concatenation
        {
            "string: adjacent concat", __LINE__,
            SVI("const char *s = \"hel\" \"lo\";\n"
               "return s[3];\n"),
            .exit_code = 'l',
        },
        // Integer constants: hex and octal
        {
            "constant: hex", __LINE__,
            SVI("return 0x2A;\n"),
            .exit_code = 42,
        },
        {
            "constant: octal", __LINE__,
            SVI("return 052;\n"),
            .exit_code = 42,
        },
        // Integer promotion: unsigned + signed
        {
            "promotion: unsigned arith", __LINE__,
            SVI("unsigned int a = 10;\n"
               "int b = 3;\n"
               "return (int)(a - b);\n"),
            .exit_code = 7,
        },
        // Truncation on narrowing assignment
        {
            "promotion: narrowing truncation", __LINE__,
            SVI("int x = 0x1FF;\n"
               "unsigned char c = x;\n"
               "return c;\n"),
            .exit_code = 255,
        },
        // Fibonacci (recursion)
        {
            "recursion: fibonacci", __LINE__,
            SVI("int fib(int n){\n"
               "  if(n <= 1) return n;\n"
               "  return fib(n-1) + fib(n-2);\n"
               "}\n"
               "return fib(11);\n"),
            .exit_code = 89,
        },
        {
            "recursion: deep calls preserve locals and alloca", __LINE__,
            SVI("int descend(int n){\n"
                "  int *p = __builtin_alloca(sizeof(int)); *p = n;\n"
                "  if(!n) return 0;\n"
                "  int r = descend(n-1);\n"
                "  return r + (*p == n);\n"
                "}\n"
                "return descend(100);\n"),
            .exit_code = 100,
        },
        {
            "recursion: deep indirect calls", __LINE__,
            SVI("int (*next)(int);\n"
                "int descend(int n){ return n ? next(n-1) : 1; }\n"
                "next = descend;\n"
                "return next(100);\n"),
            .exit_code = 1,
        },
        // Early return from deep nesting
        {
            "early return from nesting", __LINE__,
            SVI("int find(void){\n"
               "  for(int i = 0; i < 10; i++){\n"
               "    for(int j = 0; j < 10; j++){\n"
               "      if(i == 3 && j == 4) return i * 10 + j;\n"
               "    }\n"
               "  }\n"
               "  return -1;\n"
               "}\n"
               "return find();\n"),
            .exit_code = 34,
        },
        // Comma in for-loop
        {
            "for: comma in clauses", __LINE__,
            SVI("int a, b;\n"
               "for(a = 0, b = 10; a < 5; a++, b--)\n"
               "  ;\n"
               "return a * 10 + b;\n"),
            .exit_code = 55,
        },
        // Struct: sizeof with padding
        {
            "struct: sizeof", __LINE__,
            SVI("struct s { char c; int i; };\n"
               "return sizeof(struct s) >= 5;\n"),
            .exit_code = 1,
        },
        // Union: sizeof is max member
        {
            "union: sizeof", __LINE__,
            SVI("union u { char c; int i; };\n"
               "return sizeof(union u) == sizeof(int);\n"),
            .exit_code = 1,
        },
        // Cast between pointer types
        {
            "pointer: cast types", __LINE__,
            SVI("int x = 0x01020304;\n"
               "char *cp = (char*)&x;\n"
               "return *cp != 0;\n"),  // just check it doesn't crash; endian-independent
            .exit_code = 1,
        },
        // Sizeof pointer
        {
            "sizeof pointer", __LINE__,
            SVI("return sizeof(int*) == sizeof(void*);\n"),
            .exit_code = 1,
        },
        // Sizeof array
        {
            "sizeof array", __LINE__,
            SVI("int arr[10];\n"
               "return sizeof arr / sizeof arr[0];\n"),
            .exit_code = 10,
        },
        // Float / double
        {
            "float: basic arith", __LINE__,
            SVI("float f = 3.5f;\n"
               "return (int)(f * 2.0f);\n"),
            .exit_code = 7,
        },
        {
            "double: basic arith", __LINE__,
            SVI("double d = 6.5;\n"
               "return (int)(d + 0.5);\n"),
            .exit_code = 7,
        },
        {
            "float: truncation to int", __LINE__,
            SVI("float f = 9.9f;\n"
               "return (int)f;\n"),
            .exit_code = 9,
        },
        {
            "double: negative truncation", __LINE__,
            SVI("double d = -3.7;\n"
               "return (int)d;\n"),
            .exit_code = -3,
        },
        {
            "int to float to int", __LINE__,
            SVI("int a = 7;\n"
               "float f = (float)a;\n"
               "return (int)(f + 0.5f);\n"),
            .exit_code = 7,
        },
        {
            "float: comparison", __LINE__,
            SVI("float a = 1.5f;\n"
               "float b = 2.5f;\n"
               "return a < b;\n"),
            .exit_code = 1,
        },
        {
            "double: sizeof", __LINE__,
            SVI("return sizeof(double);\n"),
            .exit_code = 8,
        },
        {
            "float: sizeof", __LINE__,
            SVI("return sizeof(float);\n"),
            .exit_code = 4,
        },
        {
            "float: division", __LINE__,
            SVI("float f = 10.0f / 3.0f;\n"
               "return (int)(f * 3.0f);\n"),
            .exit_code = 10,
        },
        {
            "double: mixed arith with int", __LINE__,
            SVI("int a = 5;\n"
               "double d = a + 2.5;\n"
               "return (int)d;\n"),
            .exit_code = 7,
        },
        {
            "lambda (IIFE)", __LINE__,
            SVI("int x = int(){\n"
               "    return 42;\n"
               "}();\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "lambda (IIFE)", __LINE__,
            SVI("return int(){\n"
               "    return 42;\n"
               "}();\n"),
            .exit_code = 42,
        },
        {
            "lambda (IIFE) parenthesized", __LINE__,
            SVI("return (int(int x){ return x + 1; })(41);\n"),
            .exit_code = 42,
        },
        {
            "lambda (IIFE) parenthesized void", __LINE__,
            SVI("int x = 1;\n"
               "(void(int* p){ *p = 42; })(&x);\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "lambda (IIFE) parenthesized multi param", __LINE__,
            SVI("return (int(int a, int b){ return a * b + 2; })(8, 5);\n"),
            .exit_code = 42,
        },
        {
            "lambda (IIFE) parenthesized in expr", __LINE__,
            SVI("int x = 10 + (int(int a){ return a * 2; })(16);\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "lambda (IIFE) parenthesized nested", __LINE__,
            SVI("return (int(int x){ return x + 1; })((int(void){ return 41; })());\n"),
            .exit_code = 42,
        },
        {
            "lambda (IIFE) parenthesized no params", __LINE__,
            SVI("return (int(void){ return 42; })();\n"),
            .exit_code = 42,
        },
        {
            "lambda (IIFE) double parens", __LINE__,
            SVI("return ((int(void){ return 42; }))();\n"),
            .exit_code = 42,
        },
        {
            "auto decays lambda to function pointer", __LINE__,
            SVI("auto fp = int(int x){ return x + 1; };\n"
               "return fp(41);\n"),
            .exit_code = 42,
        },
        {
            "constexpr decays lambda to function pointer", __LINE__,
            SVI("constexpr fp = int(int x){ return x + 1; };\n"
               "return fp(41);\n"),
            .exit_code = 42,
        },
        {
            "auto decays array to pointer", __LINE__,
            SVI("int arr[3] = {10, 20, 30};\n"
               "auto p = arr;\n"
               "return p[1] + p[2];\n"),
            .exit_code = 50,
        },
        {
            "compare function pointers eq", __LINE__,
            SVI("auto fp = int(int x){ return x; };\n"
               "auto fp2 = fp;\n"
               "return fp == fp2;\n"),
            .exit_code = 1,
        },
        {
            "compare function pointers ne", __LINE__,
            SVI("auto fp1 = int(void){ return 1; };\n"
               "auto fp2 = int(void){ return 2; };\n"
               "return fp1 != fp2;\n"),
            .exit_code = 1,
        },
        {
            "function pointer == function", __LINE__,
            SVI("int g(int x){ return x; }\n"
               "auto fp = g;\n"
               "return fp == g;\n"),
            .exit_code = 1,
        },
        {
            "function pointer != null", __LINE__,
            SVI("auto fp = int(void){ return 1; };\n"
               "return fp != 0;\n"),
            .exit_code = 1,
        },
        // Atomics
        {
            "atomic: fetch_add", __LINE__,
            SVI("int x = 10;\n"
               "int old = __atomic_fetch_add(&x, 5, __ATOMIC_SEQ_CST);\n"
               "return old * 100 + x;\n"),
            .exit_code = 10 * 100 + 15,
        },
        {
            "atomic: fetch_sub", __LINE__,
            SVI("int x = 20;\n"
               "int old = __atomic_fetch_sub(&x, 7, __ATOMIC_SEQ_CST);\n"
               "return old * 100 + x;\n"),
            .exit_code = 20 * 100 + 13,
        },
        {
            "atomic: add_fetch", __LINE__,
            SVI("int x = 10;\n"
               "int now = __atomic_add_fetch(&x, 5, __ATOMIC_SEQ_CST);\n"
               "return now * 100 + x;\n"),
            .exit_code = 15 * 100 + 15,
        },
        {
            "atomic: sub_fetch", __LINE__,
            SVI("int x = 20;\n"
               "int now = __atomic_sub_fetch(&x, 7, __ATOMIC_SEQ_CST);\n"
               "return now * 100 + x;\n"),
            .exit_code = 13 * 100 + 13,
        },
        {
            "atomic: pointer add_fetch scales", __LINE__,
            SVI("int arr[4] = {1, 2, 3, 4};\n"
               "int *p = arr;\n"
               "int *now = __atomic_add_fetch(&p, 2, __ATOMIC_SEQ_CST);\n"
               "return now == arr + 2 && p == arr + 2 && *p == 3;\n"),
            .exit_code = 1,
        },
        {
            "atomic: load", __LINE__,
            SVI("int x = 42;\n"
               "return __atomic_load_n(&x, __ATOMIC_SEQ_CST);\n"),
            .exit_code = 42,
        },
        {
            "atomic: store", __LINE__,
            SVI("int x = 0;\n"
               "__atomic_store_n(&x, 99, __ATOMIC_SEQ_CST);\n"
               "return x;\n"),
            .exit_code = 99,
        },
        {
            "atomic: exchange", __LINE__,
            SVI("int x = 10;\n"
               "int old = __atomic_exchange_n(&x, 20, __ATOMIC_SEQ_CST);\n"
               "return old * 100 + x;\n"),
            .exit_code = 10 * 100 + 20,
        },
        {
            "atomic: compare_exchange success", __LINE__,
            SVI("int x = 10;\n"
               "int expected = 10;\n"
               "_Bool ok = __atomic_compare_exchange_n(&x, &expected, 20, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);\n"
               "return ok * 100 + x;\n"),
            .exit_code = 1 * 100 + 20,
        },
        {
            "atomic: compare_exchange failure", __LINE__,
            SVI("int x = 10;\n"
               "int expected = 99;\n"
               "_Bool ok = __atomic_compare_exchange_n(&x, &expected, 20, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);\n"
               "return ok * 1000 + expected * 10 + x;\n"),
            .exit_code = 0 * 1000 + 10 * 10 + 10,
        },
        {
            "atomic: fetch_add char", __LINE__,
            SVI("char x = 10;\n"
               "char old = __atomic_fetch_add(&x, 3, __ATOMIC_SEQ_CST);\n"
               "return old * 100 + x;\n"),
            .exit_code = 10 * 100 + 13,
        },
        {
            "atomic type: object load/store", __LINE__,
            SVI("_Atomic int x = 1;\n"
               "int y = x;\n"
               "x = 5;\n"
               "return y * 10 + x;\n"),
            .exit_code = 15,
        },
        {
            "atomic type: pointer dereference load/store", __LINE__,
            SVI("_Atomic int x = 3;\n"
               "_Atomic int *p = &x;\n"
               "*p = 7;\n"
               "return x * 10 + *p;\n"),
            .exit_code = 77,
        },
        {
            "atomic type: member and subscript load/store", __LINE__,
            SVI("struct S { _Atomic int x; } s = {1};\n"
               "_Atomic int arr[2] = {2, 3};\n"
               "s.x = 9;\n"
               "arr[1] = 4;\n"
               "return s.x * 10 + arr[1];\n"),
            .exit_code = 94,
        },
        {
            "atomic type: pre/post increment", __LINE__,
            SVI("_Atomic int x = 10;\n"
               "int a = x++;\n"
               "int b = ++x;\n"
               "return a * 100 + b * 10 + x;\n"),
            .exit_code = 10 * 100 + 12 * 10 + 12,
        },
        {
            "atomic type: compound assignment", __LINE__,
            SVI("_Atomic int x = 3;\n"
               "int a = (x += 4);\n"
               "int b = (x *= 2);\n"
               "int c = (x ^= 5);\n"
               "return a * 100 + b * 10 + c;\n"),
            .exit_code = 7 * 100 + 14 * 10 + 11,
        },
        {
            "atomic type: compound assignment matrix", __LINE__,
            SVI("_Atomic int x = 64;\n"
               "int ok = 1;\n"
               "ok = ok && ((x -= 4) == 60);\n"
               "ok = ok && ((x /= 3) == 20);\n"
               "ok = ok && ((x %= 7) == 6);\n"
               "ok = ok && ((x |= 8) == 14);\n"
               "ok = ok && ((x &= 11) == 10);\n"
               "ok = ok && ((x ^= 3) == 9);\n"
               "ok = ok && ((x <<= 2) == 36);\n"
               "ok = ok && ((x >>= 1) == 18);\n"
               "return ok && x == 18;\n"),
            .exit_code = 1,
        },
        {
            "atomic type: signed char compound assignment", __LINE__,
            SVI("_Atomic signed char x = -3;\n"
               "x += 1;\n"
               "return x + 10;\n"),
            .exit_code = 8,
        },
        {
            "atomic type: enum read-modify-write", __LINE__,
            SVI("enum E { A = 1, B = 2, C = 3 };\n"
               "_Atomic enum E x = A;\n"
               "x++;\n"
               "x += 1;\n"
               "return x;\n"),
            .exit_code = 3,
        },
        {
            "atomic type: bool load/store", __LINE__,
            SVI("_Atomic bool x = false;\n"
               "bool a = x;\n"
               "x = true;\n"
               "return a * 10 + x;\n"),
            .exit_code = 1,
        },
        {
            "atomic type: atomic pointer", __LINE__,
            SVI("int a = 1, b = 2;\n"
               "int * _Atomic p = &a;\n"
               "*p = 3;\n"
               "p = &b;\n"
               "*p = 4;\n"
               "return a * 10 + b;\n"),
            .exit_code = 34,
        },
        {
            "atomic type: exact-size aggregate load/store", __LINE__,
            SVI("struct S1 { char x; };\n"
               "struct S2 { short x; };\n"
               "struct S4 { int x; };\n"
               "struct S8 { long long x; };\n"
               "struct S16 { long long x, y; };\n"
               "_Static_assert(sizeof(struct S1) == 1, \"\");\n"
               "_Static_assert(sizeof(struct S2) == 2, \"\");\n"
               "_Static_assert(sizeof(struct S4) == 4, \"\");\n"
               "_Static_assert(sizeof(struct S8) == 8, \"\");\n"
               "_Static_assert(sizeof(struct S16) == 16, \"\");\n"
               "_Atomic(struct S1) a1 = {1};\n"
               "_Atomic(struct S2) a2 = {2};\n"
               "_Atomic(struct S4) a4 = {3};\n"
               "_Atomic(struct S8) a8 = {4};\n"
               "_Atomic(struct S16) a16 = {5, 6};\n"
               "struct S1 b1 = {7}; struct S2 b2 = {8}; struct S4 b4 = {9};\n"
               "struct S8 b8 = {10}; struct S16 b16 = {11, 12};\n"
               "a1 = b1; a2 = b2; a4 = b4; a8 = b8; a16 = b16;\n"
               "b1 = a1; b2 = a2; b4 = a4; b8 = a8; b16 = a16;\n"
               "return b1.x == 7 && b2.x == 8 && b4.x == 9 && b8.x == 10 && b16.x == 11 && b16.y == 12;\n"),
            .exit_code = 1,
        },
        {
            "atomic type: array element load/store", __LINE__,
            SVI("_Atomic int a[2] = {2, 3};\n"
               "a[0] = 4;\n"
               "return a[0] * 10 + a[1];\n"),
            .exit_code = 43,
        },
        {
            "atomic type: typedef", __LINE__,
            SVI("typedef _Atomic int AI;\n"
               "AI x = 6;\n"
               "x += 1;\n"
               "return x;\n"),
            .exit_code = 7,
        },
        {
            "atomic: non-seq_cst orders", __LINE__,
            SVI("int x = 1;\n"
               "__atomic_store_n(&x, 2, __ATOMIC_RELEASE);\n"
               "int a = __atomic_load_n(&x, __ATOMIC_ACQUIRE);\n"
               "int b = __atomic_fetch_add(&x, 3, __ATOMIC_RELAXED);\n"
               "return a * 100 + b * 10 + x;\n"),
            .exit_code = 2 * 100 + 2 * 10 + 5,
        },
        {
            "atomic: weak compare_exchange loop", __LINE__,
            SVI("int x = 5;\n"
               "int expected = 5;\n"
               "while(!__atomic_compare_exchange_n(&x, &expected, expected + 1, 1, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)){}\n"
               "return x;\n"),
            .exit_code = 6,
        },
        {
            "atomic: generic load/store", __LINE__,
            SVI("long long x = 7, in = 9, out = 0;\n"
               "__atomic_store(&x, &in, __ATOMIC_SEQ_CST);\n"
               "__atomic_load(&x, &out, __ATOMIC_SEQ_CST);\n"
               "return (int)out;\n"),
            .exit_code = 9,
        },
        {
            "atomic: generic exchange", __LINE__,
            SVI("int x = 3, val = 8, old = 0;\n"
               "__atomic_exchange(&x, &val, &old, __ATOMIC_SEQ_CST);\n"
               "return old * 10 + x;\n"),
            .exit_code = 38,
        },
        {
            "atomic: generic compare_exchange", __LINE__,
            SVI("int x = 3, expected = 3, desired = 4;\n"
               "_Bool ok = __atomic_compare_exchange(&x, &expected, &desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);\n"
               "return ok * 10 + x;\n"),
            .exit_code = 14,
        },
        {
            "atomic: fetch_and/or/xor", __LINE__,
            SVI("unsigned x = 0xff;\n"
               "unsigned a = __atomic_fetch_and(&x, 0x0f, __ATOMIC_SEQ_CST);\n"
               "unsigned b = __atomic_fetch_or(&x, 0x30, __ATOMIC_SEQ_CST);\n"
               "unsigned c = __atomic_fetch_xor(&x, 0xff, __ATOMIC_SEQ_CST);\n"
               "return a == 0xff && b == 0x0f && c == 0x3f && x == 0xc0;\n"),
            .exit_code = 1,
        },
        {
            "atomic: fences", __LINE__,
            SVI("int x = 1;\n"
               "__atomic_thread_fence(__ATOMIC_SEQ_CST);\n"
               "__atomic_signal_fence(__ATOMIC_ACQUIRE);\n"
               "x += 1;\n"
               "return x;\n"),
            .exit_code = 2,
        },
        {
            "atomic: 16-byte exchange and compare_exchange", __LINE__,
            SVI("struct S16 { long long x, y; };\n"
               "_Alignas(16) struct S16 obj = {1, 2};\n"
               "struct S16 val = {3, 4};\n"
               "struct S16 old;\n"
               "__atomic_exchange(&obj, &val, &old, __ATOMIC_SEQ_CST);\n"
               "int ok1 = old.x == 1 && old.y == 2 && obj.x == 3;\n"
               "struct S16 expected = {3, 4};\n"
               "struct S16 desired = {5, 6};\n"
               "_Bool ok2 = __atomic_compare_exchange(&obj, &expected, &desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);\n"
               "return ok1 * 10 + (ok2 && obj.x == 5 && obj.y == 6);\n"),
            .exit_code = 11,
        },
        {
            "atomic type: pointer increment and compound scale", __LINE__,
            SVI("int arr[4] = {1, 2, 3, 4};\n"
               "int * _Atomic p = arr;\n"
               "p++;\n"
               "int a = *p;\n"
               "p += 2;\n"
               "int b = *p;\n"
               "int c = *--p;\n"
               "return a * 100 + b * 10 + c;\n"),
            .exit_code = 2 * 100 + 4 * 10 + 3,
        },
        {
            "atomic type: compound assignment in function", __LINE__,
            SVI("int f(void){\n"
               "    _Atomic long long x = 100;\n"
               "    x *= 3;\n"
               "    x -= 50;\n"
               "    x %= 90;\n"
               "    return (int)x;\n"
               "}\n"
               "return f();\n"),
            .exit_code = (100 * 3 - 50) % 90,
        },
        {
            "atomic: Interlocked builtins", __LINE__,
            SVI("long long x = 5;\n"
               "long long old = _InterlockedCompareExchange64(&x, 9, 5);\n"
               "long long n = _InterlockedIncrement64(&x);\n"
               "return old == 5 && x == 10 && n == 10;\n"),
            .exit_code = 1,
        },
        {
            "atomic: builtins in statement context", __LINE__,
            SVI("int x = 1;\n"
               "__atomic_fetch_add(&x, 2, __ATOMIC_RELAXED);\n"
               "__atomic_exchange_n(&x, x + 1, __ATOMIC_SEQ_CST);\n"
               "return x;\n"),
            .exit_code = 4,
        },
        {
            "atomic type: volatile atomic", __LINE__,
            SVI("volatile _Atomic int x = 1;\n"
               "x = 2;\n"
               "return x;\n"),
            .exit_code = 2,
        },
        {
            "atomic type: function parameter and return", __LINE__,
            SVI("_Atomic int id(_Atomic int x){ return x; }\n"
               "return id(7);\n"),
            .exit_code = 7,
        },
        {
            "atomic type: generic lvalue conversion", __LINE__,
            SVI("_Atomic int x = 0;\n"
               "return _Generic(x, _Atomic int: 1, int: 2, default: 3);\n"),
            .exit_code = 2,
        },
        {
            "atomic type: type-name generic preserves atomic", __LINE__,
            SVI("return _Generic(_Atomic int, _Atomic int: 1, int: 2, default: 3);\n"),
            .exit_code = 1,
        },
        {
            "atomic type: sizeof alignof match unqualified", __LINE__,
            SVI("return sizeof(_Atomic int) == sizeof(int)\n"
               "    && _Alignof(_Atomic int) == _Alignof(int);\n"),
            .exit_code = 1,
        },
        {
            "atomic type: struct whole-object load", __LINE__,
            SVI("struct S { int x, y; };\n"
               "_Atomic(struct S) a = {1, 2};\n"
               "struct S b = a;\n"
               "return b.x * 10 + b.y;\n"),
            .exit_code = 12,
        },
        {
            "atomic type: struct whole-object store", __LINE__,
            SVI("struct S { int x, y; };\n"
               "_Atomic(struct S) a = {1, 2};\n"
               "struct S b = {3, 4};\n"
               "a = b;\n"
               "struct S c = a;\n"
               "return c.x * 10 + c.y;\n"),
            .exit_code = 34,
        },
        {
            "atomic type: 16-byte struct whole-object load/store", __LINE__,
            SVI("struct S { long long x, y; };\n"
               "_Atomic(struct S) a = {1, 2};\n"
               "struct S b = {3, 4};\n"
               "a = b;\n"
               "struct S c = a;\n"
               "return c.x * 10 + c.y;\n"),
            .exit_code = 34,
        },
        {
            "atomic type: nested atomic field inside non-atomic aggregate", __LINE__,
            SVI("struct S { _Atomic int x; int y; } s = {1, 2};\n"
               "s.x += 3;\n"
               "return s.x * 10 + s.y;\n"),
            .exit_code = 42,
        },
        {
            "atomic type: address escape through cast", __LINE__,
            SVI("_Atomic int x = 5;\n"
               "int *p = (int*)&x;\n"
               "*p = 6;\n"
               "return x;\n"),
            .exit_code = 6,
        },
        {
            "array truth tests use the decayed address", __LINE__,
            SVI("int a[1]={0}; static int b[8]={0};\n"
                "int yes=0; if(a) yes++; if(b) yes++;\n"
                "return yes==2&&!(!a)&&!(!b)&&(a&&b)&&(0||a)&&(1&&b);\n"),
            .exit_code = 1,
        },
        {
            "array loop conditions use the decayed address", __LINE__,
            SVI("int a[1]={0}; int n=0;\n"
                "while(a){n++; break;} for(;a;){n++; break;}\n"
                "do {n++; if(n==4) break;} while(a);\n"
                "return n==4;\n"),
            .exit_code = 1,
        },
        {
            "array truth tests preserve side effects and short circuit", __LINE__,
            SVI("struct S {int a[8];} s={}; int i=0,j=0;\n"
                "int a=!((i++,s.a)); int b=((i++,s.a)&&(j++,1));\n"
                "int c=((i++,s.a)||(j++,0)); if((i++,s.a)) i++;\n"
                "return a==0&&b==1&&c==1&&i==5&&j==1;\n"),
            .exit_code = 1,
        },
        {
            "array of returned aggregate decays in truth tests", __LINE__,
            SVI("struct S {int a[2];}; int n=0;\n"
                "struct S f(void){n++; return (struct S){{0,0}};}\n"
                "int a=!f().a; int b=f().a&&1; if(f().a) n++;\n"
                "return a==0&&b==1&&n==4;\n"),
            .exit_code = 1,
        },
        // Bitfields
        {
            "bitfield: 128-bit compound arithmetic", __LINE__,
            SVI("struct S {unsigned pad:7; unsigned __int128 n:100; unsigned tail:3;} s={5,3,6};\n"
                "unsigned __int128 high=(unsigned __int128)1<<80;\n"
                "unsigned __int128 r=(s.n+=high); s.n*=3; s.n/=3; s.n%=high; s.n-=2;\n"
                "return r==high+3&&s.n==1&&s.pad==5&&s.tail==6;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: 128-bit compound bitwise and shifts", __LINE__,
            SVI("struct S {unsigned pad:7; unsigned __int128 n:100; unsigned tail:3;} s={5,3,6};\n"
                "s.n<<=80; s.n|=7; s.n^=2; s.n&=~(unsigned __int128)4; s.n>>=80;\n"
                "return s.n==3&&s.pad==5&&s.tail==6;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: 128-bit compound truncation and side effects", __LINE__,
            SVI("struct S {unsigned pad:7; unsigned __int128 n:100; unsigned tail:3;} s={5,0,6};\n"
                "s.n=((unsigned __int128)1<<100)-1; int i=0,j=0;\n"
                "unsigned __int128 r=((i++,s.n)+=(j++,1));\n"
                "return r==0&&s.n==0&&s.pad==5&&s.tail==6&&i==1&&j==1;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: signed 128-bit compound arithmetic", __LINE__,
            SVI("struct S {unsigned pad:7; __int128 n:100; unsigned tail:3;} s={5,-30,6};\n"
                "s.n/=3; s.n%=7; s.n>>=1; __int128 r=(s.n+=1);\n"
                "return r==-1&&s.n==-1&&s.pad==5&&s.tail==6;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: full-width 128-bit compound wraparound", __LINE__,
            SVI("struct S {unsigned __int128 n:128;} s={~(unsigned __int128)0};\n"
                "unsigned __int128 r=(s.n+=1); s.n-=1;\n"
                "return r==0&&s.n==~(unsigned __int128)0;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: comma preserves promotions", __LINE__,
            SVI("struct S {unsigned a:3;} s={1}; int i=0;\n"
                "_Any boxed=+(i++,s.a);\n"
                "return !((i++,s.a)<-1)&&boxed.type==int&&boxed.as(int)==1&&i==2;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: comma assignment preserves adjacent fields", __LINE__,
            SVI("struct S {unsigned pad:3; unsigned a:3; unsigned tail:3;} s={5,1,6};\n"
                "int i=0; int r=((i++,(i++,s.a))=10);\n"
                "return r==2&&s.a==2&&s.pad==5&&s.tail==6&&i==2;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: comma compound assignment", __LINE__,
            SVI("struct S {unsigned pad:3; unsigned a:3;} s={5,3}; int i=0,j=0;\n"
                "int r=((i++,s.a)+=(j++,2)); int q=((i++,s.a)*=0.5);\n"
                "return r==5&&q==2&&s.a==2&&s.pad==5&&i==2&&j==1;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: comma increment through arrow", __LINE__,
            SVI("struct S {unsigned pad:3; signed a:3;} s={5,1}; struct S* p=&s; int i=0;\n"
                "int old=(i++,p->a)++; int next=++(i++,p->a);\n"
                "return old==1&&next==3&&s.a==3&&s.pad==5&&i==2;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: read", __LINE__,
            SVI("struct S { int a : 3; int b : 5; };\n"
               "struct S s = {3, 10};\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 3 * 100 + 10,
        },
        {
            "bitfield: write", __LINE__,
            SVI("struct S { unsigned a : 3; unsigned b : 5; };\n"
               "struct S s = {0};\n"
               "s.a = 5;\n"
               "s.b = 17;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 5 * 100 + 17,
        },
        {
            "bitfield: write preserves adjacent", __LINE__,
            SVI("struct S { unsigned a : 4; unsigned b : 4; };\n"
               "struct S s = {7, 0};\n"
               "s.b = 9;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 7 * 100 + 9,
        },
        {
            "bitfield: arrow read", __LINE__,
            SVI("struct S { int a : 3; int b : 5; };\n"
               "struct S s = {2, 15};\n"
               "struct S* p = &s;\n"
               "return p->a * 100 + p->b;\n"),
            .exit_code = 2 * 100 + 15,
        },
        {
            "bitfield: arrow write", __LINE__,
            SVI("struct S { unsigned a : 4; unsigned b : 4; };\n"
               "struct S s = {0};\n"
               "struct S* p = &s;\n"
               "p->a = 11;\n"
               "p->b = 6;\n"
               "return p->a * 100 + p->b;\n"),
            .exit_code = 11 * 100 + 6,
        },
        {
            "bitfield: preinc", __LINE__,
            SVI("struct S { unsigned a : 4; unsigned b : 4; };\n"
               "struct S s = {5, 10};\n"
               "int r = ++s.a;\n"
               "return r * 100 + s.a;\n"),
            .exit_code = 6 * 100 + 6,
        },
        {
            "bitfield: postinc", __LINE__,
            SVI("struct S { unsigned a : 4; unsigned b : 4; };\n"
               "struct S s = {5, 10};\n"
               "int r = s.a++;\n"
               "return r * 100 + s.a;\n"),
            .exit_code = 5 * 100 + 6,
        },
        {
            "bitfield: predec", __LINE__,
            SVI("struct S { unsigned a : 4; unsigned b : 4; };\n"
               "struct S s = {5, 10};\n"
               "int r = --s.b;\n"
               "return r * 100 + s.b;\n"),
            .exit_code = 9 * 100 + 9,
        },
        {
            "bitfield: postdec", __LINE__,
            SVI("struct S { unsigned a : 4; unsigned b : 4; };\n"
               "struct S s = {5, 10};\n"
               "int r = s.b--;\n"
               "return r * 100 + s.b;\n"),
            .exit_code = 10 * 100 + 9,
        },
        {
            "bitfield: inc preserves adjacent", __LINE__,
            SVI("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {7, 3};\n"
               "s.b++;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 7 * 100 + 4,
        },
        {
            "bitfield: addassign", __LINE__,
            SVI("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {2, 3};\n"
               "s.a += 5;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 7 * 100 + 3,
        },
        {
            "bitfield: subassign", __LINE__,
            SVI("struct S { unsigned a : 4; unsigned b : 4; };\n"
               "struct S s = {9, 3};\n"
               "s.a -= 4;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 5 * 100 + 3,
        },
        {
            "bitfield: orassign", __LINE__,
            SVI("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {5, 3};\n"
               "s.a |= 2;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 7 * 100 + 3,
        },
        {
            "bitfield: andassign", __LINE__,
            SVI("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {7, 3};\n"
               "s.a &= 5;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 5 * 100 + 3,
        },
        {
            "bitfield: arrow preinc", __LINE__,
            SVI("struct S { unsigned a : 4; unsigned b : 4; };\n"
               "struct S s = {5, 10};\n"
               "struct S* p = &s;\n"
               "int r = ++p->a;\n"
               "return r * 100 + p->b;\n"),
            .exit_code = 6 * 100 + 10,
        },
        {
            "bitfield: arrow addassign", __LINE__,
            SVI("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {2, 3};\n"
               "struct S* p = &s;\n"
               "p->a += 5;\n"
               "return p->a * 100 + p->b;\n"),
            .exit_code = 7 * 100 + 3,
        },
        {
            "bitfield: signed read -1", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "s.a = -1;\n"
               "return s.a + 100;\n"),
            .exit_code = 99,
        },
        {
            "bitfield: signed read min", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "s.a = -8;\n"
               "return s.a + 100;\n"),
            .exit_code = 92,
        },
        {
            "bitfield: signed read max", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "s.a = 7;\n"
               "return s.a + 100;\n"),
            .exit_code = 107,
        },
        {
            "bitfield: signed 1-bit", __LINE__,
            SVI("struct S { signed a : 1; };\n"
               "struct S s = {0};\n"
               "s.a = -1;\n"
               "return s.a + 100;\n"),
            .exit_code = 99,
        },
        {
            "bitfield: signed comparison", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "s.a = -3;\n"
               "return s.a < 0 ? 1 : 0;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: signed arithmetic", __LINE__,
            SVI("struct S { unsigned a : 4; signed b : 4; };\n"
               "struct S s = {5, -3};\n"
               "return s.a + s.b + 100;\n"),
            .exit_code = 102,
        },
        {
            "bitfield: signed preinc", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "s.a = -3;\n"
               "int r = ++s.a;\n"
               "return r + 100;\n"),
            .exit_code = 98,
        },
        {
            "bitfield: signed postinc", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "s.a = -3;\n"
               "int r = s.a++;\n"
               "return r + 100;\n"),
            .exit_code = 97,
        },
        {
            "bitfield: signed predec", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "s.a = 2;\n"
               "int r = --s.a;\n"
               "return r + 100;\n"),
            .exit_code = 101,
        },
        {
            "bitfield: signed addassign", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "s.a = -5;\n"
               "s.a += 2;\n"
               "return s.a + 100;\n"),
            .exit_code = 97,
        },
        {
            "bitfield: signed divassign", __LINE__,
            SVI("struct S { signed a : 8; };\n"
               "struct S s = {0};\n"
               "s.a = -7;\n"
               "s.a /= 3;\n"
               "return s.a + 100;\n"),
            .exit_code = 98,
        },
        {
            "bitfield: signed modassign", __LINE__,
            SVI("struct S { signed a : 8; };\n"
               "struct S s = {0};\n"
               "s.a = -7;\n"
               "s.a %= 3;\n"
               "return s.a + 100;\n"),
            .exit_code = 99,
        },
        {
            "bitfield: signed subassign", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "s.a = 3;\n"
               "s.a -= 5;\n"
               "return s.a + 100;\n"),
            .exit_code = 98,
        },
        {
            "bitfield: signed assign result", __LINE__,
            SVI("struct S { signed a : 4; };\n"
               "struct S s = {0};\n"
               "int r = (s.a = -3);\n"
               "return r + 100;\n"),
            .exit_code = 97,
        },
        {
            "big string", __LINE__,
            SVI("enum {SIZE = 1<<16};\n"
               "char buff[SIZE] = \"hello\";\n"
               "int sum = 0;\n"
               "for(__SIZE_TYPE__ i = 0; i < SIZE && buff[i]; i++)\n"
               "    sum += buff[i];\n"
               "return sum;\n"),
            .exit_code = 'h' + 'e' + 'l' + 'l' + 'o',
        },
        {
            "varargs", __LINE__,
            SVI(
               "#define va_start __builtin_va_start\n"
               "#define va_copy __builtin_va_copy\n"
               "#define va_arg __builtin_va_arg\n"
               "#define va_end __builtin_va_end\n"
               "typedef __builtin_va_list va_list;\n"
               "int vsum(int n, va_list ap){\n"
               "    int sum = 0;\n"
               "    for(int i = 0; i < n; i++){\n"
               "        sum += va_arg(ap, int);\n"
               "    }\n"
               "    return sum;\n"
               "}\n"
               "int sum(int n, ...){\n"
               "    va_list ap;\n"
               "    va_start(ap, n);\n"
               "    int result = vsum(n, ap);\n"
               "    va_end(ap);\n"
               "    return result;\n"
               "}\n"
               "int sum2(int n, ...){\n"
               "    va_list ap, ap2;\n"
               "    va_start(ap);\n" // c23
               "    va_copy(ap2, ap);\n"
               "    int result = vsum(n, ap);\n"
               "    result += vsum(n, ap2);\n"
               "    va_end(ap);\n"
               "    va_end(ap2);\n"
               "    return result;\n"
               "}\n"
               "int x = sum(3, 4, 5, 6) + sum2(2, 8, 9);\n"
               "return x;\n"
               ),
            .exit_code = 4+5+6+8+9+8+9,
        },
        // _Generic
        {
            "default args: omitted, explicit, named and positional designators", __LINE__,
            SVI("int f(int a, int b=2, int c=3){return a*100+b*10+c;}\n"
                "return f(1)==123 && f(1,4)==143 && f(1,4,5)==145\n"
                " && f(.a=1,.c=5)==125 && f([0]=1,[2]=5)==125;\n"),
            .exit_code = 1,
        },
        {
            "default args: unused defaults do not resolve dependencies", __LINE__,
            SVI("int missing(void); int f(int a=missing()){return a;}\nreturn f(7);\n"),
            .exit_code = 7,
        },
        {
            "default args: declaration binding and evaluation on every call", __LINE__,
            SVI("int n=0; int next(void){return ++n;}\n"
                "int f(int a=next()){return a;}\n"
                "int probe(void){int n=100; int a=f(); int b=f();\n"
                " return a==1 && b==2 && f(7)==7 && n==100;}\n"
                "return probe() && n==2;\n"),
            .exit_code = 1,
        },
        {
            "default args: merged declarations and definition names", __LINE__,
            SVI("int f(int, int=3); int f(int=2, int);\n"
                "int f(int a,int b){return a*10+b;}\n"
                "int f(int,int); int f();\n"
                "return f()==23 && f(.b=7)==27;\n"),
            .exit_code = 1,
        },
        {
            "default args: added after definition", __LINE__,
            SVI("int f(int a){return a;} int f(int a=7); return f();\n"),
            .exit_code = 7,
        },
        {
            "default args: lambdas and function pointer calls with explicit args", __LINE__,
            SVI("int f(int a=7){return a;} int (*p)(int)=f;\n"
                "return int(int a=3){return a;}()==3 && p(5)==5;\n"),
            .exit_code = 1,
        },
        {
            "default args: aggregate defaults own each call's expressions", __LINE__,
            SVI("int n=0; struct S {int a,b;};\n"
                "int f(struct S s={++n,2}){return s.a*10+s.b;}\n"
                "return f()==12 && f()==22 && n==2;\n"),
            .exit_code = 1,
        },
        {
            "default args: compound literal storage belongs to each caller", __LINE__,
            SVI("int n=0;\n"
                "int f(int* p=(int[]){++n}){return p[0];}\n"
                "int g(int* p=&(int){++n}){return *p;}\n"
                "int probe(void){int x=7; if(f(&x)!=7 || n!=0)return 0;\n"
                " int a=f(), b=f(), c=g(); return a==1 && b==2 && c==3 && n==3;}\n"
                "return probe();\n"),
            .exit_code = 1,
        },
        {
            "default args: compound literal addresses differ across call expressions", __LINE__,
            SVI("int n=0; int* make(int* p=(int[]){++n}){return p;}\n"
                "int probe(void){int* p=make(); int* q=make();\n"
                " return p!=q && *p==1 && *q==2 && n==2;}\nreturn probe();\n"),
            .exit_code = 1,
        },
        {
            "default args: nested defaults allocate storage in each caller", __LINE__,
            SVI("int n=0; int* inner(int* p=(int[]){++n}){return p;}\n"
                "int* middle(int* p=inner()){return p;}\n"
                "int* outer(int* p=middle()){return p;}\n"
                "int probe(void){int x=7; if(outer(&x)!=&x || n!=0)return 0;\n"
                " int* p=outer(); int* q=outer();\n"
                " return p!=q && *p==1 && *q==2 && n==2;} return probe();\n"),
            .exit_code = 1,
        },
        {
            "default args: nested typed packs allocate storage in each caller", __LINE__,
            SVI("int n=0; int* inner(int values..., int extra=0){values[0]+=extra;return values.data;}\n"
                "int* outer(int* p=inner(++n,.extra=10)){return p;}\n"
                "int probe(void){int* p=outer(); int* q=outer();\n"
                " return p!=q && *p==11 && *q==12 && n==2;} return probe();\n"),
            .exit_code = 1,
        },
        {
            "default args: nested storage preserves inner source locations", __LINE__,
            SVI("int* inner(int* p=(int[]){__builtin_LINE()}, const char* name=__builtin_FUNCTION()){return name[0]?0:p;}\n"
                "constexpr int expected=__LINE__; int* outer(int* p=inner()){return p;}\n"
                "int probe(void){int* p=outer(); int* q=outer();return p!=q && *p==expected && *q==expected;}\n"
                "return probe();\n"),
            .exit_code = 1,
        },
        {
            "default args: methods retain receiver and fill missing parameters", __LINE__,
            SVI("struct S {int x; int get(_Self* s,int add=2){return s.x+add;}};\n"
                "struct S s={5}; return s.get()==7 && s.get(.add=3)==8;\n"),
            .exit_code = 1,
        },
        {
            "default args: default array and function arguments decay", __LINE__,
            SVI("int a[2]={3,5}; int g(void){return 7;}\n"
                "int f(int p[]=a, int cb(void)=g){return p[1]+cb();}\n"
                "return f();\n"),
            .exit_code = 12,
        },
        {
            "default args: sizeof and generic operands are discarded", __LINE__,
            SVI("int f(int a=sizeof(({int x=1; x++; x;})),\n"
                " int b=_Generic(({1;}),int:7,default:({2;}))){return a+b;}\n"
                "return f();\n"),
            .exit_code = 11,
        },
        {
            "default args: typed and C varargs", __LINE__,
            SVI("int f(int a=7, int values...){return a+(int)values.count;}\n"
                "int g(int a=9,...){return a;}\n"
                "return f()==7 && f(.values=(int[]){1,2})==9 && g()==9;\n"),
            .exit_code = 1,
        },
        {
            "default args: call site source location", __LINE__,
            SVI("int same(const char* a,const char* b){while(*a && *a==*b){a++;b++;}return *a==*b;}\n"
                "int expected_line;\n"
                "int f(int line=__builtin_LINE()+1, const char* file=__builtin_FILE(),\n"
                " const char* function=__builtin_FUNCTION()){return line==expected_line+1\n"
                " && same(file,__FILE__) && function[0]=='p';}\n"
                "int probe(void){expected_line=__LINE__; return f();}\nreturn probe();\n"),
            .exit_code = 1,
        },
        {
            "default args: source location inside aggregate defaults and macros", __LINE__,
            SVI("#define LINE __builtin_LINE()\n#define CALL f()\n"
                "struct Loc {unsigned line;const char* function;}; int expected;\n"
                "int f(struct Loc loc={LINE,__builtin_FUNCTION()}){return loc.line==expected && loc.function[0]=='p';}\n"
                "int probe(void){expected=__LINE__;return CALL;}\nreturn probe();\n"),
            .exit_code = 1,
        },
        {
            "source builtins: character pointer types", __LINE__,
            SVI("_Static_assert(typeof(__builtin_FILE()) == const char*);\n"
                "_Static_assert(typeof(__builtin_FUNCTION()) == const char*);\n"
                "_Static_assert(sizeof(__builtin_FILE()) == sizeof(const char*));\n"
                "int probe(void){return __builtin_FUNCTION()[0] == 'p';}\n"
                "const char file[:] = __builtin_SRCLOC().file;\n"
                "return file.count == sizeof(__FILE__)-1 && probe();\n"),
            .exit_code = 1,
        },
        {
            "source builtins: boxed default preserves pointer type", __LINE__,
            SVI("int check(_Any name=__builtin_FUNCTION()){return name.type == const char*;}\n"
                "int comma(_Any name=(0,__builtin_FUNCTION())){return name.type == const char*;}\n"
                "int probe(void){return check() && comma();}\n"
                "return probe();\n"),
            .exit_code = 1,
        },
        {
            "source builtins: explicit calls use their own location", __LINE__,
            SVI("int same(const char* a,const char* b){while(*a && *a==*b){a++;b++;}return *a==*b;}\n"
                "constexpr int line=__builtin_LINE(); constexpr int expected=__LINE__;\n"
                "int probe(void){return __builtin_FUNCTION()[0]=='p';}\n"
                "return line==expected && same(__builtin_FILE(),__FILE__) && __builtin_COLUMN()>0 && probe();\n"),
            .exit_code = 1,
        },
        {
            "source builtins: SRCLOC returns a constexpr _SrcLoc", __LINE__,
            SVI("#if !__has_builtin(__builtin_SRCLOC)\n#error missing SRCLOC builtin\n#endif\n"
                "constexpr _SrcLoc loc=__builtin_SRCLOC(); constexpr int expected=__LINE__;\n"
                "_Static_assert(typeof(__builtin_SRCLOC())==_SrcLoc); _Static_assert(loc.line==expected);\n"
                "_Static_assert(loc.col==23);\n"
                "const char file[:]=loc.file; const char* expected_file=__FILE__;\n"
                "for(size_t i=0;i<file.count;i++)if(file[i]!=expected_file[i])return 0;\n"
                "return loc!=nullptr && expected_file[file.count]==0\n"
                " && __builtin_SRCLOC().line==__LINE__;\n"),
            .exit_code = 1,
        },
        {
            "source builtins: SRCLOC defaults use the caller and accept overrides", __LINE__,
            SVI("int expected; int f(_SrcLoc loc=__builtin_SRCLOC()){return loc.line==expected && loc.col>0 && loc.file.count>0;}\n"
                "int probe(void){expected=__LINE__;if(!f())return 0;\n"
                "expected=__LINE__;if(!f())return 0;\n"
                "_SrcLoc loc=__builtin_SRCLOC();expected=__LINE__;\n"
                "return f(loc) && f(.loc=loc);}\nreturn probe();\n"),
            .exit_code = 1,
        },
        {
            "source builtins: SRCLOC macros and aggregate defaults", __LINE__,
            SVI("#define LOC __builtin_SRCLOC()\n#define CALL f(1,2)\n"
                "struct S {_SrcLoc loc;};int expected;\n"
                "int f(int args...,struct S s={LOC}){return args.count==2 && s.loc.line==expected && s.loc.col==1;}\n"
                "int probe(void){expected=__LINE__+2;\nreturn\nCALL;}\nreturn probe();\n"),
            .exit_code = 1,
        },
        {
            "source builtins: nested SRCLOC defaults keep the inner call location", __LINE__,
            SVI("_SrcLoc inner(_SrcLoc loc=__builtin_SRCLOC()){return loc;}\n"
                "constexpr int expected=__LINE__;_SrcLoc outer(_SrcLoc loc=inner()){return loc;}\n"
                "return outer().line==expected;\n"),
            .exit_code = 1,
        },
        {
            "_Generic: constexpr selection with runtime controlling expression", __LINE__,
            SVI("int f(void){ int x=0;\n"
                "  constexpr int y=_Generic(x++, int: 7, default: x++);\n"
                "  return y==7 && x==0;\n"
                "}\nreturn f();\n"),
            .exit_code = 1,
        },
        {
            "_Generic: discarded calls do not resolve dependencies", __LINE__,
            SVI("int missing(void);\n"
                "constexpr int x=_Generic(missing(), default: missing(), int: 7);\n"
                "return x;\n"),
            .exit_code = 7,
        },
        {
            "_Generic: selected default executes once and preserves input", __LINE__,
            SVI("int x=0;\n"
                "int y=_Generic(1, default: (x++, 7), float: 8) + 2;\n"
                "return x==1 && y==9;\n"),
            .exit_code = 1,
        },
        {
            "_Generic: basic int", __LINE__,
            SVI("int x = 1;\n"
               "return _Generic(x, int: 10, float: 20, default: 30);\n"),
            .exit_code = 10,
        },
        {
            "_Generic: basic float", __LINE__,
            SVI("float x = 1.0f;\n"
               "return _Generic(x, int: 10, float: 20, default: 30);\n"),
            .exit_code = 20,
        },
        {
            "_Generic: default", __LINE__,
            SVI("double x = 1.0;\n"
               "return _Generic(x, int: 10, float: 20, default: 30);\n"),
            .exit_code = 30,
        },
        {
            "_Generic: lvalue conversion strips const", __LINE__,
            SVI("const int x = 1;\n"
               "return _Generic(x, int: 10, default: 20);\n"),
            .exit_code = 10,
        },
        {
            "_Generic: pointer type", __LINE__,
            SVI("int* p = 0;\n"
               "return _Generic(p, int*: 10, float*: 20, default: 30);\n"),
            .exit_code = 10,
        },
        {
            "_Generic: expression result used", __LINE__,
            SVI("int x = 5;\n"
               "int y = _Generic(x, int: x * 3, default: 0);\n"
               "return y;\n"),
            .exit_code = 15,
        },
        {
            "_Generic: default comes first", __LINE__,
            SVI("int x = 1;\n"
               "return _Generic(x, default: 0, int: 42);\n"),
            .exit_code = 42,
        },
        {
            "_Generic: type-name operand (C23)", __LINE__,
            SVI("return _Generic(int, int: 10, float: 20, default: 30);\n"),
            .exit_code = 10,
        },
        {
            "_Generic: type-name preserves const", __LINE__,
            SVI("return _Generic(const int, int: 10, const int: 20, default: 30);\n"),
            .exit_code = 20,
        },
        {
            "_Generic: nested in expression", __LINE__,
            SVI("int x = 1;\n"
               "return _Generic(x, int: 3, default: 0) + _Generic(x, int: 7, default: 0);\n"),
            .exit_code = 10,
        },
        // __builtin_add_overflow
        {
            "add_overflow: no overflow", __LINE__,
            SVI("int r;\n"
               "int ov = __builtin_add_overflow(3, 4, &r);\n"
               "return ov * 100 + r;\n"),
            .exit_code = 7,
        },
        {
            "add_overflow: signed overflow", __LINE__,
            SVI("int r;\n"
               "int ov = __builtin_add_overflow(2147483647, 1, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        {
            "add_overflow: unsigned no overflow", __LINE__,
            SVI("unsigned r;\n"
               "int ov = __builtin_add_overflow(3u, 4u, &r);\n"
               "return ov * 100 + r;\n"),
            .exit_code = 7,
        },
        {
            "add_overflow: unsigned overflow", __LINE__,
            SVI("unsigned char r;\n"
               "int ov = __builtin_add_overflow(200, 200, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        // __builtin_sub_overflow
        {
            "sub_overflow: no overflow", __LINE__,
            SVI("int r;\n"
               "int ov = __builtin_sub_overflow(10, 3, &r);\n"
               "return ov * 100 + r;\n"),
            .exit_code = 7,
        },
        {
            "sub_overflow: signed underflow", __LINE__,
            SVI("int r;\n"
               "int ov = __builtin_sub_overflow(-2147483647 - 1, 1, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        // __builtin_mul_overflow
        {
            "mul_overflow: no overflow", __LINE__,
            SVI("int r;\n"
               "int ov = __builtin_mul_overflow(6, 7, &r);\n"
               "return ov * 100 + r;\n"),
            .exit_code = 42,
        },
        {
            "mul_overflow: overflow", __LINE__,
            SVI("int r;\n"
               "int ov = __builtin_mul_overflow(2147483647, 2, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        {
            "add_overflow: negative into unsigned", __LINE__,
            SVI("unsigned r;\n"
               "int ov = __builtin_add_overflow(-1, 0, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        {
            "add_overflow: mixed types no overflow", __LINE__,
            SVI("long r;\n"
               "int ov = __builtin_add_overflow((short)100, 200u, &r);\n"
               "return ov * 1000 + (int)r;\n"),
            .exit_code = 300,
        },
        // __builtin_popcount
        {
            "popcount: zero", __LINE__,
            SVI("return __builtin_popcount(0);\n"),
            .exit_code = 0,
        },
        {
            "popcount: one", __LINE__,
            SVI("return __builtin_popcount(1);\n"),
            .exit_code = 1,
        },
        {
            "popcount: power of two", __LINE__,
            SVI("return __builtin_popcount(1024);\n"),
            .exit_code = 1,
        },
        {
            "popcount: all bits", __LINE__,
            SVI("return __builtin_popcount(0xFFu);\n"),
            .exit_code = 8,
        },
        {
            "popcount: mixed bits", __LINE__,
            SVI("return __builtin_popcount(0b10101010);\n"),
            .exit_code = 4,
        },
        {
            "popcountll", __LINE__,
            SVI("return __builtin_popcountll(0xFFFFFFFFull);\n"),
            .exit_code = 32,
        },
        // __builtin_ctz
        {
            "ctz: 1", __LINE__,
            SVI("return __builtin_ctz(1);\n"),
            .exit_code = 0,
        },
        {
            "ctz: power of two", __LINE__,
            SVI("return __builtin_ctz(8);\n"),
            .exit_code = 3,
        },
        {
            "ctz: trailing zeros", __LINE__,
            SVI("return __builtin_ctz(0x100);\n"),
            .exit_code = 8,
        },
        {
            "ctzll", __LINE__,
            SVI("return __builtin_ctzll(1ull << 32);\n"),
            .exit_code = 32,
        },
        // __builtin_clz
        {
            "clz: 1", __LINE__,
            SVI("return __builtin_clz(1);\n"),
            .exit_code = 31,
        },
        {
            "clz: high bit", __LINE__,
            SVI("return __builtin_clz(0x80000000u);\n"),
            .exit_code = 0,
        },
        {
            "clz: 16", __LINE__,
            SVI("return __builtin_clz(16);\n"),
            .exit_code = 27,
        },
        {
            "clzll", __LINE__,
            SVI("return __builtin_clzll(1ull << 32);\n"),
            .exit_code = 31,
        },
        {
            "typdef func forward decl", __LINE__,
            SVI("typedef int f(void);\n"
                "f fn;\n"
                "int x = fn();\n"
                "int fn(void){return 3;}\n"
                "return x;\n"),
            .exit_code = 3,
        },
        {
            "cpy", __LINE__,
            SVI("void cpy(void* d, void* s, __SIZE_TYPE__ sz){\n"
                "char *dst = d, *src = s;\n"
                "for(__SIZE_TYPE__ i = 0; i < sz; i++)\n"
                "   dst[i] = src[i];\n"
                "}\n"
                "int x = 4, y = 9;\n"
                "cpy(&x, &y, sizeof y);\n"
                "return x;\n"),
            .exit_code = 9,
        },
        {
            "assign to fla", __LINE__,
            SVI( "typedef struct FLA {int x; int vals[];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 8;\n"
                "fla->vals[0] = y;\n"
                "return fla->vals[0];\n"),
            .exit_code = 8,
        },
        {
            "assign to fake fla", __LINE__,
            SVI(
                "typedef struct FLA {int x; int vals[0];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 8;\n"
                "fla->vals[0] = y;\n"
                "return fla->vals[0];\n"),
            .exit_code = 8,
        },
        {
            "assign to really fake fla", __LINE__,
            SVI( "typedef struct FLA {int x; int vals[1];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 8;\n"
                "fla->vals[1] = y;\n"
                "return fla->vals[1];\n"),
            .exit_code = 8,
        },
        {
            "cpy fla", __LINE__,
            SVI("void cpy(void* d, void* s, __SIZE_TYPE__ sz){\n"
                "char *dst = d, *src = s;\n"
                "for(__SIZE_TYPE__ i = 0; i < sz; i++)\n"
                "   dst[i] = src[i];\n"
                "}\n"
                "typedef struct FLA {int x; int vals[];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 7;\n"
                "cpy(fla->vals, &y, sizeof y);\n"
                "return fla->vals[0];\n"),
            .exit_code = 7,
        },
        {
            "cpy fake fla", __LINE__,
            SVI("void cpy(void* d, void* s, __SIZE_TYPE__ sz){\n"
                "char *dst = d, *src = s;\n"
                "for(__SIZE_TYPE__ i = 0; i < sz; i++)\n"
                "   dst[i] = src[i];\n"
                "}\n"
                "typedef struct FLA {int x; int vals[0];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 7;\n"
                "cpy(fla->vals, &y, sizeof y);\n"
                "return fla->vals[0];\n"),
            .exit_code = 7,
        },
        {
            "struct with union copy", __LINE__,
            SVI("typedef struct {\n"
                "  union { unsigned long _bits; struct { unsigned type: 4; unsigned long _pad: 60; }; };\n"
                "  union { struct { const char* text; unsigned long len; }; };\n"
                "  unsigned long loc;\n"
                "} Tok;\n"
                "Tok arr[2] = {{._bits=1, .text=\"hi\", .len=2, .loc=10},\n"
                "              {._bits=2, .text=\"yo\", .len=2, .loc=20}};\n"
                "Tok t = arr[1];\n"
                "return (int)t.loc;\n"),
            .exit_code = 20,
        },
        {
            "struct copy from pointer subscript", __LINE__,
            SVI("typedef struct { long a; long b; long c; long d; } Big;\n"
                "Big g[2] = {{1,2,3,4},{5,6,7,8}};\n"
                "int f(const Big* p){\n"
                "  Big t = p[1];\n"
                "  return (int)(t.a + t.d);\n"
                "}\n"
                "return f(g);\n"),
            .exit_code = 13,
        },
        {
            "struct copy from array", __LINE__,
            SVI("typedef struct { long a; long b; long c; long d; } Big;\n"
                "Big arr[2] = {{1,2,3,4},{5,6,7,8}};\n"
                "Big t = arr[1];\n"
                "return (int)(t.a + t.d);\n"),
            .exit_code = 13,
        },
        {
            "struct with union alignment", __LINE__,
            SVI("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "} Frame;\n"
                "Frame f = {0};\n"
                "f.cursor = 10;\n"
                "f.cursor++;\n"
                "return (int)f.cursor;\n"),
            .exit_code = 11,
        },
        {
            "struct union cursor via pointer", __LINE__,
            SVI("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "} Frame;\n"
                "void advance(Frame* f){ f->cursor++; f->column++; }\n"
                "Frame f = {0};\n"
                "f.cursor = 10;\n"
                "advance(&f);\n"
                "advance(&f);\n"
                "return (int)f.cursor;\n"),
            .exit_code = 12,
        },
        {
            "tokenizer loop pattern", __LINE__,
            SVI("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "  const char* text;\n"
                "  __SIZE_TYPE__ length;\n"
                "} Frame;\n"
                "int next(Frame* f){\n"
                "  if(f->cursor == f->length) return -1;\n"
                "  int c = (int)(unsigned char)f->text[f->cursor++];\n"
                "  f->column++;\n"
                "  return c;\n"
                "}\n"
                "Frame f = {.text = \"hello\", .length = 5};\n"
                "int sum = 0;\n"
                "int c;\n"
                "while((c = next(&f)) != -1) sum += c;\n"
                "return sum == ('h'+'e'+'l'+'l'+'o');\n"),
            .exit_code = 1,
        },
        {
            "cppframe layout", __LINE__,
            SVI("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "  struct { __SIZE_TYPE__ length; const char* text; } txt;\n"
                "} Frame;\n"
                "Frame f = {0};\n"
                "f.txt.length = 99;\n"
                "f.txt.text = \"hello\";\n"
                "// Check that file_id doesn't alias txt.length\n"
                "f.file_id = 4;\n"
                "return (int)f.txt.length;\n"),
            .exit_code = 99,
        },
        {
            "switch negative case", __LINE__,
            SVI("int x = -1;\n"
                "switch(x){\n"
                "  case -1: return 1;\n"
                "  default: return 0;\n"
                "}\n"),
            .exit_code = 1,
        },
        {
            "struct union field write", __LINE__,
            SVI("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "} Frame;\n"
                "Frame f = {0};\n"
                "f.file_id = 99;\n"
                "f.line = 1;\n"
                "f.column = 5;\n"
                "f.cursor = 42;\n"
                "return (int)(f.file_id + f.line + f.column + f.cursor);\n"),
            .exit_code = 147,
        },
        {
            "struct copy in loop", __LINE__,
            SVI("typedef struct { long a; long b; long c; long d; } Big;\n"
                "Big arr[3] = {{1,2,3,4},{5,6,7,8},{9,10,11,12}};\n"
                "int sum = 0;\n"
                "for(int i = 0; i < 3; i++){\n"
                "  Big t = arr[i];\n"
                "  sum += (int)t.a;\n"
                "}\n"
                "return sum;\n"),
            .exit_code = 15,
        },
        {
            "varargs no extra args", __LINE__,
            SVI("int f(int x, ...){\n"
                "  __builtin_va_list va;\n"
                "  __builtin_va_start(va, x);\n"
                "  __builtin_va_end(va);\n"
                "  return x;\n"
                "}\n"
                "return f(42);\n"),
            .exit_code = 42,
        },
        {
            "msvc __va_start/__va_end", __LINE__,
            SVI("typedef __builtin_va_list va_list;\n"
                "void __va_start(va_list*, ...);\n"
                "void __va_end(va_list*);\n"
                "int sum(int n, ...){\n"
                "  va_list ap;\n"
                "  __va_start(&ap, n);\n"
                "  int s = 0;\n"
                "  for(int i = 0; i < n; i++)\n"
                "    s += __builtin_va_arg(ap, int);\n"
                "  __va_end(&ap);\n"
                "  return s;\n"
                "}\n"
                "return sum(3, 10, 20, 30);\n"),
            .exit_code = 60,
        },
        {
            "_InterlockedExchange", __LINE__,
            SVI("long _InterlockedExchange(long volatile*, long);\n"
                "long volatile x = 10;\n"
                "long old = _InterlockedExchange(&x, 42);\n"
                "return (int)(old + x);\n"),
            .exit_code = 10 + 42,
        },
        {
            "_InterlockedCompareExchange success", __LINE__,
            SVI("long _InterlockedCompareExchange(long volatile*, long, long);\n"
                "long volatile x = 10;\n"
                "long old = _InterlockedCompareExchange(&x, 42, 10);\n"
                "return (int)(old + x);\n"),
            .exit_code = 10 + 42,
        },
        {
            "_InterlockedCompareExchange failure", __LINE__,
            SVI("long _InterlockedCompareExchange(long volatile*, long, long);\n"
                "long volatile x = 10;\n"
                "long old = _InterlockedCompareExchange(&x, 42, 99);\n"
                "return (int)(old + x);\n"),
            .exit_code = 10 + 10,
        },
        {
            "_InterlockedCompareExchange evaluates exchange operand once", __LINE__,
            SVI("long _InterlockedCompareExchange(long volatile*, long, long);\n"
                "long volatile x = 10;\n"
                "int calls = 0;\n"
                "long old = _InterlockedCompareExchange(&x, (calls += 1, 42), 10);\n"
                "return calls * 1000 + (int)(old + x);\n"),
            .exit_code = 1 * 1000 + 10 + 42,
        },
        {
            "_InterlockedCompareExchange with implicit decay", __LINE__,
            SVI("long _InterlockedCompareExchange(long volatile*, long, long);\n"
                "long volatile x[1] = {10};\n"
                "int calls = 0;\n"
                "long old = _InterlockedCompareExchange(x, (calls += 1, 42), 10);\n"
                "return calls * 1000 + (int)(old + x[0]);\n"),
            .exit_code = 1 * 1000 + 10 + 42,
        },
        {
            "_InterlockedCompareExchange128 evaluates exchange operand once", __LINE__,
            SVI("unsigned char _InterlockedCompareExchange128(long long volatile*, long long, long long, long long*);\n"
                "_Alignas(16) long long volatile dst[2] = {1, 2};\n"
                "_Alignas(16) long long cmp[2] = {1, 2};\n"
                "int calls = 0;\n"
                "unsigned char ok = _InterlockedCompareExchange128(dst, (calls += 1, 8), 5, cmp);\n"
                "return calls * 10000 + (int)ok * 1000 + (int)dst[1] * 10 + (int)dst[0];\n"),
            .exit_code = 1 * 10000 + 1 * 1000 + 8 * 10 + 5,
        },
        {
            "_InterlockedExchange8", __LINE__,
            SVI("char _InterlockedExchange8(char volatile*, char);\n"
                "char volatile x = 5;\n"
                "char old = _InterlockedExchange8(&x, 7);\n"
                "return old + x;\n"),
            .exit_code = 5 + 7,
        },
        {
            "_InterlockedCompareExchange64", __LINE__,
            SVI("long long _InterlockedCompareExchange64(long long volatile*, long long, long long);\n"
                "long long volatile x = 100;\n"
                "long long old = _InterlockedCompareExchange64(&x, 200, 100);\n"
                "return (int)(old + x);\n"),
            .exit_code = 100 + 200,
        },
        {
            "_InterlockedIncrement", __LINE__,
            SVI("long _InterlockedIncrement(long volatile*);\n"
                "long volatile x = 10;\n"
                "long new_val = _InterlockedIncrement(&x);\n"
                "return (int)(new_val + x);\n"),
            .exit_code = 11 + 11,
        },
        {
            "_InterlockedDecrement", __LINE__,
            SVI("long _InterlockedDecrement(long volatile*);\n"
                "long volatile x = 10;\n"
                "long new_val = _InterlockedDecrement(&x);\n"
                "return (int)(new_val + x);\n"),
            .exit_code = 9 + 9,
        },
        {
            "_InterlockedExchangeAdd", __LINE__,
            SVI("long _InterlockedExchangeAdd(long volatile*, long);\n"
                "long volatile x = 10;\n"
                "long old = _InterlockedExchangeAdd(&x, 5);\n"
                "return (int)(old + x);\n"),
            .exit_code = 10 + 15,
        },
        {
            "_InterlockedAnd", __LINE__,
            SVI("long _InterlockedAnd(long volatile*, long);\n"
                "long volatile x = 0xFF;\n"
                "long old = _InterlockedAnd(&x, 0x0F);\n"
                "return (int)(old + x);\n"),
            .exit_code = 0xFF + 0x0F,
        },
        {
            "_InterlockedOr", __LINE__,
            SVI("long _InterlockedOr(long volatile*, long);\n"
                "long volatile x = 0xF0;\n"
                "long old = _InterlockedOr(&x, 0x0F);\n"
                "return (int)(old + x);\n"),
            .exit_code = 0xF0 + 0xFF,
        },
        {
            "_InterlockedXor", __LINE__,
            SVI("long _InterlockedXor(long volatile*, long);\n"
                "long volatile x = 0xFF;\n"
                "long old = _InterlockedXor(&x, 0x0F);\n"
                "return (int)(old + x);\n"),
            .exit_code = 0xFF + 0xF0,
        },
        {
            "_umul128", __LINE__,
            SVI("unsigned long long _umul128(unsigned long long, unsigned long long, unsigned long long*);\n"
                "unsigned long long hi;\n"
                "unsigned long long lo = _umul128(0x100000000ULL, 0x100000000ULL, &hi);\n"
                "return (int)(lo + hi);\n"),
            // 0x100000000 * 0x100000000 = 0x1_00000000_00000000, so lo=0, hi=1
            .exit_code = 0 + 1,
        },
        {
            "_umul128 no overflow", __LINE__,
            SVI("unsigned long long _umul128(unsigned long long, unsigned long long, unsigned long long*);\n"
                "unsigned long long hi;\n"
                "unsigned long long lo = _umul128(7, 9, &hi);\n"
                "return (int)(lo + hi);\n"),
            // 7*9=63, fits in low, hi=0
            .exit_code = 63,
        },
        {
            "cpy really fake fla", __LINE__,
            SVI("void cpy(void* d, void* s, __SIZE_TYPE__ sz){\n"
                "char *dst = d, *src = s;\n"
                "for(__SIZE_TYPE__ i = 0; i < sz; i++)\n"
                "   dst[i] = src[i];\n"
                "}\n"
                "typedef struct FLA {int x; int vals[1];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 7;\n"
                "cpy(fla->vals+1, &y, sizeof y);\n"
                "return fla->vals[1];\n"),
            .exit_code = 7,
        },
        {
            "negative subscript bitfield", __LINE__,
            SVI("typedef struct {\n"
                "  unsigned type: 4;\n"
                "  unsigned pad: 28;\n"
                "} Tok;\n"
                "Tok arr[3];\n"
                "arr[0].type = 1;\n"
                "arr[1].type = 2;\n"
                "arr[2].type = 3;\n"
                "Tok* end = arr + 3;\n"
                "return end[-1].type;\n"),
            .exit_code = 3,
        },
        {
            "negative subscript no bitfield", __LINE__,
            SVI("int arr[3] = {10, 20, 30};\n"
                "int* end = arr + 3;\n"
                "return end[-1];\n"),
            .exit_code = 30,
        },
        {
            "alloca basic", __LINE__,
            SVI("void* __builtin_alloca(__SIZE_TYPE__);\n"
                "void* p = __builtin_alloca(16);\n"
                "int* ip = (int*)p;\n"
                "ip[0] = 42;\n"
                "ip[1] = 58;\n"
                "return ip[0] + ip[1];\n"),
            .exit_code = 100,
        },
        {
            "alloca in loop", __LINE__,
            SVI("void* __builtin_alloca(__SIZE_TYPE__);\n"
                "int sum = 0;\n"
                "for(int i = 0; i < 5; i++){\n"
                "  int* p = (int*)__builtin_alloca(sizeof(int));\n"
                "  *p = i;\n"
                "  sum += *p;\n"
                "}\n"
                "return sum;\n"),
            .exit_code = 10,
        },
        {
            "struct return dot", __LINE__,
            SVI("typedef struct Foo Foo;\n"
               "struct Foo {struct { struct { struct { int x;} c;} b; } a;} ;\n"
               "Foo foo(){ return (Foo){3};}\n"
               "return foo().a.b.c.x;\n"),
            .exit_code = 3,
        },
        {
            "redecl after def keeps param names", __LINE__,
            SVI("static int add(int, int);\n"
               "static int add(int a, int b){ return a + b; }\n"
               "static int add(int, int);\n"
               "return add(17, 25);\n"),
            .exit_code = 42,
        },
        {
            "compound literal self-assign", __LINE__,
            SVI("typedef struct v2f v2f;\n"
               "struct v2f { float x; float y; };\n"
               "v2f v = {1.0f, 2.0f};\n"
               "v = (v2f){v.y, v.x};\n"
               "return (int)(v.x * 10 + v.y);\n"),
            .exit_code = 21,
        },
        {
            "array decay", __LINE__,
            SVI("typedef int I[1];\n"
               "I i;\n"
               "int* id(I i){ return i;}\n"
               "return i == id(i);\n"),
            .exit_code = 1,
        },
        // negative zero truthiness
        {
            "if neg zero double", __LINE__,
            SVI("double x = -0.0;\n"
               "if(x) return 1;\n"
               "return 0;\n"),
            .exit_code = 0,
        },
        {
            "if neg zero float", __LINE__,
            SVI("float x = -0.0f;\n"
               "if(x) return 1;\n"
               "return 0;\n"),
            .exit_code = 0,
        },
        {
            "while neg zero", __LINE__,
            SVI("double x = -0.0;\n"
               "int n = 0;\n"
               "while(x){ n++; break; }\n"
               "return n;\n"),
            .exit_code = 0,
        },
        {
            "lognot neg zero", __LINE__,
            SVI("double x = -0.0;\n"
               "return !x;\n"),
            .exit_code = 1,
        },
        {
            "logand neg zero", __LINE__,
            SVI("double x = -0.0;\n"
               "return x && 1;\n"),
            .exit_code = 0,
        },
        {
            "logor neg zero", __LINE__,
            SVI("double x = -0.0;\n"
               "return x || 0;\n"),
            .exit_code = 0,
        },
        {
            "ternary neg zero", __LINE__,
            SVI("double x = -0.0;\n"
               "return x ? 1 : 0;\n"),
            .exit_code = 0,
        },
        {
            "int128 literals full width", __LINE__,
            SVI("static unsigned __int128 max = 340282366920938463463374607431768211455ui128;\n"
                "volatile __int128 x = 18446744073709551617lll;\n"
                "volatile unsigned __int128 y = 0xffffffffffffffffffffffffffffffffULLL;\n"
                "return max == y && x == ((__int128)1 << 64) + 1 && -42i128 == -42;\n"),
            .exit_code = 1,
        },
        {
            "int128 signed div", __LINE__,
            SVI("__int128 a = -10;\n"
               "__int128 b = 3;\n"
               "return (int)(a / b);\n"),
            .exit_code = (int)(-10 / 3),
        },
        {
            "int128 signed mod", __LINE__,
            SVI("__int128 a = -10;\n"
               "__int128 b = 3;\n"
               "return (int)(a % b);\n"),
            .exit_code = (int)(-10 % 3),
        },
        {
            "int128 signed div both neg", __LINE__,
            SVI("__int128 a = -10;\n"
               "__int128 b = -3;\n"
               "return (int)(a / b);\n"),
            .exit_code = (int)(-10 / -3),
        },
        {
            "int128 signed mod both neg", __LINE__,
            SVI("__int128 a = -10;\n"
               "__int128 b = -3;\n"
               "return (int)(a % b);\n"),
            .exit_code = (int)(-10 % -3),
        },
        {
            "int128 signed lt neg", __LINE__,
            SVI("__int128 a = -1;\n"
               "__int128 b = 1;\n"
               "return a < b;\n"),
            .exit_code = 1,
        },
        {
            "int128 signed gt neg", __LINE__,
            SVI("__int128 a = -1;\n"
               "__int128 b = 1;\n"
               "return a > b;\n"),
            .exit_code = 0,
        },
        {
            "int128 signed le neg", __LINE__,
            SVI("__int128 a = -1;\n"
               "__int128 b = -1;\n"
               "return a <= b;\n"),
            .exit_code = 1,
        },
        {
            "int128 signed ge neg", __LINE__,
            SVI("__int128 a = 1;\n"
               "__int128 b = -1;\n"
               "return a >= b;\n"),
            .exit_code = 1,
        },
        {
            "int128 signed rshift", __LINE__,
            SVI("__int128 a = -4;\n"
               "return (int)(a >> 1);\n"),
            .exit_code = -2,
        },
        // Float: NaN
        {
            "float: nan != nan", __LINE__,
            SVI("double n = 0.0 / 0.0;\n"
               "return n != n;\n"),
            .exit_code = 1,
        },
        {
            "float: nan not equal", __LINE__,
            SVI("double n = 0.0 / 0.0;\n"
               "return !(n == n);\n"),
            .exit_code = 1,
        },
        {
            "float: nan not less", __LINE__,
            SVI("double n = 0.0 / 0.0;\n"
               "return !(n < 0.0) && !(n > 0.0) && !(n == 0.0);\n"),
            .exit_code = 1,
        },
        // Float: infinity
        {
            "float: positive inf", __LINE__,
            SVI("double inf = 1.0 / 0.0;\n"
               "return inf > 1000000.0;\n"),
            .exit_code = 1,
        },
        {
            "float: negative inf", __LINE__,
            SVI("double ninf = -1.0 / 0.0;\n"
               "return ninf < -1000000.0;\n"),
            .exit_code = 1,
        },
        {
            "float: inf + inf", __LINE__,
            SVI("double inf = 1.0 / 0.0;\n"
               "return inf + inf == inf;\n"),
            .exit_code = 1,
        },
        {
            "float: inf - inf is nan", __LINE__,
            SVI("double inf = 1.0 / 0.0;\n"
               "double r = inf - inf;\n"
               "return r != r;\n"),
            .exit_code = 1,
        },
        // Float: negative zero
        {
            "float: neg zero equals zero", __LINE__,
            SVI("double nz = -0.0;\n"
               "return nz == 0.0;\n"),
            .exit_code = 1,
        },
        {
            "float: 1/neg zero is neg inf", __LINE__,
            SVI("double nz = -0.0;\n"
               "double r = 1.0 / nz;\n"
               "return r < -1000000.0;\n"),
            .exit_code = 1,
        },
        // Shifts
        {
            "shift: by zero", __LINE__,
            SVI("return (42 << 0) + (42 >> 0) - 42;\n"),
            .exit_code = 42,
        },
        {
            "shift: 1u << 31", __LINE__,
            SVI("unsigned u = 1u << 31;\n"
               "return u == 0x80000000u;\n"),
            .exit_code = 1,
        },
        {
            "shift: signed right negative", __LINE__,
            SVI("int x = -8;\n"
               "return (x >> 1) + 100;\n"),
            .exit_code = 96,
        },
        {
            "shift: mixed width", __LINE__,
            SVI("unsigned char c = 0xFF;\n"
               "int r = c << 4;\n"
               "return r == 0xFF0;\n"),
            .exit_code = 1,
        },
        {
            "init: positional replacement after backwards designator", __LINE__,
            SVI("struct S {int a[2]; int b[2];};\nstruct S s={.b={1,2},.a={}, {3}};\nreturn s.a[0]==0 && s.a[1]==0 && s.b[0]==3 && s.b[1]==0;\n"),
            .exit_code = 1,
        },
        // Initialization: designated array
        {
            "init: nested lists need only the enclosing zero", __LINE__,
            SVI("int n=3;\n"
                "struct S {int a[2]; int b[2];};\n"
                "struct S s={{n},{n+1}};\n"
                "return s.a[0]==3 && s.a[1]==0 && s.b[0]==4 && s.b[1]==0;\n"),
            .exit_code = 1,
        },
        {
            "init: disjoint backwards designators need no extra zero", __LINE__,
            SVI("int n=3;\n"
                "struct S {int a[2]; int b[2];};\n"
                "struct S s={.b={n+1},.a={n}};\n"
                "return s.a[0]==3 && s.a[1]==0 && s.b[0]==4 && s.b[1]==0;\n"),
            .exit_code = 1,
        },
        {
            "init: partial replacement clears omitted elements", __LINE__,
            SVI("int n=3;\n"
                "struct S {int a[2]; int sentinel;};\n"
                "struct S s={.a={n,n+1},.sentinel=42,.a={7}};\n"
                "return s.a[0]==7 && s.a[1]==0 && s.sentinel==42;\n"),
            .exit_code = 1,
        },
        {
            "init: member write followed by enclosing empty list", __LINE__,
            SVI("struct S {int a[2]; int sentinel;};\n"
                "struct S s={.a[1]=7,.sentinel=42,.a={}};\n"
                "return s.a[0]==0 && s.a[1]==0 && s.sentinel==42;\n"),
            .exit_code = 1,
        },
        {
            "init: designated array", __LINE__,
            SVI("int a[5] = {[2] = 42, [4] = 99};\n"
               "return a[0] + a[1] + a[2] + a[3] + a[4] - 99;\n"),
            .exit_code = 42,
        },
        {
            "init: designated overwrite", __LINE__,
            SVI("int a[3] = {1, 2, [0] = 10};\n"
               "return a[0] + a[1];\n"),
            .exit_code = 12,
        },
        // Type system: anonymous struct/union
        {
            "anonymous union", __LINE__,
            SVI("struct S { int tag; union { int ival; double dval; }; };\n"
               "struct S s;\n"
               "s.tag = 1;\n"
               "s.ival = 42;\n"
               "return s.ival;\n"),
            .exit_code = 42,
        },
        {
            "anonymous struct", __LINE__,
            SVI("struct S { struct { int x; int y; }; int z; };\n"
               "struct S s = {{1, 2}, 3};\n"
               "return s.x + s.y + s.z;\n"),
            .exit_code = 6,
        },
        {
            "anon struct in union designated init", __LINE__,
            SVI("typedef unsigned uint32_t;\n"
               "typedef union Goals Goals;\n"
               "union Goals {\n"
               "    struct {\n"
               "        uint32_t a : 1, b : 1, c : 1, _reserved : 29;\n"
               "    };\n"
               "};\n"
               "Goals g = {.a = 1, .b = 1};\n"
               "return g.a + g.b + g.c;\n"),
            .exit_code = 2,
        },
        {
            "anon union in struct designated init", __LINE__,
            SVI("struct S { int tag; union { int ival; float fval; }; };\n"
               "struct S s = {.tag = 10, .ival = 32};\n"
               "return s.tag + s.ival;\n"),
            .exit_code = 42,
        },
        {
            "anon struct in union positional init", __LINE__,
            SVI("union U { struct { int x; int y; }; };\n"
               "union U u = {{5, 7}};\n"
               "return u.x + u.y;\n"),
            .exit_code = 12,
        },
        {
            "deeply nested anon: union>struct>union>struct", __LINE__,
            SVI("union Outer {\n"
               "    struct {\n"
               "        union {\n"
               "            struct {\n"
               "                int a;\n"
               "                int b;\n"
               "            };\n"
               "            long long raw;\n"
               "        };\n"
               "        int c;\n"
               "    };\n"
               "};\n"
               "union Outer o = {.a = 10, .b = 20, .c = 12};\n"
               "return o.a + o.b + o.c;\n"),
            .exit_code = 42,
        },
        {
            "deeply nested anon: struct>union>struct>union", __LINE__,
            SVI("struct Outer {\n"
               "    int tag;\n"
               "    union {\n"
               "        struct {\n"
               "            union {\n"
               "                int x;\n"
               "                float fx;\n"
               "            };\n"
               "            int y;\n"
               "        };\n"
               "        long long raw;\n"
               "    };\n"
               "};\n"
               "struct Outer o = {.tag = 2, .x = 30, .y = 10};\n"
               "return o.tag + o.x + o.y;\n"),
            .exit_code = 42,
        },
        {
            "anon struct in union: three fields", __LINE__,
            SVI("union V {\n"
               "    struct { int a; int b; int c; };\n"
               "    long long arr[2];\n"
               "};\n"
               "union V v = {.a = 10, .b = 12, .c = 20};\n"
               "return v.a + v.b + v.c;\n"),
            .exit_code = 42,
        },
        {
            "anon struct in union: trailing comma", __LINE__,
            SVI("union W { struct { int x; int y; }; };\n"
               "union W w = {.x = 40, .y = 2,};\n"
               "return w.x + w.y;\n"),
            .exit_code = 42,
        },
        {
            "anon struct in union: single field desig", __LINE__,
            SVI("union X { struct { int a; int b; }; int raw; };\n"
               "union X x = {.b = 42};\n"
               "return x.b;\n"),
            .exit_code = 42,
        },
        // Type system: _Static_assert
        {
            "_Static_assert", __LINE__,
            SVI("_Static_assert(sizeof(int) == 4, \"int must be 4 bytes\");\n"
               "_Static_assert(1, \"true\");\n"
               "return 42;\n"),
            .exit_code = 42,
        },
        // Type system: typedef chain
        {
            "typedef chain", __LINE__,
            SVI("typedef int myint;\n"
               "typedef myint myint2;\n"
               "typedef myint2 *myint2_ptr;\n"
               "myint2 v = 42;\n"
               "myint2_ptr p = &v;\n"
               "return *p;\n"),
            .exit_code = 42,
        },
        // Type system: array of function pointers
        {
            "array of function pointers", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"
               "int mul(int a, int b){ return a * b; }\n"
               "int (*ops[2])(int, int) = {add, mul};\n"
               "return ops[0](3, 4) + ops[1](3, 4);\n"),
            .exit_code = 19,
        },
        // Control flow: switch default in middle
        {
            "switch: default in middle", __LINE__,
            SVI("int x = 99;\n"
               "int r = 0;\n"
               "switch(x){\n"
               "  case 1: r = 1; break;\n"
               "  default: r = 42; break;\n"
               "  case 2: r = 2; break;\n"
               "}\n"
               "return r;\n"),
            .exit_code = 42,
        },
        // Control flow: nested continue
        {
            "nested continue", __LINE__,
            SVI("int count = 0;\n"
               "for(int i = 0; i < 5; i++){\n"
               "  for(int j = 0; j < 5; j++){\n"
               "    if(j == 2) continue;\n"
               "    if(j == 4) break;\n"
               "    count++;\n"
               "  }\n"
               "}\n"
               "return count;\n"),
            .exit_code = 15,
        },
        // Pointer: negative index
        {
            "pointer: negative index", __LINE__,
            SVI("int arr[5] = {10, 20, 30, 40, 50};\n"
               "int *p = arr + 3;\n"
               "return p[-2];\n"),
            .exit_code = 20,
        },
        {
            "pointer: large unsigned index", __LINE__,
            SVI("char buf[4] = {0};\n"
               "char *p = buf;\n"
               "unsigned idx = 0x80000000u;\n"
               "char *q = p + idx;\n"
               "return q > p;\n"),
            .exit_code = 1,
        },
        // Linked list
        {
            "linked list", __LINE__,
            SVI("typedef struct Node Node;\n"
               "struct Node { int val; Node *next; };\n"
               "Node c = {3, 0};\n"
               "Node b = {2, &c};\n"
               "Node a = {1, &b};\n"
               "int sum = 0;\n"
               "for(Node *p = &a; p; p = p->next) sum += p->val;\n"
               "return sum;\n"),
            .exit_code = 6,
        },
        {
            "int128 index", __LINE__,
            SVI("signed __int128 i = -1;\n"
               "int arr[3] = {1,2,3};\n"
               "int* p = arr + 2;\n"
               "return p[i];\n"),
            .exit_code = 2,
        },
        {
            "int128 ptr-math", __LINE__,
            SVI("signed __int128 i = 2;\n"
               "int arr[3] = {1,2,3};\n"
               "int* p = arr + i;\n"
               "return p[0];\n"),
            .exit_code = 3,
        },
        {
            "int128 neg", __LINE__,
            SVI("signed __int128 i = -1;\n"
               "return (int)-i;\n"),
            .exit_code = 1,
        },
        {
            "int128 not", __LINE__,
            SVI("signed __int128 i = -1;\n"
               "return (int)~i;\n"),
            .exit_code = 0,
        },
        {
            "int128 lognot", __LINE__,
            SVI("signed __int128 i = -1;\n"
               "return !i;\n"),
            .exit_code = 0,
        },
        {
            "int128 assign", __LINE__,
            SVI("signed __int128 i = 3;\n"
               "signed __int128 i2; i2 = i;\n"
               "return (int)i2;\n"),
            .exit_code = 3,
        },
        {
            "int128 compassign", __LINE__,
            SVI("signed __int128 i = 3;\n"
               "signed __int128 i2 = 1; i2 += i;\n"
               "return (int)i2;\n"),
            .exit_code = 4,
        },
        {
            "int128 post-inc", __LINE__,
            SVI("signed __int128 i = 3;\n"
               "return (int)i++;\n"),
            .exit_code = 3,
        },
        {
            "int128 pre-inc", __LINE__,
            SVI("signed __int128 i = 3;\n"
               "return (int)++i;\n"),
            .exit_code = 4,
        },
        {
            "int128 post-dec", __LINE__,
            SVI("signed __int128 i = 3;\n"
               "return (int)i--;\n"),
            .exit_code = 3,
        },
        {
            "int128 pre-dec", __LINE__,
            SVI("signed __int128 i = 3;\n"
               "return (int)--i;\n"),
            .exit_code = 2,
        },
        {
            "int128 ptr-sub", __LINE__,
            SVI("signed __int128 i = 1;\n"
               "int arr[3] = {1,2,3};\n"
               "int* p = arr + 2;\n"
               "p = p - i;\n"
               "return *p;\n"),
            .exit_code = 2,
        },
        {
            "int128 lognot zero", __LINE__,
            SVI("signed __int128 i = 0;\n"
               "return !i;\n"),
            .exit_code = 1,
        },
        {
            "float: preinc", __LINE__,
            SVI("float f = 2.5f;\n"
               "return (int)++f;\n"),
            .exit_code = 3,
        },
        {
            "float: postinc", __LINE__,
            SVI("float f = 2.5f;\n"
               "int old = (int)f++;\n"
               "return old * 10 + (int)f;\n"),
            .exit_code = 23,
        },
        {
            "float: predec", __LINE__,
            SVI("float f = 3.5f;\n"
               "return (int)--f;\n"),
            .exit_code = 2,
        },
        {
            "float: postdec", __LINE__,
            SVI("float f = 3.5f;\n"
               "int old = (int)f--;\n"
               "return old * 10 + (int)f;\n"),
            .exit_code = 32,
        },
        {
            "double: preinc", __LINE__,
            SVI("double d = 9.0;\n"
               "return (int)++d;\n"),
            .exit_code = 10,
        },
        {
            "double: postdec", __LINE__,
            SVI("double d = 5.0;\n"
               "int old = (int)d--;\n"
               "return old * 10 + (int)d;\n"),
            .exit_code = 54,
        },
        {
            "pointer: preinc", __LINE__,
            SVI("int arr[3] = {10, 20, 30};\n"
               "int *p = arr;\n"
               "return *++p;\n"),
            .exit_code = 20,
        },
        {
            "pointer: postinc", __LINE__,
            SVI("int arr[3] = {10, 20, 30};\n"
               "int *p = arr;\n"
               "int val = *p++;\n"
               "return val * 10 + *p;\n"),
            .exit_code = 120,
        },
        {
            "pointer: predec", __LINE__,
            SVI("int arr[3] = {10, 20, 30};\n"
               "int *p = arr + 2;\n"
               "return *--p;\n"),
            .exit_code = 20,
        },
        {
            "pointer: postdec", __LINE__,
            SVI("int arr[3] = {10, 20, 30};\n"
               "int *p = arr + 2;\n"
               "int val = *p--;\n"
               "return val * 10 + *p;\n"),
            .exit_code = 320,
        },
        {
            "float: addassign", __LINE__,
            SVI("float f = 2.5f;\n"
               "f += 1.5f;\n"
               "return (int)f;\n"),
            .exit_code = 4,
        },
        {
            "float: subassign", __LINE__,
            SVI("float f = 10.0f;\n"
               "f -= 3.0f;\n"
               "return (int)f;\n"),
            .exit_code = 7,
        },
        {
            "float: mulassign", __LINE__,
            SVI("float f = 3.0f;\n"
               "f *= 4.0f;\n"
               "return (int)f;\n"),
            .exit_code = 12,
        },
        {
            "float: divassign", __LINE__,
            SVI("double d = 21.0;\n"
               "d /= 3.0;\n"
               "return (int)d;\n"),
            .exit_code = 7,
        },
        {
            "int128: mulassign", __LINE__,
            SVI("unsigned __int128 a = 6;\n"
               "a *= 7;\n"
               "return (int)a;\n"),
            .exit_code = 42,
        },
        {
            "int128: divassign unsigned", __LINE__,
            SVI("unsigned __int128 a = 42;\n"
               "a /= 6;\n"
               "return (int)a;\n"),
            .exit_code = 7,
        },
        {
            "int128: modassign unsigned", __LINE__,
            SVI("unsigned __int128 a = 17;\n"
               "a %= 5;\n"
               "return (int)a;\n"),
            .exit_code = 2,
        },
        {
            "int128: andassign", __LINE__,
            SVI("unsigned __int128 a = 0xFF;\n"
               "a &= 0x0F;\n"
               "return (int)a;\n"),
            .exit_code = 15,
        },
        {
            "int128: orassign", __LINE__,
            SVI("unsigned __int128 a = 0xF0;\n"
               "a |= 0x0F;\n"
               "return (int)a;\n"),
            .exit_code = 255,
        },
        {
            "int128: xorassign", __LINE__,
            SVI("unsigned __int128 a = 0xFF;\n"
               "a ^= 0x0F;\n"
               "return (int)a;\n"),
            .exit_code = 240,
        },
        {
            "int128: lshiftassign", __LINE__,
            SVI("unsigned __int128 a = 1;\n"
               "a <<= 4;\n"
               "return (int)a;\n"),
            .exit_code = 16,
        },
        {
            "int128: rshiftassign unsigned", __LINE__,
            SVI("unsigned __int128 a = 256;\n"
               "a >>= 4;\n"
               "return (int)a;\n"),
            .exit_code = 16,
        },
        {
            "int128: signed divassign", __LINE__,
            SVI("signed __int128 a = -42;\n"
               "a /= 6;\n"
               "return (int)a + 100;\n"),
            .exit_code = 93,
        },
        {
            "int128: signed modassign", __LINE__,
            SVI("signed __int128 a = -7;\n"
               "a %= 3;\n"
               "return (int)a + 100;\n"),
            .exit_code = 99,
        },
        {
            "int128: signed rshiftassign", __LINE__,
            SVI("signed __int128 a = -8;\n"
               "a >>= 1;\n"
               "return (int)a + 100;\n"),
            .exit_code = 96,
        },
        {
            "int128: unsigned mul", __LINE__,
            SVI("unsigned __int128 a = 1000;\n"
               "unsigned __int128 b = 1000;\n"
               "return (int)(a * b / 10000);\n"),
            .exit_code = 100,
        },
        {
            "int128: unsigned div", __LINE__,
            SVI("unsigned __int128 a = 100;\n"
               "unsigned __int128 b = 7;\n"
               "return (int)(a / b);\n"),
            .exit_code = 14,
        },
        {
            "int128: unsigned mod", __LINE__,
            SVI("unsigned __int128 a = 100;\n"
               "unsigned __int128 b = 7;\n"
               "return (int)(a % b);\n"),
            .exit_code = 2,
        },
        {
            "int128: unsigned and", __LINE__,
            SVI("unsigned __int128 a = 0xFF;\n"
               "unsigned __int128 b = 0x0F;\n"
               "return (int)(a & b);\n"),
            .exit_code = 15,
        },
        {
            "int128: unsigned or", __LINE__,
            SVI("unsigned __int128 a = 0xF0;\n"
               "unsigned __int128 b = 0x0F;\n"
               "return (int)(a | b);\n"),
            .exit_code = 255,
        },
        {
            "int128: unsigned xor", __LINE__,
            SVI("unsigned __int128 a = 0xFF;\n"
               "unsigned __int128 b = 0x0F;\n"
               "return (int)(a ^ b);\n"),
            .exit_code = 240,
        },
        {
            "int128: unsigned shl", __LINE__,
            SVI("unsigned __int128 a = 1;\n"
               "return (int)(a << 8);\n"),
            .exit_code = 256,
        },
        {
            "int128: unsigned shr", __LINE__,
            SVI("unsigned __int128 a = 256;\n"
               "return (int)(a >> 4);\n"),
            .exit_code = 16,
        },
        {
            "int128: high bits survive shifts", __LINE__,
            SVI("unsigned __int128 a = 1;\n"
               "a <<= 100;\n"
               "a >>= 96;\n"
               "return (int)a;\n"),
            .exit_code = 16,
        },
        {
            "int128: mul carries into high half", __LINE__,
            SVI("unsigned __int128 a = (unsigned __int128)1 << 40;\n"
               "unsigned __int128 b = (unsigned __int128)1 << 40;\n"
               "return (int)((a * b) >> 76);\n"),
            .exit_code = 16,
        },
        {
            "int128: cmp decided by high half", __LINE__,
            SVI("unsigned __int128 a = (unsigned __int128)1 << 64;\n"
               "unsigned __int128 b = 2;\n"
               "return (a > b) + (b < a) + (a >= b) + (b != a);\n"),
            .exit_code = 4,
        },
        {
            "int128: shift by int-typed count", __LINE__,
            SVI("unsigned __int128 a = 1;\n"
               "int s = 100;\n"
               "return (int)((a << s) >> 98);\n"),
            .exit_code = 4,
        },
        {
            "int128: add carries into high half", __LINE__,
            SVI("unsigned __int128 a = ~(unsigned __int128)0 >> 64;\n" // 2**64-1
               "a += 1;\n"
               "return (int)(a >> 64);\n"),
            .exit_code = 1,
        },
        {
            "int128: sub borrows from high half", __LINE__,
            SVI("unsigned __int128 a = (unsigned __int128)1 << 64;\n"
               "unsigned __int128 b = 1;\n"
               "return (int)((a - b) >> 60) == 15;\n"),
            .exit_code = 1,
        },
        {
            "int128: cast sign extends into high half", __LINE__,
            SVI("int x = -5;\n"
               "signed __int128 a = (signed __int128)x;\n"
               "return (int)(a >> 64) + 100;\n"),
            .exit_code = 99,
        },
        {
            "int128: cast zero extends into high half", __LINE__,
            SVI("int x = -5;\n"
               "unsigned __int128 a = (unsigned __int128)(unsigned)x;\n"
               "return (int)(a >> 64);\n"),
            .exit_code = 0,
        },
        {
            "int128: cast truncation drops high half", __LINE__,
            SVI("unsigned __int128 a = ((unsigned __int128)1 << 64) + 42;\n"
               "return (int)a;\n"),
            .exit_code = 42,
        },
        {
            "int128: cast between 128-bit signedness", __LINE__,
            SVI("signed __int128 a = -1;\n"
               "unsigned __int128 b = (unsigned __int128)a;\n"
               "return (int)(b >> 100) == (1 << 28) - 1;\n"),
            .exit_code = 1,
        },
        {
            "int128: unsigned to double uses high half", __LINE__,
            SVI("unsigned __int128 a = (unsigned __int128)3 << 64;\n"
               "double d = (double)a;\n"
               "return d == 55340232221128654848.0;\n"),
            .exit_code = 1,
        },
        {
            "int128: signed to double preserves sign", __LINE__,
            SVI("signed __int128 a = -((signed __int128)5 << 64);\n"
               "double d = (double)a;\n"
               "return d == -92233720368547758080.0;\n"),
            .exit_code = 1,
        },
        {
            "int128: float round trip uses high half", __LINE__,
            SVI("unsigned __int128 a = (unsigned __int128)4 << 64;\n"
               "float f = (float)a;\n"
               "unsigned __int128 b = (unsigned __int128)f;\n"
               "return (unsigned long long)(b >> 64) == 4 && (unsigned long long)b == 0;\n"),
            .exit_code = 1,
        },
        {
            "int128: double to unsigned uses high half", __LINE__,
            SVI("double d = 129127208515966861312.0;\n"
               "unsigned __int128 a = (unsigned __int128)d;\n"
               "return (unsigned long long)(a >> 64) == 7 && (unsigned long long)a == 0;\n"),
            .exit_code = 1,
        },
        {
            "int128: double to signed truncates and preserves sign", __LINE__,
            SVI("double d = -36893488147419103232.0;\n"
               "signed __int128 a = (signed __int128)d;\n"
               "return a == -((signed __int128)2 << 64);\n"),
            .exit_code = 1,
        },
        {
            "int128: unsigned eq", __LINE__,
            SVI("unsigned __int128 a = 42;\n"
               "unsigned __int128 b = 42;\n"
               "return a == b;\n"),
            .exit_code = 1,
        },
        {
            "int128: unsigned ne", __LINE__,
            SVI("unsigned __int128 a = 42;\n"
               "unsigned __int128 b = 43;\n"
               "return a != b;\n"),
            .exit_code = 1,
        },
        {
            "int128: unsigned lt", __LINE__,
            SVI("unsigned __int128 a = 10;\n"
               "unsigned __int128 b = 20;\n"
               "return a < b;\n"),
            .exit_code = 1,
        },
        {
            "int128: unsigned gt", __LINE__,
            SVI("unsigned __int128 a = 20;\n"
               "unsigned __int128 b = 10;\n"
               "return a > b;\n"),
            .exit_code = 1,
        },
        {
            "int128: unsigned le", __LINE__,
            SVI("unsigned __int128 a = 10;\n"
               "unsigned __int128 b = 10;\n"
               "return a <= b;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_integer", __LINE__,
            SVI("return (int).is_integer;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_float", __LINE__,
            SVI("return (float).is_float;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_float int false", __LINE__,
            SVI("return (int).is_float;\n"),
            .exit_code = 0,
        },
        {
            "type introspection: is_pointer", __LINE__,
            SVI("return (int*).is_pointer;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_pointer int false", __LINE__,
            SVI("return (int).is_pointer;\n"),
            .exit_code = 0,
        },
        {
            "type introspection: is_struct", __LINE__,
            SVI("struct S { int x; };\n"
               "return (struct S).is_struct;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_union", __LINE__,
            SVI("union U { int x; float f; };\n"
               "return (union U).is_union;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_array", __LINE__,
            SVI("return (int[3]).is_array;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_slice", __LINE__,
            SVI("return (int[:]).is_slice;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_enum", __LINE__,
            SVI("enum E { A, B };\n"
               "return (enum E).is_enum;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_arithmetic int", __LINE__,
            SVI("return (int).is_arithmetic;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_arithmetic float", __LINE__,
            SVI("return (double).is_arithmetic;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_arithmetic enum", __LINE__,
            SVI("enum E { A };\n"
               "return (enum E).is_arithmetic;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_arithmetic ptr false", __LINE__,
            SVI("return (int*).is_arithmetic;\n"),
            .exit_code = 0,
        },
        {
            "type introspection: sizeof", __LINE__,
            SVI("return (int)(int).sizeof_;\n"),
            .exit_code = 4,
        },
        {
            "type introspection: alignof", __LINE__,
            SVI("return (int)(int).alignof_;\n"),
            .exit_code = 4,
        },
        {
            "type introspection: is_signed", __LINE__,
            SVI("return (int).is_signed;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_unsigned", __LINE__,
            SVI("return (unsigned).is_unsigned;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_const", __LINE__,
            SVI("return (const int).is_const;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_const false", __LINE__,
            SVI("return (int).is_const;\n"),
            .exit_code = 0,
        },
        {
            "type introspection: is_volatile", __LINE__,
            SVI("return (volatile int).is_volatile;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_function", __LINE__,
            SVI("return (int(int)).is_function;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_callable fn ptr", __LINE__,
            SVI("return (int(*)(int)).is_callable;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_callable int false", __LINE__,
            SVI("return (int).is_callable;\n"),
            .exit_code = 0,
        },
        {
            "type introspection: pointee", __LINE__,
            SVI("return (int*).pointee.is_integer;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: count", __LINE__,
            SVI("return (int)(int[5]).count;\n"),
            .exit_code = 5,
        },
        {
            "type introspection: is_incomplete", __LINE__,
            SVI("struct S;\n"
               "return (struct S).is_incomplete;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_incomplete false", __LINE__,
            SVI("struct S { int x; };\n"
               "return (struct S).is_incomplete;\n"),
            .exit_code = 0,
        },
        {
            "type introspection: is_variadic", __LINE__,
            SVI("return (int(int, ...)).is_variadic;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_variadic false", __LINE__,
            SVI("return (int(int)).is_variadic;\n"),
            .exit_code = 0,
        },
        {
            "type introspection: unqual", __LINE__,
            SVI("return (const volatile int).unqual.is_const;\n"),
            .exit_code = 0,
        },
        {
            "type introspection: tag", __LINE__,
            SVI("struct Foo { int x; };\n"
               "const char s[:] = (struct Foo).tag;\n"
               "return s[0] == 'F' && s[1] == 'o' && s[2] == 'o';\n"),
            .exit_code = 1,
        },
        {
            "type introspection: name", __LINE__,
            SVI("const char s[:] = (int).name;\n"
               "return s[0] == 'i' && s[1] == 'n' && s[2] == 't';\n"),
            .exit_code = 1,
        },
        {
            "atomic: fetch_and", __LINE__,
            SVI("int x = 0xFF;\n"
               "int old = __atomic_fetch_and(&x, 0x0F, __ATOMIC_SEQ_CST);\n"
               "return old * 10 + x;\n"),
            .exit_code = 0xFF * 10 + 0x0F,
        },
        {
            "atomic: fetch_or", __LINE__,
            SVI("int x = 0xF0;\n"
               "int old = __atomic_fetch_or(&x, 0x0F, __ATOMIC_SEQ_CST);\n"
               "return old + x - 0xFF;\n"),
            .exit_code = 0xF0,
        },
        {
            "atomic: fetch_xor", __LINE__,
            SVI("int x = 0xFF;\n"
               "int old = __atomic_fetch_xor(&x, 0x0F, __ATOMIC_SEQ_CST);\n"
               "return old - x;\n"),
            .exit_code = 0xFF - 0xF0,
        },
        {
            "type introspection: fields count", __LINE__,
            SVI("struct S { int x; int y; int z; };\n"
               "return (int)(struct S).fields;\n"),
            .exit_code = 3,
        },
        {
            "type introspection: has field and method", __LINE__,
            SVI("struct S { int x; int get(void){return 7;} };\n"
                "_Type t = struct S;\n"
                "_Static_assert((struct S).has_field(\"x\") && !(struct S).has_field(\"get\"));\n"
                "_Static_assert((struct S).has_method(\"get\") && !(struct S).has_method(\"x\"));\n"
                "_Static_assert(!(struct S).has_field(\"missing\") && !(struct S).has_method(\"missing\"));\n"
                "_Static_assert(!(struct S).has_field(\"\") && !(struct S).has_method(\"\"));\n"
                "char name[] = {'g','e','t'};\n"
                "const char field[:] = \"!x?\"[1:2];\n"
                "return t.has_field(field) && t.has_method(name) && !t.has_field(name)\n"
                "    && !t.has_method(field) && !t.has_field(\"missing\") && !t.has_method(\"missing\")\n"
                "    && !t.has_field(\"\") && !t.has_method(\"ge\") && !t.has_method(\"get\\0\");\n"),
            .exit_code = 1,
        },
        {
            "type introspection: has member guards static if", __LINE__,
            SVI("constexpr _Type T = struct S { int x; int get(void){return 7;} };\n"
                "static if(T.has_field(\"x\") && T.field(\"x\").type == int){ int found = 1; }\n"
                "static if(T.has_field(\"missing\") && T.field(\"missing\").type == int){ invalid tokens here }\n"
                "static if(T.has_method(\"missing\") && T.method(\"missing\").type == int(void)){ invalid tokens here }\n"
                "static if(T.has_method(\"get\") && T.method(\"get\").type == int(void)){ int method_found = 1; }\n"
                "_Type t = T;\n"
                "if(t.has_field(\"missing\") && t.field(\"missing\").type == int) return 0;\n"
                "if(t.has_method(\"missing\") && t.method(\"missing\").address) return 0;\n"
                "return found && method_found;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: has anonymous and union members", __LINE__,
            SVI("struct S { union { int x; int get(void){return 1;} }; };\n"
                "union U { int y; int get(void){return 2;} };\n"
                "_Static_assert((struct S).has_field(\"x\") && (struct S).has_method(\"get\"));\n"
                "_Static_assert((union U).has_field(\"y\") && (union U).has_method(\"get\"));\n"
                "_Type s = struct S; _Type u = union U;\n"
                "return s.has_field(\"x\") && s.has_method(\"get\") && !s.has_field(\"get\")\n"
                "    && u.has_field(\"y\") && u.has_method(\"get\") && !u.has_method(\"y\");\n"),
            .exit_code = 1,
        },
        {
            "type introspection: has member on non aggregate", __LINE__,
            SVI("_Static_assert(!(int).has_field(\"x\") && !(int).has_method(\"get\"));\n"
                "_Type t = int; int n = 0;\n"
                "return !t.has_field((n++, \"x\"[:1])) && !t.has_method(\"get\") && n == 1;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: has method does not resolve function", __LINE__,
            SVI("struct S { int not_defined_anywhere(void); };\n"
                "_Static_assert((struct S).has_method(\"not_defined_anywhere\"));\n"
                "_Type t = struct S;\n"
                "return t.has_method(\"not_defined_anywhere\");\n"),
            .exit_code = 1,
        },
        {
            "constexpr reflection: field name slice", __LINE__,
            SVI("struct S { int hello; };\n"
               "constexpr struct __builtin_Field f = (struct S).field(0);\n"
               "_Static_assert(f.name.count == 5);\n"
               "return f.name.count == 5 && f.name[0] == 'h' && f.name[4] == 'o' && f.type == int && f.offset == 0;\n"),
            .exit_code = 1,
        },
        {
            "constexpr reflection: enumerator name slice", __LINE__,
            SVI("enum E { HELLO = -17 };\n"
               "constexpr struct __builtin_Enumerator e = (enum E).enumerator(0);\n"
               "_Static_assert(e.name.count == 5);\n"
               "_Static_assert(e.value == -17);\n"
               "return e.name.count == 5 && e.name[0] == 'H' && e.name[4] == 'O' && e.value == -17;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: field name", __LINE__,
            SVI("struct S { int x; int y; };\n"
               "struct __builtin_Field f = (struct S).field(0);\n"
               "return f.name[0] == 'x';\n"),
            .exit_code = 1,
        },
        {
            "type introspection: field lookup by slice", __LINE__,
            SVI("struct S { int x; long hello; };\n"
                "_Type t = struct S;\n"
                "const char name[:] = \"!hello?\"[1:6];\n"
                "struct __builtin_Field f = t.field(name);\n"
                "return f.type == long && f.offset == t.field(1).offset && f.name.count == 5;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: union field lookup by name", __LINE__,
            SVI("union U { int x; long y; };\n"
                "_Type t = union U;\n"
                "const char name[:] = \"y\"[:1];\n"
                "struct __builtin_Field f = t.field(name);\n"
                "return f.type == long && f.offset == 0;\n"),
            .exit_code = 1,
        },
        {
            "constexpr reflection: field lookup by name", __LINE__,
            SVI("struct S { int x; long hello; };\n"
                "constexpr struct __builtin_Field indexed = (struct S).field(1);\n"
                "constexpr struct __builtin_Field named = (struct S).field(indexed.name);\n"
                "_Static_assert(named.type == long);\n"
                "return named.offset == indexed.offset;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: bitfield lookup by name", __LINE__,
            SVI("struct S { unsigned x:3; unsigned y:5; };\n"
                "_Type t = struct S;\n"
                "struct __builtin_Field f = t.field(\"y\"[:1]);\n"
                "struct __builtin_Field indexed = t.field(1);\n"
                "return f.is_bitfield && f.bitwidth == 5 && f.bitoffset == indexed.bitoffset;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: method lookup by name", __LINE__,
            SVI("(struct S {int x;}).push_method(get_x, int(struct S* self){return self.x;});\n"
                "_Type t = struct S;\n"
                "struct __builtin_Method f = t.method(\"get_x\"[:5]);\n"
                "constexpr struct __builtin_Method indexed = (struct S).method(0);\n"
                "constexpr struct __builtin_Method named = (struct S).method(indexed.name);\n"
                "_Static_assert(named.type == int(struct S*));\n"
                "_Static_assert(named.offset == 0 && indexed.offset == 0);\n"
                "struct S s = {42};\n"
                "int (*fn)(struct S*) = (int(*)(struct S*))f.address;\n"
                "return f.type == indexed.type && named.type == indexed.type && f.name.count == 5\n"
                "    && f.offset == 0 && t.method(0).offset == 0\n"
                "    && f.address == indexed.address && fn(&s) == 42;\n"),
            .exit_code = 1,
        },
        {
            "method reflection: address after direct calls through Plan 9 embed", __LINE__,
            SVI("struct Lock { int locked; void lock(_Self* self){self.locked = 1;} void unlock(_Self* self){self.locked = 0;} };\n"
                "struct Object { long prefix; struct Lock; void use(_Self* self){self.lock(); self.unlock();} };\n"
                "struct Object o = {.prefix = 42};\n"
                "o.use();\n"
                "static if((struct Object).has_method(\"lock\") && (struct Object).has_method(\"unlock\")){\n"
                "    struct __builtin_Method l = (struct Object).method(\"lock\");\n"
                "    _Type t = struct Object;\n"
                "    struct __builtin_Method u = t.method(\"unlock\");\n"
                "    if(!l.address || !u.address) return 0;\n"
                "    ((void(*)(struct Lock*))l.address)((struct Lock*)((char*)&o + l.offset));\n"
                "    if(!o.locked) return 0;\n"
                "    ((void(*)(struct Lock*))u.address)((struct Lock*)((char*)&o + u.offset));\n"
                "    if(o.locked || o.prefix != 42) return 0;\n"
                "    struct __builtin_Method direct = t.method(0);\n"
                "    if(!direct.address || direct.offset) return 0;\n"
                "    ((void(*)(struct Object*))direct.address)(&o);\n"
                "    return t.method(\"lock\").address == l.address;\n"
                "}\n"
                "return 0;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: nested method receiver offset", __LINE__,
            SVI("struct Outer { long prefix; union { struct { long inner_prefix; struct { int value; int get(_Self* self){return self.value;} }; }; }; };\n"
                "constexpr struct __builtin_Method m = (struct Outer).method(\"get\");\n"
                "constexpr _Type Receiver = m.type.param_type(0).pointee;\n"
                "_Static_assert(m.offset == __builtin_offsetof(struct Outer, value));\n"
                "_Static_assert(m.offset > 0);\n"
                "_Static_assert(Receiver.method(0).offset == 0);\n"
                "_Type t = struct Outer;\n"
                "struct __builtin_Method runtime = t.method(\"get\");\n"
                "struct Outer o = {.prefix = 11, .inner_prefix = 22, .value = 42};\n"
                "int (*fn)(Receiver*) = (int(*)(Receiver*))runtime.address;\n"
                "int (*constant_fn)(Receiver*) = (int(*)(Receiver*))m.address;\n"
                "return runtime.offset == m.offset && runtime.type == m.type\n"
                "    && fn((Receiver*)((char*)&o + runtime.offset)) == 42\n"
                "    && constant_fn((Receiver*)((char*)&o + m.offset)) == 42;\n"),
            .exit_code = 1,
        },
        {
            "method call: nested anonymous pointer receiver", __LINE__,
            SVI("struct Outer { long prefix; union { struct { long inner_prefix; struct { int value; int add(_Self* self, int n){return self.value += n;} }; }; }; };\n"
                "struct Outer o = {.prefix = 11, .inner_prefix = 22, .value = 40};\n"
                "struct Outer* p = &o; int calls = 0;\n"
                "int a = o.add(1);\n"
                "int b = (calls++, p)->add(1);\n"
                "return a == 41 && b == 42 && o.value == 42 && calls == 1\n"
                "    && o.prefix == 11 && o.inner_prefix == 22;\n"),
            .exit_code = 1,
        },
        {
            "method call: anonymous value receiver", __LINE__,
            SVI("struct Outer { long prefix; struct { int value; int get(_Self self){return self.value;} }; };\n"
                "struct Outer o = {.prefix = 11, .value = 42};\n"
                "struct Outer* p = &o;\n"
                "return o.get() == 42 && p->get() == 42\n"
                "    && (struct Outer){.prefix = 7, .value = 43}.get() == 43;\n"),
            .exit_code = 1,
        },
        {
            "method call: Plan 9 embeds at nonzero offsets", __LINE__,
            SVI("struct Base { int value; int add(_Self* self, int n){return self.value += n;} int get(_Self self){return self.value;} };\n"
                "struct Middle { long middle_prefix; struct Base; };\n"
                "struct Outer { long outer_prefix; struct Middle; };\n"
                "_Static_assert((struct Middle).method(\"add\").offset > 0);\n"
                "_Static_assert((struct Outer).method(\"add\").offset == (struct Outer).field(1).offset + (struct Middle).field(1).offset);\n"
                "struct Middle middle = {.middle_prefix = 11, .value = 40};\n"
                "if(middle.add(1) != 41 || middle.middle_prefix != 11) return 0;\n"
                "struct Outer outer = {.outer_prefix = 22, .middle_prefix = 33, .value = 40};\n"
                "struct Outer* p = &outer; int calls = 0;\n"
                "int a = outer.add(1);\n"
                "int b = (calls++, p)->add(1);\n"
                "_Type t = struct Outer;\n"
                "struct __builtin_Method m = t.method(\"add\");\n"
                "int (*fn)(struct Base*, int) = (int(*)(struct Base*, int))m.address;\n"
                "int c = fn((struct Base*)((char*)&outer + m.offset), 1);\n"
                "return a == 41 && b == 42 && c == 43 && calls == 1\n"
                "    && outer.get() == 43 && p->get() == 43\n"
                "    && outer.outer_prefix == 22 && outer.middle_prefix == 33;\n"),
            .exit_code = 1,
        },
        {
            "Plan 9 pointer conversion: nonzero offset", __LINE__,
            SVI("struct Base { int value; };\n"
                "struct Derived { long prefix; struct Base; };\n"
                "int read(struct Base* p){return p->value;}\n"
                "struct Base* convert(struct Derived* p){return p;}\n"
                "struct Derived d = {.prefix = 11, .value = 42};\n"
                "struct Derived* p = &d; int calls = 0;\n"
                "struct Base* b = (calls++, p);\n"
                "if(b != (struct Base*)((char*)p + __builtin_offsetof(struct Derived, value))) return 0;\n"
                "if(read(p) != 42 || convert(p) != b || calls != 1) return 0;\n"
                "b = p; b->value = 43;\n"
                "return d.value == 43 && d.prefix == 11;\n"),
            .exit_code = 1,
        },
        {
            "Plan 9 pointer conversion: nested and qualified", __LINE__,
            SVI("struct Base { int value; };\n"
                "struct Middle { long middle_prefix; struct Base; };\n"
                "struct Outer { long outer_prefix; struct Middle; };\n"
                "const struct Outer o = {.outer_prefix = 11, .middle_prefix = 22, .value = 42};\n"
                "const struct Outer* p = &o;\n"
                "const struct Base* b = p;\n"
                "return b->value == 42 && (const char*)b == (const char*)p + __builtin_offsetof(struct Outer, value);\n"),
            .exit_code = 1,
        },
        {
            "Plan 9 pointer conversion: address-of semantics and explicit cast", __LINE__,
            SVI("struct Base { int value; };\n"
                "struct Derived { long prefix; struct Base; };\n"
                "struct Derived* p = nullptr; int calls = 0;\n"
                "struct Base* b = (calls++, p);\n"
                "struct Derived d = {.prefix = 11, .value = 42};\n"
                "return (char*)b == (char*)&p->value && calls == 1\n"
                "    && (void*)(struct Base*)&d == (void*)&d;\n"),
            .exit_code = 1,
        },
        {
            "Plan 9 union embed: nonzero offset", __LINE__,
            SVI("union Base { int value; long other; int add(_Self* self, int n){return self.value += n;} };\n"
                "struct Derived { long prefix; union Base; };\n"
                "int read(union Base* p){return p->value;}\n"
                "union Base* convert(struct Derived* p){return p;}\n"
                "struct Derived d = {.prefix = 11, .value = 40};\n"
                "struct Derived* p = &d; int calls = 0;\n"
                "union Base* b = (calls++, p);\n"
                "if((char*)b != (char*)&d + __builtin_offsetof(struct Derived, value) || calls != 1) return 0;\n"
                "if(read(p) != 40 || convert(p) != b || d.add(1) != 41 || p->add(1) != 42) return 0;\n"
                "b = p; b->value = 43;\n"
                "_Static_assert((struct Derived).method(\"add\").offset == __builtin_offsetof(struct Derived, value));\n"
                "_Type t = struct Derived;\n"
                "return d.value == 43 && d.prefix == 11 && t.has_field(\"value\")\n"
                "    && t.has_method(\"add\") && t.method(\"add\").offset == (struct Derived).field(1).offset;\n"),
            .exit_code = 1,
        },
        {
            "Plan 9 union container: struct and union embeds", __LINE__,
            SVI("struct S { int x; }; union U { int y; };\n"
                "union StructContainer { struct S; long other; };\n"
                "union UnionContainer { union U; long other; };\n"
                "union StructContainer s = {.x = 41}; union UnionContainer u = {.y = 42};\n"
                "union StructContainer* sp = &s; union UnionContainer* up = &u;\n"
                "struct S* ps = sp; union U* pu = up;\n"
                "return ps->x == 41 && pu->y == 42 && (void*)ps == (void*)&s && (void*)pu == (void*)&u;\n"),
            .exit_code = 1,
        },
        {
            "Plan 9 pointer conversion: nested unions and qualifiers", __LINE__,
            SVI("union Base { int value; };\n"
                "struct Middle { long middle_prefix; union Base; };\n"
                "union Wrap { struct Middle; };\n"
                "struct Outer { long outer_prefix; union Wrap; };\n"
                "const struct Outer o = {.outer_prefix = 11, .middle_prefix = 22, .value = 42};\n"
                "const struct Outer* p = &o;\n"
                "const union Base* b = p;\n"
                "return b->value == 42 && (const char*)b == (const char*)p + __builtin_offsetof(struct Outer, value);\n"),
            .exit_code = 1,
        },
        {
            "method call: zero offset anonymous const receiver", __LINE__,
            SVI("struct Outer { union { int other; struct { int value; int get(const _Self* self){return self.value;} }; }; };\n"
                "const struct Outer o = {.value = 42};\n"
                "const struct Outer* p = &o;\n"
                "return o.get() == 42 && p->get() == 42;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: separate fields and methods", __LINE__,
            SVI("struct S { int x; int first(struct S* self){return self.x;} long y; int second(struct S* self){return self.y;} };\n"
                "_Type t = struct S;\n"
                "_Static_assert((struct S).fields == 2 && (struct S).methods == 2);\n"
                "_Static_assert((struct S).field(1).type == long);\n"
                "_Static_assert((struct S).method(1).name.count == 6);\n"
                "char name[] = {'s','e','c','o','n','d'};\n"
                "struct __builtin_Method m = t.method(name);\n"
                "struct S s = {3, 7};\n"
                "return t.fields == 2 && t.methods == 2 && t.field(1).name[0] == 'y'\n"
                "    && t.method(1).address == m.address && ((int(*)(struct S*))m.address)(&s) == 7;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: union methods and empty method count", __LINE__,
            SVI("union U { int x; int get(union U* self){return self.x;} long y; };\n"
                "_Type t = union U;\n"
                "_Static_assert((union U).fields == 2 && (union U).methods == 1);\n"
                "_Static_assert((struct Empty {}).methods == 0);\n"
                "union U u = {.x = 9};\n"
                "struct __builtin_Method m = t.method(\"get\");\n"
                "return t.fields == 2 && t.methods == 1 && t.field(1).type == long\n"
                "    && ((int(*)(union U*))m.address)(&u) == 9;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: field lookup by array and literal", __LINE__,
            SVI("struct S { int x; long hello; };\n"
                "_Type t = struct S;\n"
                "char name[5] = {'h', 'e', 'l', 'l', 'o'};\n"
                "constexpr struct __builtin_Field f = (struct S).field(\"hello\");\n"
                "_Static_assert(f.type == long);\n"
                "return t.field(name).type == long && t.field(\"hello\").offset == f.offset;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: anonymous field lookup by name", __LINE__,
            SVI("struct S { int pad; struct { int inner_pad; union { long value; unsigned bits:5; }; }; };\n"
                "_Type t = struct S;\n"
                "constexpr struct __builtin_Field f = (struct S).field(\"value\");\n"
                "constexpr struct __builtin_Field b = (struct S).field(\"bits\");\n"
                "_Static_assert(f.type == long);\n"
                "_Static_assert(f.offset == __builtin_offsetof(struct S, value));\n"
                "_Static_assert(b.is_bitfield && b.bitwidth == 5);\n"
                "struct __builtin_Field runtime = t.field(\"value\");\n"
                "struct __builtin_Field bits = t.field(\"bits\");\n"
                "return runtime.type == f.type && runtime.offset == f.offset && runtime.name.count == 5\n"
                "    && bits.offset == b.offset && bits.bitoffset == b.bitoffset && bits.bitwidth == 5 && bits.is_bitfield;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: field offset", __LINE__,
            SVI("struct S { int x; int y; };\n"
               "struct __builtin_Field f = (struct S).field(1);\n"
               "return (int)f.offset;\n"),
            .exit_code = 4,
        },
        {
            "type introspection: enumerators count", __LINE__,
            SVI("enum E { A, B, C, D };\n"
               "return (int)(enum E).enumerators;\n"),
            .exit_code = 4,
        },
        {
            "type introspection: enumerator value", __LINE__,
            SVI("enum E { A = 10, B = 20 };\n"
               "struct __builtin_Enumerator e = (enum E).enumerator(1);\n"
               "return (int)e.value;\n"),
            .exit_code = 20,
        },
        {
            "wide enum values survive static initialization and reflection", __LINE__,
            SVI("enum U:unsigned __int128 {HIGH=(unsigned __int128)1<<100,NEXT,MAX=~(unsigned __int128)0};\n"
                "enum S:__int128 {LOW=-((__int128)1<<100),AFTER};\n"
                "static enum U values[3]={HIGH,NEXT,MAX}; static enum S neg=AFTER;\n"
                "struct __builtin_Enumerator runtime=(enum U).enumerator(2);\n"
                "static struct __builtin_Enumerator folded=(enum S).enumerator(0);\n"
                "return values[0]==HIGH&&values[1]==HIGH+1&&values[2]==~(unsigned __int128)0"
                    "&&neg==LOW+1&&runtime.value==MAX&&runtime.type==enum U"
                    "&&folded.value==(unsigned __int128)LOW&&folded.type==enum S;\n"),
            .exit_code = 1,
        },
        {
            "fixed enum storage before its body retains its layout", __LINE__,
            SVI("enum E:unsigned __int128;\n"
                "struct S {enum E value; int tail;};\n"
                "static struct S s={(enum E)((unsigned __int128)1<<100),7};\n"
                "enum E {HIGH=(unsigned __int128)1<<100};\n"
                "return sizeof(s)==32&&s.value==HIGH&&s.tail==7&&!(enum E).is_incomplete;\n"),
            .exit_code = 1,
        },
        {
            "fixed enum declarations in an inner scope create a new tag", __LINE__,
            SVI("enum E:unsigned char {OUTER=255};\n"
                "{enum E:unsigned __int128; enum E {INNER=(unsigned __int128)1<<100};\n"
                " if(sizeof(enum E)!=16||INNER!=((unsigned __int128)1<<100)) return 0;}\n"
                "return sizeof(enum E)==1&&OUTER==255;\n"),
            .exit_code = 1,
        },
        {
            "packed enum arrays use finalized storage widths", __LINE__,
            SVI("enum __attribute__((packed)) P {A=255};\n"
                "enum N {NEG=-129,POS=127} __attribute__((packed));\n"
                "static enum P p[2]={A,A}; static enum N n[2]={NEG,POS};\n"
                "return sizeof(p)==2&&sizeof(n)==4&&p[1]==255&&n[0]==-129&&n[1]==127;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: enumerator name", __LINE__,
            SVI("enum E { FOO = 1, BAR = 2 };\n"
               "struct __builtin_Enumerator e = (enum E).enumerator(0);\n"
               "return e.name[0] == 'F' && e.name[1] == 'O' && e.name[2] == 'O';\n"),
            .exit_code = 1,
        },
        {
            "type introspection: return_type", __LINE__,
            SVI("return (int(int, int)).return_type.is_integer;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: param_count", __LINE__,
            SVI("return (int)(int(int, float)).param_count;\n"),
            .exit_code = 2,
        },
        {
            "type introspection: param_type", __LINE__,
            SVI("return (int(int, float)).param_type(1).is_float;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: element_type", __LINE__,
            SVI("return (int[5]).element_type.is_integer;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: underlying_type", __LINE__,
            SVI("enum E : unsigned char { A };\n"
               "return (enum E).underlying_type.is_unsigned;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: castable_to", __LINE__,
            SVI("_Bool r = (int).is_castable_to(float);\n"
               "return r;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_callable_through runtime types", __LINE__,
            SVI("_Type t = void(int*); _Type through = void(void*);\n"
                "return t.is_callable_through(through)\n"
                " && (void(*)(int*)).is_callable_through(void(const void*))\n"
                " && !t.is_callable_through(void(int))\n"
                " && !t.is_callable_through(void(*)(void*));\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_callable_through constants", __LINE__,
            SVI("struct S { int x; }; struct U { int x; };\n"
                "enum E : unsigned { E0 };\n"
                "_Static_assert((void(int*)).is_callable_through(void(int*)));\n"
                "_Static_assert((int(int*)).is_callable_through(unsigned(void*)));\n"
                "_Static_assert((void(enum E)).is_callable_through(void(unsigned)));\n"
                "_Static_assert((struct S(struct S)).is_callable_through(struct S(struct S)));\n"
                "_Static_assert(!(void(struct S)).is_callable_through(void(struct U)));\n"
                "_Static_assert(!(void(int)).is_callable_through(void(float)));\n"
                "_Static_assert(!(void(float)).is_callable_through(void(double)));\n"
                "_Static_assert(!(void(short)).is_callable_through(void(int)));\n"
                "_Static_assert(!(void(signed char)).is_callable_through(void(unsigned char)));\n"
                "_Static_assert(!(void(_Bool)).is_callable_through(void(unsigned char)));\n"
                "_Static_assert(!(void(int)).is_callable_through(void(int, int)));\n"
                "_Static_assert(!(void(int,...)).is_callable_through(void(int)));\n"
                "_Static_assert((void(int*,...)).is_callable_through(void(void*,...)));\n"
                "_Static_assert(!(void(void)).is_callable_through(int(void)));\n"
                "_Static_assert(!(int).is_callable_through(void(void)));\n"
                "_Static_assert(!(void(void)).is_callable_through(int));\n"
                "return 1;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_callable_with", __LINE__,
            SVI("typedef int fn_t(int);\n"
               "_Bool r = (fn_t).is_callable_with(int);\n"
               "return r;\n"),
            .exit_code = 1,
        },
        {
            "type introspection: is_callable_with false", __LINE__,
            SVI("struct S { int x; };\n"
               "typedef int fn_t(int);\n"
               "_Bool r = (fn_t).is_callable_with(struct S);\n"
               "return !r;\n"),
            .exit_code = 1,
        },
        {
            "__builtin_intern bounded slice and terminator", __LINE__,
            SVI("char name[3] = {'a','b','c'};\n"
                "const char a[:] = __builtin_intern(name[:3]);\n"
                "const char b[:] = __builtin_intern(\"!abc?\"[1:4]);\n"
                "name[0] = 'x';\n"
                "return a.count == 3 && a.data == b.data && a[0] == 'a'\n"
                "    && a.data[a.count] == 0;\n"),
            .exit_code = 1,
        },
        {
            "__builtin_intern empty and embedded null slices", __LINE__,
            SVI("const char empty[:] = {};\n"
                "const char a[:] = __builtin_intern(empty);\n"
                "const char b[:] = __builtin_intern(\"\");\n"
                "const char c[:] = __builtin_intern(\"a\\0b\");\n"
                "return a.count == 0 && a.data != nullptr && a.data[0] == 0\n"
                "    && a.data == b.data && c.count == 3 && c[1] == 0\n"
                "    && c[2] == 'b' && c.data[c.count] == 0;\n"),
            .exit_code = 1,
        },
        {
            "__builtin_intern", __LINE__,
            SVI("const char a[:] = __builtin_intern(\"hello\");\n"
               "const char b[:] = __builtin_intern(\"hello\");\n"
               "return a.data == b.data && a.count == 5 && a.data[5] == 0;\n"),
            .exit_code = 1,
        },
        {
            "numeric literal: unsigned long", __LINE__,
            SVI("unsigned long x = 42ul;\n"
               "return (int)x;\n"),
            .exit_code = 42,
        },
        {
            "numeric literal: unsigned long long", __LINE__,
            SVI("unsigned long long x = 42ull;\n"
               "return (int)x;\n"),
            .exit_code = 42,
        },
        {
            "cast: to long double", __LINE__,
            SVI("int n = 7;\n"
                "long double a = (long double)n;\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "cast: long double to bool", __LINE__,
            SVI("long double a = 0.5L;\n"
                "return (_Bool)a;\n"),
            .exit_code = 1,
        },
        {
            "cast: qualified slice evaluates once", __LINE__,
            SVI("int a[3] = {2,4,6};\n"
                "int s[:] = a[:];\n"
                "int calls = 0;\n"
                "const int t[:] = (const int[:])(++calls, s);\n"
                "int u[:] = (int[:])t;\n"
                "return calls * 100 + u.count * 10 + u[1];\n"),
            .exit_code = 134,
        },
        {
            "cast: array to slice comma evaluates once", __LINE__,
            SVI("int a[3] = {2,4,6};\n"
                "int calls = 0;\n"
                "int s[:] = (int[:])(++calls, (++calls, a));\n"
                "return calls * 100 + s.count * 10 + s[1];\n"),
            .exit_code = 234,
        },
        {
            "cast: array to bool evaluates address", __LINE__,
            SVI("struct S { int a[2]; } s;\n"
                "int calls = 0;\n"
                "struct S *get(void){ ++calls; return &s; }\n"
                "_Bool b = (_Bool)get()->a;\n"
                "return calls * 10 + b;\n"),
            .exit_code = 11,
        },
        {
            "cast: array address conversions", __LINE__,
            SVI("int a[2] = {3,7};\n"
                "unsigned long long addr = (unsigned long long)&a[0];\n"
                "unsigned char small = (unsigned char)a;\n"
                "unsigned __int128 wide = (unsigned __int128)a;\n"
                "typeof(nullptr) np = (typeof(nullptr))a;\n"
                "return small == (unsigned char)addr && wide == addr\n"
                "    && (unsigned long long)np == addr\n"
                "    && (unsigned long long)a == addr;\n"),
            .exit_code = 1,
        },
        {
            "cast: function address conversions", __LINE__,
            SVI("int f(void){ return 7; }\n"
                "unsigned long long addr = (unsigned long long)&f;\n"
                "struct { unsigned char small, guard; } s = {0, 93};\n"
                "s.small = (unsigned char)f;\n"
                "unsigned __int128 wide = (unsigned __int128)f;\n"
                "typeof(nullptr) np = (typeof(nullptr))f;\n"
                "_Bool b = (_Bool)f;\n"
                "return s.small == (unsigned char)addr && s.guard == 93\n"
                "    && wide == addr && (unsigned long long)np == addr\n"
                "    && (unsigned long long)f == addr && (int)b == 1;\n"),
            .exit_code = 1,
        },
        {
            "cast: dereferenced function pointer conversions", __LINE__,
            SVI("int f(void){ return 7; }\n"
                "int (*p)(void) = f;\n"
                "int calls = 0;\n"
                "_Bool b = (_Bool)(++calls, *p);\n"
                "unsigned char small = (unsigned char)*p;\n"
                "unsigned __int128 wide = (unsigned __int128)*p;\n"
                "unsigned long long addr = (unsigned long long)p;\n"
                "p = nullptr;\n"
                "return (int)b == 1 && calls == 1 && !(_Bool)*p\n"
                "    && small == (unsigned char)addr && wide == addr;\n"),
            .exit_code = 1,
        },
        {
            "cast: void preserves comma side effects", __LINE__,
            SVI("int n = 0;\n"
                "(void)(++n, ++n);\n"
                "return n;\n"),
            .exit_code = 2,
        },
        {
            "cast: nullptr_t from integer and pointer", __LINE__,
            SVI("unsigned long long z = 0;\n"
                "int *p = (int*)z;\n"
                "typeof(nullptr) a = (typeof(nullptr))z;\n"
                "typeof(nullptr) b = (typeof(nullptr))p;\n"
                "return (int)a + (int)b;\n"),
            .exit_code = 0,
        },
        {
            "numeric literal: long double", __LINE__,
            SVI("long double x = 7.0L;\n"
               "return (int)x;\n"),
            .exit_code = 7,
        },
        {
            "nullptr", __LINE__,
            SVI("int *p = nullptr;\n"
               "return p == nullptr;\n"),
            .exit_code = 1,
        },
        {
            "true false", __LINE__,
            SVI("return true && !false;\n"),
            .exit_code = 1,
        },
        {
            "_Generic: array decay to pointer", __LINE__,
            SVI("int arr[3] = {1, 2, 3};\n"
               "return _Generic(arr, int*: 1, default: 0);\n"),
            .exit_code = 1,
        },
        {
            "pointer: le comparison", __LINE__,
            SVI("int arr[3] = {0};\n"
               "return &arr[0] <= &arr[2];\n"),
            .exit_code = 1,
        },
        {
            "pointer: ge comparison", __LINE__,
            SVI("int arr[3] = {0};\n"
               "return &arr[2] >= &arr[0];\n"),
            .exit_code = 1,
        },
        {
            "pointer: le equal", __LINE__,
            SVI("int x;\n"
               "return &x <= &x;\n"),
            .exit_code = 1,
        },
        {
            "bitfield: mulassign", __LINE__,
            SVI("struct S { unsigned a : 8; };\n"
               "struct S s = {5};\n"
               "s.a *= 3;\n"
               "return s.a;\n"),
            .exit_code = 15,
        },
        {
            "bitfield: divassign unsigned", __LINE__,
            SVI("struct S { unsigned a : 8; };\n"
               "struct S s = {42};\n"
               "s.a /= 6;\n"
               "return s.a;\n"),
            .exit_code = 7,
        },
        {
            "bitfield: modassign unsigned", __LINE__,
            SVI("struct S { unsigned a : 8; };\n"
               "struct S s = {17};\n"
               "s.a %= 5;\n"
               "return s.a;\n"),
            .exit_code = 2,
        },
        {
            "bitfield: lshiftassign", __LINE__,
            SVI("struct S { unsigned a : 8; };\n"
               "struct S s = {1};\n"
               "s.a <<= 4;\n"
               "return s.a;\n"),
            .exit_code = 16,
        },
        {
            "bitfield: rshiftassign unsigned", __LINE__,
            SVI("struct S { unsigned a : 8; };\n"
               "struct S s = {128};\n"
               "s.a >>= 3;\n"
               "return s.a;\n"),
            .exit_code = 16,
        },
        {
            "bitfield: xorassign", __LINE__,
            SVI("struct S { unsigned a : 8; };\n"
               "struct S s = {0xFF};\n"
               "s.a ^= 0x0F;\n"
               "return s.a;\n"),
            .exit_code = 0xF0,
        },
        {
            "__builtin_huge_valf", __LINE__,
            SVI("float __builtin_huge_valf(void);\n"
               "float f = __builtin_huge_valf();\n"
               "return f > 1000000.0f;\n"),
            .exit_code = 1,
        },
        {
            "__builtin_huge_val", __LINE__,
            SVI("double __builtin_huge_val(void);\n"
               "double d = __builtin_huge_val();\n"
               "return d > 1000000.0;\n"),
            .exit_code = 1,
        },
        {
            "__builtin_nanf", __LINE__,
            SVI("float __builtin_nanf(const char*);\n"
               "float f = __builtin_nanf(\"\");\n"
               "return f != f;\n"),
            .exit_code = 1,
        },
        {
            "__builtin_nan", __LINE__,
            SVI("double __builtin_nan(const char*);\n"
               "double d = __builtin_nan(\"\");\n"
               "return d != d;\n"),
            .exit_code = 1,
        },
        {
            "char literal: wchar", __LINE__,
            SVI("int x = L'A';\n"
               "return x;\n"),
            .exit_code = 65,
        },
        {
            "char literal: char16", __LINE__,
            SVI("unsigned short x = u'B';\n"
               "return x;\n"),
            .exit_code = 66,
        },
        {
            "char literal: char32", __LINE__,
            SVI("unsigned x = U'C';\n"
               "return x;\n"),
            .exit_code = 67,
        },
        {
            "constexpr: int arithmetic", __LINE__,
            SVI("constexpr int a = 3 + 4 * 5;\n"
               "return a;\n"),
            .exit_code = 23,
        },
        {
            "constexpr: negation", __LINE__,
            SVI("constexpr int a = -42;\n"
               "return a + 100;\n"),
            .exit_code = 58,
        },
        {
            "constexpr: logical not", __LINE__,
            SVI("constexpr int a = !0;\n"
               "constexpr int b = !1;\n"
               "return a * 10 + b;\n"),
            .exit_code = 10,
        },
        {
            "constexpr: bitwise not", __LINE__,
            SVI("constexpr int a = ~0 & 0xFF;\n"
               "return a;\n"),
            .exit_code = 255,
        },
        {
            "constexpr: shift", __LINE__,
            SVI("constexpr int a = 1 << 4;\n"
               "constexpr int b = 256 >> 4;\n"
               "return a + b;\n"),
            .exit_code = 32,
        },
        {
            "constexpr: comparison", __LINE__,
            SVI("constexpr int a = (3 < 5);\n"
               "constexpr int b = (5 > 3);\n"
               "constexpr int c = (3 <= 3);\n"
               "constexpr int d = (3 >= 3);\n"
               "constexpr int e = (3 == 3);\n"
               "constexpr int f = (3 != 4);\n"
               "return a + b + c + d + e + f;\n"),
            .exit_code = 6,
        },
        {
            "constexpr: bitwise ops", __LINE__,
            SVI("constexpr int a = 0xFF & 0x0F;\n"
               "constexpr int b = 0xF0 | 0x0F;\n"
               "constexpr int c = 0xFF ^ 0x0F;\n"
               "return a + b - c;\n"),
            .exit_code = 15 + 255 - 240,
        },
        {
            "constexpr: modulo", __LINE__,
            SVI("constexpr int a = 17 % 5;\n"
               "return a;\n"),
            .exit_code = 2,
        },
        {
            "constexpr: ternary", __LINE__,
            SVI("constexpr int a = 1 ? 10 : 20;\n"
               "constexpr int b = 0 ? 10 : 20;\n"
               "return a + b;\n"),
            .exit_code = 30,
        },
        {
            "constexpr: cast int to float", __LINE__,
            SVI("constexpr float f = (float)7;\n"
               "return (int)f;\n"),
            .exit_code = 7,
        },
        {
            "constexpr: cast float to int", __LINE__,
            SVI("constexpr int a = (int)3.7f;\n"
               "return a;\n"),
            .exit_code = 3,
        },
        {
            "constexpr: float arithmetic", __LINE__,
            SVI("constexpr float a = 2.5f + 1.5f;\n"
               "return (int)a;\n"),
            .exit_code = 4,
        },
        {
            "constexpr: double arithmetic", __LINE__,
            SVI("constexpr double a = 10.0 - 3.0;\n"
               "return (int)a;\n"),
            .exit_code = 7,
        },
        {
            "constexpr: float comparison", __LINE__,
            SVI("constexpr int a = (1.5f < 2.5f);\n"
               "return a;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: unsigned arithmetic", __LINE__,
            SVI("constexpr unsigned a = 10u + 32u;\n"
               "return (int)a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: unsigned long long", __LINE__,
            SVI("constexpr unsigned long long a = 100ull * 2ull;\n"
               "return (int)a;\n"),
            .exit_code = 200,
        },
        {
            "constexpr: logical and or", __LINE__,
            SVI("constexpr int a = (1 && 1);\n"
               "constexpr int b = (1 && 0);\n"
               "constexpr int c = (0 || 1);\n"
               "constexpr int d = (0 || 0);\n"
               "return a * 1000 + b * 100 + c * 10 + d;\n"),
            .exit_code = 1010,
        },
        {
            "constexpr: enum value", __LINE__,
            SVI("enum { A = 2 + 3, B = A * 2 };\n"
               "return B;\n"),
            .exit_code = 10,
        },
        {
            "constexpr: sizeof in constexpr", __LINE__,
            SVI("constexpr int a = sizeof(int) + sizeof(char);\n"
               "return a;\n"),
            .exit_code = 5,
        },
        {
            "constexpr: float negation", __LINE__,
            SVI("constexpr float f = -3.0f;\n"
               "return (int)f + 100;\n"),
            .exit_code = 97,
        },
        {
            "constexpr: double negation", __LINE__,
            SVI("constexpr double d = -7.0;\n"
               "return (int)d + 100;\n"),
            .exit_code = 93,
        },
        {
            "constexpr: float logical not", __LINE__,
            SVI("constexpr int a = !0.0f;\n"
               "constexpr int b = !1.0f;\n"
               "return a * 10 + b;\n"),
            .exit_code = 10,
        },
        {
            "constexpr: long long arithmetic", __LINE__,
            SVI("constexpr long long a = 100ll + 200ll;\n"
               "return (int)a;\n"),
            .exit_code = 300,
        },
        {
            "static_assert with arithmetic", __LINE__,
            SVI("_Static_assert(3 * 4 == 12, \"mul\");\n"
               "_Static_assert(10 / 2 == 5, \"div\");\n"
               "_Static_assert(17 % 5 == 2, \"mod\");\n"
               "_Static_assert((0xFF & 0x0F) == 0x0F, \"and\");\n"
               "_Static_assert((0xF0 | 0x0F) == 0xFF, \"or\");\n"
               "_Static_assert((0xFF ^ 0x0F) == 0xF0, \"xor\");\n"
               "_Static_assert((1 << 4) == 16, \"shl\");\n"
               "_Static_assert((256 >> 4) == 16, \"shr\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: long long ops", __LINE__,
            SVI("constexpr long long a = 1000000000ll * 3ll;\n"
               "constexpr long long b = a / 1000000ll;\n"
               "constexpr long long c = a % 1000000000ll;\n"
               "constexpr long long d = a & 0xFFll;\n"
               "constexpr long long e = 0ll | 0xFFll;\n"
               "constexpr long long f = 0xFFll ^ 0x0Fll;\n"
               "constexpr long long g = 1ll << 32;\n"
               "constexpr long long h = g >> 16;\n"
               "_Static_assert(a == 3000000000ll, \"\");\n"
               "_Static_assert(b == 3000ll, \"\");\n"
               "_Static_assert(g == 4294967296ll, \"\");\n"
               "_Static_assert(h == 65536ll, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: long long comparisons", __LINE__,
            SVI("_Static_assert(1ll < 2ll, \"\");\n"
               "_Static_assert(2ll > 1ll, \"\");\n"
               "_Static_assert(3ll <= 3ll, \"\");\n"
               "_Static_assert(3ll >= 3ll, \"\");\n"
               "_Static_assert(5ll == 5ll, \"\");\n"
               "_Static_assert(5ll != 6ll, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: unsigned long long ops", __LINE__,
            SVI("constexpr unsigned long long a = 10000000000ull * 3ull;\n"
               "constexpr unsigned long long b = a / 1000000ull;\n"
               "constexpr unsigned long long c = a % 1000000000ull;\n"
               "constexpr unsigned long long d = 0xFFFFFFFFFFFFFFFFull & 0xFFull;\n"
               "constexpr unsigned long long e = 0xF0ull | 0x0Full;\n"
               "constexpr unsigned long long f = 0xFFull ^ 0x0Full;\n"
               "constexpr unsigned long long g = 1ull << 63;\n"
               "constexpr unsigned long long h = g >> 32;\n"
               "_Static_assert(a == 30000000000ull, \"\");\n"
               "_Static_assert(d == 0xFFull, \"\");\n"
               "_Static_assert(e == 0xFFull, \"\");\n"
               "_Static_assert(f == 0xF0ull, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: unsigned long long comparisons", __LINE__,
            SVI("_Static_assert(1ull < 2ull, \"\");\n"
               "_Static_assert(2ull > 1ull, \"\");\n"
               "_Static_assert(3ull <= 3ull, \"\");\n"
               "_Static_assert(3ull >= 3ull, \"\");\n"
               "_Static_assert(5ull == 5ull, \"\");\n"
               "_Static_assert(5ull != 6ull, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: unsigned negation", __LINE__,
            SVI("constexpr unsigned a = -1u;\n"
               "return a > 1000000u;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: cast double to float", __LINE__,
            SVI("constexpr float f = (float)3.14;\n"
               "return (int)(f * 100);\n"),
            .exit_code = 314,
        },
        {
            "constexpr: cast int to long long", __LINE__,
            SVI("constexpr long long a = (long long)42;\n"
               "return (int)a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: type introspection in static_assert", __LINE__,
            SVI("_Static_assert((int).is_integer, \"\");\n"
               "_Static_assert((float).is_float, \"\");\n"
               "_Static_assert((int).is_arithmetic, \"\");\n"
               "_Static_assert((int*).is_pointer, \"\");\n"
               "_Static_assert((int).is_signed, \"\");\n"
               "_Static_assert((unsigned).is_unsigned, \"\");\n"
               "_Static_assert(!(int).is_float, \"\");\n"
               "_Static_assert(!(int).is_pointer, \"\");\n"
               "_Static_assert((const int).is_const, \"\");\n"
               "_Static_assert(!(int).is_volatile, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: type introspection struct/enum", __LINE__,
            SVI("struct S { int x; };\n"
               "enum E { A };\n"
               "union U { int x; };\n"
               "_Static_assert((struct S).is_struct, \"\");\n"
               "_Static_assert((union U).is_union, \"\");\n"
               "_Static_assert((enum E).is_enum, \"\");\n"
               "_Static_assert((int[3]).is_array, \"\");\n"
               "_Static_assert((int[:]).is_slice, \"\");\n"
               "_Static_assert((int(int)).is_function, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: type sizeof/alignof", __LINE__,
            SVI("_Static_assert((int).sizeof_ == 4, \"\");\n"
               "_Static_assert((char).sizeof_ == 1, \"\");\n"
               "_Static_assert((int).alignof_ == 4, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: float mul div", __LINE__,
            SVI("constexpr float a = 3.0f * 4.0f;\n"
               "constexpr float b = 12.0f / 3.0f;\n"
               "return (int)a + (int)b;\n"),
            .exit_code = 16,
        },
        {
            "constexpr: double mul div", __LINE__,
            SVI("constexpr double a = 3.0 * 4.0;\n"
               "constexpr double b = 12.0 / 3.0;\n"
               "return (int)a + (int)b;\n"),
            .exit_code = 16,
        },
        {
            "constexpr: explicit cast int to double", __LINE__,
            SVI("constexpr double d = (double)42;\n"
               "return (int)d;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: explicit cast double to float", __LINE__,
            SVI("constexpr float f = (float)3.14;\n"
               "return (int)(f * 100.0f);\n"),
            .exit_code = 314,
        },
        {
            "constexpr: explicit cast float to double", __LINE__,
            SVI("constexpr double d = (double)2.5f;\n"
               "return (int)(d * 10.0);\n"),
            .exit_code = 25,
        },
        {
            "int128: unsigned ge", __LINE__,
            SVI("unsigned __int128 a = 10;\n"
               "unsigned __int128 b = 10;\n"
               "unsigned __int128 c = 5;\n"
               "return (a >= b) + (a >= c);\n"),
            .exit_code = 2,
        },
        {
            "constexpr: float comparison all ops", __LINE__,
            SVI("_Static_assert(1.0f == 1.0f, \"\");\n"
               "_Static_assert(1.0f != 2.0f, \"\");\n"
               "_Static_assert(1.0f < 2.0f, \"\");\n"
               "_Static_assert(2.0f > 1.0f, \"\");\n"
               "_Static_assert(1.0f <= 1.0f, \"\");\n"
               "_Static_assert(1.0f >= 1.0f, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: double comparison all ops", __LINE__,
            SVI("_Static_assert(1.0 == 1.0, \"\");\n"
               "_Static_assert(1.0 != 2.0, \"\");\n"
               "_Static_assert(1.0 < 2.0, \"\");\n"
               "_Static_assert(2.0 > 1.0, \"\");\n"
               "_Static_assert(1.0 <= 1.0, \"\");\n"
               "_Static_assert(1.0 >= 1.0, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "__func__", __LINE__,
            SVI("int check(void){\n"
               "    const char* n = __func__;\n"
               "    return n[0] == 'c' && n[1] == 'h';\n"
               "}\n"
               "return check();\n"),
            .exit_code = 1,
        },
        {
            "constexpr: unsigned int ops", __LINE__,
            SVI("constexpr unsigned a = 100u + 50u;\n"
               "constexpr unsigned b = 100u - 30u;\n"
               "constexpr unsigned c = 10u * 5u;\n"
               "constexpr unsigned d = 42u / 6u;\n"
               "constexpr unsigned e = 17u % 5u;\n"
               "constexpr unsigned f = 0xFFu & 0x0Fu;\n"
               "constexpr unsigned g = 0xF0u | 0x0Fu;\n"
               "constexpr unsigned h = 0xFFu ^ 0x0Fu;\n"
               "constexpr unsigned i = 1u << 4;\n"
               "constexpr unsigned j = 256u >> 4;\n"
               "return (int)(a - b - c + d + e + f + g - h + i - j);\n"),
            // 150 - 70 - 50 + 7 + 2 + 15 + 255 - 240 + 16 - 16 = 69
            .exit_code = 69,
        },
        {
            "constexpr: unsigned comparisons", __LINE__,
            SVI("_Static_assert(1u < 2u, \"\");\n"
               "_Static_assert(2u > 1u, \"\");\n"
               "_Static_assert(1u <= 1u, \"\");\n"
               "_Static_assert(1u >= 1u, \"\");\n"
               "_Static_assert(1u == 1u, \"\");\n"
               "_Static_assert(1u != 2u, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: int sub mul div", __LINE__,
            SVI("constexpr int a = 100 - 58;\n"
               "constexpr int b = 6 * 7;\n"
               "constexpr int c = 84 / 2;\n"
               "_Static_assert(a == 42, \"\");\n"
               "_Static_assert(b == 42, \"\");\n"
               "_Static_assert(c == 42, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: unsigned long long negation", __LINE__,
            SVI("constexpr unsigned long long a = -1ull;\n"
               "return a > 100ull;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: cast unsigned to signed", __LINE__,
            SVI("constexpr int a = (int)42u;\n"
               "return a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: cast long long to int", __LINE__,
            SVI("constexpr int a = (int)42ll;\n"
               "return a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: unary plus", __LINE__,
            SVI("constexpr int a = +42;\n"
               "return a;\n"),
            .exit_code = 42,
        },
        {
            "designated arg: named", __LINE__,
            SVI("int add(int a, int b){ return a * 10 + b; }\n"
               "return add(.b = 2, .a = 3);\n"),
            .exit_code = 32,
        },
        {
            "designated arg: positional", __LINE__,
            SVI("int add(int a, int b){ return a * 10 + b; }\n"
               "return add([1] = 2, [0] = 3);\n"),
            .exit_code = 32,
        },
        {
            "designated arg: mixed", __LINE__,
            SVI("int f(int a, int b, int c){ return a * 100 + b * 10 + c; }\n"
               "return f(1, .c = 3, .b = 2);\n"),
            .exit_code = 123,
        },
        {
            "pragma pack basic", __LINE__,
            SVI("#pragma pack(1)\n"
               "struct S { char a; int b; };\n"
               "#pragma pack()\n"
               "return sizeof(struct S);\n"),
            .exit_code = 5,
        },
        {
            "pragma pack push pop", __LINE__,
            SVI("#pragma pack(push, 1)\n"
               "struct S1 { char a; int b; };\n"
               "#pragma pack(pop)\n"
               "struct S2 { char a; int b; };\n"
               "return sizeof(struct S1) * 10 + (sizeof(struct S2) >= 5 ? sizeof(struct S2) : 0);\n"),
            .exit_code = 58,
        },
        {
            "string init char array", __LINE__,
            SVI("char s[6] = \"hello\";\n"
               "return s[0] + s[4];\n"),
            .exit_code = 'h' + 'o',
        },
        {
            "string init wchar array", __LINE__,
            SVI("int s[] = L\"abc\";\n"
               "return s[0] + s[2];\n"),
            .exit_code = 'a' + 'c',
        },
        {
            "empty braced slice init", __LINE__,
            SVI("const char global[:] = {};\n"
               "int check(void){\n"
               "  int local[:] = {};\n"
               "  const char literal[:] = (const char[:]){};\n"
               "  return global.data == nullptr && global.count == 0\n"
               "      && local.data == nullptr && local.count == 0\n"
               "      && literal.data == nullptr && literal.count == 0;\n"
               "}\n"
               "return check();\n"),
            .exit_code = 1,
        },
        {
            "empty braced slice subobjects overwrite previous values", __LINE__,
            SVI("int values[] = {1,2};\n"
               "struct S { int s[:]; int tail; };\n"
               "struct S s = {.s = values, .s = {}, .tail = 7};\n"
               "int slices[2][:] = {[0] = values, [0] = {}, [1] = {}};\n"
               "return s.s.data == nullptr && s.s.count == 0 && s.tail == 7\n"
               "    && slices[0].data == nullptr && slices[0].count == 0\n"
               "    && slices[1].data == nullptr && slices[1].count == 0;\n"),
            .exit_code = 1,
        },
        {
            "empty braced object types", __LINE__,
            SVI("enum E { ONE = 1 }; typedef int V __attribute__((vector_size(8)));\n"
               "int i = {}; double d = {}; enum E e = {}; int* p = {};\n"
               "int (*fp)(void) = {}; typeof(nullptr) n = {};\n"
               "_Any a = {}; _Type t = {}; _Module m = {}; _SrcLoc loc = {};\n"
               "struct S {int x;} s = {}; union U {int x;} u = {};\n"
               "int array[2] = {}; V v = {}; _Atomic(int) ai = {};\n"
               "return i == 0 && d == 0 && e == 0 && p == nullptr && fp == nullptr\n"
               "    && n == nullptr && a.type.is_invalid && t.is_invalid\n"
               "    && m == nullptr && loc == nullptr && s.x == 0 && u.x == 0\n"
               "    && array[0] == 0 && array[1] == 0 && v[0] == 0 && v[1] == 0 && ai == 0;\n"),
            .exit_code = 1,
        },
        {
            "zero braced object types", __LINE__,
            SVI("enum E { ONE = 1 }; typedef int V __attribute__((vector_size(8)));\n"
               "int i = {0}; double d = {0}; enum E e = {0}; int* p = {0};\n"
               "int (*fp)(void) = {0}; typeof(nullptr) n = {0};\n"
               "_Any a = {0}; _Type t = {0}; _Module m = {0}; _SrcLoc loc = {0};\n"
               "struct S {int x;} s = {0}; union U {int x;} u = {0};\n"
               "int array[2] = {0}; V v = {0}; _Atomic(int) ai = {0};\n"
               "return i == 0 && d == 0 && e == 0 && p == nullptr && fp == nullptr\n"
               "    && n == nullptr && a.type == int && t.is_invalid\n"
               "    && m == nullptr && loc == nullptr && s.x == 0 && u.x == 0\n"
               "    && array[0] == 0 && array[1] == 0 && v[0] == 0 && v[1] == 0 && ai == 0;\n"),
            .exit_code = 1,
        },
        {
            "empty braced scalar init", __LINE__,
            SVI("int x = {};\n"
               "return x;\n"),
            .exit_code = 0,
        },
        {
            "_Countof", __LINE__,
            SVI("int arr[7];\n"
               "return _Countof(arr);\n"),
            .exit_code = 7,
        },
        {
            "_Countof no parens", __LINE__,
            SVI("int arr[7];\n"
               "return _Countof arr;\n"),
            .exit_code = 7,
        },
        {
            "_Countof type", __LINE__,
            SVI("return _Countof(int[7]);\n"),
            .exit_code = 7,
        },
        {
            "_Countof compound literal", __LINE__,
            SVI("return _Countof(int[7]){0};\n"),
            .exit_code = 7,
        },
        {
            "_Countof vla", __LINE__,
            SVI("int x = 7;\n"
                "return _Countof(int[x]);\n"),
            .exit_code = 7,
            .skip = 1,
        },
        {
            "_Countof expression statement", __LINE__,
            SVI("int arr[7];\n"
                "_Countof(arr);\n"
                "return _Countof(arr);\n"),
            .exit_code = 7,
        },
        {
            "alignof struct", __LINE__,
            SVI("struct S { char c; int i; };\n"
               "return _Alignof(struct S);\n"),
            .exit_code = 4,
        },
        {
            "alignas member raises struct alignment", __LINE__,
            SVI("struct S { _Alignas(16) int x; int y; };\n"
               "return _Alignof(struct S);\n"),
            .exit_code = 16,
        },
        {
            "alignas member pads struct size", __LINE__,
            SVI("struct S { _Alignas(16) int x; int y; };\n"
               "return (int)sizeof(struct S);\n"),
            .exit_code = 16,
        },
        {
            "alignas member on later field", __LINE__,
            SVI("struct S { int a; _Alignas(16) int b; };\n"
               "return (int)(_Alignof(struct S) * 100 + sizeof(struct S) + ((char*)&((struct S*)0)->b - (char*)0));\n"),
            .exit_code = 16 * 100 + 32 + 16,
        },
        {
            "alignas pointer member raises struct alignment", __LINE__,
            SVI("struct S { _Alignas(16) int* x; int y; };\n"
               "return (int)(_Alignof(struct S) * 100 + sizeof(struct S));\n"),
            .exit_code = 16 * 100 + 16,
        },
        {
            "alignas self-referential pointer member", __LINE__,
            SVI("struct S { _Alignas(16) struct S* next; int y; };\n"
               "return (int)(_Alignof(struct S) * 100 + sizeof(struct S));\n"),
            .exit_code = 16 * 100 + 16,
        },
        {
            "alignas typedef self-referential pointer member", __LINE__,
            SVI("typedef struct S S;\n"
               "struct S { _Alignas(16) S* next; int y; };\n"
               "return (int)(_Alignof(struct S) * 100 + sizeof(struct S));\n"),
            .exit_code = 16 * 100 + 16,
        },
        {
            // mirrors CcField's packing: a 17-bit field sharing a 32-bit
            // storage unit with 15 low bits, so storing 16 sets high bits
            "wide bitfield in shared storage unit round-trips", __LINE__,
            SVI("struct F { unsigned bitwidth:7, bitoffset:6, is_method:1, is_bitfield:1, alignment:17; };\n"
               "struct F f = {0};\n"
               "f.alignment = 16;\n"
               "f.bitwidth = 5;\n"
               "return (int)(f.alignment * 100 + f.bitwidth);\n"),
            .exit_code = 16 * 100 + 5,
        },
        {
            "wide bitfield read back after full write", __LINE__,
            SVI("struct F { unsigned bitwidth:7, bitoffset:6, is_method:1, is_bitfield:1, alignment:17; };\n"
               "struct F f;\n"
               "f.bitwidth = 3; f.bitoffset = 2; f.is_method = 1; f.is_bitfield = 0; f.alignment = 16;\n"
               "unsigned ok = f.alignment == 16 && f.bitwidth == 3 && f.bitoffset == 2 && f.is_method == 1 && f.is_bitfield == 0;\n"
               "return (int)ok;\n"),
            .exit_code = 1,
        },
        {
            "alignof union", __LINE__,
            SVI("union U { char c; int i; };\n"
               "return _Alignof(union U);\n"),
            .exit_code = 4,
        },
        {
            "alignof enum", __LINE__,
            SVI("enum E: char { X };\n"
               "return _Alignof(enum E);\n"),
            .exit_code = 1,
        },
        {
            "unicode escape in string", __LINE__,
            SVI("const char* s = \"\\u0041\\u0042\";\n"
               "return s[0] + s[1];\n"),
            .exit_code = 'A' + 'B',
        },
        {
            "hex escape in string", __LINE__,
            SVI("const char* s = \"\\x41\\x42\\x43\";\n"
               "return s[0] + s[1] + s[2];\n"),
            .exit_code = 'A' + 'B' + 'C',
        },
        {
            "octal escape in string", __LINE__,
            SVI("const char* s = \"\\101\\102\";\n"
               "return s[0] + s[1];\n"),
            .exit_code = 'A' + 'B',
        },
        {
            "u8 string with unicode escape", __LINE__,
            SVI("unsigned char s[] = u8\"\\u00C0\";\n"
               "return s[0];\n"),
            .exit_code = 0xC3, // UTF-8 encoding of U+00C0 first byte
        },
        {
            "U string with high codepoint", __LINE__,
            SVI("unsigned int s[] = U\"\\U0001F600\";\n"
               "return s[0] == 0x1F600;\n"),
            .exit_code = 1,
        },
        {
            "u string with BMP char", __LINE__,
            SVI("unsigned short s[] = u\"\\u00E9\";\n"
               "return s[0];\n"),
            .exit_code = 0xE9,
        },
        {
            "L string with unicode escape", __LINE__,
            SVI("int s[] = L\"\\u00E9\";\n"
               "return s[0];\n"),
            .exit_code = 0xE9,
        },
        {
            "multichar literal", __LINE__,
            SVI("int x = 'AB';\n"
               "return x != 0;\n"),
            .exit_code = 1,
        },
        {
            "sizeof struct with method", __LINE__,
            SVI("struct S {\n"
               "    int x;\n"
               "    int get(struct S* self){ return self->x; }\n"
               "};\n"
               "return sizeof(struct S) == sizeof(int);\n"),
            .exit_code = 1,
        },
        {
            "call struct method", __LINE__,
            SVI("struct S {\n"
               "    int x;\n"
               "    int get(struct S* self){ return self->x; }\n"
               "};\n"
               "struct S s = {42};\n"
               "return s.get();\n"),
            .exit_code = 42,
        },
        {
            "constexpr: int division", __LINE__,
            SVI("constexpr int a = 100 / 3;\n"
               "return a;\n"),
            .exit_code = 33,
        },
        {
            "constexpr: int modulo", __LINE__,
            SVI("constexpr int a = 100 % 3;\n"
               "return a;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: int shifts", __LINE__,
            SVI("constexpr int a = 5 << 2;\n"
               "constexpr int b = 20 >> 2;\n"
               "return a + b;\n"),
            .exit_code = 25,
        },
        {
            "constexpr: int bitwise", __LINE__,
            SVI("constexpr int a = 0xF & 0x3;\n"
               "constexpr int b = 0x8 | 0x4;\n"
               "constexpr int c = 0xF ^ 0x3;\n"
               "return a + b + c;\n"),
            .exit_code = 3 + 12 + 12,
        },
        {
            "constexpr: long long division", __LINE__,
            SVI("constexpr long long a = 1000000000000ll / 3ll;\n"
               "return (int)(a % 1000);\n"),
            .exit_code = 333,
        },
        {
            "constexpr: long long modulo", __LINE__,
            SVI("constexpr long long a = 1000000000000ll % 7ll;\n"
               "return (int)a;\n"),
            .exit_code = (int)(1000000000000ll % 7ll),
        },
        {
            "constexpr: unsigned long long division", __LINE__,
            SVI("constexpr unsigned long long a = 1000000000000ull / 3ull;\n"
               "return (int)(a % 1000);\n"),
            .exit_code = 333,
        },
        {
            "constexpr: unsigned long long modulo", __LINE__,
            SVI("constexpr unsigned long long a = 1000000000000ull % 7ull;\n"
               "return (int)a;\n"),
            .exit_code = (int)(1000000000000ull % 7ull),
        },
        {
            "constexpr: unsigned int division", __LINE__,
            SVI("constexpr unsigned a = 100u / 3u;\n"
               "return (int)a;\n"),
            .exit_code = 33,
        },
        {
            "constexpr: unsigned int modulo", __LINE__,
            SVI("constexpr unsigned a = 100u % 3u;\n"
               "return (int)a;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection fields", __LINE__,
            SVI("struct S { int a; int b; int c; };\n"
               "_Static_assert((struct S).fields == 3, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection enumerators", __LINE__,
            SVI("enum E { A, B, C };\n"
               "_Static_assert((enum E).enumerators == 3, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection param_count", __LINE__,
            SVI("_Static_assert((int(int, float)).param_count == 2, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection return_type", __LINE__,
            SVI("_Static_assert((int(void)).return_type.is_integer, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection param_type", __LINE__,
            SVI("_Static_assert((int(int, float)).param_type(1).is_float, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection element_type", __LINE__,
            SVI("_Static_assert((int[5]).element_type.is_integer, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection count", __LINE__,
            SVI("_Static_assert((int[7]).count == 7, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection pointee", __LINE__,
            SVI("_Static_assert((int*).pointee.is_integer, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection unqual", __LINE__,
            SVI("_Static_assert(!(const int).unqual.is_const, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection is_callable", __LINE__,
            SVI("_Static_assert((int(*)(int)).is_callable, \"\");\n"
               "_Static_assert(!(int).is_callable, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection is_variadic", __LINE__,
            SVI("_Static_assert((int(int, ...)).is_variadic, \"\");\n"
               "_Static_assert(!(int(int)).is_variadic, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection is_incomplete", __LINE__,
            SVI("struct Fwd;\n"
               "_Static_assert((struct Fwd).is_incomplete, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection underlying_type", __LINE__,
            SVI("enum E : unsigned char { X };\n"
               "_Static_assert((enum E).underlying_type.is_unsigned, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection is_castable_to", __LINE__,
            SVI("_Static_assert((int).is_castable_to(float), \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: type introspection is_callable_with", __LINE__,
            SVI("_Static_assert((int(int)).is_callable_with(int), \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "atomic: __atomic_load (3-arg)", __LINE__,
            SVI("int x = 42;\n"
               "int y = 0;\n"
               "__atomic_load(&x, &y, __ATOMIC_SEQ_CST);\n"
               "return y;\n"),
            .exit_code = 42,
        },
        {
            "atomic: __atomic_store (3-arg)", __LINE__,
            SVI("int x = 0;\n"
               "int val = 99;\n"
               "__atomic_store(&x, &val, __ATOMIC_SEQ_CST);\n"
               "return x;\n"),
            .exit_code = 99,
        },
        {
            "atomic: __atomic_exchange (4-arg)", __LINE__,
            SVI("int x = 10;\n"
               "int val = 20;\n"
               "int old = 0;\n"
               "__atomic_exchange(&x, &val, &old, __ATOMIC_SEQ_CST);\n"
               "return old * 10 + x;\n"),
            .exit_code = 120,
        },
        {
            "atomic: __atomic_thread_fence", __LINE__,
            SVI("__atomic_thread_fence(__ATOMIC_SEQ_CST);\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "atomic: __atomic_signal_fence", __LINE__,
            SVI("__atomic_signal_fence(__ATOMIC_SEQ_CST);\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "_InterlockedExchange16", __LINE__,
            SVI("short _InterlockedExchange16(short volatile*, short);\n"
               "short volatile x = 5;\n"
               "short old = _InterlockedExchange16(&x, 10);\n"
               "return old + x;\n"),
            .exit_code = 15,
        },
        {
            "_InterlockedExchangeAdd16", __LINE__,
            SVI("short _InterlockedExchangeAdd16(short volatile*, short);\n"
               "short volatile x = 10;\n"
               "short old = _InterlockedExchangeAdd16(&x, 5);\n"
               "return old + x;\n"),
            .exit_code = 25,
        },
        {
            "_InterlockedCompareExchange16", __LINE__,
            SVI("short _InterlockedCompareExchange16(short volatile*, short, short);\n"
               "short volatile x = 10;\n"
               "short old = _InterlockedCompareExchange16(&x, 42, 10);\n"
               "return old + x;\n"),
            .exit_code = 52,
        },
        {
            "_InterlockedOr8", __LINE__,
            SVI("char _InterlockedOr8(char volatile*, char);\n"
               "char volatile x = 0xF0;\n"
               "char old = _InterlockedOr8(&x, 0x0F);\n"
               "return (unsigned char)old + (unsigned char)x;\n"),
            .exit_code = 0xF0 + 0xFF,
        },
        {
            "_InterlockedXor8", __LINE__,
            SVI("char _InterlockedXor8(char volatile*, char);\n"
               "char volatile x = (char)0xFF;\n"
               "char old = _InterlockedXor8(&x, 0x0F);\n"
               "return (unsigned char)x;\n"),
            .exit_code = 0xF0,
        },
        {
            "_InterlockedAnd8", __LINE__,
            SVI("char _InterlockedAnd8(char volatile*, char);\n"
               "char volatile x = (char)0xFF;\n"
               "char old = _InterlockedAnd8(&x, 0x0F);\n"
               "return (unsigned char)x;\n"),
            .exit_code = 0x0F,
        },
        {
            "_InterlockedIncrement16", __LINE__,
            SVI("short _InterlockedIncrement16(short volatile*);\n"
               "short volatile x = 10;\n"
               "short r = _InterlockedIncrement16(&x);\n"
               "return r + x;\n"),
            .exit_code = 22,
        },
        {
            "_InterlockedDecrement16", __LINE__,
            SVI("short _InterlockedDecrement16(short volatile*);\n"
               "short volatile x = 10;\n"
               "short r = _InterlockedDecrement16(&x);\n"
               "return r + x;\n"),
            .exit_code = 18,
        },
        {
            "_InterlockedIncrement64", __LINE__,
            SVI("long long _InterlockedIncrement64(long long volatile*);\n"
               "long long volatile x = 10;\n"
               "long long r = _InterlockedIncrement64(&x);\n"
               "return (int)(r + x);\n"),
            .exit_code = 22,
        },
        {
            "_InterlockedOr16", __LINE__,
            SVI("short _InterlockedOr16(short volatile*, short);\n"
               "short volatile x = 0x00F0;\n"
               "short old = _InterlockedOr16(&x, 0x000F);\n"
               "return (int)(unsigned short)x;\n"),
            .exit_code = 0xFF,
        },
        {
            "_InterlockedAnd16", __LINE__,
            SVI("short _InterlockedAnd16(short volatile*, short);\n"
               "short volatile x = (short)0x00FF;\n"
               "short old = _InterlockedAnd16(&x, 0x000F);\n"
               "return (int)(unsigned short)x;\n"),
            .exit_code = 0x0F,
        },
        {
            "_InterlockedXor16", __LINE__,
            SVI("short _InterlockedXor16(short volatile*, short);\n"
               "short volatile x = (short)0x00FF;\n"
               "short old = _InterlockedXor16(&x, 0x000F);\n"
               "return (int)(unsigned short)x;\n"),
            .exit_code = 0xF0,
        },
        {
            "_InterlockedOr64", __LINE__,
            SVI("long long _InterlockedOr64(long long volatile*, long long);\n"
               "long long volatile x = 0xF0;\n"
               "long long old = _InterlockedOr64(&x, 0x0F);\n"
               "return (int)x;\n"),
            .exit_code = 0xFF,
        },
        {
            "_InterlockedAnd64", __LINE__,
            SVI("long long _InterlockedAnd64(long long volatile*, long long);\n"
               "long long volatile x = 0xFF;\n"
               "long long old = _InterlockedAnd64(&x, 0x0F);\n"
               "return (int)x;\n"),
            .exit_code = 0x0F,
        },
        {
            "_InterlockedXor64", __LINE__,
            SVI("long long _InterlockedXor64(long long volatile*, long long);\n"
               "long long volatile x = 0xFF;\n"
               "long long old = _InterlockedXor64(&x, 0x0F);\n"
               "return (int)x;\n"),
            .exit_code = 0xF0,
        },
        {
            "_InterlockedExchangeAdd64", __LINE__,
            SVI("long long _InterlockedExchangeAdd64(long long volatile*, long long);\n"
               "long long volatile x = 10;\n"
               "long long old = _InterlockedExchangeAdd64(&x, 5);\n"
               "return (int)(old + x);\n"),
            .exit_code = 25,
        },
        {
            "_InterlockedExchange64", __LINE__,
            SVI("long long _InterlockedExchange64(long long volatile*, long long);\n"
               "long long volatile x = 10;\n"
               "long long old = _InterlockedExchange64(&x, 42);\n"
               "return (int)(old + x);\n"),
            .exit_code = 52,
        },
        {
            "_InterlockedCompareExchange8", __LINE__,
            SVI("char _InterlockedCompareExchange8(char volatile*, char, char);\n"
               "char volatile x = 10;\n"
               "char old = _InterlockedCompareExchange8(&x, 42, 10);\n"
               "return old + x;\n"),
            .exit_code = 52,
        },
        {
            "pointer subtraction with array decay", __LINE__,
            SVI("int arr[10];\n"
               "int *p = arr + 5;\n"
               "return (int)(p - arr);\n"),
            .exit_code = 5,
        },
        {
            "FUCS opaque builtins", __LINE__,
            SVI("int module_ok(_Module m){ return (int)m.type_count; }\n"
               "int loc_ok(_SrcLoc loc){ return loc == nullptr; }\n"
               "return __compile(\"typedef int T;\", \"\").module_ok() + ((_SrcLoc)nullptr).loc_ok();\n"),
            .exit_code = 2,
        },
        {
            "FUCS atomic aggregates", __LINE__,
            SVI("struct S { int x; }; union U { int x; };\n"
               "int read_s(struct S s){ return s.x; }\n"
               "int read_u(union U u){ return u.x; }\n"
               "int ptr_s(_Atomic(struct S)* s){ return read_s(*s); }\n"
               "_Atomic(struct S) s = {3}; _Atomic(union U) u = {4};\n"
               "return s.read_s() + u.read_u() + s.ptr_s();\n"),
            .exit_code = 10,
        },
        {
            "FUCS array and function decay", __LINE__,
            SVI("int first(const int* p){ return *p; }\n"
               "int value(void){ return 7; }\n"
               "int invoke(int (*f)(void)){ return f(); }\n"
               "int a[] = {5};\n"
               "return a.first() + value.invoke();\n"),
            .exit_code = 12,
        },
        {
            "FUCS direct pointer conversions precede dereference", __LINE__,
            SVI("int truth(_Bool b){ return b; }\n"
               "int boxed(_Any a){ return a.type == int*; }\n"
               "int empty(void* p){ return p == nullptr; }\n"
               "int x = 0; int* p = &x; typeof(nullptr) n = nullptr;\n"
               "return p.truth() + p.boxed() + n.empty();\n"),
            .exit_code = 3,
        },
        {
            "FUCS scalars enums unions vectors and reflection", __LINE__,
            SVI("int number(double x){ return (int)x; }\n"
               "enum E { A = 2 }; union U { int x; };\n"
               "int unpack(union U u){ return u.x; }\n"
               "typedef int V __attribute__((vector_size(8)));\n"
               "int sum(V v){ return v[0] + v[1]; }\n"
               "int type_ok(_Type t){ return t == int; }\n"
               "int any_ok(_Any a){ return a.as(int); }\n"
               "enum E e = A; union U u = {3}; V v = {4,5};\n"
               "_Type t = int; _Any a = 6;\n"
               "return e.number() + u.unpack() + v.sum() + t.type_ok() + a.any_ok();\n"),
            .exit_code = 21,
        },
        {
            "FUCS slices", __LINE__,
            SVI("int size(const char s[:]){ return (int)s.count; }\n"
               "int first(const char (*s)[:]){ return (*s).data[0]; }\n"
               "const char s[:] = \"hello\";\n"
               "return s.size() + (&s).size() + s.first();\n"),
            .exit_code = 114,
        },
        {
            "FUCS basic", __LINE__,
            SVI("struct Vec2 { float x; float y; };\n"
               "float length_sq(struct Vec2* v){ return v->x * v->x + v->y * v->y; }\n"
               "struct Vec2 v = {3.0f, 4.0f};\n"
               "return (int)v.length_sq();\n"),
            .exit_code = 25,
        },
        {
            "FUCS again", __LINE__,
            SVI("struct Vec2 { float x; float y; };\n"
               "float length_sq(struct Vec2 v){ return v->x * v->x + v->y * v->y; }\n"
               "struct Vec2 v = {3.0f, 4.0f};\n"
               "return (int)v.length_sq();\n"),
            .exit_code = 25,
        },
        {
            "FUCS again again", __LINE__,
            SVI("struct Vec2 { float x; float y; };\n"
               "float length_sq(struct Vec2 v){ return v->x * v->x + v->y * v->y; }\n"
               "struct Vec2 v = {3.0f, 4.0f};\n"
               "return (int)(&v).length_sq();\n"),
            .exit_code = 25,
        },
        {
            "pragma typedef auto", __LINE__,
            SVI("#pragma typedef on\n"
               "struct Point { int x; int y; };\n"
               "#pragma typedef off\n"
               "Point p = {3, 4};\n"
               "return p.x + p.y;\n"),
            .exit_code = 7,
        },
        {
            "sizeof incomplete array param", __LINE__,
            SVI("int sum(int arr[], int n){\n"
               "    int s = 0;\n"
               "    for(int i = 0; i < n; i++) s += arr[i];\n"
               "    return s;\n"
               "}\n"
               "int a[] = {1,2,3};\n"
               "return sum(a, 3);\n"),
            .exit_code = 6,
        },
        {
            "ternary: null pointer branches", __LINE__,
            SVI("int x = 1;\n"
               "int *p = x ? &x : (void*)0;\n"
               "return *p;\n"),
            .exit_code = 1,
        },
        {
            "ternary: function pointer decay", __LINE__,
            SVI("int f(void) { return 42; }\n"
               "int (*fp)(void) = 1 ? f : f;\n"
               "return fp();\n"),
            .exit_code = 42,
        },
        {
            "function pointer equality with function", __LINE__,
            SVI("int f(void) { return 1; }\n"
               "int g(void) { return 2; }\n"
               "int (*fp)(void) = f;\n"
               "return (fp == f) + (fp != g);\n"),
            .exit_code = 2,
        },
        {
            "pointer add with enum index", __LINE__,
            SVI("enum { IDX = 2 };\n"
               "int arr[3] = {10, 20, 30};\n"
               "return arr[IDX];\n"),
            .exit_code = 30,
        },
        {
            "float: comparison le ge", __LINE__,
            SVI("float a = 1.5f;\n"
               "float b = 2.5f;\n"
               "return (a <= b) + (b >= a) + (a <= a);\n"),
            .exit_code = 3,
        },
        {
            "double: comparison le ge", __LINE__,
            SVI("double a = 1.5;\n"
               "double b = 2.5;\n"
               "return (a <= b) + (b >= a) + (a <= a);\n"),
            .exit_code = 3,
        },
        {
            "float: mod via cast", __LINE__,
            SVI("float a = 7.5f;\n"
               "int b = (int)a % 3;\n"
               "return b;\n"),
            .exit_code = 1,
        },
        {
            "struct init brace elision", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { struct Inner in; int c; };\n"
               "struct Outer o = {1, 2, 3};\n"
               "return o.in.a + o.in.b + o.c;\n"),
            .exit_code = 6,
        },
        {
            "array init unsized", __LINE__,
            SVI("int arr[] = {10, 20, 30, 40, 50};\n"
               "return sizeof arr / sizeof arr[0];\n"),
            .exit_code = 5,
        },
        {
            "union init first member", __LINE__,
            SVI("union U { int i; float f; };\n"
               "union U u = {42};\n"
               "return u.i;\n"),
            .exit_code = 42,
        },
        {
            "union init designated", __LINE__,
            SVI("union U { int i; char c; };\n"
               "union U u = {.c = 7};\n"
               "return u.c;\n"),
            .exit_code = 7,
        },
        {
            "nested designated init", __LINE__,
            SVI("struct Inner { int x; int y; };\n"
               "struct Outer { struct Inner p; int z; };\n"
               "struct Outer o = {.p.x = 1, .p.y = 2, .z = 3};\n"
               "return o.p.x + o.p.y + o.z;\n"),
            .exit_code = 6,
        },
        {
            "chained array designator", __LINE__,
            SVI("struct S { int arr[3]; };\n"
               "struct S s = {.arr[1] = 42};\n"
               "return s.arr[0] + s.arr[1] + s.arr[2];\n"),
            .exit_code = 42,
        },
        {
            "sizeof long double", __LINE__,
            SVI("return sizeof(long double) >= 8;\n"),
            .exit_code = 1,
        },
        {
            "alignas struct member", __LINE__,
            SVI("struct S { alignas(16) int x; int y; };\n"
               "return sizeof(struct S) >= 16;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: comma expression", __LINE__,
            SVI("constexpr int x = (1, 2, 42);\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "_Alignas with type", __LINE__,
            SVI("_Alignas(double) int x = 42;\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "signed/unsigned type parsing", __LINE__,
            SVI("signed x = -5;\n"
               "unsigned y = 5;\n"
               "return x + (int)y;\n"),
            .exit_code = 0,
        },
        {
            "implicit int return", __LINE__,
            SVI("int f(void);\n"
               "int f(void){ return 42; }\n"
               "return f();\n"),
            .exit_code = 42,
        },
        {
            "sizeof expression not evaluated", __LINE__,
            SVI("int x = 5;\n"
               "int s = sizeof(x++);\n"
               "return x * 10 + s;\n"),
            .exit_code = 54,
        },
        {
            "string literal comparison", __LINE__,
            SVI("const char* a = \"hello\";\n"
               "const char* b = \"hello\";\n"
               "return a[0] == b[0];\n"),
            .exit_code = 1,
        },
        {
            "double to unsigned cast", __LINE__,
            SVI("double d = 42.7;\n"
               "unsigned u = (unsigned)d;\n"
               "return (int)u;\n"),
            .exit_code = 42,
        },
        {
            "unsigned to double cast", __LINE__,
            SVI("unsigned u = 42;\n"
               "double d = (double)u;\n"
               "return (int)d;\n"),
            .exit_code = 42,
        },
        {
            "long double arith", __LINE__,
            SVI("long double a = 3.5L;\n"
               "long double b = 2.5L;\n"
               "return (int)(a + b);\n"),
            .exit_code = 6,
        },
        {
            "array of structs init", __LINE__,
            SVI("struct P { int x; int y; };\n"
               "struct P arr[] = {{1,2},{3,4},{5,6}};\n"
               "return arr[0].x + arr[1].y + arr[2].x;\n"),
            .exit_code = 10,
        },
        {
            "switch: char value", __LINE__,
            SVI("char c = 'B';\n"
               "switch(c){\n"
               "    case 'A': return 1;\n"
               "    case 'B': return 2;\n"
               "    case 'C': return 3;\n"
               "}\n"
               "return 0;\n"),
            .exit_code = 2,
        },
        {
            "nested array init designator", __LINE__,
            SVI("int m[2][3] = {[1] = {4, 5, 6}};\n"
               "return m[0][0] + m[1][0] + m[1][2];\n"),
            .exit_code = 10,
        },
        {
            "enum: implicit conversion to int", __LINE__,
            SVI("enum E { A = 1, B = 2, C = 3 };\n"
               "int x = A + B + C;\n"
               "return x;\n"),
            .exit_code = 6,
        },
        {
            "sizeof enum", __LINE__,
            SVI("enum E { X };\n"
               "return sizeof(enum E) == sizeof(int);\n"),
            .exit_code = 1,
        },
        {
            "constexpr: comma expression in static_assert", __LINE__,
            SVI("_Static_assert((0, 1), \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: double to unsigned cast", __LINE__,
            SVI("constexpr unsigned a = (unsigned)42.7;\n"
               "return (int)a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: unsigned to float cast", __LINE__,
            SVI("constexpr float f = (float)42u;\n"
               "return (int)f;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: long long to unsigned cast", __LINE__,
            SVI("constexpr unsigned a = (unsigned)42ll;\n"
               "return (int)a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: unsigned long long to int cast", __LINE__,
            SVI("constexpr int a = (int)42ull;\n"
               "return a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: char to int cast", __LINE__,
            SVI("constexpr int a = (int)'A';\n"
               "return a;\n"),
            .exit_code = 65,
        },
        {
            "constexpr: float to unsigned long long cast", __LINE__,
            SVI("constexpr unsigned long long a = (unsigned long long)42.5f;\n"
               "return (int)a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: int to unsigned long long cast", __LINE__,
            SVI("constexpr unsigned long long a = (unsigned long long)42;\n"
               "return (int)a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: unsigned to long long cast", __LINE__,
            SVI("constexpr long long a = (long long)42u;\n"
               "return (int)a;\n"),
            .exit_code = 42,
        },
        {
            "constexpr: logical not unsigned", __LINE__,
            SVI("constexpr int a = !0u;\n"
               "constexpr int b = !1u;\n"
               "return a * 10 + b;\n"),
            .exit_code = 10,
        },
        {
            "constexpr: logical not long long", __LINE__,
            SVI("constexpr int a = !0ll;\n"
               "constexpr int b = !1ll;\n"
               "return a * 10 + b;\n"),
            .exit_code = 10,
        },
        {
            "constexpr: logical not unsigned long long", __LINE__,
            SVI("constexpr int a = !0ull;\n"
               "constexpr int b = !1ull;\n"
               "return a * 10 + b;\n"),
            .exit_code = 10,
        },
        {
            "constexpr: logical not double", __LINE__,
            SVI("constexpr int a = !0.0;\n"
               "constexpr int b = !1.0;\n"
               "return a * 10 + b;\n"),
            .exit_code = 10,
        },
        {
            "constexpr: negation long long", __LINE__,
            SVI("constexpr long long a = -42ll;\n"
               "return (int)a + 100;\n"),
            .exit_code = 58,
        },
        {
            "constexpr: negation unsigned long long", __LINE__,
            SVI("constexpr unsigned long long a = -1ull;\n"
               "return a > 100ull;\n"),
            .exit_code = 1,
        },
        {
            "constexpr: bitwise not int", __LINE__,
            SVI("constexpr int a = ~0xFF;\n"
               "return a & 0xFF;\n"),
            .exit_code = 0,
        },
        {
            "constexpr: bitwise not unsigned", __LINE__,
            SVI("constexpr unsigned a = ~0xFFu;\n"
               "return (int)(a & 0xFFu);\n"),
            .exit_code = 0,
        },
        {
            "constexpr: bitwise not long long", __LINE__,
            SVI("constexpr long long a = ~0xFFll;\n"
               "return (int)(a & 0xFFll);\n"),
            .exit_code = 0,
        },
        {
            "constexpr: bitwise not unsigned long long", __LINE__,
            SVI("constexpr unsigned long long a = ~0xFFull;\n"
               "return (int)(a & 0xFFull);\n"),
            .exit_code = 0,
        },
        {
            "static_assert: int32 all binary ops", __LINE__,
            SVI("_Static_assert(100 - 58 == 42, \"\");\n"
               "_Static_assert(6 * 7 == 42, \"\");\n"
               "_Static_assert(84 / 2 == 42, \"\");\n"
               "_Static_assert(85 % 43 == 42, \"\");\n"
               "_Static_assert((0xFF & 0x2A) == 0x2A, \"\");\n"
               "_Static_assert((0x20 | 0x0A) == 0x2A, \"\");\n"
               "_Static_assert((0x3F ^ 0x15) == 0x2A, \"\");\n"
               "_Static_assert((21 << 1) == 42, \"\");\n"
               "_Static_assert((84 >> 1) == 42, \"\");\n"
               "_Static_assert(42 < 43, \"\");\n"
               "_Static_assert(43 > 42, \"\");\n"
               "_Static_assert(42 <= 42, \"\");\n"
               "_Static_assert(42 >= 42, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "u8 string multi-byte char", __LINE__,
            SVI("unsigned char s[] = u8\"\\u00E9\";\n"
               "return s[0] == 0xC3 && s[1] == 0xA9;\n"),
            .exit_code = 1,
        },
        {
            "u8 string 3-byte char", __LINE__,
            SVI("unsigned char s[] = u8\"\\u1234\";\n"
               "return s[0] == 0xE1 && s[1] == 0x88 && s[2] == 0xB4;\n"),
            .exit_code = 1,
        },
        {
            "u8 string 4-byte char", __LINE__,
            SVI("unsigned char s[] = u8\"\\U0001F600\";\n"
               "return s[0] == 0xF0 && s[1] == 0x9F && s[2] == 0x98 && s[3] == 0x80;\n"),
            .exit_code = 1,
        },
        {
            "u string surrogate pair", __LINE__,
            SVI("unsigned short s[] = u\"\\U0001F600\";\n"
               "return s[0] == 0xD83D && s[1] == 0xDE00;\n"),
            .exit_code = 1,
        },
        {
            "L string multi-char", __LINE__,
            SVI("int s[] = L\"\\u00E9\\u1234\";\n"
               "return s[0] == 0xE9 && s[1] == 0x1234;\n"),
            .exit_code = 1,
        },
        {
            "U string high codepoints", __LINE__,
            SVI("unsigned int s[] = U\"\\U0001F600\\u00E9\";\n"
               "return s[0] == 0x1F600 && s[1] == 0xE9;\n"),
            .exit_code = 1,
        },
        {
            "string octal escape", __LINE__,
            SVI("char s[] = \"\\101\\102\\103\";\n"
               "return s[0] == 'A' && s[1] == 'B' && s[2] == 'C';\n"),
            .exit_code = 1,
        },
        {
            "string hex escape", __LINE__,
            SVI("char s[] = \"\\x41\\x42\\x43\";\n"
               "return s[0] == 'A' && s[1] == 'B' && s[2] == 'C';\n"),
            .exit_code = 1,
        },
        {
            "string all basic escapes", __LINE__,
            SVI("char s[] = \"\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\\?\";\n"
               "return s[0] == 7 && s[1] == 8 && s[2] == 12 && s[3] == 10;\n"),
            .exit_code = 1,
        },
        {
            "u16 string hex content", __LINE__,
            SVI("unsigned short s[] = u\"\\x41\\x42\";\n"
               "return s[0] == 0x41 && s[1] == 0x42;\n"),
            .exit_code = 1,
        },
        {
            "u32 string hex content", __LINE__,
            SVI("unsigned int s[] = U\"\\x41\\x42\";\n"
               "return s[0] == 0x41 && s[1] == 0x42;\n"),
            .exit_code = 1,
        },
        {
            "wchar string hex content", __LINE__,
            SVI("int s[] = L\"\\x41\\x42\";\n"
               "return s[0] == 0x41 && s[1] == 0x42;\n"),
            .exit_code = 1,
        },
        {
            "static_assert: uint32 all binary ops", __LINE__,
            SVI("_Static_assert(100u - 58u == 42u, \"\");\n"
               "_Static_assert(6u * 7u == 42u, \"\");\n"
               "_Static_assert(84u / 2u == 42u, \"\");\n"
               "_Static_assert(85u % 43u == 42u, \"\");\n"
               "_Static_assert((0xFFu & 0x2Au) == 0x2Au, \"\");\n"
               "_Static_assert((0x20u | 0x0Au) == 0x2Au, \"\");\n"
               "_Static_assert((0x3Fu ^ 0x15u) == 0x2Au, \"\");\n"
               "_Static_assert((21u << 1) == 42u, \"\");\n"
               "_Static_assert((84u >> 1) == 42u, \"\");\n"
               "_Static_assert(42u < 43u, \"\");\n"
               "_Static_assert(43u > 42u, \"\");\n"
               "_Static_assert(42u <= 42u, \"\");\n"
               "_Static_assert(42u >= 42u, \"\");\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "push_method", __LINE__,
            SVI("(struct Foo {int x;}).push_method(get_x, int(struct Foo* self){return self.x;});\n"
            "return (struct Foo){42}.get_x();\n"),
            .exit_code = 42,
        },
        {
            "prototype enumerator scope does not leak", __LINE__,
            SVI("enum { VALUE = 3 };\n"
                "void foo(enum Kind { VALUE = 7 } x, int (*a)[VALUE]);\n"
                "return VALUE;\n"),
            .exit_code = 3,
        },
        {
            "function definition parameter tag scope", __LINE__,
            SVI("int foo(enum Kind { VALUE = 7 } x) { enum Kind y = x; return y + VALUE; }\n"
                "return foo(7);\n"),
            .exit_code = 14,
        },
        {
            "lambda parameter tag scope", __LINE__,
            SVI("return (int(enum Kind { VALUE = 7 } x) { return x + VALUE; })(7);\n"),
            .exit_code = 14,
        },
        {
            "prototype parameter scope", __LINE__,
            SVI("void foo(int x, int y[sizeof x]);\n"
                "int get(int x, int y[sizeof x]) { return y[0] + x; }\n"
                "int a[4] = {7};\n"
                "return get(3, a);\n"),
            .exit_code = 10,
        },
        {
            "prototype parameter shadowing and scope restoration", __LINE__,
            SVI("typedef char x;\n"
                "void foo(int x, int (*y)[sizeof x]);\n"
                "void foo(int x, int (*y)[sizeof(int)]);\n"
                "x value = 7;\n"
                "return sizeof value;\n"),
            .exit_code = 1,
        },
        {
            "prototype adjusted parameter types", __LINE__,
            SVI("void foo(int a[3], int (*b)[sizeof a], int f(void), int (*c)[sizeof f]);\n"
                "void foo(int *a, int (*b)[sizeof(int*)], int (*f)(void), int (*c)[sizeof(int (*)(void))]);\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "nested prototype parameter scope", __LINE__,
            SVI("void foo(char x, void (*f)(int x, int (*y)[sizeof x]), int (*z)[sizeof x]);\n"
                "void foo(char x, void (*f)(int x, int (*y)[sizeof(int)]), int (*z)[sizeof(char)]);\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "prototype scope extends to end of declarator", __LINE__,
            SVI("int (*foo(int x))[sizeof x];\n"
                "int (*foo(int x))[sizeof(int)];\n"
                "return 0;\n"),
            .exit_code = 0,
        },
        {
            "sizeof vla", __LINE__,
            SVI("int x = 3;\n"
            "return (int)sizeof(int[x]);\n"),
            .exit_code = 12,
            .skip = 1,
        },
        {
            "sizeof vla in array", __LINE__,
            SVI("int x = 3;\n"
            "return (int)sizeof(int[2][x]);\n"),
            .exit_code = 24,
            .skip = 1,
        },
        {
            "vla: object sizeof captures bound", __LINE__,
            SVI("int n = 3;\n"
               "int x[n++];\n"
               "int y[n++];\n"
               "int z[n];\n"
               "return (int)(sizeof x + sizeof y + sizeof z);\n"),
            .exit_code = 48,
            .skip = 1,
        },
        {
            "vla: sizeof type evaluates bound expression", __LINE__,
            SVI("int n = 3;\n"
               "int a = sizeof(int[n++]);\n"
               "int b = sizeof(int[n++]);\n"
               "return a + b + n;\n"),
            .exit_code = 12 + 16 + 5,
            .skip = 1,
        },
        {
            "vla: sizeof object does not re-evaluate bound", __LINE__,
            SVI("int n = 3;\n"
               "int x[n++];\n"
               "int a = sizeof x;\n"
               "int b = sizeof x;\n"
               "return a + b + n;\n"),
            .exit_code = 12 + 12 + 4,
            .skip = 1,
        },
        {
            "vla: subscript storage", __LINE__,
            SVI("int n = 4;\n"
               "int x[n];\n"
               "for(int i = 0; i < n; i++) x[i] = i + 1;\n"
               "return x[0] + x[1] + x[2] + x[3];\n"),
            .exit_code = 10,
            .skip = 1,
        },
        {
            "vla: nested sizeof captures each dimension", __LINE__,
            SVI("int m = 2;\n"
               "int n = 3;\n"
               "int a[m++][n++];\n"
               "return (int)(sizeof a + sizeof a[0] + m * 10 + n);\n"),
            .exit_code = 24 + 12 + 34,
            .skip = 1,
        },
        {
            "vla: nested subscript storage", __LINE__,
            SVI("int m = 2;\n"
               "int n = 3;\n"
               "int a[m][n];\n"
               "a[0][0] = 1;\n"
               "a[0][2] = 3;\n"
               "a[1][0] = 10;\n"
               "a[1][2] = 30;\n"
               "return a[0][0] + a[0][2] + a[1][0] + a[1][2];\n"),
            .exit_code = 44,
            .skip = 1,
        },
        {
            "vla: pointer arithmetic uses captured element size", __LINE__,
            SVI("int n = 3;\n"
               "int a[2][n++];\n"
               "int (*p)[3] = a;\n"
               "return (char*)(p + 1) - (char*)p + n;\n"),
            .exit_code = 12 + 4,
            .skip = 1,
        },
        {
            "vla: typedef captures bound", __LINE__,
            SVI("int n = 3;\n"
               "typedef int A[n++];\n"
               "A x;\n"
               "n = 9;\n"
               "A y;\n"
               "return (int)(sizeof x + sizeof y + n);\n"),
            .exit_code = 12 + 12 + 9,
            .skip = 1,
        },
        {
            "vla: pointer to typedef uses captured bound", __LINE__,
            SVI("int n = 3;\n"
               "typedef int A[n++];\n"
               "A x;\n"
               "A *p = &x;\n"
               "n = 9;\n"
               "return (int)sizeof *p + n;\n"),
            .exit_code = 12 + 9,
            .skip = 1,
        },
        {
            "vla: parameter adjusts to pointer", __LINE__,
            SVI("int sizeof_param(int n, int a[n]){\n"
               "    return sizeof a == sizeof(int*);\n"
               "}\n"
               "int n = 3;\n"
               "int a[n];\n"
               "return sizeof_param(n, a);\n"),
            .exit_code = 1,
            .skip = 1,
        },
        {
            "vla: multidimensional parameter keeps inner bound", __LINE__,
            SVI("int sum(int rows, int cols, int a[rows][cols]){\n"
               "    return a[1][2] + (int)sizeof a[0];\n"
               "}\n"
               "int a[2][3];\n"
               "a[1][2] = 7;\n"
               "return sum(2, 3, a);\n"),
            .exit_code = 7 + 12,
            .skip = 1,
        },
        {
            "slice array", __LINE__,
            SVI("int x[4] = {1,2,3,4};\n"
                "int s1[:] = x[:];\n"
                "int s2[:] = x[1:];\n"
                "int s3[:] = x[:3];\n"
                "int s4[:] = x[2:4];\n"
                "return s1[0] + 10*s2[0] + 100*s3[0]+1000*s4[0];\n"),
            .exit_code = 1 + 10*2 + 100*1+1000*3,
        },
        {
            "slice slice", __LINE__,
            SVI("int a[4] = {1,2,3,4};\n"
                "int x[:] = a[:];\n"
                "int s1[:] = x[:];\n"
                "int s2[:] = x[1:];\n"
                "int s3[:] = x[:3];\n"
                "int s4[:] = x[2:4];\n"
                "return s1[0] + 10*s2[0] + 100*s3[0]+1000*s4[0];\n"),
            .exit_code = 1 + 10*2 + 100*1+1000*3,
        },
        {
            "slice pointer", __LINE__,
            SVI("int a[4] = {1,2,3,4};\n"
                "int *x = a;\n"
                "int s3[:] = x[:3];\n"
                "int s4[:] = x[2:4];\n"
                "return 100*s3[0]+1000*s4[0];\n"),
            .exit_code = 100*1+1000*3,
        },
        {
            "slice string literal", __LINE__,
            SVI("const char s[:] = \"hello\"[:];\n"
                "return (int)s.count + s[0];\n"),
            .exit_code = (int)sizeof "hello" + 'h',
        },
        {
            "discarded slice evaluates both bounds", __LINE__,
            SVI("int i = 0;\n"
                "int a[4] = {1,2,3,4};\n"
                "a[i++:i++ + 2];\n"
                "return i;\n"),
            .exit_code = 2,
        },
        {
            "discarded low slice evaluates bound", __LINE__,
            SVI("int i = 0;\n"
                "int a[4] = {1,2,3,4};\n"
                "a[i++:];\n"
                "return i;\n"),
            .exit_code = 1,
        },
        {
            "discarded high slice evaluates bound", __LINE__,
            SVI("int i = 0;\n"
                "int a[4] = {1,2,3,4};\n"
                "a[:i++];\n"
                "return i;\n"),
            .exit_code = 1,
        },
        {
            "discarded all slice evaluates base", __LINE__,
            SVI("int i = 0;\n"
                "int a[4] = {1,2,3,4};\n"
                "(i++, a)[:];\n"
                "return i;\n"),
            .exit_code = 1,
        },
        {
            "slice ternary", __LINE__,
            SVI("int a[2] = {1,2};\n"
                "int b[2] = {3,4};\n"
                "int s[:] = 0 ? a[:] : b[:];\n"
                "return s.count * 10 + s[0];\n"),
            .exit_code = 23,
        },
        {
            "slice ternary combines const", __LINE__,
            SVI("int a[2] = {1,2};\n"
                "const int b[2] = {3,4};\n"
                "const int s[:] = 1 ? a[:] : b[:];\n"
                "return s.count * 10 + s[0];\n"),
            .exit_code = 21,
        },
        {
            "discarded slice field evaluates base", __LINE__,
            SVI("int i = 0;\n"
                "int a[4] = {1,2,3,4};\n"
                "a[i++:].count;\n"
                "return i;\n"),
            .exit_code = 1,
        },
        {
            "slice function param", __LINE__,
            SVI("int sum(int v[:]){\n"
                "    int t = 0;\n"
                "    for(int i = 0; i < v.length; i++) t += v[i];\n"
                "    return t;\n"
                "}\n"
                "int a[4] = {1,2,3,4};\n"
                "return sum(a[1:4]);\n"),
            .exit_code = 2 + 3 + 4,
        },
        {
            "mutate through slice param", __LINE__,
            SVI("void fill(int v[:]){\n"
                "    for(int i = 0; i < v.length; i++) v[i] = i + 1;\n"
                "}\n"
                "int a[4] = {0,0,0,0};\n"
                "fill(a[:]);\n"
                "return a[0] + a[1] + a[2] + a[3];\n"),
            .exit_code = 1 + 2 + 3 + 4,
        },
        {
            "slice return value", __LINE__,
            SVI("int g[4] = {5,6,7,8};\n"
                "int mid(void)[:]{ return g[1:3]; }\n"
                "int s[:] = mid();\n"
                "return s.length * 10 + s[0];\n"),
            .exit_code = 2 * 10 + 6,
        },
        {
            "slice member assignment", __LINE__,
            SVI("int x[3] = {0,0,0};\n"
                "int s[:];\n"
                "s.data = x;\n"
                "s.length = _Countof x;\n"
                "for(int i = 0; i < s.length; i++) s[i] = i + 4;\n"
                "return x[0] + x[1] + x[2];\n"),
            .exit_code = 4 + 5 + 6,
        },
        {
            "slice whole assignment", __LINE__,
            SVI("int a[4] = {1,2,3,4};\n"
                "int s[:];\n"
                "s = a[1:3];\n"
                "return s.length * 10 + s[0];\n"),
            .exit_code = 2 * 10 + 2,
        },
        {
            "array converts to slice in init", __LINE__,
            SVI("int a[4] = {1,2,3,4};\n"
                "int s[:] = a;\n"
                "return s.length * 10 + s[0];\n"),
            .exit_code = 4 * 10 + 1,
        },
        {
            "array converts to slice as arg", __LINE__,
            SVI("int sum(int v[:]){\n"
                "    int t = 0;\n"
                "    for(int i = 0; i < v.length; i++) t += v[i];\n"
                "    return t;\n"
                "}\n"
                "int a[4] = {1,2,3,4};\n"
                "return sum(a);\n"),
            .exit_code = 1 + 2 + 3 + 4,
        },
        {
            "array converts to slice on return", __LINE__,
            SVI("int g[4] = {5,6,7,8};\n"
                "int all(void)[:]{ return g; }\n"
                "int s[:] = all();\n"
                "return s.length * 10 + s[0];\n"),
            .exit_code = 4 * 10 + 5,
        },
        {
            "string literal converts to slice", __LINE__,
            SVI("const char s[:] = \"hello\";\n"
                "return (int)s.count + s[0];\n"),
            .exit_code = (int)sizeof "hello" - 1 + 'h',
        },
        {
            "string literal slice conversion contexts", __LINE__,
            SVI("int count(const char s[:]){ return s.count; }\n"
                "const char text(void)[:]{ return \"hello\"; }\n"
                "const char s[:] = \"\";\n"
                "if(s.count != 0 || count(\"\") != 0) return 0;\n"
                "s = \"a\\0b\";\n"
                "return s.count == 3 && s[1] == 0 && s[2] == 'b'\n"
                "    && count(\"hello\") == 5 && text().count == 5;\n"),
            .exit_code = 1,
        },
        {
            "literal slice conversion preserves array and explicit slice length", __LINE__,
            SVI("char a[] = \"hello\";\n"
                "const char s[:] = a;\n"
                "const char full[:] = \"hello\"[:];\n"
                "const char cast[:] = (const char[:])\"hello\";\n"
                "return s.count == 6 && full.count == 6 && cast.count == 6 && sizeof(\"hello\") == 6;\n"),
            .exit_code = 1,
        },
        {
            "constexpr literal slice conversion", __LINE__,
            SVI("constexpr const char s[:] = \"hello\";\n"
                "_Static_assert(s.count == 5);\n"
                "return s[4] == 'o';\n"),
            .exit_code = 1,
        },
        {
            "array converts to const slice", __LINE__,
            SVI("int a[3] = {2,4,6};\n"
                "const int s[:] = a;\n"
                "return s.count * 10 + s[1];\n"),
            .exit_code = 3 * 10 + 4,
        },
        {
            "_Generic on array does not pick slice arm", __LINE__,
            SVI("int a[4] = {0,0,0,0};\n"
                "return _Generic(a, int*: 1, int[:]: 2, default: 3);\n"),
            .exit_code = 1,
        },
        {
            "_Generic on slice picks slice arm", __LINE__,
            SVI("int a[4] = {0,0,0,0};\n"
                "int s[:] = a[:];\n"
                "return _Generic(s, int*: 1, int[:]: 2, default: 3);\n"),
            .exit_code = 2,
        },
        {
            "explicit array to slice cast", __LINE__,
            SVI("int a[4] = {1,2,3,4};\n"
                "int s[:] = (int[:])a;\n"
                "return s.length * 10 + s[0];\n"),
            .exit_code = 4 * 10 + 1,
        },
        {
            "explicit array to slice cast drops const", __LINE__,
            SVI("const int a[3] = {2,4,6};\n"
                "int s[:] = (int[:])a;\n"
                "return s.count * 10 + s[1];\n"),
            .exit_code = 3 * 10 + 4,
        },
        {
            "struct self assign", __LINE__,
            SVI("struct S {int x, y;} s = {-1, -2};\n"
                "s = {-s.y, -s.x};\n"
                "return s.x+s.y;\n"),
            .exit_code = 1+2,
        },
        {
            // A braced assignment (not a declaration initializer) is a compound
            // literal: a distinct object, so the initializers read the old value
            // of the target. This must hold for locals too, not just globals.
            "struct self assign local", __LINE__,
            SVI("int f(void){\n"
                "    struct S {int x, y;} s = {-1, -2};\n"
                "    s = {-s.y, -s.x};\n"
                "    return s.x*100 + s.y;\n"  // expect 201, not 0
                "}\n"
                "return f();\n"),
            .exit_code = 201,
        },
        {
            "array self assign local", __LINE__,
            SVI("int f(void){\n"
                "    int a[2] = {3, 7};\n"
                "    a = {a[1], a[0]};\n"
                "    return a[0]*100 + a[1];\n"
                "}\n"
                "return f();\n"),
            .exit_code = 700 + 3,
        },
        {
            "init list partial zero fill", __LINE__,
            SVI("struct S { int a, b, c, d; };\n"
                "int f(void){\n"
                "    struct S s = {5, 6};\n"
                "    return s.a + s.b*10 + s.c*100 + s.d*1000;\n"
                "}\n"
                "return f();\n"),
            .exit_code = 5 + 60,
        },
        {
            "init list nested struct", __LINE__,
            SVI("struct Inner { int x, y; };\n"
                "struct Outer { int tag; struct Inner in; };\n"
                "int f(void){\n"
                "    struct Outer o = {9, {4, 7}};\n"
                "    return o.tag*100 + o.in.x*10 + o.in.y;\n"
                "}\n"
                "return f();\n"),
            .exit_code = 900 + 40 + 7,
        },
        {
            "init list array of structs", __LINE__,
            SVI("struct P { int a, b; };\n"
                "int f(void){\n"
                "    struct P ps[2] = {{1, 2}, {3, 4}};\n"
                "    return ps[0].a*1000 + ps[0].b*100 + ps[1].a*10 + ps[1].b;\n"
                "}\n"
                "return f();\n"),
            .exit_code = 1000 + 200 + 30 + 4,
        },
        {
            "init list bitfields", __LINE__,
            SVI("struct B { unsigned a: 3; unsigned b: 4; unsigned c: 9; };\n"
                "int f(void){\n"
                "    struct B x = {5, 9, 300};\n"
                "    return x.a + x.b*10 + x.c*1000;\n"
                "}\n"
                "return f();\n"),
            .exit_code = 5 + 90 + 300000,
        },
        {
            "init list nested value expr", __LINE__,
            SVI("struct Inner { int x, y; };\n"
                "struct Outer { struct Inner in; int tag; };\n"
                "int f(void){\n"
                "    struct Inner src = {6, 8};\n"
                "    struct Outer o = {src, 3};\n"
                "    return o.in.x*100 + o.in.y*10 + o.tag;\n"
                "}\n"
                "return f();\n"),
            .exit_code = 600 + 80 + 3,
        },
        {
            "bswap16: gcc", __LINE__,
            SVI("unsigned short v = 0x0102;\n"
                "return __builtin_bswap16(v);\n"),
            .exit_code = 0x0201,
        },
        {
            "bswap32: gcc", __LINE__,
            SVI("unsigned int v = 0x01020304;\n"
                "return (int)__builtin_bswap32(v);\n"),
            .exit_code = 0x04030201,
        },
        {
            "bswap64: gcc", __LINE__,
            SVI("unsigned long long v = 0x0102030405060708;\n"
                "return __builtin_bswap64(v) == 0x0807060504030201;\n"),
            .exit_code = 1,
        },
        {
            "bswap16: msvc", __LINE__,
            SVI("unsigned short v = 0x0102;\n"
                "return _byteswap_ushort(v);\n"),
            .exit_code = 0x0201,
        },
        {
            "bswap32: msvc", __LINE__,
            SVI("unsigned int v = 0x01020304;\n"
                "return (int)_byteswap_ulong(v);\n"),
            .exit_code = 0x04030201,
        },
        {
            "bswap64: msvc", __LINE__,
            SVI("unsigned long long v = 0x0102030405060708;\n"
                "return _byteswap_uint64(v) == 0x0807060504030201;\n"),
            .exit_code = 1,
        },
        {
            "bswap16: gcc (func)", __LINE__,
            SVI("unsigned short f(void){unsigned short v = 0x0102;\n"
                "return __builtin_bswap16(v);}\n"
                "return f();\n"),
            .exit_code = 0x0201,
        },
        {
            "bswap32: gcc (func)", __LINE__,
            SVI("int f(void){unsigned int v = 0x01020304;\n"
                "return (int)__builtin_bswap32(v);}\n"
                "return f();\n"),
            .exit_code = 0x04030201,
        },
        {
            "bswap64: gcc (func)", __LINE__,
            SVI("int f(void){unsigned long long v = 0x0102030405060708;\n"
                "return __builtin_bswap64(v) == 0x0807060504030201;}\n"
                "return f();\n"),
            .exit_code = 1,
        },
        {
            "bswap16: msvc (func)", __LINE__,
            SVI("unsigned short f(void){unsigned short v = 0x0102;\n"
                "return _byteswap_ushort(v);}\n"
                "return f();\n"),
            .exit_code = 0x0201,
        },
        {
            "bswap32: msvc (func)", __LINE__,
            SVI("int f(void){unsigned int v = 0x01020304;\n"
                "return (int)_byteswap_ulong(v);}\n"
                "return f();\n"),
            .exit_code = 0x04030201,
        },
        {
            "bswap64: msvc (func)", __LINE__,
            SVI("int f(void){unsigned long long v = 0x0102030405060708;\n"
                "return _byteswap_uint64(v) == 0x0807060504030201;}\n"
                "return f();\n"),
            .exit_code = 1,
        },
        {
            "address of literal (global)", __LINE__,
            SVI("int *p = &3;\n"
                "++*p;\n"
                "return *p;\n"),
            .exit_code = 4,
        },
        {
            "address of literal (func)", __LINE__,
            SVI("int f(void){\n"
                "  int *p = &3;\n"
                "  ++*p;\n"
                "  return *p;\n"
                "}\n"
                "return f();\n"),
            .exit_code = 4,
        },
        {
            "vector", __LINE__,
            SVI("typedef int __attribute__((vector_size(16))) int4;\n"
                "int4 i = {0, 1, 2, 3};\n"
                "i[0] = 2;\n"
                "return i[0] + i[1] + i[2] + i[3]\n"),
            .exit_code = 8,
        },
        {
            "extern array regression", __LINE__,
            SVI(
                "const int array[2] = {1, 2};\n"
                "extern const int array[];\n"
                "return array[0] + array[1];\n"
            ),
            .exit_code = 3,
        },
        {
            "extern array preserves inferred bound", __LINE__,
            SVI(
                "const int array[] = {1, 2};\n"
                "extern const int array[];\n"
                "static_assert(sizeof(array) == 2 * sizeof(int));\n"
                "return array[0] + array[1];\n"
            ),
            .exit_code = 3,
        },
        {
            "extern array completed by definition", __LINE__,
            SVI(
                "extern const int array[];\n"
                "const int array[2] = {1, 2};\n"
                "static_assert(sizeof(array) == 2 * sizeof(int));\n"
                "return array[0] + array[1];\n"
            ),
            .exit_code = 3,
        },
        {
            "extern array preserves tentative bound for initializer", __LINE__,
            SVI(
                "int array[2];\n"
                "extern int array[] = {1};\n"
                "static_assert(sizeof(array) == 2 * sizeof(int));\n"
                "return array[0] + array[1];\n"
            ),
            .exit_code = 1,
        },
        {
            "_Self in body", __LINE__,
            SVI("struct S { int x; int f(_Self* self) {\n"
               "    _Self* s = self;\n"
               "    return s.x;\n"
               "}};\n"
               "struct S s = {3};\n"
               "return s.f();\n"
            ),
            .exit_code = 3,
        },
        {
            "_Self in nested function body", __LINE__,
            SVI("struct S { int x; int f(_Self* self) {\n"
               "    int bar(_Self* s){ return s.x-1;}\n"
               "    return bar(self);\n"
               "}};\n"
               "struct S s = {3};\n"
               "return s.f();\n"
            ),
            .exit_code = 2,
        },
        {
            "local nested struct method calls later outer struct method", __LINE__,
            SVI("int run(void) {\n"
                "struct Outer {\n"
                "    struct Inner {\n"
                "        int call(_Self* self, struct Outer* outer) {\n"
                "            return outer.answer();\n"
                "        }\n"
                "    } inner;\n"
                "    int answer(_Self* self) { return 42; }\n"
                "};\n"
                "struct Outer outer = {};\n"
                "return outer.inner.call(&outer);\n"
                "}\n"
                "return run();\n"),
            .exit_code = 42,
        },
        {
            "local struct method calls enclosing function helper", __LINE__,
            SVI("int outer(void) {\n"
                "    int helper(void) { return 42; }\n"
                "    struct S {\n"
                "        int method(_Self* s) {\n"
                "            return helper();\n"
                "        }\n"
                "    };\n"
                "    struct S s = {};\n"
                "    return s.method();\n"
                "}\n"
                "return outer();\n"),
            .exit_code = 42,
        },
        {
            "local typedef method uses completed declaration", __LINE__,
            SVI("int run(void) {\n"
                "    typedef struct {\n"
                "        int value;\n"
                "        int get(_Self* self) { Local* p = self; return p.value; }\n"
                "    } Local;\n"
                "    Local s = {42};\n"
                "    return s.get();\n"
                "}\nreturn run();\n"),
            .exit_code = 42,
        },
        {
            "block local union methods retain helper and later method", __LINE__,
            SVI("int helper(void) { return 1; }\n"
                "int run(void) {\n"
                "    {\n"
                "        int helper(void) { return 42; }\n"
                "        union U {\n"
                "            int first(_Self* self) { return self.second(); }\n"
                "            int second(_Self* self) { return helper(); }\n"
                "        } u;\n"
                "        return u.first();\n"
                "    }\n"
                "}\nreturn run();\n"),
            .exit_code = 42,
        },
        {
            "local methods preserve enclosing loop and switch", __LINE__,
            SVI("int run(void) {\n"
                "    int helper(void) { return 21; }\n"
                "    int result = 0;\n"
                "    for(int i = 0; i < 2; i++) {\n"
                "        switch(i) {\n"
                "        case 0:\n"
                "            { struct S { int get(_Self* self) {\n"
                "                int n = 0;\n"
                "                for(int j = 0; j < 2; j++) {\n"
                "                    switch(j) { case 0: continue; default: n += helper(); break; }\n"
                "                }\n"
                "                return n;\n"
                "            } } s; result += s.get(); }\n"
                "            break;\n"
                "        default: result += helper(); break;\n"
                "        }\n"
                "    }\n"
                "    return result;\n"
                "}\nreturn run();\n"),
            .exit_code = 42,
        },
        {
            "nested torture test", __LINE__,
            SVI("int foo( struct S { int x; int p(_Self* s, struct S {int x; int p(_Self* s){ return s.x; } } t){ return s.x+t.p(); } } s){\n"
                "    int inner(struct S s){ return s.p({s.x+1}); }\n"
                "    return inner(s);\n"
                "}\n"
                "return foo({1});\n"),
            .exit_code = 3,
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
        _Bool ok = 0;
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
                    .target = cc_target_funcs[CC_TARGET_TEST](),
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

        err = cpp_include_file_via_file_cache(&interp.parser.cpp, SV(__FILE__));
        if(err) {TestReport("failed to include"); goto finally;}
        ma_tail(interp.parser.cpp.frames).line = tc->line+1;

        err = cc_parse_all(&interp.parser);
        if(err){TestPrintf("%s:%d: failed to parse\n", __FILE__, tc->line); goto finally;}
        err = ci_resolve_refs(&interp);
        if(err){TestPrintf("%s:%d: failed to link: %s\n", __FILE__, tc->line, _cc_error_names[err]); goto finally;}

        CiInterpFrame* frame = &interp.top_frame;
        err = ci_prepare_toplevel(&interp);
        if(err) goto finally;
        if(tc->expect_template){
            size_t zero_count = 0;
            size_t copy_count = 0;
            size_t store_count = 0;
            PointerMapItems funcs = PM_items(&interp.deps.funcs);
            for(size_t j = 0; j < funcs.count; j++){
                CcFunc* func = (CcFunc*)(uintptr_t)funcs.data[j].key;
                if(!func->interp_ops) continue;
                for(size_t k = 0; k < func->interp_ops->code.count; k++){
                    zero_count += func->interp_ops->code.data[k].kind == CI_OP_ZERO;
                    copy_count += func->interp_ops->code.data[k].kind == CI_OP_MEMCOPY;
                    store_count += func->interp_ops->code.data[k].kind == CI_OP_STORE
                        || func->interp_ops->code.data[k].kind == CI_OP_STORE_BITFIELD;
                }
            }
            TestExpect(size_t, zero_count, ==, 0);
            TestExpect(size_t, copy_count, >, 0);
            if(tc->expect_runtime_stores)
                TestExpect(size_t, store_count, ==, tc->expect_runtime_stores);
        }
        while(frame->pc < frame->op_count){
            err = ci_interp_step(&interp, frame);
            if(err) goto finally;
        }
        TEST_stats.executed++;
        if(interp.exit_code != tc->exit_code){
            TEST_stats.failures++;
            TestPrintf("%s:%d: expected (%d) != actual (%d)\n", __FILE__, tc->line, tc->exit_code, interp.exit_code);
        }
        else
            ok = 1;

        finally:
        if(log_sb.cursor && !log_sb.errored && !ok){
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

TestFunction(test_interpreter_runtime_errors){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    static struct tc {
        const char* name; int line;
        StringView program;
        StringView expect;
        _Bool skip;
        _Bool lowering_error;
        _Bool parser_error;
    } testcases[] = {
        {
            "generic unsigned builtin rejects signed argument", __LINE__,
            SVI("return __builtin_popcountg(1);\n"),
            SVI("(test):1:28: error: bit builtin requires an unsigned integer argument\n"),
            .parser_error = 1,
        },
        {
            "generic signed builtin rejects unsigned argument", __LINE__,
            SVI("return __builtin_ffsg(1u);\n"),
            SVI("(test):1:23: error: bit builtin requires a signed integer argument\n"),
            .parser_error = 1,
        },
        {
            "generic builtin rejects floating argument", __LINE__,
            SVI("return __builtin_clrsbg(1.0);\n"),
            SVI("(test):1:25: error: bit builtin requires a signed integer argument\n"),
            .parser_error = 1,
        },
        {
            "generic builtin rejects boolean argument", __LINE__,
            SVI("return __builtin_popcountg((_Bool)1);\n"),
            SVI("(test):1:28: error: bit builtin requires an unsigned integer argument\n"),
            .parser_error = 1,
        },
        {
            "stdc builtin rejects signed argument", __LINE__,
            SVI("return __builtin_stdc_bit_floor(1);\n"),
            SVI("(test):1:33: error: bit builtin requires an unsigned integer argument\n"),
            .parser_error = 1,
        },
        {
            "generic zero fallback requires int", __LINE__,
            SVI("return __builtin_clzg(1u,0L);\n"),
            SVI("(test):1:26: error: zero fallback must have int type\n"),
            .parser_error = 1,
        },
        {
            "rotation count requires integer", __LINE__,
            SVI("return __builtin_stdc_rotate_left(1u,1.0);\n"),
            SVI("(test):1:38: error: rotation count must have integer type\n"),
            .parser_error = 1,
        },
        {
            "constant generic clz rejects zero without fallback", __LINE__,
            SVI("_Static_assert(__builtin_clzg(0u));\n"),
            SVI("(test):1:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constant generic ctz rejects zero without fallback", __LINE__,
            SVI("_Static_assert(__builtin_ctzg(0u));\n"),
            SVI("(test):1:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constant rotation rejects negative count", __LINE__,
            SVI("_Static_assert(__builtin_stdc_rotate_left(1u,-1));\n"),
            SVI("(test):1:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constant bit ceil rejects unrepresentable result", __LINE__,
            SVI("_Static_assert(__builtin_stdc_bit_ceil((unsigned char)129));\n"),
            SVI("(test):1:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "native extern TLS requires a resolver", __LINE__,
            SVI("extern _Thread_local int native_tls;\nreturn native_tls;\n"),
            SVI("(test):1:26: error: native extern thread_local variable 'native_tls' is not supported\n"),
            .lowering_error = 1,
        },
        {
            "symbolic pointers: reject subtraction of different symbols", __LINE__,
            SVI("static int a,b;\nstatic long d=&b-&a; return d;\n"),
            SVI("(test):2:17: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "constexpr metadata logical not rejects before evaluation", __LINE__,
            SVI("constexpr _Type t=int;\n_Static_assert(!t); return 0;\n"),
            SVI("(test):2:16: error: '!' requires scalar type\n"),
            .parser_error = 1,
        },
        {
            "return type conflict cannot change an earlier call's ABI", __LINE__,
            SVI("int f(); int g(void){return f();}\n"
                "struct S {int a[4];};\n"
                "struct S f(void){return (struct S){{1,2,3,4}};} return g();\n"),
            SVI("(test):3:17: error: conflicting return type for 'f'\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: symbolic pointer is not absolute integer bits", __LINE__,
            SVI("static int a; constexpr union U {int* p; unsigned long bits;} u={.p=&a};\n_Static_assert(u.bits==0); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "static union: unresolved floating point arithmetic is rejected", __LINE__,
            SVI("static int a; constexpr union U {int* p; double d;} u={.p=&a};\nstatic double d=u.d+1.0; return d==0;\n"),
            SVI("(test):2:20: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: narrow floating view cannot split relocation", __LINE__,
            SVI("static int a; constexpr union U {int* p; float f;} u={.p=&a};\nstatic float f=u.f; return f==0;\n"),
            SVI("(test):2:17: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: partially overwritten floating relocation is rejected", __LINE__,
            SVI("static int a; constexpr union U {int* p; double d; unsigned bit:1;} u={.p=&a,.bit=1};\nstatic double d=u.d; return d==0;\n"),
            SVI("(test):2:18: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: unresolved floating point narrowing is rejected", __LINE__,
            SVI("static int a; constexpr union U {int* p; double d;} u={.p=&a};\nstatic float f=u.d; return f==0;\n"),
            SVI("(test):2:17: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: unresolved floating point to integer cast is rejected", __LINE__,
            SVI("static int a; constexpr union U {int* p; double d;} u={.p=&a};\nstatic long n=(long)u.d; return n==0;\n"),
            SVI("(test):2:15: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: partial overwrite loses symbolic pointer identity", __LINE__,
            SVI("static int a; constexpr union U {int* p; unsigned bit:1;} u={.p=&a,.bit=1};\n_Static_assert(u.p==&a); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: bitfield cannot read unresolved address bits", __LINE__,
            SVI("static int a; constexpr union U {int* p; unsigned long bit:1;} u={.p=&a};\n_Static_assert(u.bit==0); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "static union: bitfield cannot retain a pointer relocation", __LINE__,
            SVI("static int a; constexpr union U {int* p; unsigned long bit:1;} u={.p=&a};\nstatic unsigned long bit=u.bit; return bit;\n"),
            SVI("(test):2:27: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: partial symbolic pointer view is rejected", __LINE__,
            SVI("static int a; constexpr union U {int* p; unsigned bit:1;} u={.p=&a,.bit=1};\nstatic int* p=u.p; return p==&a;\n"),
            SVI("(test):2:16: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: partial relocation is not an integer constant", __LINE__,
            SVI("static int a; constexpr union U {int* p; unsigned long bits; unsigned bit:1;} u={.p=&a,.bit=1};\nstatic unsigned long bits=u.bits; return bits;\n"),
            SVI("(test):2:28: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: integer bits cannot fabricate type metadata", __LINE__,
            SVI("constexpr union U {unsigned long bits; _Type t;} u={.bits=4096};\n_Static_assert(u.t.sizeof_==4); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: type metadata cannot become numeric bytes", __LINE__,
            SVI("constexpr union U {_Type t; unsigned long bits;} u={.t=int};\n"
                "_Static_assert(u.bits==u.bits); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: invalid metadata is still opaque storage", __LINE__,
            SVI("constexpr union U {_Type t; unsigned long bits;} u={.t={}};\n"
                "_Static_assert(u.bits==0); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: omitted pointer members remain opaque", __LINE__,
            SVI("constexpr union U {struct P {void* p;} p; unsigned long bits;} u={.p={}};\n"
                "_Static_assert(u.bits==0); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "static union: omitted opaque members cannot become numeric aggregate fields", __LINE__,
            SVI("constexpr union U {struct P {void* p;} p; struct B {unsigned long bits;} b;} u={.p={}};\n"
                "static struct B copy=u.b; return 0;\n"),
            SVI("(test):2:23: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: omitted Any tags remain opaque", __LINE__,
            SVI("constexpr union U {_Any a; unsigned char bytes[sizeof(_Any)];} u={.a={}};\n"
                "_Static_assert(u.bytes[0]==0); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: absolute pointers cannot become numeric bytes", __LINE__,
            SVI("constexpr union U {int* p; unsigned long bits;} u={.p=(int*)7};\n"
                "_Static_assert(u.bits==7); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: null pointers are opaque to byte reinterpretation", __LINE__,
            SVI("constexpr union U {void* p; unsigned long bits;} u={.p=nullptr};\n"
                "_Static_assert(u.bits==0); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: numeric addresses reject partial pointer overwrites", __LINE__,
            SVI("constexpr union U {int* p; unsigned bit:1;} u={.p=(int*)6,.bit=1};\n"
                "_Static_assert(u.p==(int*)7); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "static union: numeric slice pointers reject partial overwrites", __LINE__,
            SVI("constexpr union U {int s[:]; struct B {unsigned long count; unsigned bit:1;} b; struct V {unsigned long count; int* p;} v;} u={.s=((int*)6)[:2],.b.bit=1};\n"
                "static struct V copy=u.v; return 0;\n"),
            SVI("(test):2:23: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: floating views cannot reinterpret pointer relocations", __LINE__,
            SVI("static int target; constexpr union U {int* p; double d;} u={.p=&target};\n"
                "static const double d=u.d; return 0;\n"),
            SVI("(test):2:24: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: aggregate copies cannot reinterpret pointer storage", __LINE__,
            SVI("constexpr union U {int* p; struct B {unsigned long bits;} b;} u={.p=(int*)7};\n"
                "static struct B copy=u.b; return 0;\n"),
            SVI("(test):2:23: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: symbolic pointers cannot become integer relocations", __LINE__,
            SVI("static int target; constexpr union U {int* p; unsigned long bits;} u={.p=&target};\n"
                "static unsigned long bits=u.bits; return 0;\n"),
            SVI("(test):2:28: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: aggregate copies cannot reinterpret metadata", __LINE__,
            SVI("constexpr union U {_Type t; struct B {unsigned long bits;} b;} u={.t=int};\n"
                "static struct B copy=u.b; return 0;\n"),
            SVI("(test):2:23: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: aggregate copies cannot reinterpret Any tags", __LINE__,
            SVI("constexpr union U {_Any a; struct B {unsigned long bits[2];} b;} u={.a=3};\n"
                "static struct B copy=u.b; return 0;\n"),
            SVI("(test):2:23: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: type metadata cannot feed a bitfield", __LINE__,
            SVI("constexpr union U {_Type t; unsigned bit:1;} u={.t=int};\n"
                "_Static_assert(u.bit==u.bit); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: Any tags cannot become numeric bytes", __LINE__,
            SVI("constexpr union U {_Any a; unsigned char bytes[sizeof(_Any)];} u={.a=3};\n"
                "_Static_assert(u.bytes[0]==u.bytes[0]); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: pointer casts cannot expose metadata bytes", __LINE__,
            SVI("constexpr _Type t=int; constexpr const unsigned char* p=(const unsigned char*)&t;\n"
                "_Static_assert(p[0]==p[0]); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "static union: fabricated type metadata is rejected", __LINE__,
            SVI("constexpr union U {unsigned long bits; _Type t;} u={.bits=4096};\nstatic _Type t=u.t; return 0;\n"),
            SVI("(test):2:17: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: type overwrite cannot fabricate metadata", __LINE__,
            SVI("constexpr union U {_Type t; unsigned long bits;} u={.t=int,.bits=4096};\n_Static_assert(u.t.sizeof_==4); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: integer bytes cannot fabricate Any tag", __LINE__,
            SVI("constexpr union U {unsigned long bits[2]; _Any a;} u={.bits={4096,0}};\n_Static_assert(u.a.type.sizeof_==4); return 0;\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
            .parser_error = 1,
        },
        {
            "static union: aggregate view cannot split a relocation", __LINE__,
            SVI("static int a; constexpr union U {int* p; struct B {unsigned low;} b;} u={.p=&a};\nstatic struct B b=u.b; return b.low;\n"),
            SVI("(test):2:20: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: large aggregate view cannot cut trailing relocation", __LINE__,
            SVI("static int a; constexpr union U {struct A {unsigned long x; int* p;} a; struct B {unsigned x[3];} b;} u={.a={7,&a}};\nstatic struct B b=u.b; return b.x[0];\n"),
            SVI("(test):2:20: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: aggregate view cannot split slice pointer", __LINE__,
            SVI("static int a[3]; constexpr union U {int s[:]; struct B {unsigned x[3];} b;} u={.s=a[:]};\nstatic struct B b=u.b; return b.x[0];\n"),
            SVI("(test):2:20: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: slice address cannot exceed one-past-end", __LINE__,
            SVI("static int a[3]; constexpr union U {int s[:];} u={.s=a[:]};\nstatic const int* p=&u.s[4]; return 0;\n"),
            SVI("(test):2:21: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static reflection: invalid field index is rejected by parser", __LINE__,
            SVI("struct S {int x;};\nstatic struct __builtin_Field f=(struct S).field(1); return 0;\n"),
            SVI("(test):2:43: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static reflection: missing named field is rejected by parser", __LINE__,
            SVI("struct S {int x;};\nstatic struct __builtin_Field f=(struct S).field(\"missing\"); return 0;\n"),
            SVI("(test):2:43: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: slice address cannot have negative index", __LINE__,
            SVI("static int a[3]; constexpr union U {int s[:];} u={.s=a[:]};\nstatic const int* p=&u.s[-1]; return 0;\n"),
            SVI("(test):2:21: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static union: aggregate view cannot start inside relocation", __LINE__,
            SVI("static int a; constexpr union U {struct A {int* p; unsigned long x;} a; struct B {unsigned pad; struct C {unsigned x[3];} c;} b;} u={.a={&a,7}};\nstatic struct C c=u.b.c; return c.x[0];\n"),
            SVI("(test):2:22: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: narrow integer view of relocation is rejected", __LINE__,
            SVI("static int a; constexpr union U {int* p; unsigned low;} u={.p=&a};\nstatic unsigned low=u.low; return low;\n"),
            SVI("(test):2:22: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "constexpr union: high half of relocation is rejected", __LINE__,
            SVI("static int a; constexpr union U {int* p; struct W {unsigned low,high;} w;} u={.p=&a};\nstatic unsigned high=u.w.high; return high;\n"),
            SVI("(test):2:25: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static Any: reject mismatched constant view", __LINE__,
            SVI("const _Any boxed=7;\nstatic long x=boxed.as(long); return x;\n"),
            SVI("(test):2:20: error: constant _Any.as requires the stored type, ignoring top-level qualifiers\n"),
            .parser_error = 1,
        },
        {
            "static data: reject volatile member read from const aggregate", __LINE__,
            SVI("const struct S {volatile int x;} s={7};\nstatic int x=s.x; return x;\n"),
            SVI("(test):2:15: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static data: reject atomic member read from const aggregate", __LINE__,
            SVI("const struct S {_Atomic(int) x;} s={7};\nstatic int x=s.x; return x;\n"),
            SVI("(test):2:15: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static data: reject volatile pointer member read", __LINE__,
            SVI("static int a; const struct S {int* volatile p;} s={&a};\nstatic int* p=s.p; return p==&a;\n"),
            SVI("(test):2:16: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "symbolic pointers: cyclic aggregate initializer is rejected", __LINE__,
            SVI("const struct P {int* p;} s={s.p};\nstatic int same=s.p==s.p; return same;\n"),
            SVI("(test):2:20: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "symbolic pointers: self copied aggregate initializer is rejected", __LINE__,
            SVI("const struct P {int* p;} s=s;\nstatic int same=s.p==s.p; return same;\n"),
            SVI("(test):2:20: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "symbolic pointers: reject automatic nested member addresses", __LINE__,
            SVI("void f(void){struct S {int a,b;} s;\nconstexpr long d=&s.b-&s.a;} f();\n"),
            SVI("(test):2:22: error: constexpr initializer requires a link-time constant\n"),
            .lowering_error = 1,
        },
        {
            "symbolic pointers: reject mutable pointer alias in static difference", __LINE__,
            SVI("static int a; int* p=&a;\nstatic long d=p-&a; return d;\n"),
            SVI("(test):2:16: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "symbolic pointers: reject volatile const pointer alias", __LINE__,
            SVI("static int a; int* const volatile p=&a;\nstatic long d=p-&a; return d;\n"),
            SVI("(test):2:16: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "symbolic pointers: reject atomic const pointer alias", __LINE__,
            SVI("static int a; _Atomic(int*) const p=&a;\nstatic long d=p-&a; return d;\n"),
            SVI("(test):2:16: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "symbolic pointers: reject runtime member read masquerading as an address", __LINE__,
            SVI("static int a; struct S {int* p;} s={&a};\nstatic long d=s.p-&a; return d;\n"),
            SVI("(test):2:18: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "symbolic pointers: reject constexpr subtraction of different symbols", __LINE__,
            SVI("static int a,b;\nconstexpr long d=&b-&a; return d;\n"),
            SVI("(test):2:20: error: constexpr initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "symbolic pointers: reject fractional element difference", __LINE__,
            SVI("static int a;\nstatic long d=(int*)((char*)&a+1)-&a; return d;\n"),
            SVI("(test):2:34: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "symbolic pointers: reject address-dependent comparison", __LINE__,
            SVI("static int a[2],b[2];\nstatic int same=(a+2)==b; return same;\n"),
            SVI("(test):2:22: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static data: reject signed conversion below the truncated range", __LINE__,
            SVI("static signed char x=(signed char)-129.0; return x;\n"),
            SVI("(test):1:22: error: static initializer conversion is out of range\n"),
            .parser_error = 1,
        },
        {
            "static data: reject reversed slice bounds", __LINE__,
            SVI("static int a[3];\nstatic int s[:]=a[2:1];\nreturn s.count;\n"),
            SVI("(test):2:18: error: static slice bounds out of range\n"),
            .lowering_error = 1,
        },
        {
            "static data: reject slice bounds past array end", __LINE__,
            SVI("static int a[3];\nstatic int s[:]=a[:4];\nreturn s.count;\n"),
            SVI("(test):2:18: error: static slice bounds out of range\n"),
            .lowering_error = 1,
        },
        {
            "static data: reject negative slice bounds", __LINE__,
            SVI("static int a[3];\nstatic int s[:]=a[-1:2];\nreturn s.count;\n"),
            SVI("(test):2:18: error: static slice bounds out of range\n"),
            .lowering_error = 1,
        },
        {
            "static data: reject runtime slice bounds", __LINE__,
            SVI("int n=1; static int a[3];\nstatic int s[:]=a[:n];\nreturn s.count;\n"),
            SVI("(test):2:18: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static data: reject runtime pointer members", __LINE__,
            SVI("struct S {const char* p;} s={\"abc\"};\nstatic const char* p=s.p;\nreturn p[0];\n"),
            SVI("(test):2:23: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static data: reject runtime variable reads", __LINE__,
            SVI("int source=7;\nstatic int value=source;\nreturn value;\n"),
            SVI("(test):2:18: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static data: parser rejects external const read without a known value", __LINE__,
            SVI("extern const int source;\nstatic int value=source;\nreturn value;\n"),
            SVI("(test):2:18: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static data: parser rejects volatile const read", __LINE__,
            SVI("const volatile int source=7;\nstatic int value=source;\nreturn value;\n"),
            SVI("(test):2:18: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "static data: parser rejects const object with runtime initializer", __LINE__,
            SVI("int f(void){return 7;} const int source=f();\nstatic int value=source;\nreturn value;\n"),
            SVI("(test):2:18: error: static initializer requires a link-time constant\n"),
            .parser_error = 1,
        },
        {
            "varargs: reject over-aligned int128", __LINE__,
            SVI("__int128 x=1;\nint f(int n,...){return n;}\nf(0,\nx);\n"),
            SVI("(test):4:1: error: variadic arguments requiring alignment greater than 8 bytes are not supported\n"),
            .lowering_error = 1,
        },
        {
            "varargs: reject over-aligned indirect argument", __LINE__,
            SVI("__int128 x=1;\nint f(int n,...){return n;}\nint (*p)(int,...)=f;\np(0,\nx);\n"),
            SVI("(test):5:1: error: variadic arguments requiring alignment greater than 8 bytes are not supported\n"),
            .lowering_error = 1,
        },
        {
            "varargs: reject over-aligned long double", __LINE__,
            SVI("long double x=1;\nint f(int n,...){return n;}\nf(0,\nx);\n"),
            SVI("(test):4:1: error: variadic arguments requiring alignment greater than 8 bytes are not supported\n"),
            .lowering_error = 1,
        },
        {
            "va_arg: reject over-aligned int128", __LINE__,
            SVI("__builtin_va_list ap;\n__builtin_va_arg(ap,__int128);\n"),
            SVI("(test):2:1: error: va_arg types requiring alignment greater than 8 bytes are not supported\n"),
            .lowering_error = 1,
        },
        {
            "va_arg: reject over-aligned long double", __LINE__,
            SVI("__builtin_va_list ap;\n__builtin_va_arg(ap,long double);\n"),
            SVI("(test):2:1: error: va_arg types requiring alignment greater than 8 bytes are not supported\n"),
            .lowering_error = 1,
        },
        {
            "recursion: deep error unwinds frames", __LINE__,
            SVI("int fail(int n){\n"
                "  int *p = __builtin_alloca(sizeof(int)); *p = n;\n"
                "  if(n) return fail(n-1) + *p;\n"
                "  _Type t = int;\n"
                "  t.field(0);\n"
                "  return 0;\n"
                "}\n"
                "return fail(100);\n"),
            SVI("(test):5:4: error: _Type.field: not a struct or union type\n"),
        },
        {
            "reflection: module validation before name", __LINE__,
            SVI("_Module m = __root_module(); *(unsigned long long*)&m = 1;\n"
                "return m.symbol((__builtin_trap(), \"x\"), int) != 0;\n"),
            SVI("(test):2:9: error: _Module is not valid\n"),
        },
        {
            "reflection: null symbol name", __LINE__,
            SVI("_Module m = __root_module();\n"
                "return m.symbol(((const char*)0)[:1], int) != 0;\n"),
            SVI("(test):2:9: error: _Module.symbol name must not be NULL\n"),
        },
        {
            "reflection: null Any symbol name", __LINE__,
            SVI("_Module m = __root_module();\n"
                "return m.symbol(((const char*)0)[:1]).type == int*;\n"),
            SVI("(test):2:9: error: _Module.symbol name must not be NULL\n"),
        },
        {
            "intern: null data with nonzero count", __LINE__,
            SVI("return __builtin_intern(((const char*)0)[:1]).count;\n"),
            SVI("(test):1:8: error: __builtin_intern data must not be NULL\n"),
        },
        {
            "compile: null source with nonzero count", __LINE__,
            SVI("return __compile(((const char*)0)[:1], \"\") != nullptr;\n"),
            SVI("(test):1:8: error: __compile source data must not be NULL\n"),
        },
        {
            "compile: null path with nonzero count", __LINE__,
            SVI("return __compile(\"int x;\", ((const char*)0)[:1]) != nullptr;\n"),
            SVI("(test):1:8: error: __compile path data must not be NULL\n"),
        },
        {
            "reflection: null parse type name", __LINE__,
            SVI("_Module m = __root_module();\n"
                "return m.parse_type(((const char*)0)[:1]) == int;\n"),
            SVI("(test):2:9: error: _Module.parse_type name must not be NULL\n"),
        },
        {
            "reflection: discarded enumerator bounds", __LINE__,
            SVI("enum E { A }; _Type t = enum E;\n"
                "t.enumerator(1);\n"),
            SVI("(test):2:2: error: _Type.enumerator: index out of range\n"),
        },
        {
            "reflection: discarded param bounds", __LINE__,
            SVI("_Type t = int(int);\n"
                "t.param_type(1);\n"),
            SVI("(test):2:2: error: _Type.param_type: index out of range\n"),
        },
        {
            "reflection: field validation before index", __LINE__,
            SVI("_Type t = int;\n"
                "t.field((__builtin_trap(), 0));\n"),
            SVI("(test):2:2: error: _Type.field: not a struct or union type\n"),
        },
        {
            "reflection: enumerator validation before index", __LINE__,
            SVI("_Type t = int;\n"
                "t.enumerator((__builtin_trap(), 0));\n"),
            SVI("(test):2:2: error: _Type.enumerator: not an enum type\n"),
        },
        {
            "reflection: param validation before index", __LINE__,
            SVI("_Type t = int;\n"
                "t.param_type((__builtin_trap(), 0));\n"),
            SVI("(test):2:2: error: _Type.param_type: not a function type\n"),
        },
        {
            "reflection: field validation before name", __LINE__,
            SVI("_Type t = int;\n"
                "t.field((__builtin_trap(), \"x\"[:1]));\n"),
            SVI("(test):2:2: error: _Type.field: not a struct or union type\n"),
        },
        {
            "reflection: missing field name", __LINE__,
            SVI("struct S { int hello; }; _Type t = struct S;\n"
                "const char name[:] = \"hello\"[:4];\n"
                "t.field(name);\n"),
            SVI("(test):3:2: error: _Type.field: no field with that name\n"),
        },
        {
            "reflection: empty field name", __LINE__,
            SVI("struct S { int x; }; _Type t = struct S;\n"
                "const char name[:] = {};\n"
                "t.field(name);\n"),
            SVI("(test):3:2: error: _Type.field: no field with that name\n"),
        },
        {
            "reflection: discarded field bounds", __LINE__,
            SVI("struct S { int x; }; _Type t = struct S;\n"
                "t.field(1);\n"),
            SVI("(test):2:2: error: _Type.field: index out of range\n"),
        },
        {
            "reflection: field rejects method name", __LINE__,
            SVI("struct S { int x; int get(void){return 1;} }; _Type t = struct S;\n"
                "t.field(\"get\");\n"),
            SVI("(test):2:2: error: _Type.field: no field with that name\n"),
        },
        {
            "reflection: field index excludes methods", __LINE__,
            SVI("struct S { int x; int get(void){return 1;} }; _Type t = struct S;\n"
                "t.field(1);\n"),
            SVI("(test):2:2: error: _Type.field: index out of range\n"),
        },
        {
            "reflection: method rejects field name", __LINE__,
            SVI("struct S { int x; int get(void){return 1;} }; _Type t = struct S;\n"
                "t.method(\"x\");\n"),
            SVI("(test):2:2: error: _Type.method: no method with that name\n"),
        },
        {
            "reflection: method index excludes fields", __LINE__,
            SVI("struct S { int x; int get(void){return 1;} }; _Type t = struct S;\n"
                "t.method(1);\n"),
            SVI("(test):2:2: error: _Type.method: index out of range\n"),
        },
        {
            "reflection: empty method name", __LINE__,
            SVI("struct S { int get(void){return 1;} }; _Type t = struct S;\n"
                "t.method(\"\");\n"),
            SVI("(test):2:2: error: _Type.method: no method with that name\n"),
        },
        {
            "reflection: negative method index", __LINE__,
            SVI("struct S { int get(void){return 1;} }; _Type t = struct S;\n"
                "t.method(-1);\n"),
            SVI("(test):2:2: error: _Type.method: index out of range\n"),
        },
        {
            "reflection: method validation before name", __LINE__,
            SVI("_Type t = int;\n"
                "t.method((__builtin_trap(), \"get\"));\n"),
            SVI("(test):2:2: error: _Type.method: not a struct or union type\n"),
        },
        {
            "reflection: methods requires aggregate", __LINE__,
            SVI("_Type t = int;\n"
                "return t.methods;\n"),
            SVI("(test):2:9: error: _Type.methods: not a struct or union type\n"),
        },
        {
            "array store past end", __LINE__,
            SVI("int a[3];\n"
                "a[3] = 1;\n"
                "return 0;\n"),
            SVI("(test):2:2: error: array subscript out of bounds: index 3 not in [0, 3)\n"),
        },
        {
            "array read past end", __LINE__,
            SVI("int a[3]; a[0] = 0;\n"
                "return a[5];\n"),
            SVI("(test):2:9: error: array subscript out of bounds: index 5 not in [0, 3)\n"),
        },
        {
            "array negative index", __LINE__,
            SVI("int a[3]; a[0] = 0;\n"
                "int i = -1;\n"
                "return a[i];\n"),
            SVI("(test):3:9: error: array subscript out of bounds: index -1 not in [0, 3)\n"),
        },
        {
            "array negative __int128 index", __LINE__,
            SVI("int a[3]; a[0] = 0;\n"
                "__int128 i = -1;\n"
                "return a[i];\n"),
            SVI("(test):3:9: error: array subscript out of bounds: index -1 not in [0, 3)\n"),
        },
        {
            "slice past end", __LINE__,
            SVI("int a[3]; a[0]=1; a[1]=2; a[2]=3;\n"
                "int s[:] = a[:];\n"
                "return s[3];\n"),
            SVI("(test):3:9: error: array subscript out of bounds: index 3 not in [0, 3)\n"),
        },
        {
            "slice high bound past end: array", __LINE__,
            SVI("int a[3] = {1,2,3};\n"
                "int s[:] = a[:5];\n"
                "return s[0];\n"),
            SVI("(test):2:13: error: array subscript out of bounds: index 5 not in [0, 3]\n"),
        },
        {
            "slice low bound past end: array", __LINE__,
            SVI("int a[3] = {1,2,3};\n"
                "int s[:] = a[4:];\n"
                "return s[0];\n"),
            SVI("(test):2:13: error: array subscript out of bounds: index 4 not in [0, 3]\n"),
        },
        {
            "slice reversed bounds: array", __LINE__,
            SVI("int a[3] = {1,2,3};\n"
                "int s[:] = a[2:1];\n"
                "return s[0];\n"),
            SVI("(test):2:13: error: array subscript out of bounds: index 2 not in [0, 1]\n"),
        },
        {
            "slice negative low bound: array", __LINE__,
            SVI("int a[3] = {1,2,3};\n"
                "int i = -1;\n"
                "int s[:] = a[i:];\n"
                "return s[0];\n"),
            SVI("(test):3:13: error: array subscript out of bounds: index -1 not in [0, 3]\n"),
        },
        {
            "slice high bound past end: slice", __LINE__,
            SVI("int a[3] = {1,2,3};\n"
                "int x[:] = a;\n"
                "int s[:] = x[:5];\n"
                "return s[0];\n"),
            SVI("(test):3:13: error: array subscript out of bounds: index 5 not in [0, 3]\n"),
        },
        {
            "slice negative high bound: pointer", __LINE__,
            SVI("int a[3] = {1,2,3};\n"
                "int* p = a;\n"
                "int h = -1;\n"
                "int s[:] = p[:h];\n"
                "return s[0];\n"),
            SVI("(test):4:13: error: array subscript out of bounds: index -1 not in [0, 9223372036854775807]\n"),
        },
        {
            "addr past one-past-end: array", __LINE__,
            SVI("int a[3] = {1, 2, 3};\n"
                "int *p = &a[4];\n"
                "return *p;\n"),
            SVI("(test):2:12: error: array subscript out of bounds: index 4 not in [0, 3]\n"),
        },
        {
            "addr negative: array", __LINE__,
            SVI("int a[3] = {1, 2, 3};\n"
                "int *p = &a[-1];\n"
                "return *p;\n"),
            SVI("(test):2:12: error: array subscript out of bounds: index -1 not in [0, 3]\n"),
        },
        {
            "addr past one-past-end: slice", __LINE__,
            SVI("int a[3] = {1, 2, 3};\n"
                "int s[:] = a;\n"
                "int *p = &s[4];\n"
                "return *p;\n"),
            SVI("(test):3:12: error: array subscript out of bounds: index 4 not in [0, 3]\n"),
        },
        {
            "addr negative: slice", __LINE__,
            SVI("int a[3] = {1, 2, 3};\n"
                "int s[:] = a;\n"
                "int *p = &s[-1];\n"
                "return *p;\n"),
            SVI("(test):3:12: error: array subscript out of bounds: index -1 not in [0, 3]\n"),
        },
        {
            "addr past one-past-end: struct wrapping array (not last member)", __LINE__,
            SVI("struct {int a[3]; int x;} s = {1, 2, 3};\n"
                "int *p = &s.a[4];\n"
                "return *p;\n"),
            SVI("(test):2:14: error: array subscript out of bounds: index 4 not in [0, 3]\n"),
        },
        {
            "addr negative: struct wrapping array (not last member)", __LINE__,
            SVI("struct {int a[3]; int x; } s = {1, 2, 3};\n"
                "int *p = &s.a[-1];\n"
                "return *p;\n"),
            SVI("(test):2:14: error: array subscript out of bounds: index -1 not in [0, 3]\n"),
        },
        {
            "addr past one-past-end: struct wrapping array", __LINE__,
            SVI("struct {int a[3]; } s = {1, 2, 3};\n"
                "int *p = &s.a[4];\n"
                "return *p;\n"),
            SVI("(test):2:14: error: array subscript out of bounds: index 4 not in [0, 3]\n"),
        },
        {
            "addr negative: struct wrapping array", __LINE__,
            SVI("struct {int a[3]; } s = {1, 2, 3};\n"
                "int *p = &s.a[-1];\n"
                "return *p;\n"),
            SVI("(test):2:14: error: array subscript out of bounds: index -1 not in [0, 3]\n"),
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
                    .target = cc_target_funcs[CC_TARGET_TEST](),
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
        fc_write_path(fc, "(test)", 6);
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

        CiInterpFrame* frame = &interp.top_frame;
        _Bool trapped = 0;
        err = cc_parse_all(&interp.parser);
        if(tc->parser_error){
            trapped = err != 0;
            goto check_error;
        }
        if(err){TestPrintf("%s:%d: failed to parse\n", __FILE__, tc->line); goto finally;}
        // Expected lowering failures must be diagnosed before executing code.
        err = ci_prepare_toplevel(&interp);
        if(err) trapped = 1;
        if(tc->lowering_error) TestExpectTrue(_Bool, trapped);
        while(!tc->lowering_error && !trapped && frame->pc < frame->op_count){
            err = ci_interp_step(&interp, frame);
            if(err) trapped = 1;
        }
        check_error:
        err = 0; // the trap is the expected outcome, not a harness failure
        TEST_stats.executed++;
        if(!trapped){
            TEST_stats.failures++;
            TestPrintf("%s:%d: %s: expected runtime error but program succeeded\n", __FILE__, tc->line, tc->name);
        }
        StringView log = msb_borrow_sv(&log_sb);
        test_expect_equals_sv(tc->expect, log, "expected error", "actual error", &TEST_stats, __FILE__, __func__, tc->line);

        finally:
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

TestFunction(test_interpreter_builtin_headers){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    struct tc {
        const char* name; int line;
        StringView program;
        int exit_code;
    } testcases[] = {
        {
            "stdatomic: typedefs use atomic-qualified types", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_int x = ATOMIC_VAR_INIT(1);\n"
               "atomic_init(&x, 2);\n"
               "int old = atomic_fetch_add(&x, 5);\n"
               "return old * 10 + atomic_load(&x);\n"),
            .exit_code = 27,
        },
        {
            "stdatomic: bitwise fetch operations return old value", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_uint x = 0xF0u;\n"
               "unsigned a = atomic_fetch_or(&x, 0x0Fu);\n"
               "unsigned b = atomic_fetch_and_explicit(&x, 0x33u, memory_order_seq_cst);\n"
               "unsigned c = atomic_fetch_xor(&x, 0x30u);\n"
               "return (a == 0xF0u) && (b == 0xFFu) && (c == 0x33u) && (x == 0x03u);\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: parenthesized _Atomic remains keyword after include", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "_Atomic(int) x = 4;\n"
               "atomic_store(&x, 9);\n"
               "return atomic_load(&x);\n"),
            .exit_code = 9,
        },
        {
            "stdatomic: exchange and compare-exchange", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_int x = 10;\n"
               "int old = atomic_exchange(&x, 20);\n"
               "int expected = 20;\n"
               "bool ok1 = atomic_compare_exchange_strong(&x, &expected, 30);\n"
               "expected = 99;\n"
               "bool ok2 = atomic_compare_exchange_weak_explicit(&x, &expected, 40, memory_order_seq_cst, memory_order_relaxed);\n"
               "return old == 10 && ok1 && !ok2 && expected == 30 && x == 30;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: flag test and clear", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_flag f = ATOMIC_FLAG_INIT;\n"
               "bool a = atomic_flag_test_and_set(&f);\n"
               "bool b = atomic_flag_test_and_set_explicit(&f, memory_order_seq_cst);\n"
               "atomic_flag_clear(&f);\n"
               "bool c = atomic_flag_test_and_set(&f);\n"
               "return !a && b && !c;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: scalar typedef load store", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_bool b = false;\n"
               "atomic_char c = 1;\n"
               "atomic_long l = 2;\n"
               "atomic_llong ll = 3;\n"
               "atomic_store(&b, true);\n"
               "atomic_store_explicit(&c, 4, memory_order_release);\n"
               "atomic_store(&l, 5);\n"
               "atomic_store(&ll, 6);\n"
               "return atomic_load_explicit(&b, memory_order_acquire)\n"
               "    && atomic_load(&c) == 4\n"
               "    && atomic_load(&l) == 5\n"
               "    && atomic_load(&ll) == 6;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: atomic_init aggregate initializer", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "struct S { int x, y; };\n"
               "_Atomic(struct S) s;\n"
               "atomic_init(&s, {3, 4});\n"
               "struct S t = s;\n"
               "return t.x * 10 + t.y;\n"),
            .exit_code = 34,
        },
        {
            "stdatomic: volatile atomic object API", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "volatile atomic_int x = 0;\n"
               "atomic_store(&x, 3);\n"
               "int a = atomic_load(&x);\n"
               "atomic_store_explicit(&x, 4, memory_order_release);\n"
               "int b = atomic_load_explicit(&x, memory_order_acquire);\n"
               "return a * 10 + b;\n"),
            .exit_code = 34,
        },
        {
            "stdatomic: remaining integer typedefs", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_schar sc = -1;\n"
               "atomic_uchar uc = 2;\n"
               "atomic_short sh = 3;\n"
               "atomic_ushort ush = 4;\n"
               "atomic_ulong ul = 5;\n"
               "atomic_ullong ull = 6;\n"
               "atomic_store(&sc, -7);\n"
               "atomic_store(&uc, 8);\n"
               "atomic_store(&sh, -9);\n"
               "atomic_store(&ush, 10);\n"
               "atomic_store(&ul, 11);\n"
               "atomic_store(&ull, 12);\n"
               "return atomic_load(&sc) == -7\n"
               "    && atomic_load(&uc) == 8\n"
               "    && atomic_load(&sh) == -9\n"
               "    && atomic_load(&ush) == 10\n"
               "    && atomic_load(&ul) == 11\n"
               "    && atomic_load(&ull) == 12;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: atomic_is_lock_free", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_int x = 0;\n"
               "atomic_llong y = 0;\n"
               "return atomic_is_lock_free(&x) && atomic_is_lock_free(&y);\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: fetch macro evaluates object argument once", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_int arr[2] = {10, 20};\n"
               "int idx = 0;\n"
               "int calls = 0;\n"
               "atomic_int *next(void){ calls++; return &arr[idx++]; }\n"
               "int old = atomic_fetch_add(next(), 5);\n"
               "return calls == 1 && idx == 1 && old == 10 && arr[0] == 15 && arr[1] == 20;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: compare exchange macro evaluates desired once", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_int x = 1;\n"
               "int expected = 1;\n"
               "int calls = 0;\n"
               "int desired(void){ calls++; return 2; }\n"
               "bool ok = atomic_compare_exchange_strong(&x, &expected, desired());\n"
               "return ok && calls == 1 && expected == 1 && x == 2;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: pointer fetch add and sub", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "int arr[5] = {1, 2, 3, 4, 5};\n"
               "_Atomic(int*) p = arr;\n"
               "int *old_add = atomic_fetch_add(&p, 2);\n"
               "int *old_sub = atomic_fetch_sub_explicit(&p, 1, memory_order_seq_cst);\n"
               "return old_add == arr && old_sub == arr + 2 && p == arr + 1 && *p == 2;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: least fast and max typedefs", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_int_least8_t a = 1;\n"
               "atomic_uint_least16_t b = 2;\n"
               "atomic_int_least32_t c = 3;\n"
               "atomic_uint_least64_t d = 4;\n"
               "atomic_int_fast8_t e = 5;\n"
               "atomic_uint_fast16_t f = 6;\n"
               "atomic_int_fast32_t g = 7;\n"
               "atomic_uint_fast64_t h = 8;\n"
               "atomic_intmax_t i = 9;\n"
               "atomic_uintmax_t j = 10;\n"
               "atomic_store(&a, -11);\n"
               "atomic_store(&b, 12);\n"
               "atomic_store(&c, -13);\n"
               "atomic_store(&d, 14);\n"
               "atomic_store(&e, -15);\n"
               "atomic_store(&f, 16);\n"
               "atomic_store(&g, -17);\n"
               "atomic_store(&h, 18);\n"
               "atomic_store(&i, -19);\n"
               "atomic_store(&j, 20);\n"
               "return atomic_load(&a) == -11\n"
               "    && atomic_load(&b) == 12\n"
               "    && atomic_load(&c) == -13\n"
               "    && atomic_load(&d) == 14\n"
               "    && atomic_load(&e) == -15\n"
               "    && atomic_load(&f) == 16\n"
               "    && atomic_load(&g) == -17\n"
               "    && atomic_load(&h) == 18\n"
               "    && atomic_load(&i) == -19\n"
               "    && atomic_load(&j) == 20;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: pointer sized typedefs", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_intptr_t ip = 1;\n"
               "atomic_uintptr_t up = 2;\n"
               "atomic_size_t sz = 3;\n"
               "atomic_ptrdiff_t pd = -4;\n"
               "atomic_store(&ip, -5);\n"
               "atomic_store(&up, 6);\n"
               "atomic_store(&sz, 7);\n"
               "atomic_store(&pd, -8);\n"
               "return atomic_load(&ip) == -5\n"
               "    && atomic_load(&up) == 6\n"
               "    && atomic_load(&sz) == 7\n"
               "    && atomic_load(&pd) == -8;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: character typedefs", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_char16_t c16 = 1;\n"
               "atomic_char32_t c32 = 2;\n"
               "atomic_wchar_t wc = 3;\n"
               "atomic_store(&c16, 4);\n"
               "atomic_store(&c32, 5);\n"
               "atomic_store(&wc, 6);\n"
               "return atomic_load(&c16) == 4 && atomic_load(&c32) == 5 && atomic_load(&wc) == 6;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: kill dependency evaluates once", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "int calls = 0;\n"
               "int next(void){ calls++; return 7; }\n"
               "int x = kill_dependency(next());\n"
               "return calls * 10 + x;\n"),
            .exit_code = 17,
        },
        {
            "stdatomic: fence macros accept memory orders", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_thread_fence(memory_order_seq_cst);\n"
               "atomic_thread_fence(memory_order_acquire);\n"
               "atomic_signal_fence(memory_order_release);\n"
               "atomic_signal_fence(memory_order_relaxed);\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: aggregate exchange", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "struct S { int x, y; };\n"
               "_Atomic(struct S) a = {1, 2};\n"
               "struct S desired = {3, 4};\n"
               "struct S old = atomic_exchange(&a, desired);\n"
               "struct S now = atomic_load(&a);\n"
               "return old.x == 1 && old.y == 2 && now.x == 3 && now.y == 4;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: aggregate compare exchange", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "struct S { int x, y; };\n"
               "_Atomic(struct S) a = {1, 2};\n"
               "struct S expected = {1, 2};\n"
               "struct S desired = {3, 4};\n"
               "bool ok1 = atomic_compare_exchange_strong(&a, &expected, desired);\n"
               "expected = (struct S){9, 9};\n"
               "desired = (struct S){5, 6};\n"
               "bool ok2 = atomic_compare_exchange_weak(&a, &expected, desired);\n"
               "struct S now = atomic_load(&a);\n"
               "return ok1 && !ok2 && expected.x == 3 && expected.y == 4 && now.x == 3 && now.y == 4;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: aggregate load and store explicit", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "struct S { int x, y; };\n"
               "_Atomic(struct S) a = {1, 2};\n"
               "struct S b = {7, 8};\n"
               "atomic_store_explicit(&a, b, memory_order_release);\n"
               "struct S c = atomic_load_explicit(&a, memory_order_acquire);\n"
               "return c.x * 10 + c.y;\n"),
            .exit_code = 78,
        },
        {
            "stdatomic: scalar atomic_init evaluates arguments once", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "atomic_int a[2] = {1, 2};\n"
               "int idx = 0;\n"
               "int calls = 0;\n"
               "atomic_int *next_obj(void){ calls++; return &a[idx++]; }\n"
               "int next_val(void){ calls++; return 9; }\n"
               "atomic_init(next_obj(), next_val());\n"
               "return calls == 2 && idx == 1 && a[0] == 9 && a[1] == 2;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic: atomic flag is atomic-qualified", __LINE__,
            SVI("#include <stdatomic.h>\n"
               "return _Generic(atomic_flag, _Atomic bool: 1, bool: 2, default: 3);\n"),
            .exit_code = 1,
        },
    };
    int err;
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < sizeof testcases/sizeof testcases[0]; i = test_atomic_increment(&idx)){
        struct tc* tc = &testcases[i];
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
                    .target = cc_target_funcs[CC_TARGET_TEST](),
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
        err = cpp_setup_builtin_headers(&interp.parser.cpp);
        if(err){TestReport("setup failure"); goto finally;}
        err = cc_define_builtin_types(&interp.parser);
        if(err){TestReport("setup failure"); goto finally;}
        err = cc_register_pragmas(&interp.parser);
        if(err){TestReport("setup failure"); goto finally;}
        err = ci_register_pragmas(&interp);
        if(err){TestReport("setup failure"); goto finally;}
        err = ci_register_macros(&interp);
        if(err){TestReport("setup failure"); goto finally;}

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
TestFunction(test_cross_target){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    static struct tc {
        const char* name; int line;
        StringView program;
        int exit_code;
        _Bool skip;
        CcTarget target;
    } testcases[] = {
        {
            "review: static binary128 bool conversion", __LINE__,
            SVI("static _Bool value=0.5L; return value;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "review: static x87 bool conversion", __LINE__,
            SVI("static _Bool value=0.5L; return value;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "static float128: precision, arithmetic, and conversions on x87 target", __LINE__,
            SVI("constexpr _Float128 q=(_Float128)(((unsigned __int128)1<<100)+1);\n"
                "static unsigned __int128 n=(unsigned __int128)(q+(_Float128)1);\n"
                "static float f=(float)(_Float128)1.25;\n"
                "static double d=(double)(_Float128)2.5;\n"
                "static long double l=(long double)(_Float128)3.5;\n"
                "return n==(((unsigned __int128)1<<100)+2) && f==1.25f && d==2.5 && l==3.5L;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "static long double retains binary128 integer precision", __LINE__,
            SVI("static long double value=(long double)(((unsigned __int128)1<<100)+1);\n"
                "return value-(long double)((unsigned __int128)1<<100)==1.L;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "static binary128 converts precisely to wide integer", __LINE__,
            SVI("static unsigned __int128 value=(unsigned __int128)0x1.0000000000000000000000001p100L;\n"
                "return value==(((unsigned __int128)1<<100)+1);\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "static binary128 rounds directly to float", __LINE__,
            SVI("static float value=(float)0x1.0000010000000000000000001p0L;\n"
                "return value>1.f;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "long double literal precision and range X86_64_LINUX", __LINE__,
            SVI("constexpr long double x=0x1.000000000000001p60L;\n"
                "_Static_assert(x-0x1p60L==1.L);\n"
                "_Static_assert(0x1p4000L/0x1p3999L==2.L);\n"
                "_Static_assert(0x1p-4000L && -0x1p-4000L<0.L);\n"
                "volatile long double y=0x1.000000000000001p60L;\n"
                "return y-0x1p60L==1.L;"),
            .exit_code=1, .target=CC_TARGET_X86_64_LINUX,
        },
        {
            "long double literal precision and range AARCH64_LINUX", __LINE__,
            SVI("constexpr long double x=0x1.0000000000000000000000001p100L;\n"
                "_Static_assert(x-0x1p100L==1.L);\n"
                "_Static_assert(0x1p4000L/0x1p3999L==2.L);\n"
                "_Static_assert(0x1p-4000L && -0x1p-4000L<0.L);\n"
                "volatile long double y=0x1.0000000000000000000000001p100L;\n"
                "return y-0x1p100L==1.L;"),
            .exit_code=1, .target=CC_TARGET_AARCH64_LINUX,
        },
        {"wide long double x87 runtime", __LINE__,
            SVI("long double x=0x1p60L+1.L;\n"
                "volatile long double y=x;\n"
                "return y-0x1p60L==1.L;"), .exit_code=1, .target=CC_TARGET_X86_64_LINUX},
        {"wide long double x87 constexpr", __LINE__,
            SVI("constexpr long double x=0x1p60L+1.L;\n"
                "_Static_assert(x-0x1p60L==1.L);\n"
                "_Static_assert(!(-0.L));\n"
                "_Static_assert(x && (1.L ? 1 : 0));\n"
                "volatile long double y=x;\n"
                "return y-0x1p60L==1.L;"), .exit_code=1, .target=CC_TARGET_X86_64_LINUX},
        {"wide long double binary128 runtime", __LINE__,
            SVI("long double x=0x1p100L+1.L;\n"
                "volatile long double y=x;\n"
                "return y-0x1p100L==1.L;"), .exit_code=1, .target=CC_TARGET_AARCH64_LINUX},
        {"wide long double binary128 constexpr", __LINE__,
            SVI("constexpr long double x=0x1p100L+1.L;\n"
                "_Static_assert(x-0x1p100L==1.L);\n"
                "_Static_assert(!(-0.L));\n"
                "_Static_assert(x && (1.L ? 1 : 0));\n"
                "volatile long double y=x;\n"
                "return y-0x1p100L==1.L;"), .exit_code=1, .target=CC_TARGET_AARCH64_LINUX},
        {"wide long double binary64 runtime", __LINE__,
            SVI("long double x=0x1p50L+1.L;\n"
                "volatile long double y=x;\n"
                "return y-0x1p50L==1.L;"), .exit_code=1, .target=CC_TARGET_X86_64_WINDOWS},
        {"wide long double binary64 constexpr", __LINE__,
            SVI("constexpr long double x=0x1p50L+1.L;\n"
                "_Static_assert(x-0x1p50L==1.L);\n"
                "_Static_assert(!(-0.L));\n"
                "_Static_assert(x && (1.L ? 1 : 0));\n"
                "volatile long double y=x;\n"
                "return y-0x1p50L==1.L;"), .exit_code=1, .target=CC_TARGET_X86_64_WINDOWS},
        {
            "constexpr wide conversions", __LINE__,
            SVI("constexpr long double a=42;\n"
                "constexpr _Float128 b=a;\n"
                "_Static_assert((float)b==42.f && (double)b==42.);\n"
                "_Static_assert((long long)b==42 && (unsigned long long)b==42);\n"
                "_Static_assert((__int128)-b==-42 && (bool)b && !(bool)-0.L);\n"
                "_Static_assert((long double)b==42.L);\n"
                "_Static_assert(((__int128)-1>>100)==-1);\n"
                "_Static_assert(((unsigned __int128)-1+1)==0);\n"
                "return 1;"),
            .exit_code=1, .target=CC_TARGET_X86_64_LINUX,
        },
        {"constexpr wide truth", __LINE__,
            SVI("constexpr unsigned __int128 x=(unsigned __int128)1<<100;\n"
                "_Static_assert(x && !(!x));\n"
                "volatile unsigned __int128 v=x;\n"
                "return v==x;"),
            .exit_code=1, .target=CC_TARGET_X86_64_LINUX},
        {"constexpr wide add", __LINE__,
            SVI("constexpr unsigned __int128 x=(unsigned __int128)1<<100;\n"
                "_Static_assert((x+3)-x==3);\n"
                "volatile unsigned __int128 v=x;\n"
                "return v==x;"),
            .exit_code=1, .target=CC_TARGET_X86_64_LINUX},
        {"constexpr wide mul", __LINE__,
            SVI("constexpr unsigned __int128 x=(unsigned __int128)1<<100;\n"
                "_Static_assert((x*7)/7==x && (x*7)%7==0);\n"
                "volatile unsigned __int128 v=x;\n"
                "return v==x;"),
            .exit_code=1, .target=CC_TARGET_X86_64_LINUX},
        {"constexpr wide bits", __LINE__,
            SVI("constexpr unsigned __int128 x=(unsigned __int128)1<<100;\n"
                "_Static_assert((~x & x)==0 && ((x|3)^x)==3);\n"
                "volatile unsigned __int128 v=x;\n"
                "return v==x;"),
            .exit_code=1, .target=CC_TARGET_X86_64_LINUX},
        {"constexpr wide neg", __LINE__,
            SVI("constexpr unsigned __int128 x=(unsigned __int128)1<<100;"
                "constexpr __int128 n=-(__int128)x;\n"
                "_Static_assert(n<0 && -n==x);\n"
                "volatile unsigned __int128 v=x;\n"
                "return v==x;"),
            .exit_code=1, .target=CC_TARGET_X86_64_LINUX},
        {"constexpr wide aggregate", __LINE__,
            SVI("constexpr unsigned __int128 x=(unsigned __int128)1<<100;"
                "struct S {unsigned __int128 x;\n"
                "   long double y;\n"
                "};\n"
                "constexpr struct S s={x,0x1.000000000000001p60L};\n"
                "_Static_assert(s.x==x && s.y-0x1p60L==1.L);\n"
                "volatile long double d=s.y;"
                "volatile unsigned __int128 v=x;\n"
                "return v==x && d-0x1p60L==1.L;"),
            .exit_code=1, .target=CC_TARGET_X86_64_LINUX},
        {"constexpr wide quad", __LINE__,
            SVI("constexpr unsigned __int128 x=(unsigned __int128)1<<100;"
                "constexpr _Float128 q=(_Float128)x+1;\n"
                "_Static_assert(q-(_Float128)x==1);\n"
                "_Static_assert((unsigned __int128)q==x+1);"
                "volatile unsigned __int128 v=x;\n"
                "return v==x;"),
            .exit_code=1, .target=CC_TARGET_X86_64_LINUX},

        {
            "any: layout and target long width", __LINE__,
            SVI("_Any a=(long)0x100000001ull;\n"
                "return sizeof(_Any)==16 && alignof(_Any)==8\n"
                " && a.type==long && a.as(long)==(long)0x100000001ull;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "any: layout and target long width", __LINE__,
            SVI("_Any a=(long)0x100000001ull;\n"
                "return sizeof(_Any)==16 && alignof(_Any)==8\n"
                " && a.type==long && a.as(long)==(long)0x100000001ull;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "any: layout and target long width", __LINE__,
            SVI("_Any a=(long)0x100000001ull;\n"
                "return sizeof(_Any)==16 && alignof(_Any)==8\n"
                " && a.type==long && a.as(long)==(long)0x100000001ull;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_MACOS,
        },
        {
            "any: layout and target long width", __LINE__,
            SVI("_Any a=(long)0x100000001ull;\n"
                "return sizeof(_Any)==16 && alignof(_Any)==8\n"
                " && a.type==long && a.as(long)==(long)0x100000001ull;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_MACOS,
        },
        {
            "any: layout and target long width", __LINE__,
            SVI("_Any a=(long)0x100000001ull;\n"
                "return sizeof(_Any)==16 && alignof(_Any)==8\n"
                " && a.type==long && a.as(long)==(long)0x100000001ull;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_WINDOWS,
        },
        {
            "fold: windows long truncates", __LINE__,
            SVI("return (long)0x100000001ULL == 1;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_WINDOWS,
        },
        {
            "fold: linux long preserves high bits", __LINE__,
            SVI("return (long)0x100000001ULL == 0x100000001ULL;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },

        {
            "sysv va_list param", __LINE__,
            SVI(
               "#define va_start __builtin_va_start\n"
               "#define va_arg __builtin_va_arg\n"
               "#define va_end __builtin_va_end\n"
               "#define va_copy __builtin_va_copy\n"
               "typedef __builtin_va_list va_list;\n"
               "int vsum(int n, va_list ap){\n"
               "    int sum = 0;\n"
               "    for(int i = 0; i < n; i++){\n"
               "        sum += va_arg(ap, int);\n"
               "    }\n"
               "    return sum;\n"
               "}\n"
               "int sum(int n, ...){\n"
               "    va_list ap;\n"
               "    va_start(ap, n);\n"
               "    int result = vsum(n, ap);\n"
               "    va_end(ap);\n"
               "    return result;\n"
               "}\n"
               "return sum(3, 4, 5, 6);\n"
               ),
            .exit_code = 15,
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "sysv va_copy param", __LINE__,
            SVI(
               "#define va_start __builtin_va_start\n"
               "#define va_arg __builtin_va_arg\n"
               "#define va_end __builtin_va_end\n"
               "#define va_copy __builtin_va_copy\n"
               "typedef __builtin_va_list va_list;\n"
               "int vsum(int n, va_list ap){\n"
               "    va_list ap2;\n"
               "    va_copy(ap2, ap);\n"
               "    int sum = 0;\n"
               "    for(int i = 0; i < n; i++){\n"
               "        sum += va_arg(ap2, int);\n"
               "    }\n"
               "    va_end(ap2);\n"
               "    return sum;\n"
               "}\n"
               "int sum(int n, ...){\n"
               "    va_list ap;\n"
               "    va_start(ap, n);\n"
               "    int result = vsum(n, ap);\n"
               "    va_end(ap);\n"
               "    return result;\n"
               "}\n"
               "return sum(3, 4, 5, 6);\n"
               ),
            .exit_code = 15,
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "aarch64 linux va_list param", __LINE__,
            SVI(
               "#define va_start __builtin_va_start\n"
               "#define va_arg __builtin_va_arg\n"
               "#define va_end __builtin_va_end\n"
               "typedef __builtin_va_list va_list;\n"
               "int vsum(int n, va_list ap){\n"
               "    int sum = 0;\n"
               "    for(int i = 0; i < n; i++){\n"
               "        sum += va_arg(ap, int);\n"
               "    }\n"
               "    return sum;\n"
               "}\n"
               "int sum(int n, ...){\n"
               "    va_list ap;\n"
               "    va_start(ap, n);\n"
               "    int result = vsum(n, ap);\n"
               "    va_end(ap);\n"
               "    return result;\n"
               "}\n"
               "return sum(3, 4, 5, 6);\n"
               ),
            .exit_code = 15,
            .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "windows sizeof long", __LINE__,
            SVI("return sizeof(long);\n"),
            .exit_code = 4,
            .target = CC_TARGET_X86_64_WINDOWS,
        },
        {
            "linux sizeof long", __LINE__,
            SVI("return sizeof(long);\n"),
            .exit_code = 8,
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "windows struct layout with long", __LINE__,
            SVI("typedef struct { char c; long x; } S;\n"
               "return sizeof(S);\n"),
            .exit_code = 8, // 1 + 3 pad + 4
            .target = CC_TARGET_X86_64_WINDOWS,
        },
        {
            "linux struct layout with long", __LINE__,
            SVI("typedef struct { char c; long x; } S;\n"
               "return sizeof(S);\n"),
            .exit_code = 16, // 1 + 7 pad + 8
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "aarch64 linux char unsigned", __LINE__,
            SVI("char c = 255;\n"
               "return c > 0;\n"),
            .exit_code = 1,
            .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "x86_64 linux char signed", __LINE__,
            SVI("char c = 255;\n"
               "return c < 0;\n"),
            .exit_code = 1,
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "windows long overflow", __LINE__,
            SVI("unsigned long x = 0xFFFFFFFF;\n"
               "return (x + 1) == 0;\n"),
            .exit_code = 1, // 32-bit wraps
            .target = CC_TARGET_X86_64_WINDOWS,
        },
        {
            "linux long no overflow", __LINE__,
            SVI("unsigned long x = 0xFFFFFFFF;\n"
               "return (x + 1) == 0x100000000;\n"),
            .exit_code = 1, // 64-bit doesn't wrap
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "aarch64 macos sizeof long double", __LINE__,
            SVI("return sizeof(long double);\n"),
            .exit_code = 8,
            .target = CC_TARGET_AARCH64_MACOS,
        },
        {
            "x86_64 linux sizeof long double", __LINE__,
            SVI("return sizeof(long double);\n"),
            .exit_code = 16,
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "x87: arithmetic and conversions", __LINE__,
            SVI("long double a = (long double)9007199254740993ull;\n"
                "long double b = (long double)9007199254740992ull;\n"
                "long double c = a - b;\n"
                "c *= 6;\n"
                "c /= 2;\n"
                "c++;\n"
                "--c;\n"
                "return c == 3 && -c < 0 && (_Bool)c && (unsigned long long)a == 9007199254740993ull;"),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "binary128 long double: arithmetic and conversions", __LINE__,
            SVI("long double a = (long double)9007199254740993ull;\n"
                "long double b = (long double)9007199254740992ull;\n"
                "long double c = a - b;\n"
                "c *= 6;\n"
                "c /= 2;\n"
                "c++;\n"
                "--c;\n"
                "return c == 3 && -c < 0 && (_Bool)c && (unsigned long long)a == 9007199254740993ull;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "float128: arithmetic and conversions", __LINE__,
            SVI("_Float128 a = (_Float128)9007199254740993ull;\n"
                "_Float128 b = (_Float128)9007199254740992ull;\n"
                "_Float128 c = a - b;\n"
                "c *= 6;\n"
                "c /= 2;\n"
                "c++;\n"
                "--c;\n"
                "return c == 3 && -c < 0 && (_Bool)c && (unsigned long long)a == 9007199254740993ull;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_MACOS,
        },
        {
            "extended literal sign and casts", __LINE__,
            SVI("long double a = -2.5L;\n"
                "long double z = -0.0L;\n"
                "return a < 0 && (double)a == -2.5 && !z;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "extended literal sign and casts", __LINE__,
            SVI("long double a = -2.5L;\n"
                "long double z = -0.0L;\n"
                "return a < 0 && (double)a == -2.5 && !z;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "long double: special values", __LINE__,
            SVI("long double z = (long double)0;\n"
                "long double n = z/z;\n"
                "long double inf = (long double)1/z;\n"
                "long double nz = -z;\n"
                "return n != n && !(n < z) && !(n >= z) && !(n <= z) && !(n > z) && (_Bool)n && inf > z && z == nz && !nz;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "long double: wide integer roundtrip", __LINE__,
            SVI("unsigned __int128 u = (unsigned __int128)1 << 100;\n"
                "__int128 i = -(__int128)u;\n"
                "long double a = (long double)u;\n"
                "long double b = (long double)i;\n"
                "return (unsigned __int128)a == u && (__int128)b == i && (int)(long double)-3.75 == -3;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "long double: memory increments and float conversions", __LINE__,
            SVI("long double a[1] = {(long double)1.5f};\n"
                "long double *p = a;\n"
                "long double old = (*p)++;\n"
                "--*p;\n"
                "*p += (long double)2.25;\n"
                "*p -= (long double)1;\n"
                "return (float)*p == 2.75f && (double)old == 1.5;\n"),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "long double: special values", __LINE__,
            SVI("long double z = (long double)0;\n"
                "long double n = z/z;\n"
                "long double inf = (long double)1/z;\n"
                "long double nz = -z;\n"
                "return n != n && !(n < z) && !(n >= z) && !(n <= z) && !(n > z) && (_Bool)n && inf > z && z == nz && !nz;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "long double: wide integer roundtrip", __LINE__,
            SVI("unsigned __int128 u = (unsigned __int128)1 << 100;\n"
                "__int128 i = -(__int128)u;\n"
                "long double a = (long double)u;\n"
                "long double b = (long double)i;\n"
                "return (unsigned __int128)a == u && (__int128)b == i && (int)(long double)-3.75 == -3;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "long double: memory increments and float conversions", __LINE__,
            SVI("long double a[1] = {(long double)1.5f};\n"
                "long double *p = a;\n"
                "long double old = (*p)++;\n"
                "--*p; *p += (long double)2.25;\n"
                "*p -= (long double)1;\n"
                "return (float)*p == 2.75f && (double)old == 1.5;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "_Float128: special values", __LINE__,
            SVI("_Float128 z = (_Float128)0;\n"
                "_Float128 n = z/z;\n"
                "_Float128 inf = (_Float128)1/z;\n"
                "_Float128 nz = -z;\n"
                "return n != n && !(n < z) && !(n >= z) && !(n <= z) && !(n > z) && (_Bool)n && inf > z && z == nz && !nz;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_MACOS,
        },
        {
            "_Float128: wide integer roundtrip", __LINE__,
            SVI("unsigned __int128 u = (unsigned __int128)1 << 100;\n"
                "__int128 i = -(__int128)u;\n"
                "_Float128 a = (_Float128)u;\n"
                "_Float128 b = (_Float128)i;\n"
                "return (unsigned __int128)a == u && (__int128)b == i && (int)(_Float128)-3.75 == -3;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_MACOS,
        },
        {
            "_Float128: memory increments and float conversions", __LINE__,
            SVI("_Float128 a[1] = {(_Float128)1.5f};\n"
                "_Float128 *p = a;\n"
                "_Float128 old = (*p)++;\n"
                "--*p; *p += (_Float128)2.25;\n"
                "*p -= (_Float128)1;\n"
                "return (float)*p == 2.75f && (double)old == 1.5;\n"),
            .exit_code = 1, .target = CC_TARGET_AARCH64_MACOS,
        },
        {
            "binary128 precision and x87 conversion", __LINE__,
            SVI("unsigned __int128 u = ((unsigned __int128)1 << 112) + 1;\n"
                " _Float128 q = (_Float128)u;\n"
                " _Float128 one = q - (_Float128)(u - 1);\n"
                " long double e = (long double)one;\n"
                " return (unsigned __int128)q == u && e == 1 && (_Float128)e == one;\n"
            ),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "long double: rounding ties and subnormal truth", __LINE__,
            SVI("unsigned __int128 u = ((unsigned __int128)1 << 64) + 1;\n"
                "long double a = (long double)u;\n"
                "long double b = (long double)(u + 2);\n"
                "union U { long double f;\n"
                "   unsigned long long bits[2];\n"
                "} tiny = {.bits = {1, 0}};\n"
                "return (unsigned __int128)a == u - 1 && (unsigned __int128)b == u + 3 && tiny.f > 0 && (_Bool)tiny.f;\n"
            ),
            .exit_code = 1, .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "_Float128: rounding ties and subnormal truth", __LINE__,
            SVI("unsigned __int128 u = ((unsigned __int128)1 << 113) + 1;\n"
                "_Float128 a = (_Float128)u;\n"
                "_Float128 b = (_Float128)(u + 2);\n"
                "union U { _Float128 f;\n"
                "   unsigned long long bits[2];\n"
                "} tiny = {.bits = {1, 0}};\n"
                "return (unsigned __int128)a == u - 1 && (unsigned __int128)b == u + 3 && tiny.f > 0 && (_Bool)tiny.f;\n"
            ),
            .exit_code = 1, .target = CC_TARGET_AARCH64_MACOS,
        },
        {
            "windows sizeof size_t", __LINE__,
            SVI("return sizeof(__SIZE_TYPE__);\n"),
            .exit_code = 8, // unsigned long long is 8
            .target = CC_TARGET_X86_64_WINDOWS,
        },
        // MSVC bitfield: different types don't share storage units
        {
            "msvc bitfield layout", __LINE__,
            SVI("typedef struct { unsigned short a: 8; unsigned int b: 8; } S;\n"
               "return sizeof(S);\n"),
            .exit_code = 8, // short unit (2) + pad(2) + int unit (4)
            .target = CC_TARGET_X86_64_WINDOWS,
        },
        {
            "sysv bitfield layout", __LINE__,
            SVI("typedef struct { unsigned short a: 8; unsigned int b: 8; } S;\n"
               "return sizeof(S);\n"),
            .exit_code = 4, // both fit in one 4-byte unit
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "enum", __LINE__,
            SVI("enum Foo { X } ; typedef struct { enum Foo f : 8; unsigned int b: 8; } S;\n"
               "return sizeof(S);\n"),
            .exit_code = 4, // enum then both fit in one 4-byte unit
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "msvc bitfield typed enum packs with underlying", __LINE__,
            SVI("enum A : unsigned int { A_VAL = 1 };\n"
               "enum B : unsigned int { B_VAL = 2 };\n"
               "typedef struct { enum A a: 4; unsigned int b: 1; enum B c: 1; } S;\n"
               "return sizeof(S);\n"),
            .exit_code = 4, // all have uint32_t underlying, pack together
            .target = CC_TARGET_X86_64_WINDOWS,
        },
        {
            "msvc bitfield diff size enum starts new unit", __LINE__,
            SVI("enum A : unsigned short { A_VAL = 1 };\n"
               "typedef struct { enum A a: 4; unsigned int b: 1; } S;\n"
               "return sizeof(S);\n"),
            .exit_code = 8, // short(2) + pad(2) + int(4)
            .target = CC_TARGET_X86_64_WINDOWS,
        },
        {
            "alignof", __LINE__,
            SVI("return _Alignof(__MAX_ALIGN_TYPE__);\n"),
            .exit_code = 16,
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "alignof", __LINE__,
            SVI("return _Alignof(__MAX_ALIGN_TYPE__);\n"),
            .exit_code = 16,
            .target = CC_TARGET_AARCH64_LINUX,
        },
        {
            "alignof", __LINE__,
            SVI("return _Alignof(__MAX_ALIGN_TYPE__);\n"),
            .exit_code = 16,
            .target = CC_TARGET_X86_64_MACOS,
        },
        {
            "alignof", __LINE__,
            SVI("return _Alignof(__MAX_ALIGN_TYPE__);\n"),
            .exit_code = 8,
            .target = CC_TARGET_AARCH64_MACOS,
        },
        {
            "alignof", __LINE__,
            SVI("return _Alignof(__MAX_ALIGN_TYPE__);\n"),
            .exit_code = 8,
            .target = CC_TARGET_X86_64_WINDOWS,
        },
        // abi smoke tests... don't have a direct way to test them in the cross interpreter
        {
            "sysv", __LINE__,
            SVI("struct S1 {int x;};\n"
                "struct S2 {int x; float y;};\n"
                "struct S3 {int x, y;};\n"
                "struct S4 {int x, y, z;};\n"
                "struct S5 {double x, y;};\n"
                "struct S6 {double x; int y, z;};\n"
                "struct S7 {double x[3]; int y[4], z;};\n"
                "struct S8 {float x[2]; double d;};\n"
                "struct S9 {float x[2]; float f[2];};\n"
                "return 12;\n"
            ),
            .exit_code = 12,
            .target = CC_TARGET_X86_64_LINUX,
        },
        {
            "win64", __LINE__,
            SVI("struct S1 {int x;};\n"
                "struct S2 {int x; float y;};\n"
                "struct S3 {int x, y;};\n"
                "struct S4 {int x, y, z;};\n"
                "struct S5 {double x, y;};\n"
                "struct S6 {double x; int y, z;};\n"
                "struct S7 {double x[3]; int y[4], z;};\n"
                "struct S8 {float x[2]; double d;};\n"
                "struct S9 {float x[2]; float f[2];};\n"
                "return 12;\n"
            ),
            .exit_code = 12,
            .target = CC_TARGET_X86_64_WINDOWS,
        },
        {
            "mac arm64", __LINE__,
            SVI("struct S1 {int x;};\n"
                "struct S2 {int x; float y;};\n"
                "struct S3 {int x, y;};\n"
                "struct S4 {int x, y, z;};\n"
                "struct S5 {double x, y;};\n"
                "struct S6 {double x; int y, z;};\n"
                "struct S7 {double x[3]; int y[4], z;};\n"
                "struct S8 {float x[2]; double d;};\n"
                "struct S9 {float x[2]; float f[2];};\n"
                "return 12;\n"
            ),
            .exit_code = 12,
            .target = CC_TARGET_AARCH64_MACOS,
        },
        {
            "linux arm64", __LINE__,
            SVI("struct S1 {int x;};\n"
                "struct S2 {int x; float y;};\n"
                "struct S3 {int x, y;};\n"
                "struct S4 {int x, y, z;};\n"
                "struct S5 {double x, y;};\n"
                "struct S6 {double x; int y, z;};\n"
                "struct S7 {double x[3]; int y[4], z;};\n"
                "struct S8 {float x[2]; double d;};\n"
                "struct S9 {float x[2]; float f[2];};\n"
                "return 12;\n"
            ),
            .exit_code = 12,
            .target = CC_TARGET_AARCH64_LINUX,
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
                    .target = cc_target_funcs[tc->target](),
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

        err = cpp_include_file_via_file_cache(&interp.parser.cpp, SV(__FILE__));
        if(err) {TestReport("failed to include"); goto finally;}
        ma_tail(interp.parser.cpp.frames).line = tc->line+1;

        err = cc_parse_all(&interp.parser);
        if(err){TestPrintf("%s:%d: failed to parse (error %d)\n%.*s", __FILE__, tc->line, err, (int)log_sb.cursor, log_sb.data); goto finally;}
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
TestFunction(test_ci_call_main){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    static struct tc {
        const char* name; int line;
        StringView program;
        int argc;
        char*_Null_unspecified argv[4];
        char*_Null_unspecified envp[4];
        int expected_ret;
        _Bool expect_err;
        StringView expected_msg;
        _Bool skip;
    } testcases[] = {
        {
            "no-arg main", __LINE__,
            SVI("int main(void){ return 42; }\n"),
            .argc = 0,
            .expected_ret = 42,
        },
        {
            "argc passthrough", __LINE__,
            SVI("int main(int argc, char** argv){ return argc; }\n"),
            .argc = 3,
            .argv = {"prog", "a", "b"},
            .expected_ret = 3,
        },
        {
            "argv read", __LINE__,
            SVI("int main(int argc, char** argv){ return argv[1][0]; }\n"),
            .argc = 2,
            .argv = {"prog", "Abc"},
            .expected_ret = 'A',
        },
        {
            "envp read", __LINE__,
            SVI("int main(int argc, char** argv, char** envp){\n"
                "    return envp[0][0];\n"
                "}\n"),
            .argc = 1,
            .argv = {"prog"},
            .envp = {"X=1"},
            .expected_ret = 'X',
        },
        {
            "argc + argv contents", __LINE__,
            SVI("int main(int argc, char** argv){\n"
                "    int sum = 0;\n"
                "    for(int i = 0; i < argc; i++) sum += argv[i][0];\n"
                "    return sum;\n"
                "}\n"),
            .argc = 3,
            .argv = {"A", "B", "C"},
            .expected_ret = 'A' + 'B' + 'C',
        },
        {
            "no main defined", __LINE__,
            SVI("int other(void){ return 0; }\n"),
            .expect_err = 1,
        },
        {
            "unsupported main signature", __LINE__,
            SVI("int main(int x){ return x; }\n"),
            .argc = 1,
            .argv = {"prog"},
            .expect_err = 1,
            .expected_msg = SVI("(test):1:5: error: main has unsupported signature (1 params)\n"),
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
        if(err){TestReport("failed to include"); goto finally;}

        err = cc_parse_all(&interp.parser);
        if(err){TestPrintf("%s:%d: failed to parse\n", __FILE__, tc->line); goto finally;}
        err = ci_resolve_refs(&interp);
        if(err){TestPrintf("%s:%d: failed to link\n", __FILE__, tc->line); goto finally;}

        char*_Null_unspecified argv_buf[5] = {0};
        for(int j = 0; j < tc->argc; j++) argv_buf[j] = tc->argv[j];
        char*_Null_unspecified envp_buf[5] = {0};
        for(int j = 0; j < (int)(sizeof tc->envp / sizeof tc->envp[0]); j++){
            if(!tc->envp[j]) break;
            envp_buf[j] = tc->envp[j];
        }
        int main_ret = 0;
        err = ci_call_main(&interp, tc->argc, argv_buf, envp_buf, &main_ret);
        if(tc->expect_err){
            if(!err){
                TEST_stats.failures++;
                TestPrintf("%s:%d: expected ci_call_main to fail but it succeeded\n", __FILE__, tc->line);
            }
            err = 0;
            StringView sv = log_sb.cursor && !log_sb.errored ? msb_borrow_sv(&log_sb) : SV("");
            test_expect_equals_sv(tc->expected_msg, sv, "expected error", "actual error", &TEST_stats, __FILE__, __func__, tc->line);
            goto cleanup;
        }
        if(err){TestPrintf("%s:%d: ci_call_main failed: err=%d\n", __FILE__, tc->line, err); goto finally;}
        if(main_ret != tc->expected_ret){
            TEST_stats.failures++;
            TestPrintf("%s:%d: expected (%d) != actual (%d)\n", __FILE__, tc->line, tc->expected_ret, main_ret);
        }

        finally:
        if(log_sb.cursor && !log_sb.errored){
            StringView sv = msb_borrow_sv(&log_sb);
            TestPrintf("%.*s\n", sv_p(sv));
        }
        cleanup:
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

TestFunction(test_ci_call_by_name){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    enum {MAX_ARGS = 4};
    static struct tc {
        const char* name; int line;
        StringView program;
        StringView func_name;
        struct {
            CcBasicTypeKind kind;
            union {
                int i;
                double d;
            };
        } args[MAX_ARGS];
        uint32_t nargs;
        uint32_t call_nargs; // overrides nargs when > 0, for mismatch tests
        int expected_ret;
        int expect_err;
        StringView expected_msg;
        _Bool skip;
    } testcases[] = {
        {
            "no args", __LINE__,
            SVI("int zero(void){ return 99; }\n"),
            .func_name = SVI("zero"),
            .expected_ret = 99,
        },
        {
            "add two ints", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"),
            .func_name = SVI("add"),
            .args = {{CCBT_int, .i=3}, {CCBT_int, .i=4}},
            .expected_ret = 7,
        },
        {
            "mixed argv layout with unnamed parameter", __LINE__,
            SVI("int mixed(int a,double,int c){return 10*a+c;}\n"),
            .func_name = SVI("mixed"),
            .args = {{CCBT_int, .i=3}, {CCBT_double, .d=9.5}, {CCBT_int, .i=4}},
            .expected_ret = 34,
        },
        {
            "subtract", __LINE__,
            SVI("int sub(int a, int b){ return a - b; }\n"),
            .func_name = SVI("sub"),
            .args = {{CCBT_int, .i=10}, {CCBT_int, .i=3}},
            .expected_ret = 7,
        },
        {
            "recursive fact", __LINE__,
            SVI("int fact(int n){\n"
                "    if(n <= 1) return 1;\n"
                "    return n * fact(n-1);\n"
                "}\n"),
            .func_name = SVI("fact"),
            .args = {{CCBT_int, .i=6}},
            .expected_ret = 720,
        },
        {
            "four int args", __LINE__,
            SVI("int sum4(int a, int b, int c, int d){ return a+b+c+d; }\n"),
            .func_name = SVI("sum4"),
            .args = {{CCBT_int, .i=1}, {CCBT_int, .i=2}, {CCBT_int, .i=3}, {CCBT_int, .i=4}},
            .expected_ret = 10,
        },
        {
            "missing function", __LINE__,
            SVI("int unrelated(void){ return 0; }\n"),
            .func_name = SVI("does_not_exist"),
            .expect_err = _cc_symbol_not_found_error,
        },
        {
            "too few args", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"),
            .func_name = SVI("add"),
            .args = {{CCBT_int, .i=3}},
            .expect_err = _cc_runtime_error,
            .expected_msg = SVI("(test):1:5: error: ci_call_by_name 'add': expected 2 args, got 1\n"),
        },
        {
            "too many args", __LINE__,
            SVI("int zero(void){ return 0; }\n"),
            .func_name = SVI("zero"),
            .args = {{CCBT_int, .i=1}, {CCBT_int, .i=2}},
            .expect_err = _cc_runtime_error,
            .expected_msg = SVI("(test):1:5: error: ci_call_by_name 'zero': expected 0 args, got 2\n"),
        },
        {
            "wrong arg type", __LINE__,
            SVI("int add(int a, int b){ return a + b; }\n"),
            .func_name = SVI("add"),
            .args = {{CCBT_int, .i=3}, {CCBT_double, .d=4.}},
            .expect_err = _cc_runtime_error,
            .expected_msg = SVI("(test):1:5: error: ci_call_by_name 'add': arg 1 type mismatch\n"),
        },
        {
            "doubles", __LINE__,
            SVI("int cmp(double a, double b){return a < b?-1: a > b? 1: 0;}\n"),
            .func_name = SVI("cmp"),
            .args = {{CCBT_double, .d=4}, {CCBT_double, .d=8}},
            .expected_ret = -1,
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
        if(err){TestReport("failed to include"); goto finally;}

        err = cc_parse_all(&interp.parser);
        if(err){TestPrintf("%s:%d: failed to parse\n", __FILE__, tc->line); goto finally;}
        err = ci_add_root(&interp, tc->func_name);
        if(err){
            if(tc->expect_err) goto expected_error;
            TestPrintf("%s:%d: ci_add_root failed: err=%d\n", __FILE__, tc->line, err);
            goto finally;
        }
        err = ci_resolve_refs(&interp);
        if(err){TestPrintf("%s:%d: failed to link\n", __FILE__, tc->line); goto finally;}

        CiArg ci_args[MAX_ARGS];
        int nargs = 0;
        for(uint32_t j = 0; j < MAX_ARGS; j++){
            if(tc->args[j].kind == CCBT_INVALID)
                break;
            ci_args[nargs++] = (CiArg){
                .data = &tc->args[j].d,
                .size = interp.parser.cpp.target.sizeof_[tc->args[j].kind],
                .type.basic.kind = tc->args[j].kind,
            };
        }
        int ret = 0;
        err = ci_call_by_name(&interp, tc->func_name, ci_args, nargs, &ret, sizeof ret);
        if(tc->expect_err){
            expected_error:;
            if(!err){
                TEST_stats.failures++;
                TestPrintf("%s:%d: expected ci_call_by_name to fail but it succeeded\n", __FILE__, tc->line);
            }
            if(err != tc->expect_err){
                TEST_stats.failures++;
                TestPrintf("%s:%d: expected ci_call_by_name to fail with '%s' but it failed with '%s'\n", __FILE__, tc->line, _cc_error_names[tc->expect_err], _cc_error_names[err]);
            }
            err = 0;
            StringView sv = log_sb.cursor && !log_sb.errored ? msb_borrow_sv(&log_sb) : SV("");
            test_expect_equals_sv(tc->expected_msg, sv, "expected error", "actual error", &TEST_stats, __FILE__, __func__, tc->line);
            goto cleanup;
        }
        if(err){TestPrintf("%s:%d: ci_call_by_name failed: err=%d\n", __FILE__, tc->line, err); goto finally;}
        if(ret != tc->expected_ret){
            TEST_stats.failures++;
            TestPrintf("%s:%d: expected (%d) != actual (%d)\n", __FILE__, tc->line, tc->expected_ret, ret);
        }

        finally:
        if(log_sb.cursor && !log_sb.errored){
            StringView sv = msb_borrow_sv(&log_sb);
            TestPrintf("%.*s\n", sv_p(sv));
        }
        cleanup:
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
TestFunction(test_float_folding);
TestFunction(test_lower_deps);
TestFunction(test_static_blobs);
TestFunction(test_symbolic_pointer_limits);


int main(int argc, char** argv){
    #ifdef USE_TESTING_ALLOCATOR
        testing_allocator_init();
    #endif
    RegisterTestFlags(test_interpreter, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_interpreter_runtime_errors, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_interpreter_builtin_headers, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_cross_target, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_ci_call_main, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_ci_call_by_name, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_float_folding, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_lower_deps, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_static_blobs, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_symbolic_pointer_limits, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
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

#include <fenv.h>
#include "ci_op_printer.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

TestFunction(test_symbolic_pointer_limits){
    TESTBEGIN();
    static const struct {
        const char* name;
        int64_t left, right, extra, expected;
        CcBasicTypeKind element;
        int status;
        _Bool needs_64_bits;
    } cases[] = {
        {"zero", 0, 0, 0, 0, CCBT_char},
        {"positive scaled difference", 7, 2, 0, 5, CCBT_int},
        {"negative scaled difference", 2, 7, 0, -5, CCBT_int},
        {"32-bit maximum", INT32_MAX, 0, 0, INT32_MAX, CCBT_char},
        {"32-bit minimum", INT32_MIN, 0, 0, INT32_MIN, CCBT_char},
        {"above 32-bit maximum", (int64_t)INT32_MAX+1, 0, 0, (int64_t)INT32_MAX+1, CCBT_char, 0, 1},
        {"below 32-bit minimum", (int64_t)INT32_MIN-1, 0, 0, (int64_t)INT32_MIN-1, CCBT_char, 0, 1},
        {"64-bit maximum", INT64_MAX, 0, 0, INT64_MAX, CCBT_char, 0, 1},
        {"64-bit minimum", INT64_MIN, 0, 0, INT64_MIN, CCBT_char, 0, 1},
        {"positive delta overflow", INT64_MAX, -1, 0, 0, CCBT_char, _cc_overflow_error},
        {"negative delta overflow", INT64_MIN, 1, 0, 0, CCBT_char, _cc_overflow_error},
        {"positive scale overflow", INT64_MAX, 0, 0, 0, CCBT_int, _cc_overflow_error},
        {"negative scale overflow", INT64_MIN, 0, 0, 0, CCBT_int, _cc_overflow_error},
        {"positive offset addition overflow", INT64_MAX, 0, 1, 0, CCBT_char, _cc_overflow_error},
        {"negative offset addition overflow", INT64_MIN, 0, -1, 0, CCBT_char, _cc_overflow_error},
    };
    static int indexes[CC_TARGET_COUNT+1] = {0};
    for(size_t target = 0; target <= CC_TARGET_COUNT; target++){
        {
            ArenaAllocator arena = {0};
            CcParser p = {.cpp = {.target = cc_target_funcs[target == CC_TARGET_COUNT ? CC_TARGET_TEST : target](), .allocator = allocator_from_arena(&arena)}};
            if(target == CC_TARGET_COUNT) p.cpp.target.sizeof_[CCBT_nullptr_t] = 4;
            CcPointer pointer = {.kind = CC_POINTER, .pointee = ccqt_basic(CCBT_char)};
            CcQualType ptr = {.bits = (uintptr_t)&pointer};
            // Keep casts in the AST: parsing literals can fold them before
            // the symbolic evaluator sees the integer-to-pointer conversion.
            uint64_t bits[] = {UINT64_MAX, UINT64_C(1)<<63, UINT64_C(1)<<32};
            for(size_t i = 0; i < arrlen(bits); i++){
                CcExpr number = {.kind = CC_EXPR_VALUE, .type = ccqt_basic(CCBT_unsigned_long_long), .uinteger = bits[i]};
                CcExpr cast = {.kind = CC_EXPR_CAST, .type = ptr, .lhs = &number};
                CcExpr expected = {.kind = CC_EXPR_VALUE, .type = ptr, .uinteger = bits[i]};
                if(p.cpp.target.sizeof_[CCBT_nullptr_t] == 4) expected.uinteger &= UINT32_MAX;
                CcExpr* equal = cc_binary_expr(&p, CC_EXPR_EQ, (SrcLoc){0}, ccqt_basic(CCBT_int), &cast, &expected);
                TestExpectTrue(_Bool, equal != NULL);
                if(equal){
                    int64_t value = 0;
                    TestExpect(int, cc_eval_symbolic_binary(&p, equal, 0, &value), ==, 0);
                    TestExpect(int64_t, value, ==, 1);
                }
            }
            ArenaAllocator_free_all(&arena);
            ArenaAllocator_free_all(&p.scratch_arena);
        }
        for(size_t i = test_atomic_increment(indexes+target); i < arrlen(cases); i = test_atomic_increment(indexes+target)){
            ArenaAllocator arena = {0};
            Allocator al = allocator_from_arena(&arena);
            CcParser p = {.cpp = {.target = cc_target_funcs[target == CC_TARGET_COUNT ? CC_TARGET_TEST : target](), .allocator = al}};
            // Exercise the target-dependent range check even though the shipped
            // configurations currently all use a 64-bit ptrdiff_t.
            if(target == CC_TARGET_COUNT) p.cpp.target.ptrdiff_type = CCBT_int;
            CcQualType element = ccqt_basic(cases[i].element);
            CcPointer pointer = {.kind = CC_POINTER, .pointee = element};
            CcQualType ptr = {.bits = (uintptr_t)&pointer};
            CcQualType diff = ccqt_basic(p.cpp.target.ptrdiff_type);
            CcVariable var = {.type = element};
            CcExpr ref = {.kind = CC_EXPR_VARIABLE, .type = element, .var = &var};
            CcExpr address = {.kind = CC_EXPR_ADDR, .type = ptr, .lhs = &ref};
            CcExpr left_value = {.kind = CC_EXPR_VALUE, .type = ccqt_basic(CCBT_long_long), .integer = cases[i].left};
            CcExpr right_value = {.kind = CC_EXPR_VALUE, .type = left_value.type, .integer = cases[i].right};
            CcExpr extra = {.kind = CC_EXPR_VALUE, .type = left_value.type, .integer = cases[i].extra};
            CcExpr* left = cc_binary_expr(&p, CC_EXPR_ADD, (SrcLoc){0}, ptr, &address, &left_value);
            CcExpr* right = cc_binary_expr(&p, CC_EXPR_ADD, (SrcLoc){0}, ptr, &address, &right_value);
            TestExpectTrue(_Bool, left && right);
            if(left && right){
                if(cases[i].extra) left = cc_binary_expr(&p, CC_EXPR_ADD, (SrcLoc){0}, ptr, left, &extra);
                CcExpr* expression = left ? cc_binary_expr(&p, CC_EXPR_SUB, (SrcLoc){0}, diff, left, right) : NULL;
                TestExpectTrue(_Bool, expression != NULL);
                if(expression){
                    int64_t value = 0;
                    int expected_status = cases[i].status;
                    if(cases[i].needs_64_bits && p.cpp.target.sizeof_[p.cpp.target.ptrdiff_type] < 8)
                        expected_status = _cc_overflow_error;
                    int status = cc_eval_symbolic_binary(&p, expression, 0, &value);
                    if(status != expected_status || (!status && value != cases[i].expected))
                        TestPrintf("symbolic pointer limit: %s, target %zu\n", cases[i].name, target);
                    TestExpect(int, status, ==, expected_status);
                    if(!status) TestExpect(int64_t, value, ==, cases[i].expected);
                }
            }
            ArenaAllocator_free_all(&arena);
            ArenaAllocator_free_all(&p.scratch_arena);
        }
        {
            ArenaAllocator arena = {0};
            CcParser p = {.cpp = {.target = cc_target_funcs[target == CC_TARGET_COUNT ? CC_TARGET_TEST : target](), .allocator = allocator_from_arena(&arena)}};
            CcQualType integer = ccqt_basic(CCBT_int);
            CcPointer pointer = {.kind = CC_POINTER, .pointee = integer};
            CcQualType ptr = {.bits = (uintptr_t)&pointer};
            CcVariable object = {.type = integer};
            CcExpr ref = {.kind = CC_EXPR_VARIABLE, .type = integer, .var = &object};
            CcExpr address = {.kind = CC_EXPR_ADDR, .type = ptr, .lhs = &ref};
            CcVariable alias = {.type = ptr, .constexpr_ = 1};
            CcExpr alias_ref = {.kind = CC_EXPR_VARIABLE, .type = ptr, .var = &alias};
            alias.initializer = &alias_ref;
            CcExpr* expression = cc_binary_expr(&p, CC_EXPR_SUB, (SrcLoc){0}, ccqt_basic(p.cpp.target.ptrdiff_type), &alias_ref, &address);
            TestExpectTrue(_Bool, expression != NULL);
            if(expression){
                int64_t value;
                // Cyclic aliases stop at the depth limit instead of recursing
                // indefinitely or resolving an actual address.
                TestExpect(int, cc_eval_symbolic_binary(&p, expression, 0, &value), ==, _cc_not_constant_error);
                TestExpect(int, cc_eval_symbolic_binary(&p, expression, 1, &value), ==, _cc_not_constant_error);
                alias.initializer = &address;
                alias.constexpr_ = 0;
                alias_ref.type.is_const = 1;
                TestExpect(int, cc_eval_symbolic_binary(&p, expression, 0, &value), ==, _cc_not_constant_error);
                TestExpect(int, cc_eval_symbolic_binary(&p, expression, 1, &value), ==, 0);
                TestExpect(int64_t, value, ==, 0);
                alias_ref.type.is_volatile = 1;
                TestExpect(int, cc_eval_symbolic_binary(&p, expression, 1, &value), ==, _cc_not_constant_error);
                alias_ref.type.is_volatile = 0;
                alias_ref.type.is_atomic = 1;
                TestExpect(int, cc_eval_symbolic_binary(&p, expression, 1, &value), ==, _cc_not_constant_error);
                alias_ref.type.is_atomic = 0;
                object.automatic = 1;
                TestExpect(int, cc_eval_symbolic_binary(&p, expression, 1, &value), ==, _cc_not_constant_error);
                object.automatic = 0;
                CcExpr unknown = {.kind = CC_EXPR_CALL, .type = ptr};
                alias.initializer = &unknown;
                TestExpect(int, cc_eval_symbolic_binary(&p, expression, 1, &value), ==, _cc_not_constant_error);
                CcExpr* casts[257];
                _Bool built = 1;
                for(size_t i = 0; i < arrlen(casts); i++){
                    casts[i] = cc_make_expr(&p, CC_EXPR_CAST, (SrcLoc){0}, ptr, 0);
                    if(!casts[i]){ built = 0; break; }
                    casts[i]->lhs = i ? casts[i-1] : &address;
                }
                TestExpectTrue(_Bool, built);
                if(built){
                    alias.initializer = casts[256];
                    TestExpect(int, cc_eval_symbolic_binary(&p, expression, 1, &value), ==, _cc_not_constant_error);
                    alias.initializer = casts[250];
                    TestExpect(int, cc_eval_symbolic_binary(&p, expression, 1, &value), ==, 0);
                    TestExpect(int64_t, value, ==, 0);
                }
            }
            ArenaAllocator_free_all(&arena);
            ArenaAllocator_free_all(&p.scratch_arena);
        }
    }
    TESTEND();
}

TestFunction(test_static_blobs){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    CiInterpreter ci = {.parser.cpp = {.target = cc_target_funcs[CC_TARGET_TEST](), .allocator = al}};
    CiLowerDeps deps = {0};
    CcQualType type = ccqt_basic(CCBT_int);
    CcExpr value = {.kind = CC_EXPR_VALUE, .type = type, .integer = 7};
    CcVariable a = {.type = type, .initializer = &value};
    CcVariable b = {.type = type, .initializer = &value};
    int target_a = 1, target_b = 2;
    CcVariable ta = {.type = type, .interp_val = &target_a};
    CcVariable tb = {.type = type, .interp_val = &target_b};
    CcPointer pointer = {.kind = CC_POINTER, .pointee = type};
    CcQualType ptr_type = {.bits = (uintptr_t)&pointer};
    CcExpr ref_a = {.kind = CC_EXPR_VARIABLE, .type = type, .var = &ta};
    CcExpr ref_b = {.kind = CC_EXPR_VARIABLE, .type = type, .var = &tb};
    CcExpr addr_a = {.kind = CC_EXPR_ADDR, .type = ptr_type, .lhs = &ref_a};
    CcExpr addr_b = {.kind = CC_EXPR_ADDR, .type = ptr_type, .lhs = &ref_b};
    int* result_a = NULL;
    int* result_b = NULL;
    CcVariable pa = {.type = ptr_type, .initializer = &addr_a, .interp_val = &result_a};
    CcVariable pb = {.type = ptr_type, .initializer = &addr_b, .interp_val = &result_b};
    CiStaticData data[] = {{.var = &a}, {.var = &b}, {.var = &pa}, {.var = &pb}};
    for(size_t i = 0; i < sizeof data / sizeof data[0]; i++){
        int err = ci_build_static_data(&ci, &data[i], &deps);
        TestExpect(int, err, ==, 0);
        if(err) goto cleanup;
    }
    TestExpectTrue(_Bool, data[0].blob == data[1].blob);
    TestExpect(uint32_t, data[0].size, ==, sizeof(int));
    int stored;
    memcpy(&stored, data[0].blob->data, sizeof stored);
    TestExpect(int, stored, ==, 7);
    // Identical unrelocated bytes can share a blob even with different symbols.
    TestExpectTrue(_Bool, data[2].blob == data[3].blob);
    ci_relocate_static_data(&data[2]);
    ci_relocate_static_data(&data[3]);
    TestExpectTrue(_Bool, result_a == &target_a);
    TestExpectTrue(_Bool, result_b == &target_b);
    TestExpect(uint64_t, data[2].blob->data[0], ==, 0);
    cleanup:
    for(size_t i = 0; i < sizeof data / sizeof data[0]; i++)
        ci_static_data_cleanup(&data[i], al);
    ci_lower_deps_cleanup(&deps, al);
    ArenaAllocator_free_all(&ci.bt.arena);
    ArenaAllocator_free_all(&arena);
    TESTEND();
}

TestFunction(test_lower_deps){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    CiInterpreter ci = {.parser.cpp = {.target = cc_target_funcs[CC_TARGET_TEST](), .allocator = al}};
    Marray(CiOp) ops = {0};
    uint32_t frame_size = 0;
    CiLowerCtx ctx = {.a = al, .out = &ops, .frame_size = &frame_size, .ptr_size = 8, .size_size = 8};
    CcVariable global = {.type = ccqt_basic(CCBT_int)};
    CcVariable local = {.type = ccqt_basic(CCBT_int), .automatic = 1};
    CcExpr expr = {.kind = CC_EXPR_VARIABLE, .type = global.type, .var = &global, .is_lvalue = 1};
    CiLowerVal value;
    CiLowerAddr addr;
    int err = ci_lower_expr_discard(&ci, &ctx, &expr);
    TestExpect(int, err, ==, 0);
    TestExpect(size_t, ctx.deps.vars.count, ==, 0);
    err = ci_lower_expr(&ci, &ctx, &expr, CI_NO_SLOT, &value);
    TestExpect(int, err, ==, 0);
    TestExpect(size_t, ctx.deps.vars.count, ==, 1);
    TestExpect(uintptr_t, (uintptr_t)PM_get(&ctx.deps.vars, &global), ==, 1);
    expr.var = &local;
    err = ci_lower_expr(&ci, &ctx, &expr, CI_NO_SLOT, &value);
    TestExpect(int, err, ==, 0);
    TestExpect(size_t, ctx.deps.vars.count, ==, 1);
    CcVariable addressed = {.type = global.type};
    expr.var = &addressed;
    err = ci_lower_addr(&ci, &ctx, &expr, 0, &addr);
    TestExpect(int, err, ==, 0);
    TestExpect(size_t, ctx.deps.vars.count, ==, 2);
    err = ci_lower_addr(&ci, &ctx, &expr, 0, &addr);
    TestExpect(int, err, ==, 0);
    TestExpect(size_t, ctx.deps.vars.count, ==, 2);
    CcFunction type = {.kind = CC_FUNCTION, .return_type = ccqt_basic(CCBT_void)};
    CcFunc func = {.type = &type};
    CcExpr callee = {.kind = CC_EXPR_FUNCTION, .type = {.bits = (uintptr_t)&type}, .func = &func};
    CcExpr call = {.kind = CC_EXPR_CALL, .type = type.return_type, .lhs = &callee};
    err = ci_lower_call(&ci, &ctx, &call, CI_NO_SLOT, NULL);
    TestExpect(int, err, ==, 0);
    TestExpect(uintptr_t, (uintptr_t)PM_get(&ctx.deps.funcs, &func), ==, CC_FUNC_DEP_USED);
    err = ci_lower_cast_operand(&ci, &ctx, &callee, CI_NO_SLOT, &value);
    TestExpect(int, err, ==, 0);
    err = ci_lower_call(&ci, &ctx, &call, CI_NO_SLOT, NULL);
    TestExpect(int, err, ==, 0);
    TestExpect(size_t, ctx.deps.funcs.count, ==, 1);
    TestExpect(uintptr_t, (uintptr_t)PM_get(&ctx.deps.funcs, &func), ==, CC_FUNC_DEP_USED | CC_FUNC_DEP_ADDR_TAKEN);
    // Lowering records dependencies locally, without modifying interpreter sets.
    TestExpect(size_t, ci.deps.funcs.count, ==, 0);
    TestExpect(size_t, ci.deps.vars.count, ==, 0);
    ci_lower_deps_cleanup(&ctx.deps, al);
    TestExpect(size_t, ctx.deps.vars.count, ==, 0);
    TestExpect(size_t, ctx.deps.funcs.count, ==, 0);
    ci_lower_deps_cleanup(&ctx.deps, al);
    // A failed lazy parse must remain an error when preparation is retried.
    CcFunc failed = {.type = &type, .defined = 1, .parse_failed = 1};
    err = ci_deps_add_func(&ci.deps, al, &failed, CC_FUNC_DEP_USED);
    TestExpect(int, err, ==, 0);
    for(int i = 0; i < 2; i++){
        err = ci_resolve_deps(&ci, &ci.deps);
        TestExpect(int, err, ==, CI_SYNTAX_ERROR);
        TestExpectTrue(_Bool, failed.interp_ops == NULL);
    }
    ci_lower_deps_cleanup(&ci.deps, al);
    ArenaAllocator_free_all(&arena);
    {
        TestingAllocator ta = {0};
        LOCK_T_init(&ta.lock);
        Allocator retry_al = {.type = ALLOCATOR_TESTING, ._data = &ta};
        CiInterpreter retry_ci = {.parser.cpp = {.target = cc_target_funcs[CC_TARGET_TEST](), .allocator = retry_al}};
        CcVariable retry_global = {.type = ccqt_basic(CCBT_int)};
        CcExpr retry_expr = {.kind = CC_EXPR_VARIABLE, .type = retry_global.type, .var = &retry_global};
        CcStmtNode retry_stmt = {.kind = CC_STMT_RETURN, .expr = &retry_expr};
        CcStmtNode* retry_nodes_data[] = {&retry_stmt};
        Parray(CcStmtNode) retry_nodes = {.data = (void**)retry_nodes_data, .count = 1};
        Marray(CiOp) retry_ops = {0};
        AtomMap(uintptr_t) retry_labels = {0};
        uint32_t retry_size = 0;
        size_t retry_lowered = 0;
        CiLowerDeps retry_deps = {0};
        // Fail the dependency merge after the local dependency and ops allocations.
        ta.fail_at = 3;
        int retry_err = ci_lower_nodes(&retry_ci, &retry_nodes, &retry_lowered, &retry_ops, &retry_labels, &retry_size, &retry_deps);
        TestExpect(int, retry_err, ==, CI_OOM_ERROR);
        TestExpect(size_t, retry_lowered, ==, 0);
        TestExpect(size_t, retry_ops.count, ==, 0);
        TestExpect(uint32_t, retry_size, ==, 0);
        ta.fail_at = 0;
        retry_err = ci_lower_nodes(&retry_ci, &retry_nodes, &retry_lowered, &retry_ops, &retry_labels, &retry_size, &retry_deps);
        TestExpect(int, retry_err, ==, 0);
        TestExpect(size_t, retry_deps.vars.count, ==, 1);
        TestExpect(size_t, retry_ops.count, ==, 3);
        recording_free_all(&ta.recorder);
        recording_cleanup(&ta.recorder);
    }
    {
        TestingAllocator ta = {0};
        LOCK_T_init(&ta.lock);
        Allocator retry_al = {.type = ALLOCATOR_TESTING, ._data = &ta};
        CiInterpreter retry_ci = {.parser.cpp = {.target = cc_target_funcs[CC_TARGET_TEST](), .allocator = retry_al}};
        CiFuncOps prepared = {0};
        CcFunc retry_func = {.type = &type, .defined = 1, .parsed = 1, .interp_ops = &prepared};
        int retry_err = ci_deps_add_func(&retry_ci.deps, retry_al, &retry_func, CC_FUNC_DEP_USED | CC_FUNC_DEP_ADDR_TAKEN);
        TestExpect(int, retry_err, ==, 0);
        ta.nallocs = 0;
        ta.fail_at = 1;
        retry_err = ci_resolve_deps(&retry_ci, &retry_ci.deps);
        TestExpect(int, retry_err, ==, CI_OOM_ERROR);
        TestExpectTrue(_Bool, retry_func.native_func == NULL);
        ta.fail_at = 0;
        retry_err = ci_resolve_deps(&retry_ci, &retry_ci.deps);
        TestExpect(int, retry_err, ==, 0);
        TestExpectTrue(_Bool, retry_func.native_func != NULL);
        TestExpectTrue(_Bool, BPM_get(&retry_ci.closure_map, &retry_func) == (void*)retry_func.native_func);
        recording_free_all(&ta.recorder);
        recording_cleanup(&ta.recorder);
    }
    TESTEND();
}

TestFunction(test_float_folding){
    TESTBEGIN();
    #if defined __DRC__ && defined __GLIBC__
    // fenv functions have inline gnu asm with glibc
    #else
    CiInterpreter ci = {.parser.cpp.target = cc_target_funcs[CC_TARGET_TEST]()};
    CiLowerCtx ctx = {.char_is_unsigned = !ci_target(&ci)->char_is_signed, .ldbl_fmt=ci_target(&ci)->long_double_format};
    static const struct {
        CcBasicTypeKind from, to;
        uint64_t input[2], expected[2];
        int status;
    } cases[] = {
        {CCBT_int, CCBT_double, {800}, {0x4089000000000000}},
        {CCBT_int, CCBT_float, {2}, {0x40000000}},
        {CCBT_int, CCBT_float, {0}, {0}},
        {CCBT_int, CCBT_double, {0xffffffff}, {0xbff0000000000000}},
        {CCBT_unsigned, CCBT_float, {16777216}, {0x4b800000}},
        {CCBT_unsigned, CCBT_float, {16777217}, {0}, -1},
        {CCBT_unsigned_long_long, CCBT_double, {9007199254740992ULL}, {0x4340000000000000}},
        {CCBT_unsigned_long_long, CCBT_double, {9007199254740993ULL}, {0}, -1},
        {CCBT_long_long, CCBT_double, {0x8000000000000000}, {0xc3e0000000000000}},
        {CCBT_unsigned_int128, CCBT_double, {0, 0x1000000000}, {0x4630000000000000}}, // 2^100
        {CCBT_unsigned_int128, CCBT_float, {0, 0x1000000000}, {0x71800000}},
        {CCBT_unsigned_int128, CCBT_double, {1, 0x1000000000}, {0}, -1},
        {CCBT_int128, CCBT_double, {0, 0x8000000000000000}, {0xc7e0000000000000}},
        {CCBT_unsigned_int128, CCBT_float, {UINT64_MAX, UINT64_MAX}, {0}, -1},
        {CCBT_float, CCBT_double, {0x3fc00000}, {0x3ff8000000000000}}, // 1.5
        {CCBT_double, CCBT_float, {0x3ff8000000000000}, {0x3fc00000}},
        {CCBT_double, CCBT_float, {0x3ff0000000000001}, {0}, -1},
        {CCBT_double, CCBT_float, {0x47f0000000000000}, {0}, -1}, // 2^128
        {CCBT_double, CCBT_float, {0x3810000000000000}, {0x00800000}}, // min normal
        {CCBT_double, CCBT_float, {0x3800000000000000}, {0}, -1}, // subnormal result
        {CCBT_float, CCBT_double, {1}, {0}, -1}, // subnormal input
        {CCBT_double, CCBT_float, {1}, {0}, -1},
        {CCBT_double, CCBT_float, {0x8000000000000000}, {0x80000000}},
        {CCBT_float, CCBT_double, {0x80000000}, {0x8000000000000000}},
        {CCBT_double, CCBT_float, {0x7ff0000000000000}, {0x7f800000}},
        {CCBT_float, CCBT_double, {0xff800000}, {0xfff0000000000000}},
        {CCBT_double, CCBT_float, {0x7ff0000000000001}, {0}, -1}, // sNaN
        {CCBT_float, CCBT_double, {0x7f800001}, {0}, -1},
        {CCBT_double, CCBT_float, {0x7ff8000000000000}, {0}, -1}, // qNaN
        {CCBT_float, CCBT_float, {0x7f800001}, {0x7f800001}}, // identity is a copy
        {CCBT_double, CCBT_int, {0x4045000000000000}, {42}},
        {CCBT_float, CCBT_int, {0xc2280000}, {0xffffffd6}}, // -42
        {CCBT_double, CCBT_int, {0x3ff8000000000000}, {0}, -1}, // fractional
        {CCBT_double, CCBT_int, {0x8000000000000000}, {0}},
        {CCBT_double, CCBT_unsigned, {0xbff0000000000000}, {0}, -1},
        {CCBT_double, CCBT_signed_char, {0x4060000000000000}, {0}, -1}, // 128
        {CCBT_double, CCBT_signed_char, {0xc060000000000000}, {0x80}},
        {CCBT_double, CCBT_long_long, {0x43e0000000000000}, {0}, -1}, // 2^63
        {CCBT_double, CCBT_long_long, {0xc3e0000000000000}, {0x8000000000000000}},
        {CCBT_double, CCBT_unsigned_long_long, {0x43e0000000000000}, {0x8000000000000000}},
        {CCBT_double, CCBT_unsigned_long_long, {0x43f0000000000000}, {0}, -1}, // 2^64
        {CCBT_double, CCBT_int128, {0x4630000000000000}, {0, 0x1000000000}},
        {CCBT_double, CCBT_int128, {0xc7e0000000000000}, {0, 0x8000000000000000}},
        {CCBT_double, CCBT_int128, {0x47e0000000000000}, {0}, -1},
        {CCBT_double, CCBT_unsigned_int128, {0x47e0000000000000}, {0, 0x8000000000000000}},
        {CCBT_double, CCBT_unsigned_int128, {0x47f0000000000000}, {0}, -1},
        {CCBT_double, CCBT_int, {0x7ff0000000000000}, {0}, -1},
        {CCBT_double, CCBT_int, {0x7ff0000000000001}, {0}, -1},
        {CCBT_double, CCBT_bool, {0x8000000000000000}, {0}},
        {CCBT_double, CCBT_bool, {0x3ff8000000000000}, {1}},
        {CCBT_double, CCBT_bool, {0x7ff0000000000001}, {0}, -1},
        {CCBT_float, CCBT_bool, {0x7f800001}, {0}, -1},
    };
    static const struct {
        CcExprKind kind;
        CcBasicTypeKind type;
        uint64_t a, b, expected;
        int status;
        _Bool comparison;
    } binary[] = {
        {CC_EXPR_DIV, CCBT_double, 0x4089000000000000, 0x4000000000000000, 0x4079000000000000},
        {CC_EXPR_DIV, CCBT_double, 0x4018000000000000, 0x4008000000000000, 0x4000000000000000},
        {CC_EXPR_DIV, CCBT_double, 0x3ff0000000000000, 0x4008000000000000, 0, -1},
        {CC_EXPR_DIV, CCBT_double, 0x3ff0000000000000, 0, 0, -1},
        {CC_EXPR_DIV, CCBT_double, 0, 0, 0, -1},
        {CC_EXPR_DIV, CCBT_double, 0x8000000000000000, 0x4000000000000000, 0x8000000000000000},
        {CC_EXPR_MUL, CCBT_double, 0x8000000000000000, 0xc000000000000000, 0},
        {CC_EXPR_MUL, CCBT_double, 0x7fefffffffffffff, 0x4000000000000000, 0, -1},
        {CC_EXPR_MUL, CCBT_double, 0x0010000000000000, 0x3fe0000000000000, 0, -1},
        {CC_EXPR_ADD, CCBT_double, 0x3ff0000000000000, 0x4000000000000000, 0x4008000000000000},
        {CC_EXPR_ADD, CCBT_double, 0x3ff0000000000000, 0x39b0000000000000, 0, -1},
        {CC_EXPR_ADD, CCBT_double, 0, 0x8000000000000000, 0, -1},
        {CC_EXPR_ADD, CCBT_double, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000},
        {CC_EXPR_SUB, CCBT_double, 0x4008000000000000, 0x4000000000000000, 0x3ff0000000000000},
        {CC_EXPR_SUB, CCBT_double, 0x4000000000000000, 0x4008000000000000, 0xbff0000000000000},
        {CC_EXPR_SUB, CCBT_double, 0x3ff0000000000000, 0x3ff0000000000000, 0, -1},
        {CC_EXPR_MUL, CCBT_float, 0x3fc00000, 0x40000000, 0x40400000},
        {CC_EXPR_DIV, CCBT_float, 0x40400000, 0x40000000, 0x3fc00000},
        {CC_EXPR_ADD, CCBT_float, 0x3f800000, 0x33800000, 0, -1},
        {CC_EXPR_EQ, CCBT_double, 0, 0x8000000000000000, 1, 0, 1},
        {CC_EXPR_LT, CCBT_double, 0xc008000000000000, 0xc000000000000000, 1, 0, 1},
        {CC_EXPR_GT, CCBT_double, 0x7ff0000000000000, 0x7fefffffffffffff, 1, 0, 1},
        {CC_EXPR_LE, CCBT_double, 0xfff0000000000000, 0xfff0000000000000, 1, 0, 1},
        {CC_EXPR_NE, CCBT_double, 0x7ff8000000000000, 0, 0, -1, 1},
        {CC_EXPR_NE, CCBT_double, 0x7ff0000000000001, 0, 0, -1, 1},
        {CC_EXPR_GT, CCBT_double, 1, 0, 0, -1, 1},
        {CC_EXPR_EQ, CCBT_float, 0x7f800001, 0, 0, -1, 1},
    };
    fenv_t saved;
    int hold = feholdexcept(&saved);
    if(hold != 0)
        EndTest("feholdexcept != 0");
    if(hold) return TEST_stats;
    int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    for(size_t m = 0; m < sizeof modes / sizeof modes[0]; m++){
        if(fesetround(modes[m]) != 0)
            EndTest("fesetround failed");
        {
            static int idx[sizeof modes/sizeof modes[0]] = {0};
            for(size_t i = test_atomic_increment(idx+m); i < sizeof binary/sizeof binary[0]; i = test_atomic_increment(idx+m)){
                CcQualType type = ccqt_basic(binary[i].type);
                uint32_t size = ci_target(&ci)->sizeof_[binary[i].type];
                CiFoldValue a = {.type = type, .sz = size, .bits = {binary[i].a}};
                CiFoldValue b = {.type = type, .sz = size, .bits = {binary[i].b}};
                CiFoldValue result = {.type = binary[i].comparison ? ccqt_basic(CCBT_int) : type, .sz = binary[i].comparison ? 4 : size};
                feclearexcept(FE_ALL_EXCEPT);
                if(i & 1) feraiseexcept(FE_DIVBYZERO);
                int flags = fetestexcept(FE_ALL_EXCEPT);
                int status = ci_fold_float_binary(&ctx, binary[i].kind, &a, &b, &result);
                int after_flags = fetestexcept(FE_ALL_EXCEPT);
                int after_round = fegetround();
                if(status != binary[i].status || (status == 0 && result.bits[0] != binary[i].expected))
                    TestPrintf("float binary fold case %zu, rounding mode %zu\n", i, m);
                TestExpect(int, status, ==, binary[i].status);
                TestExpect(int, after_flags, ==, flags);
                TestExpect(int, after_round, ==, modes[m]);
                if(status == 0) TestExpect(uint64_t, result.bits[0], ==, binary[i].expected);
            }
        }
        {
            static int idx[sizeof modes/sizeof modes[0]] = {0};
            for(size_t i = test_atomic_increment(idx+m); i < sizeof cases/sizeof cases[0]; i = test_atomic_increment(idx+m)){
                CiFoldValue from = {.type = ccqt_basic(cases[i].from), .sz = ci_target(&ci)->sizeof_[cases[i].from]};
                memcpy(from.bits, cases[i].input, sizeof from.bits);
                CiFoldValue to = {.type = ccqt_basic(cases[i].to), .sz = ci_target(&ci)->sizeof_[cases[i].to]};
                feclearexcept(FE_ALL_EXCEPT);
                if(i & 1) feraiseexcept(FE_DIVBYZERO);
                int flags = fetestexcept(FE_ALL_EXCEPT);
                int status;
                if(from.sz <= 8){
                    CcExpr value = {.kind = CC_EXPR_VALUE, .type = from.type, .uinteger = from.bits[0]};
                    CcExpr cast = {.kind = CC_EXPR_CAST, .type = to.type, .lhs = &value};
                    status = ci_fold_expr(&ci, &ctx, &cast, &to);
                }
                else status = ci_fold_float_cast(&ctx, &from, &to);
                int after_flags = fetestexcept(FE_ALL_EXCEPT);
                int after_round = fegetround();
                if(status != cases[i].status) TestPrintf("float fold case %zu, rounding mode %zu\n", i, m);
                TestExpect(int, status, ==, cases[i].status);
                TestExpect(int, after_flags, ==, flags);
                TestExpect(int, after_round, ==, modes[m]);
                if(status == 0){
                    TestExpect(uint64_t, to.bits[0], ==, cases[i].expected[0]);
                    TestExpect(uint64_t, to.bits[1], ==, cases[i].expected[1]);
                }
            }
        }
    }
    if(fesetenv(&saved) != 0)
        EndTest("fesetenv failed");
    #endif
    TESTEND();
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#ifdef __DRC__
#include "../Vendored/softfloat/softfloat_unity.c"
#endif
