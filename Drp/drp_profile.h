#ifndef DRP_PROFILE_H
#define DRP_PROFILE_H
//
// Inspired by STB's
// https://silverspaceship.com/src/iprof/iprof.txt
// As it would feel wrong to claim ownership when it's so similar,
// I am instead declaring this file to be public domain.
// - David Priver 2026 <david@davidpriver.com>
//
#if defined DRP_PROFILE && DRP_PROFILE
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../Drp/measure_time.h"
#include "../Drp/thread_utils.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/stringview.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/msb_sprintf.h"
#include "../Drp/file_util.h"

#ifndef __builtin_trap
#if defined _MSC_VER && !defined __clang__
#define __builtin_trap() __fastfail(7)
#endif
#endif
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#if defined __GNUC__ || defined __clang__ || defined __DRC__
#define DRP_PROFILE_NOINLINE __attribute__((noinline))
#elif defined _MSC_VER
#define DRP_PROFILE_NOINLINE __declspec(noinline)
#else
#define DRP_PROFILE_NOINLINE
#endif

#ifndef DRP_THREAD_LOCAL
#if defined _MSC_VER && !defined __clang__
#define DRP_THREAD_LOCAL __declspec(thread)
#else
#define DRP_THREAD_LOCAL _Thread_local
#endif
#endif

enum { DRP_PROFILE_NODES = 4096, DRP_PROFILE_HASH_SIZE = 8192, DRP_PROFILE_CACHE_SIZE = 256 };
typedef struct DrpProfileSite DrpProfileSite;
struct DrpProfileSite {
    const char *text,
               *func,
               *file;
    unsigned line;
    _Bool idle;
};

typedef struct DrpProfileNode DrpProfileNode;
struct DrpProfileNode {
    const DrpProfileSite *site;
    unsigned parent;
    uint64_t calls, total, self;
};

typedef struct DrpProfileFrame DrpProfileFrame;
typedef struct DrpProfile DrpProfile;
struct DrpProfileFrame {
    unsigned node;
    _Bool active;
    _Bool idle;
    uint64_t start, children;
    DrpProfileFrame *parent;
    DrpProfile *profile;
};

struct DrpProfile {
    DrpProfile *next;
    unsigned id;
    char name[64];
    uint64_t start, end, idle;
    unsigned idle_depth;
    uint64_t idle_start;
    unsigned count;
    uint64_t dropped;
    DrpProfileFrame *current;
    unsigned cache[DRP_PROFILE_CACHE_SIZE];
    DrpProfileNode nodes[DRP_PROFILE_NODES];
    unsigned hash[DRP_PROFILE_HASH_SIZE];
};

static DRP_THREAD_LOCAL DrpProfile* drp_profile_thread;
static DrpProfile* drp_profile_threads;
static LOCK_T drp_profile_lock = LOCK_T_INIT;
static unsigned drp_profile_thread_count;
static _Bool drp_profile_enabled;
static double drp_profile_milliseconds_per_tick;
static uint64_t drp_profile_start_tick;

force_inline
uint64_t
drp_profile_counter(void){
    #if defined __DRC__
        return performance_counter();
    #elif defined __APPLE__ && defined __aarch64__
        uint64_t ticks;
        __asm__ volatile("mrs %0, S3_4_c15_c10_6" : "=r"(ticks) :: "memory");
        return ticks;
    #elif (defined __GNUC__ || defined __clang__) && (defined __x86_64__ || defined __i386__)
        __asm__ volatile("lfence" ::: "memory");
        uint64_t ticks = performance_counter();
        __asm__ volatile("" ::: "memory");
        return ticks;
    #elif (defined __GNUC__ || defined __clang__) && defined __aarch64__
        __asm__ volatile("isb" ::: "memory");
        uint64_t ticks = performance_counter();
        __asm__ volatile("" ::: "memory");
        return ticks;
    #elif defined _MSC_VER && (defined _M_X64 || defined _M_IX86)
        _ReadWriteBarrier();
        _mm_lfence();
        uint64_t ticks = performance_counter();
        _ReadWriteBarrier();
        return ticks;
    #elif defined _MSC_VER && defined _M_ARM64
        _ReadWriteBarrier();
        __isb(0xf);
        uint64_t ticks = performance_counter();
        _ReadWriteBarrier();
        return ticks;
    #else
        _Static_assert(0, "Unsupported profiler counter target");
        #error Unsupported profiler counter target
    #endif
}

