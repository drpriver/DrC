//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "Drp/windowsheader.h"
#include <stdlib.h>
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "Drp/compiler_warnings.h"
#include "Drp/argument_parsing.h"
#include "Drp/env.h"
#include "Drp/Allocators/mallocator.h"
#include "Drp/stdlogger.h"
#include "Drp/atom_table.h"
#include "Drp/term_util.h"
#include "Drp/file_cache.h"
#include "Drp/file_util.h"
#include "Drp/msb_atomize.h"
#include "C/cpp_tok.h"
#include "C/cpp_preprocessor.h"
#include "C/cc_tok.h"
#include "C/cc_errors.h"
#include "C/cc_parser.h"
#include "cpp_args.h"

#ifndef MARRAY_STRING_VIEW
#define MARRAY_STRING_VIEW
#define MARRAY_T StringView
#include "Drp/Marray.h"
#endif

typedef Marray(StringView) StringViews;
#ifndef MARRAY_STRING_VIEWS
#define MARRAY_STRING_VIEWS
#define MARRAY_T StringViews
#include "Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


static _Bool repl_builtin_command(CcParser* parser, StringView input);
static int cc_pointer_of(CcParser*, CcQualType pointee, CcQualType* out);
static const char* cc_stringify_error(int err);
typedef struct LineCache LineCache;
struct LineCache {
    Allocator allocator;
    Marray(StringViews) files;
};
enum CtSymbolKind {
    CT_GLOBAL_VARIABLE,
    CT_TYPE,
    CT_MACRO,
    CT_FUNCTION,
    CT_ENUMERATORS,
};
typedef enum CtSymbolKind CtSymbolKind;
enum {CT_COUNT=CT_ENUMERATORS+1};
typedef struct CtCtx CtCtx;
struct CtCtx {
    Allocator allocator;
    AtomMap(Atom) (*symbols)[CT_COUNT];
    Parray(Atom)* tags;
    AtomTable* at;
    FileCache* fc;
    LineCache* lc;
    MStringBuilder* sb;
};
static int ct_add_tag(CtCtx*, CtSymbolKind, Atom, SrcLoc);
static int ct_build_tag(FileCache*, LineCache*, Atom, SrcLoc, MStringBuilder*);
static int atom_cmp(const void* a, const void* b){
    Atom l = *(const Atom*)a, r = *(const Atom*)b;
    return strcmp(l->data, r->data);
}


