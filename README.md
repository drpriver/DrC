<!-- This md file was generated from a dnd file. -->
## Table of Contents
* [Building](#building)
  * [Nobuild](#nobuild)
  * [Makefile](#makefile)
  * [build.bat](#buildbat)
  * [Dependencies](#dependencies)
* [C Interpreter](#c-interpreter)
* [REPL mode](#repl-mode)
* [Bindings-Free FFI](#bindings-free-ffi)
* [Native Threads](#native-threads)
* [First Class Types](#first-class-types)
* [Static If](#static-if)
* [Procmacros](#procmacros)
* [Methods and FUCS](#methods-and-fucs)
* [Named Arguments](#named-arguments)
* [Crazy Preprocessor](#crazy-preprocessor)
* [Embeddable](#embeddable)
* [Self Hosting](#self-hosting)


# DrC


DrC is a C Preprocessor/Parser/Interpreter targeting C2y with a boatload of
[extensions](EXTENSIONS.md).  It can interpret real C projects (including itself)
and has seamless interfacing with native dynamic libraries.  `#include` a
header and `#pragma lib` it and you can call native C functions from your
interpreted C code.


A [REPL mode](#repl-mode) is also offered.


After building, try it out with <code>Bin/cc <a href=Samples/hello.c>Samples/hello.c</a></code>
or peruse the other [Samples](Samples).


Major Features Include:


* [C Interpreter](#c-interpreter)
* [Bindings-Free FFI](#bindings-free-ffi)
* [Native Threads](#native-threads)
* [REPL mode](#repl-mode)
* [First Class Types](#first-class-types)
* [Static If](#static-if)
* [Procmacros](#procmacros)
* [Methods and FUCS](#methods-and-fucs)
* [Named Arguments](#named-arguments)
* [Crazy Preprocessor](#crazy-preprocessor)
* [Embeddable](#embeddable)
* [Self Hosting](#self-hosting)

## Building
<details><summary>Nobuild</summary>

This project uses a "nobuild" build system. Bootstrap the build system by
doing something like:

```console
$ cc build.c -o build && ./build -b Bin && ./build tests
```


From then on, you can just build using the build program. If you edit it or
any of its deps, it will rebuild itself.


The build system remembers the previous flags, like `-O`. If you
change those or switch them off, it knows to rebuild any programs built
with those flags.


See [build.c](build.c) for more info.

</details>

<details><summary>Makefile</summary>

If you like using make, the makefile will bootstrap the build system for
you.

</details>

<details><summary>build.bat</summary>

You can bootstrap the build system on windows using build.bat. This is
purely convenience.

</details>

<details><summary>Dependencies</summary>

Depends on libffi. Assumes you can just do `-lffi` and that just works. If
you are on windows, that sucks. It will work there but getting it so it
"just werks" on windows is not worth the hassle.

</details>


## C Interpreter

C scripts, top level statements, no separate compile and run steps, no
intermediate files.


## REPL mode

Run `Bin/cc` with `--repl` for a REPL experience, with interactive function calls etc.

```console
$ Bin/cc --repl
cc> #include <stdio.h>
... int x = 3;
... for(int i = 0; i < 4; i++) printf("x = %d\n", x);
...
x = 3
x = 3
x = 3
x = 3
cc> ^D
```


## Bindings-Free FFI

`#include` real C headers, `#pragma lib` to load them. Also supports `#pragma
pkg_config` to populate include paths, lib paths etc.

```C
#pragma pkg_config "sdl2" // could ifdef for non-pkg-config systems
#pragma lib "SDL2" // load the library
#include <SDL2/SDL.h> <SDL.h> // extension, tries each in order
// top level statements
SDL_Init(SDL_INIT_VIDEO);
// Call SDL funcs from your scripts now!
```


## Native Threads

Related to above, you can call actual system APIs to create threads with
interpreted code.

```C
#include <stdio.h>
#include <pthread.h>
pthread_t t;
pthread_create(&t, NULL, void*(void* arg){
  printf("Hello from a thread! %p\n", (void*)pthread_self());
  return NULL;
}, NULL);
pthread_join(t, NULL);
```


This example also shows off our captureless-lambda extension.


## First Class Types

`_Type T = int; printf("%s\n", T.name)`, etc. Pass them as values, reflect
over them, see the [json demo](Samples/Extensions/json_parse.c)


## Static If

Include or exclude statements/decls based on compile time expressions.

```C
static if(__shell("date")[0] == 'W'){
  enum {IT_IS_WEDNESDAY=1};
}
printf("IT_IS_WEDNESDAY: %d\n", IT_IS_WEDNESDAY); // only compiles on wednesday
```


More seriously:

```C
#define S_(x) #x
#define S(x) S_(x)
// __ident produces a single identifier token
// from catted strings, even if it has non-identifier characters.
#define Optional(T) __ident("Optional." S(T))
#defblock OPTIONAL_DEF(T)
typedef struct Optional(T) Optional(T);
static if(T.is_pointer){
    // use NULL as sentinel
    struct Optional(T){
        T value;
        _Bool has_value(Optional(T)* self){ return self.value; }
    };
}
else {
    struct Optional(T){
        T value;
        _Bool _has_value;
        _Bool has_value(Optional(T)* self){ return self._has_value; }
    };
}
#endblock
OPTIONAL_DEF(const char*);
OPTIONAL_DEF(int);
_Static_assert(sizeof(Optional(const char*)) == sizeof(const char*));
_Static_assert(sizeof(Optional(int)) > sizeof(int));
```


Unlike `#if`, `static if` has access to C-level information without the boilerplate of a procmacro.


## Procmacros

Register a C function as a macro, call it as a macro, outputs get turned into
pp-tokens, you can mix them into your source file for code generation and
other things.


Also, [power up preprocessor conditionals](Samples/Extensions/procmacro-if.c):

```C
_Bool IS_POINTER(_Type T){ return T.is_pointer;}
#pragma procmacro IS_POINTER

#define T void*
#if IS_POINTER(T)
#pragma message T "is a pointer"
#else
#pragma message T "is not a pointer"
#endif

#undef T
#define T int
#if IS_POINTER(T)
#pragma message T "is a pointer"
#else
#pragma message T "is not a pointer"
#endif
```


## Methods and FUCS

Define methods in struct body, auto deref, `.` and <code>-></code> merged,
FUCS: Function Uniform Call Syntax to have `.func` notation with free
functions.


## Named Arguments

Same syntax as designated initializers.

```C
int func_with_lotta_bools(_Bool jump, _Bool skip, _Bool hop);
func_with_lotta_bools(.hop = 1, .jump = 0, .skip = 1); // out of order

```


Numbered args are also supported, but honestly aren't that useful, but whatever.


## Crazy Preprocessor

There's a bunch of cool preprocessor macros like `__mixin`, `__map`, `#defblock`.
There's also an API for registering your own builtin macros if you are embedding this
interpreter.


## Embeddable

This will support being embedded. WIP. No global state.


## Self Hosting

The C interpreter is able to run itself.

```console
$ Bin/cc cc.c Samples/hello.c
Hello world
$ Bin/cc cc.c cc.c Samples/hello.c
Hello world
```
