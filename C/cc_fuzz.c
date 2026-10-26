//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "../Drp/compiler_warnings.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/env.h"
#include "../Drp/atom_table.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/file_cache.h"
#include "../Drp/msb_logger.h"
#include "cc_parser.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if(size > 10000) return -1;
    int result = 0;
    ArenaAllocator aa = {0};
    Allocator al = allocator_from_arena(&aa);
    FileCache* fc = fc_create(al, FC_FLAGS_NONE);
    if(!fc){
        ArenaAllocator_free_all(&aa);
        return 0;
    }
    MStringBuilder log_sb = {.allocator=al};
    MsbLogger logger_ = {0};
    Logger* logger = msb_logger(&logger_, &log_sb);
    AtomTable at = {0};
    Environment env = {.allocator = al, .at=&at};
    CcModule module = {0};
    CcParser cc = {.root=&module, .module=&module,
        .cpp = {
            .allocator = al,
            .fc = fc,
            .at = &at,
            .logger = logger,
            .env = &env,
            .target = &cc_target_test,
        },
        .current = &cc.module->scope,
        .eager_parsing = 1,
    };
    StringView path = SV("fuzz");
    fc_write_path(fc, path.text, path.length);
    StringView input = {size, (const char*)data};
    int err = fc_cache_file(fc, input);
    if(err) goto done;
    err = cpp_define_builtin_macros(&cc.cpp);
    if(err) goto done;
    err = cc_define_builtin_types(&cc);
    if(err) goto done;
    err = cpp_include_file_via_file_cache(&cc.cpp, path);
    if(err) goto done;
    enum {MAX_FUZZ_TOKENS=512};
    for(;;){
        CcToken tok;
        err = cpp_next_c_token(&cc.cpp, &tok);
        if(err) goto done;
        if(tok.type != CC_EOF && cc.pending.count == MAX_FUZZ_TOKENS){
            result = -1;
            goto done;
        }
        err = ma_push(CcToken)(&cc.pending, al, tok);
        if(err) goto done;
        if(tok.type == CC_EOF)
            break;
    }
    for(size_t i = 0; i < cc.pending.count / 2; i++){
        size_t j = cc.pending.count - 1 - i;
        CcToken tok = cc.pending.data[i];
        cc.pending.data[i] = cc.pending.data[j];
        cc.pending.data[j] = tok;
    }
    err = cc_parse_all(&cc);
    (void)err;

    done:;
    ArenaAllocator_free_all(&at.arena);
    ArenaAllocator_free_all(&aa);
    ArenaAllocator_free_all(&cc.cpp.synth_arena);
    ArenaAllocator_free_all(&cc.scratch_arena);
    return result;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "../Drp/Allocators/allocator.c"
#include "../Drp/file_cache.c"
#include "cpp_preprocessor.c"
#include "cc_parser.c"
