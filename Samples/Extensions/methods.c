#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#pragma typedef on
struct DA {
    int* data;
    size_t count, capacity;
    int push(_Self* self, int value){
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
    void dump(const _Self* self){
        printf("[");
        for(size_t i = 0; i < self.count; i++){
            printf(i ?", %d":"%d", self.data[i]);
        }
        printf("]\n");
    }
    void clear(_Self*self){
        self.count = 0;
    }
    void destroy(_Self* self){
        if(self.data) free(self.data);
        memset(self, 0, sizeof *self);
    }
};


DA da = {0};
da.dump(); // []
for(int i = 0; i < 10; i++)
    da.push(i);
da.dump(); // [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
struct Wrapper { DA da; };
Wrapper w = {0};
w.da.dump(); // []

DA* dp = &da;
dp.clear();
da.dump(); // []
dp.push(99); 
dp.dump(); // [99]
da.dump(); // [99]

struct W2 {
    int pre;
    DA; // Plan9 struct
    int post;
};

W2 w2 = {0};
w2.push(1337);
w2.push(6969);
w2.dump(); // [1337, 6969]

// Plan9 implicit conversion
dp = &w2;
dp.clear();
dp.push(12);
dp.push(13);
dp.dump(); // [12, 13]
w2.dump(); // [12, 13]
struct { DA; } w3;
w3.push(42);
w3.dump(); // [42]

struct FakeLock {
    bool locked;
    void lock(_Self* self){
        if(self.locked){
            printf("oh noes! deadlock!\n");
        }
        self.locked = 1;
    }
    void unlock(_Self* self){
        if(!self.locked){
            printf("oh noes! unlocked an unlocked lock!\n");
        }
        self.locked = 0;
    }
};

struct FatObject {
    int _x, _y;
    FakeLock;
    void mutate(_Self* self){
        self.lock();
        self._x++;
        self._y++;
        self.unlock();
    }
    int add(_Self* self){
        self.lock();
        int result = self._x + self._y;
        self.unlock();
        return result;
    }
    void dump(_Self* self){
        printf("locked: %s, x: %d, y: %d\n", self.locked?"true":"false", self._x, self._y);
    }
};
FatObject fo = {._x=1, ._y=2};
fo.dump(); // locked: false, x: 1, y: 2
fo.mutate();
fo.dump(); // locked: false, x: 2, y: 3
fo.mutate();
fo.dump(); // locked: false, x: 3, y: 4
printf("%d\n", fo.add()); // 7

// Methods can be introspected like fields.
// statically
static if(FatObject.has_method("lock") && FatObject.has_method("unlock")){
    fo.lock();
    fo._x++;
    fo._y++;
    fo.unlock();
    fo.dump(); // locked: false, x: 4, y: 5
}
// or at runtime
_Any obj = &fo;
if(obj.type.is_pointer){
    _Type T = obj.type.pointee;
    if(T.has_method("mutate")){
        __builtin_Method m = T.method("mutate");
        if(m.type.is_callable_through(void(void*))){
            void* receiver = obj.as(char*) + m.offset;
            ((void(*)(void*))m.address)(receiver);
        }
    }
    if(T.has_method("dump")){
        __builtin_Method m = T.method("dump");
        if(m.type.is_callable_through(void(void*))){
            void* receiver = obj.as(char*) + m.offset;
            ((void(*)(void*))m.address)(receiver); // locked: false, x: 5, y: 6
        }
    }
}
