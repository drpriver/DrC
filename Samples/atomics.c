// Atomic operations demo: shows all the atomic builtins.
//   Bin/cc Samples/atomics.c

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

enum { NTHREADS = 4, ITERS = 100000 };

// --- fetch_add / fetch_sub ---
long add_counter = 0;
long sub_counter = NTHREADS * ITERS;

void* add_sub_worker(void* arg){
    for(int i = 0; i < ITERS; i++){
        __atomic_fetch_add(&add_counter, 1, __ATOMIC_SEQ_CST);
        __atomic_fetch_sub(&sub_counter, 1, __ATOMIC_SEQ_CST);
    }
    return NULL;
}

// --- compare_exchange (lock-free stack) ---
typedef struct Node Node;
struct Node {
    int value;
    Node* next;
};

Node* stack_head = NULL;

void stack_push(int value){
    Node* n = malloc(sizeof *n);
    n->value = value;
    n->next = __atomic_load_n(&stack_head, __ATOMIC_RELAXED);
    while(!__atomic_compare_exchange_n(&stack_head, &n->next, n, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED))
        ; // retry with updated n->next
}

int stack_pop(int* out){
    Node* head = __atomic_load_n(&stack_head, __ATOMIC_ACQUIRE);
    while(head){
        if(__atomic_compare_exchange_n(&stack_head, &head, head->next, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED)){
            *out = head->value;
            free(head);
            return 1;
        }
    }
    return 0;
}

void* stack_worker(void* arg){
    int id = (int)(long)arg;
    for(int i = 0; i < 1000; i++)
        stack_push(id * 1000 + i);
    return NULL;
}

// --- exchange (hand-off between threads) ---
long mailbox = 0;

void* producer(void* arg){
    for(int i = 1; i <= 10; i++){
        while(__atomic_load_n(&mailbox, __ATOMIC_ACQUIRE) != 0)
            ;
        __atomic_store_n(&mailbox, i, __ATOMIC_RELEASE);
    }
    return NULL;
}

void* consumer(void* arg){
    long sum = 0;
    for(int i = 0; i < 10; i++){
        long val;
        while((val = __atomic_exchange_n(&mailbox, 0, __ATOMIC_ACQ_REL)) == 0)
            ;
        sum += val;
    }
    return (void*)sum;
}

int main(){
    pthread_t threads[NTHREADS];

    // 1. fetch_add / fetch_sub
    for(int i = 0; i < NTHREADS; i++)
        pthread_create(&threads[i], NULL, add_sub_worker, NULL);
    for(int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);
    printf("fetch_add counter: %ld (expected %d)\n", add_counter, NTHREADS * ITERS);
    printf("fetch_sub counter: %ld (expected 0)\n", sub_counter);

    // 2. Lock-free stack via compare_exchange
    for(int i = 0; i < NTHREADS; i++)
        pthread_create(&threads[i], NULL, stack_worker, (void*)(long)i);
    for(int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);
    int count = 0;
    int val;
    while(stack_pop(&val))
        count++;
    printf("stack items:       %d (expected %d)\n", count, NTHREADS * 1000);

    // 3. Producer-consumer via exchange
    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer, NULL);
    pthread_create(&cons, NULL, consumer, NULL);
    pthread_join(prod, NULL);
    void* result;
    pthread_join(cons, &result);
    printf("exchange sum:      %ld (expected 55)\n", (long)result);

    // 4. Fences
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    printf("fences:            ok\n");

    return 0;
}
