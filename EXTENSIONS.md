# Extensions

This C interpreter aims to be able to parse all valid C2y C programs, but
also has its own fun extensions to make programming in C more fun and
ergonomic.

## Preprocessor

### GNU Extensions
Technically extensions, but these are commonly implemented for gnu compatibility.

#### Named variadics

The cpp supports GNU named variadic parameters to support pre-historic C code.

```C
#define LOG(fmt, args...) fprintf(stderr, fmt, args)
// equivalent to
#define LOG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
```

#### `#include_oneof`

If you've ever written code like:
```C
#if __has_include(<Foo/foo.h>)
#include <Foo/foo.h>
#elif __has_include(<FooLib/foo.h>)
#include <FooLib/foo.h>
#elif __has_include(<foo.h>)
#include <foo.h>
....
```
then this is for you.

The above can be replaced with just
```C
#include_oneof <Foo/foo.h> <Foolib/foo.h> <foo.h>
```
Each one will be tried in sequence, emitting an error if none can be found.

Actually, `#include` also works this way now, get wrecked standard committee.

#### `, ## __VA_ARGS__`
The comma is deleted if `__VA_ARGS__` is empty.

### `__VA_COUNT__`
This special token expands in a variadic macro the number of args passed in the variadic slot.

This makes it a lot easier to overload by arity if you need that

```C
#include <stdio.h>
#define X(...) printf("%s:%d: __VA_COUNT__(%s) -> %d args\n", __FILE__, __LINE__, #__VA_ARGS__, __VA_COUNT__)
X(a, b, c);
X(1);
X();
```

### `__VA_ARG__(n)`
This special macro extracts the nth variadic argument from a preprocessor macro.
The index expression can itself use preprocessor arithmetic.

```C
#define FIRST(...) __VA_ARG__(0)
#define LAST(...)  __VA_ARG__(__VA_COUNT__ - 1)
FIRST(a, b, c) // -> a
LAST(a, b, c)  // -> c
```


### `defblock`

