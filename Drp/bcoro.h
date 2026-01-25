#ifndef DRP_BCORO_H
#define DRP_BCORO_H
#include "Allocators/allocator.h"
enum {
    BERROR=-1,
    BYIELDED=0,
    BFINISHED=69,
};

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
typedef struct BCoro BCoro;
struct BCoro {
    int (*func)(void* ctx, void* arg);
    int step;
    void* state;
};

#define BGO(n) case n: goto L##n
#define BYIELD(n) do{coro->step = n; return BYIELDED; L##n:;}while(0)
#define BFINISH() do{coro->step = BFINISHED; Allocator_free(allocator_from_arena(&ctx->tmp_aa), state, sizeof *state); LBFINISHED: return BFINISHED;}while(0)
#define BSTATE(func_, ...) struct state { \
        __VA_ARGS__\
    } * state; \
    if(!coro->state){ \
        state = Allocator_zalloc(allocator_from_arena(&ctx->tmp_aa), sizeof *state); \
        coro->func = (int(*)(void*, void*))func_; \
        coro->state = state; \
    } \
    else state = coro->state

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
