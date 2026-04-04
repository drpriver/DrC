//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#define USE_TESTING_ALLOCATOR
#define NO_NATIVE_CALL
#define CI_THREAD_UNSAFE_ALLOCATOR
#define HEAVY_RECORDING
#define ARENA_EXPLICIT_ALLOCATOR
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
#include "cc_target.h"
#include "ci_interp.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
static _Bool check_leaks = 0;
enum {NOT_STARTED, IN_PROGRESS, DONE};
static struct OomTestCase {
    int line;
    StringView program;
    int64_t max_allocs;
    int64_t setup_allocs;
    int baseline_done;
    int fail_idx; // atomic
} test_programs[] = {
    {__LINE__, SVI("return 13;\n")},
    {__LINE__, SVI("int x = 3 + 4;\nreturn x;\n")},
    {__LINE__, SVI("struct S { int x; int y; };\n"
         "struct S s = {3, 4};\n"
         "return s.x + s.y;\n")},
    {__LINE__, SVI("int add(int a, int b){ return a + b; }\n"
         "return add(3, 4);\n")},
    {__LINE__, SVI("int arr[5] = {1, 2, 3, 4, 5};\n"
         "int sum = 0;\n"
         "for(int i = 0; i < 5; i++) sum += arr[i];\n"
         "return sum;\n")},
    {__LINE__, SVI("enum E { A, B, C };\n"
         "return C;\n")},
    {__LINE__, SVI("typedef struct Node Node;\n"
         "struct Node { int val; Node *next; };\n"
         "Node c = {3, 0};\n"
         "Node b = {2, &c};\n"
         "Node a = {1, &b};\n"
         "int sum = 0;\n"
         "for(Node *p = &a; p; p = p->next) sum += p->val;\n"
         "return sum;\n")},
    {__LINE__, SVI("_Static_assert(sizeof(int) == 4, \"\");\n"
         "constexpr int x = 6 * 7;\n"
         "return x;\n")},
    {__LINE__, SVI("struct Inner { int a; int b; };\n"
         "struct Outer { int tag; struct Inner in; };\n"
         "struct Outer o = {.tag = 2, .in = {10, 20}};\n"
         "switch(o.tag){\n"
         "  case 1: return o.in.a;\n"
         "  case 2: return o.in.b;\n"
         "  default: return -1;\n"
         "}\n")},
    {__LINE__, SVI("typedef int (*BinOp)(int, int);\n"
         "int add(int a, int b){ return a + b; }\n"
         "int mul(int a, int b){ return a * b; }\n"
         "BinOp ops[2] = {add, mul};\n"
         "return ops[0](3, 4) + ops[1](3, 4);\n")},
    {__LINE__, SVI("int fact(int n){ if(n <= 1) return 1; return n * fact(n-1); }\n"
         "return fact(6);\n")},
    {__LINE__, SVI("int x = 1;\n"
         "float f = 2.0f;\n"
         "return _Generic(x, int: 10, float: 20, default: 30)\n"
         "     + _Generic(f, int: 10, float: 20, default: 30);\n")},
    {__LINE__, SVI("struct S { int tag; union { int ival; float fval; }; };\n"
         "struct S s = {.tag = 1, .ival = 42};\n"
         "return s.tag + s.ival;\n")},
    {__LINE__, SVI("struct V { int x; int y; };\n"
         "struct V* p = &(struct V){.x=10, .y=20};\n"
         "return p->x + p->y;\n")},
    {__LINE__, SVI("struct S { int a; int b; int c; };\n"
         "_Static_assert((struct S).fields == 3, \"\");\n"
         "_Static_assert((struct S).is_struct, \"\");\n"
         "_Static_assert((int).is_integer, \"\");\n"
         "_Static_assert((int*).pointee.is_integer, \"\");\n"
         "constexpr int x = 6 * 7;\n"
         "return x;\n")},
    {__LINE__, SVI("const char* s = \"hello\";\n"
         "unsigned short w[] = u\"AB\";\n"
         "unsigned int u[] = U\"\\U0001F600\";\n"
         "return s[0] + w[0] + (u[0] == 0x1F600);\n")},
    {__LINE__, SVI("int f(int a, int b, int c){ return a * 100 + b * 10 + c; }\n"
         "return f(.c = 3, .a = 1, .b = 2);\n")},
    {__LINE__, SVI("int x = 10;\n"
         "int y = 0;\n"
         "__atomic_load(&x, &y, __ATOMIC_SEQ_CST);\n"
         "int old = __atomic_fetch_add(&x, 5, __ATOMIC_SEQ_CST);\n"
         "return old + y;\n")},
    {__LINE__, SVI("int sum = 0;\n"
         "int i = 0;\n"
         "top:\n"
         "if(i >= 5) goto done;\n"
         "int j = 0;\n"
         "do {\n"
         "  sum += i + j;\n"
         "  j++;\n"
         "} while(j < 3);\n"
         "i++;\n"
         "goto top;\n"
         "done:\n"
         "return sum;\n")},
    {__LINE__, SVI("struct Vec { int x; int y; };\n"
         "int mag2(struct Vec* v){ return v->x * v->x + v->y * v->y; }\n"
         "struct Vec v = {3, 4};\n"
         "return v.mag2();\n")},
    {__LINE__, SVI("typedef unsigned char u8;\n"
         "typedef unsigned short u16;\n"
         "enum Color : u8 { RED, GREEN, BLUE };\n"
         "enum Color c = BLUE;\n"
         "return (int)c + sizeof(enum Color);\n")},
};