int main(int argc, char** argv, char** envp){
    Logger* logger = std_logger();
    if(!logger) return 1;
    static AtomTable at = {.allocator=MALLOCATORI};
    static Environment env = {.allocator=MALLOCATORI, .at=&at};
    unsigned flags = IS_WINDOWS?FC_IS_WINDOWS:FC_FLAGS_NONE;
    // This is incorrect, but is good enough for now
    if(IS_WINDOWS || IS_APPLE) flags |= FC_IS_CASE_INSENSITIVE;
    FileCache* fc = fc_create(MALLOCATOR, flags);
    int err = env_parse_posix(&env, envp);
    if(err){
        log_error(logger, "Unable to parse environment");
        return 1;
    }
    static CcParser parser = {
        .cpp = {
            .allocator = MALLOCATORI,
            .at = &at,
            .env = &env,
        },
        .current = &parser.global,
    };
    StringView filename = {0};
    ArgToParse pos_args[] = {
        {
            .name = SVI("file"),
            .dest = ARGDEST(&filename),
            .help = "The file to preprocess.",
            .min_num = 1, .max_num = 1,
        },
    };
    StringView output = {0}, output_vim = {0}, syntax_prefix = SV("c");
    ArgToParse kw_args[] = {
        {
            .name = SVI("-o"),
            .dest = ARGDEST(&output),
            .help = "Where to write the ctags file to",
            .min_num = 0, .max_num = 1,
        },
        {
            .name = SVI("--output-vim"),
            .dest = ARGDEST(&output_vim),
            .help = "Where to write a syntax .vim file to",
            .min_num = 0, .max_num = 1,
        },
        {
            .name = SVI("--syntax-prefix"),
            .dest = ARGDEST(&syntax_prefix),
            .help = "what to prefix the syntax groups with",
            .min_num = 0, .max_num = 1,
            .show_default = 1,
        },
    };
    enum {HELP, HIDDEN_HELP, FISH};
    ArgToParse early_args[] = {
        [HELP] = {
            .name = SVI("-h"),
            .altname1 = SVI("--help"),
            .help = "Print this help and exit.",
        },
        [HIDDEN_HELP] = {
            .name = SVI("-H"),
            .altname1 = SVI("--hidden-help"),
            .help = "Print out help for the hidden arguments and exit.",
        },
        [FISH] = {
            .name = SVI("--fish-completions"),
            .help = "Print out commands for fish shell completions.",
            .hidden = 1,
        },
    };
    ArgParser argparser = {
        .name = argv[0]?argv[0]:"drctags",
        .description = "C tags",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
        .keyword.next = cpp_kwargs(&parser.cpp),
        .early_out.args = early_args,
        .early_out.count = arrlen(early_args),
        .styling.plain = !stdout_is_terminal(),
    };
    Args args = argc?(Args){argc-1, (const char*const*)argv+1}:(Args){0, 0};
    switch(check_for_early_out_args(&argparser, &args)){
        case HELP:{
            print_argparse_help(&argparser, get_terminal_size().columns);
            fflush(stdout);
            return 0;
        }
        case HIDDEN_HELP:{
            print_argparse_hidden_help(&argparser, get_terminal_size().columns);
            fflush(stdout);
            return 0;
        }
        case FISH:{
            print_argparse_fish_completions(&argparser);
            fflush(stdout);
            return 0;
        }
        default:
            break;
    }
    enum ArgParseError parse_err = parse_args(&argparser, &args, ARGPARSE_FLAGS_ALLOW_KWARG_SEP_TO_BE_OPTIONAL);
    if(parse_err){
        print_argparse_error(&argparser, parse_err);
        return 1;
    }
    parser.cpp.fc = fc;
    parser.cpp.logger = logger;
    parser.cpp.target = cc_target_funcs[cc_target_arg]();
    err = cpp_define_builtin_macros(&parser.cpp);
    if(err) goto stringify_error;
    err = cc_define_builtin_types(&parser);
    if(err) goto stringify_error;
    {
        CcQualType char_star, char_star_star;
        err = cc_pointer_of(&parser, ccqt_basic(CCBT_char), &char_star);
        if(err) goto stringify_error;
        err = cc_pointer_of(&parser, char_star, &char_star_star);
        if(err) goto stringify_error;
        err = cc_register_extern_var(&parser, SV("_Argc"), ccqt_basic(CCBT_int));
        if(err) goto stringify_error;
        err = cc_register_extern_var(&parser, SV("_Argv"), char_star_star);
    }
    err = cc_register_pragmas(&parser);
    if(err) goto stringify_error;
    err = cpp_setup_builtin_headers(&parser.cpp);
    if(err) goto stringify_error;
    if(!cpp_nostdinc){
        err = cpp_setup_default_includes(&parser.cpp);
        if(err) goto stringify_error;
    }
    if(!filename.length){
        log_error(logger, "Must provide an input file");
        err = _cc_io_error;
        goto stringify_error;
    }
    err = cpp_cli_defines(&parser.cpp);
    if(err) goto stringify_error;
    fc->may_read_real_files = 1;
    if(filename.length){
        err = cpp_include_file_via_file_cache(&parser.cpp, (StringView){filename.length, filename.text});
        if(err){
            log_error(logger, "Unable to read '%s'", filename.text);
            goto stringify_error;
        }
    }
    err = cc_parse_all(&parser);
    if(err) goto stringify_error;
    CcScope* g = &parser.global;
    MStringBuilder sb = {.allocator=MALLOCATORI};
    AtomMap(Atom) symbols[CT_COUNT] = {0};
    Parray(Atom) ctags = {0};
    LineCache line_cache = {.allocator=MALLOCATORI};
    CtCtx ctx = {
        .allocator = MALLOCATORI,
        .tags = &ctags,
        .symbols = &symbols,
        .at = &at,
        .fc = fc,
        .lc = &line_cache,
        .sb = &sb,
    };
    {
        AtomMapItems items = AM_items(&g->structs);
        MARRAY_FOR_EACH(AtomMapItem, it, items){
            if(!it->p) continue;
            CcStruct* p = it->p;
            err = ct_add_tag(&ctx, CT_TYPE, it->atom, p->loc);
            if(err > 0) goto stringify_error;
        }
        items = AM_items(&g->unions);
        MARRAY_FOR_EACH(AtomMapItem, it, items){
            if(!it->p) continue;
            CcUnion* p = it->p;
            err = ct_add_tag(&ctx, CT_TYPE, it->atom, p->loc);
            if(err > 0) goto stringify_error;
        }
        items = AM_items(&g->variables);
        MARRAY_FOR_EACH(AtomMapItem, it, items){
            if(!it->p) continue;
            CcVariable* p = it->p;
            err = ct_add_tag(&ctx, CT_GLOBAL_VARIABLE, it->atom, p->loc);
            if(err > 0) goto stringify_error;
        }
        items = AM_items(&g->functions);
        MARRAY_FOR_EACH(AtomMapItem, it, items){
            if(!it->p) continue;
            CcFunc* p = it->p;
            err = ct_add_tag(&ctx, CT_FUNCTION, it->atom, p->loc);
            if(err > 0) goto stringify_error;
        }
        items = AM_items(&g->enums);
        MARRAY_FOR_EACH(AtomMapItem, it, items){
            if(!it->p) continue;
            CcEnum* p = it->p;
            err = ct_add_tag(&ctx, CT_TYPE, it->atom, p->loc);
            if(err > 0) goto stringify_error;
        }
        items = AM_items(&g->enumerators);
        MARRAY_FOR_EACH(AtomMapItem, it, items){
            if(!it->p) continue;
            CcEnumerator* p = it->p;
            err = ct_add_tag(&ctx, CT_ENUMERATORS, it->atom, p->loc);
            if(err > 0) goto stringify_error;
        }
        items = AM_items(&parser.cpp.macros);
        MARRAY_FOR_EACH(AtomMapItem, it, items){
            if(!it->p) continue;
            CppMacro* p = it->p;
            err = ct_add_tag(&ctx, CT_MACRO, it->atom, p->def_loc);
            if(err > 0) goto stringify_error;
        }
    }
    {
        AtomMap16Items items = AM16_items(&g->typedefs);
        MARRAY_FOR_EACH(AtomMap16Item, it, items){
            if(!it->payload[0] && !it->payload[1]) continue;
            CcTypedef* p = (CcTypedef*)it->payload;
            err = ct_add_tag(&ctx, CT_TYPE, it->atom, p->loc);
            if(err > 0) goto stringify_error;
        }
    }
    err = 0;
    qsort(ctags.data, ctags.count, sizeof(Atom), atom_cmp);
    FILE* fp = stdout;
    if(output.length){
        fp = fopen(output.text, "wb");
        if(!fp){
            fprintf(stderr, "Unable to open '%s': %s\n", output.text, strerror(errno));
            return 1;
        }
    }
    fprintf(fp, "!_TAG_FILE_FORMAT	1	/basic format; no extension fields/\n");
    fprintf(fp, "!_TAG_FILE_SORTED	1	/0=unsorted, 1=sorted, 2=foldcase/\n");
    Atom prev = nil_atom;
    for(size_t i = 0; i < ctags.count; i++){
        Atom a = ctags.data[i];
        if(a == prev) continue;
        prev = a;
        fprintf(fp, "%s\n", a->data);
    }
    _Bool write_failed = ferror(fp) != 0;
    if(fflush(fp) == EOF)
        write_failed = 1;
    if(fp != stdout && fclose(fp) == EOF)
        write_failed = 1;
    if(write_failed){
        fprintf(stderr, "Unable to write tags to '%s'\n", output.length?output.text:"stdout");
        return 1;
    }
    if(output_vim.length){
        fp = fopen(output_vim.text, "wb");
        if(!fp){
            fprintf(stderr, "Unable to open '%s': %s\n", output_vim.text, strerror(errno));
            return 1;
        }
        fclose(fp);
    }
    return 0;
    stringify_error:;
    const char* error_name = cc_stringify_error(err);
    fprintf(stderr, "Fail: %s\n", error_name);
    return 1;
}

