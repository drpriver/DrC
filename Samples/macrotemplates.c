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

#define Optional(T) __ident("Optional(" S(T) ")")
#defblock OPTIONAL_DEF(T)
typedef struct Optional(T) Optional(T);
static if(__is_pointer(T)){
    struct Optional(T){
        T value;
        _Bool has_value(Optional(T)* self){ return self.value; }
        void set(Optional(T)* self, T val){ self.value = val; }
        void clear(Optional(T)* self){ self.value = NULL; }
        void print(Optional(T)* self, const char* prefix){
            if(!self.value) {
                printf("%s: null\n", prefix);
                return;
            }
            static if(__type_equals(typeof_unqual(*self.value), char)){
                printf("%s: '%s'\n", prefix, self.value);
            }
            else {
                _Static_assert(0, "TODO: how to print this type");
            }
        }
    };
}
else {
    struct Optional(T){
        T value;
        _Bool _has_value;
        _Bool has_value(Optional(T)* self){ return self._has_value; }
        void set(Optional(T)* self, T val){ self.value = val; self._has_value = 1;}
        void clear(Optional(T)* self){ self._has_value = 0;}
        void print(Optional(T)* self, const char* prefix){
            if(!self._has_value) {
                printf("%s: null\n", prefix);
                return;
            }
            static if(__type_equals(typeof_unqual(T), int)){
                printf("%s: %d\n", prefix, self.value);
            }
            else if(__type_equals(typeof_unqual(T), double)){
                printf("%s: %f\n", prefix, self.value);
            }
            else {
                _Static_assert(0, "TODO: how to print this type");
            }
        }
    };
}
#endblock
OPTIONAL_DEF(int);
OPTIONAL_DEF(const char*);
OPTIONAL_DEF(char*);

Optional(int) optint = {0};
optint.print("optint.value");
optint.set(3);
optint.print("optint.value");
optint.clear();
optint.print("optint.value");
printf("----\n");
Optional(const char*) optstr = {0};
optstr.print("optstr.value");
optstr.set("hello");
optstr.print("optstr.value");
optstr.clear();
optstr.print("optstr.value");
printf("----\n");
Optional(char*) optmutstr = {0};
optmutstr.print("optmutstr.value");
optmutstr.set("world");
optmutstr.print("optmutstr.value");
optmutstr.clear();
optmutstr.print("optmutstr.value");
