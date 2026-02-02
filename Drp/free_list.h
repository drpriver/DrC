#ifndef DRP_FREE_LIST_H
#define DRP_FREE_LIST_H
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

typedef struct FreeList FreeList;
struct FreeList {
    void *_Nullable list;
};

#define FreeList(T) FreeList

static
void
fl_push(FreeList *fl, void *ptr){
    *(void**)ptr = fl->list;
    fl->list = ptr;
}

static
void*_Nullable
fl_pop(FreeList* fl){
    void *result = fl->list;
    if(result) fl->list = *(void**)result;
    return result;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
