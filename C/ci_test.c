//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#define HEAVY_RECORDING
#define NO_NATIVE_CALL
#define CI_THREAD_UNSAFE_ALLOCATOR
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

#include "../Drp/compiler_warnings.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

TestFunction(test_whatever){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    struct tc { 
        const char* name; int line;
        StringView program;
        int exit_code;
        _Bool skip;
    } testcases[] = {
        {
            "basic", __LINE__,
            SV("return 13;\n"),
            .exit_code = 13,
        },
        {
            "loops: for", __LINE__,
            SV("int result = 0;\n"
               "for(int i = 0; i < 10; i++) result += i;\n"
               "return result;\n"),
            .exit_code = 45,
        },
    };
    int err;
    for(size_t i = 0; i < sizeof testcases/sizeof testcases[0]; i++){
        struct tc* tc = &testcases[i];
        if(tc->skip){
            TEST_stats.skipped++;
            continue;
        }
        err = 0;
        TEST_stats.executed++;
        FileCache* fc = fc_create(al);
        if(!fc){err = 1; TestReport("setup failure"); goto finally;}
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {.allocator = al};
        Environment env = {.allocator = al, .at=&at};
        CiInterpreter interp = {
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
        if(err){TestPrintf("%s:%d: failed to link\n", __FILE__, tc->line); goto finally;}

        CiInterpFrame* frame = &interp.top_frame;
        frame->stmts = interp.parser.toplevel_statements.data;
        frame->stmt_count = interp.parser.toplevel_statements.count;
        while(frame->pc < frame->stmt_count){
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
        ArenaAllocator_free_all(&arena);
        ArenaAllocator_free_all(&interp.parser.cpp.synth_arena);
        ArenaAllocator_free_all(&interp.parser.scratch_arena);
    }
    TESTEND();
}

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(test_whatever);
    int err = test_main(argc, argv, NULL);
    testing_assert_all_freed();
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
