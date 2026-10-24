#ifndef DRP_ENV_H
#define DRP_ENV_H
#include <stddef.h>
#include <string.h>
#include "atom_table.h"
#include "Allocators/allocator.h"
#include "cstring_view.h"
#include "ckdint.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else

#ifndef _Nullable
#define _Nullable
#endif

#ifndef _Null_unspecified
#define _Null_unspecified
#endif

#endif

typedef struct EnvKV EnvKV;
struct EnvKV {
    Atom key; // will be lower-cased if case_insensitive
    Atom original_key; // for case-preserving
    Atom value;
};

typedef struct Environment Environment;
struct Environment {
    Allocator allocator;
    AtomTable* at;
    EnvKV* data;
    uint32_t cap;
    uint32_t count;
    _Bool windows;
};


static int env_setenv(Environment* env, Atom key, Atom value);
static int env_setenv4(Environment* env, const char* key, size_t keylen, const char* value, size_t valuelen);
static Atom _Nullable env_getenv(Environment* env, Atom key);
static Atom _Nullable env_getenv2(Environment* env, const char* key, size_t keylen);
static int env_parse_posix(Environment* env, char*_Null_unspecified*_Null_unspecified envp);
static int env_parse_win32(Environment* env, char*_Nullable envp);
static Atom _Nullable _env_fold_key(Environment* env, Atom key);
static char*_Null_unspecified*_Nullable env_to_posix(Environment* env, Allocator a, size_t* alloc_size, const StringView2* _Null_unspecified, size_t);
static char*_Nullable env_to_win32(Environment* env, Allocator a, size_t* alloc_size, const StringView2* _Null_unspecified, size_t);

static
int
env_parse_posix(Environment* env, char*_Null_unspecified*_Null_unspecified envp){
    if(!envp) return 0;
    for(char** p = envp; *p; p++){
        const char* eq = strchr(*p, '=');
        if(!eq) continue;
        int err = env_setenv4(env, *p, eq - *p, eq+1, strlen(eq+1));
        if(err) return err;
    }
    return 0;
}

static
int
env_parse_win32(Environment* env, char*_Nullable envp){
    if(!envp) return 0;
    for(char* p = envp;;){
        size_t len = strlen(p);
        if(!len) break;
        // Drive working-directory entries have keys such as "=C:".
        char* eq = strchr(p+(*p == '='), '=');
        if(eq){
            int err = env_setenv4(env, p, eq-p, eq+1, p+len-eq-1);
            if(err) return err;
        }
        p += len+1;
    }
    return 0;
}

static
int
env_setenv4(Environment* env, const char* key, size_t keylen, const char* value, size_t valuelen){
    Atom k = AT_atomize(env->at, key, keylen);
    if(!k) return 1;
    Atom v = AT_atomize(env->at, value, valuelen);
    if(!v) return 1;
    return env_setenv(env, k, v);
}

static
Atom _Nullable
_env_fold_key(Environment* env, Atom key){
    if(!env->windows) return key;
    for(size_t i = 0; i < key->length; i++){
        char c = key->data[i];
        if(c >= 'A' && c <= 'Z')
            goto lower;
    }
    return key;
    lower:;
    char* tmp = Allocator_alloc(env->allocator, key->length, 1, 1);
    for(size_t i = 0; i < key->length; i++){
        char c = key->data[i];
        if(c >= 'A' && c <= 'Z')
            tmp[i] = c | 0x20;
        else
            tmp[i] = c;
    }
    Atom k = AT_atomize(env->at, tmp, key->length);
    Allocator_free(env->allocator, tmp, key->length, 1, 1);
    return k;
}

static
int
env_setenv(Environment* env, Atom original_key, Atom value){
    Atom key = _env_fold_key(env, original_key);
    if(!key) return 1;
    if(env->count == env->cap){
        uint32_t old_cap = env->cap;
        uint32_t cap = 32, index_count;
        if(old_cap && mul_overflow(old_cap, 2u, &cap))
            return 1;
        if(mul_overflow(cap, 2u, &index_count))
            return 1;
        size_t sz;
        if(mul_overflow((size_t)cap, sizeof(EnvKV) + 2*sizeof(uint32_t), &sz))
            return 1;
        size_t old_sz = (size_t)old_cap * (sizeof(EnvKV) + 2*sizeof(uint32_t));
        void* p = Allocator_realloc(env->allocator, env->data, old_sz, sz, 1, _Alignof(EnvKV));
        if(!p) return 1;
        EnvKV* items = p;
        uint32_t* idxes = (uint32_t*)((char*)p+cap*sizeof *items);
        memset(idxes, 0, 2*cap * sizeof *idxes);
        for(uint32_t i = 0; i < env->count; i++){
            EnvKV kv = items[i];
            uint32_t hash = kv.key->hash;
            uint32_t idx = fast_reduce32(hash, cap);
            while(idxes[idx]){
                idx++;
                if(idx > 2*cap) idx = 0;
            }
            idxes[idx] = i+1;
        }
        env->data = p;
        env->cap = cap;
    }
    uint32_t cap = env->cap;
    uint32_t hash = key->hash;
    uint32_t idx = fast_reduce32(hash, cap);
    uint32_t i;
    EnvKV* items = env->data;
    uint32_t* idxes = (uint32_t*)((char*)env->data + cap*sizeof *items);
    while((i = idxes[idx])){
        EnvKV* item = &items[i-1];
        if(item->key == key){
            item->original_key = original_key;
            item->value = value;
            return 0;
        }
        idx++;
        if(idx > 2 * cap) idx = 0;
    }
    i = env->count++;
    idxes[idx] = i+1;
    items[i] = (EnvKV){
        .key = key,
        .original_key = original_key,
        .value = value,
    };
    return 0;
}
static
Atom _Nullable
env_getenv2(Environment* env, const char* key, size_t keylen){
    Atom k;
    if(!env->windows)
        k = AT_get_atom(env->at, key, keylen);
    else
        k = AT_atomize(env->at, key, keylen);
    if(!k) return NULL;
    return env_getenv(env, k);
}

