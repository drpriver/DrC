//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "../Drp/compiler_warnings.h"
#include "../Drp/testing.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/env.h"
#include "../Drp/atom_table.h"
#include "../Drp/stdlogger.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/file_cache.h"
#include "../Drp/msb_logger.h"
#include "../Drp/thread_utils.h"
#include "cc_parser.h"
#include "cc_target.h"
#include "ci_interp.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

struct AtomicThreadCb {
    void (*fn)(int);
    int niters;
    volatile int* entries;
    volatile int* exits;
};

static
void
test_atomic_counter_inc(volatile int* p){
#ifdef _MSC_VER
    _InterlockedIncrement((volatile long*)p);
#else
    __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
#endif
}

static
void
test_atomic_counter_add(volatile int* p, int n){
#ifdef _MSC_VER
    _InterlockedExchangeAdd((volatile long*)p, n);
#else
    __atomic_add_fetch(p, n, __ATOMIC_SEQ_CST);
#endif
}

static volatile int test_atomic_thread_iters;

static
void
test_report_atomic_thread_iters(int n){
    test_atomic_counter_add(&test_atomic_thread_iters, n);
}

static
THREADFUNC(test_atomic_thread_main){
    struct AtomicThreadCb* cb = thread_arg;
    test_atomic_counter_inc(cb->entries);
    cb->fn(cb->niters);
    test_atomic_counter_inc(cb->exits);
    return 0;
}

static
int
test_run_atomic_threads(void (*fn)(int), int nthreads, int niters, int* entries, int* exits){
    if(nthreads < 1 || nthreads > 8) return 1;
    *entries = 0;
    *exits = 0;
    test_atomic_thread_iters = 0;
    ThreadHandle threads[8];
    struct AtomicThreadCb cb = {.fn = fn, .niters = niters, .entries = entries, .exits = exits};
    for(int i = 0; i < nthreads; i++)
        if(create_thread(&threads[i], test_atomic_thread_main, &cb))
            return 2;
    for(int i = 0; i < nthreads; i++)
        join_thread(threads[i]);
    return test_atomic_thread_iters;
}

