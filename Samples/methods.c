#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef struct DA DA;
struct DA {
    int* data;
    size_t count, capacity;
    int push(DA* self, int value){
        if(self.count >= self.capacity){
            size_t cap = self.capacity?2*self.capacity:2;
            void* p = realloc(self.data, cap * sizeof *self.data);
            if(!p) return 1;
            self.data = p;
            self.capacity = cap;
        }
        self.data[self.count++] = value;
        return 0;
    }
    void dump(const DA* self){
        printf("[");
        for(size_t i = 0; i < self.count; i++){
            printf(i ?", %d":"%d", self.data[i]);
        }
        printf("]\n");
    }
    void clear(DA*self){
        self.count = 0;
    }
    void destroy(DA* self){
        if(self.data) free(self.data);
        memset(self, 0, sizeof *self);
    }
};
typedef struct Wrapper Wrapper;
struct Wrapper {
    DA da;
};

typedef struct W2 W2;
struct W2 {
    DA; // Plan9 struct
};

int err;
DA da = {0};
da.dump();
for(int i = 0; i < 10; i++){
    err = da.push(i);
    if(err) return err;
}
da.dump();
Wrapper w = {0};
w.da.dump();
W2 w2 = {0};
w2.push(1337);
w2.push(6969);
w2.dump();

DA* dp = &da;
dp.push(99);
dp.dump();

da.clear();
da.dump();
da.push(13);
da.dump();
da.destroy();
da.dump();
