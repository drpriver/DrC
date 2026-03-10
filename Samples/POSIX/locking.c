// Locking demo: threads race to increment a shared counter.
// Compares: no locking (broken), mutex (correct, slow), atomics (correct, fast).
//   Bin/cc Samples/locking.c          # mutex
//   Bin/cc Samples/locking.c nolock   # no locking (racy)
//   Bin/cc Samples/locking.c atomic   # atomic increment

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

enum { NTHREADS = 4, ITERS = 100000 };

const char* mode = __argv(1, "mutex");
_Bool use_mutex = 0;
_Bool use_atomic = 0;
if(use_mutex = (strcmp(mode, "mutex") == 0)){ }
else if(use_atomic = (strcmp(mode, "atomic") == 0)){ }
else { mode = "unsynchronized"; }

long counter = 0;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t threads[NTHREADS];

for(int i = 0; i < NTHREADS; i++){
    pthread_create(&threads[i], NULL, void*(void* arg){
        for(int i = 0; i < ITERS; i++){
            if(use_atomic){
                atomic_fetch_add(&counter, 1);
            }
            else if(use_mutex){
                pthread_mutex_lock(&mtx);
                counter++;
                pthread_mutex_unlock(&mtx);
            }
            else {
                counter++;
            }
        }
        return NULL;
    }, NULL);
}

for(int i = 0; i < NTHREADS; i++)
    pthread_join(threads[i], NULL);

printf("Mode: %s\n", mode);
printf("Counter: %ld (expected %d)\n", counter, NTHREADS * ITERS);
