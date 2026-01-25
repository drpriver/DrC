#ifndef DRP_ENV_H
#define DRP_ENV_H
#include <stddef.h>
#include <string.h>
#include "atom_table.h"
#include "Allocators/allocator.h"

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
static int env_parse_win32(Environment* env, char* envp);
static Atom _Nullable _env_fold_key(Environment* env, Atom key);
static char*_Null_unspecified*_Nullable env_to_posix(Environment* env, Allocator a, size_t* alloc_size);
static char*_Nullable env_to_win32(Environment* env, Allocator a, size_t* alloc_size);

static
int
env_parse_posix(Environment* env, char*_Null_unspecified*_Null_unspecified envp){
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
env_parse_win32(Environment* env, char* envp){
    for(char* p = envp;;){
        size_t len = strlen(p);
        if(!len) break;
        char* eq = strchr(p, '=');
        int err = env_setenv4(env, p, eq-p, eq+1, p+len-eq-1);
        if(err) return err;
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
    char* tmp = Allocator_alloc(env->allocator, key->length);
    for(size_t i = 0; i < key->length; i++){
        char c = key->data[i];
        if(c >= 'A' && c <= 'Z')
            tmp[i] = c | 0x20;
        else
            tmp[i] = c;
    }
    Atom k = AT_atomize(env->at, tmp, key->length);
    Allocator_free(env->allocator, tmp, key->length);
    return k;
}

static
int
env_setenv(Environment* env, Atom original_key, Atom value){
    Atom key = _env_fold_key(env, original_key);
    if(!key) return 1;
    if(env->count == env->cap){
        uint32_t old_cap = env->cap;
        uint32_t cap = old_cap?2*old_cap:32;
        size_t old_sz = old_cap * sizeof(EnvKV) + 2*old_cap*sizeof(uint32_t);
        size_t sz = cap * sizeof(EnvKV) + 2*cap*sizeof(uint32_t);
        void* p = Allocator_realloc(env->allocator, env->data, old_sz, sz);
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
char*_Null_unspecified*_Nullable
env_to_posix(Environment* env, Allocator a, size_t* alloc_size){
    size_t nkvs = env->count+1; // +1 for NULL terminator
    size_t strsize = 0;
    for(size_t i = 0; i < env->count; i++){
        EnvKV kv = env->data[i];
        //                         '='                   '\0'
        strsize += kv.key->length + 1 + kv.value->length + 1;
    }
    size_t sz = nkvs * sizeof(char*) + strsize;
    void* p = Allocator_alloc(a, sz);
    if(!p) return NULL;
    *alloc_size = sz;
    char** envp = p;
    char* s = (char*)p+nkvs*sizeof(char*);
    for(size_t i = 0; i < env->count; i++){
        EnvKV kv = env->data[i];
        envp[i] = s;
        memcpy(s, kv.key->data, kv.key->length);
        s += kv.key->length;
        s++[0] = '=';
        memcpy(s, kv.value->data, kv.value->length);
        s += kv.value->length;
        s++[0] = '\0';
    }
    envp[env->count] = NULL;
    return envp;
}

static 
char*_Nullable
env_to_win32(Environment* env, Allocator a, size_t* alloc_size){
    size_t strsize = 1; // double nul terminator
    for(size_t i = 0; i < env->count; i++){
        EnvKV kv = env->data[i];
        //                                  '='                   '\0'
        strsize += kv.original_key->length + 1 + kv.value->length + 1;
    }
    if(!env->count) strsize = 2;
    void* envp = Allocator_alloc(a, strsize);
    if(!envp) return NULL;
    *alloc_size = strsize;
    char* s = envp;
    for(size_t i = 0; i < env->count; i++){
        EnvKV kv = env->data[i];
        memcpy(s, kv.original_key->data, kv.original_key->length);
        s += kv.original_key->length;
        s++[0] = '=';
        memcpy(s, kv.value->data, kv.value->length);
        s += kv.value->length;
        s++[0] = '\0';
    }
    s[0] = '\0'; // final double nul terminator
    if(!env->count) s[1] = '\0';
    return envp;
}

static
void*_Nullable
env_to_envp(Environment* env, Allocator a, size_t* alloc_size){
    if(env->windows)
        return env_to_win32(env, a, alloc_size);
    else
        return env_to_posix(env, a, alloc_size);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
