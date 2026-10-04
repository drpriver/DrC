//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#define USE_TESTING_ALLOCATOR
#define NO_NATIVE_CALL
#define CI_THREAD_UNSAFE_ALLOCATOR
#define HEAVY_RECORDING
#define ARENA_EXPLICIT_ALLOCATOR
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
static _Bool check_leaks = 0;
enum {NOT_STARTED, IN_PROGRESS, DONE};
static struct OomTestCase {
    int line;
    StringView program;
    int64_t max_allocs;
    int64_t setup_allocs;
    int baseline_done;
    int fail_idx; // atomic
} test_programs[] = {
    {__LINE__, SVI("struct S {_SrcLoc loc;};int expected;\n"
         "int f(int args...,struct S s={__builtin_SRCLOC()}){return args.count==2&&s.loc.line==expected&&s.loc.file.count>0;}\n"
         "int probe(void){expected=__LINE__;return f(1,2);}return probe();\n")},
    {__LINE__, SVI("int n=0; int* inner(int* p=(int[]){++n}){return p;}\n"
         "int* outer(int* p=inner()){return p;}\n"
         "int probe(void){int* p=outer(); int* q=outer();\n"
         "return p!=q&&*p==1&&*q==2&&n==2;} return probe();\n")},
    {__LINE__, SVI("int n=0; int* inner(int values...,int extra=0){values[0]+=extra;return values.data;}\n"
         "int* outer(int* p=inner(++n,.extra=10)){return p;}\n"
         "int probe(void){int* p=outer(); int* q=outer();\n"
         "return p!=q&&*p==11&&*q==12&&n==2;} return probe();\n")},
    {__LINE__, SVI("int n=0; int* make(int* p=(int[]){++n}){return p;}\n"
         "int probe(void){int* p=make(); int* q=make();\n"
         "return p!=q&&*p==1&&*q==2&&n==2;} return probe();\n")},
    {__LINE__, SVI("int n=0; struct S {int a,b;};\n"
         "int f(struct S s={++n,2}, unsigned line=__builtin_LINE());\n"
         "int f(struct S s,unsigned line){return s.a*10+s.b+(line>0);}\n"
         "int g(int a=7,int rest...){return a+(int)rest.count;}\n"
         "return f()==13&&f()==23&&g()==7;\n")},
    {__LINE__, SVI("int x=0;\n"
         "constexpr int a=_Generic(x++, default: x++, int: 7);\n"
         "int b=_Generic(1, default: _Generic(1, default: ({x++; 7;})), float: 8)+2;\n"
         "return a==7&&b==9&&x==1;\n")},
    {__LINE__, SVI("_Alignas(64) _Thread_local int x = 17;\n"
         "int f(void){static _Thread_local int y = 4; return ++y;}\n"
         "int* p = &x; *p += f(); return x == 22 && f() == 6;\n")},
    {__LINE__, SVI("constexpr int a=__builtin_clzg((unsigned char)0,-7);\n"
         "static unsigned __int128 b=__builtin_stdc_rotate_left((unsigned __int128)1,127);\n"
         "int f(unsigned __int128 x,int n){return __builtin_clzg(x,n)+__builtin_parityg(x);}\n"
         "int i=0,j=0; __builtin_stdc_rotate_right((i++,(unsigned char)1),j++);\n"
         "return a==-7&&f(b,9)==1&&i==1&&j==1;\n")},
    {__LINE__, SVI("constexpr int a=__builtin_popcount(-1); constexpr int b=__builtin_clzll(1);\n"
         "enum E:unsigned __int128 {BITS=((unsigned __int128)1<<100)|7};\n"
         "int f(enum E x){return __builtin_popcountll(x);}\n"
         "return a==sizeof(unsigned)*8&&b==sizeof(unsigned long long)*8-1&&f(BITS)==3;\n")},
    {__LINE__, SVI("int (*f(void))[3]; int (*f(void))[]{static int a[3]={3,5,7};return &a;}\n"
         "int (*f(void))[]; _Static_assert(sizeof(*f())==3*sizeof(int));\n"
         "int g(int); int g(const int x){return x+1;} return (*f())[2]==g(6);\n")},
    {__LINE__, SVI("int f(int (*cb)(const int)); int f(int (*cb)(int)){return cb(6);}\n"
         "int g(int x){return x+1;} return f(g)==7;\n")},
    {__LINE__, SVI("long f(long); long f(); _Static_assert(typeof(f).param_count==1);\n"
         "long g(void){return f(7);} long f(long x){return x;} long f(); return g()==7;\n")},
    {__LINE__, SVI("static int a[3]={3,5,7}; constexpr struct S {int* p;} s={a};\n"
         "constexpr int* p=&a[2]; _Static_assert((&s)->p==a&&*(&p)-a==2);\n"
         "static _Bool b=(&s)->p; static int* q=*(&p);\n"
         "return b&&q==&a[2]&&*q==7;\n")},
    {__LINE__, SVI("struct S {int a[8];} s={}; int i=0,j=0;\n"
         "int a=!((i++,s.a)); int b=((i++,s.a)&&(j++,1));\n"
         "int c=((i++,s.a)||(j++,0)); if((i++,s.a)) i++;\n"
         "return a==0&&b==1&&c==1&&i==5&&j==1;\n")},
    {__LINE__, SVI("struct S {unsigned pad:7; unsigned __int128 n:100; unsigned tail:3;} s={5,3,6};\n"
         "int i=0,j=0; unsigned __int128 r=((i++,s.n)+=(j++,1)); s.n<<=80; s.n>>=80;\n"
         "return r==4&&s.n==4&&s.pad==5&&s.tail==6&&i==1&&j==1;\n")},
    {__LINE__, SVI("struct S {unsigned pad:3; unsigned n:3;} s={5,1}; int i=0;\n"
         "int r=((i++,s.n)=3); int old=(i++,s.n)++; int q=((i++,s.n)*=0.5);\n"
         "return r==3&&old==3&&q==2&&s.n==2&&s.pad==5&&i==3;\n")},
    {__LINE__, SVI("int f(void){return 7;} _Bool b=f; static _Bool stored=f;\n"
         "int (*p)(void)=f; int n=f?3:0; int i=0; _Bool side=(i++,f);\n"
         "int result=({i++; *p;})();\n"
         "return b&&stored&&f&&*p&&n==3&&side&&i==2&&result==7;\n")},
    {__LINE__, SVI("int a[3]={3,5,7}; int* volatile p=a; _Atomic(int*) q=a;\n"
         "int* x=p+1; int* y=q+2; const int part[:]=a[0:2];\n"
         "enum E:unsigned __int128; enum E wide[2]={1,2}; enum E* e=wide; ++e;\n"
         "return *x==5&&*y==7&&part[1]==5&&e-wide==1;\n")},
    {__LINE__, SVI("enum E {ZERO=0}; constexpr int zero=0; static int* a=1-1;\n"
         "static int* b=ZERO; static int* c=zero; int n=1; int* p=n?a:(2-2);\n"
         "return a==0&&b==0&&c==0&&p==(3-3);\n")},
    {__LINE__, SVI("int f(const int x){return x+1;} int (*p)(int)=f; int (*old)()=f;\n"
         "int (*q)(const int)=p; int n=1; int result=(n?old:p)(6);\n"
         "_Static_assert(typeof(n?old:p)==typeof(p)); return result==7&&q==old;\n")},
    {__LINE__, SVI("static int target; constexpr int* p=&target; *p=9;\n"
         "int f(int x){return x+1;} constexpr int (*fn)(int)=f;\n"
         "struct B {int value;}; struct D {int pad; struct B;};\n"
         "static struct D a[1]={{3,{7}}}; static struct B* base=a;\n"
         "constexpr struct P {int* p;} s={&target}; static const int* q=s.p;\n"
         "return *q==9&&fn(6)==7&&base->value==7;\n")},
    {__LINE__, SVI("int f(void){return 7;} const int a[2]={3,4};\n"
         "const int* p=1?a:0; int (*g)(void)=1?f:nullptr;\n"
         "_Atomic int x=3; volatile int y=3; int old=x++; int result=(y+=2);\n"
         "_Static_assert(typeof(++x)==int&&typeof(y=1)==int);\n"
         "return p[1]==4&&g()==7&&x==4&&old==3&&result==5&&y==5;\n")},
    {__LINE__, SVI("_Bool b=1; int old=b++; if(old!=1||*(unsigned char*)&b!=1) return 0;\n"
         "b=0; --b; struct S {_Bool b:1;} s={1}; int post=s.b++;\n"
         "_Atomic _Bool a=1; int atomic_post=a++; a=0; --a;\n"
         "return b==1&&post==1&&s.b==1&&atomic_post==1&&a==1;\n")},
    {__LINE__, SVI("int x=3; x*=0.5; unsigned char c=200; c/=300;\n"
         "struct S {unsigned n:3;} a[2]={{3},{5}}; int i=0,j=0;\n"
         "unsigned r=(a[i++].n*=(j++,0.5)); _Atomic int atomic=3; atomic*=0.5;\n"
         "_Atomic _Bool b=1; b+=1; return x==1&&c==0&&i==1&&j==1&&r==1&&atomic==1&&b;\n")},
    {__LINE__, SVI("static int target; static const long base={(long)&target};\n"
         "constexpr struct S {long address;} s={(long)&target};\n"
         "static long a=base+4,b=s.address+4,c=3+(2+((long)&target-1));\n"
         "return a==b&&b==c;\n")},
    {__LINE__, SVI("constexpr union U {struct P {void* p; _Type t;} p; struct B {void* p; _Type t;} b;} u={.p={}};\n"
         "constexpr struct B value=u.b; static struct B copy=value;\n"
         "_Static_assert(value.p==nullptr&&value.t.is_invalid);\n"
         "return copy.p==nullptr&&copy.t.is_invalid;\n")},
    {__LINE__, SVI("enum E:unsigned {ONE=1}; struct B {unsigned small:3; enum E bits:3;} b={1,ONE};\n"
         "constexpr struct B c={1,ONE}; static int complement=~c.bits;\n"
         "_Any boxed=+b.small; return !(b.small < -1)&&~b.bits==-2"
         "&&complement==-2&&boxed.type==int&&boxed.as(int)==1;\n")},
    {__LINE__, SVI("enum W:unsigned __int128 {HIGH=((unsigned __int128)1<<100)+7};\n"
         "unsigned x=4294967295u; switch(x){case -1: break; default: return 0;}\n"
         "int y=7; switch(y){case HIGH: break; default: return 0;}\n"
         "enum __attribute__((packed)) P {LAST=255}; enum P p=LAST;\n"
         "switch(p){case -1: return 0; case 255: return 1;} return 0;\n")},
    {__LINE__, SVI("enum E:unsigned __int128; struct S {enum E value; int tail;};\n"
         "static struct S s={(enum E)((unsigned __int128)1<<100),7};\n"
         "enum E {HIGH=(unsigned __int128)1<<100};\n"
         "return sizeof(s)==32&&s.value==HIGH&&s.tail==7&&!(enum E).is_incomplete;\n")},
    {__LINE__, SVI("enum U:unsigned __int128 {HIGH=(unsigned __int128)1<<100,NEXT};\n"
         "enum __attribute__((packed)) P {LAST=255}; static enum P p[2]={LAST,LAST};\n"
         "constexpr struct __builtin_Enumerator folded=(enum U).enumerator(1);\n"
         "struct __builtin_Enumerator runtime=(enum U).enumerator(0);\n"
         "return folded.value==HIGH+1&&runtime.value==HIGH&&runtime.type==enum U&&sizeof(p)==2&&p[1]==255;\n")},
    {__LINE__, SVI("constexpr union U {_Any a; struct B {_Type tag; int value;} b;} u={.a=7};\n"
         "_Static_assert(u.b.tag==int&&u.b.value==7); static struct B copy=u.b;\n"
         "return copy.tag==int&&copy.value==7;\n")},
    {__LINE__, SVI("constexpr union U {int* p; struct B {int* p;} b;} u={.p=(int*)7};\n"
         "constexpr struct B value=u.b; static struct B copy=value;\n"
         "_Static_assert(value.p==(int*)7); return copy.p==(int*)7;\n")},
    {__LINE__, SVI("constexpr union U {_Type t; unsigned long bits; struct B {unsigned low;} b;}\n"
         "u={.t=int,.b.low=7}; static struct B copy=u.b;\n"
         "_Static_assert(u.b.low==7); return copy.low==7;\n")},
    {__LINE__, SVI("int sum(int args...){int r=0; for(size_t i=0;i<args.count;i++) r+=args[i]; return r;}\n"
         "return sum(1,2,3)==6&&sum()==0;\n")},
    {__LINE__, SVI("int sum(int start=1,int args...,int scale=2){int r=start;for(size_t i=0;i<args.count;i++)r+=args[i];return r*scale;}\n"
         "return sum()==2&&sum(1,2,3,.scale=3)==18&&sum(.scale=3,1,2,3)==18&&sum(.args=(int[]){2,3})==12;\n")},
    {__LINE__, SVI("int check(_Any args...){return args.count==2&&args[0].as(int)==7&&args[1].as(double)==3.;}\n"
         "return check(7,3.);\n")},
    {__LINE__, SVI("static int a[4]={3,5,7,9}; constexpr const int part[:]=a[1:4];\n"
         "static const int tail[:]=part[1:]; static const int head[:]=part[:1];\n"
         "static const int empty[:]=a[4:4]; static const int whole[:]=a;\n"
         "return tail.count==2&&tail.data==a+2&&tail[1]==9&&head[0]==5\n"
         "&&empty.count==0&&empty.data==a+4&&whole.count==4&&whole.data==a;\n")},
    {__LINE__, SVI("constexpr const char part[:]=\"abcd\"[1:3];\n"
         "static const char tail[:]=part[1:]; return tail.count==1&&tail[0]=='c';\n")},
    {__LINE__, SVI("constexpr int part[:]=((int*)16)[1:3]; constexpr const int qualified[:]=part;\n"
         "static const int tail[:]=qualified[1:]; return tail.count==1&&tail.data==(int*)24;\n")},
    {__LINE__, SVI("static int target; struct I {int n; int* p;};\n"
         "constexpr struct I a[1025]={[1024]={9,&target}};\n"
         "constexpr const struct I* p=a+1024; static struct I copy=p[0];\n"
         "return copy.n==9&&copy.p==&target;\n")},
    {__LINE__, SVI("static int target; struct P {int* p;}; struct M {_Type t;};\n"
         "constexpr _Any pointer=(struct P){&target}, metadata=(struct M){long};\n"
         "static struct P copy=pointer.as(struct P); static struct M tag=metadata.as(struct M);\n"
         "return tag.t==long&&copy.p==&target;\n")},
    {__LINE__, SVI("if(0){struct I {int a,b;}; const struct S {struct I i; int unrelated;}\n"
         "s={{1/0,2},1/0,.i.a=9}; static struct I copy=s.i;\n"
         "if(copy.a!=9||copy.b!=2) return 0;} return 1;\n")},
    {__LINE__, SVI("static int target; struct I {_Type t; int* p;};\n"
         "constexpr union U {struct I a,b;} u={.a={int,&target}};\n"
         "constexpr const struct I* p=&u.b; constexpr struct I copies[2]={*p,*p};\n"
         "constexpr struct I x=copies[1]; static struct I copy=x;\n"
         "_Static_assert(x.t==int&&x.p==&target); return copy.p==&target;\n")},
    {__LINE__, SVI("constexpr int a[4]={3,5,7,9}; constexpr const int* p=a+2;\n"
         "constexpr const int part[:]=a[1:3]; constexpr int x=(0,p)[-1]+part[1]+*p;\n"
         "_Static_assert(x==19); static int copy=x; return copy;\n")},
    {__LINE__, SVI("struct I {int a,b;}; constexpr struct I a[1025]={[1024]={3,4}};\n"
         "constexpr const struct I* p=a+1024; constexpr struct I copy=p[0];\n"
         "_Static_assert(p->b==4&&copy.a==3); static struct I stored=copy; return stored.b;\n")},
    {__LINE__, SVI("constexpr int a[1][1][1][1][1][1][2]={{{{{{{3,4}}}}}}};\n"
         "constexpr const int* p=&a[0][0][0][0][0][0][1];\n"
         "_Static_assert(p[-1]==3); static int x=p[0]; return x;\n")},
    {__LINE__, SVI("struct S {int hello;};\n"
         "constexpr union U {struct __builtin_Field a,b;} u={.a=(struct S).field(0)};\n"
         "constexpr struct __builtin_Field f=(struct S).field(u.b.name);\n"
         "_Static_assert(f.type==int); return f.name.count==5&&f.offset==0;\n")},
    {__LINE__, SVI("static int target; struct I {_Type t; int* p;};\n"
         "constexpr union U {struct I a,b;} u={.a={int,&target}};\n"
         "constexpr struct I x=(0,u).b; static struct I copy=x;\n"
         "_Static_assert(x.t==int&&x.p==&target); return copy.t==int&&copy.p==&target;\n")},
    {__LINE__, SVI("constexpr union U {unsigned char bytes[8]; struct I {unsigned char c; int n;} i;}\n"
         "u={.bytes={1,2,3,4,5,6,7,8}}; constexpr struct I x=u.i;\n"
         "constexpr union U alias={.i=x}; static union U copy=alias;\n"
         "_Static_assert(alias.bytes[1]==2); return copy.bytes[7];\n")},
    {__LINE__, SVI("struct I {int a,b;}; struct S {struct I x[1025];};\n"
         "constexpr struct S s={.x[1024].a=3,.x[1024].b=4};\n"
         "constexpr struct I y=s.x[1024]; static struct I copy=y; return copy.a+copy.b;\n")},
    {__LINE__, SVI("struct I {int a,b;}; struct S {struct I x; int z;};\n"
         "constexpr struct S s={1+2,3+4,9,.x.b=5+6};\n"
         "constexpr struct I x=s.x; static struct I copy=x; return copy.a+copy.b;\n")},
    {__LINE__, SVI("static int target; struct I {int a,b; int* p;}; struct S {struct I x;};\n"
         "constexpr struct S s={1,2,&target,.x.b=7}; constexpr struct I x=s.x;\n"
         "_Static_assert(x.a==1&&x.b==7&&x.p==&target); return x.b;\n")},
    {__LINE__, SVI("struct L {int value; int bump(_Self* s){return ++s.value;}};\n"
         "struct S {int pad; struct {int pad2; struct {struct {struct {struct {struct {struct L;};};};};};};};\n"
         "int read(struct L* l){return l.value;} struct S s={.value=4};\n"
         "struct L* p=&s; int *v=&s.value; return s.bump()+read(p)+*v;\n")},
    {__LINE__, SVI("int f(int x){int a[1025]={[0]=1,[1]=2,[2]=3,[3]=4,[1024]=x}; return a[1024];}\nreturn f(7);\n")},
    {__LINE__, SVI("int f(int x){int a[1][1][1][1][1][1][5]={{{{{{{1,2,x,4,5}}}}}}}; return a[0][0][0][0][0][0][2];}\nreturn f(7);\n")},
    {__LINE__, SVI("static int a[2]={3,4}; struct P {int* p;};\n"
         "constexpr _Any boxed=(struct P){&a[1]}; static _Any copy=boxed;\n"
         "static int* p=boxed.as(struct P).p;\n"
         "const _Any text=\"hello\"+1; static _Any values[2]={boxed,text};\n"
         "return copy.as(struct P).p==p&&values[1].as(char*)[0]=='e';\n")},
    {__LINE__, SVI("static int g=9; struct S {int* p;int a[6];};\n"
         "int f(int x){struct S s={&g,{1,x,3,x+1,5,6}};\n"
         "return *s.p+s.a[1]+s.a[3]+s.a[5];} return f(7);\n")},
    {__LINE__, SVI("struct S {int values[3]; int* p;};\n"
         "static struct S s={{1,2,3},&s.values[1]};\n"
         "static const char* text=\"abc\"+1;\n"
         "return *s.p+text[0];\n")},
    {__LINE__, SVI("static int a[3]={2,4,6}; static int s[:]=(a[:])[1:];\n"
         "constexpr struct S {const char* p[2];} v={{\"ab\",\"cd\"}};\n"
         "static const char* p=v.p[1]+1; return s[0]+p[0];\n")},
    {__LINE__, SVI("int g; int leaf(int n){return n ? leaf(n-1)+1 : ++g;}\n"
         "int middle(int n){static int (*p)(int)=leaf; return p(n);}\n"
         "int macro(int n){return middle(n);}\n"
         "#pragma procmacro macro\n"
         "return macro(3);\n")},
    {__LINE__, SVI("constexpr _Any c=1.f; _Static_assert(c.as(float)==1.f);\n"
         "constexpr _Type T=c.type; _Static_assert(T==float);\n"
         "constexpr const char* text=\"ok\"; _Static_assert(text[1]=='k');\n"
         "_Any f(_Any a){a=a.as(int)+1; return a;}\n"
         "_Any a=f(41); return a.as(int);\n")},
    {__LINE__, SVI("_Any boxed(int x){return x+1;}\n"
         "#pragma procmacro boxed\n"
         "return boxed(41);\n")},
    {__LINE__, SVI("_Module m = __compile(\"int f(int n){ return n ? f(n-1) : 0; } f(3);\", \"\");\n"
         "if(m) m.run();\n"
         "return 0;\n")},
    {__LINE__, SVI("_Module m = __compile(\"struct S { int x; }; struct S s = {42}; int f(void){return s.x;}\", \"\");\n"
         "if(m){ _Any s = m.symbol(\"s\"); _Any f = m.symbol(\"f\"); }\n"
         "return 0;\n")},
    {__LINE__, SVI("const char name[:] = __builtin_intern(\"new_interned_name\");\n"
         "_Type t = __root_module().parse_type(\"struct OomType { int value; }\");\n"
         "return name.count != 0 && t.is_valid;\n")},
    {__LINE__, SVI("int recurse(int n){\n"
         "int *p = __builtin_alloca(sizeof(int)); *p = n;\n"
         "return n ? recurse(n-1) + *p : 0; }\n"
         "return recurse(8);\n")},
    {__LINE__, SVI("struct S { int a[5]; };\n"
         "int f(void){ static struct S s = {{1,2,3,4,5}}; return s.a[4]++; }\n"
         "return f()+f();\n")},
    {__LINE__, SVI("int twice(int x){ return x*2; }\n"
         "#pragma procmacro twice\n"
         "return twice(21);\n")},
    {__LINE__, SVI("int x;\n"
         "int identity(int x){ return x; }\n"
         "#pragma procmacro identity\n"
         "return identity(({ x = 2; switch(x){case 2: x=3; break; default: x=4;} x; }));\n")},
    {__LINE__, SVI("return 13;\n")},
    {__LINE__, SVI("int x = 3 + 4;\nreturn x;\n")},
    {__LINE__, SVI("struct S { int x; int y; };\n"
         "struct S s = {3, 4};\n"
         "return s.x + s.y;\n")},
    {__LINE__, SVI("int add(int a, int b){ return a + b; }\n"
         "return add(3, 4);\n")},
    {__LINE__, SVI("int arr[5] = {1, 2, 3, 4, 5};\n"
         "int sum = 0;\n"
         "for(int i = 0; i < 5; i++) sum += arr[i];\n"
         "return sum;\n")},
    {__LINE__, SVI("enum E { A, B, C };\n"
         "return C;\n")},
    {__LINE__, SVI("typedef struct Node Node;\n"
         "struct Node { int val; Node *next; };\n"
         "Node c = {3, 0};\n"
         "Node b = {2, &c};\n"
         "Node a = {1, &b};\n"
         "int sum = 0;\n"
         "for(Node *p = &a; p; p = p->next) sum += p->val;\n"
         "return sum;\n")},
    {__LINE__, SVI("_Static_assert(sizeof(int) == 4, \"\");\n"
         "constexpr int x = 6 * 7;\n"
         "return x;\n")},
    {__LINE__, SVI("struct Inner { int a; int b; };\n"
         "struct Outer { int tag; struct Inner in; };\n"
         "struct Outer o = {.tag = 2, .in = {10, 20}};\n"
         "switch(o.tag){\n"
         "  case 1: return o.in.a;\n"
         "  case 2: return o.in.b;\n"
         "  default: return -1;\n"
         "}\n")},
    {__LINE__, SVI("typedef int (*BinOp)(int, int);\n"
         "int add(int a, int b){ return a + b; }\n"
         "int mul(int a, int b){ return a * b; }\n"
         "BinOp ops[2] = {add, mul};\n"
         "return ops[0](3, 4) + ops[1](3, 4);\n")},
    {__LINE__, SVI("int fact(int n){ if(n <= 1) return 1; return n * fact(n-1); }\n"
         "return fact(6);\n")},
    {__LINE__, SVI("int x = 1;\n"
         "float f = 2.0f;\n"
         "return _Generic(x, int: 10, float: 20, default: 30)\n"
         "     + _Generic(f, int: 10, float: 20, default: 30);\n")},
    {__LINE__, SVI("struct S { int tag; union { int ival; float fval; }; };\n"
         "struct S s = {.tag = 1, .ival = 42};\n"
         "return s.tag + s.ival;\n")},
    {__LINE__, SVI("struct V { int x; int y; };\n"
         "struct V* p = &(struct V){.x=10, .y=20};\n"
         "return p->x + p->y;\n")},
    {__LINE__, SVI("struct S { int a; int b; int c; };\n"
         "_Static_assert((struct S).fields == 3, \"\");\n"
         "_Static_assert((struct S).is_struct, \"\");\n"
         "_Static_assert((int).is_integer, \"\");\n"
         "_Static_assert((int*).pointee.is_integer, \"\");\n"
         "constexpr int x = 6 * 7;\n"
         "return x;\n")},
    {__LINE__, SVI("const char* s = \"hello\";\n"
         "unsigned short w[] = u\"AB\";\n"
         "unsigned int u[] = U\"\\U0001F600\";\n"
         "return s[0] + w[0] + (u[0] == 0x1F600);\n")},
    {__LINE__, SVI("int f(int a, int b, int c){ return a * 100 + b * 10 + c; }\n"
         "return f(.c = 3, .a = 1, .b = 2);\n")},
    {__LINE__, SVI("int x = 10;\n"
         "int y = 0;\n"
         "__atomic_load(&x, &y, __ATOMIC_SEQ_CST);\n"
         "int old = __atomic_fetch_add(&x, 5, __ATOMIC_SEQ_CST);\n"
         "return old + y;\n")},
    {__LINE__, SVI("int sum = 0;\n"
         "int i = 0;\n"
         "top:\n"
         "if(i >= 5) goto done;\n"
         "int j = 0;\n"
         "do {\n"
         "  sum += i + j;\n"
         "  j++;\n"
         "} while(j < 3);\n"
         "i++;\n"
         "goto top;\n"
         "done:\n"
         "return sum;\n")},
    {__LINE__, SVI("struct Vec { int x; int y; };\n"
         "int mag2(struct Vec* v){ return v->x * v->x + v->y * v->y; }\n"
         "struct Vec v = {3, 4};\n"
         "return v.mag2();\n")},
    {__LINE__, SVI("typedef unsigned char u8;\n"
         "typedef unsigned short u16;\n"
         "enum Color : u8 { RED, GREEN, BLUE };\n"
         "enum Color c = BLUE;\n"
         "return (int)c + sizeof(enum Color);\n")},
};