static
int
run_one(Allocator al, StringView program, int64_t*_Nullable setup_allocs_out){
    int err = 0;
    ArenaAllocator arena = {0};
    arena.base = al;
    Allocator arena_al = allocator_from_arena(&arena);
    FileCache* fc = fc_create(arena_al);
    if(!fc){ err = 1; goto done; }
    MStringBuilder log_sb = {.allocator=arena_al};
    MsbLogger logger_ = {0};
    Logger* logger = msb_logger(&logger_, &log_sb);
    AtomTable at = {.allocator = arena_al};
    Environment env = {.allocator = arena_al, .at=&at};
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
    interp.parser.cpp.synth_arena.base = al;
    interp.parser.scratch_arena.base = al;
    fc_write_path(fc, "(oom-test)", sizeof "(oom-test)" - 1);
    err = fc_cache_file(fc, program);
    if(err) goto cleanup;
    err = cpp_define_builtin_macros(&interp.parser.cpp);
    if(err) goto cleanup;
    err = cc_define_builtin_types(&interp.parser);
    if(err) goto cleanup;
    err = cc_register_pragmas(&interp.parser);
    if(err) goto cleanup;
    err = ci_register_pragmas(&interp);
    if(err) goto cleanup;
    err = ci_register_macros(&interp);
    if(err) goto cleanup;
    CppFrame frame_data = {
        .file_id = (uint32_t)fc->map.count - 1,
        .txt = program,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CppFrame)(&interp.parser.cpp.frames, interp.parser.cpp.allocator, frame_data);
    if(err) goto cleanup;
    if(setup_allocs_out){
        TestingAllocator* ta = al._data;
        *setup_allocs_out = ta->nallocs;
    }
    err = cc_parse_all(&interp.parser);
    if(err) goto cleanup;
    err = ci_resolve_refs(&interp, 0);
    if(err) goto cleanup;
    {
        CiInterpFrame* frame = &interp.top_frame;
        frame->stmts = interp.parser.toplevel_statements.data;
        frame->stmt_count = interp.parser.toplevel_statements.count;
        while(frame->pc < frame->stmt_count){
            err = ci_interp_step(&interp, frame);
            if(err) goto cleanup;
        }
    }
    cleanup:
    msb_destroy(&log_sb);
    ArenaAllocator_free_all(&interp.parser.cpp.synth_arena);
    ArenaAllocator_free_all(&interp.parser.scratch_arena);
    ArenaAllocator_free_all(&arena);
    done:
    return err;
}

enum { NUM_PROGRAMS = sizeof test_programs / sizeof test_programs[0] };

static
size_t
recording_count_leaked(RecordingAllocator* r){
    size_t leaked = 0;
    for(size_t i = 0; i < r->count; i++){
        if(r->allocation_sizes[i]){
            leaked += r->allocation_sizes[i];
            #ifdef HEAVY_RECORDING
            dump_bt(r->backtraces[i]);
            #endif
        }
    }
    return leaked;
}

