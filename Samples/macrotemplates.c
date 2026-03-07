#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#define S_(x) #x
#define S(x) S_(x)
#define DA(T) __ident("DA." S(T))
#defblock DA_DEF(T)
typedef struct DA(T) DA(T);
struct DA(T) {
    T* data;
    size_t count, capacity;
    int push(DA(T) *self, T value){
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
    int push_ref(DA(T) *self, T* value){
        if(self.count >= self.capacity){
            size_t cap = self.capacity?2*self.capacity:2;
            void* p = realloc(self.data, cap * sizeof *self.data);
            if(!p) return 1;
            self.data = p;
            self.capacity = cap;
        }
        self.data[self.count++] = *value;
        return 0;
    }
    void clear(DA(T) *self){
        self.count = 0;
    }
    void destroy(DA(T) *self){
        if(self.data) free(self.data);
        memset(self, 0, sizeof *self);
    }
}
#endblock

DA_DEF(int);
DA_DEF(const char*);

DA(int) ints = {0};
ints.push(1);
ints.push(2);
ints.push(3);
int x = 17;
ints.push_ref(&x);

DA(const char*) strings = {0};
strings.push("hello");
strings.push("world");
strings.push("!");

for(size_t i = 0; i < ints.count; i++){
    printf("ints[%d] = %d\n", i, ints.data[i]);
}

for(size_t i = 0; i < strings.count; i++){
    printf("strings[%d] = '%s'\n", i, strings.data[i]);
}