static
int
run_one(Allocator al, StringView program, int64_t*_Nullable setup_allocs_out){
    int err = 0;
    ArenaAllocator arena = {0};
    arena.base = al;
    Allocator arena_al = allocator_from_arena(&arena);
    FileCache* fc = fc_create(arena_al, FC_FLAGS_NONE);
    if(!fc){ err = 1; goto done; }
    MStringBuilder log_sb = {.allocator=arena_al};
    MsbLogger logger_ = {0};
    Logger* logger = msb_logger(&logger_, &log_sb);
    AtomTable at = {.arena.base=arena_al};
    Environment env = {.allocator = arena_al, .at=&at};
    CiInterpreter interp = {
        .procedural_macros = 1,
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
    interp.parser.cpp.synth_arena.base = al;
    interp.parser.scratch_arena.base = al;
    interp.bt.arena.base = al;
    fc_write_path(fc, "(oom-test)", sizeof "(oom-test)" - 1);
    err = fc_cache_file(fc, program);
    if(err) goto cleanup;
    err = cpp_define_builtin_macros(&interp.parser.cpp);
    if(err) goto cleanup;
    err = cc_define_builtin_types(&interp.parser);
    if(err) goto cleanup;
    err = cc_register_pragmas(&interp.parser);
    if(err) goto cleanup;
    err = ci_register_pragmas(&interp);
    if(err) goto cleanup;
    err = ci_register_macros(&interp);
    if(err) goto cleanup;
    CppFrame frame_data = {
        .file_id = (uint32_t)fc->map.count - 1,
        .txt = program,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CppFrame)(&interp.parser.cpp.frames, interp.parser.cpp.allocator, frame_data);
    if(err) goto cleanup;
    if(setup_allocs_out){
        TestingAllocator* ta = al._data;
        *setup_allocs_out = ta->nallocs;
    }
    err = cc_parse_all(&interp.parser);
    if(err) goto cleanup;
    err = ci_resolve_refs(&interp);
    if(err) goto cleanup;
    {
        CiInterpFrame* frame = &interp.top_frame;
        err = ci_prepare_toplevel(&interp);
        if(err) goto cleanup;
        while(frame->pc < frame->op_count){
            err = ci_interp_step(&interp, frame);
            if(err) goto cleanup;
        }
    }
    cleanup:
    msb_destroy(&log_sb);
    ci_tls_cleanup(&interp);
    ArenaAllocator_free_all(&interp.bt.arena);
    ArenaAllocator_free_all(&at.arena);
    ArenaAllocator_free_all(&interp.parser.cpp.synth_arena);
    ArenaAllocator_free_all(&interp.parser.scratch_arena);
    ArenaAllocator_free_all(&arena);
    done:
    return err;
}

enum { NUM_PROGRAMS = sizeof test_programs / sizeof test_programs[0] };

static
size_t
recording_count_leaked(RecordingAllocator* r){
    size_t leaked = 0;
    for(size_t i = 0; i < r->count; i++){
        if(r->allocation_sizes[i]){
            leaked += r->allocation_sizes[i];
            #ifdef HEAVY_RECORDING
            dump_bt(r->backtraces[i]);
            #endif
        }
    }
    return leaked;
}

TestFunction(test_oom_setup){
    TESTBEGIN();
    // Baseline to find setup_allocs.
    TestingAllocator ta0 = {0};
    LOCK_T_init(&ta0.lock);
    Allocator al0 = {.type = ALLOCATOR_TESTING, ._data = &ta0};
    int64_t setup_allocs = 0;
    int err = run_one(al0, SV("return 0;\n"), &setup_allocs);
    if(err) TestReport("setup baseline failed");
    if(0) TestPrintf("setup: %lld allocations\n", (long long)setup_allocs);
    recording_free_all(&ta0.recorder);
    recording_cleanup(&ta0.recorder);
    if(!err){
        static int idx = 0;
        for(int64_t fail = test_atomic_increment(&idx) + 1; fail <= setup_allocs; fail = test_atomic_increment(&idx) + 1){
            TestingAllocator ta = {0};
            LOCK_T_init(&ta.lock);
            Allocator al = {.type = ALLOCATOR_TESTING, ._data = &ta};
            ta.fail_at = fail;
            run_one(al, SV("return 0;\n"), NULL);
            if(check_leaks){
                size_t leaked = recording_count_leaked(&ta.recorder);
                if(leaked){
                    TEST_stats.failures++;
                    TestPrintf("setup fail_at=%lld: leaked %zu bytes\n", (long long)fail, leaked);
                }
            }
            TEST_stats.executed++;
            recording_free_all(&ta.recorder);
            recording_cleanup(&ta.recorder);
        }
    }
    TESTEND();
}

TestFunction(test_oom){
    TESTBEGIN();
    for(_Bool did_work = 1;did_work;){
        did_work = 0;
        for(size_t p = 0; p < NUM_PROGRAMS; p++){
            struct OomTestCase* tc = &test_programs[p];
            {
                int state = test_atomic_load_acquire(&tc->baseline_done);
                if(state == NOT_STARTED){
                    if(test_atomic_cas(&tc->baseline_done, NOT_STARTED, IN_PROGRESS)){
                        TestingAllocator ta = {0};
                        LOCK_T_init(&ta.lock);
                        Allocator al = {.type = ALLOCATOR_TESTING, ._data = &ta};
                        int err = run_one(al, tc->program, &tc->setup_allocs);
                        TestExpectFalse(int, err);
                        tc->max_allocs = err ? 0 : ta.nallocs;
                        recording_free_all(&ta.recorder);
                        recording_cleanup(&ta.recorder);
                        test_atomic_store_release(&tc->baseline_done, DONE);
                        did_work = 1;
                    }
                    continue;
                }
                if(state == 1) continue;
            }
            int64_t n = test_atomic_increment(&tc->fail_idx) + 1;
            int64_t fail = tc->setup_allocs + n;
            if(fail > tc->max_allocs) continue;
            did_work = 1;
            TestingAllocator ta = {0};
            LOCK_T_init(&ta.lock);
            Allocator al = {.type = ALLOCATOR_TESTING, ._data = &ta};
            ta.fail_at = fail;
            run_one(al, tc->program, NULL);
            if(check_leaks){
                size_t leaked = recording_count_leaked(&ta.recorder);
                if(leaked){
                    TEST_stats.failures++;
                    TestPrintf("program %zu (line %d) fail_at=%lld: leaked %zu bytes\n",
                        p, tc->line, (long long)fail, leaked);
                }
            }
            TEST_stats.executed++;
            recording_free_all(&ta.recorder);
            recording_cleanup(&ta.recorder);
        }
    }
    TESTEND();
}


TestFunction(test_blob_size_overflow){
    TESTBEGIN();
    size_t lengths[] = {SIZE_MAX, SIZE_MAX - 15, (size_t)PTRDIFF_MAX};
    for(size_t i = 0; i < sizeof lengths / sizeof lengths[0]; i++){
        // Deny allocations so an unchecked length cannot reach memcpy.
        TestingAllocator ta = {0};
        LOCK_T_init(&ta.lock);
        Allocator al = {.type = ALLOCATOR_TESTING, ._data = &ta};
        BlobTable bt = {.arena.base = al};
        unsigned char byte = 0;
        BlobAtom seed = BT_atomize(&bt, &byte, 1);
        TestExpectTrue(_Bool, seed != NULL);
        ta.nallocs = 0;
        ta.fail_at = -1;
        BlobAtom blob = BT_raw_atomize(&bt, &byte, lengths[i]);
        TestExpectTrue(_Bool, blob == NULL);
        blob = BT_atomize(&bt, &byte, lengths[i]);
        TestExpectTrue(_Bool, blob == NULL);
        blob = BT_get_atom(&bt, &byte, lengths[i]);
        TestExpectTrue(_Bool, blob == NULL);
        TestExpect(int64_t, ta.nallocs, ==, 0);
        ArenaAllocator_free_all(&bt.arena);
        recording_free_all(&ta.recorder);
        recording_cleanup(&ta.recorder);
    }
    TESTEND();
}

int main(int argc, char** argv){
    RegisterTestFlags(test_blob_size_overflow, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_oom_setup, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_oom, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    ArgToParse extra_args[] = {
        {
            .name = SVI("--check-leaks"),
            .dest = ARGDEST(&check_leaks),
            .help = "Report leaked bytes after each OOM iteration as test failures.",
        },
    };
    ArgParseKwParams extra = {
        .args = extra_args,
        .count = sizeof extra_args / sizeof extra_args[0],
    };
    return test_main(argc, argv, &extra);
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
