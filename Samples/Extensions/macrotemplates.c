//
// Combines methods, defblock, __ident, static if
// to generate something that looks like templates if you squint.
//
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#pragma typedef on // C++ style auto typedef
#define S_(x) #x
#define S(x) S_(x)
#define DA(T) __ident("DA(" S(T) ")") // unique name for any type.
#defblock DA_DEF(T) // multiline macro
struct DA(T) {
    T* data;
    size_t count, capacity;

    // Methods
    int push(_Self *self, T value){
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
    int push_ref(_Self *self, T* value){
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
    void clear(_Self *self){
        self.count = 0;
    }
    void destroy(_Self *self){
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
    printf("ints[%zu] = %d\n", i, ints.data[i]);
}

for(size_t i = 0; i < strings.count; i++){
    printf("strings[%zu] = '%s'\n", i, strings.data[i]);
}

#define Optional(T) __ident("Optional(" S(T) ")")
#defblock OPTIONAL_DEF(T)
// static if provides introspection
static if(T.is_pointer){
    struct Optional(T){
        T value;
        _Bool has_value(_Self* self){ return self.value; }
        void set(_Self* self, T val){ self.value = val; }
        void clear(_Self* self){ self.value = NULL; }
        void print(_Self* self, const char* prefix){
            if(!self.value) {
                printf("%s: null\n", prefix);
                return;
            }
            static if(typeof_unqual(*self.value) == char){
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
        _Bool has_value(_Self* self){ return self._has_value; }
        void set(_Self* self, T val){ self.value = val; self._has_value = 1;}
        void clear(_Self* self){ self._has_value = 0;}
        void print(_Self* self, const char* prefix){
            if(!self._has_value) {
                printf("%s: null\n", prefix);
                return;
            }
            static if(typeof_unqual(T) == int){
                printf("%s: %d\n", prefix, self.value);
            }
            else if(typeof_unqual(T) == double){
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