static
int
ct_get_cached_line(LineCache* lines, uint64_t file_id, uint64_t line, StringView file_text, StringView* out){
    int err = 0;
    while(file_id >= lines->files.count){
        StringViews* p;
        err = ma_zalloc(StringViews)(&lines->files, lines->allocator, &p);
        if(err) return err;
        (void)p;
    }
    StringViews* l = &lines->files.data[file_id];
    if(!l->count && file_text.length){
        StringView remainder = file_text;
        StringView head, tail;
        while(sv_split1(remainder, '\n', &head, &tail)){
            if(head.length && head.text[head.length-1] == '\r')
                head.length--;
            err = ma_push(StringView)(l, lines->allocator, head);
            if(err) return err;
            remainder = tail;
        }
        if(remainder.length && remainder.text[remainder.length-1] == '\r')
            remainder.length--;
        if(remainder.length){
            err = ma_push(StringView)(l, lines->allocator, remainder);
            if(err) return err;
        }
    }
    line--;
    *out = line < l->count?l->data[line] : SV("");
    return err;
}

static
void
ct_write_pattern_text(MStringBuilder* sb, StringView text){
    for(size_t i = 0; i < text.length; i++){
        char c = text.text[i];
        if(c == '/' || c == '\\' || c == '^' || c == '$')
            msb_write_char(sb, '\\');
        msb_write_char(sb, c);
    }
}