static
void
drp_profile_init_clock(void){
    uint64_t start_us = monotonic_time_us();
    uint64_t start = drp_profile_counter();
    uint64_t end_us;
    do { end_us = monotonic_time_us(); } while(end_us - start_us < 2000);
    uint64_t end = drp_profile_counter();
    drp_profile_milliseconds_per_tick = (end_us - start_us) / (1000. * (end - start));
}

static DRP_PROFILE_NOINLINE
void
drp_profile_attach(void){
    DrpProfile* profile = Allocator_zalloc(MALLOCATOR, 1, sizeof *profile, _Alignof(DrpProfile));
    if(!profile) __builtin_trap();
    LOCK_T_lock(&drp_profile_lock);
    profile->id = drp_profile_thread_count++;
    profile->next = drp_profile_threads;
    drp_profile_threads = profile;
    LOCK_T_unlock(&drp_profile_lock);
    profile->start = drp_profile_counter();
    drp_profile_thread = profile;
}

static
void
drp_profile_thread_name(StringView name){
    if(!drp_profile_enabled) return;
    if(!drp_profile_thread) drp_profile_attach();
    if(name.length >= sizeof drp_profile_thread->name)
        name.length = sizeof drp_profile_thread->name - 1;
    memcpy(drp_profile_thread->name, name.text, name.length);
    drp_profile_thread->name[name.length] = 0;
}

static DRP_PROFILE_NOINLINE
unsigned
drp_profile_find(const DrpProfileSite* site, unsigned parent){
    uint32_t hash = (uint32_t)((uintptr_t)site >> 3) ^ (parent * 0x9e3779b9u);
    hash ^= hash >> 16;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    unsigned slot = hash & (DRP_PROFILE_HASH_SIZE - 1);
    for(;;){
        unsigned index = drp_profile_thread->hash[slot];
        if(!index){
            if(drp_profile_thread->count + 1 == DRP_PROFILE_NODES) return 0;
            index = ++drp_profile_thread->count;
            drp_profile_thread->nodes[index] = (DrpProfileNode){.site = site, .parent = parent};
            drp_profile_thread->hash[slot] = index;
            return index;
        }
        DrpProfileNode* node = &drp_profile_thread->nodes[index];
        if(node->site == site && node->parent == parent) return index;
        slot = (slot + 1) & (DRP_PROFILE_HASH_SIZE - 1);
    }
}

force_inline
void
drp_profile_enter(DrpProfileFrame* frame, const DrpProfileSite* site, unsigned* cached, uint64_t now){
    *frame = (DrpProfileFrame){.start = now, .parent = drp_profile_thread->current, .profile = drp_profile_thread, .active = 1, .idle = site->idle};
    if(frame->idle && !drp_profile_thread->idle_depth++)
        drp_profile_thread->idle_start = now;
    unsigned parent = frame->parent ? frame->parent->node : 0;
    if(!frame->parent || parent){
        DrpProfileNode* node = &drp_profile_thread->nodes[*cached];
        if(!*cached || node->parent != parent || node->site != site)
            *cached = drp_profile_find(site, parent);
        frame->node = *cached;
    }
    if(frame->node)
        drp_profile_thread->nodes[frame->node].calls++;
    else
        drp_profile_thread->dropped++;
    drp_profile_thread->current = frame;
}

force_inline
void
drp_profile_leave(DrpProfileFrame* frame, uint64_t now){
    DrpProfile* profile = frame->profile;
    uint64_t elapsed = now - frame->start;
    if(frame->idle && !--profile->idle_depth)
        profile->idle += now - profile->idle_start;
    if(frame->node){
        DrpProfileNode* node = &profile->nodes[frame->node];
        node->total += elapsed;
        node->self += elapsed - frame->children;
    }
    if(frame->parent) frame->parent->children += elapsed;
    profile->current = frame->parent;
    frame->active = 0;
}