Multiline macros can be ergonomically defined using `#defblock` instead of
`#define`. `#defblock` is terminated by the next `#endblock`. Preprocessor
directives within a defblock is undefined behavior (as the current
implementation does not account for `#defblock` when scanning for `#endif` for
example.

It otherwise follows the same rules as regular `#define` in regards to the
stringify and paste operators, `__VA_ARGS__`, etc.

```C
#include <stdio.h>
#include <stdlib.h>
#defblock ASSERT(x)
// Look ma, C++ style comments within a macro!
do {
    if(!x){
        fprintf(stderr, "%s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #x);
        abort();
    }
}while(0)
#endblock
ASSERT(1 == 1);
ASSERT(1 == 0);
```

### `__map(macro, ...)`
`__map()` (also spelled `__MAP__()`) applies the function-like macro in its first argument to each arg in its variadic list.

```C
#define S(x) #x,
const char* names[] = { __map(S, foo, bar, baz) };
// -> const char* names[] = { "foo", "bar", "baz", };
```

### `__eval(pp-expression)`
`__eval()` (also spelled `__EVAL__()`) evaluates the pp-expression using the same rules as in `#if` and returns a pp-number that is the result of the expansion. This is useful for token-pasting.

```C
// Expressions are evaluated and replaced with a single pp-number token,
// so it can be used with the paste operator:
#define CONCAT(a, b) a ## b
CONCAT(x, __eval(1 + 1)) // -> x2
```

### `__mixin(strings...)`
`__mixin()` (also spelled `__MIXIN__()`), concatenates the given string
literals and parses them into preprocessor tokens. This allows converting
strings into code.

```C
#define S(x) #x
__mixin("int x = 42;") // -> int x = 42;
```

### `__env("key" [, fallback])`
`__env()` (also spelled `__ENV__()`), reads the given environment variable identified by the key (which must be a string), and if set it is replaced by that as a pp-string. If it is not set, fallback is used (which can be any preprocessor tokens). To de-stringify, you can use `__mixin()`.

```C
const char* home = __env("HOME");
const char* editor = __env("EDITOR", "vim");
```

### `__if(condition, then, else)`
`__if()` (also spelled `__IF__()`) is like `#if`-`#else`-`#endif`, but with a functional syntax.
Only the taken branch is expanded, so the other branch can contain invalid tokens.

```C
#define X 1
__if(X, "yes", __eval(1/0)) // -> "yes" (division by zero never evaluated)
__if(0, bad, good)           // -> good
```

### `__ident(strings...)`
`__ident()`, also spelled `__IDENT__()`, creates an identifier token that is formed by concating the given strings. This allows you to construct an identifier token that contains characters not normally allowed in identifiers, such as spaces or periods.

```C
#define S_(x) #x
#define S(x) S_(x)
#define DA(T) __ident("DA." S(T))

DA(int) // -> a single identifier token "DA.int"
```

### `__format(fmt, ...)`
`__format()` (also spelled `__FORMAT__()`) does `sprintf`-style formatting at
preprocessing time and returns a string literal.

```C
```

### `__print(...)`
`__print()` (also spelled `__PRINT__()`) prints a diagnostic message to stderr
during preprocessing, including the source location. Useful for debugging macros.

### `__set(name, ...)` / `__get(name)` / `__append(name, ...)`
Compile-time mutable variables for the preprocessor. `__set` assigns,
`__get` retrieves, and `__append` appends without overwriting. These are
stateful across macro invocations, enabling accumulation patterns.

```C
__set(n, 0)
#define FIELD(type, name) type name; __set(n, __eval(__get(n) + 1))
FIELD(int, x)
FIELD(float, y)
FIELD(char*, name)
__get(n) // -> 3
```

```C
#define REG(fn) __append(init_calls, fn();)
REG(init_a)
REG(init_b)
__get(init_calls) // -> init_a(); init_b();
```

### `__for(start, end, macro)`
`__for()` (also spelled `__FOR__()`) invokes `macro` with each integer from
`start` (inclusive) to `end` (exclusive).

```C
#define DECL(i) int x##i;
__for(0, 3, DECL) // -> int x0; int x1; int x2;
```

### `__let(binding, body)`
`__let()` (also spelled `__LET__()`) defines a scoped macro for the duration
of `body`. Supports both object-like and function-like bindings.

```C
__let(X, 42, X + X) // -> 42 + 42
__let(DBL(x), x * 2, DBL(5)) // -> 5 * 2
__let(MK(prefix), prefix##_init, MK(foo) MK(bar)) // -> foo_init bar_init
```

### `__where(name)`
`__where()` (also spelled `__WHERE__()`) emits a diagnostic showing where a
macro was defined. Useful for debugging which definition is active.

### `__RAND__`
`__RAND__` (also spelled `__RANDOM__`) expands to a random integer.


## C

### `static if`

Compile-time conditional at the language level. The condition must be a
constant expression. Unlike `#if`, this operates on C types and expressions
after parsing, so it works with `sizeof`, type queries, etc. Declarations in
the taken branch are injected into the enclosing scope (no new scope).

```C
```

Combos with type introspection:

```C
static if(__is_pointer(T)){
    // pointer specialization
}
else if(__type_equals(T, int)){
    // int specialization
}
else {
    _Static_assert(0, "unsupported type");
}
```

### Methods

Functions can be defined directly inside structs and unions. They do not affect the struct's size or layout. The first parameter
is the receiver. Methods are called with `.` syntax and the receiver is
passed automatically.

```C
struct DA {
    int* data;
    size_t count, capacity;
    int push(DA* self, int value){
        if(self.count >= self.capacity){
            size_t cap = self.capacity ? 2*self.capacity : 2;
            void* p = realloc(self.data, cap * sizeof *self.data);
            if(!p) return 1;
            self.data = p;
            self.capacity = cap;
        }
        self.data[self.count++] = value;
        return 0;
    }
    void dump(const DA* self){
        for(size_t i = 0; i < self.count; i++)
            printf("%d ", self.data[i]);
        printf("\n");
    }
};

DA da = {0};
da.push(42);   // self = &da
da.dump();
```

Anonymous structs and Plan9 structs export their methods.

### `.` and `->` interchangeable

The `.` and `->` operators are interchangeable. Using `.` on a pointer
automatically dereferences it, and using `->` on a non-pointer automatically
takes its address. This is especially natural for method calls.

```C
DA* dp = &da;
dp.push(99);   // equivalent to dp->push(99)
dp.dump();
```

### Plan9 struct embedding

An anonymous struct member (written as just a type name without a field name)
embeds that struct's fields into the enclosing struct. The embedded fields
can be accessed directly, designated initializers work through the embedding,
and pointers to the outer struct implicitly convert to pointers to the
embedded struct.

```C
struct Base { int x; int y; };
struct Derived { struct Base; int z; };

struct Derived d = {.x = 1, .y = 2, .z = 3}; // designated through embed
struct Derived d2 = {1, 2, 3};                // brace elision works too

struct Base* bp = &d; // implicit conversion
```

### Type introspection

Built-in operators for compile-time type queries. Each accepts either a type
name or an expression (in which case its type is used). These produce integer
constant expressions (0 or 1), so they work in `static if`, `_Static_assert`,
etc.

- `__type_equals(a, b)` — true if `a` and `b` are the same type
- `__is_pointer(a)` — true if `a` is a pointer type
- `__is_arithmetic(a)` — true if `a` is an arithmetic type
- `__is_const(a)` — true if `a` is const-qualified
- `__is_castable_to(a, b)` — true if `a` can be explicitly cast to `b`
- `__is_implicitly_castable_to(a, b)` — true if `a` implicitly converts to `b`
- `__has_quals(a, b)` — true if `a` has at least the qualifiers of `b`

```C
static if(__is_pointer(T)){
    // ...
}
static if(__type_equals(typeof_unqual(T), int)){
    // ...
}
```

### `_Type`

### Top-level statements

Code at the top level of a file is executed as if it were in an implicit
`main` function. This makes C work as a scripting language — no `main`
boilerplate needed.

```C
#include <stdio.h>
printf("hello world\n");
for(int i = 0; i < 10; i++)
    printf("%d\n", i);
```

If a `main` function is defined, it is used as the entry point instead.

### Named / numbered arguments

Function calls support designated-style syntax for passing arguments by name
or by position:

```C
int clamp(int value, int lo, int hi);
clamp(.hi = 100, .lo = 0, .value = 42);
clamp([2] = 100, [1] = 0, [0] = 42);
```

### Lambdas

Anonymous functions using `type(params){ body }` syntax. The return type
starts the expression, followed by the parameter list and a braced body.

```C
int r = int(int x, int y){ return x + y; }(3, 4); // r = 7

// passed as a callback
pthread_create(&t, NULL, void*(void* arg){
    printf("hello from thread\n");
    return NULL;
}, NULL);
```

### Nested functions

Functions can be defined inside other functions. They can reference the
enclosing function's types but do not capture variables (they are not
closures).

