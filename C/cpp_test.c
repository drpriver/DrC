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
#include "cpp_preprocessor.h"

#include "../Drp/compiler_warnings.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static int cpp_next_pp_token(CPreprocessor* cpp, CPPToken* ptok);
enum {SKIP = 1};

// Helper to run preprocessor on a string and collect output tokens as a string
static
int
cpp_expand_string(StringView txt, StringView* out, const char* file, const char* func, int line){
    int result = 0;
    ArenaAllocator aa = {0};
    Allocator a = allocator_from_arena(&aa);
    FileCache *fc = fc_create(a);
    MStringBuilder log_sb = {.allocator=a};
    Logger logger = msb_logger(&log_sb);
    AtomTable at = {.allocator = a};
    Environment env = {.allocator = a, .at=&at};
    int err;
    err = env_setenv4(&env, "wolo", 4, "woo", 3);
    if(err) goto finally;
    CPreprocessor cpp = {
        .allocator = a,
        .fc = fc,
        .at = &at,
        .logger = &logger,
        .env = &env,
    };
    fc_write_path(fc, "(test)", 6);
    err = fc_cache_file(fc, txt);
    if(err){
        result = 1;
        goto finally;
    }
    err = cpp_define_builtin_macros(&cpp);
    if(err){
        result = 1;
        goto finally;
    }
    CPPFrame frame = {
        .file_id = (uint32_t)fc->map.count - 1,
        .txt = txt,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CPPFrame)(&cpp.frames, cpp.allocator, frame);
    if(err){
        result = 1;
        goto finally;
    }
    MStringBuilder sb = {.allocator = MALLOCATOR};
    CPPToken tok;
    for(;;){
        err = cpp_next_pp_token(&cpp, &tok);
        if(err){
            msb_destroy(&sb);
            result = 1;
            goto finally;
        }
        if(tok.type == CPP_EOF) break;
        if(tok.type == CPP_WHITESPACE)
            msb_write_char(&sb, ' ');
        else if(tok.type == CPP_NEWLINE)
            msb_write_char(&sb, '\n');
        else
            msb_write_str(&sb, tok.txt.text, tok.txt.length);
    }
    StringView s = msb_detach_sv(&sb);
    *out = s;

    finally:
    if(log_sb.cursor){
        StringView sv = msb_borrow_sv(&log_sb);
        TestPrintf("%s%s:%d:%s%s\n    %.*s", _test_color_gray, file, line, func, _test_color_reset, sv_p(sv));
    }
    ArenaAllocator_free_all(&aa);
    ArenaAllocator_free_all(&cpp.synth_arena);
    return result;
}
// Like cpp_expand_string, but expects an error and captures the error log output.
static
int
cpp_expand_string_expect_error(StringView txt, StringView* err_out){
    ArenaAllocator aa = {0};
    Allocator a = allocator_from_arena(&aa);
    FileCache *fc = fc_create(a);
    MStringBuilder log_sb = {.allocator=a};
    Logger logger = msb_logger(&log_sb);
    AtomTable at = {.allocator = a};
    Environment env = {.allocator = a, .at=&at};
    CPreprocessor cpp = {
        .allocator = a,
        .fc = fc,
        .at = &at,
        .logger = &logger,
        .env = &env,
    };
    fc_write_path(fc, "(test)", 6);
    int err = fc_cache_file(fc, txt);
    if(err) goto finally;
    err = cpp_define_builtin_macros(&cpp);
    if(err) goto finally;
    CPPFrame frame = {
        .file_id = (uint32_t)fc->map.count - 1,
        .txt = txt,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CPPFrame)(&cpp.frames, cpp.allocator, frame);
    if(err) goto finally;
    CPPToken tok;
    for(;;){
        err = cpp_next_pp_token(&cpp, &tok);
        if(err) break;
        if(tok.type == CPP_EOF) break;
    }

    finally:;
    int result = err ? 0 : 1; // 0 = success (error was expected), 1 = failure (no error)
    if(log_sb.cursor){
        StringView sv = msb_borrow_sv(&log_sb);
        *err_out = (StringView){.length = sv.length, .text = (const char*)Allocator_dupe(MALLOCATOR, sv.text, sv.length)};
    }
    else
        *err_out = (StringView){0};
    ArenaAllocator_free_all(&aa);
    ArenaAllocator_free_all(&cpp.synth_arena);
    return result;
}

