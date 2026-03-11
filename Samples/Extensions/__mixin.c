// __mixin + _Type: compile-time code generation with type introspection
//
// Procmacros can take _Type parameters to inspect type properties
// and generate specialized code via __mixin.

#include <stdio.h>
#include <stdlib.h>

// Example 1: Auto-generated struct printer
//
// Uses _Type.fields and _Type.field(i) to iterate struct members and
// generate a print function with the right format for each field.

const char* fmtspec(_Type T){
    if(T == (const char*)) return "%s";
    if(T.is_float) return "%g";
    if(T.is_pointer) return "%p";
    if(T.is_integer){
        if(T.is_unsigned)
            return T.sizeof_ == 8 ? "%llu" : "%u";
        return T.sizeof_ == 8 ? "%lld" : "%d";
    }
    return "<?>";
}
#pragma procmacro fmtspec

const char* gen_print(_Type T){
    if(!T.is_struct) return "";
    char buf[4096];
    int off = 0;
    _Type str = const char*;
    off += snprintf(buf+off, sizeof buf-off,
        "(%s).push_method(print, void (%s* v){\n"
        "    printf(\"%s {\\n\");\n",
        T.name, T.name, T.name);
    for(int i = 0; i < (int)T.fields; i++){
        auto f = T.field(i);
        _Type ft = f.type;
        const char* name = f.name;
        const char* fmt = (fmtspec)(ft);
        off += snprintf(buf+off, sizeof buf-off,
            "    printf(\"    %s = %s\\n\", v->%s);\n",
            name, fmt, name);
    }
    off += snprintf(buf+off, sizeof buf-off,
        "    printf(\"}\\n\");\n"
        "});\n");
    return __builtin_intern(buf);
}
#pragma procmacro gen_print

struct Player {
    const char* name;
    int hp;
    double x, y;
};

// pushes method into Player type
__mixin(gen_print(struct Player));

printf("--- auto-generated struct print ---\n");
struct Player p1 = {"Alice", 100, 3.5, 7.2};
struct Player p2 = {"Bob", 42, -1.0, 0.5};
p1.print();
p2.print();

// Example 2: Generic dynamic array
//
// Generates a Vec struct with push, pop and free, parameterized by
// element type.

const char* Vec(_Type T){
    const char* n = T.name;
    char buf[4096];
    int off = 0;
    off += snprintf(buf+off, sizeof buf-off,
        "typedef struct %s_Vec %s_Vec;\n"
        "struct %s_Vec {\n"
        "    %s* data; int len, cap;\n"
        "    void push(%s_Vec* self, %s v){\n"
        "        if(self->len >= self->cap){\n"
        "            self->cap = self->cap ? self->cap*2 : 4;\n"
        "            self->data = realloc(self->data, self->cap * sizeof(%s));\n"
        "        }\n"
        "        self->data[self->len++] = v;\n"
        "    }\n"
        "    %s pop(%s_Vec* self){ return self->data[--self->len]; }\n"
        "    void free(%s_Vec* self){\n"
        "        free(self->data); self->data = 0; self->len = self->cap = 0;\n"
        "    }\n"
        "};\n",
        n, n, n, n, n, n, n, n, n, n);
    return __builtin_intern(buf);
}
#pragma procmacro Vec

__mixin(Vec(int));
__mixin(Vec(double));

printf("\n--- generic Vec ---\n");

int_Vec iv = {0};
for(int i = 0; i < 5; i++)
    iv.push(i * i);

printf("ints:");
for(int i = 0; i < iv.len; i++)
    printf(" %d", iv.data[i]);
printf("\n");
printf("popped: %d\n", iv.pop());
iv.free();

double_Vec dv = {0};
dv.push(3.14);
dv.push(2.72);
dv.push(1.41);

printf("doubles:");
for(int i = 0; i < dv.len; i++)
    printf(" %g", dv.data[i]);
printf("\n");
dv.free();