force_inline
void
drp_profile_end(DrpProfileFrame* frame){
    if(frame->active) drp_profile_leave(frame, drp_profile_counter());
}

static
void
drp_profile_thread_finish(void){
    if(!drp_profile_thread || drp_profile_thread->end)
        return;
    uint64_t now = drp_profile_counter();
    while(drp_profile_thread->current)
        drp_profile_leave(drp_profile_thread->current, now);
    drp_profile_thread->end = now;
}

static
void
drp_profile_json_string(MStringBuilder *out, const char *text){
    msb_write_char(out, '"');
    for(const unsigned char* p = (const unsigned char*)text; *p; p++){
        if(*p == '"' || *p == '\\') msb_sprintf(out, "\\%c", *p);
        else if(*p < 32) msb_sprintf(out, "\\u%04x", *p);
        else msb_write_char(out, *p);
    }
    msb_write_char(out, '"');
}

const char *drp_profile_out = "profile.json";
static
void
drp_profile_report(void){
    if(!drp_profile_enabled) return;
    drp_profile_enabled = 0;
    drp_profile_thread_finish();
    uint64_t now = drp_profile_counter();
    size_t capacity = (size_t)drp_profile_thread_count * (DRP_PROFILE_NODES - 1);
    const DrpProfileSite** sites;
    size_t count = capacity ? capacity : 1;
    sites = Allocator_alloc(MALLOCATOR, count, sizeof *sites, _Alignof(const DrpProfileSite*));
    if(!sites) __builtin_trap();
    MStringBuilder sb = {.allocator = MALLOCATOR};
    MStringBuilder *out = &sb;
    unsigned site_count = 0;
    msb_sprintf(
        out,
        "{\"version\":1,\"milliseconds_per_tick\":%.17g,\"elapsed_ticks\":%llu,\"sites\":[",
        drp_profile_milliseconds_per_tick,
        (unsigned long long)(now - drp_profile_start_tick)
    );
    for(DrpProfile* p = drp_profile_threads; p; p = p->next){
        for(unsigned i = 1; i <= p->count; i++){
            const DrpProfileSite* site = p->nodes[i].site;
            unsigned id = 0;
            while(id < site_count && sites[id] != site) id++;
            if(id != site_count) continue;
            sites[site_count++] = site;
            msb_sprintf(out, "%s{\"id\":%u,\"name\":", id ? "," : "", id);
            drp_profile_json_string(out, site->text);
            msb_write_literal(out, ",\"file\":");
            drp_profile_json_string(out, site->file ? site->file : "");
            msb_write_literal(out, ",\"func\":");
            drp_profile_json_string(out, site->func ? site->func : "");
            msb_sprintf(out, ",\"line\":%u", site->line);
            msb_sprintf(out, ",\"idle\":%s}", site->idle ? "true" : "false");
        }
    }
    msb_write_literal(out, "],\"threads\":[");
    for(DrpProfile* p = drp_profile_threads; p; p = p->next){
        uint64_t end = p->end ? p->end : now;
        msb_sprintf(out, "%s{\"id\":%u,\"name\":", p == drp_profile_threads ? "" : ",", p->id);
        drp_profile_json_string(out, p->name);
        msb_sprintf(
            out,
            ",\"start_ticks\":%llu,\"end_ticks\":%llu,\"finished\":%s,\"idle_ticks\":%llu,\"dropped\":%llu,\"nodes\":[",
            (unsigned long long)(p->start - drp_profile_start_tick),
            (unsigned long long)(end - drp_profile_start_tick), p->end ? "true" : "false",
            (unsigned long long)p->idle, (unsigned long long)p->dropped
        );
        for(unsigned i = 1; i <= p->count; i++){
            DrpProfileNode* node = &p->nodes[i];
            unsigned site = 0;
            while(sites[site] != node->site) site++;
            msb_sprintf(out, "%s{\"id\":%u,\"parent\":%u,\"site\":%u,\"calls\":%llu,\"total_ticks\":%llu,\"self_ticks\":%llu}",
                i == 1 ? "" : ",", i, node->parent, site,
                (unsigned long long)node->calls, (unsigned long long)node->total, (unsigned long long)node->self);
        }
        msb_write_literal(out, "]}");
    }
    msb_write_literal(out, "]}\n");
    if(!sb.errored){
        StringView txt = msb_borrow_sv(&sb);
        FileError fe;
        if(drp_profile_out && *drp_profile_out)
            fe = write_file((const char*)drp_profile_out, txt.text, txt.length);
        else
            fe = write_file_handle(FU_STDOUT, txt.text, txt.length);
        (void)fe;
    }
    msb_destroy(&sb);
    Allocator_free(MALLOCATOR, sites, count, sizeof *sites, _Alignof(const DrpProfileSite*));
}

