// Mutex locking demo: threads race to increment a shared counter.
// With locking the count is correct; without it, it drifts.
//   Bin/cc Samples/locking.c          # with locking
//   Bin/cc Samples/locking.c nolock   # without locking

#include <pthread.h>
#include <stdio.h>
#include <string.h>

enum { NTHREADS = 4, ITERS = 100000 };

const char* arg = __argv(1, "lock");
_Bool use_lock = strcmp(arg, "nolock") != 0;

long counter = 0;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t threads[NTHREADS];

for(int i = 0; i < NTHREADS; i++){
    pthread_create(&threads[i], NULL, void*(void* arg){
        for(int i = 0; i < ITERS; i++){
            if(use_lock) pthread_mutex_lock(&mtx);
            counter++;
            if(use_lock) pthread_mutex_unlock(&mtx);
        }
        return NULL;
    }, NULL);
}

for(int i = 0; i < NTHREADS; i++)
    pthread_join(threads[i], NULL);

printf("Locking: %s\n", use_lock ? "on" : "off");
printf("Counter: %ld (expected %d)\n", counter, NTHREADS * ITERS);