static Atom
_Nullable
env_getenv(Environment* env, Atom key){
    key = (Atom)_env_fold_key(env, key);
    uint32_t cap = env->cap;
    uint32_t hash = key->hash;
    uint32_t idx = fast_reduce32(hash, cap);
    uint32_t i;
    const EnvKV* items= env->data;
    if(!env->count) return NULL;
    uint32_t* idxes = (uint32_t*)((char*)env->data + cap*sizeof *items);
    while((i = idxes[idx])){
        const EnvKV* item = &items[i-1];
        if(item->key == key){
            return item->value;
        }
        idx++;
        if(idx > 2*cap) idx = 0;
    }
    return NULL;
}

static
_Bool
_env_output_kv(Environment* env, const StringView2* _Null_unspecified override, size_t override_count, size_t i, StringView2* out){
    size_t start;
    if(i < env->count){
        EnvKV kv = env->data[i];
        Atom key = env->windows ? kv.original_key : kv.key;
        *out = (StringView2){{key->length, key->data}, {kv.value->length, kv.value->data}};
        start = 0;
    }
    else {
        i -= env->count;
        *out = override[i];
        start = i+1;
    }
    if(env->windows)
        for(size_t j = start; j < override_count; j++){
            if(sv_iequals(out->key, override[j].key)) return 0;
        }
    else
        for(size_t j = start; j < override_count; j++){
            if(sv_equals(out->key, override[j].key)) return 0;
        }
    return 1;
}

static
char*_Null_unspecified*_Nullable
env_to_posix(Environment* env, Allocator a, size_t* alloc_size, const StringView2* _Null_unspecified override, size_t override_count){
    size_t total;
    if(add_overflow((size_t)env->count, override_count, &total))
        return NULL;
    size_t nkvs = 1; // +1 for NULL terminator
    size_t strsize = 0;
    for(size_t i = 0; i < total; i++){
        StringView2 kv;
        if(!_env_output_kv(env, override, override_count, i, &kv)) continue;
        if(add_overflow(nkvs, (size_t)1, &nkvs)) return NULL;
        if(add_overflow(strsize, kv.key.length, &strsize)) return NULL;
        if(add_overflow(strsize, kv.value.length, &strsize)) return NULL;
        if(add_overflow(strsize, (size_t)2, &strsize)) return NULL;
    }
    size_t sz;
    if(fma_overflow(nkvs, sizeof(char*), strsize, &sz))
        return NULL;
    void* p = Allocator_alloc(a, sz, 1, _Alignof(char*));
    if(!p) return NULL;
    *alloc_size = sz;
    char** envp = p;
    char* s = (char*)p+nkvs*sizeof(char*);
    size_t n = 0;
    for(size_t i = 0; i < total; i++){
        StringView2 kv;
        if(!_env_output_kv(env, override, override_count, i, &kv)) continue;
        envp[n++] = s;
        memcpy(s, kv.key.text, kv.key.length);
        s += kv.key.length;
        s++[0] = '=';
        memcpy(s, kv.value.text, kv.value.length);
        s += kv.value.length;
        s++[0] = '\0';
    }
    envp[n] = NULL;
    return envp;
}

static
char*_Nullable
env_to_win32(Environment* env, Allocator a, size_t* alloc_size, const StringView2* _Null_unspecified override, size_t override_count){
    size_t total;
    if(add_overflow((size_t)env->count, override_count, &total))
        return NULL;
    size_t strsize = 1; // double nul terminator
    for(size_t i = 0; i < total; i++){
        StringView2 kv;
        if(!_env_output_kv(env, override, override_count, i, &kv)) continue;
        //                        '='                   '\0'
        if(add_overflow(strsize, kv.key.length, &strsize)) return NULL;
        if(add_overflow(strsize, kv.value.length, &strsize)) return NULL;
        if(add_overflow(strsize, (size_t)2, &strsize)) return NULL;
    }
    if(strsize == 1) strsize = 2;
    void* envp = Allocator_alloc(a, strsize, 1, 1);
    if(!envp) return NULL;
    *alloc_size = strsize;
    char* s = envp;
    for(size_t i = 0; i < total; i++){
        StringView2 kv;
        if(!_env_output_kv(env, override, override_count, i, &kv)) continue;
        memcpy(s, kv.key.text, kv.key.length);
        s += kv.key.length;
        s++[0] = '=';
        memcpy(s, kv.value.text, kv.value.length);
        s += kv.value.length;
        s++[0] = '\0';
    }
    s[0] = '\0'; // final double nul terminator
    if(s == envp) s[1] = '\0';
    return envp;
}

static
void*_Nullable
env_to_envp(Environment* env, Allocator a, size_t* alloc_size, const StringView2*_Null_unspecified overrides, size_t override_count){
    if(env->windows)
        return env_to_win32(env, a, alloc_size, overrides, override_count);
    else
        return env_to_posix(env, a, alloc_size, overrides, override_count);
}

static
void
env_free_envp(const Environment* env, Allocator a, const void*_Nullable envp, size_t alloc_size){
    Allocator_free(a, envp, alloc_size, 1, env->windows ? 1 : _Alignof(char*));
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
