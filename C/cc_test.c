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
    static struct Case {
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
            SVI("int (*x)[10];\n"
               "int (y);\n"
               "int bar(int x);\n"
               "void (*signal(int, void (*fp)(int)))(int);\n"
               "const int **restrict * p;\n"
               "typedef int foo(void);\n"
              ),
            {
                { SVI("x"), SVI("int (*)[10]") },
                { SVI("y"), SVI("int") },
                { SVI("p"), SVI("const int * * *") },
            },
            {
                {SVI("bar"), SVI("int(int)")},
                {SVI("signal"), SVI("void (*(int, void (*)(int)))(int)")},
            },
            {
                {SVI("foo"), SVI("int(void)")},
            },
        },
        {
            "basic types", __LINE__,
            SVI("int a;\n"
               "char b;\n"
               "unsigned long long c;\n"
               "short d;\n"
               "void *e;\n"
              ),
            .vars={
                { SVI("a"), SVI("int") },
                { SVI("b"), SVI("char") },
                { SVI("c"), SVI("unsigned long long") },
                { SVI("d"), SVI("short") },
                { SVI("e"), SVI("void *") },
            },
        },
        {
            "pointers", __LINE__,
            SVI("int *a;\n"
               "int **b;\n"
               "int ***c;\n"
               "const int *d;\n"
               "int *const e;\n"
               "const int *const volatile f;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int *") },
                { SVI("b"), SVI("int * *") },
                { SVI("c"), SVI("int * * *") },
                { SVI("d"), SVI("const int *") },
                { SVI("e"), SVI("int *const ") },
                { SVI("f"), SVI("const int *const volatile ") },
            },
        },
        {
            "arrays", __LINE__,
            SVI("int a[5];\n"
               "int b[3][4];\n"
               "int *c[10];\n"
               "int (*d)[10];\n"
               "int e[];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[5]") },
                { SVI("b"), SVI("int[3][4]") },
                { SVI("c"), SVI("int *[10]") },
                { SVI("d"), SVI("int (*)[10]") },
                { SVI("e"), SVI("int[]") },
            },
        },
        {
            "functions", __LINE__,
            SVI("int f(void);\n"
               "int g(int, int);\n"
               "int h(int, ...);\n"
               "int k();\n"
              ),
            .funcs = {
                { SVI("f"), SVI("int(void)") },
                { SVI("g"), SVI("int(int, int)") },
                { SVI("h"), SVI("int(int, ...)") },
                { SVI("k"), SVI("int()") },
            },
        },
        {
            "function pointers", __LINE__,
            SVI("int (*fp)(int, int);\n"
               "void (*vfp)(void);\n"
               "int (*(*fpp)(int))(int);\n"
              ),
            .vars = {
                { SVI("fp"), SVI("int (*)(int, int)") },
                { SVI("vfp"), SVI("void (*)(void)") },
                { SVI("fpp"), SVI("int (* (*)(int))(int)") },
            },
        },
        {
            "multiple declarators", __LINE__,
            SVI("int a, *b, **c;\n"),
            .vars = {
                { SVI("a"), SVI("int") },
                { SVI("b"), SVI("int *") },
                { SVI("c"), SVI("int * *") },
            },
        },
        {
            "array of function pointers", __LINE__,
            SVI("int (*a[4])(int);\n"),
            .vars = {
                { SVI("a"), SVI("int (*[4])(int)") },
            },
        },
        {
            "abstract declarators in params", __LINE__,
            SVI("void f(int (int));\n"
               "void g(int (*)(int));\n"
               "void h(int [10]);\n"
               "void k(int (*)[10]);\n"
               "void m(int *, int **, const int *);\n"
               "void n(int ());\n"
              ),
            .funcs = {
                { SVI("f"), SVI("void(int (*)(int))") },
                { SVI("g"), SVI("void(int (*)(int))") },
                { SVI("h"), SVI("void(int *)") },
                { SVI("k"), SVI("void(int (*)[10])") },
                { SVI("m"), SVI("void(int *, int * *, const int *)") },
                { SVI("n"), SVI("void(int (*)())") },
            },
        },
        {
            "typedef void", __LINE__,
            SVI("typedef void myvoid;\n"
               "myvoid *p;\n"
               "myvoid (*fn)(int);\n"
              ),
            .vars = {
                { SVI("p"), SVI("void *") },
                { SVI("fn"), SVI("void (*)(int)") },
            },
            .typedefs = {
                { SVI("myvoid"), SVI("void") },
            },
        },
        {
            "typedefs as base types", __LINE__,
            SVI("typedef int myint;\n"
               "myint a;\n"
               "myint *b;\n"
               "myint c[5];\n"
               "myint (*d)(myint);\n"
              ),
            .vars = {
                { SVI("a"), SVI("int") },
                { SVI("b"), SVI("int *") },
                { SVI("c"), SVI("int[5]") },
                { SVI("d"), SVI("int (*)(int)") },
            },
            .typedefs = {
                { SVI("myint"), SVI("int") },
            },
        },
        {
            "typedef typedef", __LINE__,
            SVI("typedef int x;\n"
                "typedef x y;"),
            .typedefs = {
                {SVI("x"), SVI("int")},
                {SVI("y"), SVI("int")},
            },
        },
        {
            "typedef typedef pointer", __LINE__,
            SVI("typedef void* list;\n"
                "typedef list list2;"),
            .typedefs = {
                {SVI("list"), SVI("void *")},
                {SVI("list2"), SVI("void *")},
            },
        },
        {
            "typedef pointer", __LINE__,
            SVI("typedef int *intptr;\n"
               "intptr a;\n"
               "intptr *b;\n"
               "const intptr c;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int *") },
                { SVI("b"), SVI("int * *") },
                { SVI("c"), SVI("int *const ") },
            },
            .typedefs = {
                { SVI("intptr"), SVI("int *") },
            },
        },
        {
            "typedef function type", __LINE__,
            SVI("typedef int fn(int, int);\n"
               "fn *fp;\n"
              ),
            .vars = {
                { SVI("fp"), SVI("int (*)(int, int)") },
            },
            .typedefs = {
                { SVI("fn"), SVI("int(int, int)") },
            },
        },
        {
            "typedef in function params", __LINE__,
            SVI("typedef int myint;\n"
               "void f(myint x, myint *y);\n"
              ),
            .funcs = {
                { SVI("f"), SVI("void(int, int *)") },
            },
            .typedefs = {
                { SVI("myint"), SVI("int") },
            },
        },
        {
            "typedef paren disambiguation", __LINE__,
            SVI("typedef int T;\n"
               "void f(T);\n"
               "void g(T x);\n"
               "int (h);\n"
              ),
            .vars = {
                { SVI("h"), SVI("int") },
            },
            .funcs = {
                { SVI("f"), SVI("void(int)") },
                { SVI("g"), SVI("void(int)") },
            },
            .typedefs = {
                { SVI("T"), SVI("int") },
            },
        },
        {
            "chained typedefs", __LINE__,
            SVI("typedef int A;\n"
               "typedef A B;\n"
               "typedef B *C;\n"
               "C x;\n"
              ),
            .vars = {
                { SVI("x"), SVI("int *") },
            },
            .typedefs = {
                { SVI("A"), SVI("int") },
                { SVI("B"), SVI("int") },
                { SVI("C"), SVI("int *") },
            },
        },
        {
            "typeof basic", __LINE__,
            SVI("typeof(int) a;\n"
               "typeof(int *) b;\n"
               "typeof(int [5]) c;\n"
               "typeof(int (*)(int)) d;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int") },
                { SVI("b"), SVI("int *") },
                { SVI("c"), SVI("int[5]") },
                { SVI("d"), SVI("int (*)(int)") },
            },
        },
        {
            "typeof with typedef", __LINE__,
            SVI("typedef const int *cip;\n"
               "typeof(cip) a;\n"
               "typeof(cip) *b;\n"
              ),
            .vars = {
                { SVI("a"), SVI("const int *") },
                { SVI("b"), SVI("const int * *") },
            },
            .typedefs = {
                { SVI("cip"), SVI("const int *") },
            },
        },
        {
            "typeof_unqual", __LINE__,
            SVI("typeof_unqual(const int) a;\n"
               "typeof_unqual(const volatile int *) b;\n"
               "typeof_unqual(int *const) c;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int") },
                { SVI("b"), SVI("const volatile int *") },
                { SVI("c"), SVI("int *") },
            },
        },
        {
            "typeof in typedef", __LINE__,
            SVI("typedef typeof(int *) intptr;\n"
               "intptr a;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int *") },
            },
            .typedefs = {
                { SVI("intptr"), SVI("int *") },
            },
        },
        {
            "typeof expression", __LINE__,
            SVI("typeof(1) a;\n"
               "typeof(1.0) b;\n"
               "typeof(1.0f) c;\n"
               "typeof(1 + 2) d;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int") },
                { SVI("b"), SVI("double") },
                { SVI("c"), SVI("float") },
                { SVI("d"), SVI("int") },
            },
        },
        {
            "sizeof in array dim", __LINE__,
            SVI("int a[sizeof(int)];\n"
               "int b[sizeof(char)];\n"
               "int c[sizeof(int*)];\n"
               "int d[alignof(int)];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[4]") },
                { SVI("b"), SVI("int[1]") },
                { SVI("c"), SVI("int[8]") },
                { SVI("d"), SVI("int[4]") },
            },
        },
        {
            "alignof expr", __LINE__,
            SVI("int x;\n"
               "int a[_Alignof x];\n"
               "int *p;\n"
               "int b[_Alignof p];\n"
               "double d;\n"
               "int c[_Alignof d];\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("a"), SVI("int[4]") },
                { SVI("p"), SVI("int *") },
                { SVI("b"), SVI("int[8]") },
                { SVI("d"), SVI("double") },
                { SVI("c"), SVI("int[8]") },
            },
        },
        {
            "_Alignas on decl", __LINE__,
            SVI("_Alignas(16) int x;\n"
               "_Alignas(32) char buf[64];\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("buf"), SVI("char[64]") },
            },
        },
        {
            "_Alignof _Alignas decl", __LINE__,
            SVI("_Alignas(16) int x;\n"
               "int a[_Alignof x];\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("a"), SVI("int[16]") },
            },
        },
        {
            "_Alignas with type arg", __LINE__,
            SVI("_Alignas(double) int x;\n"
               "int a[_Alignof x];\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("a"), SVI("int[8]") },
            },
        },
        {
            "prefix aligned attribute on decl", __LINE__,
            SVI("__attribute__((aligned(16))) int x;\n"
               "int a[_Alignof x];\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("a"), SVI("int[16]") },
            },
        },
        {
            "__declspec(align) on decl", __LINE__,
            SVI("__declspec(align(16)) int x;\n"
               "int a[_Alignof x];\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("a"), SVI("int[16]") },
            },
        },
        {
            "__declspec(noreturn)", __LINE__,
            SVI("__declspec(noreturn) void die(void);\n"),
            .funcs = {
                { SVI("die"), SVI("void(void)") },
            },
        },
        {
            "__declspec(dllimport) ignored", __LINE__,
            SVI("__declspec(dllimport) int foo(void);\n"),
            .funcs = {
                { SVI("foo"), SVI("int(void)") },
            },
        },
        {
            "__declspec multiple specifiers", __LINE__,
            SVI("__declspec(dllimport noreturn) void bar(void);\n"),
            .funcs = {
                { SVI("bar"), SVI("void(void)") },
            },
        },
        {
            "__declspec(deprecated) with args ignored", __LINE__,
            SVI("__declspec(deprecated(\"use bar\")) int foo(void);\n"),
            .funcs = {
                { SVI("foo"), SVI("int(void)") },
            },
        },
        {
            "empty __declspec", __LINE__,
            SVI("__declspec() int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "typeof sizeof", __LINE__,
            SVI("typeof(sizeof(int)) a;\n"),
            .vars = {
                { SVI("a"), SVI("unsigned long") },
            },
        },
        {
            "typeof cast", __LINE__,
            SVI("typeof((float)1) a;\n"
               "typeof((double)1) b;\n"
               "typeof((char)1) c;\n"
              ),
            .vars = {
                { SVI("a"), SVI("float") },
                { SVI("b"), SVI("double") },
                { SVI("c"), SVI("char") },
            },
        },
        {
            "typeof variable", __LINE__,
            SVI("int x;\n"
               "typeof(x) a;\n"
               "int *p;\n"
               "typeof(*p) b;\n"
               "typeof(&x) c;\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("a"), SVI("int") },
                { SVI("p"), SVI("int *") },
                { SVI("b"), SVI("int") },
                { SVI("c"), SVI("int *") },
            },
        },
        {
            "typeof true/false", __LINE__,
            SVI("typeof(true) a;\n"
               "typeof(false) b;\n"
              ),
            .vars = {
                { SVI("a"), SVI("_Bool") },
                { SVI("b"), SVI("_Bool") },
            },
        },
        {
            "countof in array dim", __LINE__,
            SVI("int a[5];\n"
               "int b[_Countof(a)];\n"
               "int c[_Countof(int[3])];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[5]") },
                { SVI("b"), SVI("int[5]") },
                { SVI("c"), SVI("int[3]") },
            },
        },
        {
            "int literals", __LINE__,
            SVI("int a = 42;\n"
               "int b = -1;\n"
               "unsigned c = 3u;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int"),      SVI("42") },
                { SVI("b"), SVI("int"),      SVI("-1") },
                { SVI("c"), SVI("unsigned int"), SVI("3") },
            },
        },
        {
            "float literals", __LINE__,
            SVI("float a = 1.5f;\n"
               "double b = 2.5;\n"
              ),
            .vars = {
                { SVI("a"), SVI("float"),  SVI("1.5f") },
                { SVI("b"), SVI("double"), SVI("2.5") },
            },
        },
        {
            "arithmetic", __LINE__,
            SVI("int a = 1 + 2;\n"
               "int b = 3 * 4 + 5;\n"
               "int c = 1 << 2;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int"), SVI("(1 + 2)") },
                { SVI("b"), SVI("int"), SVI("((3 * 4) + 5)") },
                { SVI("c"), SVI("int"), SVI("(1 << 2)") },
            },
        },
        {
            "comparisons", __LINE__,
            SVI("int a = 1 < 2;\n"
               "int b = 3 == 4;\n"
               "int c = 5 != 6;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int"), SVI("(1 < 2)") },
                { SVI("b"), SVI("int"), SVI("(3 == 4)") },
                { SVI("c"), SVI("int"), SVI("(5 != 6)") },
            },
        },
        {
            "logical ops", __LINE__,
            SVI("int a = 1 && 2;\n"
               "int b = 0 || 1;\n"
               "int c = !0;\n"
              ),
            .vars = {
                { SVI("a"), SVI("int"), SVI("(1 && 2)") },
                { SVI("b"), SVI("int"), SVI("(0 || 1)") },
                { SVI("c"), SVI("int"), SVI("!0") },
            },
        },
        {
            "cast expr", __LINE__,
            SVI("float a = (float)42;\n"
               "long b = (long)1;\n"
              ),
            .vars = {
                { SVI("a"), SVI("float"), SVI("(float)42") },
                { SVI("b"), SVI("long"),  SVI("(long)1") },
            },
        },
        {
            "ternary", __LINE__,
            SVI("int a = 1 ? 2 : 3;\n"),
            .vars = {
                { SVI("a"), SVI("int"), SVI("(1 ? 2 : 3)") },
            },
        },
        {
            "ternary pointer", __LINE__,
            SVI("int* p;\n"
               "int* q;\n"
               "int* r = 1 ? p : q;\n"),
            .vars = {
                { SVI("p"), SVI("int *") },
                { SVI("q"), SVI("int *") },
                { SVI("r"), SVI("int *") },
            },
        },
        {
            "ternary pointer and null", __LINE__,
            SVI("int* p;\n"
               "int* q = 1 ? p : 0;\n"
               "int* r = 0 ? 0 : p;\n"),
            .vars = {
                { SVI("p"), SVI("int *") },
                { SVI("q"), SVI("int *") },
                { SVI("r"), SVI("int *") },
            },
        },
        {
            "ternary void pointer", __LINE__,
            SVI("int* p;\n"
               "void* v;\n"
               "void* r = 1 ? p : v;\n"),
            .vars = {
                { SVI("p"), SVI("int *") },
                { SVI("v"), SVI("void *") },
                { SVI("r"), SVI("void *") },
            },
        },
        {
            "ternary pointer qualifier merge", __LINE__,
            SVI("const int* p;\n"
               "volatile int* q;\n"
               "const volatile int* r = 1 ? p : q;\n"),
            .vars = {
                { SVI("p"), SVI("const int *") },
                { SVI("q"), SVI("volatile int *") },
                { SVI("r"), SVI("const volatile int *") },
            },
        },
        {
            "ternary pointer and string literal", __LINE__,
            SVI("char* p;\n"
               "const char* r = 1 ? p : \"hello\";\n"),
            .vars = {
                { SVI("p"), SVI("char *") },
                { SVI("r"), SVI("const char *") },
            },
        },
        {
            "ternary struct pointer", __LINE__,
            SVI("struct S { int x; };\n"
               "struct S* a;\n"
               "struct S* b;\n"
               "struct S* c = 1 ? a : b;\n"),
            .vars = {
                { SVI("a"), SVI("struct S *") },
                { SVI("b"), SVI("struct S *") },
                { SVI("c"), SVI("struct S *") },
            },
        },
        {
            "sizeof expr init", __LINE__,
            SVI("unsigned long a = sizeof(int);\n"
               "unsigned long b = sizeof(char);\n"
              ),
            .vars = {
                { SVI("a"), SVI("unsigned long"), SVI("4") },
                { SVI("b"), SVI("unsigned long"), SVI("1") },
            },
        },
        {
            "variable ref", __LINE__,
            SVI("int x = 10;\n"
               "int y = x;\n"
               "int z = x + 1;\n"
              ),
            .vars = {
                { SVI("x"), SVI("int"), SVI("10") },
                { SVI("y"), SVI("int"), SVI("x") },
                { SVI("z"), SVI("int"), SVI("(x + 1)") },
            },
        },
        {
            "addr and deref", __LINE__,
            SVI("int x = 0;\n"
               "int *p = &x;\n"
               "int y = *p;\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("p"), SVI("int *"),  SVI("&x") },
                { SVI("y"), SVI("int"),    SVI("*p") },
            },
        },
        {
            "subscript", __LINE__,
            SVI("int a[4];\n"
               "int b = a[2];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[4]") },
                { SVI("b"), SVI("int"),    SVI("a[2]") },
            },
        },
        {
            "comma expr", __LINE__,
            SVI("int a = (1, 2);\n"),
            .vars = {
                { SVI("a"), SVI("int"), SVI("(1 , 2)") },
            },
        },
        {
            "true false nullptr", __LINE__,
            SVI("_Bool a = true;\n"
               "_Bool b = false;\n"
              ),
            .vars = {
                { SVI("a"), SVI("_Bool"), SVI("1") },
                { SVI("b"), SVI("_Bool"), SVI("0") },
            },
        },
        {
            "func def", __LINE__,
            SVI("int foo(int x, int y){ return x + y; }\n"),
            .funcs = {
                { SVI("foo"), SVI("int(int, int)") },
            },
        },
        {
            "forward decl then def", __LINE__,
            SVI("int bar(int);\n"
               "int bar(int x){ return x; }\n"
              ),
            .funcs = {
                { SVI("bar"), SVI("int(int)") },
            },
        },
        {
            "basic enum", __LINE__,
            SVI("enum Color { RED, GREEN, BLUE };\n"
               "enum Color c;\n"
              ),
            .vars = {
                { SVI("c"), SVI("enum Color") },
            },
        },
        {
            "anonymous enum", __LINE__,
            SVI("enum { A, B, C };\n"
               "int x;\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "enum with explicit values", __LINE__,
            SVI("enum { X = 10, Y = 20, Z };\n"
               "int a[Z];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[21]") },
            },
        },
        {
            "enum forward reference", __LINE__,
            SVI("enum Foo;\n"
               "enum Foo *p;\n"
              ),
            .vars = {
                { SVI("p"), SVI("enum Foo *") },
            },
        },
        {
            "typedef enum", __LINE__,
            SVI("typedef enum { LOW, HIGH } Level;\n"
               "Level l;\n"
              ),
            .vars = {
                { SVI("l"), SVI("enum <anon>") },
            },
            .typedefs = {
                { SVI("Level"), SVI("enum <anon>") },
            },
        },
        {
            "enum in sizeof", __LINE__,
            SVI("enum E { V1, V2 };\n"
               "int a[sizeof(enum E)];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[4]") },
            },
        },
        {
            "enumerator in expression", __LINE__,
            SVI("enum { TWO = 2, THREE = 3, FIVE = TWO + THREE };\n"
               "int a[FIVE];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[5]") },
            },
        },
        {
            "enum trailing comma", __LINE__,
            SVI("enum T { A, B, C, };\n"
               "enum T t;\n"
              ),
            .vars = {
                { SVI("t"), SVI("enum T") },
            },
        },
        {
            "implicit cast int to float", __LINE__,
            SVI("float f = 42;\n"),
            .vars = {
                { SVI("f"), SVI("float"), SVI("(float)42") },
            },
        },
        // --- Implicit conversion tests (C2y 6.5.17.2) ---
        {
            "null pointer constant 0", __LINE__,
            SVI("int *p = 0;\n"),
            .vars = {
                { SVI("p"), SVI("int *"), .init = SVI("(int *)0") },
            },
        },
        {
            "nullptr to pointer", __LINE__,
            SVI("int *p = nullptr;\n"),
            .vars = {
                { SVI("p"), SVI("int *"), .init = SVI("(int *)0") },
            },
        },
        {
            "pointer to bool", __LINE__,
            SVI("int x;\n"
               "_Bool b = &x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("b"), SVI("_Bool"), .init = SVI("(_Bool)&x") },
            },
        },
        {
            "void* to int*", __LINE__,
            SVI("void *vp;\n"
               "int *ip = vp;\n"),
            .vars = {
                { SVI("vp"), SVI("void *") },
                { SVI("ip"), SVI("int *"), .init = SVI("(int *)vp") },
            },
        },
        {
            "int* to void*", __LINE__,
            SVI("int *ip;\n"
               "void *vp = ip;\n"),
            .vars = {
                { SVI("ip"), SVI("int *") },
                { SVI("vp"), SVI("void *"), .init = SVI("(void *)ip") },
            },
        },
        {
            "int* to const int*", __LINE__,
            SVI("int *ip;\n"
               "const int *cip = ip;\n"),
            .vars = {
                { SVI("ip"), SVI("int *") },
                { SVI("cip"), SVI("const int *"), .init = SVI("(const int *)ip") },
            },
        },
        {
            "enum to int", __LINE__,
            SVI("enum E { A, B };\n"
               "enum E e;\n"
               "int x = e;\n"),
            .vars = {
                { SVI("e"), SVI("enum E") },
                { SVI("x"), SVI("int"), .init = SVI("(int)e") },
            },
        },
        {
            "type inference __auto_type", __LINE__,
            SVI("__auto_type x = 42;\n"),
            .vars = {
                { SVI("x"), SVI("int"), SVI("42") },
            },
        },
        {
            "const type inference", __LINE__,
            SVI("const __auto_type x = 42;\n"),
            .vars = {
                { SVI("x"), SVI("const int"), SVI("42") },
            },
        },
        {
            "const type inference (without type)", __LINE__,
            SVI("const x = 42;\n"),
            .vars = {
                { SVI("x"), SVI("const int"), SVI("42") },
            },
        },
        {
            "pointer assignment", __LINE__,
            SVI("int x;\n"
               "int *p = &x;\n"
              ),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("p"), SVI("int *"), SVI("&x") },
            },
        },
        {
            "enum to int", __LINE__,
            SVI("enum { A };\n"
               "int x = A;\n"
              ),
            .vars = {
                { SVI("x"), SVI("int"), SVI("0") },
            },
        },
        {
            "basic struct", __LINE__,
            SVI("struct Foo { int x; char y; };\n"
               "struct Foo f;\n"
              ),
            .vars = {
                { SVI("f"), SVI("struct Foo") },
            },
        },
        {
            "struct forward ref", __LINE__,
            SVI("struct Bar;\n"
               "struct Bar *p;\n"
              ),
            .vars = {
                { SVI("p"), SVI("struct Bar *") },
            },
        },
        {
            "struct forward then define", __LINE__,
            SVI("struct S;\n"
               "struct S { int a; };\n"
               "struct S s;\n"
              ),
            .vars = {
                { SVI("s"), SVI("struct S") },
            },
        },
        {
            "struct in sizeof", __LINE__,
            SVI("struct P { int x; char y; };\n"
               "int a[sizeof(struct P)];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[8]") },
            },
        },
        {
            "typedef struct", __LINE__,
            SVI("typedef struct { int x; int y; } Point;\n"
               "Point p;\n"
              ),
            .vars = {
                { SVI("p"), SVI("struct <anon>") },
            },
            .typedefs = {
                { SVI("Point"), SVI("struct <anon>") },
            },
        },
        {
            "basic union", __LINE__,
            SVI("union U { int i; float f; };\n"
               "union U u;\n"
              ),
            .vars = {
                { SVI("u"), SVI("union U") },
            },
        },
        {
            "union in sizeof", __LINE__,
            SVI("union V { int i; double d; };\n"
               "int a[sizeof(union V)];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[8]") },
            },
        },
        {
            "nested struct", __LINE__,
            SVI("struct Outer { struct Inner { int a; } inner; int b; };\n"
               "struct Outer o;\n"
              ),
            .vars = {
                { SVI("o"), SVI("struct Outer") },
            },
        },
        {
            "anonymous struct member", __LINE__,
            SVI("struct A { struct { int x; int y; }; int z; };\n"
               "struct A a;\n"
              ),
            .vars = {
                { SVI("a"), SVI("struct A") },
            },
        },
        {
            "packed struct", __LINE__,
            SVI("struct __attribute__((packed)) Packed { int x; char y; };\n"
               "int a[sizeof(struct Packed)];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[5]") },
            },
        },
        {
            "struct pointer member", __LINE__,
            SVI("struct Node { int val; struct Node *next; };\n"
               "struct Node n;\n"
              ),
            .vars = {
                { SVI("n"), SVI("struct Node") },
            },
        },
        {
            "self-referential struct typedef", __LINE__,
            SVI("typedef struct _ffi_type {\n"
               "  int size;\n"
               "  struct _ffi_type **elements;\n"
               "} ffi_type;\n"
               "ffi_type t;\n"
               "ffi_type *p;\n"
               "struct _ffi_type *q;\n"
              ),
            .vars = {
                { SVI("t"), SVI("struct _ffi_type") },
                { SVI("p"), SVI("struct _ffi_type *") },
                { SVI("q"), SVI("struct _ffi_type *") },
            },
            .typedefs = {
                { SVI("ffi_type"), SVI("struct _ffi_type") },
            },
        },
        {
            "self-referential struct assignment", __LINE__,
            SVI("typedef struct _node {\n"
               "  int val;\n"
               "  struct _node *next;\n"
               "} Node;\n"
               "Node n;\n"
               "Node *p = &n;\n"
               "struct _node *q = &n;\n"
              ),
            .vars = {
                { SVI("n"), SVI("struct _node") },
                { SVI("p"), SVI("struct _node *"), SVI("&n") },
                { SVI("q"), SVI("struct _node *"), SVI("&n") },
            },
            .typedefs = {
                { SVI("Node"), SVI("struct _node") },
            },
        },
        {
            "type defined in struct body is visible outside", __LINE__,
            SVI("struct S { enum Color { RED, GREEN, BLUE }; enum Color c; };\n"
               "enum Color x;\n"
              ),
            .vars = {
                { SVI("x"), SVI("enum Color") },
            },
        },
        {
            "plan9", __LINE__,
            SVI("struct Foo {int x;};\n"
               "struct Bar {struct Foo; int y;};\n"
               "int a[sizeof(struct Bar)];\n"
               "struct Bar b;\n"
               "int x[sizeof b.x]\n"),
            .vars = {
                {SVI("a"), SVI("int[8]")},
                {SVI("b"), SVI("struct Bar")},
                {SVI("x"), SVI("int[4]")},
            },
        },
        {
            "struct with bitfield", __LINE__,
            SVI("struct Bits { int a : 3; int b : 5; };\n"
               "int a[sizeof(struct Bits)];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[4]") },
            },
        },
        {
            "struct with bitfield", __LINE__,
            SVI("struct Bits { int a : 3; int b : 5; struct {int: 3;};};\n"
               "int a[sizeof(struct Bits)];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[8]") },
            },
        },
        {
            "enum bitfield", __LINE__,
            SVI("enum E { A, B, C };\n"
               "struct S { enum E x : 3; int y : 5; };\n"
               "int a[sizeof(struct S)];\n"
              ),
            .vars = {
                { SVI("a"), SVI("int[4]") },
            },
        },
        {
            "static_assert pass", __LINE__,
            SVI("static_assert(1, \"ok\");\n"
               "int x;\n"),
            .vars = { { SVI("x"), SVI("int") } },
        },
        {
            "static_assert no message (C23)", __LINE__,
            SVI("static_assert(1);\n"
               "int x;\n"),
            .vars = { { SVI("x"), SVI("int") } },
        },
        {
            "static_assert with expression", __LINE__,
            SVI("static_assert(sizeof(int) == 4, \"int must be 4 bytes\");\n"
               "int x;\n"),
            .vars = { { SVI("x"), SVI("int") } },
        },
        {
            "static_assert in struct", __LINE__,
            SVI("struct S { int a; static_assert(sizeof(int) == 4, \"bad\"); char b; };\n"
               "struct S s;\n"),
            .vars = { { SVI("s"), SVI("struct S") } },
        },
        {
            "constexpr variable in constant expr", __LINE__,
            SVI("constexpr int x = 5;\n"
               "int a[x];\n"),
            .vars = { { SVI("a"), SVI("int[5]") } },
        },
        {
            "constexpr variable in static_assert", __LINE__,
            SVI("constexpr int x = 42;\n"
               "static_assert(x == 42);\n"
               "int y;\n"),
            .vars = { { SVI("y"), SVI("int") } },
        },
        {
            "constexpr variable in case label", __LINE__,
            SVI("constexpr int N = 3;\n"
               "int f(int x) {\n"
               "    switch(x) {\n"
               "        case N: return 1;\n"
               "        default: return 0;\n"
               "    }\n"
               "}\n"),
            .funcs = { { SVI("f"), SVI("int(int)") } },
        },
        {
            "struct method declaration", __LINE__,
            SVI("struct Foo { int x; int get_x(struct Foo* self); };\n"
               "int a[sizeof(struct Foo)];\n"),
            .vars = { { SVI("a"), SVI("int[4]") } },
        },
        {
            "struct method definition", __LINE__,
            SVI("struct Foo { int x; int get_x(struct Foo* self){ return self->x; } };\n"
               "int a[sizeof(struct Foo)];\n"),
            .vars = { { SVI("a"), SVI("int[4]") } },
        },
        {
            "struct multiple methods", __LINE__,
            SVI("struct V { int x; int y;\n"
               "  int get_x(struct V* self){ return self->x; }\n"
               "  int get_y(struct V* self){ return self->y; }\n"
               "};\n"
               "int a[sizeof(struct V)];\n"),
            .vars = { { SVI("a"), SVI("int[8]") } },
        },
        {
            "flexible array member", __LINE__,
            SVI("struct Buf { int len; char data[]; };\n"
               "int a[sizeof(struct Buf)];\n"),
            .vars = { { SVI("a"), SVI("int[4]") } },
        },
        {
            "FAM with padding", __LINE__,
            SVI("struct Buf { double d; int data[]; };\n"
               "int a[sizeof(struct Buf)];\n"),
            .vars = { { SVI("a"), SVI("int[8]") } },
        },
        {
            "FAM in union", __LINE__,
            SVI("union U { int tag; char data[]; };\n"
               "int a[sizeof(union U)];\n"),
            .vars = { { SVI("a"), SVI("int[4]") } },
        },
        {
            "FAM in anonymous struct", __LINE__,
            SVI("struct S { int n; struct { int len; char data[]; }; };\n"
               "int a[sizeof(struct S)];\n"),
            .vars = { { SVI("a"), SVI("int[8]") } },
        },
        {
            "incomplete type in struct", __LINE__,
            SVI("struct Foo {\n"
               "    struct Bar;\n"
               "    int x;\n"
               "};\n"
               "struct Foo f;\n"
               "int a[sizeof(struct Foo)]\n"),
            .vars = {
                {SVI("f"), SVI("struct Foo")},
                {SVI("a"), SVI("int[4]")},
            },
        },
        // --- Brace initialization and designated initializer tests ---
        {
            "struct brace init", __LINE__,
            SVI("struct S { int a; int b; };\n"
               "struct S s = {1, 2};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "struct designated init", __LINE__,
            SVI("struct S { int a; int b; };\n"
               "struct S s = {.b = 2, .a = 1};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@4 = 2, @0 = 1}") },
            },
        },
        {
            "array init", __LINE__,
            SVI("int arr[3] = {1, 2, 3};\n"),
            .vars = {
                { SVI("arr"), SVI("int[3]"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "array designated init", __LINE__,
            SVI("int arr[5] = {[2] = 10, [4] = 20};\n"),
            .vars = {
                { SVI("arr"), SVI("int[5]"), SVI("{@8 = 10, @16 = 20}") },
            },
        },
        {
            "incomplete array sizing", __LINE__,
            SVI("int arr[] = {1, 2, 3};\n"),
            .vars = {
                { SVI("arr"), SVI("int[3]"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "nested struct init", __LINE__,
            SVI("struct S { int a[2]; int b; };\n"
               "struct S s = {{1, 2}, 3};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "union init", __LINE__,
            SVI("union U { int i; float f; };\n"
               "union U u = {.f = 1.5f};\n"),
            .vars = {
                { SVI("u"), SVI("union U"), SVI("{1.5f}") },
            },
        },
        {
            "positional continuation after designation", __LINE__,
            SVI("struct S { int a; int b; int c; };\n"
               "struct S s = {.b = 2, 3};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@4 = 2, @8 = 3}") },
            },
        },
        {
            "scalar brace init", __LINE__,
            SVI("int x = {42};\n"),
            .vars = {
                { SVI("x"), SVI("int"), SVI("{42}") },
            },
        },
        {
            "empty brace init", __LINE__,
            SVI("struct S { int a; int b; };\n"
               "struct S s = {};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{}") },
            },
        },
        {
            "chained designator", __LINE__,
            SVI("struct Inner { int x; int y; };\n"
               "struct Outer { struct Inner p; };\n"
               "struct Outer s = {.p.x = 1, .p.y = 2};\n"),
            .vars = {
                { SVI("s"), SVI("struct Outer"), SVI("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "implicit cast in init list", __LINE__,
            SVI("struct S { float f; };\n"
               "struct S s = {42};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{(float)42}") },
            },
        },
        {
            "brace elision: array of structs", __LINE__,
            SVI("struct S { int x; };\n"
               "struct S foo[] = {1, {2}, 3};\n"),
            .vars = {
                { SVI("foo"), SVI("struct S[3]"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "extra nested braces on scalar", __LINE__,
            SVI("int x[2] = {1, {{2}}};\n"),
            .vars = {
                { SVI("x"), SVI("int[2]"), SVI("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "brace elision: multi-field struct", __LINE__,
            SVI("struct P { int a; int b; };\n"
               "struct P arr[] = {1, 2, 3, 4};\n"),
            .vars = {
                { SVI("arr"), SVI("struct P[2]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "brace elision: nested structs", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { struct Inner s; int c; };\n"
               "struct Outer o = {1, 2, 3};\n"),
            .vars = {
                { SVI("o"), SVI("struct Outer"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        // --- Torture tests ---
        {
            "trailing comma in init list", __LINE__,
            SVI("int arr[3] = {1, 2, 3,};\n"),
            .vars = {
                { SVI("arr"), SVI("int[3]"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "empty init for array", __LINE__,
            SVI("int arr[3] = {};\n"),
            .vars = {
                { SVI("arr"), SVI("int[3]"), SVI("{}") },
            },
        },
        {
            "empty init for union", __LINE__,
            SVI("union U { int a; float b; };\n"
               "union U u = {};\n"),
            .vars = {
                { SVI("u"), SVI("union U"), SVI("{}") },
            },
        },
        {
            "scalar with extra braces", __LINE__,
            SVI("int x = {{{42}}};\n"),
            .vars = {
                { SVI("x"), SVI("int"), SVI("{42}") },
            },
        },
        {
            "array of arrays", __LINE__,
            SVI("int a[2][3] = {{1, 2, 3}, {4, 5, 6}};\n"),
            .vars = {
                { SVI("a"), SVI("int[2][3]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "array of arrays: brace elision", __LINE__,
            SVI("int a[2][3] = {1, 2, 3, 4, 5, 6};\n"),
            .vars = {
                { SVI("a"), SVI("int[2][3]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "array of arrays: incomplete outer", __LINE__,
            SVI("int a[][3] = {{1, 2, 3}, {4, 5, 6}};\n"),
            .vars = {
                { SVI("a"), SVI("int[2][3]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "array of arrays: incomplete outer + brace elision", __LINE__,
            SVI("int a[][3] = {1, 2, 3, 4, 5, 6};\n"),
            .vars = {
                { SVI("a"), SVI("int[2][3]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "designator then positional in array", __LINE__,
            SVI("int a[5] = {[3] = 30, 40};\n"),
            .vars = {
                { SVI("a"), SVI("int[5]"), SVI("{@12 = 30, @16 = 40}") },
            },
        },
        {
            "last-write-wins in array", __LINE__,
            SVI("int a[3] = {1, 2, 3, [0] = 10};\n"),
            .vars = {
                { SVI("a"), SVI("int[3]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @0 = 10}") },
            },
        },
        {
            "last-write-wins in struct", __LINE__,
            SVI("struct S { int a; int b; };\n"
               "struct S s = {1, 2, .a = 99};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 1, @4 = 2, @0 = 99}") },
            },
        },
        {
            "struct with array member: brace elision", __LINE__,
            SVI("struct S { int a[3]; int b; };\n"
               "struct S s = {1, 2, 3, 4};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "struct with array member: explicit braces", __LINE__,
            SVI("struct S { int a[3]; int b; };\n"
               "struct S s = {{1, 2, 3}, 4};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "nested struct array brace elision", __LINE__,
            SVI("struct P { int x; int y; };\n"
               "struct Line { struct P start; struct P end; };\n"
               "struct Line l = {1, 2, 3, 4};\n"),
            .vars = {
                { SVI("l"), SVI("struct Line"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "array of structs with designated + positional", __LINE__,
            SVI("struct P { int x; int y; };\n"
               "struct P arr[3] = {[1] = {10, 20}, {30, 40}};\n"),
            .vars = {
                { SVI("arr"), SVI("struct P[3]"), SVI("{@8 = 10, @12 = 20, @16 = 30, @20 = 40}") },
            },
        },
        {
            "struct designated init: out of order", __LINE__,
            SVI("struct S { int a; int b; int c; int d; };\n"
               "struct S s = {.d = 4, .b = 2, .c = 3, .a = 1};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@12 = 4, @4 = 2, @8 = 3, @0 = 1}") },
            },
        },
        {
            "union designated second member", __LINE__,
            SVI("union U { int i; float f; double d; };\n"
               "union U u = {.d = 3.14};\n"),
            .vars = {
                { SVI("u"), SVI("union U"), SVI("{3.14}") },
            },
        },
        {
            "union first member implicit", __LINE__,
            SVI("union U { int i; float f; };\n"
               "union U u = {42};\n"),
            .vars = {
                { SVI("u"), SVI("union U"), SVI("{42}") },
            },
        },
        {
            "incomplete array of structs: brace elision", __LINE__,
            SVI("struct P { int x; int y; };\n"
               "struct P arr[] = {1, 2, 3, 4, 5, 6};\n"),
            .vars = {
                { SVI("arr"), SVI("struct P[3]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "chained designator: array in struct", __LINE__,
            SVI("struct S { int a[3]; };\n"
               "struct S s = {.a[1] = 42};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@4 = 42}") },
            },
        },
        {
            "chained designator: struct in array", __LINE__,
            SVI("struct P { int x; int y; };\n"
               "struct P arr[2] = {[0].y = 5, [1].x = 10};\n"),
            .vars = {
                { SVI("arr"), SVI("struct P[2]"), SVI("{@4 = 5, @8 = 10}") },
            },
        },
        {
            "chained designator: deep nesting", __LINE__,
            SVI("struct A { int v; };\n"
               "struct B { struct A a; };\n"
               "struct C { struct B b; };\n"
               "struct C c = {.b.a.v = 99};\n"),
            .vars = {
                { SVI("c"), SVI("struct C"), SVI("{99}") },
            },
        },
        {
            "partial struct init: fewer inits than fields", __LINE__,
            SVI("struct S { int a; int b; int c; int d; };\n"
               "struct S s = {1, 2};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "partial array init: fewer inits than size", __LINE__,
            SVI("int a[10] = {1};\n"),
            .vars = {
                { SVI("a"), SVI("int[10]"), SVI("{1}") },
            },
        },
        {
            "mixed designated and positional struct init", __LINE__,
            SVI("struct S { int a; int b; int c; int d; };\n"
               "struct S s = {.c = 30, 40, .a = 10};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@8 = 30, @12 = 40, @0 = 10}") },
            },
        },
        {
            "single element incomplete array", __LINE__,
            SVI("int a[] = {42};\n"),
            .vars = {
                { SVI("a"), SVI("int[1]"), SVI("{42}") },
            },
        },
        {
            "array of arrays: partial inner", __LINE__,
            SVI("int a[2][3] = {{1}, {4, 5}};\n"),
            .vars = {
                { SVI("a"), SVI("int[2][3]"), SVI("{@0 = 1, @12 = 4, @16 = 5}") },
            },
        },
        {
            "struct with nested struct: designated inner", __LINE__,
            SVI("struct Inner { int x; int y; };\n"
               "struct Outer { struct Inner p; int z; };\n"
               "struct Outer o = {.p = {.y = 2, .x = 1}, .z = 3};\n"),
            .vars = {
                { SVI("o"), SVI("struct Outer"), SVI("{@4 = 2, @0 = 1, @8 = 3}") },
            },
        },
        {
            "struct with union member", __LINE__,
            SVI("union U { int i; float f; };\n"
               "struct S { union U u; int x; };\n"
               "struct S s = {{42}, 7};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 42, @4 = 7}") },
            },
        },
        {
            "struct with union: designated", __LINE__,
            SVI("union U { int i; float f; };\n"
               "struct S { union U u; int x; };\n"
               "struct S s = {.u = {.f = 1.5f}, .x = 7};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 1.5f, @4 = 7}") },
            },
        },
        {
            "brace elision with union in struct", __LINE__,
            SVI("union U { int i; };\n"
               "struct S { union U u; int x; };\n"
               "struct S s = {42, 7};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 42, @4 = 7}") },
            },
        },
        {
            "array: designator at end", __LINE__,
            SVI("int a[5] = {1, 2, [4] = 5};\n"),
            .vars = {
                { SVI("a"), SVI("int[5]"), SVI("{@0 = 1, @4 = 2, @16 = 5}") },
            },
        },
        {
            "array: designator jump backwards", __LINE__,
            SVI("int a[5] = {[4] = 50, [1] = 10};\n"),
            .vars = {
                { SVI("a"), SVI("int[5]"), SVI("{@16 = 50, @4 = 10}") },
            },
        },
        {
            "incomplete array: designated max index", __LINE__,
            SVI("int a[] = {[9] = 99};\n"),
            .vars = {
                { SVI("a"), SVI("int[10]"), SVI("{@36 = 99}") },
            },
        },
        {
            "3d array", __LINE__,
            SVI("int a[2][2][2] = {{{1,2},{3,4}},{{5,6},{7,8}}};\n"),
            .vars = {
                { SVI("a"), SVI("int[2][2][2]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6, @24 = 7, @28 = 8}") },
            },
        },
        {
            "3d array: brace elision", __LINE__,
            SVI("int a[2][2][2] = {1,2,3,4,5,6,7,8};\n"),
            .vars = {
                { SVI("a"), SVI("int[2][2][2]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6, @24 = 7, @28 = 8}") },
            },
        },
        {
            "array of structs: mixed braces", __LINE__,
            SVI("struct P { int x; int y; };\n"
               "struct P arr[] = {{1, 2}, 3, 4, {5, 6}};\n"),
            .vars = {
                { SVI("arr"), SVI("struct P[3]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4, @16 = 5, @20 = 6}") },
            },
        },
        {
            "struct with char array", __LINE__,
            SVI("struct S { int n; char name[4]; };\n"
               "struct S s = {42, {'a', 'b', 'c', 0}};\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = 42, @4 = (char)97, @5 = (char)98, @6 = (char)99, @7 = (char)0}") },
            },
        },
        // --- C standard examples (6.7.9 / 6.7.10) ---
        // EXAMPLE 2
        {
            "std ex2: incomplete array", __LINE__,
            SVI("int x[] = { 1, 3, 5 };\n"),
            .vars = {
                { SVI("x"), SVI("int[3]"), SVI("{@0 = 1, @4 = 3, @8 = 5}") },
            },
        },
        // EXAMPLE 3: 2D array, braced and flat forms
        {
            "std ex3: 2D array braced", __LINE__,
            SVI("int y[4][3] = {\n"
               "  { 1, 3, 5 },\n"
               "  { 2, 4, 6 },\n"
               "  { 3, 5, 7 },\n"
               "};\n"),
            .vars = {
                { SVI("y"), SVI("int[4][3]"), SVI("{@0 = 1, @4 = 3, @8 = 5, @12 = 2, @16 = 4, @20 = 6, @24 = 3, @28 = 5, @32 = 7}") },
            },
        },
        {
            "std ex3: 2D array flat (brace elision)", __LINE__,
            SVI("int y[4][3] = {\n"
               "  1, 3, 5, 2, 4, 6, 3, 5, 7\n"
               "};\n"),
            .vars = {
                { SVI("y"), SVI("int[4][3]"), SVI("{@0 = 1, @4 = 3, @8 = 5, @12 = 2, @16 = 4, @20 = 6, @24 = 3, @28 = 5, @32 = 7}") },
            },
        },
        // EXAMPLE 4: partial inner init
        {
            "std ex4: 2D array partial inner", __LINE__,
            SVI("int z[4][3] = {\n"
               "  { 1 }, { 2 }, { 3 }, { 4 }\n"
               "};\n"),
            .vars = {
                { SVI("z"), SVI("int[4][3]"), SVI("{@0 = 1, @12 = 2, @24 = 3, @36 = 4}") },
            },
        },
        // EXAMPLE 5: struct with array member, brace elision across elements
        {
            "std ex5: struct array member elision", __LINE__,
            SVI("struct W { int a[3]; int b; };\n"
               "struct W w[] = { { 1 }, 2 };\n"),
            .vars = {
                { SVI("w"), SVI("struct W[2]"), SVI("{@0 = 1, @16 = 2}") },
            },
        },
        // EXAMPLE 6: 3D array, three equivalent forms
        {
            "std ex6: 3D array partially braced", __LINE__,
            SVI("int q[4][3][2] = {\n"
               "  { 1 },\n"
               "  { 2, 3 },\n"
               "  { 4, 5, 6 }\n"
               "};\n"),
            .vars = {
                { SVI("q"), SVI("int[4][3][2]"), SVI("{@0 = 1, @24 = 2, @28 = 3, @48 = 4, @52 = 5, @56 = 6}") },
            },
        },
        {
            "std ex6: 3D array flat (brace elision)", __LINE__,
            SVI("int q[4][3][2] = {\n"
               "  1, 0, 0, 0, 0, 0,\n"
               "  2, 3, 0, 0, 0, 0,\n"
               "  4, 5, 6\n"
               "};\n"),
            .vars = {
                { SVI("q"), SVI("int[4][3][2]"), SVI("{@0 = 1, @4 = 0, @8 = 0, @12 = 0, @16 = 0, @20 = 0, @24 = 2, @28 = 3, @32 = 0, @36 = 0, @40 = 0, @44 = 0, @48 = 4, @52 = 5, @56 = 6}") },
            },
        },
        {
            "std ex6: 3D array fully braced", __LINE__,
            SVI("int q[4][3][2] = {\n"
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
                { SVI("q"), SVI("int[4][3][2]"), SVI("{@0 = 1, @24 = 2, @28 = 3, @48 = 4, @52 = 5, @56 = 6}") },
            },
        },
        // EXAMPLE 9: enum constants as designator indices
        {
            "std ex9: enum constant designators", __LINE__,
            SVI("enum { member_one, member_two };\n"
               "int nm[2] = {\n"
               "  [member_two] = 20,\n"
               "  [member_one] = 10,\n"
               "};\n"),
            .vars = {
                { SVI("nm"), SVI("int[2]"), SVI("{@4 = 20, @0 = 10}") },
            },
        },
        // EXAMPLE 10: designated struct init (like div_t)
        {
            "std ex10: designated struct", __LINE__,
            SVI("struct DT { int quot; int rem; };\n"
               "struct DT answer = {.quot = 2, .rem = -1};\n"),
            .vars = {
                { SVI("answer"), SVI("struct DT"), SVI("{@0 = 2, @4 = -1}") },
            },
        },
        // EXAMPLE 11: chained array+field designators
        {
            "std ex11: mixed chained designators", __LINE__,
            SVI("struct W { int a[3]; int b; };\n"
               "struct W w[] = { [0].a = {1}, [1].a[0] = 2 };\n"),
            .vars = {
                { SVI("w"), SVI("struct W[2]"), SVI("{@0 = 1, @16 = 2}") },
            },
        },
        // EXAMPLE 13: designator in middle of positional sequence
        {
            "std ex13: designator mid-sequence", __LINE__,
            SVI("int a[10] = {\n"
               "  1, 3, 5, 7, 9, [5] = 8, 6, 4, 2, 0\n"
               "};\n"),
            .vars = {
                { SVI("a"), SVI("int[10]"), SVI("{@0 = 1, @4 = 3, @8 = 5, @12 = 7, @16 = 9, @20 = 8, @24 = 6, @28 = 4, @32 = 2, @36 = 0}") },
            },
        },
        // EXAMPLE 15: bitfields
        {
            "std ex15: bitfield init", __LINE__,
            SVI("struct BF {\n"
               "  int a:10;\n"
               "  int :12;\n"
               "  long b;\n"
               "};\n"
               "struct BF s = {1, 2};\n"),
            .vars = {
                { SVI("s"), SVI("struct BF"), SVI("{@0:0:10 = 1, @8 = (long)2}") },
            },
        },
        {
            "bitfield: designated init", __LINE__,
            SVI("struct BF { int a:10; int b:6; long c; };\n"
               "struct BF s = {.b = 3, .a = 1};\n"),
            .vars = {
                { SVI("s"), SVI("struct BF"), SVI("{@0:10:6 = 3, @0:0:10 = 1}") },
            },
        },
        {
            "bitfield: multiple in same storage unit", __LINE__,
            SVI("struct BF { int a:3; int b:5; int c:8; int d:16; };\n"
               "struct BF s = {1, 2, 3, 4};\n"),
            .vars = {
                { SVI("s"), SVI("struct BF"), SVI("{@0:0:3 = 1, @0:3:5 = 2, @0:8:8 = 3, @0:16:16 = 4}") },
            },
        },
        {
            "bitfield: spanning storage units", __LINE__,
            SVI("struct BF { int a:20; int b:20; };\n"
               "struct BF s = {1, 2};\n"),
            .vars = {
                { SVI("s"), SVI("struct BF"), SVI("{@0:0:20 = 1, @4:0:20 = 2}") },
            },
        },
        {
            "bitfield: mixed with regular fields", __LINE__,
            SVI("struct BF { int x; int a:3; int b:5; int y; };\n"
               "struct BF s = {10, 1, 2, 20};\n"),
            .vars = {
                { SVI("s"), SVI("struct BF"), SVI("{@0 = 10, @4:0:3 = 1, @4:3:5 = 2, @8 = 20}") },
            },
        },
        {
            "bitfield: skip anonymous padding", __LINE__,
            SVI("struct BF { int a:4; int :4; int b:8; };\n"
               "struct BF s = {3, 7};\n"),
            .vars = {
                { SVI("s"), SVI("struct BF"), SVI("{@0:0:4 = 3, @0:8:8 = 7}") },
            },
        },
        {
            "bitfield: zero-width forces new unit", __LINE__,
            SVI("struct BF { int a:4; int :0; int b:4; };\n"
               "struct BF s = {1, 2};\n"),
            .vars = {
                { SVI("s"), SVI("struct BF"), SVI("{@0:0:4 = 1, @4:0:4 = 2}") },
            },
        },
        {
            "bitfield: nested struct with bitfields", __LINE__,
            SVI("struct Inner { int x:3; int y:5; };\n"
               "struct Outer { int a; struct Inner b; int c; };\n"
               "struct Outer s = {1, {2, 3}, 4};\n"),
            .vars = {
                { SVI("s"), SVI("struct Outer"), SVI("{@0 = 1, @4:0:3 = 2, @4:3:5 = 3, @8 = 4}") },
            },
        },
        {
            "bitfield: designated into nested bitfield", __LINE__,
            SVI("struct Inner { int x:3; int y:5; };\n"
               "struct Outer { int a; struct Inner b; };\n"
               "struct Outer s = {.b.y = 7};\n"),
            .vars = {
                { SVI("s"), SVI("struct Outer"), SVI("{@4:3:5 = 7}") },
            },
        },
        {
            "bitfield: single bit", __LINE__,
            SVI("struct BF { int flag:1; int value:31; };\n"
               "struct BF s = {1, 100};\n"),
            .vars = {
                { SVI("s"), SVI("struct BF"), SVI("{@0:0:1 = 1, @0:1:31 = 100}") },
            },
        },
        {
            "bitfield: unsigned", __LINE__,
            SVI("struct BF { unsigned a:4; unsigned b:4; };\n"
               "struct BF s = {15, 10};\n"),
            .vars = {
                { SVI("s"), SVI("struct BF"), SVI("{@0:0:4 = (unsigned int)15, @0:4:4 = (unsigned int)10}") },
            },
        },
        // EXAMPLE 16: anonymous union in struct (braced form)
        {
            "std ex16: anon union braced", __LINE__,
            SVI("struct AU {\n"
               "  union {\n"
               "    float a;\n"
               "    int b;\n"
               "    void *p;\n"
               "  };\n"
               "  char c;\n"
               "};\n"
               "struct AU s = {{.b = 1}, 2};\n"),
            .vars = {
                { SVI("s"), SVI("struct AU"), SVI("{@0 = 1, @8 = (char)2}") },
            },
        },
        // EXAMPLE 16: anonymous union in struct (designated form)
        {
            "std ex16: anon union designated", __LINE__,
            SVI("struct AU {\n"
               "  union {\n"
               "    float a;\n"
               "    int b;\n"
               "    void *p;\n"
               "  };\n"
               "  char c;\n"
               "};\n"
               "struct AU s = {.b = 1, 2};\n"),
            .vars = {
                { SVI("s"), SVI("struct AU"), SVI("{@0 = 1, @8 = (char)2}") },
            },
        },
        // EXAMPLE 7: typedef incomplete array + multiple declarators
        {
            "std ex7: typedef incomplete array", __LINE__,
            SVI("typedef int A[];\n"
               "A a = { 1, 2 }, b = { 3, 4, 5 };\n"),
            .vars = {
                { SVI("a"), SVI("int[2]"), SVI("{@0 = 1, @4 = 2}") },
                { SVI("b"), SVI("int[3]"), SVI("{@0 = 3, @4 = 4, @8 = 5}") },
            },
        },
        {
            "std ex7: multiple incomplete array decls", __LINE__,
            SVI("int a[] = { 1, 2 }, b[] = { 3, 4, 5 };\n"),
            .vars = {
                { SVI("a"), SVI("int[2]"), SVI("{@0 = 1, @4 = 2}") },
                { SVI("b"), SVI("int[3]"), SVI("{@0 = 3, @4 = 4, @8 = 5}") },
            },
        },
        // EXAMPLE 8: char array init (non-string-literal forms)
        {
            "std ex8: char array brace init", __LINE__,
            SVI("char s[] = { 'a', 'b', 'c', '\\0' },\n"
               "     t[] = { 'a', 'b', 'c' };\n"),
            .vars = {
                { SVI("s"), SVI("char[4]"), SVI("{@0 = (char)97, @1 = (char)98, @2 = (char)99, @3 = (char)0}") },
                { SVI("t"), SVI("char[3]"), SVI("{@0 = (char)97, @1 = (char)98, @2 = (char)99}") },
            },
        },
        {
            "std ex8: char pointer from string", __LINE__,
            SVI("char *p = \"abc\";\n"),
            .vars = {
                { SVI("p"), SVI("char *"), SVI("(char *)\"abc\"") },
            },
        },
        {
            "std ex8: string literal array init", __LINE__,
            SVI("char s[] = \"abc\";\n"),
            .vars = {
                { SVI("s"), SVI("char[4]"), SVI("\"abc\"") },
            },
        },
        {
            "std ex8: string literal sized array", __LINE__,
            SVI("char t[3] = \"abc\";\n"),
            .vars = {
                { SVI("t"), SVI("char[3]"), SVI("\"abc\"") },
            },
        },
        {
            "std ex8: string literal multiple decls", __LINE__,
            SVI("char s[] = \"abc\", t[3] = \"abc\";\n"),
            .vars = {
                { SVI("s"), SVI("char[4]"), SVI("\"abc\"") },
                { SVI("t"), SVI("char[3]"), SVI("\"abc\"") },
            },
        },
        {
            "string literal in struct char array field", __LINE__,
            SVI("struct S { short temp; char pair[201]; };\n"
               "struct S s = { 0, \"abc\" };\n"),
            .vars = {
                { SVI("s"), SVI("struct S"), SVI("{@0 = (short)0, @2 = \"abc\"}") },
            },
        },
        {
            "positional struct var in init", __LINE__,
            SVI("struct T { int a; int b; };\n"
               "struct S { int x; struct T t; };\n"
               "struct T v = {10, 20};\n"
               "struct S s = { 1, v };\n"),
            .vars = {
                { SVI("v"), SVI("struct T"), SVI("{@0 = 10, @4 = 20}") },
                { SVI("s"), SVI("struct S"), SVI("{@0 = 1, @4 = v}") },
            },
        },
        {
            "compound literal in array init", __LINE__,
            SVI("struct SVI { unsigned long length; const char *text; };\n"
               "struct SVI arr[] = { (struct SVI){3, \"abc\"}, (struct SVI){2, \"de\"} };\n"),
            .vars = {
                { SVI("arr"), SVI("struct SVI[2]") },
            },
        },
        {
            "parenthesized compound literal in array init", __LINE__,
            SVI("struct SVI { unsigned long length; const char *text; };\n"
               "struct SVI arr[] = { ((struct SVI){3, \"abc\"}), ((struct SVI){2, \"de\"}) };\n"),
            .vars = {
                { SVI("arr"), SVI("struct SVI[2]") },
            },
        },
        {
            "compound literal in struct init", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer o = { 1, (struct Inner){2, 3} };\n"),
            .vars = {
                { SVI("o"), SVI("struct Outer"), SVI("{@0 = 1, @4 = {@0 = 2, @4 = 3}}") },
            },
        },
        {
            "struct var as first aggregate field", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { struct Inner inner; int c; };\n"
               "struct Inner v = {10, 20};\n"
               "struct Outer o = { v, 3 };\n"),
            .vars = {
                { SVI("v"), SVI("struct Inner"), SVI("{@0 = 10, @4 = 20}") },
                { SVI("o"), SVI("struct Outer"), SVI("{@0 = v, @8 = 3}") },
            },
        },
        {
            "deeply nested brace elision", __LINE__,
            SVI("struct A { int x; };\n"
               "struct B { struct A a; int y; };\n"
               "struct C { struct B b; int z; };\n"
               "struct C c = { 1, 2, 3 };\n"),
            .vars = {
                { SVI("c"), SVI("struct C"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "brace elision with nested struct", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer o = { 1, 2, 3 };\n"),
            .vars = {
                { SVI("o"), SVI("struct Outer"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "braced nested struct init", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer o = { 1, {2, 3} };\n"),
            .vars = {
                { SVI("o"), SVI("struct Outer"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        // EXAMPLE 12: variable reference in initializer + last-write-wins
        {
            "std ex12: struct init with designated", __LINE__,
            SVI("struct T { int k; int l; };\n"
               "struct T x = {.l = 43, .k = 42};\n"),
            .vars = {
                { SVI("x"), SVI("struct T"), SVI("{@4 = 43, @0 = 42}") },
            },
        },
        {
            "std ex12: nested struct init with var ref", __LINE__,
            SVI("struct T { int k; int l; };\n"
               "struct S { int i; struct T t; };\n"
               "struct T x = {.l = 43, .k = 42};\n"
               "struct S l = { 1, .t = x, .t.l = 41};\n"),
            .vars = {
                { SVI("x"), SVI("struct T"), SVI("{@4 = 43, @0 = 42}") },
                { SVI("l"), SVI("struct S"), SVI("{@0 = 1, @4 = x, @8 = 41}") },
            },
        },
        // EXAMPLE 14: union designated init
        {
            "std ex14: union designated", __LINE__,
            SVI("union U { int x; float y; };\n"
               "union U u = {.x = 42};\n"),
            .vars = {
                { SVI("u"), SVI("union U"), SVI("{42}") },
            },
        },
        {
            "method skipping: positional init", __LINE__,
            SVI("struct Foo {\n"
               "  int x;\n"
               "  int get_x(struct Foo* self){ return self->x; }\n"
               "  int y;\n"
               "};\n"
               "struct Foo f = {1, 2};\n"),
            .vars = {
                { SVI("f"), SVI("struct Foo"), SVI("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "method skipping: designated init", __LINE__,
            SVI("struct Foo {\n"
               "  int x;\n"
               "  int get_x(struct Foo* self){ return self->x; }\n"
               "  int y;\n"
               "};\n"
               "struct Foo f = {.y = 10, .x = 20};\n"),
            .vars = {
                { SVI("f"), SVI("struct Foo"), SVI("{@4 = 10, @0 = 20}") },
            },
        },
        {
            "method skipping: brace elision in array", __LINE__,
            SVI("struct Foo {\n"
               "  int x;\n"
               "  int get_x(struct Foo* self){ return self->x; }\n"
               "  int y;\n"
               "};\n"
               "struct Foo arr[] = {1, 2, 3, 4};\n"),
            .vars = {
                { SVI("arr"), SVI("struct Foo[2]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "methods at start and end", __LINE__,
            SVI("struct Bar {\n"
               "  void init(struct Bar* self){}\n"
               "  int a;\n"
               "  int b;\n"
               "  void deinit(struct Bar* self){}\n"
               "};\n"
               "struct Bar b = {10, 20};\n"),
            .vars = {
                { SVI("b"), SVI("struct Bar"), SVI("{@0 = 10, @4 = 20}") },
            },
        },
        {
            "plan9: positional init", __LINE__,
            SVI("struct Base { int x; int y; };\n"
               "struct Derived { struct Base; int z; };\n"
               "struct Derived d = {{1, 2}, 3};\n"),
            .vars = {
                { SVI("d"), SVI("struct Derived"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9: brace elision", __LINE__,
            SVI("struct Base { int x; int y; };\n"
               "struct Derived { struct Base; int z; };\n"
               "struct Derived d = {1, 2, 3};\n"),
            .vars = {
                { SVI("d"), SVI("struct Derived"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9: designated through embed", __LINE__,
            SVI("struct Base { int x; int y; };\n"
               "struct Derived { struct Base; int z; };\n"
               "struct Derived d = {.x = 1, .y = 2, .z = 3};\n"),
            .vars = {
                { SVI("d"), SVI("struct Derived"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9: designated embed as whole", __LINE__,
            SVI("struct Base { int x; int y; };\n"
               "struct Derived { struct Base; int z; };\n"
               "struct Derived d = {{.y = 9, .x = 8}, 7};\n"),
            .vars = {
                { SVI("d"), SVI("struct Derived"), SVI("{@4 = 9, @0 = 8, @8 = 7}") },
            },
        },
        {
            "plan9: array of derived with brace elision", __LINE__,
            SVI("struct Base { int x; };\n"
               "struct Derived { struct Base; int y; };\n"
               "struct Derived arr[] = {1, 2, 3, 4};\n"),
            .vars = {
                { SVI("arr"), SVI("struct Derived[2]"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "plan9: nested embeds", __LINE__,
            SVI("struct A { int a; };\n"
               "struct B { struct A; int b; };\n"
               "struct C { struct B; int c; };\n"
               "struct C val = {1, 2, 3};\n"),
            .vars = {
                { SVI("val"), SVI("struct C"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9: designated through nested embed", __LINE__,
            SVI("struct A { int a; };\n"
               "struct B { struct A; int b; };\n"
               "struct C { struct B; int c; };\n"
               "struct C val = {.a = 1, .b = 2, .c = 3};\n"),
            .vars = {
                { SVI("val"), SVI("struct C"), SVI("{@0 = 1, @4 = 2, @8 = 3}") },
            },
        },
        {
            "plan9 + methods combined", __LINE__,
            SVI("struct Base {\n"
               "  int x;\n"
               "  int get(struct Base* self){ return self->x; }\n"
               "};\n"
               "struct Derived { struct Base; int y; };\n"
               "struct Derived d = {1, 2};\n"),
            .vars = {
                { SVI("d"), SVI("struct Derived"), SVI("{@0 = 1, @4 = 2}") },
            },
        },
        // EXAMPLE 15 union: anonymous bitfield padding in union
        {
            "std ex15: union with anon bitfield", __LINE__,
            SVI("union UB {\n"
               "  int :16;\n"
               "  char c;\n"
               "};\n"
               "union UB u = {3};\n"),
            .vars = {
                { SVI("u"), SVI("union UB"), SVI("{(char)3}") },
            },
        },
        {
            "vector init: full", __LINE__,
            SVI("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {1, 2, 3, 4};\n"),
            .vars = {
                { SVI("v"), SVI("int __attribute__((vector_size(16)))"), SVI("{@0 = 1, @4 = 2, @8 = 3, @12 = 4}") },
            },
        },
        {
            "vector init: partial", __LINE__,
            SVI("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {1, 2};\n"),
            .vars = {
                { SVI("v"), SVI("int __attribute__((vector_size(16)))"), SVI("{@0 = 1, @4 = 2}") },
            },
        },
        {
            "vector init: empty", __LINE__,
            SVI("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {};\n"),
            .vars = {
                { SVI("v"), SVI("int __attribute__((vector_size(16)))"), SVI("{}") },
            },
        },
        {
            "vector init: float elements", __LINE__,
            SVI("typedef float v2f __attribute__((vector_size(8)));\n"
               "v2f v = {1.0f, 2.0f};\n"),
            .vars = {
                { SVI("v"), SVI("float __attribute__((vector_size(8)))"), SVI("{@0 = 1f, @4 = 2f}") },
            },
        },
        {
            "vector: attribute in specifier position", __LINE__,
            SVI("__attribute__((vector_size(16))) int v = {10, 20, 30, 40};\n"),
            .vars = {
                { SVI("v"), SVI("int __attribute__((vector_size(16)))"), SVI("{@0 = 10, @4 = 20, @8 = 30, @12 = 40}") },
            },
        },
        {
            "vector: trailing attribute position", __LINE__,
            SVI("int v __attribute__((vector_size(16))) = {10, 20, 30, 40};\n"),
            .vars = {
                { SVI("v"), SVI("int __attribute__((vector_size(16)))"), SVI("{@0 = 10, @4 = 20, @8 = 30, @12 = 40}") },
            },
        },
        {
            "vector: sizeof", __LINE__,
            SVI("typedef int v4si __attribute__((vector_size(16)));\n"
               "int a[sizeof(v4si)];\n"),
            .vars = {
                { SVI("a"), SVI("int[16]") },
            },
        },
        {
            "vector: alignof", __LINE__,
            SVI("typedef int v4si __attribute__((vector_size(16)));\n"
               "int a[_Alignof(v4si)];\n"),
            .vars = {
                { SVI("a"), SVI("int[16]") },
            },
        },
        {
            "vector: sizeof float", __LINE__,
            SVI("typedef float v2f __attribute__((vector_size(8)));\n"
               "int a[sizeof(v2f)];\n"),
            .vars = {
                { SVI("a"), SVI("int[8]") },
            },
        },
        {
            "vector: alignof capped at max_align", __LINE__,
            SVI("typedef int v8si __attribute__((vector_size(32)));\n"
               "int a[_Alignof(v8si)];\n"),
            .vars = {
                { SVI("a"), SVI("int[16]") },
            },
        },
        {
            "asm label on function", __LINE__,
            SVI("int foo(void) __asm(\"_foo_mangled\");\n"),
            .funcs = {
                { SVI("foo"), SVI("int(void)"), .mangle = SVI("_foo_mangled") },
            },
        },
        {
            "asm label on variable", __LINE__,
            SVI("extern int x __asm__(\"_x_mangled\");\n"),
            .vars = {
                { SVI("x"), SVI("int"), .mangle = SVI("_x_mangled") },
            },
        },
        {
            "asm label with asm keyword", __LINE__,
            SVI("void bar(int a) asm(\"_bar\");\n"),
            .funcs = {
                { SVI("bar"), SVI("void(int)"), .mangle = SVI("_bar") },
            },
        },
        {
            "trailing comma in call", __LINE__,
            SVI("int f(int x, int y);\n"
               "int r = f(1, 2,);\n"),
            .funcs = {
                { SVI("f"), SVI("int(int, int)") },
            },
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("f(1, 2)") },
            },
        },
        {
            "named args", __LINE__,
            SVI("int f(int a, int b);\n"
               "int r = f(.b = 2, .a = 1);\n"),
            .funcs = {
                { SVI("f"), SVI("int(int, int)") },
            },
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("f(1, 2)") },
            },
        },
        {
            "named args mixed with positional", __LINE__,
            SVI("int f(int a, int b, int c);\n"
               "int r = f(1, .c = 3, .b = 2);\n"),
            .funcs = {
                { SVI("f"), SVI("int(int, int, int)") },
            },
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("f(1, 2, 3)") },
            },
        },
        {
            "positional designator args", __LINE__,
            SVI("int f(int a, int b);\n"
               "int r = f([1] = 2, [0] = 1);\n"),
            .funcs = {
                { SVI("f"), SVI("int(int, int)") },
            },
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("f(1, 2)") },
            },
        },
        {
            "mixed positional designator and named", __LINE__,
            SVI("int f(int a, int b, int c);\n"
               "int r = f([2] = 3, .a = 1, [1] = 2);\n"),
            .funcs = {
                { SVI("f"), SVI("int(int, int, int)") },
            },
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("f(1, 2, 3)") },
            },
        },
        {
            "call: int arg to long param", __LINE__,
            SVI("long f(long x);\n"
               "long r = f(42);\n"),
            .funcs = {
                { SVI("f"), SVI("long(long)") },
            },
            .vars = {
                { SVI("r"), SVI("long"), .init = SVI("f((long)42)") },
            },
        },
        {
            "call: matching types, no cast", __LINE__,
            SVI("int f(int x);\n"
               "int r = f(42);\n"),
            .funcs = {
                { SVI("f"), SVI("int(int)") },
            },
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("f(42)") },
            },
        },
        {
            "call: variadic, extra args get default promotions", __LINE__,
            SVI("int f(int x, ...);\n"
               "float g(void);\n"
               "int r = f(1, g());\n"),
            .funcs = {
                { SVI("f"), SVI("int(int, ...)") },
                { SVI("g"), SVI("float(void)") },
            },
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("f(1, (double)g())") },
            },
        },
        {
            "call: char arg promoted to int param", __LINE__,
            SVI("int f(int x);\n"
               "char c;\n"
               "int r = f(c);\n"),
            .funcs = {
                { SVI("f"), SVI("int(int)") },
            },
            .vars = {
                { SVI("c"), SVI("char") },
                { SVI("r"), SVI("int"), .init = SVI("f((int)c)") },
            },
        },
        {
            "lambda: basic immediate call", __LINE__,
            SVI("int r = int(int x, int y){ return x + y; }(3, 4);\n"),
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("<lambda>(3, 4)") },
            },
        },
        {
            "lambda: single param", __LINE__,
            SVI("int r = int(int a){ return a * 2; }(5);\n"),
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("<lambda>(5)") },
            },
        },
        {
            "lambda: void params", __LINE__,
            SVI("int r = int(void){ return 42; }();\n"),
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("<lambda>()") },
            },
        },
        {
            "lambda: in subexpression", __LINE__,
            SVI("int r = 1 + int(int a){ return a; }(10);\n"),
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("(1 + <lambda>(10))") },
            },
        },
        {
            "lambda: typedef return type", __LINE__,
            SVI("typedef int myint;\n"
               "myint r = myint(myint x){ return x; }(7);\n"),
            .typedefs = {
                { SVI("myint"), SVI("int") },
            },
            .vars = {
                { SVI("r"), SVI("int"), .init = SVI("<lambda>(7)") },
            },
        },
        {
            "lambda: implicit cast on arg", __LINE__,
            SVI("long r = long(long x){ return x; }(42);\n"),
            .vars = {
                { SVI("r"), SVI("long"), .init = SVI("<lambda>((long)42)") },
            },
        },
        {
            "offsetof: simple member", __LINE__,
            SVI("struct S { int x; double y; char z; };\n"
               "unsigned long a = __builtin_offsetof(struct S, x);\n"
               "unsigned long b = __builtin_offsetof(struct S, y);\n"
               "unsigned long c = __builtin_offsetof(struct S, z);\n"),
            .vars = {
                { SVI("a"), SVI("unsigned long"), .init = SVI("0") },
                { SVI("b"), SVI("unsigned long"), .init = SVI("8") },
                { SVI("c"), SVI("unsigned long"), .init = SVI("16") },
            },
        },
        {
            "offsetof: nested member", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "unsigned long r = __builtin_offsetof(struct Outer, inner.b);\n"),
            .vars = {
                { SVI("r"), SVI("unsigned long"), .init = SVI("8") },
            },
        },
        {
            "offsetof: array subscript", __LINE__,
            SVI("struct S { int x; int arr[4]; };\n"
               "unsigned long r = __builtin_offsetof(struct S, arr[2]);\n"),
            .vars = {
                { SVI("r"), SVI("unsigned long"), .init = SVI("12") },
            },
        },
        {
            "offsetof: array subscript with nested member", __LINE__,
            SVI("struct Inner { char c; int val; };\n"
               "struct S { int x; struct Inner items[3]; };\n"
               "unsigned long r = __builtin_offsetof(struct S, items[1].val);\n"),
            .vars = {
                { SVI("r"), SVI("unsigned long"), .init = SVI("16") },
            },
        },
        {
            "offsetof: union member", __LINE__,
            SVI("union U { int x; double y; };\n"
               "unsigned long a = __builtin_offsetof(union U, x);\n"
               "unsigned long b = __builtin_offsetof(union U, y);\n"),
            .vars = {
                { SVI("a"), SVI("unsigned long"), .init = SVI("0") },
                { SVI("b"), SVI("unsigned long"), .init = SVI("0") },
            },
        },
        {
            "offsetof: anonymous union member", __LINE__,
            SVI("struct S { int x; union { int a; float b;};};\n"
               "unsigned long ox = __builtin_offsetof(struct S, x);\n"
               "unsigned long oa = __builtin_offsetof(struct S, a);\n"
               "unsigned long ob = __builtin_offsetof(struct S, b);\n"),
            .vars = {
                { SVI("ox"), SVI("unsigned long"), .init = SVI("0")},
                { SVI("oa"), SVI("unsigned long"), .init = SVI("4")},
                { SVI("ob"), SVI("unsigned long"), .init = SVI("4")},
            },
        },
        {
            "static if true branch", __LINE__,
            SVI("static if(1) { int x; }\n"),
            .vars = { { SVI("x"), SVI("int") } },
        },
        {
            "static if false branch skipped", __LINE__,
            SVI("static if(0) { int x; } \n"
               "int y;\n"),
            .vars = { { SVI("y"), SVI("int") } },
        },
        {
            "static if else", __LINE__,
            SVI("static if(0) { int x; } else { int y; }\n"),
            .vars = { { SVI("y"), SVI("int") } },
        },
        {
            "static if else if", __LINE__,
            SVI("static if(0) { int a; }\n"
               "else if(0) { int b; }\n"
               "else if(1) { int c; }\n"
               "else { int d; }\n"),
            .vars = { { SVI("c"), SVI("int") } },
        },
        {
            "static if sizeof condition", __LINE__,
            SVI("static if(sizeof(int) == 4) { int yes; }\n"
               "else { int no; }\n"),
            .vars = { { SVI("yes"), SVI("int") } },
        },
        {
            "static if first wins", __LINE__,
            SVI("static if(1) { int a; }\n"
               "else if(1) { int b; }\n"
               "else { int c; }\n"),
            .vars = { { SVI("a"), SVI("int") } },
        },
        {
            "static if with typedef", __LINE__,
            SVI("static if(sizeof(void*) == 8) { typedef long intptr; }\n"
               "else { typedef int intptr; }\n"),
            .typedefs = { { SVI("intptr"), SVI("long") } },
        },
        {
            "static if no else", __LINE__,
            SVI("static if(0) { int x; }\n"
               "int y;\n"),
            .vars = { { SVI("y"), SVI("int") } },
        },
        {
            "array to const-qualified pointer param", __LINE__,
            SVI("void f(char *const argv[]);\n"
               "char* args[64];\n"
               "f(args);\n"),
        },
        {
            "sizeof compound literal of unspecified length", __LINE__,
            SVI("unsigned long x = sizeof (const char*[]){\"hello\", \"world\"};\n"),
            .vars = {{SVI("x"), SVI("unsigned long"), SVI("16")}},
        },
        {
            "sizeof compound literal of unspecified length with parens", __LINE__,
            SVI("unsigned long x = sizeof((const char*[]){\"hello\", \"world\"});\n"),
            .vars = {{SVI("x"), SVI("unsigned long"), SVI("16")}},
        },
        {
            "sizeof compound literal member", __LINE__,
            SVI("struct Foo { int x; };\n"
               "unsigned long a = sizeof (struct Foo){0}.x;\n"),
            .vars = {{SVI("a"), SVI("unsigned long"), SVI("4")}},
        },
        {
            "unary minus on enum", __LINE__,
            SVI("enum E { A = 1, B = 2 };\n"
               "int x = -A;\n"),
            .vars = {{SVI("x"), SVI("int"), SVI("-1")}},
        },
        {
            "string literal subscript as constant", __LINE__,
            SVI("enum { W = \"Wed\"[0] };\n"
               "int a[\"Hello\"[4] - 'n'];\n"),
            .vars = {{SVI("a"), SVI("int[1]")}},
        },
        {
            "sizeof L string as constant", __LINE__,
            SVI("int a[sizeof(L\"hello\")];\n"),
            .vars = {{SVI("a"), SVI("int[24]")}}, // 6 * 4
        },
        {
            "sizeof u string as constant", __LINE__,
            SVI("int a[sizeof(u\"hello\")];\n"),
            .vars = {{SVI("a"), SVI("int[12]")}}, // 6 * 2
        },
        {
            "sizeof U string as constant", __LINE__,
            SVI("int a[sizeof(U\"hello\")];\n"),
            .vars = {{SVI("a"), SVI("int[24]")}}, // 6 * 4
        },
        {
            "sizeof u8 string as constant", __LINE__,
            SVI("int a[sizeof(u8\"hello\")];\n"),
            .vars = {{SVI("a"), SVI("int[6]")}}, // 6 * 1
        },
        {
            "L string subscript as constant", __LINE__,
            SVI("enum { X = L\"ABC\"[1] };\n"
               "int a[X];\n"),
            .vars = {{SVI("a"), SVI("int[66]")}}, // 'B'
        },
        {
            "u string subscript as constant", __LINE__,
            SVI("enum { X = u\"ABC\"[0] };\n"
               "int a[X];\n"),
            .vars = {{SVI("a"), SVI("int[65]")}}, // 'A'
        },
        {
            "U string subscript as constant", __LINE__,
            SVI("enum { X = U\"ABC\"[2] };\n"
               "int a[X];\n"),
            .vars = {{SVI("a"), SVI("int[67]")}}, // 'C'
        },
        {
            "u8 string subscript as constant", __LINE__,
            SVI("enum { X = u8\"ABC\"[1] };\n"
               "int a[X];\n"),
            .vars = {{SVI("a"), SVI("int[66]")}}, // 'B'
        },
        {
            "L string UCN subscript as constant", __LINE__,
            SVI("enum { X = L\"\\u00E9\"[0] };\n"
               "int a[X];\n"),
            .vars = {{SVI("a"), SVI("int[233]")}}, // 0xE9
        },
        {
            "U string UCN above BMP as constant", __LINE__,
            SVI("_Static_assert(U\"\\U0001F600\"[0] == 0x1F600);\n"),
        },
        {
            "constexpr struct predecl", __LINE__,
            SVI("struct foo {int x;};\n"
               "constexpr struct foo foo = {1};\n"
               "constexpr int x = foo.x;\n"
               "_Static_assert(x);\n"),
            .vars = {
                {SVI("foo"), SVI("const struct foo"), SVI("{1}")},
                {SVI("x"), SVI("const int"), SVI("foo.@0")},
            },
        },
        {
            "constexpr struct", __LINE__,
            SVI("constexpr struct foo {int x;} foo = {1};\n"
               "constexpr int x = foo.x;\n"
               "_Static_assert(x);\n"),
            .vars = {
                {SVI("foo"), SVI("const struct foo"), SVI("{1}")},
                {SVI("x"), SVI("const int"), SVI("foo.@0")},
            },
        },
        {
            "constexpr struct init", __LINE__,
            SVI("constexpr struct foo {int x;} foo = {1};\n"
               "constexpr struct foo b = foo;\n"
               "_Static_assert(b.x);\n"),
            .vars = {
                {SVI("foo"), SVI("const struct foo"), SVI("{1}")},
                {SVI("b"), SVI("const struct foo"), SVI("(struct foo)foo")},
            },
        },
        {
            "constexpr array init", __LINE__,
            SVI("constexpr int arr[] = {1};\n"
               "constexpr int x = arr[0];\n"
               "_Static_assert(x);\n"),
            .vars = {
                {SVI("arr"), SVI("const int[1]"), SVI("{1}")},
                {SVI("x"), SVI("const int"), SVI("(int)arr[0]")},
            },
        },
        // ---- constexpr/value class torture tests ----
        {
            "constexpr: sizeof in constexpr context", __LINE__,
            SVI("int y;\n"
               "constexpr int x = sizeof(y);\n"
               "_Static_assert(x == 4);\n"),
            .vars = {
                {SVI("y"), SVI("int")},
                {SVI("x"), SVI("const int"), SVI("(int)4")},
            },
        },
        {
            "constexpr: alignof in constexpr context", __LINE__,
            SVI("int y;\n"
               "constexpr int x = _Alignof(y);\n"
               "_Static_assert(x == 4);\n"),
            .vars = {
                {SVI("y"), SVI("int")},
                {SVI("x"), SVI("const int"), SVI("(int)4")},
            },
        },
        {
            "constexpr: nested constexpr", __LINE__,
            SVI("constexpr int a = 2;\n"
               "constexpr int b = a * 3;\n"
               "constexpr int c = a + b;\n"
               "_Static_assert(c == 8);\n"),
            .vars = {
                {SVI("a"), SVI("const int"), SVI("2")},
                {SVI("b"), SVI("const int"), SVI("(int)(a * (const int)3)")},
                {SVI("c"), SVI("const int"), SVI("(int)(a + b)")},
            },
        },
        {
            "constexpr: ternary", __LINE__,
            SVI("constexpr int x = 1 ? 42 : 0;\n"
               "_Static_assert(x == 42);\n"),
            .vars = {
                {SVI("x"), SVI("const int")},
            },
        },
        {
            "constexpr: cast", __LINE__,
            SVI("constexpr int x = (int)3.14;\n"
               "_Static_assert(x == 3);\n"),
            .vars = {
                {SVI("x"), SVI("const int"), SVI("(int)3.14")},
            },
        },
        {
            "constexpr: logical ops", __LINE__,
            SVI("constexpr int a = 1 && 0;\n"
               "constexpr int b = 1 || 0;\n"
               "constexpr int c = !0;\n"
               "_Static_assert(!a);\n"
               "_Static_assert(b);\n"
               "_Static_assert(c);\n"),
            .vars = {
                {SVI("a"), SVI("const int")},
                {SVI("b"), SVI("const int")},
                {SVI("c"), SVI("const int")},
            },
        },
        {
            "constexpr: lognot on float/double", __LINE__,
            SVI("_Static_assert(!0.0 == 1);\n"
               "_Static_assert(!1.0 == 0);\n"
               "_Static_assert(!0.0f == 1);\n"
               "_Static_assert(!1.0f == 0);\n"),
        },
        {
            "constexpr: short-circuit logand/logor", __LINE__,
            SVI("_Static_assert(1 || 1/0);\n"
               "_Static_assert(!(0 && 1/0));\n"
               "_Static_assert(!(0 || 0));\n"
               "_Static_assert(1 && 1);\n"),
        },
        {
            "constexpr: float comparison", __LINE__,
            SVI("_Static_assert(1.0 == 1.0);\n"
               "_Static_assert(1.0 != 2.0);\n"
               "_Static_assert(1.0 < 2.0);\n"
               "_Static_assert(2.0 > 1.0);\n"
               "_Static_assert(1.0f == 1.0f);\n"
               "_Static_assert(1.0f < 2.0f);\n"
               "_Static_assert(1.0e30 > 1.0e20);\n"
               "_Static_assert(1.0e30 == 1.0e30);\n"),
        },
        {
            "constexpr: string subscript", __LINE__,
            SVI("constexpr int x = \"abc\"[1];\n"
               "_Static_assert(x == 'b');\n"),
            .vars = {
                {SVI("x"), SVI("const int")},
            },
        },
        {
            "constexpr: enum value in constexpr", __LINE__,
            SVI("enum { A = 10, B = A + 5 };\n"
               "constexpr int x = B;\n"
               "_Static_assert(x == 15);\n"),
            .vars = {
                {SVI("x"), SVI("const int"), SVI("15")},
            },
        },
        {
            "constexpr: bitfield width from constexpr", __LINE__,
            SVI("constexpr int W = 5;\n"
               "struct S { int x : W; };\n"
               "int a[sizeof(struct S)];\n"),
            .vars = {
                {SVI("a"), SVI("int[4]")},
            },
        },
        {
            "constexpr: array dim from constexpr", __LINE__,
            SVI("constexpr int N = 3;\n"
               "int arr[N];\n"
               "_Static_assert(sizeof(arr) == 12);\n"),
            .vars = {
                {SVI("arr"), SVI("int[3]")},
            },
        },
        {
            "constexpr: case label from constexpr arithmetic", __LINE__,
            SVI("constexpr int BASE = 100;\n"
               "int f(int x) {\n"
               "    switch(x) {\n"
               "        case BASE + 1: return 1;\n"
               "        case BASE + 2: return 2;\n"
               "        default: return 0;\n"
               "    }\n"
               "}\n"),
            .funcs = { {SVI("f"), SVI("int(int)")} },
        },
        {
            "static local: literal init", __LINE__,
            SVI("void f(void) { static int x = 42; }\n"),
            .funcs = { {SVI("f"), SVI("void(void)")} },
        },
        {
            "static local: address of global via array decay", __LINE__,
            SVI("int arr[3];\n"
               "void f(void) { static int* p = arr; }\n"),
            .funcs = { {SVI("f"), SVI("void(void)")} },
        },
        {
            "static local: string literal", __LINE__,
            SVI("void f(void) { static const char* s = \"hello\"; }\n"),
            .funcs = { {SVI("f"), SVI("void(void)")} },
        },
        {
            "static local: constexpr value", __LINE__,
            SVI("constexpr int N = 42;\n"
               "void f(void) { static int x = N; }\n"),
            .funcs = { {SVI("f"), SVI("void(void)")} },
        },
        {
            "constexpr: nested struct member", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { struct Inner i; int c; };\n"
               "constexpr struct Outer o = {{1, 2}, 3};\n"
               "_Static_assert(o.c == 3);\n"
               "_Static_assert(o.i.a == 1);\n"
               "_Static_assert(o.i.b == 2);\n"),
            .vars = {
                {SVI("o"), SVI("const struct Outer")},
            },
        },
        {
            "constexpr: array of structs", __LINE__,
            SVI("struct P { int x; int y; };\n"
               "constexpr struct P pts[] = {{1,2},{3,4}};\n"
               "_Static_assert(pts[0].x == 1);\n"
               "_Static_assert(pts[1].y == 4);\n"),
            .vars = {
                {SVI("pts"), SVI("const struct P[2]")},
            },
        },
        {
            "constexpr: char array from string literal", __LINE__,
            SVI("constexpr char s[] = \"hello\";\n"
               "_Static_assert(s[0] == 'h');\n"
               "_Static_assert(s[4] == 'o');\n"
               "_Static_assert(s[5] == 0);\n"),
            .vars = {
                {SVI("s"), SVI("const char[6]")},
            },
        },
        {
            "constexpr: struct with char array from string", __LINE__,
            SVI("struct Name { char data[4]; };\n"
               "constexpr struct Name names[] = {{\"abc\"}, {\"xyz\"}};\n"
               "_Static_assert(names[0].data[0] == 'a');\n"
               "_Static_assert(names[0].data[2] == 'c');\n"
               "_Static_assert(names[1].data[0] == 'x');\n"
               "_Static_assert(names[1].data[2] == 'z');\n"),
            .vars = {
                {SVI("names"), SVI("const struct Name[2]")},
            },
        },
        {
            "constexpr: nested arrays", __LINE__,
            SVI("constexpr int m[2][3] = {{1,2,3},{4,5,6}};\n"
               "_Static_assert(m[0][0] == 1);\n"
               "_Static_assert(m[0][2] == 3);\n"
               "_Static_assert(m[1][0] == 4);\n"
               "_Static_assert(m[1][2] == 6);\n"),
            .vars = {
                {SVI("m"), SVI("const int[2][3]")},
            },
        },
        {
            "constexpr: deep chained access", __LINE__,
            SVI("struct V { int v[2]; };\n"
               "struct Row { struct V cols[2]; };\n"
               "constexpr struct Row grid[] = {\n"
               "    {{{10, 11}, {12, 13}}},\n"
               "    {{{20, 21}, {22, 23}}},\n"
               "};\n"
               "_Static_assert(grid[0].cols[0].v[0] == 10);\n"
               "_Static_assert(grid[0].cols[0].v[1] == 11);\n"
               "_Static_assert(grid[0].cols[1].v[0] == 12);\n"
               "_Static_assert(grid[1].cols[0].v[0] == 20);\n"
               "_Static_assert(grid[1].cols[1].v[1] == 23);\n"),
            .vars = {
                {SVI("grid"), SVI("const struct Row[2]")},
            },
        },
        {
            "constexpr: bitnot truncation", __LINE__,
            SVI("_Static_assert((unsigned long long)(~0u) == 0xFFFFFFFFull);\n"
               "_Static_assert((unsigned long long)(~1u) == 0xFFFFFFFEull);\n"),
        },
        {
            "constexpr: unsigned neg truncation", __LINE__,
            SVI("_Static_assert((unsigned long long)(-(unsigned)1) == 0xFFFFFFFFull);\n"
               "_Static_assert((unsigned long long)(-(unsigned)0) == 0ull);\n"),
        },
        {
            "msvc suffix i8", __LINE__,
            SVI("_Static_assert(42i8 == 42);\n"),
        },
        {
            "msvc suffix i16", __LINE__,
            SVI("_Static_assert(1000i16 == 1000);\n"),
        },
        {
            "msvc suffix i32", __LINE__,
            SVI("_Static_assert(100000i32 == 100000);\n"),
        },
        {
            "msvc suffix i64", __LINE__,
            SVI("_Static_assert(9223372036854775807i64 == 9223372036854775807LL);\n"),
        },
        {
            "msvc suffix ui64", __LINE__,
            SVI("_Static_assert(0xFFFFFFFFFFFFFFFFui64 == 0xFFFFFFFFFFFFFFFFULL);\n"),
        },
        {
            "msvc suffix ui32", __LINE__,
            SVI("_Static_assert(0xFFFFFFFFui32 == 0xFFFFFFFFU);\n"),
        },
        {
            "msvc suffix hex i32", __LINE__,
            SVI("_Static_assert(0xDeAdBeEfi32 == 0xDeAdBeEf);\n"),
        },
        {
            "msvc suffix octal i32", __LINE__,
            SVI("_Static_assert(077i32 == 63);\n"),
        },
        {
            "msvc suffix I64 uppercase", __LINE__,
            SVI("_Static_assert(42I64 == 42LL);\n"),
        },
        {
            "msvc suffix in expression", __LINE__,
            SVI("_Static_assert(1i64 + 2i64 == 3);\n"),
        },
        {
            "constexpr: left shift negative", __LINE__,
            SVI("_Static_assert((-1 << 1) == -2);\n"
               "_Static_assert((-1 << 0) == -1);\n"
               "_Static_assert((-100 << 2) == -400);\n"),
        },
        {
            "constexpr: init_list not freed via comma", __LINE__,
            SVI("struct P { int x; int y; };\n"
               "constexpr struct P p = {10, 20};\n"
               "_Static_assert((p, 1));\n"
               "_Static_assert(p.x == 10);\n"
               "_Static_assert(p.y == 20);\n"),
            .vars = {
                {SVI("p"), SVI("const struct P")},
            },
        },
        // printf format checking: valid cases
        {
            "printf: basic %d", __LINE__,
            SVI("void f(void) { printf(\"%d\", 42); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %s with string literal", __LINE__,
            SVI("void f(void) { printf(\"%s\", \"hello\"); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %s with char*", __LINE__,
            SVI("void f(char* s) { printf(\"%s\", s); }\n"),
            .funcs = { { SVI("f"), SVI("void(char *)") } },
        },
        {
            "printf: %s with const char*", __LINE__,
            SVI("void f(const char* s) { printf(\"%s\", s); }\n"),
            .funcs = { { SVI("f"), SVI("void(const char *)") } },
        },
        {
            "printf: %p with pointer", __LINE__,
            SVI("void f(int* p) { printf(\"%p\", p); }\n"),
            .funcs = { { SVI("f"), SVI("void(int *)") } },
        },
        {
            "printf: %p with void*", __LINE__,
            SVI("void f(void* p) { printf(\"%p\", p); }\n"),
            .funcs = { { SVI("f"), SVI("void(void *)") } },
        },
        {
            "printf: %%", __LINE__,
            SVI("void f(void) { printf(\"100%%\"); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: no format args", __LINE__,
            SVI("void f(void) { printf(\"hello world\"); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: multiple specifiers", __LINE__,
            SVI("void f(void) { printf(\"%d %s %u\", 1, \"hi\", 2u); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %ld with long", __LINE__,
            SVI("void f(long x) { printf(\"%ld\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(long)") } },
        },
        {
            "printf: %lld with long long", __LINE__,
            SVI("void f(long long x) { printf(\"%lld\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(long long)") } },
        },
        {
            "printf: %zu with size_t", __LINE__,
            SVI("void f(unsigned long x) { printf(\"%zu\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(unsigned long)") } },
        },
        {
            "printf: %x with unsigned", __LINE__,
            SVI("void f(unsigned x) { printf(\"%x\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(unsigned int)") } },
        },
        {
            "printf: %o with unsigned", __LINE__,
            SVI("void f(unsigned x) { printf(\"%o\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(unsigned int)") } },
        },
        {
            "printf: %f with double", __LINE__,
            SVI("void f(double x) { printf(\"%f\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(double)") } },
        },
        {
            "printf: %e with double", __LINE__,
            SVI("void f(double x) { printf(\"%e\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(double)") } },
        },
        {
            "printf: %g with double", __LINE__,
            SVI("void f(double x) { printf(\"%g\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(double)") } },
        },
        {
            "printf: %c with int", __LINE__,
            SVI("void f(int c) { printf(\"%c\", c); }\n"),
            .funcs = { { SVI("f"), SVI("void(int)") } },
        },
        {
            "printf: %*d with width", __LINE__,
            SVI("void f(void) { printf(\"%*d\", 10, 42); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %.*s with precision", __LINE__,
            SVI("void f(void) { printf(\"%.*s\", 5, \"hello\"); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %*.*f", __LINE__,
            SVI("void f(double x) { printf(\"%*.*f\", 10, 3, x); }\n"),
            .funcs = { { SVI("f"), SVI("void(double)") } },
        },
        {
            "printf: flags and width", __LINE__,
            SVI("void f(void) { printf(\"%-+10d\", 42); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %hd (short promoted to int)", __LINE__,
            SVI("void f(int x) { printf(\"%hd\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(int)") } },
        },
        {
            "printf: %hhd (char promoted to int)", __LINE__,
            SVI("void f(int x) { printf(\"%hhd\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(int)") } },
        },
        {
            "printf: %lx with unsigned long", __LINE__,
            SVI("void f(unsigned long x) { printf(\"%lx\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(unsigned long)") } },
        },
        {
            "printf: %llx with unsigned long long", __LINE__,
            SVI("void f(unsigned long long x) { printf(\"%llx\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(unsigned long long)") } },
        },
        {
            "printf: non-literal format skipped", __LINE__,
            SVI("void f(const char* fmt) { printf(fmt, 1); }\n"),
            .funcs = { { SVI("f"), SVI("void(const char *)") } },
        },
        {
            "printf: constexpr format string", __LINE__,
            SVI("constexpr const char* fmt = \"%d\";\n"
               "void f(void) { printf(fmt, 42); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %b binary", __LINE__,
            SVI("void f(unsigned x) { printf(\"%b\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(unsigned int)") } },
        },
        {
            "printf: stb flags ' $ _", __LINE__,
            SVI("void f(void) { printf(\"%'d\", 1000); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %I64d", __LINE__,
            SVI("void f(long long x) { printf(\"%I64d\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(long long)") } },
        },
        {
            "printf: %I32u", __LINE__,
            SVI("void f(unsigned x) { printf(\"%I32u\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(unsigned int)") } },
        },
        {
            "snprintf: valid", __LINE__,
            SVI("void f(char* buf) { snprintf(buf, 100, \"%d %s\", 42, \"hi\"); }\n"),
            .funcs = { { SVI("f"), SVI("void(char *)") } },
        },
        {
            "printf: float promotes to double for %f", __LINE__,
            SVI("void f(float x) { printf(\"%f\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(float)") } },
        },
        {
            "printf: %lf (l has no effect on float)", __LINE__,
            SVI("void f(double x) { printf(\"%lf\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(double)") } },
        },
        {
            "printf: %Lf with long double", __LINE__,
            SVI("void f(long double x) { printf(\"%Lf\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(long double)") } },
        },
        {
            "printf: %lc with int (wint_t)", __LINE__,
            SVI("void f(int c) { printf(\"%lc\", c); }\n"),
            .funcs = { { SVI("f"), SVI("void(int)") } },
        },
        {
            "printf: %ls with int* (wchar_t*)", __LINE__,
            SVI("void f(int* s) { printf(\"%ls\", s); }\n"),
            .funcs = { { SVI("f"), SVI("void(int *)") } },
        },
        {
            "printf: %le (l no effect on float)", __LINE__,
            SVI("void f(double x) { printf(\"%le\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(double)") } },
        },
        {
            "printf: %La with long double", __LINE__,
            SVI("void f(long double x) { printf(\"%La\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(long double)") } },
        },
        {
            "printf: %zd with signed size type", __LINE__,
            SVI("void f(long x) { printf(\"%zd\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(long)") } },
        },
        {
            "printf: %td with ptrdiff_t", __LINE__,
            SVI("void f(long x) { printf(\"%td\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(long)") } },
        },
        {
            "printf: %jd with intmax_t", __LINE__,
            SVI("void f(long long x) { printf(\"%jd\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(long long)") } },
        },
        {
            "printf: %ld with long long (same size)", __LINE__,
            SVI("void f(long long x) { printf(\"%ld\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(long long)") } },
        },
        {
            "printf: %llu with unsigned long (same size)", __LINE__,
            SVI("void f(unsigned long x) { printf(\"%llu\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(unsigned long)") } },
        },
        {
            "printf: %d with enum of fixed underlying int", __LINE__,
            SVI("enum E : int { A = 1 };\n"
               "void f(void) { printf(\"%d\", A); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %u with enum of fixed underlying unsigned", __LINE__,
            SVI("enum E : unsigned { A = 1 };\n"
               "void f(void) { printf(\"%u\", A); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %d with enum variable", __LINE__,
            SVI("enum E : int { A = 1 };\n"
               "void f(enum E e) { printf(\"%d\", e); }\n"),
            .funcs = { { SVI("f"), SVI("void(enum E)") } },
        },
        {
            "printf: %d with enum no fixed type", __LINE__,
            SVI("enum E { A, B, C };\n"
               "void f(void) { printf(\"%d\", A); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %d with _Bool", __LINE__,
            SVI("void f(_Bool b) { printf(\"%d\", b); }\n"),
            .funcs = { { SVI("f"), SVI("void(_Bool)") } },
        },
        {
            "printf: %x with unsigned enum", __LINE__,
            SVI("enum E : unsigned { A = 1 };\n"
               "void f(void) { printf(\"%x\", A); }\n"),
            .funcs = { { SVI("f"), SVI("void(void)") } },
        },
        {
            "printf: %d with unsigned (same size)", __LINE__,
            SVI("void f(unsigned x) { printf(\"%d\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(unsigned int)") } },
        },
        {
            "printf: %u with int (same size)", __LINE__,
            SVI("void f(int x) { printf(\"%u\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(int)") } },
        },
        {
            "printf: %b with int (same size)", __LINE__,
            SVI("void f(int x) { printf(\"%b\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(int)") } },
        },
        {
            "printf: %x with int (same size)", __LINE__,
            SVI("void f(int x) { printf(\"%x\", x); }\n"),
            .funcs = { { SVI("f"), SVI("void(int)") } },
        },
        {
            "printf attr: udf valid", __LINE__,
            SVI("__attribute__((format(printf, 1, 2)))\n"
               "void my_printf(const char* fmt, ...);\n"
               "void f(void) { my_printf(\"%d %s\", 42, \"hi\"); }\n"),
            .funcs = {
                { SVI("my_printf"), SVI("void(const char *, ...)") },
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "printf attr: udf valid with prefix args", __LINE__,
            SVI("__attribute__((format(printf, 2, 3)))\n"
               "void log_msg(int level, const char* fmt, ...);\n"
               "void f(void) { log_msg(1, \"%d\", 42); }\n"),
            .funcs = {
                { SVI("log_msg"), SVI("void(int, const char *, ...)") },
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "static fwd decl then bare definition", __LINE__,
            SVI("static void foo(void);\n"
               "void foo(void){}\n"),
            .funcs = {
                { SVI("foo"), SVI("void(void)") },
            },
        },
        {
            "__has_builtin", __LINE__,
            SVI("#if __has_builtin(__builtin_expect)\n"
               "int has_expect;\n"
               "#endif\n"
               "#if __has_builtin(__builtin_trap)\n"
               "int has_trap;\n"
               "#endif\n"
               "#if __has_builtin(__builtin_nope)\n"
               "int has_nope;\n"
               "#endif\n"
               "#if __has_builtin(__atomic_load_n)\n"
               "int has_atomic;\n"
               "#endif\n"),
            .vars = {
                { SVI("has_expect"), SVI("int") },
                { SVI("has_trap"), SVI("int") },
                { SVI("has_atomic"), SVI("int") },
            },
        },
        {
            "[[nodiscard]] on function", __LINE__,
            SVI("[[nodiscard]] int important_func(void);\n"),
            .funcs = {
                { SVI("important_func"), SVI("int(void)") },
            },
        },
        {
            "[[nodiscard]] on struct", __LINE__,
            SVI("struct [[nodiscard]] error_info { int code; };\n"
               "struct error_info enable_missile_safety_mode(void);\n"),
            .funcs = {
                { SVI("enable_missile_safety_mode"), SVI("struct error_info(void)") },
            },
        },
        {
            "[[nodiscard]] with message", __LINE__,
            SVI("[[nodiscard(\"armer needs to check armed state\")]]\n"
               "_Bool arm_detonator(int within);\n"),
            .funcs = {
                { SVI("arm_detonator"), SVI("_Bool(int)") },
            },
        },
        {
            "[[deprecated]] on function", __LINE__,
            SVI("[[deprecated]] void old_api(void);\n"),
            .funcs = {
                { SVI("old_api"), SVI("void(void)") },
            },
        },
        {
            "[[deprecated]] with message on function", __LINE__,
            SVI("[[deprecated(\"use new_api\")]] void old_api(void);\n"),
            .funcs = {
                { SVI("old_api"), SVI("void(void)") },
            },
        },
        {
            "[[deprecated]] on struct", __LINE__,
            SVI("struct [[deprecated]] S { int a; };\n"
               "struct S s;\n"),
            .vars = {
                { SVI("s"), SVI("struct S") },
            },
        },
        {
            "[[deprecated]] on enum", __LINE__,
            SVI("enum [[deprecated]] E1 { one };\n"
               "int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "[[deprecated]] on enumerator", __LINE__,
            SVI("enum E2 {\n"
               "    two [[deprecated(\"use 'three' instead\")]],\n"
               "    three\n"
               "};\n"
               "int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "[[deprecated]] on typedef", __LINE__,
            SVI("[[deprecated]] typedef int Foo;\n"
               "int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
            .typedefs = {
                { SVI("Foo"), SVI("int") },
            },
        },
        {
            "[[maybe_unused]] on function and param", __LINE__,
            SVI("[[maybe_unused]] void f([[maybe_unused]] int i);\n"),
            .funcs = {
                { SVI("f"), SVI("void(int)") },
            },
        },
        {
            "[[maybe_unused]] on variable", __LINE__,
            SVI("void f(void) {\n"
               "    [[maybe_unused]] int j = 100;\n"
               "}\n"),
            .funcs = {
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "[[noreturn]] on function", __LINE__,
            SVI("[[noreturn]] void f(void);\n"),
            .funcs = {
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "[[noreturn]] on function def", __LINE__,
            SVI("[[noreturn]] void f(void) { __builtin_trap(); }\n"),
            .funcs = {
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "[[__noreturn__]] alternate spelling", __LINE__,
            SVI("[[__noreturn__]] void f(void);\n"),
            .funcs = {
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "[[_Noreturn]] obsolescent spelling", __LINE__,
            SVI("[[_Noreturn]] void f(void);\n"),
            .funcs = {
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "empty [[]] attribute", __LINE__,
            SVI("[[ ]] int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "spaced [[ ]] attribute brackets", __LINE__,
            SVI("[ [ noreturn ] ] void f(void) { __builtin_trap(); }\n"),
            .funcs = {
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "multiple attributes in one specifier", __LINE__,
            SVI("[[deprecated, nodiscard]] int f(void);\n"),
            .funcs = {
                { SVI("f"), SVI("int(void)") },
            },
        },
        {
            "multiple attribute specifiers", __LINE__,
            SVI("[[deprecated]] [[nodiscard]] int f(void);\n"),
            .funcs = {
                { SVI("f"), SVI("int(void)") },
            },
        },
        {
            "[[gnu::noreturn]]", __LINE__,
            SVI("[[gnu::noreturn]] void die(void);\n"),
            .funcs = {
                { SVI("die"), SVI("void(void)") },
            },
        },
        {
            "vendor attribute prefix", __LINE__,
            SVI("[[gnu::unused]] int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "[[reproducible]] on function type", __LINE__,
            SVI("unsigned long hash(const char *s) [[reproducible]];\n"),
            .funcs = {
                { SVI("hash"), SVI("unsigned long(const char *)") },
            },
        },
        {
            "[[unsequenced]] on function type", __LINE__,
            SVI("_Bool tendency(signed char) [[unsequenced]];\n"),
            .funcs = {
                { SVI("tendency"), SVI("_Bool(signed char)") },
            },
        },
        {
            "attribute on struct member", __LINE__,
            SVI("struct S {\n"
               "    [[deprecated]] int old_field;\n"
               "    int new_field;\n"
               "};\n"
               "struct S s;\n"),
            .vars = {
                { SVI("s"), SVI("struct S") },
            },
        },
        {
            "[[fallthrough]] in switch", __LINE__,
            SVI("int f(int n) {\n"
               "    switch (n) {\n"
               "    case 1:\n"
               "        [[fallthrough]];\n"
               "    case 2:\n"
               "        return 1;\n"
               "    default:\n"
               "        return 0;\n"
               "    }\n"
               "}\n"),
            .funcs = {
                { SVI("f"), SVI("int(int)") },
            },
        },
        {
            "__attribute__((fallthrough)) in switch", __LINE__,
            SVI("int f(int n) {\n"
               "    switch (n) {\n"
               "    case 1:\n"
               "        __attribute__((fallthrough));\n"
               "    case 2:\n"
               "        return 1;\n"
               "    default:\n"
               "        return 0;\n"
               "    }\n"
               "}\n"),
            .funcs = {
                { SVI("f"), SVI("int(int)") },
            },
        },
        {
            "attribute on pointer declarator", __LINE__,
            SVI("int * [[]] p;\n"),
            .vars = {
                { SVI("p"), SVI("int *") },
            },
        },
        {
            "attribute on label", __LINE__,
            SVI("void f(void) {\n"
               "    [[maybe_unused]] label:\n"
               "    return;\n"
               "}\n"),
            .funcs = {
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "typeof basic", __LINE__,
            SVI("int x;\n"
               "typeof(x) y;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
                { SVI("y"), SVI("int") },
            },
        },
        {
            "typeof pointer", __LINE__,
            SVI("int *p;\n"
               "typeof(p) q;\n"),
            .vars = {
                { SVI("p"), SVI("int *") },
                { SVI("q"), SVI("int *") },
            },
        },
        {
            "typeof_unqual strips const", __LINE__,
            SVI("const int x = 0;\n"
               "typeof_unqual(x) y;\n"),
            .vars = {
                { SVI("x"), SVI("const int") },
                { SVI("y"), SVI("int") },
            },
        },
        {
            "_Atomic type specifier", __LINE__,
            SVI("_Atomic int x;\n"),
            .vars = {
                { SVI("x"), SVI("_Atomic int") },
            },
            .skip = 1, // _Atomic parsing unimplemented
        },
        {
            "_Atomic with parens", __LINE__,
            SVI("_Atomic(int) x;\n"),
            .vars = {
                { SVI("x"), SVI("_Atomic int") },
            },
            .skip = 1, // _Atomic parsing unimplemented
        },
        {
            "alignas on variable", __LINE__,
            SVI("alignas(16) int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "_Alignas on variable", __LINE__,
            SVI("_Alignas(8) int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "constexpr variable", __LINE__,
            SVI("constexpr int x = 42;\n"),
            .vars = {
                { SVI("x"), SVI("const int") },
            },
        },
        {
            "thread_local variable", __LINE__,
            SVI("thread_local int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "_Thread_local variable", __LINE__,
            SVI("_Thread_local int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "__auto_type variable", __LINE__,
            SVI("__auto_type x = 42;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "struct with bitfields", __LINE__,
            SVI("struct S { unsigned a : 3; unsigned b : 5; };\n"
               "struct S s;\n"),
            .vars = {
                { SVI("s"), SVI("struct S") },
            },
        },
        {
            "struct with anonymous bitfield", __LINE__,
            SVI("struct S { unsigned a : 3; unsigned : 5; unsigned b : 8; };\n"
               "struct S s;\n"),
            .vars = {
                { SVI("s"), SVI("struct S") },
            },
        },
        {
            "enum with underlying type", __LINE__,
            SVI("enum E : unsigned char { A, B, C };\n"
               "enum E e;\n"),
            .vars = {
                { SVI("e"), SVI("enum E") },
            },
        },
        {
            "_Generic in function", __LINE__,
            SVI("int f(int x) {\n"
               "    return _Generic(x, int: 1, float: 2, default: 3);\n"
               "}\n"),
            .funcs = {
                { SVI("f"), SVI("int(int)") },
            },
        },
        {
            "_Float16 variable", __LINE__,
            SVI("_Float16 x;\n"),
            .vars = {
                { SVI("x"), SVI("_Float16") },
            },
        },
        {
            "bool variable", __LINE__,
            SVI("_Bool x;\n"),
            .vars = {
                { SVI("x"), SVI("_Bool") },
            },
        },
        {
            "signed char variable", __LINE__,
            SVI("signed char x;\n"),
            .vars = {
                { SVI("x"), SVI("signed char") },
            },
        },
        {
            "unsigned char variable", __LINE__,
            SVI("unsigned char x;\n"),
            .vars = {
                { SVI("x"), SVI("unsigned char") },
            },
        },
        {
            "unsigned short variable", __LINE__,
            SVI("unsigned short x;\n"),
            .vars = {
                { SVI("x"), SVI("unsigned short") },
            },
        },
        {
            "unsigned long variable", __LINE__,
            SVI("unsigned long x;\n"),
            .vars = {
                { SVI("x"), SVI("unsigned long") },
            },
        },
        {
            "unsigned long long variable", __LINE__,
            SVI("unsigned long long x;\n"),
            .vars = {
                { SVI("x"), SVI("unsigned long long") },
            },
        },
        {
            "long long variable", __LINE__,
            SVI("long long x;\n"),
            .vars = {
                { SVI("x"), SVI("long long") },
            },
        },
        {
            "long double variable", __LINE__,
            SVI("long double x;\n"),
            .vars = {
                { SVI("x"), SVI("long double") },
            },
        },
        {
            "__int128 variable", __LINE__,
            SVI("__int128 x;\n"),
            .vars = {
                { SVI("x"), SVI("__int128") },
            },
        },
        {
            "unsigned __int128 variable", __LINE__,
            SVI("unsigned __int128 x;\n"),
            .vars = {
                { SVI("x"), SVI("unsigned __int128") },
            },
        },
        {
            "register variable", __LINE__,
            SVI("void f(void){ register int x = 0; }\n"),
            .funcs = {
                { SVI("f"), SVI("void(void)") },
            },
        },
        {
            "restrict pointer", __LINE__,
            SVI("int * restrict p;\n"),
            .vars = {
                { SVI("p"), SVI("int *") },
            },
        },
        {
            "volatile variable", __LINE__,
            SVI("volatile int x;\n"),
            .vars = {
                { SVI("x"), SVI("volatile int") },
            },
        },
        {
            "static variable", __LINE__,
            SVI("static int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "extern variable", __LINE__,
            SVI("extern int x;\n"),
            .vars = {
                { SVI("x"), SVI("int") },
            },
        },
        {
            "pragma pack struct", __LINE__,
            SVI("#pragma pack(1)\n"
               "struct S { char a; int b; };\n"
               "#pragma pack()\n"
               "struct S s;\n"),
            .vars = {
                { SVI("s"), SVI("struct S") },
            },
        },
        {
            "__declspec align struct", __LINE__,
            SVI("struct __declspec(align(32)) S { int x; };\n"
               "struct S s;\n"),
            .vars = {
                { SVI("s"), SVI("struct S") },
            },
            .skip = 0,
        },
        {
            "struct with method", __LINE__,
            SVI("struct S {\n"
               "    int x;\n"
               "    int get(struct S* self){ return self->x; }\n"
               "};\n"
               "struct S s;\n"),
            .vars = {
                { SVI("s"), SVI("struct S") },
            },
        },
        {
            "constexpr as typedef", __LINE__,
            SVI("constexpr _Type T = int;\n"
                "T x = 3;\n"),
            .vars = {
                { SVI("x"), SVI("int"), SVI("3")},
            },
        },
        {
            "constexpr infer as typedef", __LINE__,
            SVI("constexpr T = int;\n"
                "T x = 3;\n"),
            .vars = {
                { SVI("x"), SVI("int"), SVI("3")},
            },
        },
        {
            "tag and typedef same name", __LINE__,
            SVI("struct S {int x;};\n"
                "typedef int S;\n"
                "S y = 3;\n"
                "struct S s;\n"),
            .vars = {
                { SVI("y"), SVI("int"), SVI("3") },
                { SVI("s"), SVI("struct S") },
            },
            .typedefs = {
                { SVI("S"), SVI("int") },
            },
        },
        {
            "identical typedef redecl", __LINE__,
            SVI("typedef int T;\n"
                "typedef int T;\n"
                "T x = 5;\n"),
            .vars = {
                { SVI("x"), SVI("int"), SVI("5") },
            },
            .typedefs = {
                { SVI("T"), SVI("int") },
            },
        },
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < arrlen(testcases); i = test_atomic_increment(&idx)){
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
            .eager_parsing = 1,
        };
        struct Case* c = &testcases[i];
        fc_write_path(fc, "(test)", 6);
        err = fc_cache_file(fc, c->input);
        if(err) {TestPrintf("%s:%d: failed to cache\n", __FILE__, c->line); goto finally;}
        err = cpp_define_builtin_macros(&cc.cpp);
        if(err) {TestPrintf("%s:%d: failed to define\n", __FILE__, c->line); goto finally;}
        err = cc_define_builtin_types(&cc);
        if(err) {TestPrintf("%s:%d: failed to define builtin types\n", __FILE__, c->line); goto finally;}
        err = cpp_include_file_via_file_cache(&cc.cpp, SV("(test)"));
        if(err) {TestPrintf("%s:%d: failed to include\n", __FILE__, c->line); goto finally;}
        err = cc_parse_all(&cc);
        if(err) {TestPrintf("%s:%d: failed to parse\n", __FILE__, c->line); goto finally;}
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
    static struct ErrorCase {
        const char* test; int line;
        StringView input;
        StringView expected_msg;
        _Bool skip;
    } cases[] = {
        {
            "static_assert(0) fails", __LINE__,
            SVI("static_assert(0);\n"),
            SVI("(test):1:1: error: static assertion failed: 0\n"),
        },
        {
            "static_assert(0, msg) fails", __LINE__,
            SVI("static_assert(0, \"this should fail\");\n"),
            SVI("(test):1:1: error: static assertion failed: 0: \"this should fail\"\n"),
        },
        {
            "static_assert(1-1) fails", __LINE__,
            SVI("static_assert(1-1, \"zero\");\n"),
            SVI("(test):1:1: error: static assertion failed: (1 - 1): \"zero\"\n"),
        },
        {
            "static_assert(sizeof(int)==8) fails", __LINE__,
            SVI("static_assert(sizeof(int) == 8, \"int is not 8\");\n"),
            SVI("(test):1:1: error: static assertion failed: (4 == (unsigned long)8): \"int is not 8\"\n"),
        },
        {
            "constexpr with non-constant init", __LINE__,
            SVI("int y;\n"
               "constexpr int x = y;\n"),
            SVI("(test):2:19: error: expression is not a constant expression\n"),
        },
        {
            "FAM in middle of struct", __LINE__,
            SVI("struct Bad { int data[]; int x; };\n"),
            SVI("(test):1:24: error: flexible array member must be last field\n"),
        },
        {
            "FAM embedded in struct", __LINE__,
            SVI("struct Inner { int n; char data[]; };\n"
               "struct Outer { struct Inner i; int x; };\n"),
            SVI("(test):2:30: error: struct with flexible array member cannot be embedded\n"),
        },
        {
            "FAM anon struct not at end", __LINE__,
            SVI("struct S { struct { int data[]; }; int x; };\n"),
            SVI("(test):1:34: error: struct with flexible array member cannot be embedded\n"),
        },
        {
            "FAM named struct at end", __LINE__,
            SVI("struct Inner { int n; char data[]; };\n"
               "struct Outer { int x; struct Inner i; };\n"),
            SVI("(test):2:37: error: struct with flexible array member cannot be embedded\n"),
        },
        {
            "Duplicate field", __LINE__,
            SVI("struct S { int x; int x;};\n"),
            SVI("(test):1:24: error: duplicate member 'x'\n"),
        },
        {
            "Duplicate field inside anon", __LINE__,
            SVI("struct S { int x; struct {int x;}; };\n"),
            SVI("(test):1:34: error: duplicate member 'x'\n"),
        },
        {
            "Duplicate field inside nested anon", __LINE__,
            SVI("struct S { int x; struct { struct {int x;}; int y; }; };\n"),
            SVI("(test):1:53: error: duplicate member 'x'\n"),
        },
        {
            "Duplicate field inside separate nested anon", __LINE__,
            SVI("struct S { struct {int x;}; struct { struct {int x;}; int y; }; };\n"),
            SVI("(test):1:63: error: duplicate member 'x'\n"),
        },
        {
            "bitfield width exceeds type (int)", __LINE__,
            SVI("struct S { int x : 33; };\n"),
            SVI("(test):1:18: error: bitfield width (33) exceeds size of type (32 bits)\n"),
        },
        {
            "bitfield width exceeds type (char)", __LINE__,
            SVI("struct S { char x : 9; };\n"),
            SVI("(test):1:19: error: bitfield width (9) exceeds size of type (8 bits)\n"),
        },
        {
            "named bitfield zero width", __LINE__,
            SVI("struct S { int x : 0; };\n"),
            SVI("(test):1:18: error: named bitfield 'x' cannot have zero width\n"),
        },
        {
            "anonymous bitfield width exceeds type", __LINE__,
            SVI("struct S { int : 33; };\n"),
            SVI("(test):1:16: error: bitfield width (33) exceeds size of type (32 bits)\n"),
        },
        {
            "float bitfield", __LINE__,
            SVI("struct S { float x : 3; };\n"),
            SVI("(test):1:20: error: bitfield must have integer or enum type\n"),
        },
        {
            "struct bitfield", __LINE__,
            SVI("struct A { int x; };\nstruct S { struct A a : 3; };\n"),
            SVI("(test):2:23: error: bitfield must have integer or enum type\n"),
        },
        {
            "anonymous float bitfield", __LINE__,
            SVI("struct S { float : 3; };\n"),
            SVI("(test):1:18: error: bitfield must have integer or enum type\n"),
        },
        {
            "typedef method body", __LINE__,
            SVI("typedef int fn_t(int);\n"
               "struct S { fn_t foo { return 1; } };\n"),
            SVI("(test):2:21: error: cannot define method with typedef function type\n"),
        },
        {
            "typedef without type", __LINE__,
            SVI("typedef foo bar;\n"),
            SVI("(test):1:9: error: typedef requires a type\n"),
        },
        {
            "typedef in struct member", __LINE__,
            SVI("struct S { typedef int x; };\n"),
            SVI("(test):1:1: error: Storage class specifiers not allowed in struct/union members\n"),
        },
        {
            "typedef without type in struct", __LINE__,
            SVI("struct S { typedef foo; };\n"),
            SVI("(test):1:1: error: Storage class specifiers not allowed in struct/union members\n"),
        },
        {
            "missing type in struct member", __LINE__,
            SVI("struct S { 123; };\n"),
            SVI("(test):1:12: error: Expected type specifier in struct/union member\n"),
        },
        {
            "typedef in function parameter", __LINE__,
            SVI("void f(typedef int x);\n"),
            SVI("(test):1:8: error: typedef not allowed in function parameter\n"),
        },
        {
            "missing type in function parameter", __LINE__,
            SVI("void f(123);\n"),
            SVI("(test):1:8: error: Expected type specifier in function parameter\n"),
        },
        {
            "missing type in enum underlying type", __LINE__,
            SVI("enum E : { A };\n"),
            SVI("(test):1:1: error: Expected type specifier for enum underlying type\n"),
        },
        {
            "excess elements in scalar init", __LINE__,
            SVI("int x = {1, 2};\n"),
            SVI("(test):1:13: error: excess elements in scalar initializer\n"),
        },
        {
            "designator in scalar init", __LINE__,
            SVI("int x = {.a = 1};\n"),
            SVI("(test):1:10: error: designators not allowed in scalar initializer\n"),
        },
        {
            "excess elements in struct init", __LINE__,
            SVI("struct S { int a; };\n"
               "struct S s = {1, 2};\n"),
            SVI("(test):2:18: error: excess elements in struct initializer\n"),
        },
        {
            "unknown field in designated init", __LINE__,
            SVI("struct S { int a; };\n"
               "struct S s = {.z = 1};\n"),
            SVI("(test):2:15: error: no member named 'z'\n"),
        },
        {
            "array designator in struct init", __LINE__,
            SVI("struct S { int a; };\n"
               "struct S s = {[0] = 1};\n"),
            SVI("(test):2:15: error: array designator in struct initializer\n"),
        },
        {
            "field designator in array init", __LINE__,
            SVI("int arr[3] = {.x = 1};\n"),
            SVI("(test):1:15: error: field designator in array initializer\n"),
        },
        {
            "array index out of bounds", __LINE__,
            SVI("int arr[3] = {[5] = 1};\n"),
            SVI("(test):1:15: error: array index 5 out of bounds (size 3)\n"),
        },
        {
            "excess elements in array init", __LINE__,
            SVI("int arr[2] = {1, 2, 3};\n"),
            SVI("(test):1:21: error: excess elements in array initializer\n"),
        },
        {
            "excess via brace elision: array of structs", __LINE__,
            SVI("struct P { int x; int y; };\n"
               "struct P arr[1] = {1, 2, 3};\n"),
            SVI("(test):2:26: error: excess elements in array initializer\n"),
        },
        {
            "excess via brace elision: struct", __LINE__,
            SVI("struct Inner { int a; };\n"
               "struct Outer { struct Inner s; };\n"
               "struct Outer o = {1, 2};\n"),
            SVI("(test):3:22: error: excess elements in struct initializer\n"),
        },
        {
            "field designator in union: unknown", __LINE__,
            SVI("union U { int a; float b; };\n"
               "union U u = {.c = 1};\n"),
            SVI("(test):2:14: error: no member named 'c'\n"),
        },
        {
            "chained designator: field into scalar", __LINE__,
            SVI("struct S { int a; };\n"
               "struct S s = {.a.b = 1};\n"),
            SVI("(test):2:17: error: member designator into non-struct/union type\n"),
        },
        {
            "chained designator: index into non-array", __LINE__,
            SVI("struct S { int a; };\n"
               "struct S s = {.a[0] = 1};\n"),
            SVI("(test):2:17: error: index designator into non-array type\n"),
        },
        {
            "chained designator: unknown nested field", __LINE__,
            SVI("struct Inner { int x; };\n"
               "struct Outer { struct Inner p; };\n"
               "struct Outer o = {.p.z = 1};\n"),
            SVI("(test):3:21: error: no member named 'z'\n"),
        },
        {
            "incomplete struct init", __LINE__,
            SVI("struct S;\n"
               "struct S s = {1};\n"),
            SVI("(test):2:14: error: initializer for incomplete struct type\n"),
        },
        {
            "incomplete union init", __LINE__,
            SVI("union U;\n"
               "union U u = {1};\n"),
            SVI("(test):2:13: error: initializer for incomplete union type\n"),
        },
        {
            "init list for function type", __LINE__,
            SVI("typedef void fn(void);\n"
               "fn f = {1};\n"),
            SVI("(test):2:8: error: cannot initialize type with initializer list\n"),
        },
        {
            "unterminated union init", __LINE__,
            SVI("union U { int a; float b; };\n"
               "union U u = {\n"),
            SVI("(test):2:13: error: unterminated initializer list\n"),
        },
        {
            "excess elements in braced scalar", __LINE__,
            SVI("int x = {{1, 2}};\n"),
            SVI("(test):1:14: error: excess elements in scalar initializer\n"),
        },
        {
            "chained designator: array index out of bounds", __LINE__,
            SVI("struct S { int arr[3]; };\n"
               "struct S s = {.arr[5] = 1};\n"),
            SVI("(test):2:19: error: array index 5 out of bounds (size 3)\n"),
        },
        {
            "negative array designator", __LINE__,
            SVI("int arr[3] = {[-1] = 1};\n"),
            SVI("(test):1:15: error: array designator value out of range\n"),
        },
        {
            "negative chained array designator", __LINE__,
            SVI("struct S { int arr[3]; };\n"
               "struct S s = {.arr[-1] = 1};\n"),
            SVI("(test):2:19: error: array designator value out of range\n"),
        },
        {
            "vector_size on non-scalar type", __LINE__,
            SVI("struct S { int a; };\n"
               "typedef struct S v4s __attribute__((vector_size(16)));\n"),
            SVI("(test):2:1: error: vector_size attribute requires a scalar type\n"),
        },
        {
            "vector_size not power of 2", __LINE__,
            SVI("typedef int v __attribute__((vector_size(7)));\n"),
            SVI("(test):1:30: error: vector_size must be a power of 2\n"),
        },
        {
            "vector_size zero", __LINE__,
            SVI("typedef int v __attribute__((vector_size(0)));\n"),
            SVI("(test):1:30: error: vector_size must be a power of 2\n"),
        },
        {
            "vector_size smaller than element", __LINE__,
            SVI("typedef int v __attribute__((vector_size(2)));\n"),
            SVI("(test):1:1: error: vector_size is smaller than the element type\n"),
        },
        {
            "aligned on typedef", __LINE__,
            SVI("typedef int aligned_int __attribute__((aligned(16)));\n"),
            SVI("(test):1:1: error: aligned attribute on non-struct/union type is not supported\n"),
        },
        {
            "vector init: excess elements", __LINE__,
            SVI("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {1, 2, 3, 4, 5};\n"),
            SVI("(test):2:23: error: excess elements in vector initializer\n"),
        },
        {
            "vector init: designator not allowed", __LINE__,
            SVI("typedef int v4si __attribute__((vector_size(16)));\n"
               "v4si v = {[0] = 1};\n"),
            SVI("(test):2:11: error: designators not allowed in vector initializer\n"),
        },
        {
            "packed on non-struct", __LINE__,
            SVI("typedef int packed_int __attribute__((packed));\n"),
            SVI("(test):1:1: error: packed attribute on non-struct type is not supported\n"),
        },
        {
            "transparent_union on non-union", __LINE__,
            SVI("typedef int tu __attribute__((transparent_union));\n"),
            SVI("(test):1:1: error: transparent_union attribute on non-union type is not supported\n"),
        },
        {
            "too few args", __LINE__,
            SVI("void f(int a, int b);\n"
               "int x = f(1);\n"),
            SVI("(test):2:10: error: Expected 2 arguments, got 1\n"),
        },
        {
            "too many args", __LINE__,
            SVI("void f(int a);\n"
               "int x = f(1, 2);\n"),
            SVI("(test):2:10: error: Expected 1 arguments, got 2\n"),
        },
        {
            "zero args to non-void function", __LINE__,
            SVI("int f(int a);\n"
               "int x = f();\n"),
            SVI("(test):2:10: error: Expected 1 arguments, got 0\n"),
        },
        {
            "too few args to variadic", __LINE__,
            SVI("int printf(const char *fmt, ...);\n"
               "int x = printf();\n"),
            SVI("(test):2:15: error: Too few arguments: expected at least 1, got 0\n"),
        },
        {
            "call non-function", __LINE__,
            SVI("int x;\n"
               "int y = x(1);\n"),
            SVI("(test):2:10: error: Called object is not a function or function pointer\n"),
        },
        {
            "call: struct arg to int param", __LINE__,
            SVI("struct S { int x; };\n"
               "void f(int a);\n"
               "struct S s;\n"
               "int x = f(s);\n"),
            SVI("(test):4:11: error: cannot implicitly convert from 'struct S' to 'int'\n"),
        },
        {
            "call: struct arg to float param", __LINE__,
            SVI("struct S { int x; };\n"
               "int f(float a);\n"
               "struct S s;\n"
               "int x = f(s);\n"),
            SVI("(test):4:11: error: cannot implicitly convert from 'struct S' to 'float'\n"),
        },
        {
            "call: pointer arg to struct param", __LINE__,
            SVI("struct S { int x; };\n"
               "void f(struct S s);\n"
               "int *p;\n"
               "int x = f(p);\n"),
            SVI("(test):4:11: error: cannot implicitly convert from 'int *' to 'struct S'\n"),
        },
        {
            "assign struct to int", __LINE__,
            SVI("struct S { int x; };\n"
               "struct S s;\n"
               "int x = s;\n"),
            SVI("(test):3:9: error: cannot implicitly convert from 'struct S' to 'int'\n"),
        },
        {
            "assign int to struct", __LINE__,
            SVI("struct S { int x; };\n"
               "struct S s = 42;\n"),
            SVI("(test):2:14: error: cannot implicitly convert from 'int' to 'struct S'\n"),
        },
        {
            "assign non-zero int to pointer", __LINE__,
            SVI("int *p = 1;\n"),
            SVI("(test):1:10: error: cannot implicitly convert from 'int' to 'int *'\n"),
        },
        {
            "assign pointer to int", __LINE__,
            SVI("int *p;\n"
               "int x = p;\n"),
            SVI("(test):2:9: error: cannot implicitly convert from 'int *' to 'int'\n"),
        },
        {
            "const int* to int* (drops const)", __LINE__,
            SVI("const int *cip;\n"
               "int *ip = cip;\n"),
            SVI("(test):2:11: error: cannot implicitly convert from 'const int *' to 'int *'\n"),
        },
        {
            "assign to const variable", __LINE__,
            SVI("const int x = 0;\n"
               "x = 1;\n"),
            SVI("(test):2:3: error: cannot assign to variable with const-qualified type\n"),
        },
        {
            "incompatible struct types", __LINE__,
            SVI("struct A { int x; };\n"
               "struct B { int x; };\n"
               "struct A a;\n"
               "struct B b = a;\n"),
            SVI("(test):4:14: error: cannot implicitly convert from 'struct A' to 'struct B'\n"),
        },
        {
            "lambda: non-function type", __LINE__,
            SVI("int r = int{1};\n"),
            SVI("(test):1:9: error: Lambda requires a function type, got non-function type\n"),
        },
        {
            "static if: non-constant condition", __LINE__,
            SVI("int x;\n"
               "static if(x) { int y; }\n"),
            SVI("(test):2:11: error: expression is not a constant expression\n"),
        },
        {
            "address of bitfield", __LINE__,
            SVI("struct S { int a : 3; int b : 5; };\n"
               "struct S s;\n"
               "int* p = &s.a;\n"),
            SVI("(test):3:10: error: cannot take address of bitfield\n"),
        },

        //
        {
            "self sizeof auto var", __LINE__,
            SVI("auto x = sizeof x;\n"),
            SVI("(test):1:10: error: cannot take sizeof incomplete type\n"),
        },
        {
            "self sizeof const var", __LINE__,
            SVI("const x = sizeof x;\n"),
            SVI("(test):1:11: error: cannot take sizeof incomplete type\n"),
        },
        {
            "self sizeof constexpr var", __LINE__,
            SVI("constexpr x = sizeof x;\n"),
            SVI("(test):1:15: error: cannot take sizeof incomplete type\n"),
        },
        {
            "self sizeof __auto_type var", __LINE__,
            SVI("__auto_type x = sizeof x;\n"),
            SVI("(test):1:17: error: cannot take sizeof incomplete type\n"),
        },
        {
            "self sizeof inferred array", __LINE__,
            SVI("int x[] = {sizeof x};\n"),
            SVI("(test):1:12: error: sizeof applied to incomplete array type\n"),
        },
        {
            "assign to rvalue", __LINE__,
            SVI("int foo(void); void bar(void){ foo() = 1; }\n"),
            SVI("(test):1:38: error: expression is not assignable\n"),
        },
        {
            "prefix inc rvalue", __LINE__,
            SVI("int foo(void); void bar(void){ ++foo(); }\n"),
            SVI("(test):1:32: error: expression is not an lvalue\n"),
        },
        {
            "postfix inc rvalue", __LINE__,
            SVI("int foo(void); void bar(void){ foo()++; }\n"),
            SVI("(test):1:37: error: expression is not an lvalue\n"),
        },
        {
            "address of rvalue", __LINE__,
            SVI("int foo(void); void bar(void){ int* p = &foo(); }\n"),
            SVI("(test):1:41: error: cannot take address of rvalue\n"),
        },
        {
            "continue in switch without loop", __LINE__,
            SVI("switch(1){ case 1: continue; }\n"),
            SVI("(test):1:20: error: 'continue' statement not in loop statement\n"),
        },
        {
            "pointer plus pointer", __LINE__,
            SVI("int* a; int* b; int* c = a + b;\n"),
            SVI("(test):1:28: error: addition of two pointers\n"),
        },
        {
            "prefix inc const", __LINE__,
            SVI("const int x = 1; int y = ++x;\n"),
            SVI("(test):1:26: error: cannot modify const-qualified variable\n"),
        },
        {
            "postfix inc const", __LINE__,
            SVI("const int x = 1; int y = x++;\n"),
            SVI("(test):1:27: error: cannot modify const-qualified variable\n"),
        },
        {
            "bitnot float", __LINE__,
            SVI("int x = ~3.0f;\n"),
            SVI("(test):1:9: error: '~' requires integer type\n"),
        },
        {
            "modulo float", __LINE__,
            SVI("float x = 3.0f % 2.0f;\n"),
            SVI("(test):1:16: error: operator requires integer operands\n"),
        },
        {
            "bitand float", __LINE__,
            SVI("float a; float b; int z = a & b;\n"),
            SVI("(test):1:29: error: operator requires integer operands\n"),
        },
        {
            "bitor float", __LINE__,
            SVI("float a; float b; int z = a | b;\n"),
            SVI("(test):1:29: error: operator requires integer operands\n"),
        },
        {
            "bitxor float", __LINE__,
            SVI("float a; float b; int z = a ^ b;\n"),
            SVI("(test):1:29: error: operator requires integer operands\n"),
        },
        {
            "switch float", __LINE__,
            SVI("switch(1.0){}\n"),
            SVI("(test):1:1: error: switch requires integer expression\n"),
        },
        {
            "duplicate case", __LINE__,
            SVI("switch(1){ case 1: break; case 1: break; }\n"),
            SVI("(test):1:1: error: duplicate case value '1'\n"),
        },
        {
            "if struct condition", __LINE__,
            SVI("struct S{int x;}; struct S s; if(s){}\n"),
            SVI("(test):1:31: error: 'if' condition requires scalar type\n"),
        },
        {
            "while struct condition", __LINE__,
            SVI("struct S{int x;}; struct S s; while(s){}\n"),
            SVI("(test):1:31: error: 'while' condition requires scalar type\n"),
        },
        {
            "do-while struct condition", __LINE__,
            SVI("struct S{int x;}; struct S s; do{}while(s);\n"),
            SVI("(test):1:31: error: 'do-while' condition requires scalar type\n"),
        },
        {
            "for struct condition", __LINE__,
            SVI("struct S{int x;}; struct S s; for(;s;){}\n"),
            SVI("(test):1:31: error: 'for' condition requires scalar type\n"),
        },
        {
            "logand struct", __LINE__,
            SVI("struct S{int x;}; struct S a; struct S b; int c = a && b;\n"),
            SVI("(test):1:53: error: '&&' requires scalar type\n"),
        },
        {
            "logor struct", __LINE__,
            SVI("struct S{int x;}; struct S a; struct S b; int c = a || b;\n"),
            SVI("(test):1:53: error: '||' requires scalar type\n"),
        },
        {
            "lognot struct", __LINE__,
            SVI("struct S{int x;}; struct S s; int c = !s;\n"),
            SVI("(test):1:39: error: '!' requires scalar type\n"),
        },
        {
            "subscript float index", __LINE__,
            SVI("int a[10]; int x = a[1.5];\n"),
            SVI("(test):1:21: error: array subscript requires integer or pointer type\n"),
        },
        {
            "inc struct", __LINE__,
            SVI("struct S{int x;}; struct S s; struct S t = s++;\n"),
            SVI("(test):1:45: error: increment/decrement requires arithmetic or pointer type\n"),
        },
        {
            "dec struct", __LINE__,
            SVI("struct S{int x;}; struct S s; struct S t = --s;\n"),
            SVI("(test):1:44: error: increment/decrement requires arithmetic or pointer type\n"),
        },
        {
            "ptr sub incompatible", __LINE__,
            SVI("int* a; char* b; long d = a - b;\n"),
            SVI("(test):1:29: error: pointer subtraction with incompatible types\n"),
        },
        {
            "ptr cmp incompatible", __LINE__,
            SVI("int* a; char* b; int c = a < b;\n"),
            SVI("(test):1:28: error: comparison of incompatible pointer types\n"),
        },
        {
            "modassign float", __LINE__,
            SVI("float x = 3.0f; float y = (x %= 2);\n"),
            SVI("(test):1:30: error: operator requires integer operands\n"),
        },
        {
            "bitandassign float", __LINE__,
            SVI("float x = 3.0f; float y = (x &= 1);\n"),
            SVI("(test):1:30: error: operator requires integer operands\n"),
        },
        {
            "bitorassign float", __LINE__,
            SVI("float x = 3.0f; float y = (x |= 1);\n"),
            SVI("(test):1:30: error: operator requires integer operands\n"),
        },
        {
            "bitxorassign float", __LINE__,
            SVI("float x = 3.0f; float y = (x ^= 1);\n"),
            SVI("(test):1:30: error: operator requires integer operands\n"),
        },
        {
            "lshiftassign float", __LINE__,
            SVI("float x = 3.0f; float y = (x <<= 1);\n"),
            SVI("(test):1:30: error: operator requires integer operands\n"),
        },
        {
            "rshiftassign float", __LINE__,
            SVI("float x = 3.0f; float y = (x >>= 1);\n"),
            SVI("(test):1:30: error: operator requires integer operands\n"),
        },
        {
            "duplicate local var", __LINE__,
            SVI("{ int x = 1; int x = 2; }\n"),
            SVI("(test):1:20: error: redefinition of 'x'\n"),
        },
        {
            "incomplete struct var", __LINE__,
            SVI("struct S; struct S x;\n"),
            SVI("(test):1:21: error: variable has incomplete type 'struct S'\n"),
        },
        {
            "array of void", __LINE__,
            SVI("void a[10];\n"),
            SVI("(test):1:1: error: array of void is not allowed\n"),
        },
        {
            "func returning array", __LINE__,
            SVI("int f(void)[10];\n"),
            SVI("(test):1:1: error: function cannot return array type\n"),
        },
        {
            "func returning function", __LINE__,
            SVI("int f(void)(int);\n"),
            SVI("(test):1:1: error: function cannot return function type\n"),
        },
        {
            "ptr < float", __LINE__,
            SVI("int *p; float f; int x = p < f;\n"),
            SVI("(test):1:28: error: comparison of pointer with non-pointer\n"),
        },
        {
            "ptr > int", __LINE__,
            SVI("int *p; int x = p > 42;\n"),
            SVI("(test):1:19: error: comparison of pointer with non-pointer\n"),
        },
        {
            "ptr + float", __LINE__,
            SVI("int *p; int *q = p + 3.14f;\n"),
            SVI("(test):1:20: error: pointer arithmetic requires integer operand\n"),
        },
        {
            "ptr - float", __LINE__,
            SVI("int *p; int *q = p - 3.14f;\n"),
            SVI("(test):1:20: error: pointer arithmetic requires integer operand\n"),
        },
        {
            "float lshift", __LINE__,
            SVI("int x = 3.0 << 2;\n"),
            SVI("(test):1:13: error: shift operands require integer type\n"),
        },
        {
            "rshift float", __LINE__,
            SVI("int x = 1 >> 2.0;\n"),
            SVI("(test):1:11: error: shift operands require integer type\n"),
        },
        {
            "ternary struct condition", __LINE__,
            SVI("struct S{int x;}; struct S s; int x = s ? 1 : 0;\n"),
            SVI("(test):1:41: error: '?:' requires scalar type\n"),
        },
        {
            "void func assigned to int", __LINE__,
            SVI("void foo(void);\n"
               "int x = foo();\n"),
            SVI("(test):2:12: error: cannot implicitly convert from 'void' to 'int'\n"),
        },
        {
            "ternary ptr and float", __LINE__,
            SVI("int *p; int cond; void* q = cond ? p : 3.14;\n"),
            SVI("(test):1:34: error: incompatible operand types for ternary\n"),
        },
        {
            "ptr mulassign", __LINE__,
            SVI("int *p; p *= 0;\n"),
            SVI("(test):1:11: error: compound assignment requires arithmetic operands\n"),
        },
        {
            "ptr divassign", __LINE__,
            SVI("int *p; p /= 0;\n"),
            SVI("(test):1:11: error: compound assignment requires arithmetic operands\n"),
        },
        {
            "global var redef", __LINE__,
            SVI("int x = 1;\n"
               "int x = 2;\n"),
            SVI("(test):2:10: error: redefinition of 'x'\n"),
        },
        {
            "void param with other", __LINE__,
            SVI("void foo(void, int);\n"),
            SVI("(test):1:10: error: parameter cannot have void type\n"),
        },
        {
            "named void param", __LINE__,
            SVI("void foo(void x);\n"),
            SVI("(test):1:10: error: parameter cannot have void type\n"),
        },
        {
            "duplicate param name", __LINE__,
            SVI("void f(int x, int x);\n"),
            SVI("(test):1:15: error: duplicate parameter name 'x'\n"),
        },
        {
            "typedef redef diff type", __LINE__,
            SVI("typedef int foo;\n"
               "typedef float foo;\n"),
            SVI("(test):2:18: error: redefinition of typedef 'foo' as 'float'; previously defined as 'int'\n"),
        },
        {
            "ptr addassign float", __LINE__,
            SVI("int *p; p += 3.14;\n"),
            SVI("(test):1:11: error: pointer arithmetic requires integer operand\n"),
        },
        {
            "ptr subassign float", __LINE__,
            SVI("int *p; p -= 3.14;\n"),
            SVI("(test):1:11: error: pointer arithmetic requires integer operand\n"),
        },
        {
            "bare qualifier in expression", __LINE__,
            SVI("int x = const;\n"),
            SVI("(test):1:9: error: Expected type in expression, got only qualifiers/storage class\n"),
        },
        {
            "bare qualifier in cast", __LINE__,
            SVI("int x = (const)3;\n"),
            SVI("(test):1:10: error: Expected type name, got only qualifiers/storage class\n"),
        },
        {
            "bare qualifier in struct member", __LINE__,
            SVI("struct S { const; };\n"),
            SVI("(test):1:12: error: Expected type specifier in struct/union member\n"),
        },
        {
            "bare qualifier in function param", __LINE__,
            SVI("void foo(const);\n"),
            SVI("(test):1:10: error: Expected type in function parameter\n"),
        },
        {
            "case label: variable not constexpr", __LINE__,
            SVI("int n = 3;\n"
               "int f(int x) { switch(x) { case n: return 1; default: return 0; } }\n"),
            SVI("(test):2:33: error: expression is not a constant expression\n"),
        },
        {
            "case label: function call", __LINE__,
            SVI("int g(void);\n"
               "int f(int x) { switch(x) { case g(): return 1; default: return 0; } }\n"),
            SVI("(test):2:34: error: function call in constant expression\n"),
        },
        {
            "enum value: variable", __LINE__,
            SVI("int x = 5;\n"
               "enum E { A = x };\n"),
            SVI("(test):2:14: error: expression is not a constant expression\n"),
        },
        {
            "bitfield width: variable", __LINE__,
            SVI("int w = 3;\n"
               "struct S { int x : w; };\n"),
            SVI("(test):2:20: error: expression is not a constant expression\n"),
        },
        {
            "static_assert: variable", __LINE__,
            SVI("int x = 1;\n"
               "static_assert(x);\n"),
            SVI("(test):2:15: error: expression is not a constant expression\n"),
        },
        {
            "constexpr init: variable", __LINE__,
            SVI("int y = 5;\n"
               "constexpr int x = y;\n"),
            SVI("(test):2:19: error: expression is not a constant expression\n"),
        },
        {
            "constexpr init: function call", __LINE__,
            SVI("int f(void);\n"
               "constexpr int x = f();\n"),
            SVI("(test):2:20: error: function call in constant expression\n"),
        },
        {
            "static local init: variable", __LINE__,
            SVI("void f(int y) { static int x = y; }\n"),
            SVI("(test):1:32: error: expression is not a constant expression\n"),
        },
        {
            "case label: assignment", __LINE__,
            SVI("int x;\n"
               "int f(int v) { switch(v) { case (x=1): return 1; default: return 0; } }\n"),
            SVI("(test):2:34: error: expression is not a constant expression\n"),
        },
        {
            "case label: pre-increment", __LINE__,
            SVI("int x;\n"
               "int f(int v) { switch(v) { case ++x: return 1; default: return 0; } }\n"),
            SVI("(test):2:33: error: increment/decrement in constant expression\n"),
        },
        // ---- constexpr/value class error torture tests ----
        {
            "enum value: function call", __LINE__,
            SVI("int f(void);\n"
               "enum E { A = f() };\n"),
            SVI("(test):2:15: error: function call in constant expression\n"),
        },
        {
            "bitfield width: function call", __LINE__,
            SVI("int f(void);\n"
               "struct S { int x : f(); };\n"),
            SVI("(test):2:21: error: function call in constant expression\n"),
        },
        {
            "array dim: non-constexpr variable", __LINE__,
            SVI("int n = 10;\n"
               "int arr[n];\n"),
            SVI("(test):2:9: error: expression is not a constant expression\n"),
        },
        {
            "array dim: function call", __LINE__,
            SVI("int f(void);\n"
               "int arr[f()];\n"),
            SVI("(test):2:10: error: function call in constant expression\n"),
        },
        {
            "constexpr: post-increment", __LINE__,
            SVI("int x;\n"
               "constexpr int y = x++;\n"),
            SVI("(test):2:19: error: expression is not a constant expression\n"),
        },
        {
            "constexpr: address-of", __LINE__,
            SVI("int x;\n"
               "constexpr int* p = &x;\n"),
            SVI("(test):2:20: error: address-of in constant expression\n"),
        },
        {
            "static local: automatic variable", __LINE__,
            SVI("void f(int y) { static int x = y; }\n"),
            SVI("(test):1:32: error: expression is not a constant expression\n"),
        },
        {
            "static local: function call", __LINE__,
            SVI("int g(void);\n"
               "void f(void) { static int x = g(); }\n"),
            SVI("(test):2:32: error: function call in constant expression\n"),
        },
        {
            "static local: parameter in expr", __LINE__,
            SVI("void f(int a, int b) { static int x = a + b; }\n"),
            SVI("(test):1:39: error: expression is not a constant expression\n"),
        },
        {
            "case label: post-decrement", __LINE__,
            SVI("int x;\n"
               "int f(int v) { switch(v) { case x--: return 1; default: return 0; } }\n"),
            SVI("(test):2:33: error: expression is not a constant expression\n"),
        },
        {
            "enum value: assignment", __LINE__,
            SVI("int x;\n"
               "enum E { A = (x = 5) };\n"),
            SVI("(test):2:15: error: expression is not a constant expression\n"),
        },
        {
            "constexpr: compound assignment", __LINE__,
            SVI("int x = 1;\n"
               "constexpr int y = (x += 1);\n"),
            SVI("(test):2:20: error: expression is not a constant expression\n"),
        },
        {
            "_Alignas: non-constexpr", __LINE__,
            SVI("int n = 16;\n"
               "_Alignas(n) int x;\n"),
            SVI("(test):2:10: error: expression is not a constant expression\n"),
        },
        {
            "signed neg overflow in constexpr", __LINE__,
            SVI("enum { X = -(-2147483647 - 1) };\n"),
            SVI("(test):1:10: error: enumerator value must be a constant integer expression\n"),
        },
        {
            "negative constexpr array subscript", __LINE__,
            SVI("constexpr int arr[] = {10, 20, 30};\n"
               "_Static_assert(arr[-1] == 0);\n"),
            SVI("(test):2:1: error: static_assert expression is not a constant expression\n"),
        },
        {
            "printf: wrong type for %d", __LINE__,
            SVI("void f(void) { printf(\"%d\", \"hello\"); }\n"),
            SVI("(test):1:23: error: format specifier '%d' (argument 1) expects 'int', but argument has type 'char *'\n"),
        },
        {
            "printf: wrong type for %s", __LINE__,
            SVI("void f(void) { printf(\"%s\", 42); }\n"),
            SVI("(test):1:23: error: format specifier '%s' (argument 1) expects 'char *', but argument has type 'int'\n"),
        },
        {
            "printf: too few args", __LINE__,
            SVI("void f(void) { printf(\"%d %d\", 1); }\n"),
            SVI("(test):1:23: error: format specifies 2 arguments, but only 1 provided\n"),
        },
        {
            "printf: too many args", __LINE__,
            SVI("void f(void) { printf(\"%d\", 1, 2); }\n"),
            SVI("(test):1:23: error: format specifies 1 argument, but 2 provided\n"),
        },
        {
            "printf: %p expects pointer", __LINE__,
            SVI("void f(void) { printf(\"%p\", 42); }\n"),
            SVI("(test):1:23: error: format specifier '%p' (argument 1) expects 'void *', but argument has type 'int'\n"),
        },
        {
            "printf: %ld expects long", __LINE__,
            SVI("void f(void) { printf(\"%ld\", 42); }\n"),
            SVI("(test):1:23: error: format specifier '%ld' (argument 1) expects 'long', but argument has type 'int'\n"),
        },
        {
            "printf: %zu expects size_t", __LINE__,
            SVI("void f(void) { printf(\"%zu\", 42); }\n"),
            SVI("(test):1:23: error: format specifier '%zu' (argument 1) expects 'unsigned long', but argument has type 'int'\n"),
        },
        {
            "snprintf: wrong type for %d", __LINE__,
            SVI("void f(char* buf) { snprintf(buf, 100, \"%d\", \"hello\"); }\n"),
            SVI("(test):1:40: error: format specifier '%d' (argument 1) expects 'int', but argument has type 'char *'\n"),
        },
        {
            "printf: invalid specifier", __LINE__,
            SVI("void f(void) { printf(\"%q\"); }\n"),
            SVI("(test):1:23: error: invalid format specifier '%q'\n"),
        },
        {
            "printf: %*s consumes int then char*", __LINE__,
            SVI("void f(void) { printf(\"%*s\", \"hello\", \"world\"); }\n"),
            SVI("(test):1:23: error: format specifier '%*' (argument 1) expects 'int', but argument has type 'char *'\n"),
        },
        {
            "printf: %.*d precision consumes int", __LINE__,
            SVI("void f(void) { printf(\"%.*d\", \"hello\"); }\n"),
            SVI("(test):1:23: error: format specifier '%.*' (argument 1) expects 'int', but argument has type 'char *'\n"),
        },
        {
            "printf: %d with long", __LINE__,
            SVI("void f(long x) { printf(\"%d\", x); }\n"),
            SVI("(test):1:25: error: format specifier '%d' (argument 1) expects 'int', but argument has type 'long'\n"),
        },


        {
            "printf: %lld with int", __LINE__,
            SVI("void f(void) { printf(\"%lld\", 42); }\n"),
            SVI("(test):1:23: error: format specifier '%lld' (argument 1) expects 'long long', but argument has type 'int'\n"),
        },
        {
            "printf: %f with int", __LINE__,
            SVI("void f(void) { printf(\"%f\", 42); }\n"),
            SVI("(test):1:23: error: format specifier '%f' (argument 1) expects 'double', but argument has type 'int'\n"),
        },
        {
            "printf: %s with int pointer", __LINE__,
            SVI("void f(int* p) { printf(\"%s\", p); }\n"),
            SVI("(test):1:25: error: format specifier '%s' (argument 1) expects 'char *', but argument has type 'int *'\n"),
        },
        {
            "printf: %d with double", __LINE__,
            SVI("void f(double x) { printf(\"%d\", x); }\n"),
            SVI("(test):1:27: error: format specifier '%d' (argument 1) expects 'int', but argument has type 'double'\n"),
        },
        {
            "printf: too many args plural", __LINE__,
            SVI("void f(void) { printf(\"%d\", 1, 2, 3); }\n"),
            SVI("(test):1:23: error: format specifies 1 argument, but 3 provided\n"),
        },
        {
            "printf: too few args 0 for 1", __LINE__,
            SVI("void f(void) { printf(\"%d\"); }\n"),
            SVI("(test):1:23: error: format specifies 1 argument, but only 0 provided\n"),
        },
        {
            "printf: %hd with long (size mismatch)", __LINE__,
            SVI("void f(long x) { printf(\"%hd\", x); }\n"),
            SVI("(test):1:25: error: format specifier '%hd' (argument 1) expects 'int', but argument has type 'long'\n"),
        },
        {
            "printf: constexpr format string error", __LINE__,
            SVI("constexpr const char* fmt = \"%d\";\n"
               "void f(void) { printf(fmt, \"hello\"); }\n"),
            SVI("(test):2:23: error: format specifier '%d' (argument 1) expects 'int', but argument has type 'char *'\n"),
        },
        {
            "printf: %I64d with int", __LINE__,
            SVI("void f(void) { printf(\"%I64d\", 42); }\n"),
            SVI("(test):1:23: error: format specifier '%I64d' (argument 1) expects 'long long', but argument has type 'int'\n"),
        },


        {
            "printf: incomplete % at end", __LINE__,
            SVI("void f(void) { printf(\"%\"); }\n"),
            SVI("(test):1:23: error: incomplete format specifier at end of string\n"),
        },
        {
            "printf: %hs invalid", __LINE__,
            SVI("void f(void) { printf(\"%hs\", \"hi\"); }\n"),
            SVI("(test):1:23: error: invalid length modifier for '%s' format specifier\n"),
        },
        {
            "printf: %hc invalid", __LINE__,
            SVI("void f(void) { printf(\"%hc\", 42); }\n"),
            SVI("(test):1:23: error: invalid length modifier for '%c' format specifier\n"),
        },
        {
            "printf: %llf invalid", __LINE__,
            SVI("void f(double x) { printf(\"%llf\", x); }\n"),
            SVI("(test):1:27: error: invalid length modifier for '%f' format specifier\n"),
        },
        {
            "printf: %Ld invalid", __LINE__,
            SVI("void f(long double x) { printf(\"%Ld\", x); }\n"),
            SVI("(test):1:32: error: invalid length modifier for '%d' format specifier\n"),
        },
        {
            "printf: %zs invalid", __LINE__,
            SVI("void f(void) { printf(\"%zs\", \"hi\"); }\n"),
            SVI("(test):1:23: error: invalid length modifier for '%s' format specifier\n"),
        },
        {
            "printf: %Lc invalid", __LINE__,
            SVI("void f(void) { printf(\"%Lc\", 42); }\n"),
            SVI("(test):1:23: error: invalid length modifier for '%c' format specifier\n"),
        },
        {
            "printf: %hhf invalid", __LINE__,
            SVI("void f(double x) { printf(\"%hhf\", x); }\n"),
            SVI("(test):1:27: error: invalid length modifier for '%f' format specifier\n"),
        },
        {
            "printf: %Lp invalid", __LINE__,
            SVI("void f(void* p) { printf(\"%Lp\", p); }\n"),
            SVI("(test):1:26: error: invalid length modifier for '%p' format specifier\n"),
        },
        {
            "printf: %lc with char*", __LINE__,
            SVI("void f(void) { printf(\"%lc\", \"hello\"); }\n"),
            SVI("(test):1:23: error: format specifier '%lc' (argument 1) expects 'int', but argument has type 'char *'\n"),
        },
        {
            "printf: %ls with char*", __LINE__,
            SVI("void f(char* s) { printf(\"%ls\", s); }\n"),
            SVI("(test):1:26: error: format specifier '%ls' (argument 1) expects 'int *', but argument has type 'char *'\n"),
        },
        {
            "printf: %Lf with double", __LINE__,
            SVI("void f(double x) { printf(\"%Lf\", x); }\n"),
            SVI("(test):1:27: error: format specifier '%Lf' (argument 1) expects 'long double', but argument has type 'double'\n"),
        },
        {
            "printf attr: udf wrong type", __LINE__,
            SVI("__attribute__((format(printf, 1, 2)))\n"
               "void my_printf(const char* fmt, ...);\n"
               "void f(void) { my_printf(\"%d\", \"hello\"); }\n"),
            SVI("(test):3:26: error: format specifier '%d' (argument 1) expects 'int', but argument has type 'char *'\n"),
        },
        {
            "printf attr: udf with prefix args wrong type", __LINE__,
            SVI("__attribute__((format(printf, 2, 3)))\n"
               "void log_msg(int level, const char* fmt, ...);\n"
               "void f(void) { log_msg(1, \"%d\", \"hello\"); }\n"),
            SVI("(test):3:27: error: format specifier '%d' (argument 1) expects 'int', but argument has type 'char *'\n"),
        },
        {
            "extern definition after static declaration", __LINE__,
            SVI("static void foo(void);\n"
               "extern void foo(void){}\n"),
            SVI("(test):2:22: error: non-static declaration of 'foo' follows static declaration\n"),
        },
        {
            "msvc suffix i7 invalid", __LINE__,
            SVI("int x = 42i7;\n"),
            SVI("(test):1:9: error: Invalid digit in number\n"),
        },
        {
            "msvc suffix bare i invalid", __LINE__,
            SVI("int x = 42i;\n"),
            SVI("(test):1:9: error: Invalid digit in number\n"),
        },
        {
            "msvc suffix i128 invalid", __LINE__,
            SVI("int x = 42i128;\n"),
            SVI("(test):1:9: error: Invalid digit in number\n"),
        },
        {
            "anonymous function definition", __LINE__,
            SVI("void(void){}\n"),
            SVI("(test):1:11: error: Expected ',' or ';'\n"),
        },
        {
            "double int", __LINE__,
            SVI("double int x;\n"),
            SVI("(test):1:8: error: Second type in declaration\n"),
        },
        {
            "int float", __LINE__,
            SVI("int float x;\n"),
            SVI("(test):1:5: error: Second type in declaration\n"),
        },
        {
            "short long", __LINE__,
            SVI("short long x;\n"),
            SVI("(test):1:7: error: long after short\n"),
        },
        {
            "long short", __LINE__,
            SVI("long short x;\n"),
            SVI("(test):1:6: error: short after long\n"),
        },
        {
            "long long long", __LINE__,
            SVI("long long long x;\n"),
            SVI("(test):1:11: error: Duplicate long after long long in declaration\n"),
        },
        {
            "duplicate int", __LINE__,
            SVI("int int x;\n"),
            SVI("(test):1:5: error: Duplicate int in declaration\n"),
        },
        {
            "duplicate char", __LINE__,
            SVI("char char x;\n"),
            SVI("(test):1:6: error: Duplicate char in declaration\n"),
        },
        {
            "char after int", __LINE__,
            SVI("int char x;\n"),
            SVI("(test):1:5: error: char after int\n"),
        },
        {
            "int after char", __LINE__,
            SVI("char int x;\n"),
            SVI("(test):1:6: error: int after char\n"),
        },
        {
            "long after char", __LINE__,
            SVI("char long x;\n"),
            SVI("(test):1:6: error: long after char\n"),
        },
        {
            "char after long", __LINE__,
            SVI("long char x;\n"),
            SVI("(test):1:6: error: char after long\n"),
        },
        {
            "__int128 after int", __LINE__,
            SVI("int __int128 x;\n"),
            SVI("(test):1:5: error: __int128 after int\n"),
        },
        {
            "int after __int128", __LINE__,
            SVI("__int128 int x;\n"),
            SVI("(test):1:10: error: int after __int128\n"),
        },
        {
            "__int128 after char", __LINE__,
            SVI("char __int128 x;\n"),
            SVI("(test):1:6: error: __int128 after char\n"),
        },
        {
            "__int128 after short", __LINE__,
            SVI("short __int128 x;\n"),
            SVI("(test):1:7: error: __int128 after short\n"),
        },
        {
            "__int128 after long", __LINE__,
            SVI("long __int128 x;\n"),
            SVI("(test):1:6: error: __int128 after long\n"),
        },
        {
            "duplicate __int128", __LINE__,
            SVI("__int128 __int128 x;\n"),
            SVI("(test):1:10: error: Duplicate __int128 in declaration\n"),
        },
        {
            "char after short", __LINE__,
            SVI("short char x;\n"),
            SVI("(test):1:7: error: char after short\n"),
        },
        {
            "unsigned after signed", __LINE__,
            SVI("signed unsigned x;\n"),
            SVI("(test):1:8: error: unsigned after signed\n"),
        },
        {
            "signed after unsigned", __LINE__,
            SVI("unsigned signed x;\n"),
            SVI("(test):1:10: error: signed after unsigned\n"),
        },
        {
            "constexpr after extern", __LINE__,
            SVI("extern constexpr int x;\n"),
            SVI("(test):1:8: error: constexpr after extern\n"),
        },
        {
            "constexpr after typedef", __LINE__,
            SVI("typedef constexpr int x;\n"),
            SVI("(test):1:9: error: constexpr after typedef\n"),
        },
        {
            "register after static", __LINE__,
            SVI("static register int x;\n"),
            SVI("(test):1:8: error: register after static\n"),
        },
        {
            "register after extern", __LINE__,
            SVI("extern register int x;\n"),
            SVI("(test):1:8: error: register after extern\n"),
        },
        {
            "register after typedef", __LINE__,
            SVI("typedef register int x;\n"),
            SVI("(test):1:9: error: register after typedef\n"),
        },
        {
            "register after thread_local", __LINE__,
            SVI("thread_local register int x;\n"),
            SVI("(test):1:14: error: register after thread_local\n"),
        },
        {
            "constexpr after thread_local", __LINE__,
            SVI("thread_local constexpr int x;\n"),
            SVI("(test):1:14: error: constexpr after thread_local\n"),
        },
        {
            "unsigned after __auto_type", __LINE__,
            SVI("__auto_type unsigned x = 1;\n"),
            SVI("(test):1:13: error: unsigned after __auto_type\n"),
        },
        {
            "int after __auto_type", __LINE__,
            SVI("__auto_type int x = 1;\n"),
            SVI("(test):1:13: error: int after __auto_type\n"),
        },
        {
            "long after __auto_type", __LINE__,
            SVI("__auto_type long x = 1;\n"),
            SVI("(test):1:13: error: long after __auto_type\n"),
        },
        {
            "char after __auto_type", __LINE__,
            SVI("__auto_type char x = 'a';\n"),
            SVI("(test):1:13: error: char after __auto_type\n"),
        },
        {
            "__int128 after __auto_type", __LINE__,
            SVI("__auto_type __int128 x = 1;\n"),
            SVI("(test):1:13: error: __int128 after __auto_type\n"),
        },
        {
            "duplicate short", __LINE__,
            SVI("short short x;\n"),
            SVI("(test):1:7: error: Duplicate short in declaration\n"),
        },
        {
            "short after __int128", __LINE__,
            SVI("__int128 short x;\n"),
            SVI("(test):1:10: error: short after __int128\n"),
        },
        {
            "short after __auto_type", __LINE__,
            SVI("__auto_type short x = 1;\n"),
            SVI("(test):1:13: error: short after __auto_type\n"),
        },
        {
            "extern after static", __LINE__,
            SVI("static extern int x;\n"),
            SVI("(test):1:8: error: extern after static\n"),
        },
        {
            "extern after typedef", __LINE__,
            SVI("typedef extern int x;\n"),
            SVI("(test):1:9: error: extern after typedef\n"),
        },
        {
            "extern after constexpr", __LINE__,
            SVI("constexpr extern int x = 1;\n"),
            SVI("(test):1:11: error: extern after constexpr\n"),
        },
        {
            "static after extern", __LINE__,
            SVI("extern static int x;\n"),
            SVI("(test):1:8: error: static after extern\n"),
        },
        {
            "static after typedef", __LINE__,
            SVI("typedef static int x;\n"),
            SVI("(test):1:9: error: static after typedef\n"),
        },
        {
            "inline after typedef", __LINE__,
            SVI("typedef inline int x;\n"),
            SVI("(test):1:9: error: inline after typedef\n"),
        },
        {
            "signed after __auto_type", __LINE__,
            SVI("__auto_type signed x = 1;\n"),
            SVI("(test):1:13: error: signed after __auto_type\n"),
        },
        {
            "long after __int128", __LINE__,
            SVI("__int128 long x;\n"),
            SVI("(test):1:10: error: long after __int128\n"),
        },
        {
            "double with int", __LINE__,
            SVI("int double x;\n"),
            SVI("(test):1:5: error: double with other types\n"),
        },
        {
            "auto after typedef", __LINE__,
            SVI("typedef auto x;\n"),
            SVI("(test):1:9: error: auto after typedef\n"),
        },
        {
            "redef var as typedef", __LINE__,
            SVI("int x;\n"
                "typedef float x;\n"),
            SVI("(test):2:16: error: redefinition of 'x' as a different kind of symbol\n"),
        },
        {
            "redef typedef as var", __LINE__,
            SVI("typedef int x;\n"
                "int x;\n"),
            SVI("(test):2:6: error: redefinition of 'x' as a different kind of symbol\n"),
        },
        {
            "redef var as function", __LINE__,
            SVI("int x;\n"
                "int x(void);\n"),
            SVI("(test):2:12: error: redefinition of 'x' as a different kind of symbol\n"),
        },
        {
            "redef var as function def", __LINE__,
            SVI("int x;\n"
                "int x(void){return 0;};\n"),
            SVI("(test):2:12: error: Redefinition of 'x' as a different kind of symbol\n"),
        },
        {
            "redef function as typedef", __LINE__,
            SVI("int x(void);\n"
                "typedef int x;\n"),
            SVI("(test):2:14: error: redefinition of 'x' as a different kind of symbol\n"),
        },
        {
            "redef enumerator as var", __LINE__,
            SVI("enum {X};\n"
                "int X;\n"),
            SVI("(test):2:6: error: redefinition of 'X' as a different kind of symbol\n"),
        },
        {
            "redef enumerator as typedef", __LINE__,
            SVI("enum {X};\n"
                "typedef int X;\n"),
            SVI("(test):2:14: error: redefinition of 'X' as a different kind of symbol\n"),
        },
        {
            "redef typedef as enumerator", __LINE__,
            SVI("typedef int X;\n"
                "enum {X};\n"),
            SVI("(test):2:7: error: idk\n"),
            .skip = 1,
        },
        {
            "local redef typedef as var", __LINE__,
            SVI("typedef int T;\n"
                "void f(void){ typedef T T; int T; }\n"),
            SVI("(test):2:33: error: redefinition of 'T' as a different kind of symbol\n"),
        },
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < arrlen(cases); i = test_atomic_increment(&idx)){
        struct ErrorCase* c = &cases[i];
        if(c->skip){
            TEST_stats.skipped++;
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
            .eager_parsing = 1,
        };
        fc_write_path(fc, "(test)", 6);
        int err = fc_cache_file(fc, c->input);
        if(err) {TestPrintf("%s:%d: failed to cache\n", __FILE__, c->line); goto fin;}
        err = cpp_define_builtin_macros(&cc.cpp);
        if(err) {TestPrintf("%s:%d: failed to define\n", __FILE__, c->line); goto fin;}
        err = cc_define_builtin_types(&cc);
        if(err) {TestPrintf("%s:%d: failed to define builtin types\n", __FILE__, c->line); goto fin;}
        err = cpp_include_file_via_file_cache(&cc.cpp, SV("(test)"));
        if(err) {TestPrintf("%s:%d: failed to include\n", __FILE__, c->line); goto fin;}
        err = cc_parse_all(&cc);
        TEST_stats.executed++;
        if(!err){
            TEST_stats.failures++;
            TestPrintf("%s:%d: %s: expected error but parsing succeeded\n", __FILE__, c->line, c->test);
        }
        if(1){
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
    static struct StructCase {
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
            SVI("struct S { int x; char y; };\n"),
            SVI("S"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("x"), .offset = 0 },
                { SVI("y"), .offset = 4 },
            },
        },
        {
            "struct with padding", __LINE__,
            SVI("struct P { char a; int b; char c; };\n"),
            SVI("P"), 0,
            .size = 12, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 4 },
                { SVI("c"), .offset = 8 },
            },
        },
        {
            "struct double alignment", __LINE__,
            SVI("struct D { char a; double b; };\n"),
            SVI("D"), 0,
            .size = 16, .alignment = 8,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 8 },
            },
        },
        {
            "packed struct", __LINE__,
            SVI("struct __attribute__((packed)) Pk { char a; int b; char c; };\n"),
            SVI("Pk"), 0,
            .size = 6, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 1 },
                { SVI("c"), .offset = 5 },
            },
        },
        {
            "packed struct double", __LINE__,
            SVI("struct __attribute__((packed)) PkD { char a; double b; };\n"),
            SVI("PkD"), 0,
            .size = 9, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 1 },
            },
        },
        {
            "aligned struct", __LINE__,
            SVI("struct __attribute__((aligned(16))) Al { int x; };\n"),
            SVI("Al"), 0,
            .size = 16, .alignment = 16,
            .fields = {
                { SVI("x"), .offset = 0 },
            },
        },
        {
            "__declspec(align) struct", __LINE__,
            SVI("__declspec(align(16)) struct Al2 { int x; };\n"),
            SVI("Al2"), 0,
            .size = 16, .alignment = 16,
            .fields = {
                { SVI("x"), .offset = 0 },
            },
        },
        {
            "__declspec(align) after struct keyword", __LINE__,
            SVI("struct __declspec(align(16)) Al3 { int x; };\n"),
            SVI("Al3"), 0,
            .size = 16, .alignment = 16,
            .fields = {
                { SVI("x"), .offset = 0 },
            },
        },
        {
            "[[gnu::aligned]] struct", __LINE__,
            SVI("struct [[gnu::aligned(16)]] Al4 { int x; };\n"),
            SVI("Al4"), 0,
            .size = 16, .alignment = 16,
            .fields = {
                { SVI("x"), .offset = 0 },
            },
        },
        {
            "[[gnu::packed]] struct", __LINE__,
            SVI("struct [[gnu::packed]] Pk2 { int x; char y; int z; };\n"),
            SVI("Pk2"), 0,
            .size = 9, .alignment = 1,
            .fields = {
                { SVI("x"), .offset = 0 },
                { SVI("y"), .offset = 4 },
                { SVI("z"), .offset = 5 },
            },
        },
        {
            "bitfield packing", __LINE__,
            SVI("struct BF { int a : 3; int b : 5; int c : 8; };\n"),
            SVI("BF"), 0,
            .size = 4, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SVI("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
                { SVI("c"), .offset = 0, .bitwidth = 8, .bitoffset = 8 },
            },
        },
        {
            "bitfield overflow to next unit", __LINE__,
            SVI("struct BF2 { int a : 30; int b : 5; };\n"),
            SVI("BF2"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 30, .bitoffset = 0 },
                { SVI("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "bitfield mixed with regular", __LINE__,
            SVI("struct BF3 { int a : 3; int x; int b : 5; };\n"),
            SVI("BF3"), 0,
            .size = 12, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SVI("x"), .offset = 4 },
                { SVI("b"), .offset = 8, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "simple union", __LINE__,
            SVI("union U { int i; double d; char c; };\n"),
            SVI("U"), 1,
            .size = 8, .alignment = 8,
            .fields = {
                { SVI("i"), .offset = 0 },
                { SVI("d"), .offset = 0 },
                { SVI("c"), .offset = 0 },
            },
        },
        {
            "union size is max", __LINE__,
            SVI("union U2 { char a; short b; int c; long d; };\n"),
            SVI("U2"), 1,
            .size = 8, .alignment = 8,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 0 },
                { SVI("c"), .offset = 0 },
                { SVI("d"), .offset = 0 },
            },
        },
        {
            "nested struct offsets", __LINE__,
            SVI("struct Inner { int a; int b; };\n"
               "struct Outer { char c; struct Inner s; int d; };\n"),
            SVI("Outer"), 0,
            .size = 16, .alignment = 4,
            .fields = {
                { SVI("c"), .offset = 0 },
                { SVI("s"), .offset = 4, .type_repr = SVI("struct Inner") },
                { SVI("d"), .offset = 12 },
            },
        },
        {
            "all chars no padding", __LINE__,
            SVI("struct Chars { char a; char b; char c; };\n"),
            SVI("Chars"), 0,
            .size = 3, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 1 },
                { SVI("c"), .offset = 2 },
            },
        },
        {
            "empty struct", __LINE__,
            SVI("struct Empty {};\n"),
            SVI("Empty"), 0,
            .size = 0, .alignment = 1,
        },
        {
            "struct with pointer", __LINE__,
            SVI("struct WP { char a; void *p; };\n"),
            SVI("WP"), 0,
            .size = 16, .alignment = 8,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("p"), .offset = 8 },
            },
        },
        {
            "struct with array", __LINE__,
            SVI("struct WA { int x; char buf[7]; int y; };\n"),
            SVI("WA"), 0,
            .size = 16, .alignment = 4,
            .fields = {
                { SVI("x"), .offset = 0 },
                { SVI("buf"), .offset = 4 },
                { SVI("y"), .offset = 12 },
            },
        },
        {
            "char short int alignment", __LINE__,
            SVI("struct CSI { char a; short b; int c; };\n"),
            SVI("CSI"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 2 },
                { SVI("c"), .offset = 4 },
            },
        },
        {
            "trailing padding", __LINE__,
            SVI("struct TP { int a; char b; };\n"),
            SVI("TP"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 4 },
            },
        },
        {
            "alignas on struct", __LINE__,
            SVI("_Alignas(32) struct AS { int x; };\n"
               "struct AS as;\n"),
            SVI("AS"), 0,
            .size = 4, .alignment = 4,
            .fields = {
                { SVI("x"), .offset = 0 },
            },
        },
        {
            "alignas on struct field", __LINE__,
            SVI("struct AF { char a; _Alignas(16) int b; };\n"
               "struct AF af;\n"),
            SVI("AF"), 0,
            .size = 32, .alignment = 16,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 16 },
            },
        },
        {
            "pragma pack(1)", __LINE__,
            SVI("#pragma pack(1)\n"
               "struct PP1 { char a; int b; char c; };\n"
               "#pragma pack()\n"),
            SVI("PP1"), 0,
            .size = 6, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 1 },
                { SVI("c"), .offset = 5 },
            },
        },
        {
            "__pragma pack(1)", __LINE__,
            SVI("__pragma(pack(1))\n"
               "struct MPP1 { char a; int b; char c; };\n"
               "__pragma(pack())\n"),
            SVI("MPP1"), 0,
            .size = 6, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 1 },
                { SVI("c"), .offset = 5 },
            },
        },
        {
            "__pragma pack in macro", __LINE__,
            SVI("#define PACK(n) __pragma(pack(n))\n"
               "PACK(1)\n"
               "struct MPP2 { char a; int b; char c; };\n"
               "PACK()\n"),
            SVI("MPP2"), 0,
            .size = 6, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 1 },
                { SVI("c"), .offset = 5 },
            },
        },
        {
            "pragma pack(2)", __LINE__,
            SVI("#pragma pack(2)\n"
               "struct PP2 { char a; int b; double c; };\n"
               "#pragma pack()\n"),
            SVI("PP2"), 0,
            .size = 14, .alignment = 2,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 2 },
                { SVI("c"), .offset = 6 },
            },
        },
        {
            "pragma pack push/pop", __LINE__,
            SVI("#pragma pack(push, 1)\n"
               "struct PPush { char a; int b; };\n"
               "#pragma pack(pop)\n"
               "struct PAfter { char a; int b; };\n"),
            SVI("PPush"), 0,
            .size = 5, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 1 },
            },
        },
        {
            "pragma pack pop restores", __LINE__,
            SVI("#pragma pack(push, 1)\n"
               "struct Ignore { char a; int b; };\n"
               "#pragma pack(pop)\n"
               "struct Restored { char a; int b; };\n"),
            SVI("Restored"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 4 },
            },
        },
        {
            "pragma pack(4) limits alignment", __LINE__,
            SVI("#pragma pack(4)\n"
               "struct PP4 { char a; double b; };\n"
               "#pragma pack()\n"),
            SVI("PP4"), 0,
            .size = 12, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 4 },
            },
        },
        {
            "pragma pack macro expansion", __LINE__,
            SVI("#define MYPACK 1\n"
               "#pragma pack(MYPACK)\n"
               "struct PPM { int a; char b; };\n"
               "#pragma pack()\n"),
            SVI("PPM"), 0,
            .size = 5, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 4 },
            },
        },
        {
            "pragma pack(push) macro expansion", __LINE__,
            SVI("#define P 1\n"
               "#pragma pack(push, P)\n"
               "struct PPPM { int a; char b; };\n"
               "#pragma pack(pop)\n"),
            SVI("PPPM"), 0,
            .size = 5, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 4 },
            },
        },
        {
            "pragma pack(push, ident)", __LINE__,
            SVI("#pragma pack(1)\n"
               "#pragma pack(push, A, 4)\n"
               "#pragma pack(push, 8)\n"
               "#pragma pack(pop)\n"
               "struct S { int a; char b; };\n"),
            SVI("S"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 4 },
            },
        },
        {
            "pragma pack(pop, ident)", __LINE__,
            SVI("#pragma pack(1)\n"
               "#pragma pack(push, A, 4)\n"
               "#pragma pack(push, 8)\n"
               "#pragma pack(pop, A)\n"
               "struct S { int a; char b; };\n"),
            SVI("S"), 0,
            .size = 5, .alignment = 1,
            .fields = {
                { SVI("a"), .offset = 0 },
                { SVI("b"), .offset = 4 },
            },
        },
        {
            "zero-width bitfield forces alignment", __LINE__,
            SVI("struct Z { int a : 3; int : 0; int b : 5; };\n"),
            SVI("Z"), 0,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { {0}, .offset = 4 },
                { SVI("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "FAM at end", __LINE__,
            SVI("struct F { int len; char data[]; };\n"),
            SVI("F"), 0,
            .size = 4, .alignment = 4,
            .fields = {
                { SVI("len"), .offset = 0 },
                { SVI("data"), .offset = 4 },
            },
        },
        {
            "FAM alignment", __LINE__,
            SVI("struct F { char c; int data[]; };\n"),
            SVI("F"), 0,
            .size = 4, .alignment = 4,
            .fields = {
                { SVI("c"), .offset = 0 },
                { SVI("data"), .offset = 4 },
            },
        },
        {
            "FAM double alignment", __LINE__,
            SVI("struct F { int n; double data[]; };\n"),
            SVI("F"), 0,
            .size = 8, .alignment = 8,
            .fields = {
                { SVI("n"), .offset = 0 },
                { SVI("data"), .offset = 8 },
            },
        },
        {
            "FAM only member", __LINE__,
            SVI("struct F { int data[]; };\n"),
            SVI("F"), 0,
            .size = 0, .alignment = 4,
            .fields = {
                { SVI("data"), .offset = 0 },
            },
        },
    };
    MStringBuilder sb = {0};
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < arrlen(cases); i = test_atomic_increment(&idx)){
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
    static struct BFCase {
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
            SVI("struct S { int a : 3; unsigned b : 5; };\n"),
            SVI("S"), CC_BITFIELD_SYSV,
            .size = 4, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SVI("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
            },
        },
        {
            "msvc: different types don't share", __LINE__,
            SVI("struct S { int a : 3; unsigned b : 5; };\n"),
            SVI("S"), CC_BITFIELD_MSVC,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SVI("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "msvc: same type shares", __LINE__,
            SVI("struct S { int a : 3; int b : 5; };\n"),
            SVI("S"), CC_BITFIELD_MSVC,
            .size = 4, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SVI("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
            },
        },
        {
            "sysv: different sizes pack", __LINE__,
            SVI("struct S { int a : 3; short b : 5; };\n"),
            SVI("S"), CC_BITFIELD_SYSV,
            .size = 4, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SVI("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
            },
        },
        {
            "msvc: different sizes don't share", __LINE__,
            SVI("struct S { int a : 3; short b : 5; };\n"),
            SVI("S"), CC_BITFIELD_MSVC,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SVI("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "msvc: three fields, type changes", __LINE__,
            SVI("struct S { int a : 3; int b : 5; unsigned c : 8; };\n"),
            SVI("S"), CC_BITFIELD_MSVC,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SVI("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
                { SVI("c"), .offset = 4, .bitwidth = 8, .bitoffset = 0 },
            },
        },
        {
            "sysv: three fields, same size", __LINE__,
            SVI("struct S { int a : 3; int b : 5; unsigned c : 8; };\n"),
            SVI("S"), CC_BITFIELD_SYSV,
            .size = 4, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { SVI("b"), .offset = 0, .bitwidth = 5, .bitoffset = 3 },
                { SVI("c"), .offset = 0, .bitwidth = 8, .bitoffset = 8 },
            },
        },
        {
            "sysv: zero-width ends run", __LINE__,
            SVI("struct S { int a : 3; int : 0; int b : 5; };\n"),
            SVI("S"), CC_BITFIELD_SYSV,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 3, .bitoffset = 0 },
                { {0} },
                { SVI("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "sysv: zero-width when no run is noop", __LINE__,
            SVI("struct S { int x; int : 0; int a : 5; };\n"),
            SVI("S"), CC_BITFIELD_SYSV,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("x"), .offset = 0 },
                { {0} },
                { SVI("a"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "sysv: overflow to next storage unit", __LINE__,
            SVI("struct S { int a : 30; int b : 5; };\n"),
            SVI("S"), CC_BITFIELD_SYSV,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 30, .bitoffset = 0 },
                { SVI("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
        {
            "msvc: overflow to next storage unit", __LINE__,
            SVI("struct S { int a : 30; int b : 5; };\n"),
            SVI("S"), CC_BITFIELD_MSVC,
            .size = 8, .alignment = 4,
            .fields = {
                { SVI("a"), .offset = 0, .bitwidth = 30, .bitoffset = 0 },
                { SVI("b"), .offset = 4, .bitwidth = 5, .bitoffset = 0 },
            },
        },
    };
    MStringBuilder sb = {0};
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < arrlen(cases); i = test_atomic_increment(&idx)){
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
        fc_write_path(fc, "(test)", sizeof "(test)" - 1);
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
#ifdef USE_TESTING_ALLOCATOR
    testing_allocator_init();
#endif
    RegisterTestFlags(test_parse_decls, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_parse_errors, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_struct_layout, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_bitfield_abi, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    int err = test_main(argc, argv, NULL);
#ifdef USE_TESTING_ALLOCATOR
    testing_assert_all_freed();
#endif
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