TestFunction(test_func_macros){
    TESTBEGIN();
    struct {
        const char* name; StringView inp, exp; int line;
    } test_cases[] = {
        {"simple_func_macro",SV("#define F(x) x*x\nF(2)"), SV("\n2*2"), __LINE__},
        {"multi_param_macro",SV("#define ADD(a,b) a+b\nADD(1, 2)"), SV("\n1+2"), __LINE__},
        {"stringify",SV("#define STR(x) #x\nSTR(hello)"), SV("\n\"hello\""), __LINE__},
        {"stringify_with_spaces",SV("#define STR(x) #x\nSTR(hello world)"),SV("\n\"hello world\""),__LINE__},
        {"token_paste",SV("#define PASTE(a,b) a##b\nPASTE(foo, bar)"), SV("\nfoobar"), __LINE__},
        {"token_paste_3",SV("#define PASTE3(a,b,c) a##b##c\nPASTE3(x,y,z)"),SV("\nxyz"),__LINE__},
        {"paste_empty_left",SV("#define PASTE(a,b) a##b\nPASTE(,bar)"),SV("\nbar"), __LINE__},
        {"paste_empty_right",SV("#define PASTE(a,b) a##b\nPASTE(foo,)"),SV("\nfoo"), __LINE__},
        {"va_args",SV("#define VA(...) __VA_ARGS__\nVA(a, b, c)"),SV("\na, b, c"),__LINE__},
        {"va_args_empty",SV("#define VA(...) [__VA_ARGS__]\nVA()"),SV("\n[]"),__LINE__},
        {"va_opt_nonempty",SV("#define VA_OPT(...) prefix __VA_OPT__(, __VA_ARGS__) suffix\nVA_OPT(1, 2)"),SV("\nprefix , 1, 2 suffix"),__LINE__},
        {"va_opt_empty",SV("#define VA_OPT(...) prefix __VA_OPT__(, __VA_ARGS__) suffix\nVA_OPT()"),SV("\nprefix  suffix"),__LINE__},
        {"nested_expansion",SV(
            "#define F(x) x*x\n"
            "#define IDENT(x) x\n"
            "IDENT(F(3))"),SV("\n\n3*3"), __LINE__},
        {"indirect_stringify",SV(
            "#define STR(x) #x\n"
            "#define XSTR(x) STR(x)\n"
            "#define VALUE 42\n"
            "XSTR(VALUE)"),SV("\n\n\n\"42\""), __LINE__},
        {"direct_stringify_no_expand",SV(
            "#define STR(x) #x\n"
            "#define VALUE 42\n"
            "STR(VALUE)"),SV("\n\n\"VALUE\""), __LINE__},
        {"empty_macro_in_arg",SV(
        "#define EMPTY() \n"
        "#define IDENT(x) x\n"
        "IDENT(EMPTY())test"),SV("\n\ntest"), __LINE__},
        {"stringify_va_args",SV("#define LOG(...) #__VA_ARGS__\nLOG(hello, world)"),SV("\n\"hello, world\""),__LINE__},
        {"variadic_with_named_param", SV(
        "#define FOO(x, ...) x __VA_ARGS__\n"
        "FOO(1)"), SV("\n1 "), __LINE__},
        {"variadic_with_named_param_and_args", SV("#define FOO(x, ...) x __VA_ARGS__\nFOO(1, 2, 3)"), SV("\n1 2, 3"), __LINE__},
        {"variadic_with_named_param_va_opt", SV("#define FOO(x, ...) x __VA_OPT__(, __VA_ARGS__)\nFOO(1)"), SV("\n1 "), __LINE__},
        {"variadic_with_named_param_va_opt_nonempty", SV( "#define FOO(x, ...) x __VA_OPT__(, __VA_ARGS__)\nFOO(1, 2, 3)"),SV("\n1 , 2, 3"), __LINE__},
        // __VA_OPT__ emptiness is determined after expansion (C23 6.10.4.1)
        {"va_opt_expands_to_empty", SV(
            "#define EMPTY\n"
            "#define F(...) __VA_OPT__(yes)no\n"
            "F(EMPTY)"), SV("\n\nno"), __LINE__},
        {"va_opt_expands_to_nonempty", SV(
            "#define ONE 1\n"
            "#define F(...) __VA_OPT__(yes)no\n"
            "F(ONE)"), SV("\n\nyesno"), __LINE__},
        // param used both raw (## adjacent) and expanded in same replacement
        {"param_raw_and_expanded", SV(
            "#define X 1\n"
            "#define F(a) a a ## 2\n"
            "F(X)"), SV("\n\n1 X2"), __LINE__},
        // __VA_OPT__ with nested parens in content
        {"va_opt_nested_parens", SV(
            "#define F(...) __VA_OPT__((a, b))\n"
            "F(x)"), SV("\n(a, b)"), __LINE__},
        // multiple __VA_OPT__ in one replacement list
        {"va_opt_multiple", SV(
            "#define F(...) [__VA_OPT__(L)] [__VA_OPT__(R)]\n"
            "F(x)\n"
            "F()"), SV("\n[L] [R]\n[] []"), __LINE__},
        // __VA_OPT__ as left operand of ##
        {"va_opt_paste_left", SV(
            "#define F(...) __VA_OPT__(x) ## y\n"
            "F(1)\n"
            "F()"), SV("\nxy\ny"), __LINE__},
        // empty __VA_OPT__ content with non-empty VA_ARGS
        {"va_opt_empty_content", SV(
            "#define F(...) a __VA_OPT__() b\n"
            "F(x)"), SV("\na  b"), __LINE__},
        // # __VA_OPT__ with ## inside content
        {"stringify_va_opt_with_paste", SV(
            "#define F(...) #__VA_OPT__(a ## b)\n"
            "F(x)\n"
            "F()"), SV("\n\"ab\"\n\"\""), __LINE__},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "exp", "result", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s%s:%d:%s %s failed\n", _test_color_gray, __FILE__, line, _test_color_reset, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}
TestFunction(test_func_macros_extensions){
    TESTBEGIN();
    struct {
        const char* name; StringView inp, exp; int line;
    } test_cases[] = {
        // ## __VA_ARGS
        {"gnu va args comma",SV("#define F(...) f(1, ##__VA_ARGS__)\nF(2)\nF(2,3)\nF()"), SV("\nf(1,2)\nf(1,2,3)\nf(1)"), __LINE__},
        {"gnu va args comma2",SV("#define F(...) f(1, ## __VA_ARGS__)\nF(2)\nF(2,3)\nF()"), SV("\nf(1,2)\nf(1,2,3)\nf(1)"), __LINE__},
        {"gnu va args nocomma",SV("#define F(...) f(1 ##__VA_ARGS__)\nF(2)\nF(2, 3)\nF()"), SV("\nf(12)\nf(12, 3)\nf(1)"), __LINE__},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "exp", "result", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s%s:%d:%s %s failed\n", _test_color_gray, __FILE__, line, _test_color_reset, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

TestFunction(test_obj_macros){
    TESTBEGIN();
    struct {
        const char* name; StringView inp, exp; int line;
    } test_cases[] = {
        {"recursive_self_reference", SV("#define X X\nX"), SV("\nX"), __LINE__},
        {"mutual_recursion", SV(
        "#define A B*B\n"
        "#define B C*C\n"
        "#define C A*A\n"
        "A"), SV("\n\n\nA*A*A*A*A*A*A*A"), __LINE__},
        {"empty_obj_macro", SV("#define Z\nZ"), SV("\n"), __LINE__},
        {"self_ref", SV("#define Y Y\nY"), SV("\nY"), __LINE__},
        {"parens_macro", SV("#define PARENS ()\nPARENS"), SV("\n()"), __LINE__},
        // ## in object-like macros should paste during expansion
        {"obj_paste_simple", SV("#define AB a ## b\nAB"), SV("\nab"), __LINE__},
        {"obj_paste_hash", SV("#define HH # ## #\nHH"), SV("\n##"), __LINE__},
        {"obj_paste_multi", SV("#define XYZ x ## y ## z\nXYZ"), SV("\nxyz"), __LINE__},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "result", "exp", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

TestFunction(test_for_each){
    TESTBEGIN();
    StringView result;

    // The complex FOR_EACH pattern from foo.c
    int err = cpp_expand_string(SV(
        "#define F(x) x*x\n"
        "#define PARENS ()\n"
        "#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))\n"
        "#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))\n"
        "#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))\n"
        "#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))\n"
        "#define EXPAND1(...) __VA_ARGS__\n"
        "#define FOR_EACH(macro, ...) __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))\n"
        "#define FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(FOR_EACH_AGAIN PARENS (macro, __VA_ARGS__))\n"
        "#define FOR_EACH_AGAIN() FOR_EACH_HELPER\n"
        "FOR_EACH(F, a, b, c, 1, 2, 3)"), &result, __FILE__, __func__, __LINE__);
    // 10 newlines from #defines, then the expansion
    TestAssertFalse(err);
    TestExpectEqualsSv(SV("\n\n\n\n\n\n\n\n\n\na*a b*b c*c 1*1 2*2 3*3"), result);

    Allocator_free(MALLOCATOR, result.text, result.length);
    TESTEND();
}

TestFunction(test_for_each_empty){
    TESTBEGIN();
    StringView result;

    // FOR_EACH with no arguments should expand to nothing
    int err = cpp_expand_string(SV(
        "#define F(x) x*x\n"
        "#define PARENS ()\n"
        "#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))\n"
        "#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))\n"
        "#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))\n"
        "#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))\n"
        "#define EXPAND1(...) __VA_ARGS__\n"
        "#define FOR_EACH(macro, ...) __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))\n"
        "#define FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(FOR_EACH_AGAIN PARENS (macro, __VA_ARGS__))\n"
        "#define FOR_EACH_AGAIN() FOR_EACH_HELPER\n"
        "FOR_EACH(F)x"), &result, __FILE__, __func__, __LINE__);
    TestAssertFalse(err);
    // 10 newlines from #defines, then just "x" (FOR_EACH with no varargs expands to nothing)
    TestExpectEqualsSv(SV("\n\n\n\n\n\n\n\n\n\nx"), result);

    Allocator_free(MALLOCATOR, result.text, result.length);
    TESTEND();
}

TestFunction(test_c23){
    TESTBEGIN();
    StringView result;
    int err;
    struct {
        const char* name;
        int line;
        StringView input, output;
    } test_cases[] = {
        {
            "Example 1", __LINE__,
            SV(
            "#define LPAREN() (\n"
            "#define G(Q) 42\n"
            "#define F(R, X, ...) __VA_OPT__(G R X) )\n"
            "int x = F(LPAREN(), 0, <:-);\n" // replaced by int x = 42;
            ),
            SV("\n\n\nint x = 42;\n")
        },
        {
            "Example 2", __LINE__,
            SV(
                "#define F(...) f(0 __VA_OPT__(,) __VA_ARGS__)\n"
                "#define G(X, ...) f(0, X __VA_OPT__(,) __VA_ARGS__)\n"
                "#define SDEF(sname, ...) S sname __VA_OPT__(= {__VA_ARGS__ })\n"
                "#define EMP\n"
                "F(a, b, c)\n" // replaced by f(0, a, b, c)
                "F()\n" // replaced by f(0)
                "F(EMP)\n" // replaced by f(0)
                "G(a, b, c)\n" // replaced by f(0, a, b, c)
                "G(a, )\n" // replaced by f(0, a)
                "G(a)\n" // replaced by f(0, a)
                "SDEF(foo);\n" // replaced by S foo;
                "SDEF(bar, 1, 2);\n" // replaced by S bar = { 1, 2 };
                "//#define H1(X, ...) X __VA_OPT__(##) __VA_ARGS__\n"
                "\n"// error: ## on line above
                "\n"// cannot appear at the beginning of a replacement
                "\n"// list (6.10.5.4)
                "#define H2(X, Y, ...) __VA_OPT__(X ## Y,) __VA_ARGS__\n"
                "H2(a, b, c, d)\n" // replaced by ab, c, d
                "#define H3(X, ...) #__VA_OPT__(X##X X##X)\n"
                "H3(, 0)\n" // replaced by""
                "#define H4(X, ...) __VA_OPT__(a X ## X) ## b\n"
                "H4(, 1)\n" // replaced by a b
                "#define H5A(...) __VA_OPT__()/**/__VA_OPT__()\n"
                "#define H5B(X) a ## X ## b\n"
                "#define H5C(X) H5B(X)\n"
                "H5C(H5A())\n" // replaced by ab
            ),
            SV(
                "\n\n\n\n"
                "f(0 , a, b, c)\n" // F(a, b, c)"
                "f(0  )\n" // F()
                "f(0  )\n" // F(EMP)
                "f(0, a , b, c)\n" // G(a, b, c)
                "f(0, a  )\n" // G(a, )
                "f(0, a  )\n" // G(a)
                "S foo ;\n" // SDEF(foo);
                "S bar = {1, 2 };\n" // SDEF(bar, 1, 2);
                " \n"
                "\n\n\n\n"
                "ab, c, d\n" // H2(a, b, c, d)
                "\n"
                "\"\"\n" // H3(, 0)
                "\n"
                "a b\n" // H4(, 1)
                "\n\n\n"
                "ab\n" // H5C(H5A())
            )
        },
        {
            "Unnamed Example", __LINE__,
            SV( "#define hash_hash # ## #\n"
                "#define mkstr(a) # a\n"
                "#define in_between(a) mkstr(a)\n"
                "#define join(c, d) in_between(c hash_hash d)\n"
                "join(x, y)"),
            SV("\n\n\n\n\"x ## y\""),
        },

        {
            "Ambiguous Example", __LINE__,
            SV("#define f(a) a*g\n"
               "#define g(a) f(a)\n"
               "f(2)(9)"),
            // standard says either of these are allowed,
            // but actual compilers produce the second one
            // SV("\n\n2*f(9)"),
            SV("\n\n2*9*g"),
        },
        // 6.10.5.6
        {
            "example 1", __LINE__,
            SV( "#define TABSIZE 100\n"
                "int table[TABSIZE];\n"),
            SV("\nint table[100];\n"),
        },
        {
            "example 2 (extended)", __LINE__,
            SV("#define max(a, b) ((a) > (b) ? (a): (b))\nmax(1,2)\n"),
            SV("\n((1) > (2) ? (1): (2))\n"),
        },
        {
            "Redef etc. (example 3)", __LINE__,
            SV( "#define x 3\n"
                "#define f(a) f(x * (a))\n"
                "#undef x\n"
                "#define x 2\n"
                "#define g f\n"
                "#define z z[0]\n"
                "#define h g(~\n"
                "#define m(a) a(w)\n"
                "#define w 0,1\n"
                "#define t(a) a\n"
                "#define p() int\n"
                "#define q(x) x\n"
                "#define r(x,y) x## y\n"
                "#define str(x) # x\n"
                "f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);\n"
                "g(x+(3,4)-w) | h 5) & m\n"
                "(f)^m(m);\n"
                "p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };\n"
                "char c[2][6] = { str(hello), str() };\n"),
            SV( "\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                "f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);\n"
                "f(2 * (2+(3,4)-0,1)) | f(2 * (~ 5)) & f(2 * (0,1))^m(0,1);\n"
                "int i[] = { 1, 23, 4, 5,  };\n"
                "char c[2][6] = { \"hello\", \"\" };\n"),
        },
        {
            "example 4", __LINE__,
            SV( "#define str(s) # s\n"
                "#define xstr(s) str(s)\n"
                "#define debug(s, t) printf(\"x\" # s \"= %d, x\" # t \"= %s\", \\\n"
                " x ## s, x ## t)\n"
                "#define INCFILE(n) vers ## n\n"
                "#define glue(a, b) a ## b\n"
                "#define xglue(a, b) glue(a, b)\n"
                "#define HIGHLOW \"hello\"\n"
                "#define LOW LOW \", world\"\n"
                "debug(1, 2);\n"
                "fputs(str(strncmp(\"abc\\0d\", \"abc\", ’\\4’) // this goes away\n"
                "== 0) str(: @\n), s);\n"
                "xstr(INCFILE(2).h)\n"
                "glue(HIGH, LOW);\n"
                "xglue(HIGH, LOW)\n"),
            SV( "\n\n\n\n\n\n\n\n"
                "printf(\"x\" \"1\" \"= %d, x\" \"2\" \"= %s\", x1, x2);\n"
                "fputs(\"strncmp(\\\"abc\\\\0d\\\", \\\"abc\\\", ’\\4’) == 0\" \": @\", s);\n"
                "\"vers2.h\"\n"
                "\"hello\";\n"
                "\"hello\" \", world\"\n"),
        },
        {
            "placeholders example (example 5)", __LINE__,
            SV( "#define t(x,y,z) x ## y ## z\n"
                "int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,),\n"
                "t(10,,), t(,11,), t(,,12), t(,,) };\n"),
            SV( "\n"
                "int j[] = { 123, 45, 67, 89,\n"
                "10, 11, 12,  };\n"),
        },
        {
            "redefs (example 6)", __LINE__,
            SV( "#define OBJ_LIKE (1-1)\n"
                "#define OBJ_LIKE /* white space */ (1-1) /* other */\n"
                "#define FUNC_LIKE(a) ( a )\n"
                "#define FUNC_LIKE( a ) ( /* note the white space */ \\\n"
                "a /* other stuff on this line\n"
                "*/)\n"),
            SV("\n\n\n\n"),
        },
        {
            "redefs2", __LINE__,
            SV( "#define OBJ_LIKE (/**/ 1-1)\n"
                "#define OBJ_LIKE /* white space */ ( 1-1) /* other */\n"
                "#define FUNC_LIKE(a) ( a/* */)\n"
                "#define FUNC_LIKE( a ) ( /* note the white space */ \\\n"
                "a /* other stuff on this line\n"
                "*/)\n"),
            SV("\n\n\n\n"),
        },
        {
            "6.10.5 example 7", __LINE__,
            SV( "#define debug(...) fprintf(stderr, __VA_ARGS__)\n"
                "#define showlist(...) puts(#__VA_ARGS__)\n"
                "#define report(test, ...) ((test)?puts(#test):\\\n"
                " printf(__VA_ARGS__))\n"
                "debug(\"Flag\");\n"
                "debug(\"X = %d\\n\", x);\n"
                "showlist(The first, second, and third items.);\n"
                "report(x>y, \"x is %d but y is %d\", x, y);\n"),
            SV( "\n\n\n"
                "fprintf(stderr, \"Flag\");\n"
                "fprintf(stderr, \"X = %d\\n\", x);\n"
                "puts(\"The first, second, and third items.\");\n"
                "((x>y)?puts(\"x>y\"): printf(\"x is %d but y is %d\", x, y));\n"),
        },
    };
    for(size_t i = 0; i < sizeof test_cases / sizeof test_cases[0]; i++){
        int line = test_cases[i].line;
        err = cpp_expand_string(test_cases[i].input, &result, __FILE__, __func__, line);
        TestExpectFalse(err);
        if(err){
            TestPrintf("%s%s:%d: %s%s test_cases[%zu].input = \"%.*s\"\n", _test_color_gray, __FILE__, __LINE__, __func__, _test_color_reset, i, sv_p(test_cases[i].input));
            continue;
        }
        StringView expected = test_cases[i].output;
        if(!test_expect_equals_sv(expected, result, "expected", "result", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

TestFunction(test_torture){
    TESTBEGIN();
    StringView result;
    int err;
    struct {
        const char* name; int line;
        StringView in, out;
    } test_cases[] = {
        {
            "", __LINE__,
            SV("#define S(x) #x\n"
               "S(\"hello\\\\\")\n"),
            SV("\n"
               "\"\\\"hello\\\\\\\\\\\"\"\n"),
        },
        {
            "", __LINE__,
            SV("#define a b\n"
               "#define M(x) x x##y\n"
               "M(a)\n"),
            SV("\n"
               "\n"
               "b ay\n")
        },
        {
            "", __LINE__,
            SV("#define CAT(a,b) a##b\n"
               "#define FOO BAR\n"
               "CAT(FO,O)\n"),
            SV("\n"
               "\n"
               "BAR\n"),
        },
        {
            "", __LINE__,
            SV("#define FOO(a,b) a b\n"
               "FOO((x,y),z)\n"),
            SV("\n"
               "(x,y) z\n"),
        },
        {
            "", __LINE__,
            SV("#define CAT(a,b,c,d) a##b##c##d\n"
               "CAT(1, e, -, 2)\n"),
            SV("\n"
               "1e-2\n")
        },
        {
            "", __LINE__,
            SV("#define L (\n"
               "#define M() 1\n"
               "M L )\n"),
            SV("\n"
               "\n"
               "M( )\n")
        },
        {
            "", __LINE__,
            SV( "#define M() 1\n"
               "M\n"),
            SV("\n"
               "M")
        },
        {
            "", __LINE__,
            SV("#define M(...) __VA_OPT__(yes)no\n"
               "M()\n"
               "M(a)\n"
               "M( )\n"),
            SV("\n"
               "no\n"
               "yesno\n"
               "no\n"),
        },
        {
            "", __LINE__,
            SV("#define M(x, ...) __VA_OPT__(yes)no\n"
               "M(1)\n"
               "M(1, a)\n"
               "M(1, )\n"),
            SV("\n"
               "no\n"
               "yesno\n"
               "no\n"),
        },
    };
    for(size_t i = 0; i < sizeof test_cases / sizeof test_cases[0]; i++){
        int line = test_cases[i].line;
        StringView in = test_cases[i].in;
        StringView out = test_cases[i].out;
        err = cpp_expand_string(in, &result, __FILE__, __func__, line);
        TestExpectFalse(err);
        if(err){
            TestPrintValue("input   ", in);
            TestPrintValue("expected", out);
            TestPrintf("Error instead");
            continue;
        }
        if(!test_expect_equals_sv(out, result, "out", "result", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
        result = (StringView){0};
    }

    TESTEND();
}

TestFunction(test_builtin_macros){
    TESTBEGIN();
    struct {
        const char* name; StringView inp, exp; int line; _Bool disabled;
    } test_cases[] = {
        {"__FILE__", SV("__FILE__"), SV("\"(test)\""), __LINE__, 0},
        {"__LINE__", SV("__LINE__"), SV("1"), __LINE__, 0},
        {"__LI\\NE__", SV("__LI\\\nNE__"), SV("1"), __LINE__, 0},
        {"__COUNTER__", SV("__COUNTER__\n__COUNTER__\n__COUNTER__\n__COUNTER__ __COUNTER__"), SV("0\n1\n2\n3 4"), __LINE__, 0},
        {"counter paste", SV("#define C(x,y) x##y\n"
                             "#define E(x) x\n"
                             "#define C2(x,y) E(C(x, y))\n"
                             "C(__COUNTER__, __COUNTER__)\n"
                             "C2(__COUNTER__,__COUNTER__)\n"
                             "E(C(__COUNTER__, __COUNTER__))"), SV("\n\n\n__COUNTER____COUNTER__\n01\n__COUNTER____COUNTER__"), __LINE__, 0},
        {"multiple", SV("__FILE__\n__LINE__\n__LINE__\n__FILE__"), SV("\"(test)\"\n2\n3\n\"(test)\""), __LINE__, 0},
        {"indirect", SV("#define L1 L2\n#define L2 __LINE__\nL1"), SV("\n\n3"), __LINE__, 0},
        {"__FILE_NAME__", SV("__FILE_NAME__"), SV("\"(test)\""), __LINE__, 0},
        {"__INCLUDE_LEVEL__", SV("__INCLUDE_LEVEL__"), SV("1"), __LINE__, 0},
        {"__DATE__", SV("__DATE__"), SV("\"Jan 01 1900\""), __LINE__, 0},
        {"__TIME__", SV("__TIME__"), SV("\"01:02:03\""), __LINE__, 0},

        // extensions
        {"__EVAL__", SV("__EVAL__(1+1)"), SV("2"), __LINE__, 0},
        {"__EVAL__", SV("__EVAL__(defined __EVAL__)"), SV("1"), __LINE__, 0},
        {"__eval", SV("__eval(3*4/3)"), SV("4"), __LINE__, 0},
        {"__MIXIN__", SV("__MIXIN__(\"3\")"), SV("3"), __LINE__, 0},
        {"__MIXIN__", SV("__MIXIN__(__mixin(\"\\\"3\\\"\"))"), SV("3"), __LINE__, 0},
        {"__mixin", SV("#define S(x) #x\n__mixin(S(1)\"+1\")"), SV("\n1+1"), __LINE__, 0},
        {"__env", SV("__env(\"wolo\")"), SV("\"woo\""), __LINE__, 0},
        {"__ENV__", SV("__ENV__(\"foobar\")"), SV("\"\""), __LINE__, 0},
        {"__if", SV("__if(1, 2, 3)"), SV("2"), __LINE__, 0},
        {"__if", SV("__if(1, , __eval(1/0))"), SV(""), __LINE__, 0},
        {"__IF__", SV("#define X 1\n__IF__(X, 3, __eval(1/0))"), SV("\n3"), __LINE__, 0},
        {"__ident", SV("__ident(\"this is an ident\")"), SV("this is an ident"), __LINE__, 0}, // can't really tell from the stringification, but this is a single token
        {"__FORMAT__", SV("__format(\"%d = %s\", 10, \"hello world\")"), SV("\"10 = hello world\""), __LINE__, 0},

    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        if(test_cases[i].disabled) continue;
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "result", "exp", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        if(result.length) Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

TestFunction(test_error_locations){
    TESTBEGIN();
    struct {
        const char* name; int line; StringView inp, exp;
    } test_cases[] = {
        {"indirect too many args", __LINE__,
            SV("#define F(x) x\n"
               "#define M F(1,2)\n"
               "M"),
            SV("(test):2:14: error: Too many arguments to function-like macro F()\n"
               "(test):3:1: ... expanded from here\n")},
        {"bad token paste", __LINE__,
            SV("#define P(a,b) a##b\n"
               "P(+,-)"),
            SV("(test):1:17: error: pasting \"+\" and \"-\" does not give a valid preprocessing token\n"
               "(test):2:1: ... expanded from here\n")},
        {"indirect too few args", __LINE__,
            SV("#define F(x,y) x y\n"
               "#define M F(1)\n"
               "M"),
            SV("(test):2:13: error: Too few arguments to function-like macro F()\n"
               "(test):3:1: ... expanded from here\n")},
        {"multi-level bad paste", __LINE__,
            SV("#define P(a,b) a##b\n"
               "#define Q(a,b) P(a,b)\n"
               "#define R(a,b) Q(a,b)\n"
               "R(+,-)"),
            SV("(test):1:17: error: pasting \"+\" and \"-\" does not give a valid preprocessing token\n"
               "(test):2:16: ... expanded from here\n"
               "(test):3:16: ... expanded from here\n"
               "(test):4:1: ... expanded from here\n")},
        {"error from pasted macro name", __LINE__,
            SV("#define FF(x) x\n"
               "#define C(a,b) a##b\n"
               "C(F,F)(1,2)"),
            SV("(test):3:9: error: Too many arguments to function-like macro FF()\n")},
        {"error from pasted macro name chained", __LINE__,
            SV("#define FF(x) x\n"
               "#define C(a,b) a##b\n"
               "#define M C(F,F)(1,2)\n"
               "M"),
            SV("(test):3:19: error: Too many arguments to function-like macro FF()\n"
               "(test):4:1: ... expanded from here\n")},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        int line = test_cases[i].line;
        StringView err_msg;
        int err = cpp_expand_string_expect_error(test_cases[i].inp, &err_msg);
        TestExpectFalse(err);
        if(err) continue;
        if(!test_expect_equals_sv(test_cases[i].exp, err_msg, "exp", "err_msg", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        if(err_msg.text) Allocator_free(MALLOCATOR, err_msg.text, err_msg.length);
    }
    TESTEND();
}
TestFunction(test_condition){
    TESTBEGIN();
    struct {
        const char* name; int line; _Bool disabled; StringView inp, exp;
    } test_cases[] = {
        {"#if", __LINE__, 0,
            SV("#if 1\n"
                    "Foo\n"
                    "#endif"),
            SV("\nFoo\n")
        },
        {"ifdef (defined)", __LINE__, 0,
            SV("#define FOO\n"
               "#ifdef FOO\n"
               "Foo\n"
               "#endif"),
            SV("\n\nFoo\n")
        },
        {"ifdef (undefined)", __LINE__, 0,
            SV("#define FOO\n"
               "#ifdef foo\n"
               "Foo\n"
               "#endif"),
            SV("\n\n\n")
        },
        {"ifndef (undefined)", __LINE__, 0,
            SV("#define FOO\n"
               "#ifndef foo\n"
               "Foo\n"
               "#endif"),
            SV("\n\nFoo\n")
        },
        {"ifndef (defined)", __LINE__, 0,
            SV("#define FOO\n"
               "#ifndef FOO\n"
               "Foo\n"
               "#endif // ifndef FOO"),
            SV("\n\n\n")
        },
        // #ifdef / #else
        {"ifdef else (true branch)", __LINE__, 0,
            SV("#define X\n"
               "#ifdef X\n"
               "yes\n"
               "#else\n"
               "no\n"
               "#endif"),
            SV("\n\nyes\n\n\n")
        },
        {"ifdef else (false branch)", __LINE__, 0,
            SV("#ifdef X\n"
               "yes\n"
               "#else\n"
               "no\n"
               "#endif"),
            SV("\n\n\nno\n")
        },
        // #ifndef / #else
        {"ifndef else (true branch)", __LINE__, 0,
            SV("#ifndef X\n"
               "yes\n"
               "#else\n"
               "no\n"
               "#endif"),
            SV("\nyes\n\n\n")
        },
        {"ifndef else (false branch)", __LINE__, 0,
            SV("#define X\n"
               "#ifndef X\n"
               "yes\n"
               "#else\n"
               "no\n"
               "#endif"),
            SV("\n\n\n\nno\n")
        },
        // Nested conditionals
        {"nested ifdef both true", __LINE__, 0,
            SV("#define A\n"
               "#define B\n"
               "#ifdef A\n"
               "#ifdef B\n"
               "both\n"
               "#endif\n"
               "#endif"),
            SV("\n\n\n\nboth\n\n")
        },
        {"nested ifdef outer false", __LINE__, 0,
            SV("#define B\n"
               "#ifdef A\n"
               "#ifdef B\n"
               "inner\n"
               "#endif\n"
               "#endif"),
            SV("\n\n\n\n\n")
        },
        {"nested ifdef inner false", __LINE__, 0,
            SV("#define A\n"
               "#ifdef A\n"
               "#ifdef B\n"
               "inner\n"
               "#else\n"
               "else_inner\n"
               "#endif\n"
               "after_inner\n"
               "#endif"),
            SV("\n\n\n\n\nelse_inner\n\nafter_inner\n")
        },
        // No macro expansion in false branches
        {"no expand in false", __LINE__, 0,
            SV("#define X error_if_expanded\n"
               "#ifdef NOPE\n"
               "X\n"
               "#endif"),
            SV("\n\n\n")
        },
        // #define inside conditional branches
        {"define in true branch", __LINE__, 0,
            SV("#ifdef NOPE\n"
               "#define X bad\n"
               "#else\n"
               "#define X good\n"
               "#endif\n"
               "X"),
            SV("\n\n\n\n\ngood")
        },
        {"define in false branch ignored", __LINE__, 0,
            SV("#define A\n"
               "#ifdef A\n"
               "#define X yes\n"
               "#else\n"
               "#define X no\n"
               "#endif\n"
               "X"),
            SV("\n\n\n\n\n\nyes")
        },
        // #undef inside conditional
        {"undef in true branch", __LINE__, 0,
            SV("#define X hello\n"
               "#ifdef X\n"
               "#undef X\n"
               "#endif\n"
               "#ifdef X\n"
               "still_defined\n"
               "#else\n"
               "undefined\n"
               "#endif"),
            SV("\n\n\n\n\n\n\nundefined\n")
        },
        // #elifdef
        {"elifdef first true", __LINE__, 0,
            SV("#define A A\n"
               "#define B B\n"
               "#ifdef A\n"
               "A\n"
               "#elifdef B\n"
               "B\n"
               "#endif"),
            SV("\n\n\nA\n\n\n")
        },
        {"elifdef second true", __LINE__, 0,
            SV("#define B B\n"
               "#ifdef A\n"
               "A\n"
               "#elifdef B\n"
               "B\n"
               "#endif"),
            SV("\n\n\n\nB\n")
        },
        {"elifdef neither", __LINE__, 0,
            SV("#ifdef A\n"
               "A\n"
               "#elifdef B\n"
               "B\n"
               "#else\n"
               "neither\n"
               "#endif"),
            SV("\n\n\n\n\nneither\n")
        },
        // #elifndef
        {"elifndef activates", __LINE__, 0,
            SV("#define A A\n"
               "#ifdef A\n"
               "#ifdef NOPE\n"
               "nope\n"
               "#elifndef ALSO_NOPE\n"
               "yes\n"
               "#endif\n"
               "#endif"),
            SV("\n\n\n\n\nyes\n\n")
        },
        {"elifndef skipped when true_taken", __LINE__, 0,
            SV("#define A A\n"
               "#ifdef A\n"
               "A\n"
               "#elifndef WHATEVER\n"
               "no\n"
               "#endif"),
            SV("\n\nA\n\n\n")
        },
        // Deeply nested in false branch
        {"deep nesting in false branch", __LINE__, 0,
            SV("#ifdef NOPE\n"
               "#ifdef ALSO_NOPE\n"
               "#ifdef DEEP\n"
               "deep\n"
               "#else\n"
               "deep_else\n"
               "#endif\n"
               "#endif\n"
               "#else\n"
               "outer_else\n"
               "#endif"),
            SV("\n\n\n\n\n\n\n\n\nouter_else\n")
        },
        // Empty branches
        {"empty true branch", __LINE__, 0,
            SV("#define X X\n"
               "#ifdef X\n"
               "#endif"),
            SV("\n\n")
        },
        {"empty false branch", __LINE__, 0,
            SV("#ifdef X\n"
               "#else\n"
               "#endif"),
            SV("\n\n")
        },
        // Multiple elif chain
        {"elifdef chain", __LINE__, 0,
            SV("#define C C\n"
               "#ifdef A\n"
               "A\n"
               "#elifdef B\n"
               "B\n"
               "#elifdef C\n"
               "C\n"
               "#elifdef D\n"
               "D\n"
               "#else\n"
               "none\n"
               "#endif"),
            SV("\n\n\n\n\n\nC\n\n\n\n\n")
        },
        // Include guard pattern
        {"include guard pattern", __LINE__, 0,
            SV("#ifndef GUARD_H\n"
               "#define GUARD_H\n"
               "content\n"
               "#endif"),
            SV("\n\ncontent\n")
        },
        // Only first matching elifdef activates
        {"elifdef only first match", __LINE__, 0,
            SV("#define B B\n"
               "#define C C\n"
               "#ifdef A\n"
               "A\n"
               "#elifdef B\n"
               "B\n"
               "#elifdef C\n"
               "C\n"
               "#endif"),
            SV("\n\n\n\n\nB\n\n\n")
        },
        // elifndef direct (not nested)
        {"elifndef direct", __LINE__, 0,
            SV("#ifdef NOPE\n"
               "nope\n"
               "#elifndef ALSO_NOPE\n"
               "yes\n"
               "#endif"),
            SV("\n\n\nyes\n")
        },
        // Macro expansion resumes after #endif
        {"expansion after endif", __LINE__, 0,
            SV("#define X hello\n"
               "#ifdef NOPE\n"
               "skipped\n"
               "#endif\n"
               "X"),
            SV("\n\n\n\nhello")
        },
        // Null directive (# on a line by itself) in false branch
        {"null directive in false branch", __LINE__, 0,
            SV("#ifdef NOPE\n"
               "#\n"
               "#endif"),
            SV("\n\n")
        },
        // Adjacent independent conditionals
        {"adjacent conditionals", __LINE__, 0,
            SV("#define A\n"
               "#ifdef A\n"
               "first\n"
               "#endif\n"
               "#ifdef B\n"
               "second\n"
               "#else\n"
               "not_second\n"
               "#endif"),
            SV("\n\nfirst\n\n\n\n\nnot_second\n")
        },
        {
            "Weird c23 pseudo macros", __LINE__, 0,
            SV( "#ifdef __has_include\n"
                "__has_include\n"
                "#endif\n"
                "#ifdef __has_embed\n"
                "__has_embed\n"
                "#endif\n"
                "#ifdef __has_c_attribute\n"
                "__has_c_attribute\n"
                "#endif\n"),
            SV("\n__has_include\n\n\n__has_embed\n\n\n__has_c_attribute\n\n")
        },

    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        if(test_cases[i].disabled) continue;
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "result", "exp", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}
TestFunction(test_erroneous_condition){
    TESTBEGIN();
    struct {
        const char* name; int line; StringView inp, exp;
    } test_cases[] = {
        {"endif without if", __LINE__,
            SV("#endif"), SV("(test):1:2: error: #endif outside of #if (or similar construct)\n")},
        {"else without if", __LINE__,
            SV("#else"), SV("(test):1:2: error: #else outside of #if (or similar construct)\n")},
        {"elif without if", __LINE__,
            SV("#elif 1"), SV("(test):1:2: error: #elif outside of #if (or similar construct)\n")},
        {"elifdef without if", __LINE__,
            SV("#elifdef X"), SV("(test):1:2: error: #elifdef outside of #if (or similar construct)\n")},
        {"elifndef without if", __LINE__,
            SV("#elifndef X"), SV("(test):1:2: error: #elifndef outside of #if (or similar construct)\n")},
        {"duplicate else", __LINE__,
            SV("#ifdef X\n"
               "#else\n"
               "#else\n"
               "#endif"),
            SV("(test):3:2: error: another #else\n")},
        {"elif after else", __LINE__,
            SV("#ifdef X\n"
               "#else\n"
               "#elif 1\n"
               "#endif"),
            SV("(test):3:2: error: #elif after #else\n")},
        {"elifdef after else", __LINE__,
            SV("#define Y\n"
               "#ifdef X\n"
               "#else\n"
               "#elifdef Y\n"
               "#endif"),
            SV("(test):4:2: error: #elifdef after #else\n")},
        {"elifndef after else", __LINE__,
            SV("#define Y\n"
               "#ifdef X\n"
               "#else\n"
               "#elifndef Y\n"
               "#endif"),
            SV("(test):4:2: error: #elifndef after #else\n")},
        {"duplicate else (active branch)", __LINE__,
            SV("#define X\n"
               "#ifdef X\n"
               "#else\n"
               "#else\n"
               "#endif"),
            SV("(test):4:2: error: another #else\n")},
        {"elif after else (active branch)", __LINE__,
            SV("#define X\n"
               "#ifndef X\n"
               "skip\n"
               "#else\n"
               "#elif 1\n"
               "#endif"),
            SV("(test):5:2: error: #elif after #else\n")},
        {"unterminated if", __LINE__,
            SV("#ifdef X\n"
               "stuff"),
            SV("(test):1:2: error: Unterminated conditional directive\n")},
        {"unterminated nested if", __LINE__,
            SV("#define A\n"
               "#ifdef A\n"
               "#ifdef B\n"
               "stuff"),
            SV("(test):3:2: error: Unterminated conditional directive\n")},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        int line = test_cases[i].line;
        StringView err_msg;
        int err = cpp_expand_string_expect_error(test_cases[i].inp, &err_msg);
        TestExpectFalse(err);
        if(err) continue;
        if(!test_expect_equals_sv(test_cases[i].exp, err_msg, "exp", "err_msg", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        if(err_msg.text) Allocator_free(MALLOCATOR, err_msg.text, err_msg.length);
    }
    TESTEND();
}

TestFunction(test_if_eval){
    TESTBEGIN();
    struct {
        const char* name; int line; _Bool disabled; StringView inp, exp;
    } test_cases[] = {
        // Basic constants
        {"if 0", __LINE__, 0,
            SV("#if 0\nyes\n#endif"), SV("\n\n")},
        {"if 1", __LINE__, 0,
            SV("#if 1\nyes\n#endif"), SV("\nyes\n")},
        {"if 42", __LINE__, 0,
            SV("#if 42\nyes\n#endif"), SV("\nyes\n")},
        // Hex
        {"hex 0x10", __LINE__, 0,
            SV("#if 0x10 == 16\nyes\n#endif"), SV("\nyes\n")},
        {"hex 0xFF", __LINE__, 0,
            SV("#if 0xFF == 255\nyes\n#endif"), SV("\nyes\n")},
        {"hex 0XA", __LINE__, 0,
            SV("#if 0XA == 10\nyes\n#endif"), SV("\nyes\n")},
        // Octal
        {"octal 077", __LINE__, 0,
            SV("#if 077 == 63\nyes\n#endif"), SV("\nyes\n")},
        {"octal 010", __LINE__, 0,
            SV("#if 010 == 8\nyes\n#endif"), SV("\nyes\n")},
        // Binary
        {"binary 0b101", __LINE__, 0,
            SV("#if 0b101 == 5\nyes\n#endif"), SV("\nyes\n")},
        {"binary 0B1111", __LINE__, 0,
            SV("#if 0B1111 == 15\nyes\n#endif"), SV("\nyes\n")},
        // Integer suffixes
        {"suffix U", __LINE__, 0,
            SV("#if 42U == 42\nyes\n#endif"), SV("\nyes\n")},
        {"suffix L", __LINE__, 0,
            SV("#if 42L == 42\nyes\n#endif"), SV("\nyes\n")},
        {"suffix LL", __LINE__, 0,
            SV("#if 42LL == 42\nyes\n#endif"), SV("\nyes\n")},
        {"suffix ULL", __LINE__, 0,
            SV("#if 42ULL == 42\nyes\n#endif"), SV("\nyes\n")},
        {"suffix uLL", __LINE__, 0,
            SV("#if 42uLL == 42\nyes\n#endif"), SV("\nyes\n")},
        {"suffix LLU", __LINE__, 0,
            SV("#if 100LLU == 100\nyes\n#endif"), SV("\nyes\n")},
        // Character constants
        {"char 'A'", __LINE__, 0,
            SV("#if 'A' == 65\nyes\n#endif"), SV("\nyes\n")},
        {"char '0'", __LINE__, 0,
            SV("#if '0' == 48\nyes\n#endif"), SV("\nyes\n")},
        {"char '\\n'", __LINE__, 0,
            SV("#if '\\n' == 10\nyes\n#endif"), SV("\nyes\n")},
        {"char '\\t'", __LINE__, 0,
            SV("#if '\\t' == 9\nyes\n#endif"), SV("\nyes\n")},
        {"char '\\0'", __LINE__, 0,
            SV("#if '\\0' == 0\nyes\n#endif"), SV("\nyes\n")},
        {"char '\\x41'", __LINE__, 0,
            SV("#if '\\x41' == 65\nyes\n#endif"), SV("\nyes\n")},
        {"char '\\\\' backslash", __LINE__, 0,
            SV("#if '\\\\' == 92\nyes\n#endif"), SV("\nyes\n")},
        {"char '\\'' escaped quote", __LINE__, 0,
            SV("#if '\\'' == 39\nyes\n#endif"), SV("\nyes\n")},
        // Unary operators
        {"unary !", __LINE__, 0,
            SV("#if !0\nyes\n#endif"), SV("\nyes\n")},
        {"unary ! truthy", __LINE__, 0,
            SV("#if !1\nyes\n#else\nno\n#endif"), SV("\n\n\nno\n")},
        {"unary ! nonzero", __LINE__, 0,
            SV("#if !42\nyes\n#else\nno\n#endif"), SV("\n\n\nno\n")},
        {"unary ~", __LINE__, 0,
            SV("#if (~0 & 0xFF) == 0xFF\nyes\n#endif"), SV("\nyes\n")},
        {"unary +", __LINE__, 0,
            SV("#if +1 == 1\nyes\n#endif"), SV("\nyes\n")},
        {"unary -", __LINE__, 0,
            SV("#if -1 < 0\nyes\n#endif"), SV("\nyes\n")},
        {"unary - value", __LINE__, 0,
            SV("#if -5 + 5 == 0\nyes\n#endif"), SV("\nyes\n")},
        // Arithmetic
        {"add", __LINE__, 0,
            SV("#if 1 + 2 == 3\nyes\n#endif"), SV("\nyes\n")},
        {"sub", __LINE__, 0,
            SV("#if 10 - 3 == 7\nyes\n#endif"), SV("\nyes\n")},
        {"mul", __LINE__, 0,
            SV("#if 6 * 7 == 42\nyes\n#endif"), SV("\nyes\n")},
        {"div", __LINE__, 0,
            SV("#if 42 / 6 == 7\nyes\n#endif"), SV("\nyes\n")},
        {"mod", __LINE__, 0,
            SV("#if 17 % 5 == 2\nyes\n#endif"), SV("\nyes\n")},
        // Shifts
        {"left shift", __LINE__, 0,
            SV("#if 1 << 4 == 16\nyes\n#endif"), SV("\nyes\n")},
        {"right shift", __LINE__, 0,
            SV("#if 256 >> 4 == 16\nyes\n#endif"), SV("\nyes\n")},
        // Relational
        {"less than true", __LINE__, 0,
            SV("#if 1 < 2\nyes\n#endif"), SV("\nyes\n")},
        {"less than false", __LINE__, 0,
            SV("#if 2 < 1\nyes\n#else\nno\n#endif"), SV("\n\n\nno\n")},
        {"greater than", __LINE__, 0,
            SV("#if 3 > 2\nyes\n#endif"), SV("\nyes\n")},
        {"less equal", __LINE__, 0,
            SV("#if 2 <= 2\nyes\n#endif"), SV("\nyes\n")},
        {"greater equal", __LINE__, 0,
            SV("#if 3 >= 3\nyes\n#endif"), SV("\nyes\n")},
        // Equality
        {"equal true", __LINE__, 0,
            SV("#if 5 == 5\nyes\n#endif"), SV("\nyes\n")},
        {"equal false", __LINE__, 0,
            SV("#if 5 == 6\nyes\n#else\nno\n#endif"), SV("\n\n\nno\n")},
        {"not equal true", __LINE__, 0,
            SV("#if 5 != 6\nyes\n#endif"), SV("\nyes\n")},
        {"not equal false", __LINE__, 0,
            SV("#if 5 != 5\nyes\n#else\nno\n#endif"), SV("\n\n\nno\n")},
        // Bitwise
        {"bitwise and", __LINE__, 0,
            SV("#if (0xF0 & 0x0F) == 0\nyes\n#endif"), SV("\nyes\n")},
        {"bitwise and 2", __LINE__, 0,
            SV("#if (0xFF & 0x0F) == 0x0F\nyes\n#endif"), SV("\nyes\n")},
        {"bitwise or", __LINE__, 0,
            SV("#if (0xF0 | 0x0F) == 0xFF\nyes\n#endif"), SV("\nyes\n")},
        {"bitwise xor", __LINE__, 0,
            SV("#if (0xFF ^ 0x0F) == 0xF0\nyes\n#endif"), SV("\nyes\n")},
        // Logical
        {"logical and tt", __LINE__, 0,
            SV("#if 1 && 1\nyes\n#endif"), SV("\nyes\n")},
        {"logical and tf", __LINE__, 0,
            SV("#if 1 && 0\nyes\n#else\nno\n#endif"), SV("\n\n\nno\n")},
        {"logical and ft", __LINE__, 0,
            SV("#if 0 && 1\nyes\n#else\nno\n#endif"), SV("\n\n\nno\n")},
        {"logical or ff", __LINE__, 0,
            SV("#if 0 || 0\nyes\n#else\nno\n#endif"), SV("\n\n\nno\n")},
        {"logical or tf", __LINE__, 0,
            SV("#if 1 || 0\nyes\n#endif"), SV("\nyes\n")},
        {"logical or ft", __LINE__, 0,
            SV("#if 0 || 1\nyes\n#endif"), SV("\nyes\n")},
        // Ternary
        {"ternary true", __LINE__, 0,
            SV("#if (1 ? 2 : 3) == 2\nyes\n#endif"), SV("\nyes\n")},
        {"ternary false", __LINE__, 0,
            SV("#if (0 ? 2 : 3) == 3\nyes\n#endif"), SV("\nyes\n")},
        {"ternary nested", __LINE__, 0,
            SV("#if (1 ? 1 ? 10 : 20 : 30) == 10\nyes\n#endif"), SV("\nyes\n")},
        {"ternary nested false", __LINE__, 0,
            SV("#if (0 ? 10 : 1 ? 20 : 30) == 20\nyes\n#endif"), SV("\nyes\n")},
        // Parentheses
        {"parens override prec", __LINE__, 0,
            SV("#if (1 + 2) * 3 == 9\nyes\n#endif"), SV("\nyes\n")},
        {"nested parens", __LINE__, 0,
            SV("#if ((2 + 3) * (4 - 1)) == 15\nyes\n#endif"), SV("\nyes\n")},
        // defined() combined with expressions
        {"defined && defined", __LINE__, 0,
            SV("#define FOO\n#define BAR\n#if defined(FOO) && defined(BAR)\nyes\n#endif"),
            SV("\n\n\nyes\n")},
        {"defined && !defined", __LINE__, 0,
            SV("#define FOO\n#if defined(FOO) && !defined(BAR)\nyes\n#endif"),
            SV("\n\nyes\n")},
        {"defined || defined", __LINE__, 0,
            SV("#define FOO\n#if defined(FOO) || defined(BAR)\nyes\n#endif"),
            SV("\n\nyes\n")},
        // Macro expansion in #if
        {"macro value in if", __LINE__, 0,
            SV("#define X 42\n#if X == 42\nyes\n#endif"),
            SV("\n\nyes\n")},
        {"macro expr in if", __LINE__, 0,
            SV("#define A 10\n#define B 20\n#if A + B == 30\nyes\n#endif"),
            SV("\n\n\nyes\n")},
        // #elif with expressions
        {"elif expression", __LINE__, 0,
            SV("#if 0\nno\n#elif 2 + 2 == 4\nyes\n#endif"),
            SV("\n\n\nyes\n")},
        {"elif chain", __LINE__, 0,
            SV("#if 0\na\n#elif 0\nb\n#elif 1\nc\n#elif 1\nd\n#endif"),
            SV("\n\n\n\n\nc\n\n\n")},
        // Undefined macro evaluates to 0
        {"undefined macro is 0", __LINE__, 0,
            SV("#if UNDEFINED\nyes\n#else\nno\n#endif"),
            SV("\n\n\nno\n")},
        {"undefined macro == 0", __LINE__, 0,
            SV("#if UNDEFINED == 0\nyes\n#endif"),
            SV("\nyes\n")},
        // true keyword
        {"true keyword", __LINE__, 0,
            SV("#if true\nyes\n#endif"),
            SV("\nyes\n")},

        // ---- Precedence stress tests ----

        // * binds tighter than +
        {"prec: + vs *", __LINE__, 0,
            SV("#if 1 + 2 * 3 == 7\nyes\n#endif"), SV("\nyes\n")},
        {"prec: * vs + reversed", __LINE__, 0,
            SV("#if 2 * 3 + 1 == 7\nyes\n#endif"), SV("\nyes\n")},
        // - and * precedence
        {"prec: - vs *", __LINE__, 0,
            SV("#if 10 - 2 * 3 == 4\nyes\n#endif"), SV("\nyes\n")},
        // / binds tighter than -
        {"prec: - vs /", __LINE__, 0,
            SV("#if 10 - 6 / 2 == 7\nyes\n#endif"), SV("\nyes\n")},
        // % same precedence as *
        {"prec: % vs +", __LINE__, 0,
            SV("#if 10 + 7 % 3 == 11\nyes\n#endif"), SV("\nyes\n")},
        // << binds tighter than ==, looser than +
        {"prec: << vs +", __LINE__, 0,
            SV("#if 1 << 2 + 1 == 8\nyes\n#endif"), SV("\nyes\n")},
        {"prec: << vs ==", __LINE__, 0,
            SV("#if 1 << 4 == 16\nyes\n#endif"), SV("\nyes\n")},
        // >> similar
        {"prec: >> vs +", __LINE__, 0,
            SV("#if 32 >> 2 + 1 == 4\nyes\n#endif"), SV("\nyes\n")},
        // < binds tighter than ==
        {"prec: < vs ==", __LINE__, 0,
            SV("#if (1 < 2) == 1\nyes\n#endif"), SV("\nyes\n")},
        // == binds tighter than &
        {"prec: == vs &", __LINE__, 0,
            SV("#if 3 & 1 == 1\nyes\n#endif"), SV("\nyes\n")},
        {"prec: == vs & explicit", __LINE__, 0,
            SV("#if (3 & 1) == 1\nyes\n#endif"), SV("\nyes\n")},
        // & binds tighter than ^
        {"prec: & vs ^", __LINE__, 0,
            SV("#if (0xF & 0x3 ^ 0x1) == 2\nyes\n#endif"), SV("\nyes\n")},
        // ^ binds tighter than |
        {"prec: ^ vs |", __LINE__, 0,
            SV("#if (1 | 2 ^ 3) == 1 | (2 ^ 3)\nyes\n#endif"), SV("\nyes\n")},
        // | binds tighter than &&
        {"prec: | vs &&", __LINE__, 0,
            SV("#if 1 | 0 && 0 | 1\nyes\n#endif"), SV("\nyes\n")},
        // && binds tighter than ||
        {"prec: && vs ||", __LINE__, 0,
            SV("#if 1 || 0 && 0\nyes\n#endif"), SV("\nyes\n")},
        {"prec: || && classic", __LINE__, 0,
            SV("#if 0 || 1 && 1\nyes\n#endif"), SV("\nyes\n")},
        {"prec: && || false path", __LINE__, 0,
            SV("#if 0 && 0 || 0\nyes\n#else\nno\n#endif"), SV("\n\n\nno\n")},
        // || binds tighter than ?:
        {"prec: || vs ?:", __LINE__, 0,
            SV("#if (1 || 0 ? 10 : 20) == 10\nyes\n#endif"), SV("\nyes\n")},
        // Ternary is right-associative
        {"ternary right-assoc", __LINE__, 0,
            SV("#if (1 ? 2 : 3 ? 4 : 5) == 2\nyes\n#endif"), SV("\nyes\n")},
        {"ternary right-assoc 2", __LINE__, 0,
            SV("#if (0 ? 2 : 1 ? 4 : 5) == 4\nyes\n#endif"), SV("\nyes\n")},
        {"ternary right-assoc 3", __LINE__, 0,
            SV("#if (0 ? 2 : 0 ? 4 : 5) == 5\nyes\n#endif"), SV("\nyes\n")},
        // Left-associativity of arithmetic
        {"left-assoc sub", __LINE__, 0,
            SV("#if 10 - 3 - 2 == 5\nyes\n#endif"), SV("\nyes\n")},
        {"left-assoc div", __LINE__, 0,
            SV("#if 100 / 10 / 2 == 5\nyes\n#endif"), SV("\nyes\n")},
        {"left-assoc mod", __LINE__, 0,
            SV("#if 17 % 10 % 4 == 3\nyes\n#endif"), SV("\nyes\n")},
        {"left-assoc shift", __LINE__, 0,
            SV("#if 1 << 2 << 3 == 32\nyes\n#endif"), SV("\nyes\n")},
        // Complex multi-operator expressions
        {"complex expr 1", __LINE__, 0,
            SV("#if 2 + 3 * 4 - 6 / 2 == 11\nyes\n#endif"), SV("\nyes\n")},
        {"complex expr 2", __LINE__, 0,
            SV("#if (1 << 8) - 1 == 255\nyes\n#endif"), SV("\nyes\n")},
        {"complex expr 3", __LINE__, 0,
            SV("#if 0xFF & 0x0F | 0xF0 == 0xFF\nyes\n#endif"), SV("\nyes\n")},
        {"complex expr 4", __LINE__, 0,
            SV("#if 1 + 2 > 2 && 3 * 4 == 12\nyes\n#endif"), SV("\nyes\n")},
        // Unary in complex expressions
        {"unary in expr", __LINE__, 0,
            SV("#if -1 + 2 == 1\nyes\n#endif"), SV("\nyes\n")},
        {"double negation", __LINE__, 0,
            SV("#if !!42 == 1\nyes\n#endif"), SV("\nyes\n")},
        {"not in logical", __LINE__, 0,
            SV("#if !0 && !0\nyes\n#endif"), SV("\nyes\n")},
        {"complement and mask", __LINE__, 0,
            SV("#if (~0xF0 & 0xFF) == 0x0F\nyes\n#endif"), SV("\nyes\n")},
        // All comparison operators
        {"cmp chain", __LINE__, 0,
            SV("#if 1 < 2 && 2 > 1 && 3 <= 3 && 3 >= 3 && 4 == 4 && 4 != 5\nyes\n#endif"),
            SV("\nyes\n")},
        // Large ternary chain
        {"ternary chain", __LINE__, 0,
            SV("#if (0 ? 1 : 0 ? 2 : 0 ? 3 : 4) == 4\nyes\n#endif"),
            SV("\nyes\n")},
        // Shift precedence vs comparison
        {"shift vs compare", __LINE__, 0,
            SV("#if 1 << 4 > 10\nyes\n#endif"), SV("\nyes\n")},
        // Nested defined with complex logic
        {"complex defined", __LINE__, 0,
            SV("#define A\n#define B\n#define C\n"
               "#if defined(A) && (defined(B) || defined(D)) && !defined(E)\nyes\n#endif"),
            SV("\n\n\n\nyes\n")},
        // Bitwise ops full chain
        {"bitwise chain", __LINE__, 0,
            SV("#if ((0x0F & 0xFF) ^ 0x05 | 0x10) == 0x1A\nyes\n#endif"),
            SV("\nyes\n")},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        if(test_cases[i].disabled) continue;
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "exp", "result", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(test_obj_macros);
    RegisterTest(test_func_macros);
    RegisterTest(test_func_macros_extensions);
    RegisterTest(test_for_each);
    RegisterTest(test_for_each_empty);
    RegisterTest(test_c23);
    RegisterTest(test_torture);
    RegisterTest(test_builtin_macros);
    RegisterTest(test_error_locations);
    RegisterTest(test_condition);
    RegisterTest(test_erroneous_condition);
    RegisterTest(test_if_eval);
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
