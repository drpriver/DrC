#include <pthread.h>
#include <stdio.h>

typedef struct Job Job;
struct Job {
    int id;
};
enum {NTHREADS=4};
pthread_t threads[NTHREADS];
Job jobs[NTHREADS];
for(int i = 0; i < NTHREADS; i++){
    jobs[i].id = i;
}
void thread(int i, void* (*fn)(void*)){
    pthread_create(&threads[i], NULL, fn, &jobs[i]);
}

for(int i = 0; i < NTHREADS; i++){
    thread(i, void*(void* arg){
        typeof(&jobs[i]) j = arg;
        for(int i = 0; i < 5; i++){
            printf("Hello from %d (pthread_self: %p)\n", j->id, (void*)pthread_self());
            sched_yield();
        }
        return NULL;
    });
}
for(int i = 0; i < NTHREADS; i++){
    pthread_join(threads[i], NULL);
}
