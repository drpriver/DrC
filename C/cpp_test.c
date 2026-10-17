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
#include "cpp_preprocessor.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static int cpp_next_pp_token(CppPreprocessor* cpp, CppToken* ptok);
static void cpp_write_token(MStringBuilder*, CppToken);
enum {SKIP = 1};

// Helper to run preprocessor on a string and collect output tokens as a string
static
int
cpp_expand_string(StringView txt, StringView* out, const char* file, const char* func, int line){
    int result = 0;
    ArenaAllocator aa = {0};
    Allocator a = allocator_from_arena(&aa);
    FileCache *fc = fc_create(a, FC_FLAGS_NONE);
    MStringBuilder log_sb = {.allocator=a};
    MsbLogger logger_ = {0};
    Logger* logger = msb_logger(&logger_, &log_sb);
    AtomTable at = {0};
    Environment env = {.allocator = a, .at=&at};
    int err;
    err = env_setenv4(&env, "wolo", 4, "woo", 3);
    if(err) goto finally;
    CppPreprocessor cpp = {
        .allocator = a,
        .fc = fc,
        .at = &at,
        .logger = logger,
        .env = &env,
        .target = &cc_target_test,
        .rng = {0x5f05085516fda9a8, 0xef1f011b1f63e07},
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
    CppFrame frame = {
        .file_id = (uint32_t)fc->map.count - 1,
        .txt = txt,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CppFrame)(&cpp.frames, cpp.allocator, frame);
    if(err){
        result = 1;
        goto finally;
    }
    MStringBuilder sb = {.allocator = MALLOCATOR};
    CppToken tok;
    for(;;){
        err = cpp_next_pp_token(&cpp, &tok);
        if(err){
            msb_destroy(&sb);
            result = 1;
            goto finally;
        }
        if(tok.type == CPP_END_OF_INPUT) break;
        if(tok.type == CPP_WHITESPACE)
            msb_write_char(&sb, ' ');
        else if(tok.type == CPP_NEWLINE)
            msb_write_char(&sb, '\n');
        else
            cpp_write_token(&sb, tok);
    }
    StringView s = msb_detach_sv(&sb);
    *out = s;

    finally:
    if(log_sb.cursor){
        StringView sv = msb_borrow_sv(&log_sb);
        TestPrintf("%s%s:%d:%s%s\n    %.*s", _test_color_gray, file, line, func, _test_color_reset, sv_p(sv));
    }
    ArenaAllocator_free_all(&at.arena);
    ArenaAllocator_free_all(&aa);
    ArenaAllocator_free_all(&cpp.synth_arena);
    return result;
}
typedef struct { StringView path; StringView content; } IncludeTestFile;

static
int
cpp_expand_with_files(
    const IncludeTestFile* files, size_t nfiles,
    const StringView* _Nullable Ipaths, size_t nIpaths,
    const StringView* _Nullable Fpaths, size_t nFpaths,
    StringView* out, const char* file, const char* func, int line
){
    int result = 0;
    ArenaAllocator aa = {0};
    Allocator a = allocator_from_arena(&aa);
    FileCache *fc = fc_create(a, FC_FLAGS_NONE);
    MStringBuilder log_sb = {.allocator=a};
    MsbLogger logger_ = {0};
    Logger* logger = msb_logger(&logger_, &log_sb);
    AtomTable at = {0};
    Environment env = {.allocator = a, .at=&at};
    int err;
    CppPreprocessor cpp = {
        .allocator = a,
        .fc = fc,
        .at = &at,
        .logger = logger,
        .env = &env,
        .target = &cc_target_test,
    };
    for(size_t i = 0; i < nIpaths; i++){
        err = ma_push(StringView)(&cpp.Ipaths, a, Ipaths[i]);
        if(err){ result = 1; goto finally; }
    }
    for(size_t i = 0; i < nFpaths; i++){
        err = ma_push(StringView)(&cpp.framework_paths, a, Fpaths[i]);
        if(err){ result = 1; goto finally; }
    }
    for(size_t i = 0; i < nfiles; i++){
        fc_write_path(fc, files[i].path.text, files[i].path.length);
        err = fc_cache_file(fc, files[i].content);
        if(err){ result = 1; goto finally; }
    }
    err = cpp_define_builtin_macros(&cpp);
    if(err){ result = 1; goto finally; }
    {
        CppFrame frame = {
            .file_id = 0,
            .txt = files[0].content,
            .line = 1,
            .column = 1,
        };
        err = ma_push(CppFrame)(&cpp.frames, cpp.allocator, frame);
        if(err){ result = 1; goto finally; }
    }
    {
        MStringBuilder sb = {.allocator = MALLOCATOR};
        CppToken tok;
        for(;;){
            err = cpp_next_pp_token(&cpp, &tok);
            if(err){
                msb_destroy(&sb);
                result = 1;
                goto finally;
            }
            if(tok.type == CPP_END_OF_INPUT) break;
            if(tok.type == CPP_WHITESPACE)
                msb_write_char(&sb, ' ');
            else if(tok.type == CPP_NEWLINE)
                msb_write_char(&sb, '\n');
            else
                cpp_write_token(&sb, tok);
        }
        if(sb.cursor) *out = msb_detach_sv(&sb);
        else { *out = (StringView){0}; msb_destroy(&sb); }
    }
    finally:
    if(log_sb.cursor){
        StringView sv = msb_borrow_sv(&log_sb);
        TestPrintf("%s%s:%d:%s%s\n    %.*s", _test_color_gray, file, line, func, _test_color_reset, sv_p(sv));
    }
    ArenaAllocator_free_all(&at.arena);
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
    FileCache *fc = fc_create(a, FC_FLAGS_NONE);
    MStringBuilder log_sb = {.allocator=a};
    MsbLogger logger_ = {0};
    Logger* logger = msb_logger(&logger_, &log_sb);
    AtomTable at = {0};
    Environment env = {.allocator = a, .at=&at};
    CppPreprocessor cpp = {
        .allocator = a,
        .fc = fc,
        .at = &at,
        .logger = logger,
        .env = &env,
        .target = &cc_target_test,
    };
    fc_write_path(fc, "(test)", 6);
    int err = fc_cache_file(fc, txt);
    if(err) goto finally;
    err = cpp_define_builtin_macros(&cpp);
    if(err) goto finally;
    CppFrame frame = {
        .file_id = (uint32_t)fc->map.count - 1,
        .txt = txt,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CppFrame)(&cpp.frames, cpp.allocator, frame);
    if(err) goto finally;
    CppToken tok;
    for(;;){
        err = cpp_next_pp_token(&cpp, &tok);
        if(err) break;
        if(tok.type == CPP_END_OF_INPUT) break;
    }

    finally:;
    int result = err ? 0 : 1; // 0 = success (error was expected), 1 = failure (no error)
    if(log_sb.cursor){
        StringView sv = msb_borrow_sv(&log_sb);
        *err_out = (StringView){.length = sv.length, .text = (const char*)Allocator_dupe(MALLOCATOR, sv.text, sv.length)};
    }
    else
        *err_out = (StringView){0};
    ArenaAllocator_free_all(&at.arena);
    ArenaAllocator_free_all(&aa);
    ArenaAllocator_free_all(&cpp.synth_arena);
    return result;
}

static
int
cpp_expand_string_expect_message(StringView txt, StringView* err_out){
    ArenaAllocator aa = {0};
    Allocator a = allocator_from_arena(&aa);
    FileCache *fc = fc_create(a, FC_FLAGS_NONE);
    MStringBuilder log_sb = {.allocator=a};
    MsbLogger logger_ = {0};
    Logger* logger = msb_logger(&logger_, &log_sb);
    AtomTable at = {0};
    Environment env = {.allocator = a, .at=&at};
    CppPreprocessor cpp = {
        .allocator = a,
        .fc = fc,
        .at = &at,
        .logger = logger,
        .env = &env,
        .target = &cc_target_test,
    };
    fc_write_path(fc, "(test)", 6);
    int err = fc_cache_file(fc, txt);
    if(err) goto finally;
    err = cpp_define_builtin_macros(&cpp);
    if(err) goto finally;
    CppFrame frame = {
        .file_id = (uint32_t)fc->map.count - 1,
        .txt = txt,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CppFrame)(&cpp.frames, cpp.allocator, frame);
    if(err) goto finally;
    CppToken tok;
    for(;;){
        err = cpp_next_pp_token(&cpp, &tok);
        if(err) break;
        if(tok.type == CPP_END_OF_INPUT) break;
    }

    finally:;
    int result = err;
    if(log_sb.cursor){
        StringView sv = msb_borrow_sv(&log_sb);
        *err_out = (StringView){.length = sv.length, .text = (const char*)Allocator_dupe(MALLOCATOR, sv.text, sv.length)};
    }
    else
        *err_out = (StringView){0};
    ArenaAllocator_free_all(&at.arena);
    ArenaAllocator_free_all(&aa);
    ArenaAllocator_free_all(&cpp.synth_arena);
    return result;
}

TestFunction(test_func_macros){
    TESTBEGIN();
    static const struct {
        const char* name; StringView inp, exp; int line;
    } test_cases[] = {
        {"simple_func_macro",SVI("#define F(x) x*x\nF(2)"), SVI("\n2*2"), __LINE__},
        {"counter argument cached", SVI("#define F(x) x x\nF(__COUNTER__) __COUNTER__"), SVI("\n0 0 1"), __LINE__},
        {"counter unused and raw", SVI("#define F(x) #x\n#define DROP(x)\nF(__COUNTER__) DROP(__COUNTER__) __COUNTER__"), SVI("\n\n\"__COUNTER__\"  0"), __LINE__},
        {"counter va opt cached", SVI("#define F(...) __VA_OPT__(__VA_ARGS__ __VA_ARGS__)\nF(__COUNTER__) __COUNTER__"), SVI("\n0 0 1"), __LINE__},
        {"empty variadic forms", SVI("#define E\n#define F(...) [__VA_OPT__(yes)]\nF() F( ) F(\n) F(E) F(,)"), SVI("\n\n[] [] [] [] [yes]"), __LINE__},
        {"empty paste operands", SVI("#define C(a,b) a##b\nC(,) C(,word) C(word,)"), SVI("\n word word"), __LINE__},
        {"suppressed token through argument", SVI("#define F(x) x\n#define A F(A)\nF(A) A"), SVI("\n\nA A"), __LINE__},
        {"pasted token rescanned", SVI("#define C(a,b) a##b\n#define AB 7\n#define I(x) x\nC(A,B) I(C(A,B))"), SVI("\n\n\n7 7"), __LINE__},
        {"counter expression paths", SVI("#define F(x) (x==x)\n#if F(__COUNTER__)\nyes\n#endif\n__CALC__(F(__COUNTER__)) __COUNTER__"), SVI("\n\nyes\n\n1 2"), __LINE__},
        {"comment_before_macro_name", SVI("#define /* comment */ F(x) x\nF(2)"), SVI("\n2"), __LINE__},
        {"uninvoked_macro_before_define", SVI(
            "#define F(x) x\nF\n#define A 1\nA"), SVI("\nF\n\n1"), __LINE__},
        {"uninvoked_macro_before_conditional", SVI(
            "#define F(x) x\nF\n#if 0\nskipped\n#endif\nkept"),
            SVI("\nF\n\n\n\nkept"), __LINE__},
        {"no_space_after_rparen",SVI("#define F(x)x*x\nF(2)"), SVI("\n2*2"), __LINE__},
        {"multi_param_macro",SVI("#define ADD(a,b) a+b\nADD(1, 2)"), SVI("\n1+2"), __LINE__},
        {"stringify",SVI("#define STR(x) #x\nSTR(hello)"), SVI("\n\"hello\""), __LINE__},
        {"object hash preserves whitespace", SVI(
            "#define HASH # word\n#define STR(x) #x\n#define EXPAND(x) STR(x)\n"
            "EXPAND(HASH)"), SVI("\n\n\n\"# word\""), __LINE__},
        {"object digraph hash preserves whitespace", SVI(
            "#define HASH %: word\n#define STR(x) #x\n#define EXPAND(x) STR(x)\n"
            "EXPAND(HASH)"), SVI("\n\n\n\"%: word\""), __LINE__},
        {"stringify_with_spaces",SVI("#define STR(x) #x\nSTR(hello world)"),SVI("\n\"hello world\""),__LINE__},
        {"stringify_digraphs", SVI("#define STR(x) #x\nSTR(<: :> <% %> %: %:%:)"),
            SVI("\n\"<: :> <% %> %: %:%:\""), __LINE__},
        {"stringify_pasted_digraph", SVI(
            "#define CAT(a,b) a##b\n#define STR(x) #x\n#define EXPAND(x) STR(x)\n"
            "EXPAND(CAT(<,:))"), SVI("\n\n\n\"<:\""), __LINE__},
        {"digraph_macro_operators", SVI(
            "%:define CAT(a,b) a %:%: b\n%:define STR(x) %:x\nCAT(a,b) STR(word)"),
            SVI("\n\nab \"word\""), __LINE__},
        {"stringify_uninvoked_function_macro", SVI(
            "#define F(x) x\n#define STR(x) #x\n#define EXPAND(x) STR(x)\n"
            "EXPAND(F word)"), SVI("\n\n\n\"F word\""), __LINE__},
        {"stringify_uninvoked_function_macro_newline", SVI(
            "#define F(x) x\n#define STR(x) #x\n#define EXPAND(x) STR(x)\n"
            "EXPAND(F\nword)"), SVI("\n\n\n\"F word\""), __LINE__},
        {"token_paste",SVI("#define PASTE(a,b) a##b\nPASTE(foo, bar)"), SVI("\nfoobar"), __LINE__},
        {"token_paste_3",SVI("#define PASTE3(a,b,c) a##b##c\nPASTE3(x,y,z)"),SVI("\nxyz"),__LINE__},
        {"paste_empty_left",SVI("#define PASTE(a,b) a##b\nPASTE(,bar)"),SVI("\nbar"), __LINE__},
        {"paste_empty_right",SVI("#define PASTE(a,b) a##b\nPASTE(foo,)"),SVI("\nfoo"), __LINE__},
        {"paste_va_opt_leading_space", SVI("#define F(...) a ## __VA_OPT__( b)\nF(1)"), SVI("\nab"), __LINE__},
        {"paste_va_opt_trailing_space", SVI("#define F(...) __VA_OPT__(a ) ## b\nF(1)"), SVI("\nab"), __LINE__},
        {"paste_multiline_arg",SVI("#define X(a) x##a\nint X(\n        asd);"), SVI("\nint xasd;"), __LINE__},
        {"va_args",SVI("#define VA(...) __VA_ARGS__\nVA(a, b, c)"),SVI("\na, b, c"),__LINE__},
        {"gnu_va_args",SVI("#define VA(args...) args\nVA(a, b, c)"),SVI("\na, b, c"),__LINE__},
        {"gnu_va_args_space_before_ellipsis", SVI("#define VA(args ...) args\nVA(a, b)"), SVI("\na, b"), __LINE__},
        {"gnu_va_args_comments_before_ellipsis", SVI("#define VA(args /* one */ /* two */ ...) args\nVA(a, b)"), SVI("\na, b"), __LINE__},
        {"va_args_empty",SVI("#define VA(...) [__VA_ARGS__]\nVA()"),SVI("\n[]"),__LINE__},
        {"gnu_va_args_empty",SVI("#define VA(args...) [args]\nVA()"),SVI("\n[]"),__LINE__},
        {"va_opt_nonempty",SVI("#define VA_OPT(...) prefix __VA_OPT__(, __VA_ARGS__) suffix\nVA_OPT(1, 2)"),SVI("\nprefix , 1, 2 suffix"),__LINE__},
        {"va_opt_empty",SVI("#define VA_OPT(...) prefix __VA_OPT__(, __VA_ARGS__) suffix\nVA_OPT()"),SVI("\nprefix  suffix"),__LINE__},
        {"gnu_va_opt_nonempty",SVI("#define VA_OPT(args...) prefix __VA_OPT__(, args) suffix\nVA_OPT(1, 2)"),SVI("\nprefix , 1, 2 suffix"),__LINE__},
        {"gnu_va_opt_empty",SVI("#define VA_OPT(args...) prefix __VA_OPT__(, args) suffix\nVA_OPT()"),SVI("\nprefix  suffix"),__LINE__},
        {"nested_expansion",SVI(
            "#define F(x) x*x\n"
            "#define IDENT(x) x\n"
            "IDENT(F(3))"),SVI("\n\n3*3"), __LINE__},
        {"indirect_stringify",SVI(
            "#define STR(x) #x\n"
            "#define XSTR(x) STR(x)\n"
            "#define VALUE 42\n"
            "XSTR(VALUE)"),SVI("\n\n\n\"42\""), __LINE__},
        {"direct_stringify_no_expand",SVI(
            "#define STR(x) #x\n"
            "#define VALUE 42\n"
            "STR(VALUE)"),SVI("\n\n\"VALUE\""), __LINE__},
        {"empty_macro_in_arg",SVI(
        "#define EMPTY() \n"
        "#define IDENT(x) x\n"
        "IDENT(EMPTY())test"),SVI("\n\ntest"), __LINE__},
        {"empty_argument_in_nested_call", SVI(
            "#define F(x) [x]\n#define ID(x) x\nID(F())"), SVI("\n\n[]"), __LINE__},
        {"zero_parameter_nested_call_with_space", SVI(
            "#define F() yes\n#define ID(x) x\nID(F( ))"), SVI("\n\nyes"), __LINE__},
        {"zero_parameter_nested_call_with_newline", SVI(
            "#define F() yes\n#define ID(x) x\nID(F(\n))"), SVI("\n\nyes"), __LINE__},
        {"zero_parameter_call_with_space", SVI(
            "#define F() yes\nF( )"), SVI("\nyes"), __LINE__},
        {"zero_parameter_call_with_comment", SVI(
            "#define F() yes\nF(/* empty */)"), SVI("\nyes"), __LINE__},
        {"stringify_va_args",SVI("#define LOG(...) #__VA_ARGS__\nLOG(hello, world)"),SVI("\n\"hello, world\""),__LINE__},
        {"gnu_stringify_va_args",SVI("#define LOG(args...) #args\nLOG(hello, world)"),SVI("\n\"hello, world\""),__LINE__},
        {"variadic_with_named_param", SVI(
        "#define FOO(x, ...) x __VA_ARGS__\n"
        "FOO(1)"), SVI("\n1 "), __LINE__},
        {"gnu_variadic_with_named_param", SVI(
        "#define FOO(x, args...) x args\n"
        "FOO(1)"), SVI("\n1 "), __LINE__},
        {"variadic_with_named_param_and_args", SVI("#define FOO(x, ...) x __VA_ARGS__\nFOO(1, 2, 3)"), SVI("\n1 2, 3"), __LINE__},
        {"variadic_with_named_param_va_opt", SVI("#define FOO(x, ...) x __VA_OPT__(, __VA_ARGS__)\nFOO(1)"), SVI("\n1 "), __LINE__},
        {"variadic_with_named_param_va_opt_nonempty", SVI( "#define FOO(x, ...) x __VA_OPT__(, __VA_ARGS__)\nFOO(1, 2, 3)"),SVI("\n1 , 2, 3"), __LINE__},
        {"gnu_variadic_with_named_param_and_args", SVI("#define FOO(x, args...) x args\nFOO(1, 2, 3)"), SVI("\n1 2, 3"), __LINE__},
        {"gnu_variadic_with_named_param_va_opt", SVI("#define FOO(x, args...) x __VA_OPT__(, args)\nFOO(1)"), SVI("\n1 "), __LINE__},
        {"gnu_variadic_with_named_param_va_opt_nonempty", SVI( "#define FOO(x, args...) x __VA_OPT__(, args)\nFOO(1, 2, 3)"),SVI("\n1 , 2, 3"), __LINE__},
        // __VA_OPT__ emptiness is determined after expansion (C23 6.10.4.1)
        {"va_opt_expands_to_empty", SVI(
            "#define EMPTY\n"
            "#define F(...) __VA_OPT__(yes)no\n"
            "F(EMPTY)"), SVI("\n\nno"), __LINE__},
        {"va_opt_expands_to_nonempty", SVI(
            "#define ONE 1\n"
            "#define F(...) __VA_OPT__(yes)no\n"
            "F(ONE)"), SVI("\n\nyesno"), __LINE__},
        // param used both raw (## adjacent) and expanded in same replacement
        {"param_raw_and_expanded", SVI(
            "#define X 1\n"
            "#define F(a) a a ## 2\n"
            "F(X)"), SVI("\n\n1 X2"), __LINE__},
        // __VA_OPT__ with nested parens in content
        {"va_opt_nested_parens", SVI(
            "#define F(...) __VA_OPT__((a, b))\n"
            "F(x)"), SVI("\n(a, b)"), __LINE__},
        // multiple __VA_OPT__ in one replacement list
        {"va_opt_multiple", SVI(
            "#define F(...) [__VA_OPT__(L)] [__VA_OPT__(R)]\n"
            "F(x)\n"
            "F()"), SVI("\n[L] [R]\n[] []"), __LINE__},
        // __VA_OPT__ as left operand of ##
        {"va_opt_paste_left", SVI(
            "#define F(...) __VA_OPT__(x) ## y\n"
            "F(1)\n"
            "F()"), SVI("\nxy\ny"), __LINE__},
        {"va_opt_empty_paste_after_prefix", SVI(
            "#define F(...) prefix __VA_OPT__(x) ## y\n"
            "F()"), SVI("\nprefix y"), __LINE__},
        {"va_opt_empty_content_paste_after_prefix", SVI(
            "#define F(...) prefix __VA_OPT__() ## y\n"
            "F(1)"), SVI("\nprefix y"), __LINE__},
        {"va_opt_empty_paste_after_punctuator", SVI(
            "#define F(...) +__VA_OPT__(x) ## y\n"
            "F()"), SVI("\n+y"), __LINE__},
        // empty __VA_OPT__ content with non-empty VA_ARGS
        {"va_opt_empty_content", SVI(
            "#define F(...) a __VA_OPT__() b\n"
            "F(x)"), SVI("\na  b"), __LINE__},
        // # __VA_OPT__ with ## inside content
        {"stringify_va_opt_with_paste", SVI(
            "#define F(...) #__VA_OPT__(a ## b)\n"
            "F(x)\n"
            "F()"), SVI("\n\"ab\"\n\"\""), __LINE__},
        // extensions
        // __VA_COUNT__
        {"va_count_basic", SVI("#define F(...) __VA_COUNT__\nF(a, b, c)"), SVI("\n3"), __LINE__},
        {"va_count_one", SVI("#define F(...) __VA_COUNT__\nF(a)"), SVI("\n1"), __LINE__},
        {"va_count_empty", SVI("#define F(...) __VA_COUNT__\nF()"), SVI("\n0"), __LINE__},
        {"va_count_whitespace", SVI("#define F(...) __VA_COUNT__\nF( )"), SVI("\n0"), __LINE__},
        {"va_count_comment", SVI("#define F(...) __VA_COUNT__\nF(/* empty */)"), SVI("\n0"), __LINE__},
        {"va_count_whitespace_stringify", SVI("#define F(...) #__VA_COUNT__\nF( )"), SVI("\n\"0\""), __LINE__},
        {"va_count_whitespace_paste", SVI("#define F(...) func ## __VA_COUNT__\nF(\n)"), SVI("\nfunc0"), __LINE__},
        {"va_count_explicit_empty_arguments", SVI("#define F(...) __VA_COUNT__\nF( , )"), SVI("\n2"), __LINE__},
        {"va_count_named", SVI("#define F(x, ...) __VA_COUNT__\nF(1, 2, 3)"), SVI("\n2"), __LINE__},
        {"va_count_named_empty", SVI("#define F(x, ...) __VA_COUNT__\nF(1)"), SVI("\n0"), __LINE__},
        {"va_count_paste", SVI("#define F(...) func ## __VA_COUNT__\nF(a, b)"), SVI("\nfunc2"), __LINE__},
        {"va_count_stringify", SVI("#define F(...) #__VA_COUNT__\nF(a, b)"), SVI("\n\"2\""), __LINE__},
        {"va_count_overloading", SVI("#define F1() 1\n#define F2() 2\n#define CAT(a, b) CAT2(a, b)\n#define CAT2(a, b) a##b\n#define F(...) CAT(F, __VA_COUNT__)()\nF(a)\nF(a,b)\nF(a,b,c)"),
                                 SVI("\n\n\n\n\n1\n2\nF3()"), __LINE__},
        // __VA_ARG__
        {"va_arg_basic", SVI("#define F(...) __VA_ARG__(0)\nF(a, b, c)"), SVI("\na"), __LINE__},
        {"va_arg_second", SVI("#define F(...) __VA_ARG__(1)\nF(a, b, c)"), SVI("\nb"), __LINE__},
        {"va_arg_last", SVI("#define F(...) __VA_ARG__(2)\nF(a, b, c)"), SVI("\nc"), __LINE__},
        {"va_arg_named", SVI("#define F(x, ...) __VA_ARG__(0)\nF(1, 2, 3)"), SVI("\n2"), __LINE__},
        {"va_arg_named2", SVI("#define F(x, ...) __VA_ARG__(1)\nF(1, 2, 3)"), SVI("\n3"), __LINE__},
        {"va_arg_stringify", SVI("#define F(...) #__VA_ARG__(1)\nF(a, b, c)"), SVI("\n\"b\""), __LINE__},
        {"va_arg_paste", SVI("#define F(...) prefix ## __VA_ARG__(0)\nF(1, 2)"), SVI("\nprefix1"), __LINE__},
        {"va_arg_expr", SVI("#define F(...) __VA_ARG__(__VA_COUNT__ - 1)\nF(a, b, c)"), SVI("\nc"), __LINE__},
        {"va_arg_expand", SVI("#define X 42\n#define F(...) __VA_ARG__(0)\nF(X)"), SVI("\n\n42"), __LINE__},
        {"identical func redef", SVI("#define F(a, b) a + b\n#define F(a, b) a + b\nF(1, 2)"), SVI("\n\n1 + 2"), __LINE__},
        {"identical func redef with leading comment", SVI(
            "#define F(x) /* comment */ x\n#define F(x) x\nF(1)"), SVI("\n\n1"), __LINE__},
        {"identical func redef with adjacent comments", SVI(
            "#define F(x)/**//**/x\n#define F(x) x\nF(1)"), SVI("\n\n1"), __LINE__},
        {"va_opt_paste_right", SVI(
            "#define F(...) x ## __VA_OPT__(y)\n"
            "F(1)\n"
            "F()"), SVI("\nxy\nx"), __LINE__},
        // __RAND__
        // deterministic seed, so this is ok.
        {"__RAND__", SVI("__RAND__"), SVI("1364989054"), __LINE__},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(int, err);
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
    static const struct {
        const char* name; StringView inp, exp; int line;
    } test_cases[] = {
        // ## __VA_ARGS
        {"gnu comma keeps explicitly empty argument", SVI("#define F(x,...) f(x,##__VA_ARGS__)\nF(1) F(1,)"), SVI("\nf(1) f(1,)"), __LINE__},
        {"gnu comma argument expansion cached", SVI("#define F(...) __VA_ARGS__ ,##__VA_ARGS__\nF(__COUNTER__)"), SVI("\n0 ,0"), __LINE__},
        {"gnu comma expansion cached before ordinary use", SVI("#define F(...) ,##__VA_ARGS__ __VA_ARGS__\nF(__COUNTER__)"), SVI("\n,0 0"), __LINE__},
        {"gnu named comma keeps explicitly empty argument", SVI("#define F(x,args...) f(x,##args)\nF(1) F(1,)"), SVI("\nf(1) f(1,)"), __LINE__},
        {"gnu va args comma",SVI("#define F(...) f(1, ##__VA_ARGS__)\nF(2)\nF(2,3)\nF()"), SVI("\nf(1,2)\nf(1,2,3)\nf(1)"), __LINE__},
        {"gnu va args comma2",SVI("#define F(...) f(1, ## __VA_ARGS__)\nF(2)\nF(2,3)\nF()"), SVI("\nf(1,2)\nf(1,2,3)\nf(1)"), __LINE__},
        {"gnu va args nocomma",SVI("#define F(...) f(1 ##__VA_ARGS__)\nF(2)\nF(2, 3)\nF()"), SVI("\nf(12)\nf(12, 3)\nf(1)"), __LINE__},
        {"gnu gnu va args comma",SVI("#define F(args...) f(1, ##args)\nF(2)\nF(2,3)\nF()"), SVI("\nf(1,2)\nf(1,2,3)\nf(1)"), __LINE__},
        {"gnu gnu va args comma2",SVI("#define F(args...) f(1, ## args)\nF(2)\nF(2,3)\nF()"), SVI("\nf(1,2)\nf(1,2,3)\nf(1)"), __LINE__},
        {"gnu gnu va args nocomma",SVI("#define F(args...) f(1 ##args)\nF(2)\nF(2, 3)\nF()"), SVI("\nf(12)\nf(12, 3)\nf(1)"), __LINE__},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(int, err);
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
    static const struct {
        const char* name; StringView inp, exp; int line;
    } test_cases[] = {
        {"recursive_self_reference", SVI("#define X X\nX"), SVI("\nX"), __LINE__},
        {"mutual_recursion", SVI(
        "#define A B*B\n"
        "#define B C*C\n"
        "#define C A*A\n"
        "A"), SVI("\n\n\nA*A*A*A*A*A*A*A"), __LINE__},
        {"empty_obj_macro", SVI("#define Z\nZ"), SVI("\n"), __LINE__},
        {"self_ref", SVI("#define Y Y\nY"), SVI("\nY"), __LINE__},
        {"parens_macro", SVI("#define PARENS ()\nPARENS"), SVI("\n()"), __LINE__},
        // ## in object-like macros should paste during expansion
        {"obj_paste_simple", SVI("#define AB a ## b\nAB"), SVI("\nab"), __LINE__},
        {"obj_paste_hash", SVI("#define HH # ## #\nHH"), SVI("\n##"), __LINE__},
        {"obj_paste_multi", SVI("#define XYZ x ## y ## z\nXYZ"), SVI("\nxyz"), __LINE__},
        {"identical obj redef", SVI("#define X 42\n#define X 42\nX"), SVI("\n\n42"), __LINE__},
        {"standalone trailing backslash", SVI(" \\"), SVI(" \\"), __LINE__},
        {"backslash after identifier at eof", SVI("word\\"), SVI("word\\"), __LINE__},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(int, err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "exp", "result", &TEST_stats, __FILE__, __func__, line)){
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
    static const struct {
        const char* name;
        int line;
        StringView input, output;
    } test_cases[] = {
        {
            "Example 1", __LINE__,
            SVI(
            "#define LPAREN() (\n"
            "#define G(Q) 42\n"
            "#define F(R, X, ...) __VA_OPT__(G R X) )\n"
            "int x = F(LPAREN(), 0, <:-);\n" // replaced by int x = 42;
            ),
            SVI("\n\n\nint x = 42;\n")
        },
        {
            "Example 2", __LINE__,
            SVI(
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
            SVI(
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
            SVI( "#define hash_hash # ## #\n"
                "#define mkstr(a) # a\n"
                "#define in_between(a) mkstr(a)\n"
                "#define join(c, d) in_between(c hash_hash d)\n"
                "join(x, y)"),
            SVI("\n\n\n\n\"x ## y\""),
        },

        {
            "Ambiguous Example", __LINE__,
            SVI("#define f(a) a*g\n"
               "#define g(a) f(a)\n"
               "f(2)(9)"),
            // standard says either of these are allowed,
            // but actual compilers produce the second one
            // SVI("\n\n2*f(9)"),
            SVI("\n\n2*9*g"),
        },
        // 6.10.5.6
        {
            "example 1", __LINE__,
            SVI( "#define TABSIZE 100\n"
                "int table[TABSIZE];\n"),
            SVI("\nint table[100];\n"),
        },
        {
            "example 2 (extended)", __LINE__,
            SVI("#define max(a, b) ((a) > (b) ? (a): (b))\nmax(1,2)\n"),
            SVI("\n((1) > (2) ? (1): (2))\n"),
        },
        {
            "Redef etc. (example 3)", __LINE__,
            SVI( "#define x 3\n"
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
            SVI( "\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                "f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);\n"
                "f(2 * (2+(3,4)-0,1)) | f(2 * (~ 5)) & f(2 * (0,1))^m(0,1);\n"
                "int i[] = { 1, 23, 4, 5,  };\n"
                "char c[2][6] = { \"hello\", \"\" };\n"),
        },
        {
            "example 4", __LINE__,
            SVI( "#define str(s) # s\n"
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
            SVI( "\n\n\n\n\n\n\n\n"
                "printf(\"x\" \"1\" \"= %d, x\" \"2\" \"= %s\", x1, x2);\n"
                "fputs(\"strncmp(\\\"abc\\\\0d\\\", \\\"abc\\\", ’\\4’) == 0\" \": @\", s);\n"
                "\"vers2.h\"\n"
                "\"hello\";\n"
                "\"hello\" \", world\"\n"),
        },
        {
            "placeholders example (example 5)", __LINE__,
            SVI( "#define t(x,y,z) x ## y ## z\n"
                "int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,),\n"
                "t(10,,), t(,11,), t(,,12), t(,,) };\n"),
            SVI( "\n"
                "int j[] = { 123, 45, 67, 89,\n"
                "10, 11, 12,  };\n"),
        },
        {
            "redefs (example 6)", __LINE__,
            SVI( "#define OBJ_LIKE (1-1)\n"
                "#define OBJ_LIKE /* white space */ (1-1) /* other */\n"
                "#define FUNC_LIKE(a) ( a )\n"
                "#define FUNC_LIKE( a ) ( /* note the white space */ \\\n"
                "a /* other stuff on this line\n"
                "*/)\n"),
            SVI("\n\n\n\n"),
        },
        {
            "redefs2", __LINE__,
            SVI( "#define OBJ_LIKE (/**/ 1-1)\n"
                "#define OBJ_LIKE /* white space */ ( 1-1) /* other */\n"
                "#define FUNC_LIKE(a) ( a/* */)\n"
                "#define FUNC_LIKE( a ) ( /* note the white space */ \\\n"
                "a /* other stuff on this line\n"
                "*/)\n"),
            SVI("\n\n\n\n"),
        },
        {
            "6.10.5 example 7", __LINE__,
            SVI( "#define debug(...) fprintf(stderr, __VA_ARGS__)\n"
                "#define showlist(...) puts(#__VA_ARGS__)\n"
                "#define report(test, ...) ((test)?puts(#test):\\\n"
                " printf(__VA_ARGS__))\n"
                "debug(\"Flag\");\n"
                "debug(\"X = %d\\n\", x);\n"
                "showlist(The first, second, and third items.);\n"
                "report(x>y, \"x is %d but y is %d\", x, y);\n"),
            SVI( "\n\n\n"
                "fprintf(stderr, \"Flag\");\n"
                "fprintf(stderr, \"X = %d\\n\", x);\n"
                "puts(\"The first, second, and third items.\");\n"
                "((x>y)?puts(\"x>y\"): printf(\"x is %d but y is %d\", x, y));\n"),
        },
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < sizeof test_cases / sizeof test_cases[0]; i = test_atomic_increment(&idx)){
        int line = test_cases[i].line;
        err = cpp_expand_string(test_cases[i].input, &result, __FILE__, __func__, line);
        TestExpectFalse(int, err);
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
    static const struct {
        const char* name; int line;
        StringView in, out;
    } test_cases[] = {
        {
            "", __LINE__,
            SVI("#define S(x) #x\n"
               "S(\"hello\\\\\")\n"),
            SVI("\n"
               "\"\\\"hello\\\\\\\\\\\"\"\n"),
        },
        {
            "", __LINE__,
            SVI("#define a b\n"
               "#define M(x) x x##y\n"
               "M(a)\n"),
            SVI("\n"
               "\n"
               "b ay\n")
        },
        {
            "", __LINE__,
            SVI("#define CAT(a,b) a##b\n"
               "#define FOO BAR\n"
               "CAT(FO,O)\n"),
            SVI("\n"
               "\n"
               "BAR\n"),
        },
        {
            "", __LINE__,
            SVI("#define FOO(a,b) a b\n"
               "FOO((x,y),z)\n"),
            SVI("\n"
               "(x,y) z\n"),
        },
        {
            "", __LINE__,
            SVI("#define CAT(a,b,c,d) a##b##c##d\n"
               "CAT(1, e, -, 2)\n"),
            SVI("\n"
               "1e-2\n")
        },
        {
            "", __LINE__,
            SVI("#define L (\n"
               "#define M() 1\n"
               "M L )\n"),
            SVI("\n"
               "\n"
               "M ( )\n")
        },
        {
            "", __LINE__,
            SVI( "#define M() 1\n"
               "M\n"),
            SVI("\n"
               "M\n")
        },
        {
            "stringify strips leading/trailing whitespace", __LINE__,
            SVI("#define S(x) #x\n"
               "#define REDIRECT(name, alias) name __asm__(S(alias))\n"
               "REDIRECT(sscanf,\n"
               "         __isoc99_sscanf)\n"),
            SVI("\n"
               "\n"
               "sscanf __asm__(\"__isoc99_sscanf\")\n"),
        },
        {
            "", __LINE__,
            SVI("#define M(...) __VA_OPT__(yes)no\n"
               "M()\n"
               "M(a)\n"
               "M( )\n"),
            SVI("\n"
               "no\n"
               "yesno\n"
               "no\n"),
        },
        {
            "", __LINE__,
            SVI("#define M(x, ...) __VA_OPT__(yes)no\n"
               "M(1)\n"
               "M(1, a)\n"
               "M(1, )\n"),
            SVI("\n"
               "no\n"
               "yesno\n"
               "no\n"),
        },
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < sizeof test_cases / sizeof test_cases[0]; i = test_atomic_increment(&idx)){
        int line = test_cases[i].line;
        StringView in = test_cases[i].in;
        StringView out = test_cases[i].out;
        err = cpp_expand_string(in, &result, __FILE__, __func__, line);
        TestExpectFalse(int, err);
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
    static const struct {
        const char* name; StringView inp, exp; int line; _Bool disabled;
    } test_cases[] = {
        {"__FILE__", SVI("__FILE__"), SVI("\"(test)\""), __LINE__, 0},
        {"__LINE__", SVI("__LINE__"), SVI("1"), __LINE__, 0},
        {"line after leading continuation", SVI("\\\n__LINE__"), SVI("2"), __LINE__, 0},
        {"line after leading CRLF continuation", SVI("\\\r\n__LINE__"), SVI("2"), __LINE__, 0},
        {"line after continuation at line start", SVI("\n\\\n__LINE__"), SVI("\n3"), __LINE__, 0},
        {"__COLUMN__", SVI("__COLUMN__"), SVI("1"), __LINE__, 0},
        {"__FILE_ID__", SVI("__FILE_ID__"), SVI("0"), __LINE__, 0}, // just happens to be 0, could change
        {"__LI\\NE__", SVI("__LI\\\nNE__"), SVI("1"), __LINE__, 0},
        {"__COUNTER__", SVI("__COUNTER__\n__COUNTER__\n__COUNTER__\n__COUNTER__ __COUNTER__"), SVI("0\n1\n2\n3 4"), __LINE__, 0},
        {"counter paste", SVI("#define C(x,y) x##y\n"
                             "#define E(x) x\n"
                             "#define C2(x,y) E(C(x, y))\n"
                             "C(__COUNTER__, __COUNTER__)\n"
                             "C2(__COUNTER__,__COUNTER__)\n"
                             "E(C(__COUNTER__, __COUNTER__))"), SVI("\n\n\n__COUNTER____COUNTER__\n01\n__COUNTER____COUNTER__"), __LINE__, 0},
        {"multiple", SVI("__FILE__\n__LINE__\n__LINE__\n__FILE__"), SVI("\"(test)\"\n2\n3\n\"(test)\""), __LINE__, 0},
        {"indirect", SVI("#define L1 L2\n#define L2 __LINE__\nL1"), SVI("\n\n3"), __LINE__, 0},
        {"__FILE_NAME__", SVI("__FILE_NAME__"), SVI("\"(test)\""), __LINE__, 0},
        {"__INCLUDE_LEVEL__", SVI("__INCLUDE_LEVEL__"), SVI("0"), __LINE__, 0},
        {"__DATE__", SVI("__DATE__"), SVI("\"Jan 01 1900\""), __LINE__, 0},
        {"__TIME__", SVI("__TIME__"), SVI("\"01:02:03\""), __LINE__, 0},

        // extensions
        {"__CALC__", SVI("__CALC__(1+1)"), SVI("2"), __LINE__, 0},
        {"negative predefined exponents", SVI(
            "__CALC__(__FLT_MIN_EXP__ == -125) "
            "__CALC__(__DBL_MIN_10_EXP__ == -307) "
            "__CALC__(__LDBL_MIN_EXP__ < 0)"), SVI("1 1 1"), __LINE__, 0},
        {"predefined minimum expression", SVI(
            "__CALC__(__SIG_ATOMIC_MIN__ == (-__SIG_ATOMIC_MAX__-1))"), SVI("1"), __LINE__, 0},
        {"predefined expression in if", SVI(
            "#if __FLT_MIN_EXP__ < 0 && __SIG_ATOMIC_MIN__ < 0\nyes\n#endif"),
            SVI("\nyes\n"), __LINE__, 0},
        {"calc multiline defined", SVI("__CALC__(defined\n__CALC__)"), SVI("1"), __LINE__, 0},
        {"calc multiline macro call", SVI("#define F(x) x\n__CALC__(F\n(7))"), SVI("\n7"), __LINE__, 0},
        {"calc multiline has_include", SVI("__CALC__(__has_include(\n\"missing-newline-test.h\"\n))"), SVI("0"), __LINE__, 0},
        {"pragma newline after empty macro", SVI("#define EMPTY\n_Pragma(EMPTY\n\"unknown_pragma\")ok"), SVI("\nok"), __LINE__, 0},
        {"pragma multiline name", SVI("__pragma(\nunknown_pragma\n)ok"), SVI("ok"), __LINE__, 0},
        {"__CALC__", SVI("__CALC__(defined __CALC__)"), SVI("1"), __LINE__, 0},
        {"__calc", SVI("__calc(3*4/3)"), SVI("4"), __LINE__, 0},
        {"__calc", SVI("__calc(3-4)"), SVI("-1"), __LINE__, 0},
        {"__calc int64min", SVI("__calc(-9223372036854775807 - 1)"), SVI("(-9223372036854775807ll-1)"), __LINE__, 0},
        {"calc signed negative result", SVI("__CALC__(__CALC__(-2147483648) < 0)"), SVI("1"), __LINE__, 0},
        {"calc signed positive result", SVI("__CALC__(__CALC__(2147483648) > -1)"), SVI("1"), __LINE__, 0},
        {"calc signed minimum result", SVI("__CALC__(__CALC__(-9223372036854775807 - 1) < 0)"), SVI("1"), __LINE__, 0},
        {"calc small unsigned result", SVI("__CALC__(__CALC__(1u) < -1)"), SVI("1"), __LINE__, 0},
        {"calc maximum unsigned result", SVI("__CALC__(__CALC__(18446744073709551615u) / 2)"),
            SVI("9223372036854775807llu"), __LINE__, 0},
        {"__MIXIN__", SVI("__MIXIN__(\"3\")"), SVI("3"), __LINE__, 0},
        {"__MIXIN__", SVI("__MIXIN__(__mixin(\"\\\"3\\\"\"))"), SVI("3"), __LINE__, 0},
        {"__mixin", SVI("#define S(x) #x\n__mixin(S(1)\"+1\")"), SVI("\n1+1"), __LINE__, 0},
        {"__env", SVI("__env(\"wolo\")"), SVI("\"woo\""), __LINE__, 0},
        {"__ENV__", SVI("__ENV__(\"foobar\")"), SVI(""), __LINE__, 0},
        {"__if", SVI("__if(1, 2, 3)"), SVI("2"), __LINE__, 0},
        {"__if", SVI("__if(1, , __calc(1/0))"), SVI(""), __LINE__, 0},
        {"__IF__", SVI("#define X 1\n__IF__(X, 3, __calc(1/0))"), SVI("\n3"), __LINE__, 0},
        {"__ident", SVI("__ident(\"this is an ident\")"), SVI("this is an ident"), __LINE__, 0}, // can't really tell from the stringification, but this is a single token
        {"__FORMAT__", SVI("__format(\"%d = %s\", 10, \"hello world\")"), SVI("\"10 = hello world\""), __LINE__, 0},
        {"__FORMAT__", SVI("__format(\"%d = %d\", - - -10, +-+-+-4)"), SVI("\"-10 = -4\""), __LINE__, 0},
        {"format integer expression", SVI("__format(\"%d:%d\", (2 + 3) * 4, 1 << 5)"), SVI("\"20:32\""), __LINE__, 0},
        {"format integer defined", SVI("#define X 42\n__FORMAT__(\"%d:%d\", defined(X) + X, defined(missing))"), SVI("\n\"43:0\""), __LINE__, 0},
        {"format integer lazy evaluation", SVI("__format(\"%d:%d\", 0 && (1 / 0), 1 ? 7 : (1 / 0))"), SVI("\"0:7\""), __LINE__, 0},
        {"format integer limits", SVI("__format(\"%d:%d\", -9223372036854775807LL - 1, -1ULL)"), SVI("\"-9223372036854775808:18446744073709551615\""), __LINE__, 0},
        {"format expanded format and nested args", SVI("#define F \"%d:%s\"\n#define ADD(a,b) ((a)+(b))\n__format(F, ADD(2, 3), __format(\"%d\", 3 * 4))"), SVI("\n\n\"5:12\""), __LINE__, 0},
        {"format octal integer", SVI("__format(\"%d\", 010)"), SVI("\"8\""), __LINE__, 0},
        {"format integer suffix", SVI("__format(\"%d\", 42ULL)"), SVI("\"42\""), __LINE__, 0},
        {"format string identifiers", SVI("__format(\"%s\", hello world)"), SVI("\"helloworld\""), __LINE__, 0},
        {"format string pp number spellings", SVI("__format(\"%s\", 010 42ULL 1'024 1e+foo)"), SVI("\"01042ULL1'0241e+foo\""), __LINE__, 0},
        {"format string mixed tokens and whitespace", SVI("__format(\"%s\", item \t\n /* gap */ 42 \"\\n\\\"\")"), SVI("\"item42\\012\\\"\""), __LINE__, 0},
        {"format string expanded arguments", SVI("#define LABEL item\n#define VALUE 0x2a\n__format(\"%s:%s:%d\", LABEL VALUE, next 7, 8)"), SVI("\n\n\"item0x2a:next7:8\""), __LINE__, 0},
        {"format separated integer", SVI("__format(\"%d\", 1'024)"), SVI("\"1024\""), __LINE__, 0},
        {"format signed suffixed octal", SVI("__format(\"%d\", - - -010L)"), SVI("\"-8\""), __LINE__, 0},
        {"__set/__get basic", SVI("__set(x, 42)\n__get(x)"), SVI("\n42"), __LINE__, 0},
        {"__get unset", SVI("__get(x)"), SVI(""), __LINE__, 0},
        {"__set overwrite", SVI("__set(x, 1)\n__set(x, 2)\n__get(x)"), SVI("\n\n2"), __LINE__, 0},
        {"__set multiple tokens", SVI("__set(stuff, a b c)\n__get(stuff)"), SVI("\na b c"), __LINE__, 0},
        {"__set with calc", SVI("__set(n, 0)\n__set(n, __calc(__get(n) + 1))\n__get(n)"), SVI("\n\n1"), __LINE__, 0},
        {"__set empty", SVI("__set(e)\n__get(e)"), SVI("\n"), __LINE__, 0},
        {"__set count fields", SVI(
            "#define FIELD(type, name) type name; __set(n, __calc(__get(n) + 1))\n"
            "__set(n, 0)\n"
            "FIELD(int, x)\n"
            "FIELD(float, y)\n"
            "FIELD(char*, name)\n"
            "__get(n)"), SVI(
            "\n"
            "\n"
            "int x; \n"
            "float y; \n"
            "char* name; \n"
            "3"), __LINE__, 0},
        {"__set accumulate", SVI(
            "__set(init_calls, )\n"
            "#define REG(fn) __set(init_calls, __get(init_calls) fn();)\n"
            "REG(init_a)\n"
            "REG(init_b)\n"
            "REG(init_c)\n"
            "__get(init_calls)"), SVI(
            "\n"
            "\n"
            "\n"
            "\n"
            "\n"
            "init_a(); init_b(); init_c();"), __LINE__, 0},
        {"__append accumulate", SVI(
            "#define REG2(fn) __append(calls, fn(); )\n"
            "REG2(init_a)\n"
            "REG2(init_b)\n"
            "REG2(init_c)\n"
            "__get(calls)"), SVI(
            "\n"
            "\n"
            "\n"
            "\n"
            "init_a();init_b();init_c();"), __LINE__, 0},
        {"__set with __if", SVI(
            "__set(x, 1)\n"
            "__if(__get(x), yes, no)"), SVI(
            "\n"
            "yes"), __LINE__, 0},
        {"__set with __format", SVI(
            "__set(count, 3)\n"
            "__format(\"count=%d\", __get(count))"), SVI(
            "\n"
            "\"count=3\""), __LINE__, 0},
        {"__set enum strings", SVI(
            "__set(n, 0)\n"
            "__set(strs, )\n"
            "#define EV(name) name = __get(n), __set(strs, __get(strs) [name] = #name,) __set(n, __calc(__get(n) + 1))\n"
            "enum { EV(RED) EV(GREEN) EV(BLUE) };\n"
            "const char* names[] = { __get(strs) };"), SVI(
            "\n"
            "\n"
            "\n"
            "enum { RED = 0,   GREEN = 1,   BLUE = 2,   };\n"
            "const char* names[] = { [RED] = \"RED\", [GREEN] = \"GREEN\", [BLUE] = \"BLUE\", };"), __LINE__, 0},
        {"__for negative indices", SVI(
            "#define P(n) __CALC__((n) * (n))\n"
            "__for(-2, 1, P)"), SVI("\n4 1 0"), __LINE__, 0},
        {"__for minimum index signed evaluation", SVI(
            "#define P(i) __format(\"%d:%d\", (i) < 0, (i) / 2)\n"
            "__for(-9223372036854775807 - 1, -9223372036854775807, P)"),
            SVI("\n\"1:-4611686018427387904\""), __LINE__, 0},
        {"__for minimum index", SVI(
            "#define P(n) __CALC__((n) == (-9223372036854775807 - 1))\n"
            "__for(-9223372036854775807 - 1, -9223372036854775807, P)"),
            SVI("\n1"), __LINE__, 0},
        {"__for negative stringify", SVI(
            "#define P(n) #n\n__for(-2, 1, P)"),
            SVI("\n\"-2\" \"-1\" \"0\""), __LINE__, 0},
        {"__for basic", SVI(
            "#define DECL(i) int x##i;\n"
            "__for(0, 3, DECL)"), SVI(
            "\n"
            "int x0; int x1; int x2;"), __LINE__, 0},
        {"__for expr", SVI(
            "#define P(i) i\n"
            "__for(1+1, 5, P)"), SVI(
            "\n"
            "2 3 4"), __LINE__, 0},
        {"__for empty", SVI(
            "#define P(i) i\n"
            "__for(3, 3, P)"), SVI(
            "\n"
            ""), __LINE__, 0},
        {"__for accumulate", SVI(
            "#define ACC(i) __append(items, i,)\n"
            "__for(0, 4, ACC)\n"
            "int arr[] = {__get(items)};"), SVI(
            "\n"
            "   \n"
            "int arr[] = {0,1,2,3,};"), __LINE__, 0},
        {"__for stringify", SVI(
            "#define ENTRY(i) [i] = #i,\n"
            "const char* t[] = { __for(0, 3, ENTRY) };"), SVI(
            "\n"
            "const char* t[] = { [0] = \"0\", [1] = \"1\", [2] = \"2\", };"), __LINE__, 0},
        {"__map basic", SVI(
            "#define DBL(x) (x*2)\n"
            "__map(DBL, 1, 2, 3)"), SVI(
            "\n"
            "(1*2) (2*2) (3*2)"), __LINE__, 0},
        {"__map stringify", SVI(
            "#define S(x) #x,\n"
            "const char* names[] = { __map(S, foo, bar, baz) };"), SVI(
            "\n"
            "const char* names[] = { \"foo\", \"bar\", \"baz\", };"), __LINE__, 0},
        {"__map paste", SVI(
            "#define DECL(name) int name##_val;\n"
            "__map(DECL, alpha, beta)"), SVI(
            "\n"
            "int alpha_val; int beta_val;"), __LINE__, 0},
        {"__map single", SVI(
            "#define ID(x) x\n"
            "__map(ID, hello)"), SVI(
            "\n"
            "hello"), __LINE__, 0},
        {"__map multi-token", SVI(
            "#define PARENS(x) (x)\n"
            "__map(PARENS, a + b, c * d)"), SVI(
            "\n"
            "(a + b) (c * d)"), __LINE__, 0},
        {"__map tuple", SVI(
            "#define UNPACK(...) __VA_ARGS__\n"
            "#define STRIP(x) UNPACK x\n"
            "__map(STRIP, (a + b), (c * d))"), SVI(
            "\n\n"
            "a + b c * d"), __LINE__, 0},
        {"__let obj", SVI(
            "__let(X, 42, X + X)"), SVI(
            "42 + 42"), __LINE__, 0},
        {"__let fn", SVI(
            "__let(DBL(x), x * 2, DBL(5))"), SVI(
            "5 * 2"), __LINE__, 0},
        {"__let scoped", SVI(
            "__let(TMP, hello, TMP)\nTMP"), SVI(
            "hello\nTMP"), __LINE__, 0},
        {"__let paste", SVI(
            "__let(MK(prefix), prefix##_init, MK(foo) MK(bar))"), SVI(
            "foo_init bar_init"), __LINE__, 0},
        {"__let stringify", SVI(
            "__let(S(x), #x, S(hello))"), SVI(
            "\"hello\""), __LINE__, 0},
        {"__let curry map", SVI(
            "__let(PAIR(x), make_pair(ctx, x), __map(PAIR, a, b, c))"), SVI(
            "make_pair(ctx, a) make_pair(ctx, b) make_pair(ctx, c)"), __LINE__, 0},
        {"__let multi param", SVI(
            "__let(ADD(a, b), a + b, ADD(1, 2))"), SVI(
            "1 + 2"), __LINE__, 0},
        {"__let newline before parameter list", SVI(
            "__let(ADD\n(a, b), a + b, ADD(1, 2))"), SVI("1 + 2"), __LINE__, 0},
        {"__let newline after parameter comma", SVI(
            "__let(ADD(a,\nb), a + b, ADD(1, 2))"), SVI("1 + 2"), __LINE__, 0},
        {"__let multiline stringify", SVI("__let(S(x), #\nx, S(word))"), SVI("\"word\""), __LINE__, 0},
        {"__let multiline paste", SVI("__let(C(a,b), a ##\nb, C(x,y))"), SVI("xy"), __LINE__, 0},
        {"__append enum strings", SVI(
            "__set(n2, 0)\n"
            "#define EV2(name) name = __get(n2), __append(strs2, [name] = #name,) __set(n2, __calc(__get(n2) + 1))\n"
            "enum { EV2(RED) EV2(GREEN) EV2(BLUE) };\n"
            "const char* names[] = { __get(strs2) };"), SVI(
            "\n"
            "\n"
            "enum { RED = 0,   GREEN = 1,   BLUE = 2,   };\n"
            "const char* names[] = { [RED] = \"RED\",[GREEN] = \"GREEN\",[BLUE] = \"BLUE\", };"), __LINE__, 0},
        {"__BASE_FILE__", SVI("__BASE_FILE__"), SVI("\"(test)\""), __LINE__, 0},
        {"__TIMESTAMP__", SVI("__TIMESTAMP__"), SVI("\"??? ??? ?? ??:??:?? ????\""), __LINE__, 0},
        {"__format", SVI("__format(\"%d + %d = %d\", 1, 2, 3)"), SVI("\"1 + 2 = 3\""), __LINE__, 0},
        {"defblock obj", SVI("#defblock HELLO\nhello world\n#endblock\nHELLO"), SVI("\nhello world"), __LINE__, 0},
        {"defblock object hash whitespace", SVI("#defblock H\n# word\n#endblock\n#define S(x) #x\n#define E(x) S(x)\nE(H)"), SVI("\n\n\n\"# word\""), __LINE__, 0},
        {"defblock inline object hash whitespace", SVI("#defblock H\na # word\n#endblock\n#define S(x) #x\n#define E(x) S(x)\nE(H)"), SVI("\n\n\n\"a # word\""), __LINE__, 0},
        {"defblock comment before name", SVI("#defblock /* comment */ HELLO\nhello world\n#endblock\nHELLO"), SVI("\nhello world"), __LINE__, 0},
        {"defifndef comment before name", SVI("#defifndef /* comment */ HELLO hello\nHELLO"), SVI("\nhello"), __LINE__, 0},
        {"defblock func", SVI("#defblock ADD(x, y)\nx + y\n#endblock\nADD(3, 4)"), SVI("\n3 + 4"), __LINE__, 0},
        {"defblock variadic", SVI("#defblock LOG(fmt, ...)\nfmt __VA_ARGS__\n#endblock\nLOG(hello, world)"), SVI("\nhello world"), __LINE__, 0},
        {"defblock named variadic with comments", SVI("#defblock VA(args /* one */ /* two */ ...)\nargs\n#endblock\nVA(a, b)"), SVI("\na, b"), __LINE__, 0},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        if(test_cases[i].disabled) continue;
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(int, err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "exp", "result", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        if(result.length) Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

TestFunction(test_va_opt_error_propagation){
    TESTBEGIN();
    static const StringView inputs[] = {
        SVI("#define B(x) x\n#define F(...) __VA_OPT__(yes)\nF(B(1,2))"),
        SVI("#define B(x) x\n#define F(...) #__VA_OPT__(yes)\nF(B(1,2))"),
        SVI("#define B(x) x\n#define F(...) prefix ## __VA_OPT__(yes)\nF(B(1,2))"),
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(inputs); i = test_atomic_increment(&idx)){
        StringView message;
        int err = cpp_expand_string_expect_error(inputs[i], &message);
        TestExpectFalse(int, err);
        test_expect_equals_sv(SV("(test):3:6: error: Too many arguments to function-like macro B()\n"),
                              message, "expected", "message", &TEST_stats, __FILE__, __func__, __LINE__);
        if(message.text) Allocator_free(MALLOCATOR, message.text, message.length);
    }
    TESTEND();
}

TestFunction(test_error_locations){
    TESTBEGIN();
    static const struct {
        const char* name; int line; StringView inp, exp;
    } test_cases[] = {
        {"va_arg whitespace-only arguments", __LINE__,
            SVI("#define F(...) __VA_ARG__(0)\nF( )"),
            SVI("(test):1:16: error: __VA_ARG__ index out of range\n"
               "(test):2:1: ... expanded from here\n")},
        {"indirect too many args", __LINE__,
            SVI("#define F(x) x\n"
               "#define M F(1,2)\n"
               "M"),
            SVI("(test):2:14: error: Too many arguments to function-like macro F()\n"
               "(test):3:1: ... expanded from here\n")},
        {"bad token paste", __LINE__,
            SVI("#define P(a,b) a##b\n"
               "P(+,-)"),
            SVI("(test):1:17: error: pasting \"+\" and \"-\" does not give a valid preprocessing token\n"
               "(test):2:1: ... expanded from here\n")},
        {"identifier pasted with backslash", __LINE__,
            SVI("#define C(a,b) a##b\nC(word,\\)"),
            SVI("(test):1:17: error: pasting \"word\" and \"\\\" does not give a valid preprocessing token\n"
               "(test):2:1: ... expanded from here\n")},
        {"number pasted with backslash", __LINE__,
            SVI("#define C(a,b) a##b\nC(1,\\)"),
            SVI("(test):1:17: error: pasting \"1\" and \"\\\" does not give a valid preprocessing token\n"
               "(test):2:1: ... expanded from here\n")},
        {"indirect too few args", __LINE__,
            SVI("#define F(x,y) x y\n"
               "#define M F(1)\n"
               "M"),
            SVI("(test):2:13: error: Too few arguments to function-like macro F()\n"
               "(test):3:1: ... expanded from here\n")},
        {"multi-level bad paste", __LINE__,
            SVI("#define P(a,b) a##b\n"
               "#define Q(a,b) P(a,b)\n"
               "#define R(a,b) Q(a,b)\n"
               "R(+,-)"),
            SVI("(test):1:17: error: pasting \"+\" and \"-\" does not give a valid preprocessing token\n"
               "(test):2:16: ... expanded from here\n"
               "(test):3:16: ... expanded from here\n"
               "(test):4:1: ... expanded from here\n")},
        {"error from pasted macro name", __LINE__,
            SVI("#define FF(x) x\n"
               "#define C(a,b) a##b\n"
               "C(F,F)(1,2)"),
            SVI("(test):3:9: error: Too many arguments to function-like macro FF()\n")},
        {
            "error from pasted macro name chained", __LINE__,
            SVI("#define FF(x) x\n"
               "#define C(a,b) a##b\n"
               "#define M C(F,F)(1,2)\n"
               "M"),
            SVI("(test):3:19: error: Too many arguments to function-like macro FF()\n"
               "(test):4:1: ... expanded from here\n"),
        },
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        int line = test_cases[i].line;
        StringView err_msg;
        int err = cpp_expand_string_expect_error(test_cases[i].inp, &err_msg);
        TestExpectFalse(int, err);
        if(err) continue;
        if(!test_expect_equals_sv(test_cases[i].exp, err_msg, "exp", "err_msg", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        if(err_msg.text) Allocator_free(MALLOCATOR, err_msg.text, err_msg.length);
    }
    TESTEND();
}

TestFunction(test_messages){
    TESTBEGIN();
    static const struct {
        const char* name; int line; StringView inp, exp;
    } test_cases[] = {
        {
            "#warning", __LINE__,
            SVI("#warning hello\n"),
            SVI("(test):1:2: warning: #warning hello\n"),
        },
        {"object redefinition uses new value", __LINE__,
            SVI("#define X 1\n#define X 2\n__print(X)\n"),
            SVI("(test):2:9: warning: Macro (X) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 2\n")},
        {"equal numeric values with different spellings", __LINE__,
            SVI("#define X 1U\n#define X 0x0001U\n__print(X)\n"),
            SVI("(test):2:9: warning: Macro (X) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 0x0001U\n")},
        {"object redefinition changes token count and kind", __LINE__,
            SVI("#define X 1\n#define X name + other\n__print(X)\n"),
            SVI("(test):2:9: warning: Macro (X) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: name + other\n")},
        {"object redefinition to empty", __LINE__,
            SVI("#define X 1\n#define X\n__print([X])\n"),
            SVI("(test):2:9: warning: Macro (X) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: []\n")},
        {"empty object redefinition to nonempty", __LINE__,
            SVI("#define X\n#define X 1\n__print(X)\n"),
            SVI("(test):2:9: warning: Macro (X) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 1\n")},
        {"object redefined as function", __LINE__,
            SVI("#define F 1\n#define F(x) x\n__print(F(2))\n"),
            SVI("(test):2:9: warning: Macro (F) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 2\n")},
        {"function redefined as object", __LINE__,
            SVI("#define F(x) x\n#define F 2\n__print(F)\n"),
            SVI("(test):2:9: warning: Macro (F) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 2\n")},
        {"function redefined as empty object", __LINE__,
            SVI("#define F(x) x\n#define F\n__print([F])\n"),
            SVI("(test):2:9: warning: Macro (F) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: []\n")},
        {"function redefinition changes body", __LINE__,
            SVI("#define F(a) a\n#define F(a) a + 1\n__print(F(2))\n"),
            SVI("(test):2:9: warning: Macro (F) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 2 + 1\n")},
        {"function redefinition changes parameter count", __LINE__,
            SVI("#define F(a, b) a\n#define F(a) a\n__print(F(2))\n"),
            SVI("(test):2:9: warning: Macro (F) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 2\n")},
        {"function redefinition changes parameter name", __LINE__,
            SVI("#define F(a) a\n#define F(b) b\n__print(F(2))\n"),
            SVI("(test):2:9: warning: Macro (F) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 2\n")},
        {"function redefinition changes variadic status", __LINE__,
            SVI("#define F(a) a\n#define F(a, ...) a __VA_ARGS__\n__print(F(2, + 1))\n"),
            SVI("(test):2:9: warning: Macro (F) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 2 + 1\n")},
        {"named varargs redefinition changes substitution", __LINE__,
            SVI("#define F(args...) args\n#define F(other...) args\n__print(F(2))\n"),
            SVI("(test):2:9: warning: Macro (F) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: args\n")},
        {"type builtin replacement can redefine again", __LINE__,
            SVI("#define __int64 1\n#define __int64 2\n__print(__int64)\n"),
            SVI("(test):1:9: warning: Macro (__int64) redefined with a different definition\n"
                "(test):0:0: info: ... previously defined here\n"
                "(test):2:9: warning: Macro (__int64) redefined with a different definition\n"
                "(test):1:9: info: ... previously defined here\n"
                "(test):3:1: 2\n")},
        {"identical definitions remain silent", __LINE__,
            SVI("#define X 1\n#define X 1\n#define E\n#define E\n"
                "#define F(a) a + 1\n#define F(a) a  +  1\n"),
            SVI("")},
        {
            "__print", __LINE__,
            SVI("__print(hello)\n"),
            SVI("(test):1:1: hello\n"),
        },
        {
            "__print packed embed", __LINE__,
            SVI("__print(_Embed(\"__FILE__ limit(8)\"))\n"),
            SVI("(test):1:1: 95,95,112,114,105,110,116,40\n"),
        },
        {
            "__PRINT", __LINE__,
            SVI("__PRINT__(hello)\n"),
            SVI("(test):1:1: hello\n"),
        },
        {
            "__where", __LINE__,
            SVI("#define FOO BAR\n"
               "__where(FOO)\n"),
            SVI("(test):1:9: info: FOO defined\n"),
        },
        {"__where multiline", __LINE__,
            SVI("#define FOO BAR\n__where(\nFOO\n)"),
            SVI("(test):1:9: info: FOO defined\n")},
        {
            "__WHERE__", __LINE__,
            SVI("#define FOO BAR\n"
               "__WHERE__(FOO)\n"),
            SVI("(test):1:9: info: FOO defined\n"),
        },
        {
            "__where - missing", __LINE__,
            SVI("#define FOO BAR\n"
               "__where(foo)\n"),
            SVI("(test):2:1: warning: __where__: 'foo' is not defined\n"),
        },
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        int line = test_cases[i].line;
        StringView err_msg;
        int err = cpp_expand_string_expect_message(test_cases[i].inp, &err_msg);
        TestExpectFalse(int, err);
        if(err) continue;
        if(!test_expect_equals_sv(test_cases[i].exp, err_msg, "exp", "msg", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        if(err_msg.text) Allocator_free(MALLOCATOR, err_msg.text, err_msg.length);
    }
    TESTEND();
}

TestFunction(test_condition){
    TESTBEGIN();
    static const struct {
        const char* name; int line; _Bool disabled; StringView inp, exp;
    } test_cases[] = {
        {"#if", __LINE__, 0,
            SVI("#if 1\n"
                    "Foo\n"
                    "#endif"),
            SVI("\nFoo\n")
        },
        {"unsigned arithmetic and conversions", __LINE__, 0,
            SVI("#if -1 < 1U || !(0xffffffffffffffff > 0)\n#error comparison\n#endif\n"
               "#if (0xffffffffffffffffU + 1) != 0 || (0U - 1) != 0xffffffffffffffff\n#error wrap\n#endif\n"
               "#if (0xffffffffffffffffU * 2) != 0xfffffffffffffffeU\n#error multiply\n#endif\n"
               "#if (0xffffffffffffffffU / 2) != 0x7fffffffffffffff || (0xffffffffffffffffU % 2) != 1\n#error division\n#endif\n"
               "#if (0xffffffffffffffffU >> 63) != 1 || (-2 >> 1U) != -1\n#error shift\n#endif\n"
               "#if -0x8000000000000000U != 0x8000000000000000U || -1U < 0\n#error negate\n#endif\n"
               "#if (1 ? -1 : 0U) < 0 || (0 ? 0U : -1) < 0\n#error ternary\n#endif\n"
               "#if (1 ? -1 : (1U / 0)) < 0 || (1 ? -1 : (0 ? 0 : 1U)) < 0\n#error skipped_type\n#endif\n"
               "#if -1 >= !0U || -1 >= (1U == 1) || -1 >= (1U && 1) || -1 >= (1U < 2)\n#error boolean_type\n#endif\n"
               "#if (1 ? -1 : (1U < 2)) >= 0 || (1 ? -1 : (1 << 1U)) >= 0\n#error skipped_signed_type\n#endif\n"
               "#if ~(0U) != 0xffffffffffffffffU || (+1uL - 2) < 0 || (1ui64 - 2) < 0\n#error suffix\n#endif\n"
               "ok"),
            SVI("\n\n\n" "\n\n\n" "\n\n\n" "\n\n\n" "\n\n\n" "\n\n\n"
               "\n\n\n" "\n\n\n" "\n\n\n" "\n\n\n" "\n\n\n" "ok")},
        {"ifdef (defined)", __LINE__, 0,
            SVI("#define FOO\n"
               "#ifdef FOO\n"
               "Foo\n"
               "#endif"),
            SVI("\n\nFoo\n")
        },
        {"ifdef (undefined)", __LINE__, 0,
            SVI("#define FOO\n"
               "#ifdef foo\n"
               "Foo\n"
               "#endif"),
            SVI("\n\n\n")
        },
        {"ifndef (undefined)", __LINE__, 0,
            SVI("#define FOO\n"
               "#ifndef foo\n"
               "Foo\n"
               "#endif"),
            SVI("\n\nFoo\n")
        },
        {"ifndef (defined)", __LINE__, 0,
            SVI("#define FOO\n"
               "#ifndef FOO\n"
               "Foo\n"
               "#endif // ifndef FOO"),
            SVI("\n\n\n")
        },
        // #ifdef / #else
        {"ifdef else (true branch)", __LINE__, 0,
            SVI("#define X\n"
               "#ifdef X\n"
               "yes\n"
               "#else\n"
               "no\n"
               "#endif"),
            SVI("\n\nyes\n\n\n")
        },
        {"ifdef else (false branch)", __LINE__, 0,
            SVI("#ifdef X\n"
               "yes\n"
               "#else\n"
               "no\n"
               "#endif"),
            SVI("\n\n\nno\n")
        },
        // #ifndef / #else
        {"ifndef else (true branch)", __LINE__, 0,
            SVI("#ifndef X\n"
               "yes\n"
               "#else\n"
               "no\n"
               "#endif"),
            SVI("\nyes\n\n\n")
        },
        {"ifndef else (false branch)", __LINE__, 0,
            SVI("#define X\n"
               "#ifndef X\n"
               "yes\n"
               "#else\n"
               "no\n"
               "#endif"),
            SVI("\n\n\n\nno\n")
        },
        // Nested conditionals
        {"nested ifdef both true", __LINE__, 0,
            SVI("#define A\n"
               "#define B\n"
               "#ifdef A\n"
               "#ifdef B\n"
               "both\n"
               "#endif\n"
               "#endif"),
            SVI("\n\n\n\nboth\n\n")
        },
        {"nested ifdef outer false", __LINE__, 0,
            SVI("#define B\n"
               "#ifdef A\n"
               "#ifdef B\n"
               "inner\n"
               "#endif\n"
               "#endif"),
            SVI("\n\n\n\n\n")
        },
        {"nested ifdef inner false", __LINE__, 0,
            SVI("#define A\n"
               "#ifdef A\n"
               "#ifdef B\n"
               "inner\n"
               "#else\n"
               "else_inner\n"
               "#endif\n"
               "after_inner\n"
               "#endif"),
            SVI("\n\n\n\n\nelse_inner\n\nafter_inner\n")
        },
        // No macro expansion in false branches
        {"no expand in false", __LINE__, 0,
            SVI("#define X error_if_expanded\n"
               "#ifdef NOPE\n"
               "X\n"
               "#endif"),
            SVI("\n\n\n")
        },
        // #define inside conditional branches
        {"define in true branch", __LINE__, 0,
            SVI("#ifdef NOPE\n"
               "#define X bad\n"
               "#else\n"
               "#define X good\n"
               "#endif\n"
               "X"),
            SVI("\n\n\n\n\ngood")
        },
        {"define in false branch ignored", __LINE__, 0,
            SVI("#define A\n"
               "#ifdef A\n"
               "#define X yes\n"
               "#else\n"
               "#define X no\n"
               "#endif\n"
               "X"),
            SVI("\n\n\n\n\n\nyes")
        },
        // #undef inside conditional
        {"undef in true branch", __LINE__, 0,
            SVI("#define X hello\n"
               "#ifdef X\n"
               "#undef X\n"
               "#endif\n"
               "#ifdef X\n"
               "still_defined\n"
               "#else\n"
               "undefined\n"
               "#endif"),
            SVI("\n\n\n\n\n\n\nundefined\n")
        },
        // #elifdef
        {"elifdef first true", __LINE__, 0,
            SVI("#define A A\n"
               "#define B B\n"
               "#ifdef A\n"
               "A\n"
               "#elifdef B\n"
               "B\n"
               "#endif"),
            SVI("\n\n\nA\n\n\n")
        },
        {"elifdef second true", __LINE__, 0,
            SVI("#define B B\n"
               "#ifdef A\n"
               "A\n"
               "#elifdef B\n"
               "B\n"
               "#endif"),
            SVI("\n\n\n\nB\n")
        },
        {"elifdef neither", __LINE__, 0,
            SVI("#ifdef A\n"
               "A\n"
               "#elifdef B\n"
               "B\n"
               "#else\n"
               "neither\n"
               "#endif"),
            SVI("\n\n\n\n\nneither\n")
        },
        // #elifndef
        {"elifndef activates", __LINE__, 0,
            SVI("#define A A\n"
               "#ifdef A\n"
               "#ifdef NOPE\n"
               "nope\n"
               "#elifndef ALSO_NOPE\n"
               "yes\n"
               "#endif\n"
               "#endif"),
            SVI("\n\n\n\n\nyes\n\n")
        },
        {"elifndef skipped when true_taken", __LINE__, 0,
            SVI("#define A A\n"
               "#ifdef A\n"
               "A\n"
               "#elifndef WHATEVER\n"
               "no\n"
               "#endif"),
            SVI("\n\nA\n\n\n")
        },
        // Deeply nested in false branch
        {"deep nesting in false branch", __LINE__, 0,
            SVI("#ifdef NOPE\n"
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
            SVI("\n\n\n\n\n\n\n\n\nouter_else\n")
        },
        // Empty branches
        {"empty true branch", __LINE__, 0,
            SVI("#define X X\n"
               "#ifdef X\n"
               "#endif"),
            SVI("\n\n")
        },
        {"empty false branch", __LINE__, 0,
            SVI("#ifdef X\n"
               "#else\n"
               "#endif"),
            SVI("\n\n")
        },
        // Multiple elif chain
        {"elifdef chain", __LINE__, 0,
            SVI("#define C C\n"
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
            SVI("\n\n\n\n\n\nC\n\n\n\n\n")
        },
        // Include guard pattern
        {"include guard pattern", __LINE__, 0,
            SVI("#ifndef GUARD_H\n"
               "#define GUARD_H\n"
               "content\n"
               "#endif"),
            SVI("\n\ncontent\n")
        },
        // Only first matching elifdef activates
        {"elifdef only first match", __LINE__, 0,
            SVI("#define B B\n"
               "#define C C\n"
               "#ifdef A\n"
               "A\n"
               "#elifdef B\n"
               "B\n"
               "#elifdef C\n"
               "C\n"
               "#endif"),
            SVI("\n\n\n\n\nB\n\n\n")
        },
        // elifndef direct (not nested)
        {"elifndef direct", __LINE__, 0,
            SVI("#ifdef NOPE\n"
               "nope\n"
               "#elifndef ALSO_NOPE\n"
               "yes\n"
               "#endif"),
            SVI("\n\n\nyes\n")
        },
        // Macro expansion resumes after #endif
        {"expansion after endif", __LINE__, 0,
            SVI("#define X hello\n"
               "#ifdef NOPE\n"
               "skipped\n"
               "#endif\n"
               "X"),
            SVI("\n\n\n\nhello")
        },
        // Null directive (# on a line by itself) in false branch
        {"null directive in false branch", __LINE__, 0,
            SVI("#ifdef NOPE\n"
               "#\n"
               "#endif"),
            SVI("\n\n")
        },
        // Adjacent independent conditionals
        {"adjacent conditionals", __LINE__, 0,
            SVI("#define A\n"
               "#ifdef A\n"
               "first\n"
               "#endif\n"
               "#ifdef B\n"
               "second\n"
               "#else\n"
               "not_second\n"
               "#endif"),
            SVI("\n\nfirst\n\n\n\n\nnot_second\n")
        },
        {
            "Weird c23 pseudo macros", __LINE__, 0,
            SVI( "#ifdef __has_include\n"
                "__has_include\n"
                "#endif\n"
                "#ifdef __has_embed\n"
                "__has_embed\n"
                "#endif\n"
                "#ifdef __has_c_attribute\n"
                "__has_c_attribute\n"
                "#endif\n"),
            SVI("\n__has_include\n\n\n__has_embed\n\n\n__has_c_attribute\n\n")
        },
        {
            "__has_include", __LINE__, SKIP,
            SVI("#if __has_include(__FILE__)\n"
               "yes\n"
               "#endif"),
            SVI("\nyes\n"),
        },
        // Skipped as we should capture the output, this is wrong test
        {"warning directive", __LINE__, SKIP,
            SVI("#warning test warning\nafter"),
            SVI("\nafter")},
        {"error in false branch", __LINE__, 0,
            SVI("#if 0\n#error should not fire\n#endif\nok"),
            SVI("\n\n\nok")},
        {"line directive", __LINE__, 0,
            SVI("#line 100\nvalue"),
            SVI("\nvalue")},
        {"line directive with file", __LINE__, 0,
            SVI("#line 100 \"fake.c\"\nvalue"),
            SVI("\nvalue")},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        if(test_cases[i].disabled) continue;
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(int, err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "exp", "result", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}
TestFunction(test_erroneous_condition){
    TESTBEGIN();
    static const struct {
        const char* name; int line; StringView inp, exp;
    } test_cases[] = {
        {"defblock duplicate parameters", __LINE__,
            SVI("#defblock F(x, x)\nx\n#endblock\n"),
            SVI("(test):1:16: error: Duplicate macro param name\n")},
        {"nested va opt in unused macro", __LINE__,
            SVI("#define F(...) __VA_OPT__(__VA_OPT__(x))\n"),
            SVI("(test):1:27: error: Nested __VA_OPT__ is not allowed\n")},
        {"va opt in object macro", __LINE__,
            SVI("#define X __VA_OPT__(x)\n"),
            SVI("(test):1:11: error: __VA_OPT__ requires a variadic macro\n")},
        {"va opt without parentheses", __LINE__,
            SVI("#define F(...) __VA_OPT__ x\n"),
            SVI("(test):1:16: error: Expected '(' after __VA_OPT__\n")},
        {"unterminated va opt in unused macro", __LINE__,
            SVI("#define F(...) __VA_OPT__(x\n"),
            SVI("(test):1:16: error: Unterminated __VA_OPT__\n")},
        {"va opt in nonvariadic macro", __LINE__,
            SVI("#define F(x) __VA_OPT__(x)\n"),
            SVI("(test):1:14: error: __VA_OPT__ requires a variadic macro\n")},
        {"empty has include header", __LINE__,
            SVI("#if __has_include(\"\")\n#endif"),
            SVI("(test):1:5: error: Empty header name in __has_include\n")},
        {"empty angle has include header", __LINE__,
            SVI("#if __has_include(<>)\n#endif"),
            SVI("(test):1:5: error: Empty header name in __has_include\n")},
        {"empty expanded quoted has include header", __LINE__,
            SVI("#define H \"\"\n#if __has_include(H)\n#endif"),
            SVI("(test):2:5: error: Empty header name in __has_include\n")},
        {"empty expanded has include header", __LINE__,
            SVI("#define H <>\n#if __has_include(H)\n#endif"),
            SVI("(test):2:5: error: Empty header name in __has_include\n")},
        {"defblock duplicate named variadic parameter", __LINE__,
            SVI("#defblock F(x, x...)\nx\n#endblock\n"),
            SVI("(test):1:16: error: Duplicate macro param name\n")},
        {"let duplicate parameters", __LINE__,
            SVI("__let(F(x,x), x, F(1,2))"),
            SVI("(test):1:11: error: Duplicate macro param name\n")},
        {"let function signature trailing tokens", __LINE__,
            SVI("__let(F() junk, 1, F())"),
            SVI("(test):1:11: error: Unexpected token after __let macro signature\n")},
        {"let object signature trailing tokens", __LINE__,
            SVI("__let(X junk, 1, X)"),
            SVI("(test):1:9: error: Unexpected token after __let macro signature\n")},
        {"get multiple identifiers with unknown first key", __LINE__,
            SVI("__set(key, 7) __get(missing key)"),
            SVI("(test):1:29: error: Only one identifier as key to __get\n")},
        {"format empty integer argument", __LINE__,
            SVI("__format(\"%d%d\", , 2)"),
            SVI("(test):1:1: error: Missing integer argument to format\n")},
        {"format string rejects punctuation", __LINE__,
            SVI("__format(\"%s\", name + 1)"),
            SVI("(test):1:21: error: Invalid arg to format (expected string, identifier, or pp-number)\n")},
        {"format trailing minus", __LINE__,
            SVI("__format(\"%d\", 1-)"),
            SVI("(test):0:0: error: Unexpected end of #if expression\n")},
        {"format postfix increment", __LINE__,
            SVI("__format(\"%d\", 1++)"),
            SVI("(test):1:17: error: Unexpected token after #if expression\n")},
        {"format invalid octal", __LINE__,
            SVI("__format(\"%d\", 08)"),
            SVI("(test):1:16: error: Invalid octal digit in number\n")},
        {"expanded has include trailing tokens", __LINE__,
            SVI("#define H \"missing.h\" junk\n#if __has_include(H)\n#endif"),
            SVI("(test):2:5: error: Unexpected token after header name in __has_include\n")},
        {"expanded angle has include trailing tokens", __LINE__,
            SVI("#define H <missing.h> junk\n#if __has_include(H)\n#endif"),
            SVI("(test):2:5: error: Unexpected token after header name in __has_include\n")},
        {"duplicate unsigned suffix", __LINE__,
            SVI("#if 1uU\n#endif"),
            SVI("(test):1:5: error: Invalid integer suffix\n")},
        {"interleaved long suffix", __LINE__,
            SVI("#if 1lUl\n#endif"),
            SVI("(test):1:5: error: Invalid integer suffix\n")},
        {"separator before suffix", __LINE__,
            SVI("#if 1'u\n#endif"),
            SVI("(test):1:5: error: Invalid digit separator in number\n")},
        {"separator after hex prefix", __LINE__,
            SVI("#if 0x'ff\n#endif"),
            SVI("(test):1:5: error: Invalid digit separator in number\n")},
        {"defblock leading paste", __LINE__,
            SVI("#defblock F(x)\n## x\n#endblock\n"),
            SVI("(test):2:1: error: '##' cannot appear at start of replacement list\n")},
        {"defblock trailing paste", __LINE__,
            SVI("#defblock F\nx ##\n#endblock\n"),
            SVI("(test):2:3: error: '##' cannot appear at end of replacement list\n")},
        {"unused macro invalid stringification", __LINE__,
            SVI("#define F(x) #other\n"),
            SVI("(test):1:14: error: '#' is not followed by a macro parameter\n")},
        {"unused defblock invalid stringification", __LINE__,
            SVI("#defblock F(x)\n#other\n#endblock\n"),
            SVI("(test):2:1: error: '#' is not followed by a macro parameter\n")},
        {"unused let invalid stringification", __LINE__,
            SVI("__let(F(x), #other, unused)"),
            SVI("(test):1:13: error: '#' is not followed by a macro parameter\n")},
        {"unused let leading paste", __LINE__,
            SVI("__let(F(x), ## x, unused)"),
            SVI("(test):1:13: error: '##' cannot appear at start of replacement list\n")},
        {"unterminated active if", __LINE__,
            SVI("#if 1\nyes"), SVI("(test):1:2: error: Unterminated conditional directive\n")},
        {"unterminated active if at eof", __LINE__,
            SVI("#if 1"), SVI("(test):1:2: error: Unterminated conditional directive\n")},
        {"unterminated active else", __LINE__,
            SVI("#if 0\n#else\nyes\n"), SVI("(test):1:2: error: Unterminated conditional directive\n")},
        {"unterminated outer active if", __LINE__,
            SVI("#if 1\n#if 0\n#endif\n"), SVI("(test):1:2: error: Unterminated conditional directive\n")},
        {"endif without if", __LINE__,
            SVI("#endif"), SVI("(test):1:2: error: #endif outside of #if (or similar construct)\n")},
        {"else without if", __LINE__,
            SVI("#else"), SVI("(test):1:2: error: #else outside of #if (or similar construct)\n")},
        {"elif without if", __LINE__,
            SVI("#elif 1"), SVI("(test):1:2: error: #elif outside of #if (or similar construct)\n")},
        {"elifdef without if", __LINE__,
            SVI("#elifdef X"), SVI("(test):1:2: error: #elifdef outside of #if (or similar construct)\n")},
        {"elifndef without if", __LINE__,
            SVI("#elifndef X"), SVI("(test):1:2: error: #elifndef outside of #if (or similar construct)\n")},
        {"duplicate else", __LINE__,
            SVI("#ifdef X\n"
               "#else\n"
               "#else\n"
               "#endif"),
            SVI("(test):3:2: error: another #else\n")},
        {"elif after else", __LINE__,
            SVI("#ifdef X\n"
               "#else\n"
               "#elif 1\n"
               "#endif"),
            SVI("(test):3:2: error: #elif after #else\n")},
        {"elifdef after else", __LINE__,
            SVI("#define Y\n"
               "#ifdef X\n"
               "#else\n"
               "#elifdef Y\n"
               "#endif"),
            SVI("(test):4:2: error: #elifdef after #else\n")},
        {"elifndef after else", __LINE__,
            SVI("#define Y\n"
               "#ifdef X\n"
               "#else\n"
               "#elifndef Y\n"
               "#endif"),
            SVI("(test):4:2: error: #elifndef after #else\n")},
        {"duplicate else (active branch)", __LINE__,
            SVI("#define X\n"
               "#ifdef X\n"
               "#else\n"
               "#else\n"
               "#endif"),
            SVI("(test):4:2: error: another #else\n")},
        {"elif after else (active branch)", __LINE__,
            SVI("#define X\n"
               "#ifndef X\n"
               "skip\n"
               "#else\n"
               "#elif 1\n"
               "#endif"),
            SVI("(test):5:2: error: #elif after #else\n")},
        {"unterminated if", __LINE__,
            SVI("#ifdef X\n"
               "stuff"),
            SVI("(test):1:2: error: Unterminated conditional directive\n")},
        {"unterminated nested if", __LINE__,
            SVI("#define A\n"
               "#ifdef A\n"
               "#ifdef B\n"
               "stuff"),
            SVI("(test):3:2: error: Unterminated conditional directive\n")},
        {"if eval bad msvc suffix i7", __LINE__,
            SVI("#if 42i7\n#endif"),
            SVI("(test):1:5: error: Invalid digit in number\n")},
        {"decimal trailing apostrophe", __LINE__,
            SVI("#if 1'\n#endif"),
            SVI("(test):1:6: error: Unterminated character literal\n")},
        {"apostrophe before arithmetic", __LINE__,
            SVI("#if 1' + 2\n#endif"),
            SVI("(test):1:6: error: Unterminated character literal\n")},
        {"hex trailing apostrophe", __LINE__,
            SVI("#if 0x1'\n#endif"),
            SVI("(test):1:8: error: Unterminated character literal\n")},
        {"if eval bad msvc suffix bare i", __LINE__,
            SVI("#if 42i\n#endif"),
            SVI("(test):1:5: error: Invalid digit in number\n")},
        {"char constant too long", __LINE__,
            SVI("#if 'ABCDE'\n#endif"),
            SVI("(test):1:5: error: Character constant too long\n")},
        // Overflow / UB checks
        {"if trailing number", __LINE__,
            SVI("#if 1 2\n#endif"),
            SVI("(test):1:7: error: Unexpected token after #if expression\n")},
        {"if unmatched closing paren", __LINE__,
            SVI("#if 1)\n#endif"),
            SVI("(test):1:6: error: Unexpected token after #if expression\n")},
        {"short circuit still checks syntax", __LINE__,
            SVI("#if 1 || (2 + )\n#endif"),
            SVI("(test):1:15: error: Unexpected punctuator in #if expression\n")},
        {"short circuit still evaluates selected operand", __LINE__,
            SVI("#if 0 || 1 / 0\n#endif"),
            SVI("(test):1:12: error: Division by zero in #if\n")},
        {"ternary still evaluates selected arm", __LINE__,
            SVI("#if 1 ? 1 / 0 : 1\n#endif"),
            SVI("(test):1:11: error: Division by zero in #if\n")},
        {"add overflow", __LINE__,
            SVI("#if 9223372036854775807 + 1\n#endif"),
            SVI("(test):1:25: error: overflow in add in #if\n")},
        {"sub overflow", __LINE__,
            SVI("#if -9223372036854775807 - 2\n#endif"),
            SVI("(test):1:26: error: overflow in sub in #if\n")},
        {"mul overflow", __LINE__,
            SVI("#if 9223372036854775807 * 2\n#endif"),
            SVI("(test):1:25: error: overflow in mul in #if\n")},
        {"div overflow", __LINE__,
            SVI("#if (-9223372036854775807 - 1) / -1\n#endif"),
            SVI("(test):1:32: error: overflow in div in #if\n")},
        {"div by zero", __LINE__,
            SVI("#if 1 / 0\n#endif"),
            SVI("(test):1:7: error: Division by zero in #if\n")},
        {"mod overflow", __LINE__,
            SVI("#if (-9223372036854775807 - 1) % -1\n#endif"),
            SVI("(test):1:32: error: overflow in mod in #if\n")},
        {"mod by zero", __LINE__,
            SVI("#if 1 % 0\n#endif"),
            SVI("(test):1:7: error: Modulo by zero in #if\n")},
        {"lshift negative amount", __LINE__,
            SVI("#if 1 << -1\n#endif"),
            SVI("(test):1:7: error: shift amount out of range in #if\n")},
        {"lshift too large", __LINE__,
            SVI("#if 1 << 64\n#endif"),
            SVI("(test):1:7: error: shift amount out of range in #if\n")},
        {"rshift negative amount", __LINE__,
            SVI("#if 1 >> -1\n#endif"),
            SVI("(test):1:7: error: shift amount out of range in #if\n")},
        {"rshift too large", __LINE__,
            SVI("#if 1 >> 64\n#endif"),
            SVI("(test):1:7: error: shift amount out of range in #if\n")},
        {"negate INT64_MIN", __LINE__,
            SVI("#if -(-9223372036854775807 - 1)\n#endif"),
            SVI("(test):1:5: error: overflow in negation in #if\n")},
        {"error directive", __LINE__,
            SVI("#error this is an error message"),
            SVI("(test):1:2: error: #error this is an error message\n")},
        {"error directive empty", __LINE__,
            SVI("#error\n"),
            SVI("(test):1:2: error: #error \n")},
        {"named variadic parameter duplicates fixed parameter", __LINE__,
            SVI("#define F(x, x...) x\n"),
            SVI("(test):1:14: error: Duplicate macro param name\n")},
        {"duplicate fixed macro parameters", __LINE__,
            SVI("#define F(x, x) x\n"),
            SVI("(test):1:14: error: Duplicate macro param name\n")},
        {"named variadic parameter duplicates later fixed parameter", __LINE__,
            SVI("#define F(a, b, b...) b\n"),
            SVI("(test):1:17: error: Duplicate macro param name\n")},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        int line = test_cases[i].line;
        StringView err_msg;
        int err = cpp_expand_string_expect_error(test_cases[i].inp, &err_msg);
        TestExpectFalse(int, err);
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
    static const struct {
        const char* name; int line; _Bool disabled; StringView inp, exp;
    } test_cases[] = {
        {"if 0", __LINE__, 0,
            SVI("#if 0\nyes\n#endif"), SVI("\n\n")},
        {"if 1", __LINE__, 0,
            SVI("#if 1\nyes\n#endif"), SVI("\nyes\n")},
        {"if 42", __LINE__, 0,
            SVI("#if 42\nyes\n#endif"), SVI("\nyes\n")},
        {"comment_before_ifdef_name", __LINE__, 0,
            SVI("#define X\n#ifdef /* comment */ X\nyes\n#endif"), SVI("\n\nyes\n")},
        {"comment_before_ifndef_name", __LINE__, 0,
            SVI("#ifndef /* comment */ X\nyes\n#endif"), SVI("\nyes\n")},
        {"comment_before_undef_name", __LINE__, 0,
            SVI("#define X\n#undef /* comment */ X\n#ifndef X\nyes\n#endif"), SVI("\n\n\nyes\n")},
        {"comment_before_active_elifdef_name", __LINE__, 0,
            SVI("#if 1\nyes\n#elifdef /* comment */ X\nno\n#endif"), SVI("\nyes\n\n\n")},
        {"comment_before_inactive_elifdef_name", __LINE__, 0,
            SVI("#define X\n#if 0\nno\n#elifdef /* comment */ X\nyes\n#endif"), SVI("\n\n\n\nyes\n")},
        {"comment_before_active_elifndef_name", __LINE__, 0,
            SVI("#if 1\nyes\n#elifndef /* comment */ X\nno\n#endif"), SVI("\nyes\n\n\n")},
        {"comment_before_inactive_elifndef_name", __LINE__, 0,
            SVI("#if 0\nno\n#elifndef /* comment */ X\nyes\n#endif"), SVI("\n\n\nyes\n")},
        {"zero_parameter_condition_call_with_space", __LINE__, 0,
            SVI("#define F() 1\n#if F( )\nyes\n#endif"), SVI("\n\nyes\n")},
        {"decimal digit separators", __LINE__, 0,
            SVI("#if 1'000 == 1000\nyes\n#endif"), SVI("\nyes\n")},
        {"hex digit separators and unsigned suffix", __LINE__, 0,
            SVI("#if 0xffff'ffff'ffff'ffffU == ~0U\nyes\n#endif"), SVI("\nyes\n")},
        {"binary digit separators", __LINE__, 0,
            SVI("#if 0b1010'0101 == 165\nyes\n#endif"), SVI("\nyes\n")},
        {"octal digit separators", __LINE__, 0,
            SVI("#if 0'777 == 511\nyes\n#endif"), SVI("\nyes\n")},
        {"prefixed octal", __LINE__, 0,
            SVI("#if 0o755 == 493 && 0O7'55ULL == 493\nyes\n#endif"), SVI("\nyes\n")},
        {"delimited escapes", __LINE__, 0,
            SVI("#if '\\o{123}' == 83 && '\\x{41}' == 65 && U'\\u{1F600}' == 0x1F600\nyes\n#endif"), SVI("\nyes\n")},
        {"hex 0x10", __LINE__, 0,
            SVI("#if 0x10 == 16\nyes\n#endif"), SVI("\nyes\n")},
        {"hex leading zeroes", __LINE__, 0,
            SVI("#if 0x000000000000000000000000000000001 == 1\nyes\n#endif"), SVI("\nyes\n")},
        {"octal leading zeroes", __LINE__, 0,
            SVI("#if 000000000000000000000000000000001 == 1\nyes\n#endif"), SVI("\nyes\n")},
        {"binary leading zeroes", __LINE__, 0,
            SVI("#if 0b000000000000000000000000000000000"
               "0000000000000000000000000000000001 == 1\nyes\n#endif"), SVI("\nyes\n")},
        {"hex 0xFF", __LINE__, 0,
            SVI("#if 0xFF == 255\nyes\n#endif"), SVI("\nyes\n")},
        {"hex 0XA", __LINE__, 0,
            SVI("#if 0XA == 10\nyes\n#endif"), SVI("\nyes\n")},
        {"octal 077", __LINE__, 0,
            SVI("#if 077 == 63\nyes\n#endif"), SVI("\nyes\n")},
        {"octal 010", __LINE__, 0,
            SVI("#if 010 == 8\nyes\n#endif"), SVI("\nyes\n")},
        {"binary 0b101", __LINE__, 0,
            SVI("#if 0b101 == 5\nyes\n#endif"), SVI("\nyes\n")},
        {"binary 0B1111", __LINE__, 0,
            SVI("#if 0B1111 == 15\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix U", __LINE__, 0,
            SVI("#if 42U == 42\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix L", __LINE__, 0,
            SVI("#if 42L == 42\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix LL", __LINE__, 0,
            SVI("#if 42LL == 42\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix ULL", __LINE__, 0,
            SVI("#if 42ULL == 42\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix uLL", __LINE__, 0,
            SVI("#if 42uLL == 42\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix LLU", __LINE__, 0,
            SVI("#if 100LLU == 100\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix i128", __LINE__, 0,
            SVI("#if 42i128 == 42ui128 && 1lll == 1ULLL && 2LLLU == 2I128\nYES\n#endif"), SVI("\nYES\n")},
        {"suffix i8", __LINE__, 0,
            SVI("#if 42i8 == 42\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix i16", __LINE__, 0,
            SVI("#if 1000i16 == 1000\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix i32", __LINE__, 0,
            SVI("#if 100000i32 == 100000\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix i64", __LINE__, 0,
            SVI("#if 9223372036854775807i64 == 9223372036854775807\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix ui8", __LINE__, 0,
            SVI("#if 255ui8 == 255\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix ui16", __LINE__, 0,
            SVI("#if 65535ui16 == 65535\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix ui32", __LINE__, 0,
            SVI("#if 0xFFFFFFFFui32 == 0xFFFFFFFF\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix ui64", __LINE__, 0,
            SVI("#if 0xFFFFFFFFFFFFFFFFui64 == 0xFFFFFFFFFFFFFFFF\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix hex i32", __LINE__, 0,
            SVI("#if 0xDeAdBeEfi32 == 0xDeAdBeEf\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix octal i32", __LINE__, 0,
            SVI("#if 077i32 == 63\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix I64 uppercase", __LINE__, 0,
            SVI("#if 42I64 == 42\nyes\n#endif"), SVI("\nyes\n")},
        {"suffix Ui32 mixed case", __LINE__, 0,
            SVI("#if 42Ui32 == 42\nyes\n#endif"), SVI("\nyes\n")},
        {"char 'A'", __LINE__, 0,
            SVI("#if 'A' == 65\nyes\n#endif"), SVI("\nyes\n")},
        {"char '0'", __LINE__, 0,
            SVI("#if '0' == 48\nyes\n#endif"), SVI("\nyes\n")},
        {"char '\\n'", __LINE__, 0,
            SVI("#if '\\n' == 10\nyes\n#endif"), SVI("\nyes\n")},
        {"char '\\t'", __LINE__, 0,
            SVI("#if '\\t' == 9\nyes\n#endif"), SVI("\nyes\n")},
        {"char '\\0'", __LINE__, 0,
            SVI("#if '\\0' == 0\nyes\n#endif"), SVI("\nyes\n")},
        {"char '\\x41'", __LINE__, 0,
            SVI("#if '\\x41' == 65\nyes\n#endif"), SVI("\nyes\n")},
        {"char '\\\\' backslash", __LINE__, 0,
            SVI("#if '\\\\' == 92\nyes\n#endif"), SVI("\nyes\n")},
        {"char '\\'' escaped quote", __LINE__, 0,
            SVI("#if '\\'' == 39\nyes\n#endif"), SVI("\nyes\n")},
        {"char L'A'", __LINE__, 0,
            SVI("#if L'A' == 65\nyes\n#endif"), SVI("\nyes\n")},
        {"char u'A'", __LINE__, 0,
            SVI("#if u'A' == 65\nyes\n#endif"), SVI("\nyes\n")},
        {"char U'A'", __LINE__, 0,
            SVI("#if U'A' == 65\nyes\n#endif"), SVI("\nyes\n")},
        {"char u8'A'", __LINE__, 0,
            SVI("#if u8'A' == 65\nyes\n#endif"), SVI("\nyes\n")},
        {"char '\\u0041'", __LINE__, 0,
            SVI("#if '\\u0041' == 65\nyes\n#endif"), SVI("\nyes\n")},
        {"char '\\U00000041'", __LINE__, 0,
            SVI("#if '\\U00000041' == 65\nyes\n#endif"), SVI("\nyes\n")},
        {"unary !", __LINE__, 0,
            SVI("#if !0\nyes\n#endif"), SVI("\nyes\n")},
        {"unary ! truthy", __LINE__, 0,
            SVI("#if !1\nyes\n#else\nno\n#endif"), SVI("\n\n\nno\n")},
        {"unary ! nonzero", __LINE__, 0,
            SVI("#if !42\nyes\n#else\nno\n#endif"), SVI("\n\n\nno\n")},
        {"unary ~", __LINE__, 0,
            SVI("#if (~0 & 0xFF) == 0xFF\nyes\n#endif"), SVI("\nyes\n")},
        {"unary +", __LINE__, 0,
            SVI("#if +1 == 1\nyes\n#endif"), SVI("\nyes\n")},
        {"unary -", __LINE__, 0,
            SVI("#if -1 < 0\nyes\n#endif"), SVI("\nyes\n")},
        {"unary - value", __LINE__, 0,
            SVI("#if -5 + 5 == 0\nyes\n#endif"), SVI("\nyes\n")},
        {"add", __LINE__, 0,
            SVI("#if 1 + 2 == 3\nyes\n#endif"), SVI("\nyes\n")},
        {"sub", __LINE__, 0,
            SVI("#if 10 - 3 == 7\nyes\n#endif"), SVI("\nyes\n")},
        {"mul", __LINE__, 0,
            SVI("#if 6 * 7 == 42\nyes\n#endif"), SVI("\nyes\n")},
        {"div", __LINE__, 0,
            SVI("#if 42 / 6 == 7\nyes\n#endif"), SVI("\nyes\n")},
        {"mod", __LINE__, 0,
            SVI("#if 17 % 5 == 2\nyes\n#endif"), SVI("\nyes\n")},
        {"left shift", __LINE__, 0,
            SVI("#if 1 << 4 == 16\nyes\n#endif"), SVI("\nyes\n")},
        {"right shift", __LINE__, 0,
            SVI("#if 256 >> 4 == 16\nyes\n#endif"), SVI("\nyes\n")},
        {"less than true", __LINE__, 0,
            SVI("#if 1 < 2\nyes\n#endif"), SVI("\nyes\n")},
        {"less than false", __LINE__, 0,
            SVI("#if 2 < 1\nyes\n#else\nno\n#endif"), SVI("\n\n\nno\n")},
        {"greater than", __LINE__, 0,
            SVI("#if 3 > 2\nyes\n#endif"), SVI("\nyes\n")},
        {"less equal", __LINE__, 0,
            SVI("#if 2 <= 2\nyes\n#endif"), SVI("\nyes\n")},
        {"greater equal", __LINE__, 0,
            SVI("#if 3 >= 3\nyes\n#endif"), SVI("\nyes\n")},
        {"equal true", __LINE__, 0,
            SVI("#if 5 == 5\nyes\n#endif"), SVI("\nyes\n")},
        {"equal false", __LINE__, 0,
            SVI("#if 5 == 6\nyes\n#else\nno\n#endif"), SVI("\n\n\nno\n")},
        {"not equal true", __LINE__, 0,
            SVI("#if 5 != 6\nyes\n#endif"), SVI("\nyes\n")},
        {"not equal false", __LINE__, 0,
            SVI("#if 5 != 5\nyes\n#else\nno\n#endif"), SVI("\n\n\nno\n")},
        {"bitwise and", __LINE__, 0,
            SVI("#if (0xF0 & 0x0F) == 0\nyes\n#endif"), SVI("\nyes\n")},
        {"bitwise and 2", __LINE__, 0,
            SVI("#if (0xFF & 0x0F) == 0x0F\nyes\n#endif"), SVI("\nyes\n")},
        {"bitwise or", __LINE__, 0,
            SVI("#if (0xF0 | 0x0F) == 0xFF\nyes\n#endif"), SVI("\nyes\n")},
        {"bitwise xor", __LINE__, 0,
            SVI("#if (0xFF ^ 0x0F) == 0xF0\nyes\n#endif"), SVI("\nyes\n")},
        {"logical and tt", __LINE__, 0,
            SVI("#if 1 && 1\nyes\n#endif"), SVI("\nyes\n")},
        {"logical and tf", __LINE__, 0,
            SVI("#if 1 && 0\nyes\n#else\nno\n#endif"), SVI("\n\n\nno\n")},
        {"logical and ft", __LINE__, 0,
            SVI("#if 0 && 1\nyes\n#else\nno\n#endif"), SVI("\n\n\nno\n")},
        {"logical or ff", __LINE__, 0,
            SVI("#if 0 || 0\nyes\n#else\nno\n#endif"), SVI("\n\n\nno\n")},
        {"logical or tf", __LINE__, 0,
            SVI("#if 1 || 0\nyes\n#endif"), SVI("\nyes\n")},
        {"logical or ft", __LINE__, 0,
            SVI("#if 0 || 1\nyes\n#endif"), SVI("\nyes\n")},
        {"ternary true", __LINE__, 0,
            SVI("#if (1 ? 2 : 3) == 2\nyes\n#endif"), SVI("\nyes\n")},
        {"short circuit and", __LINE__, 0,
            SVI("#if 0 && 1 / 0\nno\n#else\nyes\n#endif"), SVI("\n\n\nyes\n")},
        {"short circuit or", __LINE__, 0,
            SVI("#if 1 || 1 % 0\nyes\n#endif"), SVI("\nyes\n")},
        {"standalone cpp leaves compiler queries undefined", __LINE__, 0,
            SVI("#if defined(__has_builtin) || defined(__has_feature) || defined(__has_extension)\n#error compiler query defined\n#endif\n"
               "__has_feature(c_atomic) __has_extension(c_atomic)"),
            SVI("\n\n\n__has_feature(c_atomic) __has_extension(c_atomic)")},
        {"short circuit ternary true", __LINE__, 0,
            SVI("#if 1 ? 1 : 1 / 0\nyes\n#endif"), SVI("\nyes\n")},
        {"short circuit ternary false", __LINE__, 0,
            SVI("#if 0 ? 1 / 0 : 1\nyes\n#endif"), SVI("\nyes\n")},
        {"short circuit nested operations", __LINE__, 0,
            SVI("#if 1 || ((1 << -1) + (1 >> 64) + (9223372036854775807 + 1) + (-9223372036854775807 - 2) + (9223372036854775807 * 2) + -(-9223372036854775807 - 1))\nyes\n#endif"), SVI("\nyes\n")},
        {"short circuit nested ternary", __LINE__, 0,
            SVI("#if 0 ? (1 ? 1 / 0 : 2 % 0) : 1\nyes\n#endif"), SVI("\nyes\n")},
        {"short circuit resumes evaluation", __LINE__, 0,
            SVI("#if (0 && 1 / 0) || 2 == 2\nyes\n#endif"), SVI("\nyes\n")},
        {"ternary false", __LINE__, 0,
            SVI("#if (0 ? 2 : 3) == 3\nyes\n#endif"), SVI("\nyes\n")},
        {"ternary nested", __LINE__, 0,
            SVI("#if (1 ? 1 ? 10 : 20 : 30) == 10\nyes\n#endif"), SVI("\nyes\n")},
        {"ternary nested false", __LINE__, 0,
            SVI("#if (0 ? 10 : 1 ? 20 : 30) == 20\nyes\n#endif"), SVI("\nyes\n")},
        {"parens override prec", __LINE__, 0,
            SVI("#if (1 + 2) * 3 == 9\nyes\n#endif"), SVI("\nyes\n")},
        {"nested parens", __LINE__, 0,
            SVI("#if ((2 + 3) * (4 - 1)) == 15\nyes\n#endif"), SVI("\nyes\n")},
        {"defined && defined", __LINE__, 0,
            SVI("#define FOO\n#define BAR\n#if defined(FOO) && defined(BAR)\nyes\n#endif"),
            SVI("\n\n\nyes\n")},
        {"defined && !defined", __LINE__, 0,
            SVI("#define FOO\n#if defined(FOO) && !defined(BAR)\nyes\n#endif"),
            SVI("\n\nyes\n")},
        {"defined || defined", __LINE__, 0,
            SVI("#define FOO\n#if defined(FOO) || defined(BAR)\nyes\n#endif"),
            SVI("\n\nyes\n")},
        {"macro value in if", __LINE__, 0,
            SVI("#define X 42\n#if X == 42\nyes\n#endif"),
            SVI("\n\nyes\n")},
        {"macro expr in if", __LINE__, 0,
            SVI("#define A 10\n#define B 20\n#if A + B == 30\nyes\n#endif"),
            SVI("\n\n\nyes\n")},
        {"elif expression", __LINE__, 0,
            SVI("#if 0\nno\n#elif 2 + 2 == 4\nyes\n#endif"),
            SVI("\n\n\nyes\n")},
        {"elif chain", __LINE__, 0,
            SVI("#if 0\na\n#elif 0\nb\n#elif 1\nc\n#elif 1\nd\n#endif"),
            SVI("\n\n\n\n\nc\n\n\n")},
        {"undefined macro is 0", __LINE__, 0,
            SVI("#if UNDEFINED\nyes\n#else\nno\n#endif"),
            SVI("\n\n\nno\n")},
        {"undefined macro == 0", __LINE__, 0,
            SVI("#if UNDEFINED == 0\nyes\n#endif"),
            SVI("\nyes\n")},
        {"self reference through argument in condition", __LINE__, 0,
            SVI("#define A (1 + A)\n#define ID(x) x\n#if ID(A) == 1\nyes\n#endif"),
            SVI("\n\n\nyes\n")},
        {"indirect self reference through argument in condition", __LINE__, 0,
            SVI("#define A B\n#define B (1 + A)\n#define ID(x) x\n#if ID(A) == 1\nyes\n#endif"),
            SVI("\n\n\n\nyes\n")},
        {"true keyword", __LINE__, 0,
            SVI("#if true\nyes\n#endif"),
            SVI("\nyes\n")},
        {"true macro in condition", __LINE__, 0,
            SVI("#define true 0\n#if !true\nyes\n#endif"),
            SVI("\n\nyes\n")},
        {"true function macro in condition", __LINE__, 0,
            SVI("#define true(x) x\n#if true(1)\nyes\n#endif"),
            SVI("\n\nyes\n")},

        {"prec: + vs *", __LINE__, 0,
            SVI("#if 1 + 2 * 3 == 7\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: * vs + reversed", __LINE__, 0,
            SVI("#if 2 * 3 + 1 == 7\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: - vs *", __LINE__, 0,
            SVI("#if 10 - 2 * 3 == 4\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: - vs /", __LINE__, 0,
            SVI("#if 10 - 6 / 2 == 7\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: % vs +", __LINE__, 0,
            SVI("#if 10 + 7 % 3 == 11\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: << vs +", __LINE__, 0,
            SVI("#if 1 << 2 + 1 == 8\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: << vs ==", __LINE__, 0,
            SVI("#if 1 << 4 == 16\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: >> vs +", __LINE__, 0,
            SVI("#if 32 >> 2 + 1 == 4\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: < vs ==", __LINE__, 0,
            SVI("#if (1 < 2) == 1\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: == vs &", __LINE__, 0,
            SVI("#if 3 & 1 == 1\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: == vs & explicit", __LINE__, 0,
            SVI("#if (3 & 1) == 1\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: & vs ^", __LINE__, 0,
            SVI("#if (0xF & 0x3 ^ 0x1) == 2\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: ^ vs |", __LINE__, 0,
            SVI("#if (1 | 2 ^ 3) == 1 | (2 ^ 3)\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: | vs &&", __LINE__, 0,
            SVI("#if 1 | 0 && 0 | 1\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: && vs ||", __LINE__, 0,
            SVI("#if 1 || 0 && 0\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: || && classic", __LINE__, 0,
            SVI("#if 0 || 1 && 1\nyes\n#endif"), SVI("\nyes\n")},
        {"prec: && || false path", __LINE__, 0,
            SVI("#if 0 && 0 || 0\nyes\n#else\nno\n#endif"), SVI("\n\n\nno\n")},
        {"prec: || vs ?:", __LINE__, 0,
            SVI("#if (1 || 0 ? 10 : 20) == 10\nyes\n#endif"), SVI("\nyes\n")},
        {"ternary right-assoc", __LINE__, 0,
            SVI("#if (1 ? 2 : 3 ? 4 : 5) == 2\nyes\n#endif"), SVI("\nyes\n")},
        {"ternary right-assoc 2", __LINE__, 0,
            SVI("#if (0 ? 2 : 1 ? 4 : 5) == 4\nyes\n#endif"), SVI("\nyes\n")},
        {"ternary right-assoc 3", __LINE__, 0,
            SVI("#if (0 ? 2 : 0 ? 4 : 5) == 5\nyes\n#endif"), SVI("\nyes\n")},
        {"left-assoc sub", __LINE__, 0,
            SVI("#if 10 - 3 - 2 == 5\nyes\n#endif"), SVI("\nyes\n")},
        {"left-assoc div", __LINE__, 0,
            SVI("#if 100 / 10 / 2 == 5\nyes\n#endif"), SVI("\nyes\n")},
        {"left-assoc mod", __LINE__, 0,
            SVI("#if 17 % 10 % 4 == 3\nyes\n#endif"), SVI("\nyes\n")},
        {"left-assoc shift", __LINE__, 0,
            SVI("#if 1 << 2 << 3 == 32\nyes\n#endif"), SVI("\nyes\n")},
        {"complex expr 1", __LINE__, 0,
            SVI("#if 2 + 3 * 4 - 6 / 2 == 11\nyes\n#endif"), SVI("\nyes\n")},
        {"complex expr 2", __LINE__, 0,
            SVI("#if (1 << 8) - 1 == 255\nyes\n#endif"), SVI("\nyes\n")},
        {"complex expr 3", __LINE__, 0,
            SVI("#if 0xFF & 0x0F | 0xF0 == 0xFF\nyes\n#endif"), SVI("\nyes\n")},
        {"complex expr 4", __LINE__, 0,
            SVI("#if 1 + 2 > 2 && 3 * 4 == 12\nyes\n#endif"), SVI("\nyes\n")},
        {"unary in expr", __LINE__, 0,
            SVI("#if -1 + 2 == 1\nyes\n#endif"), SVI("\nyes\n")},
        {"double negation", __LINE__, 0,
            SVI("#if !!42 == 1\nyes\n#endif"), SVI("\nyes\n")},
        {"not in logical", __LINE__, 0,
            SVI("#if !0 && !0\nyes\n#endif"), SVI("\nyes\n")},
        {"complement and mask", __LINE__, 0,
            SVI("#if (~0xF0 & 0xFF) == 0x0F\nyes\n#endif"), SVI("\nyes\n")},
        {"cmp chain", __LINE__, 0,
            SVI("#if 1 < 2 && 2 > 1 && 3 <= 3 && 3 >= 3 && 4 == 4 && 4 != 5\nyes\n#endif"),
            SVI("\nyes\n")},
        {"ternary chain", __LINE__, 0,
            SVI("#if (0 ? 1 : 0 ? 2 : 0 ? 3 : 4) == 4\nyes\n#endif"),
            SVI("\nyes\n")},
        {"shift vs compare", __LINE__, 0,
            SVI("#if 1 << 4 > 10\nyes\n#endif"), SVI("\nyes\n")},
        {"complex defined", __LINE__, 0,
            SVI("#define A\n#define B\n#define C\n"
               "#if defined(A) && (defined(B) || defined(D)) && !defined(E)\nyes\n#endif"),
            SVI("\n\n\n\nyes\n")},
        {"bitwise chain", __LINE__, 0,
            SVI("#if ((0x0F & 0xFF) ^ 0x05 | 0x10) == 0x1A\nyes\n#endif"),
            SVI("\nyes\n")},
        {"defined no parens", __LINE__, 0,
            SVI("#define X\n#if defined X\nyes\n#endif"),
            SVI("\n\nyes\n")},
        {"defined no parens undef", __LINE__, 0,
            SVI("#if defined NOPE\nyes\n#else\nno\n#endif"),
            SVI("\n\n\nno\n")},
        {"has_c_attribute unknown", __LINE__, 0,
            SVI("#if __has_c_attribute(nonexistent_fake_attr)\nyes\n#else\nno\n#endif"),
            SVI("\n\n\nno\n")},
        {"defined in ternary", __LINE__, 0,
            SVI("#define X\n#if defined(X) ? 1 : 0\nyes\n#endif"),
            SVI("\n\nyes\n")},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        if(test_cases[i].disabled) continue;
        StringView inp = test_cases[i].inp;
        StringView exp = test_cases[i].exp;
        int line = test_cases[i].line;
        StringView result;
        int err = cpp_expand_string(inp, &result, __FILE__, __func__, line);
        TestExpectFalse(int, err);
        if(err) continue;
        if(!test_expect_equals_sv(exp, result, "exp", "result", &TEST_stats, __FILE__, __func__, line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, line, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

TestFunction(test_include){
    TESTBEGIN();
    static const struct {
        const char* name; int line; int disabled;
        StringView path0; StringView content0;
        StringView path1; StringView content1;
        StringView Ipath;
        StringView expected;
    } test_cases[] = {
        {"basic quoted include", __LINE__, 0,
            SVI("test/main.c"),   SVI("#include \"header.h\"\nMAIN_BODY"),
            SVI("test/header.h"), SVI("#define MAIN_BODY hello\n"),
            {0}, SVI("\n\nhello")},
        {"multiline pragma include path", __LINE__, 0,
            SVI("main.c"), SVI("__pragma(\ninclude_path\n\"-I\"\n\"inc\"\n)\n#include <header.h>\n"),
            SVI("inc/header.h"), SVI("ok"),
            {0}, SVI("\n\nok")},
        {"angle bracket with -I path", __LINE__, 0,
            SVI("src/main.c"), SVI("#include <sys.h>\nVALUE"),
            SVI("inc/sys.h"),  SVI("#define VALUE 42\n"),
            SVI("inc"), SVI("\n\n42")},
        {"pragma once prevents double inclusion", __LINE__, 0,
            SVI("test/main.c"), SVI("#include \"hdr.h\"\n#include \"hdr.h\"\nVAL"),
            SVI("test/hdr.h"),  SVI("#pragma once\n#define VAL ok\n"),
            {0}, SVI("\n\n\n\nok")},
        {"macro-expanded include", __LINE__, 0,
            SVI("test/main.c"),   SVI("#define HDR \"header.h\"\n#include HDR\nRESULT"),
            SVI("test/header.h"), SVI("#define RESULT yes\n"),
            {0}, SVI("\n\n\nyes")},
        {"absolute include", __LINE__, 0,
            SVI("src/main.c"), SVI("#include \"/virtual/absolute.h\"\n"),
            SVI("/virtual/absolute.h"), SVI("OK"), {0}, SVI("\nOK")},
        {"absolute has include", __LINE__, 0,
            SVI("src/main.c"), SVI("#if __has_include(\"/virtual/absolute.h\")\nOK\n#endif\n"),
            SVI("/virtual/absolute.h"), SVI(""), {0}, SVI("\nOK\n\n")},
        {"macro expanded fallback candidates", __LINE__, 0,
            SVI("src/main.c"), SVI("#define HEADERS \"missing.h\" \"good.h\"\n#include_oneof HEADERS\n"),
            SVI("src/good.h"), SVI("OK"), {0}, SVI("\n\nOK")},
        {"macro candidate after literal fallback", __LINE__, 0,
            SVI("src/main.c"), SVI("#define HEADER \"good.h\"\n#include_oneof \"missing.h\" HEADER\n"),
            SVI("src/good.h"), SVI("OK"), {0}, SVI("\n\nOK")},
        {"angle macro candidate after angle fallback", __LINE__, 0,
            SVI("src/main.c"), SVI("#define HEADER <good.h>\n#include <missing.h> HEADER\n"),
            SVI("inc/good.h"), SVI("OK"), SVI("inc"), SVI("\n\nOK")},
        {"include path pragma dot in current directory", __LINE__, 0,
            SVI("main.c"), SVI("#pragma include_path \"-I\" \".\"\n#include <good.h>\n"),
            SVI("good.h"), SVI("OK"), {0}, SVI("\n\nOK")},
        {"_Pragma once prevents double inclusion", __LINE__, 0,
            SVI("test/main.c"), SVI("#include \"hdr.h\"\n#include \"hdr.h\"\nVAL"),
            SVI("test/hdr.h"),  SVI("_Pragma(\"once\")\n#define VAL ok\n"),
            {0}, SVI("\n\n\n\nok")},
        {"__pragma once prevents double inclusion", __LINE__, 0,
            SVI("test/main.c"), SVI("#include \"hdr.h\"\n#include \"hdr.h\"\nVAL"),
            SVI("test/hdr.h"),  SVI("__pragma(once)\n#define VAL ok\n"),
            {0}, SVI("\n\n\n\nok")},
        {"include guard prevents double inclusion", __LINE__, 0,
            SVI("test/main.c"), SVI("#include \"hdr.h\"\n#include \"hdr.h\"\nVAL"),
            SVI("test/hdr.h"),  SVI("#ifndef HDR_H\n#define HDR_H\n#define VAL ok\n#endif\n"),
            {0}, SVI("\n\n\n\n\nok")},
        {"include guard with trailing comment", __LINE__, 0,
            SVI("test/main.c"), SVI("#include \"hdr.h\"\n#include \"hdr.h\"\nVAL"),
            SVI("test/hdr.h"),  SVI("#ifndef HDR_H\n#define HDR_H\n#define VAL ok\n#endif // HDR_H\n"),
            {0}, SVI("\n\n\n\n\nok")},
        {"include guard with #else invalidates guard", __LINE__, 0,
            SVI("test/main.c"), SVI("#include \"hdr.h\"\n#include \"hdr.h\"\nVAL"),
            SVI("test/hdr.h"),  SVI("#ifndef HDR_H\n#define HDR_H\n#define VAL ok\n#else\n#endif\n"),
            {0}, SVI("\n\n\n\n\n\n\n\n\n\nok")},
        {"content after #endif invalidates guard", __LINE__, 0,
            SVI("test/main.c"), SVI("#include \"hdr.h\"\n#include \"hdr.h\"\nVAL"),
            SVI("test/hdr.h"),  SVI("#ifndef HDR_H\n#define HDR_H\n#define VAL ok\n#endif\nint x;\n"),
            {0}, SVI("\n\n\n\nint x;\n\n\n\n\nint x;\nok")},
        {"undef guard allows re-include", __LINE__, 0,
            SVI("test/main.c"), SVI("#include \"hdr.h\"\n#undef HDR_H\n#undef VAL\n#include \"hdr.h\"\nVAL"),
            SVI("test/hdr.h"),  SVI("#ifndef HDR_H\n#define HDR_H\n#define VAL ok\n#endif\n"),
            {0}, SVI("\n\n\n\n\n\n\n\n\n\nok")},
        {"include_oneof finds second candidate", __LINE__, 0,
            SVI("test/main.c"),   SVI("#include_oneof <nonexistent.h> \"header.h\"\nRESULT"),
            SVI("test/header.h"), SVI("#define RESULT found\n"),
            {0}, SVI("\n\nfound")},
        {"include with fallback candidates", __LINE__, 0,
            SVI("test/main.c"),   SVI("#include <no1.h> <no2.h> \"header.h\"\nRESULT"),
            SVI("test/header.h"), SVI("#define RESULT fallback\n"),
            {0}, SVI("\n\nfallback")},
        {"include_oneof first candidate found", __LINE__, 0,
            SVI("test/main.c"),   SVI("#include_oneof \"header.h\" <nonexistent.h>\nRESULT"),
            SVI("test/header.h"), SVI("#define RESULT first\n"),
            {0}, SVI("\n\nfirst")},
        {"has_include quoted same dir", __LINE__, 0,
            SVI("test/main.c"),   SVI("#if __has_include(\"header.h\")\n"
                                     "found\n"
                                     "#else\n"
                                     "not_found\n"
                                     "#endif"),
            SVI("test/header.h"), SVI(""),
            {0}, SVI("\nfound\n\n\n")},
        {"has_include not found", __LINE__, 0,
            SVI("test/main.c"),   SVI("#if __has_include(\"nonexistent.h\")\n"
                                     "found\n"
                                     "#else\n"
                                     "not_found\n"
                                     "#endif"),
            SVI("test/other.h"),  SVI(""),
            {0}, SVI("\n\n\nnot_found\n")},
        {"pragma once prevents double include", __LINE__, 0,
            SVI("test/main.c"),   SVI("#include \"header.h\"\n#include \"header.h\"\nDONE"),
            SVI("test/header.h"), SVI("#pragma once\n#define DONE ok\n"),
            {0}, SVI("\n\n\n\nok")},
        {"include guard prevents double include", __LINE__, 0,
            SVI("test/main.c"),   SVI("#include \"header.h\"\n#include \"header.h\"\nVAL"),
            SVI("test/header.h"), SVI("#ifndef GUARD_H\n#define GUARD_H\n#define VAL 42\n#endif\n"),
            {0}, SVI("\n\n\n\n\n42")},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        if(test_cases[i].disabled){
            TEST_stats.skipped++;
            continue;
        }
        IncludeTestFile files[] = {
            {test_cases[i].path0, test_cases[i].content0},
            {test_cases[i].path1, test_cases[i].content1},
        };
        StringView result;
        int err = cpp_expand_with_files(
            files, 2,
            test_cases[i].Ipath.length ? &test_cases[i].Ipath : NULL,
            test_cases[i].Ipath.length ? 1 : 0,
            NULL, 0,
            &result, __FILE__, __func__, test_cases[i].line);
        TestExpectFalse(int, err);
        if(err) continue;
        if(!test_expect_equals_sv(test_cases[i].expected, result, "expected", "result", &TEST_stats, __FILE__, __func__, test_cases[i].line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, test_cases[i].line, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

TestFunction(test_include_eof){
    TESTBEGIN();
    static const struct { StringView main, inner, expected; } cases[] = {
        {SVI("#pragma include_path \"-iquote\" \"quote\"\n#include \"sub/inner.h\"\n"), SVI("#include_next \"only.h\"\n"), SVI("QUOTE")},
        {SVI("#include <Wrap/a.h>\n"), SVI(""), SVI("WRAPNEXT")},
        {SVI("#include \"sub/inner.h\""), SVI("#include \"depth.h\""), SVI("2")},
        {SVI("#include \"sub/inner.h\"\nLEVEL"), SVI("#define LEVEL __INCLUDE_LEVEL__\nLEVEL"), SVI("10")},
        {SVI("#include \"sub/inner.h\"\n)"), SVI("#define ID(x) x\nID(__BASE_FILE__"), SVI("\"src/main.c\"")},
        {SVI("#include \"sub/inner.h\""), SVI("__INCLUDE_LEVEL__"), SVI("1")},
        {SVI("#include \"sub/inner.h\"\n)"), SVI("#define ID(x) x\nID(__INCLUDE_LEVEL__"), SVI("0")},
        {SVI("#include \"sub/inner.h\"\n)"), SVI("#define LEVEL __INCLUDE_LEVEL__\n#define ID(x) x\nID(LEVEL"), SVI("0")},
        {SVI("#include \"sub/inner.h\"\n\"once\")\n#include \"sub/inner.h\"\nAFTER"), SVI("OK\n_Pragma("), SVI("OKAFTER")},
        {SVI("#include \"sub/inner.h\"\n\"include_path \\\".\\\"\")\n#include <local.h>"), SVI("_Pragma("), SVI("OK")},
        {SVI("#include \"sub/inner.h\"\n#include \"sub/inner.h\"\nAFTER"), SVI("OK\n#pragma once"), SVI("OKAFTER")},
        {SVI("#include \"sub/inner.h\"\n#include <local.h>"), SVI("#pragma include_path \".\""), SVI("OK")},
        {SVI("#include \"sub/inner.h\"\n#include \"sub/inner.h\"\nAFTER"), SVI("#ifndef GUARD\n#define GUARD\nOK\n#endif"), SVI("OKAFTER")},
        {SVI("#include \"sub/inner.h\"\nV"), SVI("#define V OK"), SVI("OK")},
        {SVI("#include \"sub/inner.h\"\nAFTER"), SVI("#pragma once"), SVI("AFTER")},
        {SVI("#include \"sub/inner.h\"\nAFTER"), SVI("#if 1\nOK\n#endif"), SVI("OKAFTER")},
        {SVI("#include \"sub/inner.h\"\nAFTER"), SVI("#if 0\nBAD\n#endif"), SVI("AFTER")},
        {SVI("#include \"sub/inner.h\"\n(7)"), SVI("#define F(x) x\nF"), SVI("7")},
        {SVI("#include \"sub/inner.h\"\n7)"), SVI("#define F(x) x\nF("), SVI("7")},
        {SVI("#include \"sub/inner.h\"\nAFTER"), SVI("/* empty */"), SVI("AFTER")},
        {SVI("#include \"sub/inner.h\""), SVI("OK\n"), SVI("OK")},
        {SVI("#define H \"sub/inner.h\"\n#include H"), SVI("OK\n"), SVI("OK")},
        {SVI("#include \"sub/inner.h\"\nAFTER"), SVI("#define H \"local.h\"\n#include H"), SVI("OKAFTER")},
        {SVI("#include \"sub/inner.h\"\nAFTER"), SVI("#include \"local.h\""), SVI("OKAFTER")},
        {SVI("#include <chain.h>"), SVI(""), SVI("OK")},
        {SVI("#include \"sub/inner.h\"\nAFTER"), SVI("#embed \"data\" limit(2)"), SVI("1,2AFTER")},
        {SVI("#include \"sub/inner.h\"\nAFTER"), SVI("_Embed(\"\\\"data\\\"\")"), SVI("1,2,3,4AFTER")},
        {SVI("#include \"sub/inner.h\"\nE"), SVI("#define E _Embed(\"\\\"data\\\"\")\n"), SVI("9")},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(cases); i = test_atomic_increment(&idx)){
        IncludeTestFile files[] = {
            {SV("src/main.c"), cases[i].main},
            {SV("src/sub/inner.h"), cases[i].inner},
            {SV("src/quote/only.h"), SV("QUOTE")},
            {SV("frameworks/Wrap.framework/Headers/a.h"), SV("#pragma once\nWRAP\n#include_next <Wrap/a.h>\n")},
            {SV("next-frameworks/Wrap.framework/Headers/a.h"), SV("NEXT")},
            {SV("src/sub/local.h"), SV("OK\n")},
            {SV("src/sub/depth.h"), SV("__INCLUDE_LEVEL__")},
            {SV("src/local.h"), SV("WRONG\n")},
            {SV("inc1/chain.h"), SV("#if !__has_include_next(<chain.h>)\n#error next header missing\n#endif\n#define NEXT <chain.h>\n#include_next NEXT")},
            {SV("inc2/chain.h"), SV("OK\n")},
            {SV("src/sub/data"), SV("\1\2\3\4")},
            {SV("src/data"), SV("\11")},
        };
        StringView paths[] = {SV("inc0"), SV("inc1"), SV("inc2")};
        StringView frameworks[] = {SV("frameworks"), SV("next-frameworks")};
        StringView result = {0};
        int err = cpp_expand_with_files(files, countof(files), paths, countof(paths), frameworks, countof(frameworks), &result, __FILE__, __func__, __LINE__);
        TestExpectFalse(int, err);
        if(err){
            TestPrintf("source boundary case %zu failed\n", i);
            continue;
        }
        MStringBuilder compact = {.allocator=MALLOCATOR};
        for(size_t j = 0; j < result.length; j++)
            if(result.text[j] != '\n' && result.text[j] != ' ')
                msb_write_char(&compact, result.text[j]);
        TestExpectEqualsSv(cases[i].expected, msb_borrow_sv(&compact));
        msb_destroy(&compact);
        if(result.text) Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

TestFunction(test_embed){
    TESTBEGIN();
    static const struct { StringView source, expected; } cases[] = {
        {SVI("#embed \"data\" gnu::offset(2) limit(3)"), SVI("3,255,0")},
        {SVI("#embed \"data\" limit(2) __gnu__::__offset__(1+2)"), SVI("255,0")},
        {SVI("#embed \"data\" gnu::offset(6) if_empty(42)"), SVI("42")},
        {SVI("#embed \"data\" gnu::offset(9223372036854775807) if_empty(42)"), SVI("42")},
        {SVI("#embed \".\" gnu::base64(\"AQID/wAI\") prefix(9,) suffix(,10)"), SVI("9,1,2,3,255,0,8,10")},
        {SVI("#embed \".\" __gnu__::__base64__(\"A\" \"Q=\" \"=\")"), SVI("1")},
        {SVI("#embed \".\" gnu::base64(\"AQI=\")"), SVI("1,2")},
        {SVI("#embed \".\" gnu::base64(\"\") if_empty(42)"), SVI("42")},
        {SVI("#define B \"AQID\"\n#embed \".\" gnu::base64(B)"), SVI("\n1,2,3")},
        {SVI("_Embed(\"\\\".\\\" gnu::base64(\\\"AQID/wAI\\\")\")"), SVI("1,2,3,255,0,8")},
        {SVI("#if __has_embed(\"data\" gnu::offset(5)) == 1 && __has_embed(\"data\" gnu::offset(6)) == 2\nyes\n#endif\n"), SVI("\nyes\n\n")},
        {SVI("#if __has_embed(\".\" gnu::base64(\"AQ==\")) == 1 && __has_embed(\".\" gnu::base64(\"\")) == 2\nyes\n#endif\n"), SVI("\nyes\n\n")},
        {SVI("#if __has_embed(\"wrong\" gnu::base64(\"AQ==\")) == 0 && __has_embed(\".\" gnu::base64(\"\") limit(0)) == 0\nyes\n#endif\n"), SVI("\nyes\n\n")},
        {SVI("_Embed(\"\\\"empty\\\" if_empty(\\\"\\\\n\\\\\\\"\\\\\\\\\\\")\")"), SVI("\"\\n\\\"\\\\\"")},
        {SVI("#embed \"data\"\n"), SVI("1,2,3,255,0,8\n")},
        {SVI("#embed \"data\""), SVI("1,2,3,255,0,8")},
        {SVI("_Embed(\"\\\"data\\\" limit(2) prefix(9,) suffix(,10)\")"), SVI("9,1,2,10")},
        {SVI("_Embed(\"\\\"data\\\" limit (1 + 1)\")"), SVI("1,2")},
        {SVI("_Embed(\"\\\"data\\\"\")"), SVI("1,2,3,255,0,8")},
        {SVI("#define E \"\\\"data\\\" limit(2)\"\n_Embed( E )"), SVI("\n1,2")},
        {SVI("#define EMBED(...) _Embed(#__VA_ARGS__)\nEMBED(\"data\" limit(2) prefix(9,))"), SVI("\n9,1,2")},
        {SVI("#define F \"data\"\n#define N 2\n_Embed(\"F limit(N)\")"), SVI("\n\n1,2")},
        {SVI("_Embed(\nL\"<data> limit(2)\"\n)"), SVI("1,2")},
        {SVI("#define data wrong\n#embed <data> limit(2)\n"), SVI("\n1,2\n")},
        {SVI("#define F \"data\"\n#embed F __limit__(2) prefix(9,) suffix(,10)\n"), SVI("\n9,1,2,10\n")},
        {SVI("_Embed(\"\\\"data\\\" limit(0) prefix(bad) suffix(bad) if_empty(42)\")"), SVI("42")},
        {SVI("_Embed(\"\\\"empty\\\" prefix(bad) suffix(bad)\")"), SVI("")},
        {SVI("#if __has_embed(\"data\") == __STDC_EMBED_FOUND__ && __has_embed(\"empty\") == __STDC_EMBED_EMPTY__\nyes\n#endif\n"), SVI("\nyes\n\n")},
        {SVI("#if __has_embed(\"data\" limit(0)) == 2 && __has_embed(\"missing\") == 0 && __has_embed(\"data\" vendor::unknown(1)) == 0\nyes\n#endif\n"), SVI("\nyes\n\n")},
        {SVI("#define F \"data\"\n#if __has_embed(F limit(1)) == 1\nyes\n#endif\n"), SVI("\n\nyes\n\n")},
        {SVI("#if 0\n#embed \"missing\"\n#endif\nok"), SVI("\n\n\nok")},
        {SVI("#define S(...) #__VA_ARGS__\n#define X(...) S(__VA_ARGS__)\nX(_Embed(\"\\\"data\\\"\"))"), SVI("\n\n\"1,2,3,255,0,8\"")},
        {SVI("#define F(a,b,c,d,e,f) b+c+d\n#define X(...) F(__VA_ARGS__)\nX(_Embed(\"\\\"data\\\"\"))"), SVI("\n\n2+3+255")},
        {SVI("#define F(a,b,c,d,e,f) a##b e##f\n#define X(...) F(__VA_ARGS__)\nX(_Embed(\"\\\"data\\\"\"))"), SVI("\n\n12 08")},
        {SVI("#define S(x) #x\nS(_Embed(E))"), SVI("\n\"_Embed(E)\"")},
    };
    static int case_idx = 0;
    for(size_t i = test_atomic_increment(&case_idx); i < countof(cases); i = test_atomic_increment(&case_idx)){
        IncludeTestFile files[] = {
            {SV("test/main.c"), cases[i].source},
            {SV("test/data"), SV("\1\2\3\xff\0\10")},
            {SV("test/empty"), SV("")},
        };
        StringView path = SV("test"), result = {0};
        int err = cpp_expand_with_files(files, countof(files), &path, 1, NULL, 0, &result, __FILE__, __func__, __LINE__);
        TestExpectFalse(int, err);
        if(err) goto embed_case_cleanup;
        test_expect_equals_sv(cases[i].expected, result, "expected", "result", &TEST_stats, __FILE__, __func__, __LINE__);
        embed_case_cleanup:
        if(result.text) Allocator_free(MALLOCATOR, result.text, result.length);
    }
    static const StringView bad[] = {
        SVI("#embed \".\" gnu::base64()"),
        SVI("#embed \".\" gnu::base64(123)"),
        SVI("#embed \".\" gnu::base64(L\"AQ==\")"),
        SVI("#embed \"wrong\" gnu::base64(\"AQ==\")"),
        SVI("#embed <.> gnu::base64(\"AQ==\")"),
        SVI("#embed \".\" gnu::base64(\"AQ==\") limit(0)"),
        SVI("#embed \".\" gnu::offset(0) gnu::base64(\"AQ==\")"),
        SVI("#embed \".\" gnu::base64(\"AQ==\") gnu::base64(\"AQ==\")"),
        SVI("#embed __FILE__ gnu::offset(0) __gnu__::__offset__(1)"),
        SVI("#embed __FILE__ gnu::offset(-1)"),
        SVI("#embed __FILE__ gnu::offset(defined(X))"),
        SVI("#embed \".\" gnu::base64(\"AQ=\")"),
        SVI("#embed \".\" gnu::base64(\"AR==\")"),
        SVI("#embed \".\" gnu::base64(\"AQJ=\")"),
        SVI("#embed \".\" gnu::base64(\"AQ==AQ==\")"),
        SVI("#embed \".\" gnu::base64(\"!!!!\")"),
        SVI("#embed \".\" gnu::base64(\"\\x41Q==\")"),
        SVI("#if __has_embed(\".\" gnu::base64(\"AR==\"))\n#endif"),
        SVI("_Embed()"),
        SVI("_Embed(123)"),
        SVI("_Embed(\"\\\"data\\\"\" \"limit(1)\")"),
        SVI("_Embed(\"data\" limit(1))"),
        SVI("#embed \"missing\"\n"),
        SVI("_Embed(\"\\\"missing\\\" limit(0)\")"),
        SVI("_Embed(\"__FILE__ limit(-1)\")"),
        SVI("_Embed(\"__FILE__ limit()\")"),
        SVI("_Embed(\"__FILE__ limit(1 2)\")"),
        SVI("_Embed(\"__FILE__ limit(1, 2)\")"),
        SVI("_Embed(\"__FILE__ limit(defined(X))\")"),
        SVI("_Embed(\"__FILE__ limit(1) __limit__(2)\")"),
        SVI("_Embed(\"__FILE__ prefix([)\"))"),
        SVI("_Embed(\"__FILE__ unknown(1)\")"),
        SVI("_Embed(\"__FILE__ suffix\")"),
    };
    static int bad_idx = 0;
    for(size_t i = test_atomic_increment(&bad_idx); i < countof(bad); i = test_atomic_increment(&bad_idx)){
        StringView log = {0};
        int err = cpp_expand_string_expect_error(bad[i], &log);
        TestExpectFalse(int, err);
        TestExpectTrue(size_t, log.length);
        if(log.text) Allocator_free(MALLOCATOR, log.text, log.length);
    }
    TESTEND();
}

TestFunction(test_embed_file_queries){
    TESTBEGIN();
    static const StringView cases[] = {
        SVI("#if __has_embed(\"metadata\") != 1\n#error probe loaded data\n#endif\n"),
        SVI("#if __has_embed(\"metadata\" limit(0)) != 2\n#error zero limit loaded data\n#endif\n"),
        SVI("#embed \"metadata\" limit(0)\n"),
        SVI("#embed \"" __FILE__ "\" limit(4)\n"),
        SVI("#if __has_embed(\"oversized\") != 0 || __has_embed(\"oversized\" limit(1)) != 0 || __has_embed(\"oversized\" limit(0)) != 0\n#error oversized resource accepted\n#endif\n"),
        SVI("#embed \"oversized\"\n"),
        SVI("#embed \"oversized\" limit(0)\n"),
        SVI("_Embed(\"\\\"oversized\\\" limit(1)\")"),
        SVI("_Embed(\"\\\"metadata\\\" limit(4)\")"),
        SVI("#if __has_embed(\"metadata\" gnu::offset(4194303)) != 1 || __has_embed(\"metadata\" gnu::offset(4194304)) != 2\n#error offset loaded data\n#endif\n"),
        SVI("#if __has_embed(\"oversized\" gnu::offset(4194305)) != 0\n#error offset bypassed size limit\n#endif\n"),
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(cases); i = test_atomic_increment(&idx)){
        ArenaAllocator arena = {0};
        Allocator a = allocator_from_arena(&arena);
        FileCache* fc = fc_create(a, FC_FLAGS_NONE);
        AtomTable at = {0};
        Environment env = {.allocator=a, .at=&at};
        MStringBuilder log = {.allocator=a};
        MsbLogger logger = {0};
        CppPreprocessor cpp = {.allocator=a, .fc=fc, .at=&at,
            .env=&env, .target=&cc_target_test, .logger=msb_logger(&logger, &log)};
        fc_write_path(fc, "main.c", 6);
        TestExpectEquals(int, fc_cache_file(fc, cases[i]), 0);
        fc_write_path(fc, "metadata", 8);
        uint32_t id;
        TestExpectEquals(int, fc_intern_path(fc, &id), 0);
        CachedFile* f = &fc->map.data[id];
        f->exists = f->is_file = f->size_cached = 1;
        f->data_size = 4 * 1024 * 1024;
        if(i == 8){
            char* data = Allocator_alloc(a, f->data_size);
            memset(data, 1, f->data_size);
            f->data_cached = 1;
            f->data.buff = data;
            f->data.n_bytes = f->data_size;
        }
        fc_write_path(fc, "oversized", 9);
        TestExpectEquals(int, fc_intern_path(fc, &id), 0);
        f = &fc->map.data[id];
        f->exists = f->is_file = f->size_cached = 1;
        f->data_size = 4 * 1024 * 1024 + 1;
        fc->may_read_real_files = i == 3;
        TestExpectEquals(int, cpp_define_builtin_macros(&cpp), 0);
        TestExpectEquals(int, ma_push(CppFrame)(&cpp.frames, a,
            ((CppFrame){.file_id=0, .txt=cases[i], .line=1, .column=1})), 0);
        int err;
        CppToken tok;
        MStringBuilder output = {.allocator=a};
        for(;;){
            err = cpp_next_pp_token(&cpp, &tok);
            if(err) break;
            if(tok.type == CPP_END_OF_INPUT) break;
            cpp_write_token(&output, tok);
        }
        if(i >= 5 && i <= 7){
            TestExpectTrue(int, err);
            msb_nul_terminate(&log);
            TestExpectTrue(int, strstr(log.data, "4 MiB") != NULL);
        }
        else TestExpectFalse(int, err);
        if(i == 8) TestExpectEqualsSv(SV("1,1,1,1"), msb_borrow_sv(&output));
        if(i == 3){
            fc_write_path(fc, __FILE__, sizeof __FILE__ - 1);
            TestExpectEquals(int, fc_intern_path(fc, &id), 0);
            TestExpectTrue(int, fc->map.data[id].data_cached);
            StringView full = {0};
            fc_write_path(fc, __FILE__, sizeof __FILE__ - 1);
            TestExpectEquals(int, fc_read_file(fc, &full, &id), 0);
            TestExpectTrue(size_t, full.length > 8);
            TestExpectEquals(size_t, fc->map.data[id].data.n_bytes, full.length);
            MStringBuilder expected = {.allocator=a};
            for(size_t j = 0; j < 4; j++)
                msb_sprintf(&expected, "%u%s", (unsigned)(unsigned char)full.text[j], j == 3 ? "\n" : ",");
            TestExpectEqualsSv(msb_borrow_sv(&expected), msb_borrow_sv(&output));
        }
        ArenaAllocator_free_all(&cpp.synth_arena);
        ArenaAllocator_free_all(&at.arena);
        ArenaAllocator_free_all(&arena);
    }
    TESTEND();
}

TestFunction(test_embed_base64_size){
    TESTBEGIN();
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < 3; i = test_atomic_increment(&idx)){
        size_t size = 4*1024*1024-1+i;
        size_t encoded_size = (size+2)/3*4;
        char* encoded = Allocator_alloc(MALLOCATOR, encoded_size);
        TestExpectTrue(void*, encoded);
        if(!encoded) return TEST_stats;
        memset(encoded, 'A', encoded_size);
        size_t padding = (3-size%3)%3;
        memset(encoded+encoded_size-padding, '=', padding);
        MStringBuilder source = {.allocator=MALLOCATOR};
        msb_write_literal(&source, "#if __has_embed(\".\" gnu::base64(\"");
        msb_write_str(&source, encoded, encoded_size);
        msb_sprintf(&source, "\")) != %d\n#error incorrect base64 size limit\n#endif\n", size > 4*1024*1024 ? 0 : 1);
        StringView result = {0};
        int err = cpp_expand_string(msb_borrow_sv(&source), &result, __FILE__, __func__, __LINE__);
        TestExpectFalse(int, err);
        if(result.text) Allocator_free(MALLOCATOR, result.text, result.length);
        if(size > 4*1024*1024){
            msb_reset(&source);
            msb_write_literal(&source, "#embed \".\" gnu::base64(\"");
            msb_write_str(&source, encoded, encoded_size);
            msb_write_literal(&source, "\")\n");
            StringView log = {0};
            err = cpp_expand_string_expect_error(msb_borrow_sv(&source), &log);
            TestExpectFalse(int, err);
            TestExpectTrue(size_t, log.length);
            if(log.text) Allocator_free(MALLOCATOR, log.text, log.length);
        }
        msb_destroy(&source);
        Allocator_free(MALLOCATOR, encoded, encoded_size);
    }
    TESTEND();
}

TestFunction(test_framework_include){
    TESTBEGIN();
    // #include <Foo/Bar.h> resolves to {fwdir}/Foo.framework/Headers/Bar.h.
    // A multi-component subpath <Foo/sub/Bar.h> must resolve to
    // {fwdir}/Foo.framework/Headers/sub/Bar.h (e.g. <IOKit/hidsystem/IOLLEvent.h>).
    static const struct {
        const char* name; int line; int disabled;
        StringView main_src;
        StringView hdr_path; StringView hdr_content;
        StringView expected;
        _Bool empty_path;
    } test_cases[] = {
        {"empty framework search directory", __LINE__, 0,
            SVI("#include <Local/header.h>\n"),
            SVI("Local.framework/Headers/header.h"), SVI("LOCAL"),
            SVI("\nLOCAL"), 1},
        {"single-component framework include", __LINE__, 0,
            SVI("#include <Cocoa/Cocoa.h>\nVALUE"),
            SVI("frameworks/Cocoa.framework/Headers/Cocoa.h"), SVI("#define VALUE single\n"),
            SVI("\n\nsingle")},
        {"multi-component framework subpath", __LINE__, 0,
            SVI("#include <IOKit/hidsystem/IOLLEvent.h>\nVALUE"),
            SVI("frameworks/IOKit.framework/Headers/hidsystem/IOLLEvent.h"), SVI("#define VALUE nested\n"),
            SVI("\n\nnested")},
    };
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < countof(test_cases); i = test_atomic_increment(&idx)){
        if(test_cases[i].disabled){
            TEST_stats.skipped++;
            continue;
        }
        IncludeTestFile files[] = {
            {SV("test/main.c"), test_cases[i].main_src},
            {test_cases[i].hdr_path, test_cases[i].hdr_content},
        };
        StringView fpath = test_cases[i].empty_path ? SV("") : SV("frameworks");
        StringView result;
        int err = cpp_expand_with_files(
            files, 2,
            NULL, 0,
            &fpath, 1,
            &result, __FILE__, __func__, test_cases[i].line);
        TestExpectFalse(int, err);
        if(err) continue;
        if(!test_expect_equals_sv(test_cases[i].expected, result, "expected", "result", &TEST_stats, __FILE__, __func__, test_cases[i].line)){
            TestPrintf("%s:%d: %s failed\n", __FILE__, test_cases[i].line, test_cases[i].name);
        }
        Allocator_free(MALLOCATOR, result.text, result.length);
    }
    TESTEND();
}

TestFunction(test_literal_boundaries){
    TESTBEGIN();
    static const struct {
        IncludeTestFile files[2];
        StringView expected;
    } cases[] = {
        {{{SVI("dir\"/file\".c"), SVI("__FILE__ __FILE_NAME__ __DIR__ __BASE_FILE__")},
          {SVI("unused.h"), SVI("")}},
         SVI("\"dir\\\"/file\\\".c\" \"file\\\".c\" \"dir\\\"\" \"dir\\\"/file\\\".c\"")},
        {{{SVI("C:\\new\\test.c"), SVI("__FILE__ __BASE_FILE__")},
          {SVI("unused.h"), SVI("")}},
         SVI("\"C:\\\\new\\\\test.c\" \"C:\\\\new\\\\test.c\"")},
        {{{SVI("main.c"), SVI("#pragma include_path u8\"in\\143\"\n#include <header.h>\n")},
          {SVI("inc/header.h"), SVI("OK\n")}},
         SVI("\n\nOK\n")},
        {{{SVI("main.c"), SVI("_Pragma(\"include_path \\\"in\\\\143\\\"\")\n#include <header.h>\n")},
          {SVI("inc/header.h"), SVI("OK\n")}},
         SVI("\n\nOK\n")},
        {{{SVI("main.c"), SVI("#pragma framework_path L\"fra\\155eworks\"\n#include <Foo/header.h>\n")},
          {SVI("frameworks/Foo.framework/Headers/header.h"), SVI("OK\n")}},
         SVI("\n\nOK\n")},
        {{{SVI("main.c"), SVI("#include \"once.h\"\n#include \"once.h\"\n")},
          {SVI("once.h"), SVI("_Pragma(L\"once\")\nOK\n")}},
         SVI("\n\nOK\n\n")},
    };
    for(size_t i = 0; i < countof(cases); i++){
        StringView result = {0};
        int err = cpp_expand_with_files(cases[i].files, 2, NULL, 0, NULL, 0, &result, __FILE__, __func__, __LINE__);
        TestExpectFalse(int, err);
        if(!err) test_expect_equals_sv(cases[i].expected, result, "expected", "result", &TEST_stats, __FILE__, __func__, __LINE__);
        if(result.text) Allocator_free(MALLOCATOR, result.text, result.length);
    }
    // Stringification must preserve the spelling of string/character literals.
    StringView result = {0};
    int err = cpp_expand_string(SV("#define S(x) #x\nS(u\"\\x1234\" U'\\U0001f600')"), &result, __FILE__, __func__, __LINE__);
    TestExpectFalse(int, err);
    if(!err) test_expect_equals_sv(SV("\n\"u\\\"\\\\x1234\\\" U'\\\\U0001f600'\""), result, "expected", "result", &TEST_stats, __FILE__, __func__, __LINE__);
    if(result.text) Allocator_free(MALLOCATOR, result.text, result.length);
    TESTEND();
}

int main(int argc, char** argv){
    #ifdef USE_TESTING_ALLOCATOR
    testing_allocator_init();
    #endif
    RegisterTestFlags(test_obj_macros, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_func_macros, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_func_macros_extensions, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTest(test_literal_boundaries);
    RegisterTest(test_for_each);
    RegisterTest(test_for_each_empty);
    RegisterTestFlags(test_c23, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_torture, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_builtin_macros, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_error_locations, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_va_opt_error_propagation, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_messages, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_condition, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_erroneous_condition, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_if_eval, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_include, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_embed, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_include_eof, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_embed_file_queries, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_embed_base64_size, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_framework_include, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    int err = test_main(argc, argv, NULL);
    #ifdef USE_TESTING_ALLOCATOR
    testing_assert_all_freed();
    #endif
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "../Drp/Allocators/allocator.c"
#include "../Drp/file_cache.c"
#include "cpp_preprocessor.c"