```C
void outer(void){
    int helper(int x){ return x * 2; }
    printf("%d\n", helper(21));
}
```


## Interpreter-only

These extensions are only available when running under the interpreter, not
when compiling to native code.

### Native FFI
Functions declared but not defined are automatically resolved against loaded
native libraries at call time using libffi.

This means you can write a script that `#include`s a library's headers and
immediately start using it.

### `__ARGC__` / `__argv(n [, default])`
`__ARGC__` expands to the number of command-line arguments passed to the
script. `__argv(n)` expands to the nth argument as a string literal, with
an optional default if the argument wasn't provided.

```C
const char* input = __argv(1, "default.txt");
```

### `__shell(program, args...)`
`__shell()` (also spelled `__SHELL__()`) executes a command during
preprocessing and expands to its stdout as a string literal.

### `#pragma lib "name"`
Link against a native shared library.

### `#pragma lib_path "path"`
Add a directory to the library search path.

### `#pragma pkg_config "package"`
Use `pkg-config` to find include paths and libraries for a package.

### `#pragma procmacro name`

Registers a previously defined C function as a preprocessor macro.
When invoked, the cpp tokens are expanded and converted to c tokens, then
parsed as actual C arguments. This means this macro has C semantics (it can see enums),
but because it runs in the preprocessor other functions are not defined yet and
cannot be called and global variables can't be referenced.

The return value is converted to a preprocessor token:
integers/floats become pp-numbers, `const char*` becomes a string literal,
`_Bool` becomes `true`/`false`, and `void` produces no output.

This lets you write arbitrary compile-time computation in plain C.

```C
unsigned long hash(const char* s){
    unsigned long h = 5381;
    while(*s)
        h = h * 33 + *s++;
    return h;
}
#pragma procmacro hash

// case labels must be constant expressions. The proc macro computes
// hash("quit") etc. at compile time, while (hash)(cmd) runs at runtime.
void handle(const char* cmd){
    switch((hash)(cmd)){
        case hash("quit"): exit(0);
        case hash("help"): print_help(); break;
        case hash("run"):  do_run(); break;
    }
}
```

The function must be defined (not just declared) before the pragma.
The macro takes the same number of arguments as the function's parameter list.

Use `(hash)(cmd)` to call the original function.

Comboing this with `__mixin` allows you to generate code.

```C
const char* gen_vec(int n){
    // n=3, ... build string: "typedef struct Vecn Vec3; struct Vec3 { float v0; float v1; float v2; };"
}
#pragma procmacro gen_vec

__mixin(gen_vec(2))  // generates struct Vec2 with 2 float fields
__mixin(gen_vec(3))  // generates struct Vec3 with 3 float fields
__mixin(gen_vec(4))  // generates struct Vec4 with 4 float fields
```

Things can get even crazier with `_Type` (types as values).
