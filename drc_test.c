//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Runs tests via the actual compiler driver.
// NOTE: this is way slower than an in-process test like we have in C/*_test.c,
// so only add tests here that absolutely need an entire process lifetime.
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "Drp/compiler_warnings.h"
#include "Drp/windowsheader.h"
#ifdef __linux__
enum {IS_LINUX = 1};
#else
enum {IS_LINUX = 0};
#endif
#ifdef __APPLE__
enum {IS_APPLE = 1};
#else
enum {IS_APPLE = 0};
#endif
#include "Drp/testing.h"
#include "Drp/cmd_builder.h"
#include "Drp/cmd_run.h"
#include "Drp/Allocators/mallocator.h"
#include "Drp/Allocators/arena_allocator.h"
#include "Drp/msb_sprintf.h"
LongString DRC_PATH;
StringView COVDIR;
char** ENVP;

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// For testing programs short enough to fit in a command line.
// Avoids having to muck around with tempfiles.
// NOTE: windows command line max length is shorter than you'd expect.
TestFunction(test_snippets){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    CmdBuilder cmd = {.prog.allocator=allocator_from_arena(&arena), .allocator=allocator_from_arena(&arena)};
    AtomTable at = {.allocator=allocator_from_arena(&arena)};
    Environment env = {.allocator=allocator_from_arena(&arena), .at = &at, .windows=IS_WINDOWS};
    env_parse_posix(&env, ENVP);
    MStringBuilder prefix = {.allocator=allocator_from_arena(&arena)};
    static const struct Case {
        const char* name; int line;
        LongString program;
        StringView expected_output;
        LongString args[4];
        _Bool skip;
    } testcases[] = {
        {
            // atexit callbacks require the interpreter to stay alive after main returns.
            "atexit cb", __LINE__,
            .program = LSI(
                "#include <stdio.h>\n"
                "#include <stdlib.h>\n"
                "atexit(void(void){\n"
                "    puts(\"atexit cb\");\n"
                "});\n"
                "puts(\"toplevel\");\n"
            ),
            // Tests are using crt functions
            // so we have to account for line end uses \r.
            #ifdef EOL
            #undef EOL
            #endif
            #ifdef _WIN32
            #define EOL "\r\n"
            #else
            #define EOL "\n"
            #endif
            .expected_output=SVI(
                "toplevel" EOL
                "atexit cb" EOL
            ),
        },
        {
            // ditto, but via main()
            "atexit cb (main)", __LINE__,
            .program = LSI(
                "#include <stdio.h>\n"
                "#include <stdlib.h>\n"
                "void cb(void){\n"
                "    puts(\"atexit cb\");\n"
                "}\n"
                "int main(void){ puts(\"body\"); atexit(cb); return 0; }\n"
            ),
            .expected_output=SVI(
                "body" EOL
                "atexit cb" EOL
            ),
        },
        {
            "_Argc/_Argv/argc/argv", __LINE__,
            .program = LSI(
                "#include <stdio.h>\n"
                "for(int i = 1; i < _Argc; i++) puts(_Argv[i]);\n"
                "puts(\"---\");\n"
                "int main(int argc, char** argv){\n"
                "   for(int i = 1; i < argc; i++) puts(argv[i]);\n"
                "}\n"
            ),
            .args = {LSI("hello"), LSI("world")},
            .expected_output=SVI(
                "hello" EOL
                "world" EOL
                "---" EOL
                "hello" EOL
                "world" EOL
            ),
        },
        {
            "_Argc/_Argv/argc/argv", __LINE__,
            .program = LSI(
                "#include <stdio.h>\n"
                "for(int i = 1; i < _Argc; i++) puts(_Argv[i]);\n"
                "puts(\"---\");\n"
                "int main(int argc, char** argv){\n"
                "   for(int i = 1; i < argc; i++) puts(argv[i]);\n"
                "}\n"
            ),
            .expected_output=SVI(
                "---" EOL
            ),
        },
        {
            "switch _Type", __LINE__,
            .program = LSI(
                "#include <stdio.h>\n"
                "const char* name(_Type T){\n"
                "    switch(T){\n"
                "        case int: return \"int\";\n"
                "        case char*: return \"char*\";\n"
                "        default: return \"other\";\n"
                "    }\n"
                "}\n"
                "puts(name(int));\n"
                "puts(name(char*));\n"
                "puts(name(double));\n"
                "_Type P = int*;\n"
                "puts(name(P.pointee));\n"
            ),
            .expected_output=SVI(
                "int" EOL
                "char*" EOL
                "other" EOL
                "int" EOL
            ),
        },
    };
    static int idx = 0;
    int err = 0;
    for(size_t i = test_atomic_increment(&idx); i < arrlen(testcases); i = test_atomic_increment(&idx)){
        const struct Case* c = &testcases[i];
        cmd_clear(&cmd);
        cmd_prog(&cmd, DRC_PATH);
        msb_write_str(&cmd.prog, DRC_PATH.text, DRC_PATH.length);
        cmd_arg(&cmd, LS("-e"));
        cmd_arg(&cmd, c->program);
        for(size_t a = 0; a < arrlen(c->args); a++){
            if(!c->args[a].text) break;
            cmd_arg(&cmd, c->args[a]);
        }
        LongString output = {0};
        if(COVDIR.length){
            msb_reset(&prefix);
            msb_sprintf(&prefix, "%s/snippet_%zu", COVDIR.text, i);
            if(prefix.errored){
                TEST_stats.assert_failures++;
                continue;
            }
            StringView sv = msb_borrow_sv(&prefix);
            err = env_setenv4(&env, "GCOV_PREFIX", sizeof "GCOV_PREFIX" -1, sv.text, sv.length);
            if(err){
                TEST_stats.assert_failures++;
                continue;
            }
            err = env_setenv4(&env, "GCOV_PREFIX_STRIP", sizeof "GCOV_PREFIX_STRIP" - 1, "999", sizeof "999" - 1);
            if(err){
                TEST_stats.assert_failures++;
                continue;
            }
        }
        size_t envp_size = 0;
        void* envp = env_to_envp(&env, allocator_from_arena(&arena), &envp_size);
        err = cmd_run_capture(&cmd, envp, allocator_from_arena(&arena), &output);
        if(err){
            TestPrintf("%s:%d %s failed: %d\n", __FILE__, c->line, c->name, err);
            if(output.length){
                TestPrintf("%s:%d output: '%s'\n", __FILE__, c->line, output.text);
            }
            if(output.text) Allocator_free(allocator_from_arena(&arena), output.text, output.length+1);
            TEST_stats.failures++;
            continue;
        }
        test_expect_equals_sv(c->expected_output, LS_to_SV(output), "expected output", "actual output", &TEST_stats, __FILE__, __func__, c->line);
        if(output.text) Allocator_free(allocator_from_arena(&arena), output.text, output.length+1);
        if(envp_size) Allocator_free(allocator_from_arena(&arena), envp, envp_size);
    }
    ArenaAllocator_free_all(&arena);
    TESTEND();
}

