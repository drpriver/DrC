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
#include "Drp/dre.h"
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
enum {CT_EXCLUDE_MAX = 32};
typedef struct CtCtx CtCtx;
struct CtCtx {
    Allocator allocator;
    StringView (*any_exclude)[CT_EXCLUDE_MAX];
    StringView (*specific_excludes)[CT_COUNT][CT_EXCLUDE_MAX];
    AtomMap(Atom) (*symbols)[CT_COUNT];
    Parray(Atom)* tags;
    AtomTable* at;
    FileCache* fc;
    LineCache* lc;
    MStringBuilder* sb;
    _Bool debug_re;
    Logger* logger;
};
static int ct_add_tag(CtCtx*, CtSymbolKind, Atom, SrcLoc);
static int ct_build_tag(FileCache*, LineCache*, CtSymbolKind, Atom, SrcLoc, MStringBuilder*);
static int ct_generate_vim(CtCtx*, StringView prefix);
static int ct_generate_tags(CtCtx*);
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
    CppPreprocessor cpp = {
        .allocator = MALLOCATORI,
        .at = &at,
        .env = &env,
    };
    StringView filenames[128] = {0};
    ArgToParse pos_args[] = {
        {
            .name = SVI("file"),
            .dest = ARGDEST(filenames),
            .help = "The files to process.",
            .min_num = 1, .max_num = arrlen(filenames),
        },
    };
    StringView output = {0},
               output_vim = {0},
               syntax_prefix = SVI("c"),
               exclude[CT_EXCLUDE_MAX] = {SVI("__"), SVI("_[hH]$"), SVI("^[a-z][a-z0-9]$")},
               specific_excludes[CT_COUNT][CT_EXCLUDE_MAX] = {
                   [CT_MACRO] = {SVI("^_Nonnull$"), SVI("^_Nullable$"), SVI("_Null_unspecified")},
               };
    _Bool debug_re = 0;
    enum {EXCLUDE_IDX=3, MACRO_IDX=5};
    ArgToParse kw_args[] = {
        {
            .name = SVI("-o"),
            .altname1 = SVI("--output-tags"),
            .dest = ARGDEST(&output),
            .help = "Where to write the ctags file to",
            .min_num = 1, .max_num = 1,
        },
        {
            .name = SVI("--output-vim"),
            .dest = ARGDEST(&output_vim),
            .help = "Where to write a syntax .vim file to",
            .min_num = 1, .max_num = 1,
        },
        {
            .name = SVI("--syntax-prefix"),
            .dest = ARGDEST(&syntax_prefix),
            .help = "what to prefix the syntax groups with",
            .min_num = 1, .max_num = 1,
            .show_default = 1,
        },
        [EXCLUDE_IDX] = {
            .name = SVI("--exclude"),
            .dest = ARGDEST(exclude),
            .help = "regex which if matches, the symbol of any kind is excluded from syntax highlighting",
            .min_num = 1, .max_num = CT_EXCLUDE_MAX,
            .show_default = 3,
        },
        {
            .name = SVI("--type-exclude"),
            .dest = ARGDEST(&specific_excludes[CT_TYPE][0]),
            .help = "regex which if matches, the type is excluded from syntax highlighting",
            .min_num = 1, .max_num = CT_EXCLUDE_MAX,
        },
        [MACRO_IDX] = {
            .name = SVI("--macro-exclude"),
            .dest = ARGDEST(&specific_excludes[CT_MACRO][0]),
            .help = "regex which if matches, the macro is excluded from syntax highlighting",
            .min_num = 1, .max_num = CT_EXCLUDE_MAX,
            .show_default = 3,
        },
        {
            .name = SVI("--function-exclude"),
            .dest = ARGDEST(&specific_excludes[CT_FUNCTION][0]),
            .help = "regex which if matches, the function is excluded from syntax highlighting",
            .min_num = 1, .max_num = CT_EXCLUDE_MAX,
        },
        {
            .name = SVI("--global-var-exclude"),
            .dest = ARGDEST(&specific_excludes[CT_GLOBAL_VARIABLE][0]),
            .help = "regex which if matches, the global var is excluded from syntax highlighting",
            .min_num = 1, .max_num = CT_EXCLUDE_MAX,
        },
        {
            .name = SVI("--enumerator-exclude"),
            .dest = ARGDEST(&specific_excludes[CT_ENUMERATORS][0]),
            .help = "regex which if matches, the global var is excluded from syntax highlighting",
            .min_num = 1, .max_num = CT_EXCLUDE_MAX,
        },
        {
            .name = SVI("--debug-re"),
            .dest = ARGDEST(&debug_re),
            .help = "Print out which symbols matched the regexes",
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
        .keyword.next = cpp_kwargs(&cpp),
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
    if(kw_args[EXCLUDE_IDX].num_parsed){
        for(size_t i = kw_args[EXCLUDE_IDX].num_parsed; i < 3; i++)
            exclude[i] = (StringView){0};
    }
    if(kw_args[MACRO_IDX].num_parsed){
        for(size_t i = kw_args[MACRO_IDX].num_parsed; i < 3; i++)
            specific_excludes[CT_MACRO][i] = (StringView){0};
    }
    // Virtual files belong to the shared cache, so register them only once.
    cpp.fc = fc;
    cpp.logger = logger;
    cpp.target = cc_target_funcs[cc_target_arg]();
    err = cpp_setup_builtin_headers(&cpp);
    if(err) goto stringify_error;
    if(!cpp_nostdinc){
        err = cpp_setup_default_includes(&cpp);
        if(err) goto stringify_error;
    }
    err = cpp_cache_cli_defines(&cpp);
    if(err) goto stringify_error;
    MStringBuilder sb = {.allocator=MALLOCATORI};
    AtomMap(Atom) symbols[CT_COUNT] = {0};
    Parray(Atom) ctags = {0};
    LineCache line_cache = {.allocator=MALLOCATORI};
    CtCtx ctx = {
        .allocator = MALLOCATORI,
        .any_exclude = &exclude,
        .specific_excludes = &specific_excludes,
        .tags = &ctags,
        .symbols = &symbols,
        .at = &at,
        .fc = fc,
        .lc = &line_cache,
        .sb = &sb,
        .debug_re = debug_re,
        .logger = logger,
    };
    for(size_t i = 0; i < pos_args[0].num_parsed; i++){
        StringView filename = filenames[i];
        if(!filename.length) continue;
        CcParser parser = {
            .cpp = {
                .allocator = MALLOCATORI,
                .at = &at,
                .env = &env,
            },
            .current = &parser.global,
        };
        parser.cpp.fc = fc;
        parser.cpp.logger = logger;
        parser.cpp.target = cpp.target;
        // Copy the arrays: source pragmas may append paths during parsing.
        for(size_t j = 0; j < arrlen(cpp.include_paths); j++){
            err = ma_extend(StringView)(&parser.cpp.include_paths[j], parser.cpp.allocator, cpp.include_paths[j].data, cpp.include_paths[j].count);
            if(err) goto stringify_error;
        }
        err = ma_extend(StringView)(&parser.cpp.framework_paths, parser.cpp.allocator, cpp.framework_paths.data, cpp.framework_paths.count);
        if(err) goto stringify_error;
        err = cpp_define_builtin_macros(&parser.cpp);
        if(err) goto stringify_error;
        err = cc_define_builtin_types(&parser);
        if(err) goto stringify_error;
        err = cc_register_pragmas(&parser);
        if(err) goto stringify_error;
        err = cpp_apply_cli_defines(&parser.cpp);
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
        {
            AtomMap16Items items = AM16_items(&g->typedefs);
            MARRAY_FOR_EACH(AtomMap16Item, it, items){
                if(!it->payload[0] && !it->payload[1]) continue;
                CcTypedef* p = (CcTypedef*)it->payload;
                err = ct_add_tag(&ctx, CT_TYPE, it->atom, p->loc);
                if(err > 0) goto stringify_error;
            }
        }
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
    }
    err = ct_generate_tags(&ctx);
    if(err) goto stringify_error;
    {
        if(ctx.sb->errored){
            err = _cc_oom_error;
            goto stringify_error;
        }
        StringView contents = msb_borrow_sv(ctx.sb);
        FileError fe = output.length?write_file(output.text, contents.text, contents.length):write_file_handle(FU_STDOUT, contents.text, contents.length);
        if(fe.errored){
            #ifdef _WIN32
            log_error(logger, "Unable to write tags to '%s'\n", output.length?output.text:"stdout");
            #else
            log_error(logger, "Unable to write tags to '%s': %s\n", output.length?output.text:"stdout", strerror(fe.native_error));
            #endif
            return 1;
        }
    }
    if(output_vim.length){
        err = ct_generate_vim(&ctx, syntax_prefix);
        if(err) goto stringify_error;
        if(ctx.sb->errored){
            err = _cc_oom_error;
            goto stringify_error;
        }
        StringView contents = msb_borrow_sv(ctx.sb);
        FileError fe = write_file(output_vim.text, contents.text, contents.length);
        if(fe.errored){
            #ifndef _WIN32
            log_error(logger, "Unable to write '%s': %s\n", output_vim.text, strerror(fe.native_error));
            #else
            log_error(logger, "Unable to write '%s'\n", output_vim.text);
            #endif
            return 1;
        }
    }
    return 0;
    stringify_error:;
    const char* error_name = cc_stringify_error(err);
    log_error(logger, "Fail: %s\n", error_name);
    return 1;
}

static
int
ct_generate_tags(CtCtx* ctx){
    qsort(ctx->tags->data, ctx->tags->count, sizeof(Atom), atom_cmp);
    msb_reset(ctx->sb);
    msb_sprintf(ctx->sb, "!_TAG_FILE_FORMAT	2	/extended format/\n");
    msb_sprintf(ctx->sb, "!_TAG_FILE_SORTED	1	/0=unsorted, 1=sorted, 2=foldcase/\n");
    Atom prev = nil_atom;
    for(size_t i = 0; i < ctx->tags->count; i++){
        Atom a = ctx->tags->data[i];
        if(a == prev) continue;
        prev = a;
        msb_sprintf(ctx->sb, "%s\n", a->data);
    }
    return 0;
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
ct_build_tag(FileCache* fc, LineCache* lines, CtSymbolKind kind, Atom a, SrcLoc loc, MStringBuilder* sb){
    static const char kinds[CT_COUNT] = {
        [CT_GLOBAL_VARIABLE] = 'v',
        [CT_TYPE] = 't',
        [CT_MACRO] = 'd',
        [CT_FUNCTION] = 'f',
        [CT_ENUMERATORS] = 'e',
    };
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
    msb_sprintf(sb, "$/;\"\t%c\tline:%llu", kinds[kind], (unsigned long long)line);
    return sb->errored?_cc_oom_error:0;
}


static
_Bool
ct_vim_needs_escape_in_syntax(Atom name){
    if(name->length > 80) return 1;
    StringView n = {name->length, name->data};
    static const StringView options[] = {
        SVI("contained"), SVI("oneline"), SVI("keepend"), SVI("extend"), SVI("excludenl"),
        SVI("transparent"), SVI("skipnl"), SVI("skipwhite"), SVI("skipempty"), SVI("grouphere"),
        SVI("groupthere"), SVI("display"), SVI("fold"), SVI("conceal"), SVI("concealends"),
        SVI("cchar"), SVI("contains"), SVI("containedin"), SVI("nextgroup"),
    };
    for(size_t i = 0; i < arrlen(options); i++)
        if(sv_iequals(n, options[i])) return 1;
    return 0;
}

static
int
ct_generate_vim(CtCtx* ctx, StringView prefix){
    msb_reset(ctx->sb);
    static const struct { const char* suffix; const char* link; } groups[CT_COUNT] = {
        [CT_GLOBAL_VARIABLE] = {"GlobalVariable", "Identifier"},
        [CT_TYPE] = {"Type", "Type"},
        [CT_MACRO] = {"PreProc", "Macro"},
        [CT_FUNCTION] = {"Function", "Function"},
        [CT_ENUMERATORS] = {"Enum", "Constant"},
    };
    Parray(Atom) names = {0};
    int err = 0;
    msb_sprintf(ctx->sb, "syntax case match\n");
    for(size_t kind = 0; kind < CT_COUNT; kind++){
        names.count = 0;
        AtomMapItems items = AM_items(&(*ctx->symbols)[kind]);
        MARRAY_FOR_EACH(AtomMapItem, it, items){
            if(!it->p) continue;
            err = pa_push(&names, ctx->allocator, (void*)(uintptr_t)it->atom);
            if(err){ err = _cc_oom_error; goto finally; }
        }
        if(!names.count) continue;
        qsort(names.data, names.count, sizeof(Atom), atom_cmp);
        const char* suffix = groups[kind].suffix;
        msb_sprintf(ctx->sb, "\nhighlight default link %.*s%s %s\n", (int)prefix.length, prefix.text, suffix, groups[kind].link);
        size_t batch = 0;
        for(size_t i = 0; i < names.count; i++){
            Atom name = names.data[i];
            if(!ct_vim_needs_escape_in_syntax(name)){
                if(!batch)
                    msb_sprintf(ctx->sb, "syntax keyword %.*s%s", (int)prefix.length, prefix.text, suffix);
                msb_sprintf(ctx->sb, " %s", name->data);
                if(++batch == 64){
                    msb_write_char(ctx->sb, '\n');
                    batch = 0;
                }
            }
            else {
                if(batch){ msb_write_char(ctx->sb, '\n'); batch = 0; }
                msb_sprintf(ctx->sb, "syntax match %.*s%s /\\m\\C\\<\\V", (int)prefix.length, prefix.text, suffix);
                for(size_t j = 0; j < name->length; j++){
                    char c = name->data[j];
                    if(c == '/' || c == '\\') msb_write_char(ctx->sb, '\\');
                    msb_write_char(ctx->sb, c);
                }
                msb_sprintf(ctx->sb, "\\m\\>/\n");
            }
        }
        if(batch) msb_write_char(ctx->sb, '\n');
    }
    finally:;
    pa_cleanup(&names, ctx->allocator);
    return err;
}

static
int
ct_add_tag(CtCtx* ctx, CtSymbolKind kind, Atom name, SrcLoc loc){
    DreContext re_ctx = {0};
    StringView n = {name->length, name->data};
    StringView (*excludes)[CT_EXCLUDE_MAX] = ctx->any_exclude;
    for(size_t i = 0; i < CT_EXCLUDE_MAX; i++){
        StringView re = (*excludes)[i];
        if(!re.text) break;
        if(!re.length) continue;
        size_t match;
        if(dre_match_sv(&re_ctx, re, n, &match)){
            if(ctx->debug_re) log_error(ctx->logger, "Excluding: '%.*s' due to '%.*s'\n", (int)n.length, n.text, (int)re.length, re.text);
            goto skip_symbol;
            return -1;
        }
    }
    excludes = &(*ctx->specific_excludes)[kind];
    for(size_t i = 0; i < CT_EXCLUDE_MAX; i++){
        StringView re = (*excludes)[i];
        if(!re.text) break;
        if(!re.length) continue;
        size_t match;
        if(dre_match_sv(&re_ctx, re, n, &match)){
            if(ctx->debug_re) log_error(ctx->logger, "Excluding: '%.*s' due to '%.*s'\n", (int)n.length, n.text, (int)re.length, re.text);
            goto skip_symbol;
            return -1;
        }
    }
    int err;
    if(kind == CT_MACRO){
        // prevent macros from shadowing other types' highlighting
        for(size_t i = 0; i < CT_COUNT; i++){
            AtomMap* am = &(*ctx->symbols)[i];
            if(AM_get(am, name)){
                goto skip_symbol;
            }
        }
    }
    err = AM_put(&(*ctx->symbols)[kind], ctx->allocator, name, name);
    if(err) return _cc_oom_error;
    skip_symbol:;
    msb_reset(ctx->sb);
    err = ct_build_tag(ctx->fc, ctx->lc, kind, name, loc, ctx->sb);
    if(err){
        if(0) log_error(ctx->logger, "Failed to build tag for '%s'\n", name->data);
        return err;
    }
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
#include "Drp/dre.c"
#include "C/cpp_preprocessor.c"
#include "C/cc_parser.c"

#ifdef __DRC__
#pragma include_path "Vendored/softfloat/SoftFloat-3e/source/include"
#pragma include_path "Vendored/softfloat"
#pragma include_path "Vendored/softfloat/SoftFloat-3e/source/8086-SSE"
#include "Vendored/softfloat/softfloat_unity.c"
#endif
