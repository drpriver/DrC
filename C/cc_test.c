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
        if(err) {TestReport("failed to cache"); goto finally;}
        err = cpp_define_builtin_macros(&cc.lexer.cpp);
        if(err) {TestReport("failed to define"); goto finally;}
        err = cpp_include_file_via_file_cache(&cc.lexer.cpp, SV("(test)"));
        if(err) {TestReport("failed to include"); goto finally;}
        for(_Bool finished = 0; !finished;){
            err = cc_parse_top_level(&cc, &finished);
            if(err) {TestReport("failed to parse"); goto finally;}
        }
        for(size_t n = 0; n < N; n++){
            StringView name = c->vars[n].name;
            if(!name.length) break;
            Atom a = AT_get_atom(&at, name.text, name.length);
            if(!a) {err = 1; goto finally;}
            CcVariable* var = cc_scope_lookup_var(&cc.global, a, CC_SCOPE_NO_WALK);
            TestExpectTrue(var);
            if(!var){
                TestPrintf("%s:%d: %s %.*s is undefined\n", __FILE__, c->line, c->test, sv_p(name));
                continue;
            }
            msb_reset(&sb);
            cc_print_type(&sb, var->type);
            if(sb.errored) { err = 1; TestReport("allocation failure"); goto finally; }
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

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(test_parse_decls);
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