static
void
drp_profile_start(void){
    drp_profile_init_clock();
    drp_profile_start_tick = drp_profile_counter();
    drp_profile_enabled = 1;
    drp_profile_attach();
    drp_profile_thread_name(SV("main"));
    atexit(drp_profile_report);
}

#if defined(__STDC_DEFER_TS25755__)
#define DRP_PROFILE_REGION(scope, label, is_idle) { \
    static const DrpProfileSite scope##_site = {.text = label, .idle = is_idle, .func = __func__, .file = __FILE__, .line = __LINE__}; \
    DrpProfileFrame scope; \
    scope.active = 0; \
    _Defer drp_profile_end(&scope); \
    DRP_PROFILE_ENTER(scope);
#define DRP_PROFILE_END(scope) }
#elif defined(_MSC_VER) && !defined(__clang__)
#define DRP_PROFILE_REGION(scope, label, is_idle) { \
    static const DrpProfileSite scope##_site = {.text = label, .idle = is_idle, .func = __func__, .file = __FILE__, .line = __LINE__}; \
    DrpProfileFrame scope; \
    scope.active = 0; \
    DRP_PROFILE_ENTER(scope); \
    __try {
#define DRP_PROFILE_END(scope) } __finally { drp_profile_end(&scope); } }
#else
#define DRP_PROFILE_REGION(scope, label, is_idle) { \
    static const DrpProfileSite scope##_site = {.text = label, .idle = is_idle, .func = __func__, .file = __FILE__, .line = __LINE__}; \
    DrpProfileFrame scope __attribute__((cleanup(drp_profile_end))); \
    scope.active = 0; \
    DRP_PROFILE_ENTER(scope);
#define DRP_PROFILE_END(scope) }
#endif
#if defined __GNUC__ && !defined __clang__ && __GNUC__ >= 12
#define DRP_PROFILE_DIAGNOSTIC_PUSH \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wdangling-pointer\"")
#define DRP_PROFILE_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#else
#define DRP_PROFILE_DIAGNOSTIC_PUSH
#define DRP_PROFILE_DIAGNOSTIC_POP
#endif
#define DRP_PROFILE_ENTER(scope) do { \
    DRP_PROFILE_DIAGNOSTIC_PUSH \
    if(drp_profile_enabled){ \
        if(!drp_profile_thread) drp_profile_attach(); \
        if(!drp_profile_thread->end){ \
            unsigned* scope##_cached = &drp_profile_thread->cache[((uintptr_t)&scope##_site >> 5) & (DRP_PROFILE_CACHE_SIZE - 1)]; \
            drp_profile_enter(&scope, &scope##_site, scope##_cached, drp_profile_counter()); \
        } \
    } \
    DRP_PROFILE_DIAGNOSTIC_POP \
} while(0)
#define DRP_PROFILE_BEGIN(scope, label) DRP_PROFILE_REGION(scope, label, 0)
#define DRP_PROFILE_IDLE_BEGIN(scope, label) DRP_PROFILE_REGION(scope, label, 1)
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#else
#define drp_profile_start() ((void)0)
#define drp_profile_thread_finish() ((void)0)
#define drp_profile_thread_name(name) ((void)0)
#define DRP_PROFILE_BEGIN(scope, label) {
#define DRP_PROFILE_IDLE_BEGIN(scope, label) {
#define DRP_PROFILE_END(scope) }
#endif
#endif
