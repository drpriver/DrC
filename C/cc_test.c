//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#define HEAVY_RECORDING
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

#include "../Drp/compiler_warnings.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
static void cc_print_type(MStringBuilder* sb, CcQualType t);
static void cc_print_expr(MStringBuilder* sb, CcExpr* e);
TestFunction(test_parse_decls){
    TESTBEGIN();
    enum {N=8}; // can increase if we need to
    struct Case {
        const char* test; int line;
        StringView input;
        struct {
            StringView name;
            StringView repr;
            StringView init; // expected cc_print_expr output, empty = no check
            StringView mangle; // expected asm label, empty = no check
        } vars[N];
        struct {
            StringView name;
            StringView repr;
            StringView mangle; // expected asm label, empty = no check
        } funcs[N];
        struct {
            StringView name;
            StringView repr;
        } typedefs[N];
        _Bool skip;
    } testcases[] = {
        {
            "parse decls", __LINE__,
            SV("int (*x)[10];\n"
               "int (y);\n"
               "int bar(int x);\n"
               "void (*signal(int, void (*fp)(int)))(int);\n"
               "const int **restrict * p;\n"
               "typedef int foo(void);\n"
              ),
            {
                { SV("x"), SV("int (*)[10]") },
                { SV("y"), SV("int") },
                { SV("p"), SV("const int * *restrict  *") },
            },
            {
                {SV("bar"), SV("int(int)")},
                {SV("signal"), SV("void (*(int, void (*)(int)))(int)")},
            },
            {
                {SV("foo"), SV("int(void)")},
            },
        },
        {
            "basic types", __LINE__,
            SV("int a;\n"
               "char b;\n"
               "unsigned long long c;\n"
               "short d;\n"
               "void *e;\n"
              ),
            .vars={
                { SV("a"), SV("int") },
                { SV("b"), SV("char") },
                { SV("c"), SV("unsigned long long") },
                { SV("d"), SV("short") },
                { SV("e"), SV("void *") },
            },
        },
        {
            "pointers", __LINE__,
            SV("int *a;\n"
               "int **b;\n"
               "int ***c;\n"
               "const int *d;\n"
               "int *const e;\n"
               "const int *const volatile f;\n"
              ),
            .vars = {
                { SV("a"), SV("int *") },
                { SV("b"), SV("int * *") },
                { SV("c"), SV("int * * *") },
                { SV("d"), SV("const int *") },
                { SV("e"), SV("int *const ") },
                { SV("f"), SV("const int *const volatile ") },
            },
        },
        {
            "arrays", __LINE__,
            SV("int a[5];\n"
               "int b[3][4];\n"
               "int *c[10];\n"
               "int (*d)[10];\n"
               "int e[];\n"
              ),
            .vars = {
                { SV("a"), SV("int[5]") },
                { SV("b"), SV("int[3][4]") },
                { SV("c"), SV("int *[10]") },
                { SV("d"), SV("int (*)[10]") },
                { SV("e"), SV("int[]") },
            },
        },
        {
            "functions", __LINE__,
            SV("int f(void);\n"
               "int g(int, int);\n"
               "int h(int, ...);\n"
               "int k();\n"
              ),
            .funcs = {
                { SV("f"), SV("int(void)") },
                { SV("g"), SV("int(int, int)") },
                { SV("h"), SV("int(int, ...)") },
                { SV("k"), SV("int()") },
            },
        },
        {
            "function pointers", __LINE__,
            SV("int (*fp)(int, int);\n"
               "void (*vfp)(void);\n"
               "int (*(*fpp)(int))(int);\n"
              ),
            .vars = {
                { SV("fp"), SV("int (*)(int, int)") },
                { SV("vfp"), SV("void (*)(void)") },
                { SV("fpp"), SV("int (* (*)(int))(int)") },
            },
        },
        {
            "multiple declarators", __LINE__,
            SV("int a, *b, **c;\n"),
            .vars = {
                { SV("a"), SV("int") },
                { SV("b"), SV("int *") },
                { SV("c"), SV("int * *") },
            },
        },
        {
            "array of function pointers", __LINE__,
            SV("int (*a[4])(int);\n"),
            .vars = {
                { SV("a"), SV("int (*[4])(int)") },
            },
        },
        {
            "abstract declarators in params", __LINE__,
            SV("void f(int (int));\n"
               "void g(int (*)(int));\n"
               "void h(int [10]);\n"
               "void k(int (*)[10]);\n"
               "void m(int *, int **, const int *);\n"
               "void n(int ());\n"
              ),
            .funcs = {
                { SV("f"), SV("void(int(int))") },
                { SV("g"), SV("void(int (*)(int))") },
                { SV("h"), SV("void(int[10])") },
                { SV("k"), SV("void(int (*)[10])") },
                { SV("m"), SV("void(int *, int * *, const int *)") },
                { SV("n"), SV("void(int())") },
            },
        },
        {
            "typedef void", __LINE__,
            SV("typedef void myvoid;\n"
               "myvoid *p;\n"
               "myvoid (*fn)(int);\n"
              ),
            .vars = {
                { SV("p"), SV("void *") },
                { SV("fn"), SV("void (*)(int)") },
            },
            .typedefs = {
                { SV("myvoid"), SV("void") },
            },
        },
        {
            "typedefs as base types", __LINE__,
            SV("typedef int myint;\n"
               "myint a;\n"
               "myint *b;\n"
               "myint c[5];\n"
               "myint (*d)(myint);\n"
              ),
            .vars = {
                { SV("a"), SV("int") },
                { SV("b"), SV("int *") },
                { SV("c"), SV("int[5]") },
                { SV("d"), SV("int (*)(int)") },
            },
            .typedefs = {
                { SV("myint"), SV("int") },
            },
        },
        {
            "typedef typedef", __LINE__,
            SV("typedef int x;\n"
                "typedef x y;"),
            .typedefs = {
                {SV("x"), SV("int")},
                {SV("y"), SV("int")},
            },
        },
        {
            "typedef typedef pointer", __LINE__,
            SV("typedef void* list;\n"
                "typedef list list2;"),
            .typedefs = {
                {SV("list"), SV("void *")},
                {SV("list2"), SV("void *")},
            },
        },
        {
            "typedef pointer", __LINE__,
            SV("typedef int *intptr;\n"
               "intptr a;\n"
               "intptr *b;\n"
               "const intptr c;\n"
              ),
            .vars = {
                { SV("a"), SV("int *") },
                { SV("b"), SV("int * *") },
                { SV("c"), SV("int *const ") },
            },
            .typedefs = {
                { SV("intptr"), SV("int *") },
            },
        },
        {
            "typedef function type", __LINE__,
            SV("typedef int fn(int, int);\n"
               "fn *fp;\n"
              ),
            .vars = {
                { SV("fp"), SV("int (*)(int, int)") },
            },
            .typedefs = {
                { SV("fn"), SV("int(int, int)") },
            },
        },
        {
            "typedef in function params", __LINE__,
            SV("typedef int myint;\n"
               "void f(myint x, myint *y);\n"
              ),
            .funcs = {
                { SV("f"), SV("void(int, int *)") },
            },
            .typedefs = {
                { SV("myint"), SV("int") },
            },
        },
        {
            "typedef paren disambiguation", __LINE__,
            SV("typedef int T;\n"
               "void f(T);\n"
               "void g(T x);\n"
               "int (h);\n"
              ),
            .vars = {
                { SV("h"), SV("int") },
            },
            .funcs = {
                { SV("f"), SV("void(int)") },
                { SV("g"), SV("void(int)") },
            },
            .typedefs = {
                { SV("T"), SV("int") },
            },
        },
        {
            "chained typedefs", __LINE__,
            SV("typedef int A;\n"
               "typedef A B;\n"
               "typedef B *C;\n"
               "C x;\n"
              ),
            .vars = {
                { SV("x"), SV("int *") },
            },
            .typedefs = {
                { SV("A"), SV("int") },
                { SV("B"), SV("int") },
                { SV("C"), SV("int *") },
            },
        },
        {
            "typeof basic", __LINE__,
            SV("typeof(int) a;\n"
               "typeof(int *) b;\n"
               "typeof(int [5]) c;\n"
               "typeof(int (*)(int)) d;\n"
              ),
            .vars = {
                { SV("a"), SV("int") },
                { SV("b"), SV("int *") },
                { SV("c"), SV("int[5]") },
                { SV("d"), SV("int (*)(int)") },
            },
        },
        {
            "typeof with typedef", __LINE__,
            SV("typedef const int *cip;\n"
               "typeof(cip) a;\n"
               "typeof(cip) *b;\n"
              ),
            .vars = {
                { SV("a"), SV("const int *") },
                { SV("b"), SV("const int * *") },
            },
            .typedefs = {
                { SV("cip"), SV("const int *") },
            },
        },
        {
            "typeof_unqual", __LINE__,
            SV("typeof_unqual(const int) a;\n"
               "typeof_unqual(const volatile int *) b;\n"
               "typeof_unqual(int *const) c;\n"
              ),
            .vars = {
                { SV("a"), SV("int") },
                { SV("b"), SV("const volatile int *") },
                { SV("c"), SV("int *") },
            },
        },
        {
            "typeof in typedef", __LINE__,
            SV("typedef typeof(int *) intptr;\n"
               "intptr a;\n"
              ),
            .vars = {
                { SV("a"), SV("int *") },
            },
            .typedefs = {
                { SV("intptr"), SV("int *") },
            },
        },
        {
            "typeof expression", __LINE__,
            SV("typeof(1) a;\n"
               "typeof(1.0) b;\n"
               "typeof(1.0f) c;\n"
               "typeof(1 + 2) d;\n"
              ),
            .vars = {
                { SV("a"), SV("int") },
                { SV("b"), SV("double") },
                { SV("c"), SV("float") },
                { SV("d"), SV("int") },
            },
        },
        {
            "sizeof in array dim", __LINE__,
            SV("int a[sizeof(int)];\n"
               "int b[sizeof(char)];\n"
               "int c[sizeof(int*)];\n"
               "int d[alignof(int)];\n"
              ),
            .vars = {
                { SV("a"), SV("int[4]") },
                { SV("b"), SV("int[1]") },
                { SV("c"), SV("int[8]") },
                { SV("d"), SV("int[4]") },
            },
        },
        {
            "alignof expr", __LINE__,
            SV("int x;\n"
               "int a[_Alignof x];\n"
               "int *p;\n"
               "int b[_Alignof p];\n"
               "double d;\n"
               "int c[_Alignof d];\n"
              ),
            .vars = {
                { SV("x"), SV("int") },
                { SV("a"), SV("int[4]") },
                { SV("p"), SV("int *") },
                { SV("b"), SV("int[8]") },
                { SV("d"), SV("double") },
                { SV("c"), SV("int[8]") },
            },
        },
        {
            "_Alignas on decl", __LINE__,
            SV("_Alignas(16) int x;\n"
               "_Alignas(32) char buf[64];\n"
              ),
            .vars = {
                { SV("x"), SV("int") },
                { SV("buf"), SV("char[64]") },
            },
        },
        {
            "_Alignof _Alignas decl", __LINE__,
            SV("_Alignas(16) int x;\n"
               "int a[_Alignof x];\n"
              ),
            .vars = {
                { SV("x"), SV("int") },
                { SV("a"), SV("int[16]") },
            },
        },
        {
            "_Alignas with type arg", __LINE__,
            SV("_Alignas(double) int x;\n"
               "int a[_Alignof x];\n"
              ),
            .vars = {
                { SV("x"), SV("int") },
                { SV("a"), SV("int[8]") },
            },
        },
        {
            "prefix aligned attribute on decl", __LINE__,
            SV("__attribute__((aligned(16))) int x;\n"
               "int a[_Alignof x];\n"
              ),
            .vars = {
                { SV("x"), SV("int") },
                { SV("a"), SV("int[16]") },
            },
        },
        {
            "typeof sizeof", __LINE__,
            SV("typeof(sizeof(int)) a;\n"),
            .vars = {
                { SV("a"), SV("unsigned long") },
            },
        },
        {
            "typeof cast", __LINE__,
            SV("typeof((float)1) a;\n"
               "typeof((double)1) b;\n"
               "typeof((char)1) c;\n"
              ),
            .vars = {
                { SV("a"), SV("float") },
                { SV("b"), SV("double") },
                { SV("c"), SV("char") },
            },
        },
        {
            "typeof variable", __LINE__,
            SV("int x;\n"
               "typeof(x) a;\n"
               "int *p;\n"
               "typeof(*p) b;\n"
               "typeof(&x) c;\n"
              ),
            .vars = {
                { SV("x"), SV("int") },
                { SV("a"), SV("int") },
                { SV("p"), SV("int *") },
                { SV("b"), SV("int") },
                { SV("c"), SV("int *") },
            },
        },
        {
            "typeof true/false", __LINE__,
            SV("typeof(true) a;\n"
               "typeof(false) b;\n"
              ),
            .vars = {
                { SV("a"), SV("_Bool") },
                { SV("b"), SV("_Bool") },
            },
        },
        {
            "countof in array dim", __LINE__,
            SV("int a[5];\n"
               "int b[_Countof(a)];\n"
               "int c[_Countof(int[3])];\n"
              ),
            .vars = {
                { SV("a"), SV("int[5]") },
                { SV("b"), SV("int[5]") },
                { SV("c"), SV("int[3]") },
            },
        },
        {
            "int literals", __LINE__,
            SV("int a = 42;\n"
               "int b = -1;\n"
               "unsigned c = 3u;\n"
              ),
            .vars = {
                { SV("a"), SV("int"),      SV("42") },
                { SV("b"), SV("int"),      SV("-1") },
                { SV("c"), SV("unsigned int"), SV("3") },
            },
        },
        {
            "float literals", __LINE__,
            SV("float a = 1.5f;\n"
               "double b = 2.5;\n"
              ),
            .vars = {
                { SV("a"), SV("float"),  SV("1.5f") },
                { SV("b"), SV("double"), SV("2.5") },
            },
        },
        {
            "arithmetic", __LINE__,
            SV("int a = 1 + 2;\n"
               "int b = 3 * 4 + 5;\n"
               "int c = 1 << 2;\n"
              ),
            .vars = {
                { SV("a"), SV("int"), SV("(1 + 2)") },
                { SV("b"), SV("int"), SV("((3 * 4) + 5)") },
                { SV("c"), SV("int"), SV("(1 << 2)") },
            },
        },
        {
            "comparisons", __LINE__,
            SV("int a = 1 < 2;\n"
               "int b = 3 == 4;\n"
               "int c = 5 != 6;\n"
              ),
            .vars = {
                { SV("a"), SV("int"), SV("(1 < 2)") },
                { SV("b"), SV("int"), SV("(3 == 4)") },
                { SV("c"), SV("int"), SV("(5 != 6)") },
            },
        },
        {
            "logical ops", __LINE__,
            SV("int a = 1 && 2;\n"
               "int b = 0 || 1;\n"
               "int c = !0;\n"
              ),
            .vars = {
                { SV("a"), SV("int"), SV("(1 && 2)") },
                { SV("b"), SV("int"), SV("(0 || 1)") },
                { SV("c"), SV("int"), SV("!0") },
            },
        },
        {
            "cast expr", __LINE__,
            SV("float a = (float)42;\n"
               "long b = (long)1;\n"
              ),
            .vars = {
                { SV("a"), SV("float"), SV("(float)42") },
                { SV("b"), SV("long"),  SV("(long)1") },
            },
        },
        {
            "ternary", __LINE__,
            SV("int a = 1 ? 2 : 3;\n"),
            .vars = {
                { SV("a"), SV("int"), SV("(1 ? 2 : 3)") },
            },
        },
        {
            "sizeof expr init", __LINE__,
            SV("unsigned long a = sizeof(int);\n"
               "unsigned long b = sizeof(char);\n"
              ),
            .vars = {
                { SV("a"), SV("unsigned long"), SV("4") },
                { SV("b"), SV("unsigned long"), SV("1") },
            },
        },
        {
            "variable ref", __LINE__,
            SV("int x = 10;\n"
               "int y = x;\n"
               "int z = x + 1;\n"
              ),
            .vars = {
                { SV("x"), SV("int"), SV("10") },
                { SV("y"), SV("int"), SV("x") },
                { SV("z"), SV("int"), SV("(x + 1)") },
            },
        },
        {
            "addr and deref", __LINE__,
            SV("int x = 0;\n"
               "int *p = &x;\n"
               "int y = *p;\n"
              ),
            .vars = {
                { SV("x"), SV("int") },
                { SV("p"), SV("int *"),  SV("&x") },
                { SV("y"), SV("int"),    SV("*p") },
            },
        },
        {
            "subscript", __LINE__,
            SV("int a[4];\n"
               "int b = a[2];\n"
              ),
            .vars = {
                { SV("a"), SV("int[4]") },
                { SV("b"), SV("int"),    SV("a[2]") },
            },
        },
        {
            "comma expr", __LINE__,
            SV("int a = (1, 2);\n"),
            .vars = {
                { SV("a"), SV("int"), SV("(1 , 2)") },
            },
        },
        {
            "true false nullptr", __LINE__,
            SV("_Bool a = true;\n"
               "_Bool b = false;\n"
              ),
            .vars = {
                { SV("a"), SV("_Bool"), SV("1") },
                { SV("b"), SV("_Bool"), SV("0") },
            },
        },
        {
            "func def", __LINE__,
            SV("int foo(int x, int y){ return x + y; }\n"),
            .funcs = {
                { SV("foo"), SV("int(int, int)") },
            },
        },
        {
            "forward decl then def", __LINE__,
            SV("int bar(int);\n"
               "int bar(int x){ return x; }\n"
              ),
            .funcs = {
                { SV("bar"), SV("int(int)") },
            },
        },
        {
            "basic enum", __LINE__,
            SV("enum Color { RED, GREEN, BLUE };\n"
               "enum Color c;\n"
              ),
            .vars = {
                { SV("c"), SV("enum Color") },
            },
        },
        {
            "anonymous enum", __LINE__,
            SV("enum { A, B, C };\n"
               "int x;\n"
              ),
            .vars = {
                { SV("x"), SV("int") },
            },
        },
        {
            "enum with explicit values", __LINE__,
            SV("enum { X = 10, Y = 20, Z };\n"
               "int a[Z];\n"
              ),
            .vars = {
                { SV("a"), SV("int[21]") },
            },
        },
        {
            "enum forward reference", __LINE__,
            SV("enum Foo;\n"
               "enum Foo *p;\n"
              ),
            .vars = {
                { SV("p"), SV("enum Foo *") },
            },
        },
        {
            "typedef enum", __LINE__,
            SV("typedef enum { LOW, HIGH } Level;\n"
               "Level l;\n"
              ),
            .vars = {
                { SV("l"), SV("enum <anon>") },
            },
            .typedefs = {
                { SV("Level"), SV("enum <anon>") },
            },
        },
        {
            "enum in sizeof", __LINE__,
            SV("enum E { V1, V2 };\n"
               "int a[sizeof(enum E)];\n"
              ),
            .vars = {
                { SV("a"), SV("int[4]") },
            },
        },
        {
            "enumerator in expression", __LINE__,
            SV("enum { TWO = 2, THREE = 3, FIVE = TWO + THREE };\n"
               "int a[FIVE];\n"
              ),
            .vars = {
                { SV("a"), SV("int[5]") },
            },
        },
        {
            "enum trailing comma", __LINE__,
            SV("enum T { A, B, C, };\n"
               "enum T t;\n"
              ),
            .vars = {
                { SV("t"), SV("enum T") },
            },
        },
        {
            "implicit cast int to float", __LINE__,
            SV("float f = 42;\n"),
            .vars = {
                { SV("f"), SV("float"), SV("(float)42") },
            },
        },
        // --- Implicit conversion tests (C2y 6.5.17.2) ---
        {
            "null pointer constant 0", __LINE__,
            SV("int *p = 0;\n"),
            .vars = {
                { SV("p"), SV("int *"), .init = SV("(int *)0") },
            },
        },
        {
            "nullptr to pointer", __LINE__,
            SV("int *p = nullptr;\n"),
            .vars = {
                { SV("p"), SV("int *"), .init = SV("(int *)0") },
            },
        },
        {
            "pointer to bool", __LINE__,
            SV("int x;\n"
               "_Bool b = &x;\n"),
            .vars = {
                { SV("x"), SV("int") },
                { SV("b"), SV("_Bool"), .init = SV("(_Bool)&x") },
            },
        },
        {
            "void* to int*", __LINE__,
            SV("void *vp;\n"
               "int *ip = vp;\n"),
            .vars = {
                { SV("vp"), SV("void *") },
                { SV("ip"), SV("int *"), .init = SV("(int *)vp") },
            },
        },
        {
            "int* to void*", __LINE__,
            SV("int *ip;\n"
               "void *vp = ip;\n"),
            .vars = {
                { SV("ip"), SV("int *") },
                { SV("vp"), SV("void *"), .init = SV("(void *)ip") },
            },
        },
        {
            "int* to const int*", __LINE__,
            SV("int *ip;\n"
               "const int *cip = ip;\n"),
            .vars = {
                { SV("ip"), SV("int *") },
                { SV("cip"), SV("const int *"), .init = SV("(const int *)ip") },
            },
        },
        {
            "enum to int", __LINE__,
            SV("enum E { A, B };\n"
               "enum E e;\n"
               "int x = e;\n"),
            .vars = {
                { SV("e"), SV("enum E") },
                { SV("x"), SV("int"), .init = SV("(int)e") },
            },
        },
        {
            "type inference __auto_type", __LINE__,
            SV("__auto_type x = 42;\n"),
            .vars = {
                { SV("x"), SV("int"), SV("42") },
            },
        },
        {
            "const type inference", __LINE__,
            SV("const __auto_type x = 42;\n"),
            .vars = {
                { SV("x"), SV("const int"), SV("42") },
            },
        },
        {
            "const type inference (without type)", __LINE__,
            SV("const x = 42;\n"),
            .vars = {
                { SV("x"), SV("const int"), SV("42") },
            },
        },
        {
            "pointer assignment", __LINE__,
            SV("int x;\n"
               "int *p = &x;\n"
              ),
            .vars = {
                { SV("x"), SV("int") },
                { SV("p"), SV("int *"), SV("&x") },
            },
        },
        {
            "enum to int", __LINE__,
            SV("enum { A };\n"
               "int x = A;\n"
              ),
            .vars = {
                { SV("x"), SV("int"), SV("0") },
            },
        },
        {
            "basic struct", __LINE__,
            SV("struct Foo { int x; char y; };\n"
               "struct Foo f;\n"
              ),
            .vars = {
                { SV("f"), SV("struct Foo") },
            },
        },
        {
            "struct forward ref", __LINE__,
            SV("struct Bar;\n"
               "struct Bar *p;\n"
              ),
            .vars = {
                { SV("p"), SV("struct Bar *") },
            },
        },
        {
            "struct forward then define", __LINE__,
            SV("struct S;\n"
               "struct S { int a; };\n"
               "struct S s;\n"
              ),
            .vars = {
                { SV("s"), SV("struct S") },
            },
        },
        {
            "struct in sizeof", __LINE__,
            SV("struct P { int x; char y; };\n"
               "int a[sizeof(struct P)];\n"
              ),
            .vars = {
                { SV("a"), SV("int[8]") },
            },
        },
        {
            "typedef struct", __LINE__,
            SV("typedef struct { int x; int y; } Point;\n"
               "Point p;\n"
              ),
            .vars = {
                { SV("p"), SV("struct <anon>") },
            },
            .typedefs = {
                { SV("Point"), SV("struct <anon>") },
            },
        },
        {
            "basic union", __LINE__,
            SV("union U { int i; float f; };\n"
               "union U u;\n"
              ),
            .vars = {
                { SV("u"), SV("union U") },
            },
        },
        {
            "union in sizeof", __LINE__,
            SV("union V { int i; double d; };\n"
               "int a[sizeof(union V)];\n"
              ),
            .vars = {
                { SV("a"), SV("int[8]") },
            },
        },
        {
            "nested struct", __LINE__,
            SV("struct Outer { struct Inner { int a; } inner; int b; };\n"
               "struct Outer o;\n"
              ),
            .vars = {
                { SV("o"), SV("struct Outer") },
            },
        },
        {
            "anonymous struct member", __LINE__,
            SV("struct A { struct { int x; int y; }; int z; };\n"
               "struct A a;\n"
              ),
            .vars = {
                { SV("a"), SV("struct A") },
            },
        },
        {
            "packed struct", __LINE__,
            SV("struct __attribute__((packed)) Packed { int x; char y; };\n"
               "int a[sizeof(struct Packed)];\n"
              ),
            .vars = {
                { SV("a"), SV("int[5]") },
            },
        },
        {
            "struct pointer member", __LINE__,
            SV("struct Node { int val; struct Node *next; };\n"
               "struct Node n;\n"
              ),
            .vars = {
                { SV("n"), SV("struct Node") },
            },
        },
        {
            "type defined in struct body is visible outside", __LINE__,
            SV("struct S { enum Color { RED, GREEN, BLUE }; enum Color c; };\n"
               "enum Color x;\n"
              ),
            .vars = {
                { SV("x"), SV("enum Color") },
            },
        },
        {
            "plan9", __LINE__,
            SV("struct Foo {int x;};\n"
               "struct Bar {struct Foo; int y;};\n"
               "int a[sizeof(struct Bar)];\n"
               "struct Bar b;\n"
               "int x[sizeof b.x]\n"),
            .vars = {
                {SV("a"), SV("int[8]")},
                {SV("b"), SV("struct Bar")},
                {SV("x"), SV("int[4]")},
            },
        },
        {
            "struct with bitfield", __LINE__,
            SV("struct Bits { int a : 3; int b : 5; };\n"
               "int a[sizeof(struct Bits)];\n"
              ),
            .vars = {
                { SV("a"), SV("int[4]") },
            },
        },
        {
            "struct with bitfield", __LINE__,
            SV("struct Bits { int a : 3; int b : 5; struct {int: 3;};};\n"
               "int a[sizeof(struct Bits)];\n"
              ),
            .vars = {
                { SV("a"), SV("int[8]") },
            },
        },
        {
            "enum bitfield", __LINE__,
            SV("enum E { A, B, C };\n"
               "struct S { enum E x : 3; int y : 5; };\n"
               "int a[sizeof(struct S)];\n"
              ),
            .vars = {
                { SV("a"), SV("int[4]") },
            },
        },
        {
            "static_assert pass", __LINE__,
            SV("static_assert(1, \"ok\");\n"
               "int x;\n"),
            .vars = { { SV("x"), SV("int") } },
        },
        {
            "static_assert no message (C23)", __LINE__,
            SV("static_assert(1);\n"
               "int x;\n"),
            .vars = { { SV("x"), SV("int") } },
        },
        {
            "static_assert with expression", __LINE__,
            SV("static_assert(sizeof(int) == 4, \"int must be 4 bytes\");\n"
               "int x;\n"),
            .vars = { { SV("x"), SV("int") } },
        },
        {
            "static_assert in struct", __LINE__,
            SV("struct S { int a; static_assert(sizeof(int) == 4, \"bad\"); char b; };\n"
               "struct S s;\n"),
            .vars = { { SV("s"), SV("struct S") } },
        },
        {
            "struct method declaration", __LINE__,
            SV("struct Foo { int x; int get_x(struct Foo* self); };\n"
               "int a[sizeof(struct Foo)];\n"),
            .vars = { { SV("a"), SV("int[4]") } },
        },
        {
            "struct method definition", __LINE__,
            SV("struct Foo { int x; int get_x(struct Foo* self){ return self->x; } };\n"
               "int a[sizeof(struct Foo)];\n"),
            .vars = { { SV("a"), SV("int[4]") } },
        },
        {
            "struct multiple methods", __LINE__,
            SV("struct V { int x; int y;\n"
               "  int get_x(struct V* self){ return self->x; }\n"
               "  int get_y(struct V* self){ return self->y; }\n"
               "};\n"
               "int a[sizeof(struct V)];\n"),
            .vars = { { SV("a"), SV("int[8]") } },
        },
        {
            "flexible array member", __LINE__,
            SV("struct Buf { int len; char data[]; };\n"
               "int a[sizeof(struct Buf)];\n"),
            .vars = { { SV("a"), SV("int[4]") } },
        },
        {
            "FAM with padding", __LINE__,
            SV("struct Buf { double d; int data[]; };\n"
               "int a[sizeof(struct Buf)];\n"),
            .vars = { { SV("a"), SV("int[8]") } },
        },
        {
            "FAM in union", __LINE__,
            SV("union U { int tag; char data[]; };\n"
               "int a[sizeof(union U)];\n"),
            .vars = { { SV("a"), SV("int[4]") } },
        },
        {
            "FAM in anonymous struct", __LINE__,
            SV("struct S { int n; struct { int len; char data[]; }; };\n"
               "int a[sizeof(struct S)];\n"),
            .vars = { { SV("a"), SV("int[8]") } },
        },
        {
            "incomplete type in struct", __LINE__,
            SV("struct Foo {\n"
               "    struct Bar;\n"
               "    int x;\n"
               "};\n"
               "struct Foo f;\n"
               "int a[sizeof(struct Foo)]\n"),
            .vars = {
                {SV("f"), SV("struct Foo")},
                {SV("a"), SV("int[4]")},
            },
        },
        // --- Brace initialization and designated initializer tests ---
        {
            "struct brace init", __LINE__,
            SV("struct S { int a; int b; };\n"
               "struct S s = {1, 2};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "struct designated init", __LINE__,
            SV("struct S { int a; int b; };\n"
               "struct S s = {.b = 2, .a = 1};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@4 = 2, @0 = 1}") },
            },
        },
        {
            "array init", __LINE__,
            SV("int arr[3] = {1, 2, 3};\n"),
            .vars = {
                { SV("arr"), SV("int[3]"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "array designated init", __LINE__,
            SV("int arr[5] = {[2] = 10, [4] = 20};\n"),
            .vars = {
                { SV("arr"), SV("int[5]"), SV("{@8 = 10, @16 = 20}") },
            },
        },
        {
            "incomplete array sizing", __LINE__,
            SV("int arr[] = {1, 2, 3};\n"),
            .vars = {
                { SV("arr"), SV("int[3]"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "nested struct init", __LINE__,
            SV("struct S { int a[2]; int b; };\n"
               "struct S s = {{1, 2}, 3};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "union init", __LINE__,
            SV("union U { int i; float f; };\n"
               "union U u = {.f = 1.5f};\n"),
            .vars = {
                { SV("u"), SV("union U"), SV("{1.5f}") },
            },
        },
        {
            "positional continuation after designation", __LINE__,
            SV("struct S { int a; int b; int c; };\n"
               "struct S s = {.b = 2, 3};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@4 = 2, @8 = 3}") },
            },
        },
        {
            "scalar brace init", __LINE__,
            SV("int x = {42};\n"),
            .vars = {
                { SV("x"), SV("int"), SV("{42}") },
            },
        },
        {
            "empty brace init", __LINE__,
            SV("struct S { int a; int b; };\n"
               "struct S s = {};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{}") },
            },
        },
        {
            "chained designator", __LINE__,
            SV("struct Inner { int x; int y; };\n"
               "struct Outer { struct Inner p; };\n"
               "struct Outer s = {.p.x = 1, .p.y = 2};\n"),
            .vars = {
                { SV("s"), SV("struct Outer"), SV("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "implicit cast in init list", __LINE__,
            SV("struct S { float f; };\n"
               "struct S s = {42};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{(float)42}") },
            },
        },
        {
            "brace elision: array of structs", __LINE__,
            SV("struct S { int x; };\n"
               "struct S foo[] = {1, {2}, 3};\n"),
            .vars = {
                { SV("foo"), SV("struct S[3]"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "extra nested braces on scalar", __LINE__,
            SV("int x[2] = {1, {{2}}};\n"),
            .vars = {
                { SV("x"), SV("int[2]"), SV("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "brace elision: multi-field struct", __LINE__,
            SV("struct P { int a; int b; };\n"
               "struct P arr[] = {1, 2, 3, 4};\n"),
            .vars = {
                { SV("arr"), SV("struct P[2]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "brace elision: nested structs", __LINE__,
            SV("struct Inner { int a; int b; };\n"
               "struct Outer { struct Inner s; int c; };\n"
               "struct Outer o = {1, 2, 3};\n"),
            .vars = {
                { SV("o"), SV("struct Outer"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        // --- Torture tests ---
        {
            "trailing comma in init list", __LINE__,
            SV("int arr[3] = {1, 2, 3,};\n"),
            .vars = {
                { SV("arr"), SV("int[3]"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "empty init for array", __LINE__,
            SV("int arr[3] = {};\n"),
            .vars = {
                { SV("arr"), SV("int[3]"), SV("{}") },
            },
        },
        {
            "empty init for union", __LINE__,
            SV("union U { int a; float b; };\n"
               "union U u = {};\n"),
            .vars = {
                { SV("u"), SV("union U"), SV("{}") },
            },
        },
        {
            "scalar with extra braces", __LINE__,
            SV("int x = {{{42}}};\n"),
            .vars = {
                { SV("x"), SV("int"), SV("{42}") },
            },
        },
        {
            "array of arrays", __LINE__,
            SV("int a[2][3] = {{1, 2, 3}, {4, 5, 6}};\n"),
            .vars = {
                { SV("a"), SV("int[2][3]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "array of arrays: brace elision", __LINE__,
            SV("int a[2][3] = {1, 2, 3, 4, 5, 6};\n"),
            .vars = {
                { SV("a"), SV("int[2][3]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "array of arrays: incomplete outer", __LINE__,
            SV("int a[][3] = {{1, 2, 3}, {4, 5, 6}};\n"),
            .vars = {
                { SV("a"), SV("int[2][3]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "array of arrays: incomplete outer + brace elision", __LINE__,
            SV("int a[][3] = {1, 2, 3, 4, 5, 6};\n"),
            .vars = {
                { SV("a"), SV("int[2][3]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "designator then positional in array", __LINE__,
            SV("int a[5] = {[3] = 30, 40};\n"),
            .vars = {
                { SV("a"), SV("int[5]"), SV("{@12 = 30, @16 = 40}") },
            },
        },
        {
            "last-write-wins in array", __LINE__,
            SV("int a[3] = {1, 2, 3, [0] = 10};\n"),
            .vars = {
                { SV("a"), SV("int[3]"), SV("{@0 = 1, @4 = 2, @8 = 3, @0 = 10}") },
            },
        },
        {
            "last-write-wins in struct", __LINE__,
            SV("struct S { int a; int b; };\n"
               "struct S s = {1, 2, .a = 99};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 1, @4 = 2, @0 = 99}") },
            },
        },
        {
            "struct with array member: brace elision", __LINE__,
            SV("struct S { int a[3]; int b; };\n"
               "struct S s = {1, 2, 3, 4};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "struct with array member: explicit braces", __LINE__,
            SV("struct S { int a[3]; int b; };\n"
               "struct S s = {{1, 2, 3}, 4};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "nested struct array brace elision", __LINE__,
            SV("struct P { int x; int y; };\n"
               "struct Line { struct P start; struct P end; };\n"
               "struct Line l = {1, 2, 3, 4};\n"),
            .vars = {
                { SV("l"), SV("struct Line"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "array of structs with designated + positional", __LINE__,
            SV("struct P { int x; int y; };\n"
               "struct P arr[3] = {[1] = {10, 20}, {30, 40}};\n"),
            .vars = {
                { SV("arr"), SV("struct P[3]"), SV("{@8 = 10, @12 = 20, @16 = 30, @20 = 40}") },
            },
        },
        {
            "struct designated init: out of order", __LINE__,
            SV("struct S { int a; int b; int c; int d; };\n"
               "struct S s = {.d = 4, .b = 2, .c = 3, .a = 1};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@12 = 4, @4 = 2, @8 = 3, @0 = 1}") },
            },
        },
        {
            "union designated second member", __LINE__,
            SV("union U { int i; float f; double d; };\n"
               "union U u = {.d = 3.14};\n"),
            .vars = {
                { SV("u"), SV("union U"), SV("{3.14}") },
            },
        },
        {
            "union first member implicit", __LINE__,
            SV("union U { int i; float f; };\n"
               "union U u = {42};\n"),
            .vars = {
                { SV("u"), SV("union U"), SV("{42}") },
            },
        },
        {
            "incomplete array of structs: brace elision", __LINE__,
            SV("struct P { int x; int y; };\n"
               "struct P arr[] = {1, 2, 3, 4, 5, 6};\n"),
            .vars = {
                { SV("arr"), SV("struct P[3]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "chained designator: array in struct", __LINE__,
            SV("struct S { int a[3]; };\n"
               "struct S s = {.a[1] = 42};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@4 = 42}") },
            },
        },
        {
            "chained designator: struct in array", __LINE__,
            SV("struct P { int x; int y; };\n"
               "struct P arr[2] = {[0].y = 5, [1].x = 10};\n"),
            .vars = {
                { SV("arr"), SV("struct P[2]"), SV("{@4 = 5, @8 = 10}") },
            },
        },
        {
            "chained designator: deep nesting", __LINE__,
            SV("struct A { int v; };\n"
               "struct B { struct A a; };\n"
               "struct C { struct B b; };\n"
               "struct C c = {.b.a.v = 99};\n"),
            .vars = {
                { SV("c"), SV("struct C"), SV("{99}") },
            },
        },
        {
            "partial struct init: fewer inits than fields", __LINE__,
            SV("struct S { int a; int b; int c; int d; };\n"
               "struct S s = {1, 2};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "partial array init: fewer inits than size", __LINE__,
            SV("int a[10] = {1};\n"),
            .vars = {
                { SV("a"), SV("int[10]"), SV("{1}") },
            },
        },
        {
            "mixed designated and positional struct init", __LINE__,
            SV("struct S { int a; int b; int c; int d; };\n"
               "struct S s = {.c = 30, 40, .a = 10};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@8 = 30, @12 = 40, @0 = 10}") },
            },
        },
        {
            "single element incomplete array", __LINE__,
            SV("int a[] = {42};\n"),
            .vars = {
                { SV("a"), SV("int[1]"), SV("{42}") },
            },
        },
        {
            "array of arrays: partial inner", __LINE__,
            SV("int a[2][3] = {{1}, {4, 5}};\n"),
            .vars = {
                { SV("a"), SV("int[2][3]"), SV("{@0 = 1, @12 = 4, @16 = 5}") },
            },
        },
        {
            "struct with nested struct: designated inner", __LINE__,
            SV("struct Inner { int x; int y; };\n"
               "struct Outer { struct Inner p; int z; };\n"
               "struct Outer o = {.p = {.y = 2, .x = 1}, .z = 3};\n"),
            .vars = {
                { SV("o"), SV("struct Outer"), SV("{@4 = 2, @0 = 1, @8 = 3}") },
            },
        },
        {
            "struct with union member", __LINE__,
            SV("union U { int i; float f; };\n"
               "struct S { union U u; int x; };\n"
               "struct S s = {{42}, 7};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 42, @4 = 7}") },
            },
        },
        {
            "struct with union: designated", __LINE__,
            SV("union U { int i; float f; };\n"
               "struct S { union U u; int x; };\n"
               "struct S s = {.u = {.f = 1.5f}, .x = 7};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 1.5f, @4 = 7}") },
            },
        },
        {
            "brace elision with union in struct", __LINE__,
            SV("union U { int i; };\n"
               "struct S { union U u; int x; };\n"
               "struct S s = {42, 7};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 42, @4 = 7}") },
            },
        },
        {
            "array: designator at end", __LINE__,
            SV("int a[5] = {1, 2, [4] = 5};\n"),
            .vars = {
                { SV("a"), SV("int[5]"), SV("{@0 = 1, @4 = 2, @16 = 5}") },
            },
        },
        {
            "array: designator jump backwards", __LINE__,
            SV("int a[5] = {[4] = 50, [1] = 10};\n"),
            .vars = {
                { SV("a"), SV("int[5]"), SV("{@16 = 50, @4 = 10}") },
            },
        },
        {
            "incomplete array: designated max index", __LINE__,
            SV("int a[] = {[9] = 99};\n"),
            .vars = {
                { SV("a"), SV("int[10]"), SV("{@36 = 99}") },
            },
        },
        {
            "3d array", __LINE__,
            SV("int a[2][2][2] = {{{1,2},{3,4}},{{5,6},{7,8}}};\n"),
            .vars = {
                { SV("a"), SV("int[2][2][2]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6, @24 = 7, @28 = 8}") },
            },
        },
        {
            "3d array: brace elision", __LINE__,
            SV("int a[2][2][2] = {1,2,3,4,5,6,7,8};\n"),
            .vars = {
                { SV("a"), SV("int[2][2][2]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6, @24 = 7, @28 = 8}") },
            },
        },
        {
            "array of structs: mixed braces", __LINE__,
            SV("struct P { int x; int y; };\n"
               "struct P arr[] = {{1, 2}, 3, 4, {5, 6}};\n"),
            .vars = {
                { SV("arr"), SV("struct P[3]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "struct with char array", __LINE__,
            SV("struct S { int n; char name[4]; };\n"
               "struct S s = {42, {'a', 'b', 'c', 0}};\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = 42, @4 = (char)97, @5 = (char)98, @6 = (char)99, @7 = (char)0}") },
            },
        },
        // --- C standard examples (6.7.9 / 6.7.10) ---
        // EXAMPLE 2
        {
            "std ex2: incomplete array", __LINE__,
            SV("int x[] = { 1, 3, 5 };\n"),
            .vars = {
                { SV("x"), SV("int[3]"), SV("{@0 = 1, @4 = 3, @8 = 5}") },
            },
        },
        // EXAMPLE 3: 2D array, braced and flat forms
        {
            "std ex3: 2D array braced", __LINE__,
            SV("int y[4][3] = {\n"
               "  { 1, 3, 5 },\n"
               "  { 2, 4, 6 },\n"
               "  { 3, 5, 7 },\n"
               "};\n"),
            .vars = {
                { SV("y"), SV("int[4][3]"), SV("{@0 = 1, @4 = 3, @8 = 5, @12 = 2, @16 = 4, @20 = 6, @24 = 3, @28 = 5, @32 = 7}") },
            },
        },
        {
            "std ex3: 2D array flat (brace elision)", __LINE__,
            SV("int y[4][3] = {\n"
               "  1, 3, 5, 2, 4, 6, 3, 5, 7\n"
               "};\n"),
            .vars = {
                { SV("y"), SV("int[4][3]"), SV("{@0 = 1, @4 = 3, @8 = 5, @12 = 2, @16 = 4, @20 = 6, @24 = 3, @28 = 5, @32 = 7}") },
            },
        },
        // EXAMPLE 4: partial inner init
        {
            "std ex4: 2D array partial inner", __LINE__,
            SV("int z[4][3] = {\n"
               "  { 1 }, { 2 }, { 3 }, { 4 }\n"
               "};\n"),
            .vars = {
                { SV("z"), SV("int[4][3]"), SV("{@0 = 1, @12 = 2, @24 = 3, @36 = 4}") },
            },
        },
        // EXAMPLE 5: struct with array member, brace elision across elements
        {
            "std ex5: struct array member elision", __LINE__,
            SV("struct W { int a[3]; int b; };\n"
               "struct W w[] = { { 1 }, 2 };\n"),
            .vars = {
                { SV("w"), SV("struct W[2]"), SV("{@0 = 1, @16 = 2}") },
            },
        },
        // EXAMPLE 6: 3D array, three equivalent forms
        {
            "std ex6: 3D array partially braced", __LINE__,
            SV("int q[4][3][2] = {\n"
               "  { 1 },\n"
               "  { 2, 3 },\n"
               "  { 4, 5, 6 }\n"
               "};\n"),
            .vars = {
                { SV("q"), SV("int[4][3][2]"), SV("{@0 = 1, @24 = 2, @28 = 3, @48 = 4, @52 = 5, @56 = 6}") },
            },
        },
        {
            "std ex6: 3D array flat (brace elision)", __LINE__,
            SV("int q[4][3][2] = {\n"
               "  1, 0, 0, 0, 0, 0,\n"
               "  2, 3, 0, 0, 0, 0,\n"
               "  4, 5, 6\n"
               "};\n"),
            .vars = {
                { SV("q"), SV("int[4][3][2]"), SV("{@0 = 1, @4 = 0, @8 = 0, @12 = 0, @16 = 0, @20 = 0, @24 = 2, @28 = 3, @32 = 0, @36 = 0, @40 = 0, @44 = 0, @48 = 4, @52 = 5, @56 = 6}") },
            },
        },
        {
            "std ex6: 3D array fully braced", __LINE__,
            SV("int q[4][3][2] = {\n"
               "  {\n"
               "    { 1 },\n"
               "  },\n"
               "  {\n"
               "    { 2, 3 },\n"
               "  },\n"
               "  {\n"
               "    { 4, 5 },\n"
               "    { 6 },\n"
               "  }\n"
               "};\n"),
            .vars = {
                { SV("q"), SV("int[4][3][2]"), SV("{@0 = 1, @24 = 2, @28 = 3, @48 = 4, @52 = 5, @56 = 6}") },
            },
        },
        // EXAMPLE 9: enum constants as designator indices
        {
            "std ex9: enum constant designators", __LINE__,
            SV("enum { member_one, member_two };\n"
               "int nm[2] = {\n"
               "  [member_two] = 20,\n"
               "  [member_one] = 10,\n"
               "};\n"),
            .vars = {
                { SV("nm"), SV("int[2]"), SV("{@4 = 20, @0 = 10}") },
            },
        },
        // EXAMPLE 10: designated struct init (like div_t)
        {
            "std ex10: designated struct", __LINE__,
            SV("struct DT { int quot; int rem; };\n"
               "struct DT answer = {.quot = 2, .rem = -1};\n"),
            .vars = {
                { SV("answer"), SV("struct DT"), SV("{@0 = 2, @4 = -1}") },
            },
        },
        // EXAMPLE 11: chained array+field designators
        {
            "std ex11: mixed chained designators", __LINE__,
            SV("struct W { int a[3]; int b; };\n"
               "struct W w[] = { [0].a = {1}, [1].a[0] = 2 };\n"),
            .vars = {
                { SV("w"), SV("struct W[2]"), SV("{@0 = 1, @16 = 2}") },
            },
        },
        // EXAMPLE 13: designator in middle of positional sequence
        {
            "std ex13: designator mid-sequence", __LINE__,
            SV("int a[10] = {\n"
               "  1, 3, 5, 7, 9, [5] = 8, 6, 4, 2, 0\n"
               "};\n"),
            .vars = {
                { SV("a"), SV("int[10]"), SV("{@0 = 1, @4 = 3, @8 = 5, @12 = 7, @16 = 9, @20 = 8, @24 = 6, @28 = 4, @32 = 2, @36 = 0}") },
            },
        },
        // EXAMPLE 15: bitfields
        {
            "std ex15: bitfield init", __LINE__,
            SV("struct BF {\n"
               "  int a:10;\n"
               "  int :12;\n"
               "  long b;\n"
               "};\n"
               "struct BF s = {1, 2};\n"),
            .vars = {
                { SV("s"), SV("struct BF"), SV("{@0:0:10 = 1, @8 = (long)2}") },
            },
        },
        {
            "bitfield: designated init", __LINE__,
            SV("struct BF { int a:10; int b:6; long c; };\n"
               "struct BF s = {.b = 3, .a = 1};\n"),
            .vars = {
                { SV("s"), SV("struct BF"), SV("{@0:10:6 = 3, @0:0:10 = 1}") },
            },
        },
        {
            "bitfield: multiple in same storage unit", __LINE__,
            SV("struct BF { int a:3; int b:5; int c:8; int d:16; };\n"
               "struct BF s = {1, 2, 3, 4};\n"),
            .vars = {
                { SV("s"), SV("struct BF"), SV("{@0:0:3 = 1, @0:3:5 = 2, @0:8:8 = 3, @0:16:16 = 4}") },
            },
        },
        {
            "bitfield: spanning storage units", __LINE__,
            SV("struct BF { int a:20; int b:20; };\n"
               "struct BF s = {1, 2};\n"),
            .vars = {
                { SV("s"), SV("struct BF"), SV("{@0:0:20 = 1, @4:0:20 = 2}") },
            },
        },
        {
            "bitfield: mixed with regular fields", __LINE__,
            SV("struct BF { int x; int a:3; int b:5; int y; };\n"
               "struct BF s = {10, 1, 2, 20};\n"),
            .vars = {
                { SV("s"), SV("struct BF"), SV("{@0 = 10, @4:0:3 = 1, @4:3:5 = 2, @8 = 20}") },
            },
        },
        {
            "bitfield: skip anonymous padding", __LINE__,
            SV("struct BF { int a:4; int :4; int b:8; };\n"
               "struct BF s = {3, 7};\n"),
            .vars = {
                { SV("s"), SV("struct BF"), SV("{@0:0:4 = 3, @0:8:8 = 7}") },
            },
        },
        {
            "bitfield: zero-width forces new unit", __LINE__,
            SV("struct BF { int a:4; int :0; int b:4; };\n"
               "struct BF s = {1, 2};\n"),
            .vars = {
                { SV("s"), SV("struct BF"), SV("{@0:0:4 = 1, @4:0:4 = 2}") },
            },
        },
        {
            "bitfield: nested struct with bitfields", __LINE__,
            SV("struct Inner { int x:3; int y:5; };\n"
               "struct Outer { int a; struct Inner b; int c; };\n"
               "struct Outer s = {1, {2, 3}, 4};\n"),
            .vars = {
                { SV("s"), SV("struct Outer"), SV("{@0 = 1, @4:0:3 = 2, @4:3:5 = 3, @8 = 4}") },
            },
        },
        {
            "bitfield: designated into nested bitfield", __LINE__,
            SV("struct Inner { int x:3; int y:5; };\n"
               "struct Outer { int a; struct Inner b; };\n"
               "struct Outer s = {.b.y = 7};\n"),
            .vars = {
                { SV("s"), SV("struct Outer"), SV("{@4:3:5 = 7}") },
            },
        },
        {
            "bitfield: single bit", __LINE__,
            SV("struct BF { int flag:1; int value:31; };\n"
               "struct BF s = {1, 100};\n"),
            .vars = {
                { SV("s"), SV("struct BF"), SV("{@0:0:1 = 1, @0:1:31 = 100}") },
            },
        },
        {
            "bitfield: unsigned", __LINE__,
            SV("struct BF { unsigned a:4; unsigned b:4; };\n"
               "struct BF s = {15, 10};\n"),
            .vars = {
                { SV("s"), SV("struct BF"), SV("{@0:0:4 = (unsigned int)15, @0:4:4 = (unsigned int)10}") },
            },
        },
        // EXAMPLE 16: anonymous union in struct (braced form)
        {
            "std ex16: anon union braced", __LINE__,
            SV("struct AU {\n"
               "  union {\n"
               "    float a;\n"
               "    int b;\n"
               "    void *p;\n"
               "  };\n"
               "  char c;\n"
               "};\n"
               "struct AU s = {{.b = 1}, 2};\n"),
            .vars = {
                { SV("s"), SV("struct AU"), SV("{@0 = 1, @8 = (char)2}") },
            },
        },
        // EXAMPLE 16: anonymous union in struct (designated form)
        {
            "std ex16: anon union designated", __LINE__,
            SV("struct AU {\n"
               "  union {\n"
               "    float a;\n"
               "    int b;\n"
               "    void *p;\n"
               "  };\n"
               "  char c;\n"
               "};\n"
               "struct AU s = {.b = 1, 2};\n"),
            .vars = {
                { SV("s"), SV("struct AU"), SV("{@0 = 1, @8 = (char)2}") },
            },
        },
        // EXAMPLE 7: typedef incomplete array + multiple declarators
        {
            "std ex7: typedef incomplete array", __LINE__,
            SV("typedef int A[];\n"
               "A a = { 1, 2 }, b = { 3, 4, 5 };\n"),
            .vars = {
                { SV("a"), SV("int[2]"), SV("{@0 = 1, @4 = 2}") },
                { SV("b"), SV("int[3]"), SV("{@0 = 3, @4 = 4, @8 = 5}") },
            },
        },
        {
            "std ex7: multiple incomplete array decls", __LINE__,
            SV("int a[] = { 1, 2 }, b[] = { 3, 4, 5 };\n"),
            .vars = {
                { SV("a"), SV("int[2]"), SV("{@0 = 1, @4 = 2}") },
                { SV("b"), SV("int[3]"), SV("{@0 = 3, @4 = 4, @8 = 5}") },
            },
        },
        // EXAMPLE 8: char array init (non-string-literal forms)
        {
            "std ex8: char array brace init", __LINE__,
            SV("char s[] = { 'a', 'b', 'c', '\\0' },\n"
               "     t[] = { 'a', 'b', 'c' };\n"),
            .vars = {
                { SV("s"), SV("char[4]"), SV("{@0 = (char)97, @1 = (char)98, @2 = (char)99, @3 = (char)0}") },
                { SV("t"), SV("char[3]"), SV("{@0 = (char)97, @1 = (char)98, @2 = (char)99}") },
            },
        },
        {
            "std ex8: char pointer from string", __LINE__,
            SV("char *p = \"abc\";\n"),
            .vars = {
                { SV("p"), SV("char *"), SV("(char *)\"abc\"") },
            },
        },
        {
            "std ex8: string literal array init", __LINE__,
            SV("char s[] = \"abc\";\n"),
            .vars = {
                { SV("s"), SV("char[4]"), SV("\"abc\"") },
            },
        },
        {
            "std ex8: string literal sized array", __LINE__,
            SV("char t[3] = \"abc\";\n"),
            .vars = {
                { SV("t"), SV("char[3]"), SV("\"abc\"") },
            },
        },
        {
            "std ex8: string literal multiple decls", __LINE__,
            SV("char s[] = \"abc\", t[3] = \"abc\";\n"),
            .vars = {
                { SV("s"), SV("char[4]"), SV("\"abc\"") },
                { SV("t"), SV("char[3]"), SV("\"abc\"") },
            },
        },
        {
            "string literal in struct char array field", __LINE__,
            SV("struct S { short temp; char pair[201]; };\n"
               "struct S s = { 0, \"abc\" };\n"),
            .vars = {
                { SV("s"), SV("struct S"), SV("{@0 = (short)0, @2 = \"abc\"}") },
            },
        },
        {
            "positional struct var in init", __LINE__,
            SV("struct T { int a; int b; };\n"
               "struct S { int x; struct T t; };\n"
               "struct T v = {10, 20};\n"
               "struct S s = { 1, v };\n"),
            .vars = {
                { SV("v"), SV("struct T"), SV("{@0 = 10, @4 = 20}") },
                { SV("s"), SV("struct S"), SV("{@0 = 1, @4 = v}") },
            },
        },
        {
            "compound literal in array init", __LINE__,
            SV("struct SV { unsigned long length; const char *text; };\n"
               "struct SV arr[] = { (struct SV){3, \"abc\"}, (struct SV){2, \"de\"} };\n"),
            .vars = {
                { SV("arr"), SV("struct SV[2]") },
            },
        },
        {
            "parenthesized compound literal in array init", __LINE__,
            SV("struct SV { unsigned long length; const char *text; };\n"
               "struct SV arr[] = { ((struct SV){3, \"abc\"}), ((struct SV){2, \"de\"}) };\n"),
            .vars = {
                { SV("arr"), SV("struct SV[2]") },
            },
        },
        {
            "compound literal in struct init", __LINE__,
            SV("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer o = { 1, (struct Inner){2, 3} };\n"),
            .vars = {
                { SV("o"), SV("struct Outer"), SV("{@0 = 1, @4 = (struct Inner){@0 = 2, @4 = 3}}") },
            },
        },
        {
            "struct var as first aggregate field", __LINE__,
            SV("struct Inner { int a; int b; };\n"
               "struct Outer { struct Inner inner; int c; };\n"
               "struct Inner v = {10, 20};\n"
               "struct Outer o = { v, 3 };\n"),
            .vars = {
                { SV("v"), SV("struct Inner"), SV("{@0 = 10, @4 = 20}") },
                { SV("o"), SV("struct Outer"), SV("{@0 = v, @8 = 3}") },
            },
        },
        {
            "deeply nested brace elision", __LINE__,
            SV("struct A { int x; };\n"
               "struct B { struct A a; int y; };\n"
               "struct C { struct B b; int z; };\n"
               "struct C c = { 1, 2, 3 };\n"),
            .vars = {
                { SV("c"), SV("struct C"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "brace elision with nested struct", __LINE__,
            SV("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer o = { 1, 2, 3 };\n"),
            .vars = {
                { SV("o"), SV("struct Outer"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "braced nested struct init", __LINE__,
            SV("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer o = { 1, {2, 3} };\n"),
            .vars = {
                { SV("o"), SV("struct Outer"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        // EXAMPLE 12: variable reference in initializer + last-write-wins
        {
            "std ex12: struct init with designated", __LINE__,
            SV("struct T { int k; int l; };\n"
               "struct T x = {.l = 43, .k = 42};\n"),
            .vars = {
                { SV("x"), SV("struct T"), SV("{@4 = 43, @0 = 42}") },
            },
        },
        {
            "std ex12: nested struct init with var ref", __LINE__,
            SV("struct T { int k; int l; };\n"
               "struct S { int i; struct T t; };\n"
               "struct T x = {.l = 43, .k = 42};\n"
               "struct S l = { 1, .t = x, .t.l = 41};\n"),
            .vars = {
                { SV("x"), SV("struct T"), SV("{@4 = 43, @0 = 42}") },
                { SV("l"), SV("struct S"), SV("{@0 = 1, @4 = x, @8 = 41}") },
            },
        },
        // EXAMPLE 14: union designated init
        {
            "std ex14: union designated", __LINE__,
            SV("union U { int x; float y; };\n"
               "union U u = {.x = 42};\n"),
            .vars = {
                { SV("u"), SV("union U"), SV("{42}") },
            },
        },
            // --- Method slot skipping tests ---
        {
            "method skipping: positional init", __LINE__,
            SV("struct Foo {\n"
               "  int x;\n"
               "  int get_x(struct Foo* self){ return self->x; }\n"
               "  int y;\n"
               "};\n"
               "struct Foo f = {1, 2};\n"),
            .vars = {
                { SV("f"), SV("struct Foo"), SV("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "method skipping: designated init", __LINE__,
            SV("struct Foo {\n"
               "  int x;\n"
               "  int get_x(struct Foo* self){ return self->x; }\n"
               "  int y;\n"
               "};\n"
               "struct Foo f = {.y = 10, .x = 20};\n"),
            .vars = {
                { SV("f"), SV("struct Foo"), SV("{@4 = 10, @0 = 20}") },
            },
        },
        {
            "method skipping: brace elision in array", __LINE__,
            SV("struct Foo {\n"
               "  int x;\n"
               "  int get_x(struct Foo* self){ return self->x; }\n"
               "  int y;\n"
               "};\n"
               "struct Foo arr[] = {1, 2, 3, 4};\n"),
            .vars = {
                { SV("arr"), SV("struct Foo[2]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "methods at start and end", __LINE__,
            SV("struct Bar {\n"
               "  void init(struct Bar* self){}\n"
               "  int a;\n"
               "  int b;\n"
               "  void deinit(struct Bar* self){}\n"
               "};\n"
               "struct Bar b = {10, 20};\n"),
            .vars = {
                { SV("b"), SV("struct Bar"), SV("{@0 = 10, @4 = 20}") },
            },
        },
        // --- Plan9 struct tests ---
        {
            "plan9: positional init", __LINE__,
            SV("struct Base { int x; int y; };\n"
               "struct Derived { struct Base; int z; };\n"
               "struct Derived d = {{1, 2}, 3};\n"),
            .vars = {
                { SV("d"), SV("struct Derived"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9: brace elision", __LINE__,
            SV("struct Base { int x; int y; };\n"
               "struct Derived { struct Base; int z; };\n"
               "struct Derived d = {1, 2, 3};\n"),
            .vars = {
                { SV("d"), SV("struct Derived"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9: designated through embed", __LINE__,
            SV("struct Base { int x; int y; };\n"
               "struct Derived { struct Base; int z; };\n"
               "struct Derived d = {.x = 1, .y = 2, .z = 3};\n"),
            .vars = {
                { SV("d"), SV("struct Derived"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9: designated embed as whole", __LINE__,
            SV("struct Base { int x; int y; };\n"
               "struct Derived { struct Base; int z; };\n"
               "struct Derived d = {{.y = 9, .x = 8}, 7};\n"),
            .vars = {
                { SV("d"), SV("struct Derived"), SV("{@4 = 9, @0 = 8, @8 = 7}") },
            },
        },
        {
            "plan9: array of derived with brace elision", __LINE__,
            SV("struct Base { int x; };\n"
               "struct Derived { struct Base; int y; };\n"
               "struct Derived arr[] = {1, 2, 3, 4};\n"),
            .vars = {
                { SV("arr"), SV("struct Derived[2]"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "plan9: nested embeds", __LINE__,
            SV("struct A { int a; };\n"
               "struct B { struct A; int b; };\n"
               "struct C { struct B; int c; };\n"
               "struct C val = {1, 2, 3};\n"),
            .vars = {
                { SV("val"), SV("struct C"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9: designated through nested embed", __LINE__,
            SV("struct A { int a; };\n"
               "struct B { struct A; int b; };\n"
               "struct C { struct B; int c; };\n"
               "struct C val = {.a = 1, .b = 2, .c = 3};\n"),
            .vars = {
                { SV("val"), SV("struct C"), SV("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9 + methods combined", __LINE__,
            SV("struct Base {\n"
               "  int x;\n"
               "  int get(struct Base* self){ return self->x; }\n"
               "};\n"
               "struct Derived { struct Base; int y; };\n"
               "struct Derived d = {1, 2};\n"),
            .vars = {
                { SV("d"), SV("struct Derived"), SV("{@0 = 1, @4 = 2}") },
            },
        },
        // EXAMPLE 15 union: anonymous bitfield padding in union
        {
            "std ex15: union with anon bitfield", __LINE__,
            SV("union UB {\n"
               "  int :16;\n"
               "  char c;\n"
               "};\n"
               "union UB u = {3};\n"),
            .vars = {
                { SV("u"), SV("union UB"), SV("{(char)3}") },
            },
        },
        // --- Vector type tests ---
        {
            "vector init: full", __LINE__,
            SV("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {1, 2, 3, 4};\n"),
            .vars = {
                { SV("v"), SV("int __attribute__((vector_size(16)))"), SV("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "vector init: partial", __LINE__,
            SV("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {1, 2};\n"),
            .vars = {
                { SV("v"), SV("int __attribute__((vector_size(16)))"), SV("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "vector init: empty", __LINE__,
            SV("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {};\n"),
            .vars = {
                { SV("v"), SV("int __attribute__((vector_size(16)))"), SV("{}") },
            },
        },
        {
            "vector init: float elements", __LINE__,
            SV("typedef float v2f __attribute__((vector_size(8)));\n"
               "v2f v = {1.0f, 2.0f};\n"),
            .vars = {
                { SV("v"), SV("float __attribute__((vector_size(8)))"), SV("{@0 = 1f, @4 = 2f}") },
            },
        },
        {
            "vector: attribute in specifier position", __LINE__,
            SV("__attribute__((vector_size(16))) int v = {10, 20, 30, 40};\n"),
            .vars = {
                { SV("v"), SV("int __attribute__((vector_size(16)))"), SV("{@0 = 10, @4 = 20, @8 = 30, @12 = 40}") },
            },
        },
        {
            "vector: trailing attribute position", __LINE__,
            SV("int v __attribute__((vector_size(16))) = {10, 20, 30, 40};\n"),
            .vars = {
                { SV("v"), SV("int __attribute__((vector_size(16)))"), SV("{@0 = 10, @4 = 20, @8 = 30, @12 = 40}") },
            },
        },
        {
            "vector: sizeof", __LINE__,
            SV("typedef int v4si __attribute__((vector_size(16)));\n"
               "int a[sizeof(v4si)];\n"),
            .vars = {
                { SV("a"), SV("int[16]") },
            },
        },
        {
            "vector: alignof", __LINE__,
            SV("typedef int v4si __attribute__((vector_size(16)));\n"
               "int a[_Alignof(v4si)];\n"),
            .vars = {
                { SV("a"), SV("int[16]") },
            },
        },
        {
            "vector: sizeof float", __LINE__,
            SV("typedef float v2f __attribute__((vector_size(8)));\n"
               "int a[sizeof(v2f)];\n"),
            .vars = {
                { SV("a"), SV("int[8]") },
            },
        },
        {
            "vector: alignof capped at max_align", __LINE__,
            SV("typedef int v8si __attribute__((vector_size(32)));\n"
               "int a[_Alignof(v8si)];\n"),
            .vars = {
                { SV("a"), SV("int[16]") },
            },
        },
        {
            "asm label on function", __LINE__,
            SV("int foo(void) __asm(\"_foo_mangled\");\n"),
            .funcs = {
                { SV("foo"), SV("int(void)"), .mangle = SV("_foo_mangled") },
            },
        },
        {
            "asm label on variable", __LINE__,
            SV("extern int x __asm__(\"_x_mangled\");\n"),
            .vars = {
                { SV("x"), SV("int"), .mangle = SV("_x_mangled") },
            },
        },
        {
            "asm label with asm keyword", __LINE__,
            SV("void bar(int a) asm(\"_bar\");\n"),
            .funcs = {
                { SV("bar"), SV("void(int)"), .mangle = SV("_bar") },
            },
        },
        {
            "trailing comma in call", __LINE__,
            SV("int f(int x, int y);\n"
               "int r = f(1, 2,);\n"),
            .funcs = {
                { SV("f"), SV("int(int, int)") },
            },
            .vars = {
                { SV("r"), SV("int"), .init = SV("f(1, 2)") },
            },
        },
        // --- Named argument tests ---
        {
            "named args", __LINE__,
            SV("int f(int a, int b);\n"
               "int r = f(.b = 2, .a = 1);\n"),
            .funcs = {
                { SV("f"), SV("int(int, int)") },
            },
            .vars = {
                { SV("r"), SV("int"), .init = SV("f(1, 2)") },
            },
        },
        {
            "named args mixed with positional", __LINE__,
            SV("int f(int a, int b, int c);\n"
               "int r = f(1, .c = 3, .b = 2);\n"),
            .funcs = {
                { SV("f"), SV("int(int, int, int)") },
            },
            .vars = {
                { SV("r"), SV("int"), .init = SV("f(1, 2, 3)") },
            },
        },
        {
            "positional designator args", __LINE__,
            SV("int f(int a, int b);\n"
               "int r = f([1] = 2, [0] = 1);\n"),
            .funcs = {
                { SV("f"), SV("int(int, int)") },
            },
            .vars = {
                { SV("r"), SV("int"), .init = SV("f(1, 2)") },
            },
        },
        {
            "mixed positional designator and named", __LINE__,
            SV("int f(int a, int b, int c);\n"
               "int r = f([2] = 3, .a = 1, [1] = 2);\n"),
            .funcs = {
                { SV("f"), SV("int(int, int, int)") },
            },
            .vars = {
                { SV("r"), SV("int"), .init = SV("f(1, 2, 3)") },
            },
        },
        // --- Function call implicit cast tests ---
        {
            "call: int arg to long param", __LINE__,
            SV("long f(long x);\n"
               "long r = f(42);\n"),
            .funcs = {
                { SV("f"), SV("long(long)") },
            },
            .vars = {
                { SV("r"), SV("long"), .init = SV("f((long)42)") },
            },
        },
        {
            "call: matching types, no cast", __LINE__,
            SV("int f(int x);\n"
               "int r = f(42);\n"),
            .funcs = {
                { SV("f"), SV("int(int)") },
            },
            .vars = {
                { SV("r"), SV("int"), .init = SV("f(42)") },
            },
        },
        {
            "call: variadic, extra args get default promotions", __LINE__,
            SV("int f(int x, ...);\n"
               "float g(void);\n"
               "int r = f(1, g());\n"),
            .funcs = {
                { SV("f"), SV("int(int, ...)") },
                { SV("g"), SV("float(void)") },
            },
            .vars = {
                { SV("r"), SV("int"), .init = SV("f(1, (double)g())") },
            },
        },
        {
            "call: char arg promoted to int param", __LINE__,
            SV("int f(int x);\n"
               "char c;\n"
               "int r = f(c);\n"),
            .funcs = {
                { SV("f"), SV("int(int)") },
            },
            .vars = {
                { SV("c"), SV("char") },
                { SV("r"), SV("int"), .init = SV("f((int)c)") },
            },
        },
        // --- Lambda tests ---
        {
            "lambda: basic immediate call", __LINE__,
            SV("int r = int(int x, int y){ return x + y; }(3, 4);\n"),
            .vars = {
                { SV("r"), SV("int"), .init = SV("<lambda>(3, 4)") },
            },
        },
        {
            "lambda: single param", __LINE__,
            SV("int r = int(int a){ return a * 2; }(5);\n"),
            .vars = {
                { SV("r"), SV("int"), .init = SV("<lambda>(5)") },
            },
        },
        {
            "lambda: void params", __LINE__,
            SV("int r = int(void){ return 42; }();\n"),
            .vars = {
                { SV("r"), SV("int"), .init = SV("<lambda>()") },
            },
        },
        {
            "lambda: in subexpression", __LINE__,
            SV("int r = 1 + int(int a){ return a; }(10);\n"),
            .vars = {
                { SV("r"), SV("int"), .init = SV("(1 + <lambda>(10))") },
            },
        },
        {
            "lambda: typedef return type", __LINE__,
            SV("typedef int myint;\n"
               "myint r = myint(myint x){ return x; }(7);\n"),
            .typedefs = {
                { SV("myint"), SV("int") },
            },
            .vars = {
                { SV("r"), SV("int"), .init = SV("<lambda>(7)") },
            },
        },
        {
            "lambda: implicit cast on arg", __LINE__,
            SV("long r = long(long x){ return x; }(42);\n"),
            .vars = {
                { SV("r"), SV("long"), .init = SV("<lambda>((long)42)") },
            },
        },
        {
            "offsetof: simple member", __LINE__,
            SV("struct S { int x; double y; char z; };\n"
               "unsigned long a = __builtin_offsetof(struct S, x);\n"
               "unsigned long b = __builtin_offsetof(struct S, y);\n"
               "unsigned long c = __builtin_offsetof(struct S, z);\n"),
            .vars = {
                { SV("a"), SV("unsigned long"), .init = SV("0") },
                { SV("b"), SV("unsigned long"), .init = SV("8") },
                { SV("c"), SV("unsigned long"), .init = SV("16") },
            },
        },
        {
            "offsetof: nested member", __LINE__,
            SV("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "unsigned long r = __builtin_offsetof(struct Outer, inner.b);\n"),
            .vars = {
                { SV("r"), SV("unsigned long"), .init = SV("8") },
            },
        },
        {
            "offsetof: array subscript", __LINE__,
            SV("struct S { int x; int arr[4]; };\n"
               "unsigned long r = __builtin_offsetof(struct S, arr[2]);\n"),
            .vars = {
                { SV("r"), SV("unsigned long"), .init = SV("12") },
            },
        },
        {
            "offsetof: array subscript with nested member", __LINE__,
            SV("struct Inner { char c; int val; };\n"
               "struct S { int x; struct Inner items[3]; };\n"
               "unsigned long r = __builtin_offsetof(struct S, items[1].val);\n"),
            .vars = {
                { SV("r"), SV("unsigned long"), .init = SV("16") },
            },
        },
        {
            "offsetof: union member", __LINE__,
            SV("union U { int x; double y; };\n"
               "unsigned long a = __builtin_offsetof(union U, x);\n"
               "unsigned long b = __builtin_offsetof(union U, y);\n"),
            .vars = {
                { SV("a"), SV("unsigned long"), .init = SV("0") },
                { SV("b"), SV("unsigned long"), .init = SV("0") },
            },
        },
        {
            "offsetof: anonymous union member", __LINE__,
            SV("struct S { int x; union { int a; float b;};};\n"
               "unsigned long ox = __builtin_offsetof(struct S, x);\n"
               "unsigned long oa = __builtin_offsetof(struct S, a);\n"
               "unsigned long ob = __builtin_offsetof(struct S, b);\n"),
            .vars = {
                { SV("ox"), SV("unsigned long"), .init = SV("0")},
                { SV("oa"), SV("unsigned long"), .init = SV("4")},
                { SV("ob"), SV("unsigned long"), .init = SV("4")},
            },
        },
    };
    for(size_t i = 0; i < arrlen(testcases); i++){
        if(testcases[i].skip){
            continue;
        }
        ArenaAllocator aa = {0};
        Allocator al = allocator_from_arena(&aa);
        FileCache* fc = fc_create(al);
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {.allocator = al};
        Environment env = {.allocator = al, .at=&at};
        int err;
        MStringBuilder sb = {.allocator=al};
        CcParser cc = {
            .cpp = {
                .allocator = al,
                .fc = fc,
                .at = &at,
                .logger = logger,
                .env = &env,
                .target = cc_target_test(),
            },
            .current = &cc.global,
        };
        struct Case* c = &testcases[i];
        fc_write_path(fc, "(test)", 6);
        err = fc_cache_file(fc, c->input);
        if(err) {TestPrintf("%s:%d: failed to cache", __FILE__, c->line); goto finally;}
        err = cpp_define_builtin_macros(&cc.cpp);
        if(err) {TestPrintf("%s:%d: failed to define", __FILE__, c->line); goto finally;}
        err = cc_define_builtin_types(&cc);
        if(err) {TestPrintf("%s:%d: failed to define builtin types", __FILE__, c->line); goto finally;}
        err = cpp_include_file_via_file_cache(&cc.cpp, SV("(test)"));
        if(err) {TestPrintf("%s:%d: failed to include", __FILE__, c->line); goto finally;}
        err = cc_parse_all(&cc);
        if(err) {TestPrintf("%s:%d: failed to parse", __FILE__, c->line); goto finally;}
        for(size_t n = 0; n < N; n++){
            StringView name = c->vars[n].name;
            if(!name.length) break;
            Atom a = AT_get_atom(&at, name.text, name.length);
            if(!a) {err = 1; goto finally;}
            CcVariable* var = cc_scope_lookup_var(&cc.global, a, CC_SCOPE_NO_WALK);
            TEST_stats.executed++;
            if(!var){
                TEST_stats.failures++;
                TestPrintf("%s:%d: %s %.*s is undefined\n", __FILE__, c->line, c->test, sv_p(name));
                continue;
            }
            msb_reset(&sb);
            cc_print_type(&sb, var->type);
            if(sb.errored) { err = 1; TestPrintf("%s:%d: allocation failure", __FILE__, c->line); goto finally; }
            StringView r = msb_borrow_sv(&sb);
            test_expect_equals_sv(c->vars[n].repr, r, "expected", "actual", &TEST_stats, __FILE__, __func__, c->line);
            if(c->vars[n].init.length){
                TestExpectTrue(var->initializer);
                if(var->initializer){
                    msb_reset(&sb);
                    CcExpr* init = var->initializer;
                    cc_print_expr(&sb, init);
                    if(sb.errored) { err = 1; TestReport("allocation failure"); goto finally; }
                    StringView ir = msb_borrow_sv(&sb);
                    test_expect_equals_sv(c->vars[n].init, ir, "expected init", "actual init", &TEST_stats, __FILE__, __func__, c->line);
                }
            }
            if(c->vars[n].mangle.length){
                TestExpectTrue(var->mangle);
                if(var->mangle){
                    StringView mr = {var->mangle->length, var->mangle->data};
                    test_expect_equals_sv(c->vars[n].mangle, mr, "expected mangle", "actual mangle", &TEST_stats, __FILE__, __func__, c->line);
                }
            }
        }
        for(size_t n = 0; n < N; n++){
            StringView name = c->funcs[n].name;
            if(!name.length) break;
            Atom a = AT_get_atom(&at, name.text, name.length);
            if(!a) {err = 1; goto finally;}
            CcFunc* func = cc_scope_lookup_func(&cc.global, a, CC_SCOPE_NO_WALK);
            TestExpectTrue(func);
            if(!func){
                TestPrintf("%s:%d: %s %.*s is undefined\n", __FILE__, c->line, c->test, sv_p(name));
                continue;
            }
            msb_reset(&sb);
            cc_print_type(&sb, (CcQualType){.bits=(uintptr_t)func->type});
            if(sb.errored) { err = 1; TestReport("allocation failure"); goto finally; }
            StringView r = msb_borrow_sv(&sb);
            test_expect_equals_sv(c->funcs[n].repr, r, "expected", "actual", &TEST_stats, __FILE__, __func__, c->line);
            if(c->funcs[n].mangle.length){
                TestExpectTrue(func->mangle);
                if(func->mangle){
                    StringView mr = {func->mangle->length, func->mangle->data};
                    test_expect_equals_sv(c->funcs[n].mangle, mr, "expected mangle", "actual mangle", &TEST_stats, __FILE__, __func__, c->line);
                }
            }
        }
        for(size_t n = 0; n < N; n++){
            StringView name = c->typedefs[n].name;
            if(!name.length) break;
            Atom a = AT_get_atom(&at, name.text, name.length);
            if(!a) {err = 1; goto finally;}
            CcQualType t = cc_scope_lookup_typedef(&cc.global, a, CC_SCOPE_NO_WALK);
            TestExpectNotEquals(t.bits, (uintptr_t)0);
            if(!t.bits){
                TestPrintf("%s:%d: %s %.*s is undefined\n", __FILE__, c->line, c->test, sv_p(name));
                continue;
            }
            msb_reset(&sb);
            cc_print_type(&sb, t);
            if(sb.errored) { err = 1; TestReport("allocation failure"); goto finally; }
            StringView r = msb_borrow_sv(&sb);
            test_expect_equals_sv(c->typedefs[n].repr, r, "expected", "actual", &TEST_stats, __FILE__, __func__, c->line);
        }
        finally:
        if(log_sb.cursor && ! log_sb.errored){
            StringView sv = msb_borrow_sv(&log_sb);
            TestPrintf("%s:%d: %s %.*s\n", __FILE__, c->line, c->test, sv_p(sv));
        }
        TestExpectFalse(err);
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&cc.cpp.synth_arena);
        ArenaAllocator_free_all(&cc.scratch_arena);
    }
    TESTEND();
}

TestFunction(test_parse_errors){
    TESTBEGIN();
    struct ErrorCase {
        const char* test; int line;
        StringView input;
        StringView expected_msg;
    } cases[] = {
        {
            "static_assert(0) fails", __LINE__,
            SV("static_assert(0);\n"),
            SV("(test):1:1: error: static assertion failed: 0\n"),
        },
        {
            "static_assert(0, msg) fails", __LINE__,
            SV("static_assert(0, \"this should fail\");\n"),
            SV("(test):1:1: error: static assertion failed: 0: \"this should fail\"\n"),
        },
        {
            "static_assert(1-1) fails", __LINE__,
            SV("static_assert(1-1, \"zero\");\n"),
            SV("(test):1:1: error: static assertion failed: (1 - 1): \"zero\"\n"),
        },
        {
            "static_assert(sizeof(int)==8) fails", __LINE__,
            SV("static_assert(sizeof(int) == 8, \"int is not 8\");\n"),
            SV("(test):1:1: error: static assertion failed: (4 == (unsigned long)8): \"int is not 8\"\n"),
        },
        {
            "FAM in middle of struct", __LINE__,
            SV("struct Bad { int data[]; int x; };\n"),
            SV("(test):1:24: error: flexible array member must be last field\n"),
        },
        {
            "FAM embedded in struct", __LINE__,
            SV("struct Inner { int n; char data[]; };\n"
               "struct Outer { struct Inner i; int x; };\n"),
            SV("(test):2:30: error: struct with flexible array member cannot be embedded\n"),
        },
        {
            "FAM anon struct not at end", __LINE__,
            SV("struct S { struct { int data[]; }; int x; };\n"),
            SV("(test):1:34: error: struct with flexible array member cannot be embedded\n"),
        },
        {
            "FAM named struct at end", __LINE__,
            SV("struct Inner { int n; char data[]; };\n"
               "struct Outer { int x; struct Inner i; };\n"),
            SV("(test):2:37: error: struct with flexible array member cannot be embedded\n"),
        },
        {
            "Duplicate field", __LINE__,
            SV("struct S { int x; int x;};\n"),
            SV("(test):1:24: error: duplicate member 'x'\n"),
        },
        {
            "Duplicate field inside anon", __LINE__,
            SV("struct S { int x; struct {int x;}; };\n"),
            SV("(test):1:34: error: duplicate member 'x'\n"),
        },
        {
            "Duplicate field inside nested anon", __LINE__,
            SV("struct S { int x; struct { struct {int x;}; int y; }; };\n"),
            SV("(test):1:53: error: duplicate member 'x'\n"),
        },
        {
            "Duplicate field inside separate nested anon", __LINE__,
            SV("struct S { struct {int x;}; struct { struct {int x;}; int y; }; };\n"),
            SV("(test):1:63: error: duplicate member 'x'\n"),
        },
        {
            "bitfield width exceeds type (int)", __LINE__,
            SV("struct S { int x : 33; };\n"),
            SV("(test):1:18: error: bitfield width (33) exceeds size of type (32 bits)\n"),
        },
        {
            "bitfield width exceeds type (char)", __LINE__,
            SV("struct S { char x : 9; };\n"),
            SV("(test):1:19: error: bitfield width (9) exceeds size of type (8 bits)\n"),
        },
        {
            "named bitfield zero width", __LINE__,
            SV("struct S { int x : 0; };\n"),
            SV("(test):1:18: error: named bitfield 'x' cannot have zero width\n"),
        },
        {
            "anonymous bitfield width exceeds type", __LINE__,
            SV("struct S { int : 33; };\n"),
            SV("(test):1:16: error: bitfield width (33) exceeds size of type (32 bits)\n"),
        },
        {
            "float bitfield", __LINE__,
            SV("struct S { float x : 3; };\n"),
            SV("(test):1:20: error: bitfield must have integer or enum type\n"),
        },
        {
            "struct bitfield", __LINE__,
            SV("struct A { int x; };\nstruct S { struct A a : 3; };\n"),
            SV("(test):2:23: error: bitfield must have integer or enum type\n"),
        },
        {
            "anonymous float bitfield", __LINE__,
            SV("struct S { float : 3; };\n"),
            SV("(test):1:18: error: bitfield must have integer or enum type\n"),
        },
        {
            "typedef method body", __LINE__,
            SV("typedef int fn_t(int);\n"
               "struct S { fn_t foo { return 1; } };\n"),
            SV("(test):2:21: error: cannot define method with typedef function type\n"),
        },
        {
            "typedef without type", __LINE__,
            SV("typedef foo bar;\n"),
            SV("(test):1:9: error: typedef requires a type\n"),
        },
        {
            "typedef in struct member", __LINE__,
            SV("struct S { typedef int x; };\n"),
            SV("(test):1:1: error: Storage class specifiers not allowed in struct/union members\n"),
        },
        {
            "typedef without type in struct", __LINE__,
            SV("struct S { typedef foo; };\n"),
            SV("(test):1:1: error: Storage class specifiers not allowed in struct/union members\n"),
        },
        {
            "missing type in struct member", __LINE__,
            SV("struct S { 123; };\n"),
            SV("(test):1:12: error: Expected type specifier in struct/union member\n"),
        },
        {
            "typedef in function parameter", __LINE__,
            SV("void f(typedef int x);\n"),
            SV("(test):1:8: error: typedef not allowed in function parameter\n"),
        },
        {
            "missing type in function parameter", __LINE__,
            SV("void f(123);\n"),
            SV("(test):1:8: error: Expected type specifier in function parameter\n"),
        },
        {
            "missing type in enum underlying type", __LINE__,
            SV("enum E : { A };\n"),
            SV("(test):1:1: error: Expected type specifier for enum underlying type\n"),
        },
        // --- Init list error tests ---
        {
            "excess elements in scalar init", __LINE__,
            SV("int x = {1, 2};\n"),
            SV("(test):1:13: error: excess elements in scalar initializer\n"),
        },
        {
            "designator in scalar init", __LINE__,
            SV("int x = {.a = 1};\n"),
            SV("(test):1:10: error: designators not allowed in scalar initializer\n"),
        },
        {
            "excess elements in struct init", __LINE__,
            SV("struct S { int a; };\n"
               "struct S s = {1, 2};\n"),
            SV("(test):2:18: error: excess elements in struct initializer\n"),
        },
        {
            "unknown field in designated init", __LINE__,
            SV("struct S { int a; };\n"
               "struct S s = {.z = 1};\n"),
            SV("(test):2:15: error: no member named 'z'\n"),
        },
        {
            "array designator in struct init", __LINE__,
            SV("struct S { int a; };\n"
               "struct S s = {[0] = 1};\n"),
            SV("(test):2:15: error: array designator in struct initializer\n"),
        },
        {
            "field designator in array init", __LINE__,
            SV("int arr[3] = {.x = 1};\n"),
            SV("(test):1:15: error: field designator in array initializer\n"),
        },
        {
            "array index out of bounds", __LINE__,
            SV("int arr[3] = {[5] = 1};\n"),
            SV("(test):1:15: error: array index 5 out of bounds (size 3)\n"),
        },
        // --- More init list error torture tests ---
        {
            "excess elements in array init", __LINE__,
            SV("int arr[2] = {1, 2, 3};\n"),
            SV("(test):1:21: error: excess elements in array initializer\n"),
        },
        {
            "excess via brace elision: array of structs", __LINE__,
            SV("struct P { int x; int y; };\n"
               "struct P arr[1] = {1, 2, 3};\n"),
            SV("(test):2:26: error: excess elements in array initializer\n"),
        },
        {
            "excess via brace elision: struct", __LINE__,
            SV("struct Inner { int a; };\n"
               "struct Outer { struct Inner s; };\n"
               "struct Outer o = {1, 2};\n"),
            SV("(test):3:22: error: excess elements in struct initializer\n"),
        },
        {
            "field designator in union: unknown", __LINE__,
            SV("union U { int a; float b; };\n"
               "union U u = {.c = 1};\n"),
            SV("(test):2:14: error: no member named 'c'\n"),
        },
        {
            "chained designator: field into scalar", __LINE__,
            SV("struct S { int a; };\n"
               "struct S s = {.a.b = 1};\n"),
            SV("(test):2:17: error: member designator into non-struct/union type\n"),
        },
        {
            "chained designator: index into non-array", __LINE__,
            SV("struct S { int a; };\n"
               "struct S s = {.a[0] = 1};\n"),
            SV("(test):2:17: error: index designator into non-array type\n"),
        },
        {
            "chained designator: unknown nested field", __LINE__,
            SV("struct Inner { int x; };\n"
               "struct Outer { struct Inner p; };\n"
               "struct Outer o = {.p.z = 1};\n"),
            SV("(test):3:21: error: no member named 'z'\n"),
        },
        {
            "incomplete struct init", __LINE__,
            SV("struct S;\n"
               "struct S s = {1};\n"),
            SV("(test):2:14: error: initializer for incomplete struct type\n"),
        },
        {
            "incomplete union init", __LINE__,
            SV("union U;\n"
               "union U u = {1};\n"),
            SV("(test):2:13: error: initializer for incomplete union type\n"),
        },
        {
            "init list for function type", __LINE__,
            SV("typedef void fn(void);\n"
               "fn f = {1};\n"),
            SV("(test):2:8: error: cannot initialize type with initializer list\n"),
        },
        {
            "unterminated union init", __LINE__,
            SV("union U { int a; float b; };\n"
               "union U u = {\n"),
            SV("(test):2:13: error: unterminated initializer list\n"),
        },
        {
            "excess elements in braced scalar", __LINE__,
            SV("int x = {{1, 2}};\n"),
            SV("(test):1:14: error: excess elements in scalar initializer\n"),
        },
        {
            "chained designator: array index out of bounds", __LINE__,
            SV("struct S { int arr[3]; };\n"
               "struct S s = {.arr[5] = 1};\n"),
            SV("(test):2:19: error: array index 5 out of bounds (size 3)\n"),
        },
        {
            "negative array designator", __LINE__,
            SV("int arr[3] = {[-1] = 1};\n"),
            SV("(test):1:15: error: array designator value out of range\n"),
        },
        {
            "negative chained array designator", __LINE__,
            SV("struct S { int arr[3]; };\n"
               "struct S s = {.arr[-1] = 1};\n"),
            SV("(test):2:19: error: array designator value out of range\n"),
        },
        {
            "vector_size on non-scalar type", __LINE__,
            SV("struct S { int a; };\n"
               "typedef struct S v4s __attribute__((vector_size(16)));\n"),
            SV("(test):2:1: error: vector_size attribute requires a scalar type\n"),
        },
        {
            "vector_size not power of 2", __LINE__,
            SV("typedef int v __attribute__((vector_size(7)));\n"),
            SV("(test):1:30: error: vector_size must be a power of 2\n"),
        },
        {
            "vector_size zero", __LINE__,
            SV("typedef int v __attribute__((vector_size(0)));\n"),
            SV("(test):1:30: error: vector_size must be a power of 2\n"),
        },
        {
            "vector_size smaller than element", __LINE__,
            SV("typedef int v __attribute__((vector_size(2)));\n"),
            SV("(test):1:1: error: vector_size is smaller than the element type\n"),
        },
        {
            "aligned on typedef", __LINE__,
            SV("typedef int aligned_int __attribute__((aligned(16)));\n"),
            SV("(test):1:1: error: aligned attribute on non-struct/union type is not supported\n"),
        },
        {
            "vector init: excess elements", __LINE__,
            SV("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {1, 2, 3, 4, 5};\n"),
            SV("(test):2:23: error: excess elements in vector initializer\n"),
        },
        {
            "vector init: designator not allowed", __LINE__,
            SV("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {[0] = 1};\n"),
            SV("(test):2:11: error: designators not allowed in vector initializer\n"),
        },
        {
            "packed on non-struct", __LINE__,
            SV("typedef int packed_int __attribute__((packed));\n"),
            SV("(test):1:1: error: packed attribute on non-struct type is not supported\n"),
        },
        {
            "transparent_union on non-union", __LINE__,
            SV("typedef int tu __attribute__((transparent_union));\n"),
            SV("(test):1:1: error: transparent_union attribute on non-union type is not supported\n"),
        },
        // --- Function call arg errors ---
        {
            "too few args", __LINE__,
            SV("void f(int a, int b);\n"
               "int x = f(1);\n"),
            SV("(test):2:10: error: Expected 2 arguments, got 1\n"),
        },
        {
            "too many args", __LINE__,
            SV("void f(int a);\n"
               "int x = f(1, 2);\n"),
            SV("(test):2:10: error: Expected 1 arguments, got 2\n"),
        },
        {
            "zero args to non-void function", __LINE__,
            SV("int f(int a);\n"
               "int x = f();\n"),
            SV("(test):2:10: error: Expected 1 arguments, got 0\n"),
        },
        {
            "too few args to variadic", __LINE__,
            SV("int printf(const char *fmt, ...);\n"
               "int x = printf();\n"),
            SV("(test):2:15: error: Too few arguments: expected at least 1, got 0\n"),
        },
        {
            "call non-function", __LINE__,
            SV("int x;\n"
               "int y = x(1);\n"),
            SV("(test):2:10: error: Called object is not a function or function pointer\n"),
        },
        // --- Incompatible type errors ---
        {
            "call: struct arg to int param", __LINE__,
            SV("struct S { int x; };\n"
               "void f(int a);\n"
               "struct S s;\n"
               "int x = f(s);\n"),
            SV("(test):4:11: error: cannot implicitly convert from 'struct S' to 'int'\n"),
        },
        {
            "call: struct arg to float param", __LINE__,
            SV("struct S { int x; };\n"
               "int f(float a);\n"
               "struct S s;\n"
               "int x = f(s);\n"),
            SV("(test):4:11: error: cannot implicitly convert from 'struct S' to 'float'\n"),
        },
        {
            "call: pointer arg to struct param", __LINE__,
            SV("struct S { int x; };\n"
               "void f(struct S s);\n"
               "int *p;\n"
               "int x = f(p);\n"),
            SV("(test):4:11: error: cannot implicitly convert from 'int *' to 'struct S'\n"),
        },
        // --- Assignment implicit conversion errors ---
        {
            "assign struct to int", __LINE__,
            SV("struct S { int x; };\n"
               "struct S s;\n"
               "int x = s;\n"),
            SV("(test):3:9: error: cannot implicitly convert from 'struct S' to 'int'\n"),
        },
        {
            "assign int to struct", __LINE__,
            SV("struct S { int x; };\n"
               "struct S s = 42;\n"),
            SV("(test):2:14: error: cannot implicitly convert from 'int' to 'struct S'\n"),
        },
        {
            "assign non-zero int to pointer", __LINE__,
            SV("int *p = 1;\n"),
            SV("(test):1:10: error: cannot implicitly convert from 'int' to 'int *'\n"),
        },
        {
            "assign pointer to int", __LINE__,
            SV("int *p;\n"
               "int x = p;\n"),
            SV("(test):2:9: error: cannot implicitly convert from 'int *' to 'int'\n"),
        },
        {
            "const int* to int* (drops const)", __LINE__,
            SV("const int *cip;\n"
               "int *ip = cip;\n"),
            SV("(test):2:11: error: cannot implicitly convert from 'const int *' to 'int *'\n"),
        },
        {
            "assign to const variable", __LINE__,
            SV("const int x = 0;\n"
               "x = 1;\n"),
            SV("(test):2:3: error: cannot assign to variable with const-qualified type\n"),
        },
        {
            "incompatible struct types", __LINE__,
            SV("struct A { int x; };\n"
               "struct B { int x; };\n"
               "struct A a;\n"
               "struct B b = a;\n"),
            SV("(test):4:14: error: cannot implicitly convert from 'struct A' to 'struct B'\n"),
        },
        // --- Lambda error tests ---
        {
            "lambda: non-function type", __LINE__,
            SV("int r = int{1};\n"),
            SV("(test):1:9: error: Lambda requires a function type, got non-function type\n"),
        },
        {
            "lambda: missing body", __LINE__,
            SV("int r = int(int x);\n"),
            SV("(test):1:9: error: Expected '{' for lambda body\n"),
        },
    };
    for(size_t i = 0; i < arrlen(cases); i++){
        ArenaAllocator aa = {0};
        Allocator al = allocator_from_arena(&aa);
        FileCache* fc = fc_create(al);
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {.allocator = al};
        Environment env = {.allocator = al, .at=&at};
        CcParser cc = {
            .cpp = {
                .allocator = al,
                .fc = fc,
                .at = &at,
                .logger = logger,
                .env = &env,
                .target = cc_target_test(),
            },
            .current = &cc.global,
        };
        struct ErrorCase* c = &cases[i];
        fc_write_path(fc, "(test)", 6);
        int err = fc_cache_file(fc, c->input);
        if(err) {TestPrintf("%s:%d: failed to cache", __FILE__, c->line); goto fin;}
        err = cpp_define_builtin_macros(&cc.cpp);
        if(err) {TestPrintf("%s:%d: failed to define", __FILE__, c->line); goto fin;}
        err = cc_define_builtin_types(&cc);
        if(err) {TestPrintf("%s:%d: failed to define builtin types", __FILE__, c->line); goto fin;}
        err = cpp_include_file_via_file_cache(&cc.cpp, SV("(test)"));
        if(err) {TestPrintf("%s:%d: failed to include", __FILE__, c->line); goto fin;}
        err = cc_parse_all(&cc);
        TEST_stats.executed++;
        if(!err){
            TEST_stats.failures++;
            TestPrintf("%s:%d: %s: expected error but parsing succeeded\n", __FILE__, c->line, c->test);
        }
        if(c->expected_msg.length){
            StringView log = msb_borrow_sv(&log_sb);
            test_expect_equals_sv(c->expected_msg, log, "expected error", "actual error", &TEST_stats, __FILE__, __func__, c->line);
        }
        fin:
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&cc.cpp.synth_arena);
        ArenaAllocator_free_all(&cc.scratch_arena);
    }
    TESTEND();
}

TestFunction(test_struct_layout){
    TESTBEGIN();
    enum { MAXFIELDS = 16 };
    struct FieldExpect {
        StringView name; // empty = end sentinel
        uint32_t offset;
        uint32_t bitwidth;  // 0 = not a bitfield
        uint32_t bitoffset;
        StringView type_repr; // empty = don't check
    };
    struct StructCase {
        const char* test; int line;
        StringView input;
        StringView tag; // struct/union tag to look up
        _Bool is_union;
        uint32_t size;
        uint32_t alignment;
        struct FieldExpect fields[MAXFIELDS];
    } cases[] = {
        {
            "simple struct layout", __LINE__,
            SV("struct S { int x; char y; };\n"),
            SV("S"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("x"), .offset = 0 },
                { SV("y"), .offset = 4 },
            },
        },
        {
            "struct with padding", __LINE__,
            SV("struct P { char a; int b; char c; };\n"),
            SV("P"), 0,
            .size = 12, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 4 },
                { SV("c"), .offset = 8 },
            },
        },
        {
            "struct double alignment", __LINE__,
            SV("struct D { char a; double b; };\n"),
            SV("D"), 0,
            .size = 16, .alignment = 8,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 8 },
            },
        },
        {
            "packed struct", __LINE__,
            SV("struct __attribute__((packed)) Pk { char a; int b; char c; };\n"),
            SV("Pk"), 0,
            .size = 6, .alignment = 1,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 1 },
                { SV("c"), .offset = 5 },
            },
        },
        {
            "packed struct double", __LINE__,
            SV("struct __attribute__((packed)) PkD { char a; double b; };\n"),
            SV("PkD"), 0,
            .size = 9, .alignment = 1,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 1 },
            },
        },
        {
            "aligned struct", __LINE__,
            SV("struct __attribute__((aligned(16))) Al { int x; };\n"),
            SV("Al"), 0,
            .size = 16, .alignment = 16,
            .fields = {
                { SV("x"), .offset = 0 },
            },
        },
        {
            "bitfield packing", __LINE__,
            SV("struct BF { int a : 3; int b : 5; int c : 8; };\n"),
            SV("BF"), 0,
            .size = 4, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SV("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
                { SV("c"), .offset = 0, .bitwidth = 8, .bitoffset = 8 },
            },
        },
        {
            "bitfield overflow to next unit", __LINE__,
            SV("struct BF2 { int a : 30; int b : 5; };\n"),
            SV("BF2"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 30, .bitoffset = 0 },
                { SV("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "bitfield mixed with regular", __LINE__,
            SV("struct BF3 { int a : 3; int x; int b : 5; };\n"),
            SV("BF3"), 0,
            .size = 12, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SV("x"), .offset = 4 },
                { SV("b"), .offset = 8, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "simple union", __LINE__,
            SV("union U { int i; double d; char c; };\n"),
            SV("U"), 1,
            .size = 8, .alignment = 8,
            .fields = {
                { SV("i"), .offset = 0 },
                { SV("d"), .offset = 0 },
                { SV("c"), .offset = 0 },
            },
        },
        {
            "union size is max", __LINE__,
            SV("union U2 { char a; short b; int c; long d; };\n"),
            SV("U2"), 1,
            .size = 8, .alignment = 8,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 0 },
                { SV("c"), .offset = 0 },
                { SV("d"), .offset = 0 },
            },
        },
        {
            "nested struct offsets", __LINE__,
            SV("struct Inner { int a; int b; };\n"
               "struct Outer { char c; struct Inner s; int d; };\n"),
            SV("Outer"), 0,
            .size = 16, .alignment = 4,
            .fields = {
                { SV("c"), .offset = 0 },
                { SV("s"), .offset = 4, .type_repr = SV("struct Inner") },
                { SV("d"), .offset = 12 },
            },
        },
        {
            "all chars no padding", __LINE__,
            SV("struct Chars { char a; char b; char c; };\n"),
            SV("Chars"), 0,
            .size = 3, .alignment = 1,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 1 },
                { SV("c"), .offset = 2 },
            },
        },
        {
            "empty struct", __LINE__,
            SV("struct Empty {};\n"),
            SV("Empty"), 0,
            .size = 0, .alignment = 1,
        },
        {
            "struct with pointer", __LINE__,
            SV("struct WP { char a; void *p; };\n"),
            SV("WP"), 0,
            .size = 16, .alignment = 8,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("p"), .offset = 8 },
            },
        },
        {
            "struct with array", __LINE__,
            SV("struct WA { int x; char buf[7]; int y; };\n"),
            SV("WA"), 0,
            .size = 16, .alignment = 4,
            .fields = {
                { SV("x"), .offset = 0 },
                { SV("buf"), .offset = 4 },
                { SV("y"), .offset = 12 },
            },
        },
        {
            "char short int alignment", __LINE__,
            SV("struct CSI { char a; short b; int c; };\n"),
            SV("CSI"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 2 },
                { SV("c"), .offset = 4 },
            },
        },
        {
            "trailing padding", __LINE__,
            SV("struct TP { int a; char b; };\n"),
            SV("TP"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 4 },
            },
        },
        {
            "alignas on struct", __LINE__,
            SV("_Alignas(32) struct AS { int x; };\n"
               "struct AS as;\n"),
            SV("AS"), 0,
            .size = 4, .alignment = 4,
            .fields = {
                { SV("x"), .offset = 0 },
            },
        },
        {
            "alignas on struct field", __LINE__,
            SV("struct AF { char a; _Alignas(16) int b; };\n"
               "struct AF af;\n"),
            SV("AF"), 0,
            .size = 32, .alignment = 16,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 16 },
            },
        },
        {
            "pragma pack(1)", __LINE__,
            SV("#pragma pack(1)\n"
               "struct PP1 { char a; int b; char c; };\n"
               "#pragma pack()\n"),
            SV("PP1"), 0,
            .size = 6, .alignment = 1,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 1 },
                { SV("c"), .offset = 5 },
            },
        },
        {
            "pragma pack(2)", __LINE__,
            SV("#pragma pack(2)\n"
               "struct PP2 { char a; int b; double c; };\n"
               "#pragma pack()\n"),
            SV("PP2"), 0,
            .size = 14, .alignment = 2,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 2 },
                { SV("c"), .offset = 6 },
            },
        },
        {
            "pragma pack push/pop", __LINE__,
            SV("#pragma pack(push, 1)\n"
               "struct PPush { char a; int b; };\n"
               "#pragma pack(pop)\n"
               "struct PAfter { char a; int b; };\n"),
            SV("PPush"), 0,
            .size = 5, .alignment = 1,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 1 },
            },
        },
        {
            "pragma pack pop restores", __LINE__,
            SV("#pragma pack(push, 1)\n"
               "struct Ignore { char a; int b; };\n"
               "#pragma pack(pop)\n"
               "struct Restored { char a; int b; };\n"),
            SV("Restored"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 4 },
            },
        },
        {
            "pragma pack(4) limits alignment", __LINE__,
            SV("#pragma pack(4)\n"
               "struct PP4 { char a; double b; };\n"
               "#pragma pack()\n"),
            SV("PP4"), 0,
            .size = 12, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 4 },
            },
        },
        {
            "pragma pack macro expansion", __LINE__,
            SV("#define MYPACK 1\n"
               "#pragma pack(MYPACK)\n"
               "struct PPM { int a; char b; };\n"
               "#pragma pack()\n"),
            SV("PPM"), 0,
            .size = 5, .alignment = 1,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 4 },
            },
        },
        {
            "pragma pack(push) macro expansion", __LINE__,
            SV("#define P 1\n"
               "#pragma pack(push, P)\n"
               "struct PPPM { int a; char b; };\n"
               "#pragma pack(pop)\n"),
            SV("PPPM"), 0,
            .size = 5, .alignment = 1,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 4 },
            },
        },
        {
            "pragma pack(push, ident)", __LINE__,
            SV("#pragma pack(1)\n"
               "#pragma pack(push, A, 4)\n"
               "#pragma pack(push, 8)\n"
               "#pragma pack(pop)\n"
               "struct S { int a; char b; };\n"),
            SV("S"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 4 },
            },
        },
        {
            "pragma pack(pop, ident)", __LINE__,
            SV("#pragma pack(1)\n"
               "#pragma pack(push, A, 4)\n"
               "#pragma pack(push, 8)\n"
               "#pragma pack(pop, A)\n"
               "struct S { int a; char b; };\n"),
            SV("S"), 0,
            .size = 5, .alignment = 1,
            .fields = {
                { SV("a"), .offset = 0 },
                { SV("b"), .offset = 4 },
            },
        },
        {
            "zero-width bitfield forces alignment", __LINE__,
            SV("struct Z { int a : 3; int : 0; int b : 5; };\n"),
            SV("Z"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { {0}, .offset = 4 },
                { SV("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "FAM at end", __LINE__,
            SV("struct F { int len; char data[]; };\n"),
            SV("F"), 0,
            .size = 4, .alignment = 4,
            .fields = {
                { SV("len"), .offset = 0 },
                { SV("data"), .offset = 4 },
            },
        },
        {
            "FAM alignment", __LINE__,
            SV("struct F { char c; int data[]; };\n"),
            SV("F"), 0,
            .size = 4, .alignment = 4,
            .fields = {
                { SV("c"), .offset = 0 },
                { SV("data"), .offset = 4 },
            },
        },
        {
            "FAM double alignment", __LINE__,
            SV("struct F { int n; double data[]; };\n"),
            SV("F"), 0,
            .size = 8, .alignment = 8,
            .fields = {
                { SV("n"), .offset = 0 },
                { SV("data"), .offset = 8 },
            },
        },
        {
            "FAM only member", __LINE__,
            SV("struct F { int data[]; };\n"),
            SV("F"), 0,
            .size = 0, .alignment = 4,
            .fields = {
                { SV("data"), .offset = 0 },
            },
        },
    };
    MStringBuilder sb = {0};
    for(size_t i = 0; i < arrlen(cases); i++){
        ArenaAllocator aa = {0};
        Allocator al = allocator_from_arena(&aa);
        sb.allocator = al;
        msb_reset(&sb);
        FileCache* fc = fc_create(al);
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {.allocator = al};
        Environment env = {.allocator = al, .at=&at};
        int err = 0;
        CcParser cc = {
            .cpp = {
                .allocator = al,
                .fc = fc,
                .at = &at,
                .logger = logger,
                .env = &env,
                .target = cc_target_test(),
            },
            .current = &cc.global,
        };
        struct StructCase* c = &cases[i];
        fc_write_path(fc, "(test)", 6);
        err = fc_cache_file(fc, c->input);
        if(err) {TestReport("failed to cache"); goto fin;}
        err = cpp_define_builtin_macros(&cc.cpp);
        if(err) {TestReport("failed to define builtins"); goto fin;}
        err = cc_define_builtin_types(&cc);
        if(err) {TestReport("failed to define builtin types"); goto fin;}
        err = cc_register_pragmas(&cc);
        if(err) {TestReport("failed to register pragmas"); goto fin;}
        err = cpp_include_file_via_file_cache(&cc.cpp, SV("(test)"));
        if(err) {TestReport("failed to include"); goto fin;}
        err = cc_parse_all(&cc);
        if(err) {TestReport("failed to parse"); goto fin;}
        {
            Atom tag = AT_get_atom(&at, c->tag.text, c->tag.length);
            if(!tag){
                TestReport("tag atom not found");
                err = 1; goto fin;
            }
            uint32_t actual_size, actual_align, actual_field_count;
            CcField* _Nullable actual_fields;
            if(c->is_union){
                CcUnion* u = cc_scope_lookup_union_tag(&cc.global, tag, CC_SCOPE_NO_WALK);
                TestExpectTrue(u);
                if(!u){ err = 1; goto fin; }
                actual_size = u->size;
                actual_align = u->alignment;
                actual_field_count = u->field_count;
                actual_fields = u->fields;
            }
            else {
                CcStruct* s = cc_scope_lookup_struct_tag(&cc.global, tag, CC_SCOPE_NO_WALK);
                TestExpectTrue(s);
                if(!s){ err = 1; goto fin; }
                actual_size = s->size;
                actual_align = s->alignment;
                actual_field_count = s->field_count;
                actual_fields = s->fields;
            }
            TEST_stats.executed++;
            if(actual_size != c->size){
                TEST_stats.failures++;
                TestPrintf("%s:%d: size mismatch: got %u, expected %u\n", __FILE__, c->line, actual_size, c->size);
            }
            TEST_stats.executed++;
            if(actual_align != c->alignment){
                TEST_stats.failures++;
                TestPrintf("%s:%d: alignment mismatch: got %u, expected %u\n", __FILE__, c->line, actual_align, c->alignment);
            }
            // Check fields
            for(size_t n = 0; n < MAXFIELDS; n++){
                struct FieldExpect* fe = &c->fields[n];
                if(!fe->name.length) break;
                TEST_stats.executed++;
                if(n >= actual_field_count){
                    TEST_stats.failures++;
                    TestPrintf("%s:%d: field %zu '%.*s' missing (only %u fields)\n", __FILE__, c->line, n, sv_p(fe->name), actual_field_count);
                    break;
                }
                CcField* af = &actual_fields[n];
                // Check name
                TEST_stats.executed++;
                if(af->name){
                    StringView actual_name = {.text = af->name->data, .length = af->name->length};
                    test_expect_equals_sv(fe->name, actual_name, "expected", "field name", &TEST_stats, __FILE__, __func__, c->line);
                }
                else if(fe->name.length){
                    TEST_stats.failures++;
                    TestPrintf("%s:%d: field %zu: expected name '%.*s' but got anonymous\n", __FILE__, c->line, n, sv_p(fe->name));
                }
                // Check offset
                TEST_stats.executed++;
                if(af->offset != fe->offset){
                    TEST_stats.failures++;
                    TestPrintf("%s:%d: field '%.*s' offset: got %u, expected %u\n", __FILE__, c->line, sv_p(fe->name), af->offset, fe->offset);
                }
                // Check bitfield
                if(fe->bitwidth){
                    TEST_stats.executed++;
                    if(af->bitwidth != fe->bitwidth){
                        TEST_stats.failures++;
                        TestPrintf("%s:%d: field '%.*s' bitwidth: got %u, expected %u\n", __FILE__, c->line, sv_p(fe->name), af->bitwidth, fe->bitwidth);
                    }
                    TEST_stats.executed++;
                    if(af->bitoffset != fe->bitoffset){
                        TEST_stats.failures++;
                        TestPrintf("%s:%d: field '%.*s' bitoffset: got %u, expected %u\n", __FILE__, c->line, sv_p(fe->name), af->bitoffset, fe->bitoffset);
                    }
                }
                // Check type repr
                if(fe->type_repr.length){
                    msb_reset(&sb);
                    cc_print_type(&sb, af->type);
                    StringView r = msb_borrow_sv(&sb);
                    test_expect_equals_sv(fe->type_repr, r, "expected type", "field type", &TEST_stats, __FILE__, __func__, c->line);
                }
            }
        }
        fin:
        if(log_sb.cursor && !log_sb.errored){
            StringView sv = msb_borrow_sv(&log_sb);
            TestPrintf("%s:%d: %s %.*s\n", __FILE__, c->line, c->test, sv_p(sv));
        }
        TestExpectFalse(err);
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&cc.cpp.synth_arena);
        ArenaAllocator_free_all(&cc.scratch_arena);
    }
    TESTEND();
}

TestFunction(test_bitfield_abi){
    TESTBEGIN();
    enum { MAXFIELDS = 16 };
    struct FieldExpect {
        StringView name;
        uint32_t offset;
        uint32_t bitwidth;
        uint32_t bitoffset;
    };
    struct BFCase {
        const char* test; int line;
        StringView input;
        StringView tag;
        CcBitfieldABI abi;
        uint32_t size;
        uint32_t alignment;
        struct FieldExpect fields[MAXFIELDS];
    } cases[] = {
        {
            "sysv: same-size different types share", __LINE__,
            SV("struct S { int a : 3; unsigned b : 5; };\n"),
            SV("S"), CC_BITFIELD_SYSV,
            .size = 4, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SV("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
            },
        },
        {
            "msvc: different types don't share", __LINE__,
            SV("struct S { int a : 3; unsigned b : 5; };\n"),
            SV("S"), CC_BITFIELD_MSVC,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SV("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "msvc: same type shares", __LINE__,
            SV("struct S { int a : 3; int b : 5; };\n"),
            SV("S"), CC_BITFIELD_MSVC,
            .size = 4, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SV("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
            },
        },
        {
            "sysv: different sizes don't share", __LINE__,
            SV("struct S { int a : 3; short b : 5; };\n"),
            SV("S"), CC_BITFIELD_SYSV,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SV("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "msvc: different sizes don't share", __LINE__,
            SV("struct S { int a : 3; short b : 5; };\n"),
            SV("S"), CC_BITFIELD_MSVC,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SV("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "msvc: three fields, type changes", __LINE__,
            SV("struct S { int a : 3; int b : 5; unsigned c : 8; };\n"),
            SV("S"), CC_BITFIELD_MSVC,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SV("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
                { SV("c"), .offset = 4, .bitwidth = 8, .bitoffset = 0 },
            },
        },
        {
            "sysv: three fields, same size", __LINE__,
            SV("struct S { int a : 3; int b : 5; unsigned c : 8; };\n"),
            SV("S"), CC_BITFIELD_SYSV,
            .size = 4, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SV("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
                { SV("c"), .offset = 0, .bitwidth = 8, .bitoffset = 8 },
            },
        },
        {
            "sysv: zero-width ends run", __LINE__,
            SV("struct S { int a : 3; int : 0; int b : 5; };\n"),
            SV("S"), CC_BITFIELD_SYSV,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { {0} },
                { SV("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "sysv: zero-width when no run is noop", __LINE__,
            SV("struct S { int x; int : 0; int a : 5; };\n"),
            SV("S"), CC_BITFIELD_SYSV,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("x"), .offset = 0 },
                { {0} },
                { SV("a"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "sysv: overflow to next storage unit", __LINE__,
            SV("struct S { int a : 30; int b : 5; };\n"),
            SV("S"), CC_BITFIELD_SYSV,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 30, .bitoffset = 0 },
                { SV("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "msvc: overflow to next storage unit", __LINE__,
            SV("struct S { int a : 30; int b : 5; };\n"),
            SV("S"), CC_BITFIELD_MSVC,
            .size = 8, .alignment = 4,
            .fields = {
                { SV("a"), .offset = 0, .bitwidth = 30, .bitoffset = 0 },
                { SV("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
    };
    MStringBuilder sb = {0};
    for(size_t i = 0; i < arrlen(cases); i++){
        ArenaAllocator aa = {0};
        Allocator al = allocator_from_arena(&aa);
        sb.allocator = al;
        msb_reset(&sb);
        FileCache* fc = fc_create(al);
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {.allocator = al};
        Environment env = {.allocator = al, .at=&at};
        int err = 0;
        CcTargetConfig tgt = cc_target_test();
        tgt.bitfield_abi = cases[i].abi;
        CcParser cc = {
            .cpp = {
                .allocator = al,
                .fc = fc,
                .at = &at,
                .logger = logger,
                .env = &env,
                .target = tgt,
            },
            .current = &cc.global,
        };
        struct BFCase* c = &cases[i];
        fc_write_path(fc, "(test)", 6);
        err = fc_cache_file(fc, c->input);
        if(err) {TestReport("failed to cache"); goto bffin;}
        err = cpp_define_builtin_macros(&cc.cpp);
        if(err) {TestReport("failed to define builtins"); goto bffin;}
        err = cc_define_builtin_types(&cc);
        if(err) {TestReport("failed to define builtin types"); goto bffin;}
        err = cpp_include_file_via_file_cache(&cc.cpp, SV("(test)"));
        if(err) {TestReport("failed to include"); goto bffin;}
        err = cc_parse_all(&cc);
        if(err) {TestReport("failed to parse"); goto bffin;}
        {
            Atom tag = AT_get_atom(&at, c->tag.text, c->tag.length);
            if(!tag){ TestReport("tag atom not found"); err = 1; goto bffin; }
            CcStruct* s = cc_scope_lookup_struct_tag(&cc.global, tag, CC_SCOPE_NO_WALK);
            TestExpectTrue(s);
            if(!s){ err = 1; goto bffin; }
            TEST_stats.executed++;
            if(s->size != c->size){
                TEST_stats.failures++;
                TestPrintf("%s:%d: %s: size mismatch: got %u, expected %u\n", __FILE__, c->line, c->test, s->size, c->size);
            }
            TEST_stats.executed++;
            if(s->alignment != c->alignment){
                TEST_stats.failures++;
                TestPrintf("%s:%d: %s: alignment mismatch: got %u, expected %u\n", __FILE__, c->line, c->test, s->alignment, c->alignment);
            }
            for(size_t n = 0; n < MAXFIELDS; n++){
                struct FieldExpect* fe = &c->fields[n];
                if(!fe->name.length) break;
                TEST_stats.executed++;
                if(n >= s->field_count){
                    TEST_stats.failures++;
                    TestPrintf("%s:%d: %s: field %zu missing\n", __FILE__, c->line, c->test, n);
                    break;
                }
                CcField* af = &s->fields[n];
                TEST_stats.executed++;
                if(af->offset != fe->offset){
                    TEST_stats.failures++;
                    TestPrintf("%s:%d: %s: field '%.*s' offset: got %u, expected %u\n", __FILE__, c->line, c->test, sv_p(fe->name), af->offset, fe->offset);
                }
                if(fe->bitwidth){
                    TEST_stats.executed++;
                    if(af->bitwidth != fe->bitwidth){
                        TEST_stats.failures++;
                        TestPrintf("%s:%d: %s: field '%.*s' bitwidth: got %u, expected %u\n", __FILE__, c->line, c->test, sv_p(fe->name), af->bitwidth, fe->bitwidth);
                    }
                    TEST_stats.executed++;
                    if(af->bitoffset != fe->bitoffset){
                        TEST_stats.failures++;
                        TestPrintf("%s:%d: %s: field '%.*s' bitoffset: got %u, expected %u\n", __FILE__, c->line, c->test, sv_p(fe->name), af->bitoffset, fe->bitoffset);
                    }
                }
            }
        }
        bffin:
        if(log_sb.cursor && !log_sb.errored){
            StringView sv = msb_borrow_sv(&log_sb);
            TestPrintf("%s:%d: %s %.*s\n", __FILE__, c->line, c->test, sv_p(sv));
        }
        TestExpectFalse(err);
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&cc.cpp.synth_arena);
        ArenaAllocator_free_all(&cc.scratch_arena);
    }
    TESTEND();
}

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(test_parse_decls);
    RegisterTest(test_parse_errors);
    RegisterTest(test_struct_layout);
    RegisterTest(test_bitfield_abi);
    int err = test_main(argc, argv, NULL);
    testing_assert_all_freed();
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "../Drp/Allocators/allocator.c"
#include "../Drp/file_cache.c"
#include "cpp_preprocessor.c"
#include "cc_parser.c"
