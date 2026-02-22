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

static void cc_print_type(MStringBuilder* sb, CcQualType t);
TestFunction(test_parse_decls){
    TESTBEGIN();
    enum {N=8}; // can increase if we need to
    struct Case {
        const char* test; int line;
        StringView input; 
        struct {
            StringView name;
            StringView repr;
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

#include "../Drp/Allocators/allocator.c"
#include "../Drp/file_cache.c"
#include "cpp_preprocessor.c"
#include "cc_lexer.c"
#include "cc_parser.c"