TestFunction(test_concurrent_callbacks){
    TESTBEGIN();
    int err = 0;
    struct tc {
        const char* name; int line;
        StringView program;
        int exit_code;
    } testcases[] = {
        {
            "implicit atomic post-increment", __LINE__,
            SV("int run_atomic_threads(void (*)(int), int, int, int*, int*);\n"
               "void report_atomic_thread_iters(int);\n"
               "_Atomic int counter = 0;\n"
               "void worker(int n){\n"
               "    int local = 0;\n"
               "    for(int i = 0; i < n; i++){ counter++; local++; }\n"
               "    report_atomic_thread_iters(local);\n"
               "}\n"
               "int entries = -1, exits = -1;\n"
               "int reported = run_atomic_threads(worker, 4, 1000, &entries, &exits);\n"
               "if(entries != 4) return 200000 + entries;\n"
               "if(exits != 4) return 300000 + exits;\n"
               "if(reported != 4000) return 500000 + reported;\n"
               "return counter == 4000 ? 1 : 400000 + counter;\n"),
            .exit_code = 1,
        },
        {
            "implicit atomic compound assignment", __LINE__,
            SV("int run_atomic_threads(void (*)(int), int, int, int*, int*);\n"
               "void report_atomic_thread_iters(int);\n"
               "_Atomic int counter = 0;\n"
               "void worker(int n){\n"
               "    int local = 0;\n"
               "    for(int i = 0; i < n; i++){ counter += 2; local++; }\n"
               "    report_atomic_thread_iters(local);\n"
               "}\n"
               "int entries = -1, exits = -1;\n"
               "int reported = run_atomic_threads(worker, 4, 1000, &entries, &exits);\n"
               "if(entries != 4) return 200000 + entries;\n"
               "if(exits != 4) return 300000 + exits;\n"
               "if(reported != 4000) return 500000 + reported;\n"
               "return counter == 8000 ? 1 : 400000 + counter;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic fetch add", __LINE__,
            SV("#include <stdatomic.h>\n"
               "int run_atomic_threads(void (*)(int), int, int, int*, int*);\n"
               "void report_atomic_thread_iters(int);\n"
               "atomic_int counter = 0;\n"
               "void worker(int n){\n"
               "    int local = 0;\n"
               "    for(int i = 0; i < n; i++){ atomic_fetch_add(&counter, 3); local++; }\n"
               "    report_atomic_thread_iters(local);\n"
               "}\n"
               "int entries = -1, exits = -1;\n"
               "int reported = run_atomic_threads(worker, 4, 1000, &entries, &exits);\n"
               "if(entries != 4) return 200000 + entries;\n"
               "if(exits != 4) return 300000 + exits;\n"
               "if(reported != 4000) return 500000 + reported;\n"
               "return counter == 12000 ? 1 : 400000 + counter;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic add fetch", __LINE__,
            SV("#include <stdatomic.h>\n"
               "int run_atomic_threads(void (*)(int), int, int, int*, int*);\n"
               "void report_atomic_thread_iters(int);\n"
               "atomic_int counter = 0;\n"
               "void worker(int n){\n"
               "    int local = 0;\n"
               "    for(int i = 0; i < n; i++){ __atomic_add_fetch(&counter, 1, __ATOMIC_SEQ_CST); local++; }\n"
               "    report_atomic_thread_iters(local);\n"
               "}\n"
               "int entries = -1, exits = -1;\n"
               "int reported = run_atomic_threads(worker, 4, 1000, &entries, &exits);\n"
               "if(entries != 4) return 200000 + entries;\n"
               "if(exits != 4) return 300000 + exits;\n"
               "if(reported != 4000) return 500000 + reported;\n"
               "return counter == 4000 ? 1 : 400000 + counter;\n"),
            .exit_code = 1,
        },
        {
            "stdatomic compare exchange loop", __LINE__,
            SV("#include <stdatomic.h>\n"
               "int run_atomic_threads(void (*)(int), int, int, int*, int*);\n"
               "void report_atomic_thread_iters(int);\n"
               "atomic_int counter = 0;\n"
               "void worker(int n){\n"
               "    int local = 0;\n"
               "    for(int i = 0; i < n; i++){\n"
               "        int old = atomic_load(&counter);\n"
               "        while(!atomic_compare_exchange_weak(&counter, &old, old + 1)){}\n"
               "        local++;\n"
               "    }\n"
               "    report_atomic_thread_iters(local);\n"
               "}\n"
               "int entries = -1, exits = -1;\n"
               "int reported = run_atomic_threads(worker, 4, 1000, &entries, &exits);\n"
               "if(entries != 4) return 200000 + entries;\n"
               "if(exits != 4) return 300000 + exits;\n"
               "if(reported != 4000) return 500000 + reported;\n"
               "return counter == 4000 ? 1 : 400000 + counter;\n"),
            .exit_code = 1,
        },
        {
            "atomic pointer fetch add", __LINE__,
            SV("#include <stdatomic.h>\n"
               "int run_atomic_threads(void (*)(int), int, int, int*, int*);\n"
               "void report_atomic_thread_iters(int);\n"
               "int arr[4096];\n"
               "_Atomic(int*) p = arr;\n"
               "void worker(int n){\n"
               "    int local = 0;\n"
               "    for(int i = 0; i < n; i++){ atomic_fetch_add(&p, 1); local++; }\n"
               "    report_atomic_thread_iters(local);\n"
               "}\n"
               "int entries = -1, exits = -1;\n"
               "int reported = run_atomic_threads(worker, 4, 1000, &entries, &exits);\n"
               "if(entries != 4) return 200000 + entries;\n"
               "if(exits != 4) return 300000 + exits;\n"
               "if(reported != 4000) return 500000 + reported;\n"
               "return p == arr + 4000 ? 1 : 400000 + (int)(p - arr);\n"),
            .exit_code = 1,
        },
        {
            "atomic flag test and set", __LINE__,
            SV("#include <stdatomic.h>\n"
               "int run_atomic_threads(void (*)(int), int, int, int*, int*);\n"
               "void report_atomic_thread_iters(int);\n"
               "atomic_flag flag = ATOMIC_FLAG_INIT;\n"
               "atomic_int winners = 0;\n"
               "void worker(int n){\n"
               "    int local = 0;\n"
               "    for(int i = 0; i < n; i++){\n"
               "        if(!atomic_flag_test_and_set(&flag)) atomic_fetch_add(&winners, 1);\n"
               "        local++;\n"
               "    }\n"
               "    report_atomic_thread_iters(local);\n"
               "}\n"
               "int entries = -1, exits = -1;\n"
               "int reported = run_atomic_threads(worker, 4, 1000, &entries, &exits);\n"
               "if(entries != 4) return 200000 + entries;\n"
               "if(exits != 4) return 300000 + exits;\n"
               "if(reported != 4000) return 500000 + reported;\n"
               "return winners == 1 ? 1 : 400000 + winners;\n"),
            .exit_code = 1,
        },
    };

    for(size_t t = 0; t < sizeof testcases / sizeof testcases[0]; t++){
    struct tc* tc = &testcases[t];
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    err = 0;
    FileCache* fc = fc_create(al);
    if(!fc){ err = 1; TestReport("setup failure"); goto finally; }
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
                .target = cc_target_funcs[CC_TARGET_NATIVE](),
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
    fc_write_path(fc, "(test)", sizeof "(test)" - 1);
    err = fc_cache_file(fc, tc->program);
    if(err){ TestReport("setup failure"); goto finally; }
    err = cpp_define_builtin_macros(&interp.parser.cpp);
    if(err){ TestReport("setup failure"); goto finally; }
    err = cpp_setup_builtin_headers(&interp.parser.cpp);
    if(err){ TestReport("setup failure"); goto finally; }
    err = cc_define_builtin_types(&interp.parser);
    if(err){ TestReport("setup failure"); goto finally; }
    err = cc_register_pragmas(&interp.parser);
    if(err){ TestReport("setup failure"); goto finally; }
    err = ci_register_pragmas(&interp);
    if(err){ TestReport("setup failure"); goto finally; }
    err = ci_register_macros(&interp);
    if(err){ TestReport("setup failure"); goto finally; }
    err = ci_register_sym(&interp, SV("builtins"), SV("run_atomic_threads"), (void*)test_run_atomic_threads);
    if(err){ TestReport("register sym failure"); goto finally; }
    err = ci_register_sym(&interp, SV("builtins"), SV("report_atomic_thread_iters"), (void*)test_report_atomic_thread_iters);
    if(err){ TestReport("register sym failure"); goto finally; }

    err = cpp_include_file_via_file_cache(&interp.parser.cpp, SV("(test)"));
    if(err){ TestReport("failed to include"); goto finally; }
    err = cc_parse_all(&interp.parser);
    if(err){ TestReport("failed to parse"); goto finally; }
    err = ci_resolve_refs(&interp, 0);
    if(err){ TestReport("failed to link"); goto finally; }

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
        TestPrintf("%s:%d: %s: expected (%d) != actual (%d)\n", __FILE__, tc->line, tc->name, tc->exit_code, interp.exit_code);
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
    RegisterTest(test_concurrent_callbacks);
    return test_main(argc, argv, NULL);
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