// Might be slow, but we can't ship broken samples
TestFunction(test_samples){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    CmdBuilder cmd = {.prog.allocator=allocator_from_arena(&arena), .allocator=allocator_from_arena(&arena)};
    AtomTable at = {.allocator=allocator_from_arena(&arena)};
    Environment env = {.allocator=allocator_from_arena(&arena), .at = &at, .windows=IS_WINDOWS};
    env_parse_posix(&env, ENVP);
    MStringBuilder prefix = {.allocator=allocator_from_arena(&arena)};
    static const struct Case {
        int line;
        LongString program;
        StringView expected_output;
        LongString args[4];
        _Bool syntax_only;
        _Bool skip;
    } testcases[] = {
        {
            __LINE__, LSI("Samples/hello.c"),
            SVI("Hello world" EOL),
        },
        {
            __LINE__, LSI("Samples/main.c"),
            SVI(
                "Hello from main!" EOL
                "0) Samples/main.c" EOL
            ),
        },
        {
            __LINE__, LSI("Samples/script.c"),
            SVI( "Hello world from 'Samples/script.c'" EOL),
        },
        {
            __LINE__, LSI("Samples/Simple/calc.c"),
            SVI( "12" EOL),
            {LSI("3 3 3 * +")},
        },
        { __LINE__, LSI("Samples/Simple/mandelbrot.c"), .syntax_only = 1, },
        { __LINE__, LSI("Samples/Simple/primes.c"), .syntax_only = 1, },
        { __LINE__, LSI("Samples/CLI/bf.c"), .syntax_only = 1, },
        { __LINE__, LSI("Samples/CLI/hexdump.c"), .syntax_only = 1, },
        { __LINE__, LSI("Samples/CLI/minigrep.c"), .syntax_only = 1, },
        { __LINE__, LSI("Samples/CLI/sort.c"), .syntax_only = 1, },
        { __LINE__, LSI("Samples/CLI/wc.c"), .syntax_only = 1, },
        { __LINE__, LSI("Samples/TUI/life.c"), .syntax_only = 1, .skip=IS_WINDOWS},
        { __LINE__, LSI("Samples/TUI/mines.c"), .syntax_only = 1, .skip=IS_WINDOWS},
        { __LINE__, LSI("Samples/TUI/rule110.c"), .syntax_only = 1, .skip=IS_WINDOWS},
        {
            __LINE__, LSI("Samples/Simple/vfprintf.c"),
            SVI(
                "Samples/Simple/vfprintf.c:14: Hello world" EOL
                "Samples/Simple/vfprintf.c:15: Hello world" EOL
                "Samples/Simple/vfprintf.c:14: 1. + 2. = 3.000000" EOL
                "Samples/Simple/vfprintf.c:15: 1. + 2. = 3.000000" EOL
                "Samples/Simple/vfprintf.c:14: hi 2.000000 you" EOL
                "Samples/Simple/vfprintf.c:15: hi 2.000000 you" EOL
                "Samples/Simple/vfprintf.c:14: Goodbye world" EOL
                "Samples/Simple/vfprintf.c:15: Goodbye world" EOL
                "Samples/Simple/vfprintf.c:14: hello" EOL
                "Samples/Simple/vfprintf.c:15: hello" EOL
            )
        },
        { __LINE__, LSI("Samples/Extensions/moremacros.c"), .syntax_only = 1, .skip=1},
        { __LINE__, LSI("Samples/Extensions/__get.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/__mixin.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/__VA_COUNT__.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/_Type.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/defblock.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/enum_strings.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/fucs.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/json_parse.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/macrotemplates.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/methods.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/moremacros.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/procmacro-if.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/procmacro.c"), .syntax_only = 1 , .skip=1},
        { __LINE__, LSI("Samples/Extensions/static-if.c"), .syntax_only = 1, .skip = 1 },
        {
            __LINE__, LSI("Samples/Extensions/argv.c"),
            SVI(
                "argc: 1" EOL
                "0) Samples/Extensions/argv.c" EOL
            ),
        },
        {
            __LINE__, LSI("Samples/Extensions/autotypedef.c"),
            SVI(""),
        },
        {
            __LINE__, LSI("Samples/POSIX/atomics.c"),
            SVI(
                "fetch_add counter: 400000 (expected 400000)" EOL
                "fetch_sub counter: 0 (expected 0)" EOL
                "stack items:       4000 (expected 4000)" EOL
                "exchange sum:      55 (expected 55)" EOL
                "fences:            ok" EOL
            ),
            .skip = IS_WINDOWS,
        },
        {
            __LINE__, LSI("Samples/POSIX/locking.c"),
            SVI(
                "Mode: mutex" EOL
                "Counter: 400000 (expected 400000)" EOL
            ),
            .skip = IS_WINDOWS,
        },
        { __LINE__, LSI("Samples/POSIX/dirwatch.c"), .syntax_only = 1, .skip = !IS_APPLE}, // TODO: is this supposed to work on linux?
        { __LINE__, LSI("Samples/POSIX/http_get.c"), .syntax_only = 1, .skip = IS_WINDOWS},
        { __LINE__, LSI("Samples/POSIX/http_server.c"), .syntax_only = 1, .skip = IS_WINDOWS},
        { __LINE__, LSI("Samples/POSIX/ls.c"), .syntax_only = 1, .skip = IS_WINDOWS},
        { __LINE__, LSI("Samples/POSIX/mandelbrot_multithreaded.c"), .syntax_only = 1, .skip = IS_WINDOWS},
        { __LINE__, LSI("Samples/POSIX/pipe.c"), .syntax_only = 1, .skip = IS_WINDOWS},
    };
    static int idx = 0;
    int err = 0;
    for(size_t i = test_atomic_increment(&idx); i < arrlen(testcases); i = test_atomic_increment(&idx)){
        const struct Case* c = &testcases[i];
        if(c->skip) continue;
        cmd_clear(&cmd);
        cmd_prog(&cmd, DRC_PATH);
        msb_write_str(&cmd.prog, DRC_PATH.text, DRC_PATH.length);
        if(c->syntax_only)
            cmd_arg(&cmd, LS("--syntax-only"));
        cmd_arg(&cmd, c->program);
        for(size_t a = 0; a < arrlen(c->args); a++){
            if(!c->args[a].text) break;
            cmd_arg(&cmd, c->args[a]);
        }
        LongString output = {0};
        if(COVDIR.length){
            msb_reset(&prefix);
            msb_sprintf(&prefix, "%s/snippet_%zu", COVDIR.text, i);
            if(prefix.errored){
                TEST_stats.assert_failures++;
                continue;
            }
            StringView sv = msb_borrow_sv(&prefix);
            err = env_setenv4(&env, "GCOV_PREFIX", sizeof "GCOV_PREFIX" -1, sv.text, sv.length);
            if(err){
                TEST_stats.assert_failures++;
                continue;
            }
            err = env_setenv4(&env, "GCOV_PREFIX_STRIP", sizeof "GCOV_PREFIX_STRIP" - 1, "999", sizeof "999" - 1);
            if(err){
                TEST_stats.assert_failures++;
                continue;
            }
        }
        size_t envp_size = 0;
        void* envp = env_to_envp(&env, allocator_from_arena(&arena), &envp_size);
        err = cmd_run_capture(&cmd, envp, allocator_from_arena(&arena), &output);
        if(err){
            TestPrintf("%s:%d %s failed: %d\n", __FILE__, c->line, c->program.text, err);
            if(output.length){
                TestPrintf("%s:%d output: '%s'\n", __FILE__, c->line, output.text);
            }
            if(output.text) Allocator_free(allocator_from_arena(&arena), output.text, output.length+1);
            TEST_stats.failures++;
            continue;
        }
        test_expect_equals_sv(c->expected_output, LS_to_SV(output), "expected output", "actual output", &TEST_stats, __FILE__, __func__, c->line);
        if(output.text) Allocator_free(allocator_from_arena(&arena), output.text, output.length+1);
        if(envp_size) Allocator_free(allocator_from_arena(&arena), envp, envp_size);
    }
    ArenaAllocator_free_all(&arena);
    TESTEND();
}


int main(int argc, char** argv, char** envp){
    ENVP = envp;
    RegisterTestFlags(test_snippets, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_samples, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    ArgToParse kwargs[] = {
        {
            .name = SV("--drc"),
            .dest = ARGDEST((StringView*)&DRC_PATH),
            .help = "Path to drc binary",
            .min_num = 1, .max_num = 1,
            .required = 1,
        },
        {
            .name = SV("--covdir"),
            .dest = ARGDEST(&COVDIR),
            .help = "Where to write coverage data to avoid collisions",
            .min_num = 0, .max_num = 1,
            .required = 0,
        },
    };
    ArgParseKwParams extra = {
        .args = kwargs,
        .count = arrlen(kwargs),
    };
    int err = test_main(argc, argv, &extra);
    return err;
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Drp/Allocators/allocator.c"