TestFunction(test_oom_setup){
    TESTBEGIN();
    // Baseline to find setup_allocs.
    TestingAllocator ta0 = {0};
    LOCK_T_init(&ta0.lock);
    Allocator al0 = {.type = ALLOCATOR_TESTING, ._data = &ta0};
    int64_t setup_allocs = 0;
    int err = run_one(al0, SV("return 0;\n"), &setup_allocs);
    if(err) TestReport("setup baseline failed");
    if(0) TestPrintf("setup: %lld allocations\n", (long long)setup_allocs);
    recording_free_all(&ta0.recorder);
    recording_cleanup(&ta0.recorder);
    if(!err){
        static int idx = 0;
        for(int64_t fail = test_atomic_increment(&idx) + 1; fail <= setup_allocs; fail = test_atomic_increment(&idx) + 1){
            TestingAllocator ta = {0};
            LOCK_T_init(&ta.lock);
            Allocator al = {.type = ALLOCATOR_TESTING, ._data = &ta};
            ta.fail_at = fail;
            run_one(al, SV("return 0;\n"), NULL);
            if(check_leaks){
                size_t leaked = recording_count_leaked(&ta.recorder);
                if(leaked){
                    TEST_stats.failures++;
                    TestPrintf("setup fail_at=%lld: leaked %zu bytes\n", (long long)fail, leaked);
                }
            }
            TEST_stats.executed++;
            recording_free_all(&ta.recorder);
            recording_cleanup(&ta.recorder);
        }
    }
    TESTEND();
}

TestFunction(test_oom){
    TESTBEGIN();
    for(_Bool did_work = 1;did_work;){
        did_work = 0;
        for(size_t p = 0; p < NUM_PROGRAMS; p++){
            struct OomTestCase* tc = &test_programs[p];
            {
                int state = test_atomic_load_acquire(&tc->baseline_done);
                if(state == NOT_STARTED){
                    if(test_atomic_cas(&tc->baseline_done, NOT_STARTED, IN_PROGRESS)){
                        TestingAllocator ta = {0};
                        LOCK_T_init(&ta.lock);
                        Allocator al = {.type = ALLOCATOR_TESTING, ._data = &ta};
                        int err = run_one(al, tc->program, &tc->setup_allocs);
                        TestExpectFalse(err);
                        tc->max_allocs = err ? 0 : ta.nallocs;
                        recording_free_all(&ta.recorder);
                        recording_cleanup(&ta.recorder);
                        test_atomic_store_release(&tc->baseline_done, DONE);
                        did_work = 1;
                    }
                    continue;
                }
                if(state == 1) continue;
            }
            int64_t n = test_atomic_increment(&tc->fail_idx) + 1;
            int64_t fail = tc->setup_allocs + n;
            if(fail > tc->max_allocs) continue;
            did_work = 1;
            TestingAllocator ta = {0};
            LOCK_T_init(&ta.lock);
            Allocator al = {.type = ALLOCATOR_TESTING, ._data = &ta};
            ta.fail_at = fail;
            run_one(al, tc->program, NULL);
            if(check_leaks){
                size_t leaked = recording_count_leaked(&ta.recorder);
                if(leaked){
                    TEST_stats.failures++;
                    TestPrintf("program %zu (line %d) fail_at=%lld: leaked %zu bytes\n",
                        p, tc->line, (long long)fail, leaked);
                }
            }
            TEST_stats.executed++;
            recording_free_all(&ta.recorder);
            recording_cleanup(&ta.recorder);
        }
    }
    TESTEND();
}


int main(int argc, char** argv){
    RegisterTestFlags(test_oom_setup, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_oom, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    ArgToParse extra_args[] = {
        {
            .name = SVI("--check-leaks"),
            .dest = ARGDEST(&check_leaks),
            .help = "Report leaked bytes after each OOM iteration as test failures.",
        },
    };
    ArgParseKwParams extra = {
        .args = extra_args,
        .count = sizeof extra_args / sizeof extra_args[0],
    };
    return test_main(argc, argv, &extra);
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
