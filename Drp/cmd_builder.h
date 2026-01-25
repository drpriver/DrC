#ifndef DRP_CMD_BUILDER_H
#define DRP_CMD_BUILDER_H
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include "stringview.h"
#include "long_string.h"
#include "MStringBuilder.h"
#include "env.h"
#include "which.h"
#include "atom.h"

#ifndef MARRAY_T_LongString
#define MARRAY_T_LongString
#define MARRAY_T LongString
#include "Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

typedef struct CmdBuilder CmdBuilder;
struct CmdBuilder {
    MStringBuilder prog;
    Marray(LongString) args;
    union {
        MStringBuilder cmd_line;
        struct {
            size_t sb_cursor;
            size_t sb_capacity;
            char*_Null_unspecified sb_data;
            Allocator allocator;
            int errored;
        };
    };
};


static
void
cmd_prog(CmdBuilder* cmd, LongString prog){
    if(cmd->errored) return;
    if(cmd->args.count != 0)
        cmd->errored = 1;
    if(cmd->errored) return;
    cmd->errored = ma_push(LongString)(&cmd->args, cmd->allocator, prog);
    if(cmd->errored) return;
    cmd->prog.allocator = cmd->allocator;
}

static
void
cmd_arg(CmdBuilder* cmd, LongString arg){
    if(!cmd->args.count) cmd->errored = 1;
    if(cmd->errored) return;
    // XXX: leaking arg
    arg.text = Allocator_dupe(cmd->allocator, arg.text, arg.length+1);
    cmd->errored = ma_push(LongString)(&cmd->args, cmd->allocator, arg);
}

static
void
cmd_carg(CmdBuilder* cmd, const char* arg){
    cmd_arg(cmd, (LongString){strlen(arg), arg});
}
static
void
cmd_aarg(CmdBuilder* cmd, Atom arg){
    cmd_arg(cmd, (LongString){arg->length, arg->data});
}

static
void
cmd_vargf(CmdBuilder* cmd, const char* fmt, va_list vap){
    char buff[1024];
    int n = vsnprintf(buff, sizeof buff, fmt, vap);
    if(n < 0 || n > (int)sizeof buff){
        cmd->errored = 1;
        return;
    }
    cmd_arg(cmd, (LongString){n, buff});
}

static
void
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
cmd_argf(CmdBuilder* cmd, const char* fmt, ...){
    va_list vap;
    va_start(vap, fmt);
    cmd_vargf(cmd, fmt, vap);
    va_end(vap);
}


static
void
cmd_args_(CmdBuilder* cmd, size_t count, LongString* args){
    for(size_t i = 0; i < count; i++)
        cmd_arg(cmd, args[i]);
}

static
void
cmd_cargs_(CmdBuilder* cmd, size_t count, const char*_Nonnull*_Nonnull args){
    for(size_t i = 0; i < count; i++)
        cmd_carg(cmd, args[i]);
}

#define cmd_args(cmd, ...) cmd_args_(cmd, sizeof (LongString[]){__VA_ARGS__} / sizeof(LongString), (LongString[]){__VA_ARGS__})
#define cmd_cargs(cmd, ...) cmd_cargs_(cmd, sizeof (const char*[]){__VA_ARGS__} / sizeof(const char*), (const char*[]){__VA_ARGS__})

static
void
cmd_clear(CmdBuilder* cmd){
    cmd->args.count = 0;
    msb_reset(&cmd->prog);
    msb_reset(&cmd->cmd_line);
    cmd->errored = 0;
}

static
void
cmd_destroy(CmdBuilder* cmd){
    msb_destroy(&cmd->prog);
    msb_destroy(&cmd->cmd_line);
    ma_cleanup(LongString)(&cmd->args, cmd->allocator);
    cmd->errored = 0;
}

static
void
cmd_resolve_prog_path(CmdBuilder* cmd, Environment* env, exist_func* file_exists, void*_Null_unspecified exists_ctx){
    if(cmd->errored) return;
    msb_reset(&cmd->prog);
    cmd->errored = env_resolve_prog_path(env, LS_to_SV(cmd->args.data[0]), &cmd->prog, file_exists, exists_ctx);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
