//
// Demonstrates __get, __set, __append
// Different parts of the program can append to a keyed token list
// which can be retrieved with __get macro.
//
#include <stdio.h>
// Print any variable, with new types being able to register
// how to print.
#define PRINT(x) (fprintf(stdout, "%s = ", #x), _Generic(x, __get(printers))(x, stdout), fprintf(stdout, "\n"))
__append(printers, struct {}: NULL) // matches nothing

typedef struct Foo Foo;
struct Foo {
    int x, y;
};
void print_foo(Foo* f, FILE* fp){
    fprintf(fp, "(Foo){%d, %d}", f->x, f->y);
}
__append(printers, , Foo*: print_foo )


void print_int(int x, FILE* fp){
    fprintf(fp, "%d", x);
}
__append(printers, , int:print_int)


void print_string(const char* s, FILE* fp){
    fprintf(fp, "\"%s\"", s);
}
__append(printers, , const char*: print_string)


Foo f = {1, 2};
int i = 3;
const char* txt = "hello";
PRINT(&f);
PRINT(i);
PRINT(txt);