static
int
ct_build_tag(FileCache* fc, LineCache* lines, Atom a, SrcLoc loc, MStringBuilder* sb){
    uint64_t line = 0, column = 0, file_id = 0;
    if(loc.is_actually_a_pointer){
        SrcLocExp* e = (SrcLocExp*)(loc.bits & ~1);
        while(e->parent) e = e->parent;
        line = e->line;
        column = e->column;
        file_id = e->file_id;
    }
    else {
        line = loc.line;
        column = loc.column;
        file_id = loc.file_id;
    }
    if(!file_id) return -1;
    if(file_id >= fc->map.count) return -1;
    CachedFile* cf = &fc->map.data[file_id];
    LongString path = cf->path;
    if(path.length && path.text[0] == '<') return -1;
    StringView file_text = {
        cf->data.n_bytes,
        cf->data.buff,
    };
    StringView line_text;
    int err = ct_get_cached_line(lines, file_id, line, file_text, &line_text);
    if(err) return err;
    if(!line || !column || column > line_text.length) return -1;
    msb_sprintf(sb, "%s\t%s\t/", a->data, path.text);
    if(column > 1){
        msb_write_literal(sb, "\\(^");
        ct_write_pattern_text(sb, (StringView){column-1, line_text.text});
        msb_write_literal(sb, "\\)\\@<=");
        ct_write_pattern_text(sb, (StringView){line_text.length-column+1, line_text.text+column-1});
    }
    else{
        msb_write_char(sb, '^');
        ct_write_pattern_text(sb, line_text);
    }
    msb_write_literal(sb, "$/");
    return sb->errored?_cc_oom_error:0;
}

static
int
ct_add_tag(CtCtx* ctx, CtSymbolKind kind, Atom name, SrcLoc loc){
    int err;
    err = AM_put(&(*ctx->symbols)[kind], ctx->allocator, name, name);
    if(err) return _cc_oom_error;
    msb_reset(ctx->sb);
    err = ct_build_tag(ctx->fc, ctx->lc, name, loc, ctx->sb);
    if(err) return err;
    Atom a = msb_atomize(ctx->sb, ctx->at);
    if(!a) return _cc_oom_error;
    err = pa_push(ctx->tags, ctx->allocator, (void*)(uintptr_t)a);
    return err?_cc_oom_error:0;
}


static
const char*
cc_stringify_error(int err){
    const char* error_name = err >= 0 && (size_t)err < sizeof _cc_error_names / sizeof _cc_error_names[0] ? _cc_error_names[err] : "Unknown error";
    return error_name;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Drp/Allocators/allocator.c"
#include "Drp/file_cache.c"
#include "C/cpp_preprocessor.c"
#include "C/cc_parser.c"

#ifdef __DRC__
#pragma include_path "Vendored/softfloat/SoftFloat-3e/source/include"
#pragma include_path "Vendored/softfloat"
#pragma include_path "Vendored/softfloat/SoftFloat-3e/source/8086-SSE"
#include "Vendored/softfloat/softfloat_unity.c"
#endif
