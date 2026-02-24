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
        } vars[N];
        struct {
            StringView name;
            StringView repr;
        } funcs[N];
        struct {
            StringView name;
            StringView repr;
        } typedefs[N];

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
    };
    for(size_t i = 0; i < arrlen(testcases); i++){
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
            .lexer = {
                .cpp = {
                    .allocator = al,
                    .fc = fc,
                    .at = &at,
                    .logger = logger,
                    .env = &env,
                    .target = cc_target_test(),
                },
            },
            .current = &cc.global,
        };
        struct Case* c = &testcases[i];
        fc_write_path(fc, "(test)", 6);
        err = fc_cache_file(fc, c->input);
        if(err) {TestPrintf("%s:%d: failed to cache", __FILE__, c->line); goto finally;}
        err = cpp_define_builtin_macros(&cc.lexer.cpp);
        if(err) {TestPrintf("%s:%d: failed to define", __FILE__, c->line); goto finally;}
        err = cpp_include_file_via_file_cache(&cc.lexer.cpp, SV("(test)"));
        if(err) {TestPrintf("%s:%d: failed to include", __FILE__, c->line); goto finally;}
        for(_Bool finished = 0; !finished;){
            err = cc_parse_top_level(&cc, &finished);
            if(err) {TestPrintf("%s:%d: failed to parse", __FILE__, c->line); goto finally;}
        }
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
            test_expect_equals_sv(r, c->vars[n].repr, "actual", "expected", &TEST_stats, __FILE__, __func__, c->line);
            if(c->vars[n].init.length){
                TestExpectTrue(var->initializer);
                if(var->initializer){
                    msb_reset(&sb);
                    CcExpr* init = var->initializer;
                    cc_print_expr(&sb, init);
                    if(sb.errored) { err = 1; TestReport("allocation failure"); goto finally; }
                    StringView ir = msb_borrow_sv(&sb);
                    test_expect_equals_sv(ir, c->vars[n].init, "actual init", "expected init", &TEST_stats, __FILE__, __func__, c->line);
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
            test_expect_equals_sv(r, c->funcs[n].repr, "actual", "expected", &TEST_stats, __FILE__, __func__, c->line);
        }
        for(size_t n = 0; n < N; n++){
            StringView name = c->typedefs[n].name;
            if(!name.length) break;
            Atom a = AT_get_atom(&at, name.text, name.length);
            if(!a) {err = 1; goto finally;}
            CcQualType t = cc_scope_lookup_typedef(&cc.global, a, CC_SCOPE_NO_WALK);
            TestExpectNotEquals(t.bits, (uintptr_t)-1);
            if(t.bits == (uintptr_t)-1){
                TestPrintf("%s:%d: %s %.*s is undefined\n", __FILE__, c->line, c->test, sv_p(name));
                continue;
            }
            msb_reset(&sb);
            cc_print_type(&sb, t);
            if(sb.errored) { err = 1; TestReport("allocation failure"); goto finally; }
            StringView r = msb_borrow_sv(&sb);
            test_expect_equals_sv(r, c->typedefs[n].repr, "actual", "expected", &TEST_stats, __FILE__, __func__, c->line);
        }
        finally:
        if(log_sb.cursor && ! log_sb.errored){
            StringView sv = msb_borrow_sv(&log_sb);
            TestPrintf("%s:%d: %s %.*s\n", __FILE__, c->line, c->test, sv_p(sv));
        }
        TestExpectFalse(err);
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&cc.lexer.cpp.synth_arena);
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
            .lexer = {
                .cpp = {
                    .allocator = al,
                    .fc = fc,
                    .at = &at,
                    .logger = logger,
                    .env = &env,
                    .target = cc_target_test(),
                },
            },
            .current = &cc.global,
        };
        struct ErrorCase* c = &cases[i];
        fc_write_path(fc, "(test)", 6);
        int err = fc_cache_file(fc, c->input);
        if(err) {TestPrintf("%s:%d: failed to cache", __FILE__, c->line); goto fin;}
        err = cpp_define_builtin_macros(&cc.lexer.cpp);
        if(err) {TestPrintf("%s:%d: failed to define", __FILE__, c->line); goto fin;}
        err = cpp_include_file_via_file_cache(&cc.lexer.cpp, SV("(test)"));
        if(err) {TestPrintf("%s:%d: failed to include", __FILE__, c->line); goto fin;}
        for(_Bool finished = 0; !finished;){
            err = cc_parse_top_level(&cc, &finished);
            if(err) break;
        }
        TEST_stats.executed++;
        if(!err){
            TEST_stats.failures++;
            TestPrintf("%s:%d: %s: expected error but parsing succeeded\n", __FILE__, c->line, c->test);
        }
        if(c->expected_msg.length){
            StringView log = msb_borrow_sv(&log_sb);
            test_expect_equals_sv(log, c->expected_msg, "actual error", "expected error", &TEST_stats, __FILE__, __func__, c->line);
        }
        fin:
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&cc.lexer.cpp.synth_arena);
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
            .size = 32, .alignment = 32,
            .fields = {
                { SV("x"), .offset = 0 },
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
            .lexer = {
                .cpp = {
                    .allocator = al,
                    .fc = fc,
                    .at = &at,
                    .logger = logger,
                    .env = &env,
                    .target = cc_target_test(),
                },
            },
            .current = &cc.global,
        };
        struct StructCase* c = &cases[i];
        fc_write_path(fc, "(test)", 6);
        err = fc_cache_file(fc, c->input);
        if(err) {TestReport("failed to cache"); goto fin;}
        err = cpp_define_builtin_macros(&cc.lexer.cpp);
        if(err) {TestReport("failed to define builtins"); goto fin;}
        err = cc_register_pragmas(&cc);
        if(err) {TestReport("failed to register pragmas"); goto fin;}
        err = cpp_include_file_via_file_cache(&cc.lexer.cpp, SV("(test)"));
        if(err) {TestReport("failed to include"); goto fin;}
        for(_Bool finished = 0; !finished;){
            err = cc_parse_top_level(&cc, &finished);
            if(err) {TestReport("failed to parse"); goto fin;}
        }
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
                    test_expect_equals_sv(actual_name, fe->name, "field name", "expected", &TEST_stats, __FILE__, __func__, c->line);
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
                    test_expect_equals_sv(r, fe->type_repr, "field type", "expected type", &TEST_stats, __FILE__, __func__, c->line);
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
        ArenaAllocator_free_all(&cc.lexer.cpp.synth_arena);
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
            .lexer = {
                .cpp = {
                    .allocator = al,
                    .fc = fc,
                    .at = &at,
                    .logger = logger,
                    .env = &env,
                    .target = tgt,
                },
            },
            .current = &cc.global,
        };
        struct BFCase* c = &cases[i];
        fc_write_path(fc, "(test)", 6);
        err = fc_cache_file(fc, c->input);
        if(err) {TestReport("failed to cache"); goto bffin;}
        err = cpp_define_builtin_macros(&cc.lexer.cpp);
        if(err) {TestReport("failed to define builtins"); goto bffin;}
        err = cpp_include_file_via_file_cache(&cc.lexer.cpp, SV("(test)"));
        if(err) {TestReport("failed to include"); goto bffin;}
        for(_Bool finished = 0; !finished;){
            err = cc_parse_top_level(&cc, &finished);
            if(err) {TestReport("failed to parse"); goto bffin;}
        }
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
        ArenaAllocator_free_all(&cc.lexer.cpp.synth_arena);
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
#include "cc_lexer.c"
#include "cc_parser.c"
