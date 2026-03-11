//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#if 1
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#define HEAVY_RECORDING
#endif
#define NO_NATIVE_CALL
#define CI_THREAD_UNSAFE_ALLOCATOR
#include "../Drp/Allocators/testing_allocator.h"
#include "../Drp/testing.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/env.h"
#include "../Drp/atom_table.h"
#include "../Drp/stdlogger.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/file_cache.h"
#include "../Drp/msb_logger.h"
#include "cc_parser.h"
#include "cc_target.h"
#include "ci_interp.h"

#include "../Drp/compiler_warnings.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


TestFunction(test_interpreter){
    TESTBEGIN();
    ArenaAllocator arena = {0};
    Allocator al = allocator_from_arena(&arena);
    struct tc {
        const char* name; int line;
        StringView program;
        int exit_code;
        _Bool skip;
    } testcases[] = {
        {
            "basic", __LINE__,
            SV("return 13;\n"),
            .exit_code = 13,
        },
        {
            "loops: for", __LINE__,
            SV("int result = 0;\n"
               "for(int i = 0; i < 10; i++) result += i;\n"
               "return result;\n"),
            .exit_code = 45,
        },
        // Arithmetic
        {
            "arith: add", __LINE__,
            SV("return 3 + 4;\n"),
            .exit_code = 7,
        },
        {
            "arith: sub", __LINE__,
            SV("return 10 - 3;\n"),
            .exit_code = 7,
        },
        {
            "arith: mul", __LINE__,
            SV("return 6 * 7;\n"),
            .exit_code = 42,
        },
        {
            "arith: div", __LINE__,
            SV("return 42 / 6;\n"),
            .exit_code = 7,
        },
        {
            "arith: mod", __LINE__,
            SV("return 17 % 5;\n"),
            .exit_code = 2,
        },
        {
            "arith: negative div", __LINE__,
            SV("return -7 / 2;\n"),
            .exit_code = -3,
        },
        {
            "arith: negative mod", __LINE__,
            SV("return -7 % 3;\n"),
            .exit_code = -1,
        },
        {
            "arith: precedence", __LINE__,
            SV("return 2 + 3 * 4;\n"),
            .exit_code = 14,
        },
        {
            "arith: parens", __LINE__,
            SV("return (2 + 3) * 4;\n"),
            .exit_code = 20,
        },
        {
            "arith: unary minus", __LINE__,
            SV("return -(-5);\n"),
            .exit_code = 5,
        },
        // Bitwise
        {
            "bitwise: and", __LINE__,
            SV("return 0xFF & 0x0F;\n"),
            .exit_code = 15,
        },
        {
            "bitwise: or", __LINE__,
            SV("return 0xF0 | 0x0F;\n"),
            .exit_code = 255,
        },
        {
            "bitwise: xor", __LINE__,
            SV("return 0xFF ^ 0x0F;\n"),
            .exit_code = 240,
        },
        {
            "bitwise: not", __LINE__,
            SV("return ~0 & 0xFF;\n"),
            .exit_code = 255,
        },
        {
            "bitwise: lshift", __LINE__,
            SV("return 1 << 4;\n"),
            .exit_code = 16,
        },
        {
            "bitwise: rshift", __LINE__,
            SV("return 256 >> 4;\n"),
            .exit_code = 16,
        },
        // Comparison
        {
            "cmp: eq true", __LINE__,
            SV("return 5 == 5;\n"),
            .exit_code = 1,
        },
        {
            "cmp: eq false", __LINE__,
            SV("return 5 == 6;\n"),
            .exit_code = 0,
        },
        {
            "cmp: neq", __LINE__,
            SV("return 5 != 6;\n"),
            .exit_code = 1,
        },
        {
            "cmp: lt", __LINE__,
            SV("return 3 < 5;\n"),
            .exit_code = 1,
        },
        {
            "cmp: gt", __LINE__,
            SV("return 5 > 3;\n"),
            .exit_code = 1,
        },
        {
            "cmp: le", __LINE__,
            SV("return 5 <= 5;\n"),
            .exit_code = 1,
        },
        {
            "cmp: ge", __LINE__,
            SV("return 5 >= 6;\n"),
            .exit_code = 0,
        },
        // Logical
        {
            "logical: and true", __LINE__,
            SV("return 1 && 2;\n"),
            .exit_code = 1,
        },
        {
            "logical: and false", __LINE__,
            SV("return 1 && 0;\n"),
            .exit_code = 0,
        },
        {
            "logical: or", __LINE__,
            SV("return 0 || 5;\n"),
            .exit_code = 1,
        },
        {
            "logical: not", __LINE__,
            SV("return !0;\n"),
            .exit_code = 1,
        },
        {
            "logical: not truthy", __LINE__,
            SV("return !42;\n"),
            .exit_code = 0,
        },
        {
            "logical: short circuit and", __LINE__,
            SV("int x = 0;\n"
               "0 && (x = 1);\n"
               "return x;\n"),
            .exit_code = 0,
        },
        {
            "logical: short circuit or", __LINE__,
            SV("int x = 0;\n"
               "1 || (x = 1);\n"
               "return x;\n"),
            .exit_code = 0,
        },
        // Ternary
        {
            "ternary: true", __LINE__,
            SV("return 1 ? 10 : 20;\n"),
            .exit_code = 10,
        },
        {
            "ternary: false", __LINE__,
            SV("return 0 ? 10 : 20;\n"),
            .exit_code = 20,
        },
        // Comma
        {
            "comma", __LINE__,
            SV("return (1, 2, 3);\n"),
            .exit_code = 3,
        },
        // Assignment operators
        {
            "assign: plus_eq", __LINE__,
            SV("int x = 10;\n"
               "x += 5;\n"
               "return x;\n"),
            .exit_code = 15,
        },
        {
            "assign: minus_eq", __LINE__,
            SV("int x = 10;\n"
               "x -= 3;\n"
               "return x;\n"),
            .exit_code = 7,
        },
        {
            "assign: mul_eq", __LINE__,
            SV("int x = 6;\n"
               "x *= 7;\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "assign: div_eq", __LINE__,
            SV("int x = 42;\n"
               "x /= 6;\n"
               "return x;\n"),
            .exit_code = 7,
        },
        {
            "assign: mod_eq", __LINE__,
            SV("int x = 17;\n"
               "x %= 5;\n"
               "return x;\n"),
            .exit_code = 2,
        },
        {
            "assign: and_eq", __LINE__,
            SV("int x = 0xFF;\n"
               "x &= 0x0F;\n"
               "return x;\n"),
            .exit_code = 15,
        },
        {
            "assign: or_eq", __LINE__,
            SV("int x = 0xF0;\n"
               "x |= 0x0F;\n"
               "return x;\n"),
            .exit_code = 255,
        },
        {
            "assign: xor_eq", __LINE__,
            SV("int x = 0xFF;\n"
               "x ^= 0x0F;\n"
               "return x;\n"),
            .exit_code = 240,
        },
        {
            "assign: lshift_eq", __LINE__,
            SV("int x = 1;\n"
               "x <<= 4;\n"
               "return x;\n"),
            .exit_code = 16,
        },
        {
            "assign: rshift_eq", __LINE__,
            SV("int x = 256;\n"
               "x >>= 4;\n"
               "return x;\n"),
            .exit_code = 16,
        },
        // Increment / Decrement
        {
            "prefix increment", __LINE__,
            SV("int x = 5;\n"
               "return ++x;\n"),
            .exit_code = 6,
        },
        {
            "postfix increment", __LINE__,
            SV("int x = 5;\n"
               "int y = x++;\n"
               "return y * 10 + x;\n"),
            .exit_code = 56,
        },
        {
            "prefix decrement", __LINE__,
            SV("int x = 5;\n"
               "return --x;\n"),
            .exit_code = 4,
        },
        {
            "postfix decrement", __LINE__,
            SV("int x = 5;\n"
               "int y = x--;\n"
               "return y * 10 + x;\n"),
            .exit_code = 54,
        },
        // Cast
        {
            "cast: int to char truncation", __LINE__,
            SV("int x = 257;\n"
               "char c = (char)x;\n"
               "return c;\n"),
            .exit_code = 1,
        },
        // sizeof
        {
            "sizeof int", __LINE__,
            SV("return sizeof(int);\n"),
            .exit_code = 4,
        },
        {
            "sizeof char", __LINE__,
            SV("return sizeof(char);\n"),
            .exit_code = 1,
        },
        {
            "sizeof expr", __LINE__,
            SV("int x = 0;\n"
               "return sizeof x;\n"),
            .exit_code = 4,
        },
        // If/else
        {
            "if: true branch", __LINE__,
            SV("if(1) return 10;\n"
               "return 20;\n"),
            .exit_code = 10,
        },
        {
            "if: false branch", __LINE__,
            SV("if(0) return 10;\n"
               "return 20;\n"),
            .exit_code = 20,
        },
        {
            "if: else", __LINE__,
            SV("if(0) return 10;\n"
               "else return 20;\n"),
            .exit_code = 20,
        },
        {
            "if: else if chain", __LINE__,
            SV("int x = 2;\n"
               "if(x == 0) return 0;\n"
               "else if(x == 1) return 1;\n"
               "else if(x == 2) return 2;\n"
               "else return 3;\n"),
            .exit_code = 2,
        },
        // While
        {
            "while", __LINE__,
            SV("int i = 0;\n"
               "int s = 0;\n"
               "while(i < 5){ s += i; i++; }\n"
               "return s;\n"),
            .exit_code = 10,
        },
        // Do-while
        {
            "do while", __LINE__,
            SV("int i = 0;\n"
               "do { i++; } while(i < 5);\n"
               "return i;\n"),
            .exit_code = 5,
        },
        {
            "do while: executes at least once", __LINE__,
            SV("int i = 0;\n"
               "do { i++; } while(0);\n"
               "return i;\n"),
            .exit_code = 1,
        },
        // For loop variants
        {
            "for: empty body", __LINE__,
            SV("int i;\n"
               "for(i = 0; i < 10; i++);\n"
               "return i;\n"),
            .exit_code = 10,
        },
        {
            "for: no init", __LINE__,
            SV("int i = 5;\n"
               "for(; i < 10; i++);\n"
               "return i;\n"),
            .exit_code = 10,
        },
        // Break / Continue
        {
            "break", __LINE__,
            SV("int i = 0;\n"
               "while(1){ if(i == 5) break; i++; }\n"
               "return i;\n"),
            .exit_code = 5,
        },
        {
            "continue", __LINE__,
            SV("int s = 0;\n"
               "for(int i = 0; i < 10; i++){\n"
               "  if(i % 2 == 0) continue;\n"
               "  s += i;\n"
               "}\n"
               "return s;\n"),
            .exit_code = 25,
        },
        {
            "break: nested loops", __LINE__,
            SV("int s = 0;\n"
               "for(int i = 0; i < 5; i++){\n"
               "  for(int j = 0; j < 5; j++){\n"
               "    if(j == 2) break;\n"
               "    s++;\n"
               "  }\n"
               "}\n"
               "return s;\n"),
            .exit_code = 10,
        },
        // Switch
        {
            "switch: match", __LINE__,
            SV("int x = 2;\n"
               "switch(x){\n"
               "  case 1: return 10;\n"
               "  case 2: return 20;\n"
               "  case 3: return 30;\n"
               "}\n"
               "return 0;\n"),
            .exit_code = 20,
        },
        {
            "switch: default", __LINE__,
            SV("int x = 99;\n"
               "switch(x){\n"
               "  case 1: return 10;\n"
               "  default: return 42;\n"
               "}\n"
               "return 0;\n"),
            .exit_code = 42,
        },
        {
            "switch: fallthrough", __LINE__,
            SV("int x = 1;\n"
               "int r = 0;\n"
               "switch(x){\n"
               "  case 1: r += 1;\n"
               "  case 2: r += 2;\n"
               "  case 3: r += 3; break;\n"
               "  case 4: r += 4;\n"
               "}\n"
               "return r;\n"),
            .exit_code = 6,
        },
        // Goto
        {
            "goto: forward", __LINE__,
            SV("goto end;\n"
               "return 1;\n"
               "end:\n"
               "return 0;\n"),
            .exit_code = 0,
        },
        {
            "goto: backward (loop)", __LINE__,
            SV("int i = 0;\n"
               "top:\n"
               "if(i >= 5) return i;\n"
               "i++;\n"
               "goto top;\n"),
            .exit_code = 5,
        },
        // Blocks / scoping
        {
            "block: variable shadowing", __LINE__,
            SV("int x = 1;\n"
               "{\n"
               "  int x = 2;\n"
               "  x = x + 10;\n"
               "}\n"
               "return x;\n"),
            .exit_code = 1,
        },
        // Pointers
        {
            "pointer: address and deref", __LINE__,
            SV("int x = 42;\n"
               "int *p = &x;\n"
               "return *p;\n"),
            .exit_code = 42,
        },
        {
            "pointer: write through", __LINE__,
            SV("int x = 0;\n"
               "int *p = &x;\n"
               "*p = 42;\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "pointer: arithmetic", __LINE__,
            SV("int arr[4] = {10, 20, 30, 40};\n"
               "int *p = arr;\n"
               "return *(p + 2);\n"),
            .exit_code = 30,
        },
        {
            "pointer: subscript", __LINE__,
            SV("int arr[3] = {5, 10, 15};\n"
               "int *p = arr;\n"
               "return p[1];\n"),
            .exit_code = 10,
        },
        {
            "pointer: difference", __LINE__,
            SV("int arr[5] = {0};\n"
               "int *a = &arr[1];\n"
               "int *b = &arr[4];\n"
               "return (int)(b - a);\n"),
            .exit_code = 3,
        },
        // Arrays
        {
            "array: basic", __LINE__,
            SV("int arr[3] = {10, 20, 30};\n"
               "return arr[0] + arr[1] + arr[2];\n"),
            .exit_code = 60,
        },
        {
            "array: zero init", __LINE__,
            SV("int arr[5] = {0};\n"
               "return arr[0] + arr[1] + arr[4];\n"),
            .exit_code = 0,
        },
        {
            "array: partial init", __LINE__,
            SV("int arr[5] = {1, 2};\n"
               "return arr[0] + arr[1] + arr[2];\n"),
            .exit_code = 3,
        },
        // Struct
        {
            "struct: basic", __LINE__,
            SV("struct point { int x; int y; };\n"
               "struct point p = {3, 4};\n"
               "return p.x * 10 + p.y;\n"),
            .exit_code = 34,
        },
        {
            "struct: designated init", __LINE__,
            SV("struct point { int x; int y; };\n"
               "struct point p = {.y = 7, .x = 3};\n"
               "return p.x * 10 + p.y;\n"),
            .exit_code = 37,
        },
        {
            "struct: pointer arrow", __LINE__,
            SV("struct point { int x; int y; };\n"
               "struct point p = {5, 6};\n"
               "struct point *pp = &p;\n"
               "return pp->x * 10 + pp->y;\n"),
            .exit_code = 56,
        },
        {
            "struct: nested", __LINE__,
            SV("struct inner { int val; };\n"
               "struct outer { struct inner a; int b; };\n"
               "struct outer o = {{42}, 7};\n"
               "return o.a.val + o.b;\n"),
            .exit_code = 49,
        },
        // Union
        {
            "union: basic", __LINE__,
            SV("union u { int i; char c; };\n"
               "union u v;\n"
               "v.i = 0;\n"
               "v.c = 5;\n"
               "return v.c;\n"),
            .exit_code = 5,
        },
        // Enum
        {
            "enum", __LINE__,
            SV("enum color { RED, GREEN, BLUE };\n"
               "enum color c = BLUE;\n"
               "return c;\n"),
            .exit_code = 2,
        },
        {
            "enum: explicit values", __LINE__,
            SV("enum { A = 10, B, C = 20 };\n"
               "return A + B + C;\n"),
            .exit_code = 41,
        },
        // Typedef
        {
            "typedef", __LINE__,
            SV("typedef int myint;\n"
               "myint x = 42;\n"
               "return x;\n"),
            .exit_code = 42,
        },
        // Functions
        {
            "function: basic call", __LINE__,
            SV("int add(int a, int b){ return a + b; }\n"
               "return add(3, 4);\n"),
            .exit_code = 7,
        },
        {
            "function: recursion (factorial)", __LINE__,
            SV("int fact(int n){\n"
               "  if(n <= 1) return 1;\n"
               "  return n * fact(n - 1);\n"
               "}\n"
               "return fact(5);\n"),
            .exit_code = 120,
        },
        {
            "function: mutual recursion", __LINE__,
            SV("int is_even(int n);\n"
               "int is_odd(int n);\n"
               "int is_even(int n){ if(n == 0) return 1; return is_odd(n - 1); }\n"
               "int is_odd(int n){ if(n == 0) return 0; return is_even(n - 1); }\n"
               "return is_even(10);\n"),
            .exit_code = 1,
        },
        // Function pointers
        {
            "function pointer", __LINE__,
            SV("int add(int a, int b){ return a + b; }\n"
               "int (*fp)(int, int) = add;\n"
               "return fp(3, 4);\n"),
            .exit_code = 7,
            .skip = 0,
        },
        // Static locals
        {
            "static local", __LINE__,
            SV("int counter(void){\n"
               "  static int n = 0;\n"
               "  return n++;\n"
               "}\n"
               "counter();\n"
               "counter();\n"
               "return counter();\n"),
            .exit_code = 2,
        },
        // Global variables
        {
            "global variable", __LINE__,
            SV("int g = 10;\n"
               "int get(void){ return g; }\n"
               "void set(int v){ g = v; }\n"
               "set(42);\n"
               "return get();\n"),
            .exit_code = 42,
        },
        // Type conversions
        {
            "unsigned wrap", __LINE__,
            SV("unsigned int x = 0;\n"
               "x = x - 1;\n"
               "return (x > 1000) ? 1 : 0;\n"),
            .exit_code = 1,
        },
        // Compound literals
        {
            "compound literal", __LINE__,
            SV("struct pt { int x; int y; };\n"
               "struct pt p = (struct pt){.x=3, .y=4};\n"
               "return p.x + p.y;\n"),
            .exit_code = 7,
        },
        {
            "compound literal parens", __LINE__,
            SV("struct pt { int x; int y; };\n"
               "struct pt p = ((struct pt){.x=3, .y=4});\n"
               "return p.x + p.y;\n"),
            .exit_code = 7,
        },
        {
            "compound literal lvalue", __LINE__,
            SV("struct pt { int x; int y; };\n"
               "struct pt* p = &(struct pt){.x=3, .y=4};\n"
               "return p->x + p->y;\n"),
            .exit_code = 7,
        },
        {
            "compound literal lvalue parens", __LINE__,
            SV("struct pt { int x; int y; };\n"
               "struct pt* p = &((struct pt){.x=3, .y=4});\n"
               "return p->x + p->y;\n"),
            .exit_code = 7,
        },
        {
            "compound literal member access", __LINE__,
            SV("struct pt { int x; int y; };\n"
               "return (struct pt){.x=10, .y=20}.y;\n"),
            .exit_code = 20,
        },
        {
            "compound literal array subscript", __LINE__,
            SV("return (int[]){10, 20, 30}[1];\n"),
            .exit_code = 20,
        },
        {
            "compound literal assignment", __LINE__,
            SV("struct pt { int x; int y; };\n"
               "struct pt p = {0, 0};\n"
               "p = (struct pt){.x=5, .y=6};\n"
               "return p.x + p.y;\n"),
            .exit_code = 11,
        },
        {
            "compound literal in struct init", __LINE__,
            SV("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer o = { 1, (struct Inner){2, 3} };\n"
               "return o.x + o.inner.a + o.inner.b;\n"),
            .exit_code = 6,
        },
        {
            "compound literal array lvalue", __LINE__,
            SV("int *p = (int[]){10, 20, 30};\n"
               "p[1] = 99;\n"
               "return p[1];\n"),
            .exit_code = 99,
        },
        {
            "compound literal member lvalue", __LINE__,
            SV("struct pt { int x; int y; };\n"
               "(struct pt){.x=3, .y=4}.x = 10;\n"
               "return 0;\n"),
            .exit_code = 0,
        },
        {
            "compound literal deep lvalue", __LINE__,
            SV("struct Inner { int a; int b; };\n"
               "struct Outer { int x; struct Inner inner; };\n"
               "struct Outer *p = &(struct Outer){.x=1, .inner={.a=2, .b=3}};\n"
               "p->inner.a = 42;\n"
               "return p->inner.a + p->inner.b;\n"),
            .exit_code = 45,
        },
        {
            "compound literal array element lvalue", __LINE__,
            SV("struct pt { int x; int y; };\n"
               "struct pt *p = (struct pt[]){ {1, 2}, {3, 4} };\n"
               "p[1].x = 50;\n"
               "return p[0].x + p[1].x + p[1].y;\n"),
            .exit_code = 55,
        },
        {
            "compound literal member array decay", __LINE__,
            SV("struct SmallStr { char txt[8]; };\n"
               "char *p = (struct SmallStr){\"hello\"}.txt;\n"
               "return p[1];\n"),
            .exit_code = 101, // 'e'
        },
        {
            "compound literal as function arg", __LINE__,
            SV("struct pt { int x; int y; };\n"
               "int* bump(int* p) { *p += 10; return p; }\n"
               "int* q = bump(&(struct pt){3, 4}.x);\n"
               "return *q;\n"),
            .exit_code = 13,
        },
        // String literals
        {
            "string literal indexing", __LINE__,
            SV("const char *s = \"hello\";\n"
               "return s[1];\n"),
            .exit_code = 101, // 'e'
        },
        // Multidimensional array
        {
            "2d array", __LINE__,
            SV("int m[2][3] = {{1,2,3},{4,5,6}};\n"
               "return m[1][2];\n"),
            .exit_code = 6,
        },
        // Deeply nested expressions
        {
            "nested ternary", __LINE__,
            SV("int x = 3;\n"
               "return x == 1 ? 10 : x == 2 ? 20 : x == 3 ? 30 : 40;\n"),
            .exit_code = 30,
        },
        // Zero iteration
        {
            "zero iteration for", __LINE__,
            SV("int s = 5;\n"
               "for(int i = 0; i < 0; i++) s += i;\n"
               "return s;\n"),
            .exit_code = 5,
        },
        {
            "zero iteration while", __LINE__,
            SV("int s = 5;\n"
               "while(0) s = 0;\n"
               "return s;\n"),
            .exit_code = 5,
        },
        // Unary plus
        {
            "unary plus", __LINE__,
            SV("int x = -5;\n"
               "return +x;\n"),
            .exit_code = -5,
        },
        // Ternary: side effects only in taken branch
        {
            "ternary: no side effect in untaken", __LINE__,
            SV("int x = 0;\n"
               "int y = 1 ? (x = 10) : (x = 20);\n"
               "return x;\n"),
            .exit_code = 10,
        },
        // Cast: sign extension
        {
            "cast: sign extend char to int", __LINE__,
            SV("char c = -1;\n"
               "int x = (int)c;\n"
               "return x;\n"),
            .exit_code = -1,
        },
        // Subscript equivalence *(a+i)
        {
            "subscript: *(a+i)", __LINE__,
            SV("int arr[3] = {10, 20, 30};\n"
               "return *(arr + 2);\n"),
            .exit_code = 30,
        },
        // Return without value (void function)
        {
            "return void", __LINE__,
            SV("void noop(void){ return; }\n"
               "noop();\n"
               "return 42;\n"),
            .exit_code = 42,
        },
        // for(;;) infinite loop with break
        {
            "for: infinite with break", __LINE__,
            SV("int i = 0;\n"
               "for(;;){ if(i == 7) break; i++; }\n"
               "return i;\n"),
            .exit_code = 7,
        },
        // Empty statement
        {
            "empty statement", __LINE__,
            SV(";;;\n"
               "return 1;\n"),
            .exit_code = 1,
        },
        // Integer types: short
        {
            "type: short", __LINE__,
            SV("short s = 127;\n"
               "return s;\n"),
            .exit_code = 127,
        },
        // Integer types: long
        {
            "type: long", __LINE__,
            SV("long l = 100;\n"
               "return (int)l;\n"),
            .exit_code = 100,
        },
        // Integer types: unsigned char
        {
            "type: unsigned char", __LINE__,
            SV("unsigned char c = 200;\n"
               "return c;\n"),
            .exit_code = 200,
        },
        // Multi-level pointer
        {
            "pointer: multi-level", __LINE__,
            SV("int x = 42;\n"
               "int *p = &x;\n"
               "int **pp = &p;\n"
               "return **pp;\n"),
            .exit_code = 42,
        },
        // Null pointer check
        {
            "pointer: null check", __LINE__,
            SV("int *p = 0;\n"
               "return p == 0;\n"),
            .exit_code = 1,
        },
        // Array decay to pointer
        {
            "array: decay to pointer", __LINE__,
            SV("int arr[3] = {10, 20, 30};\n"
               "int *p = arr;\n"
               "return *p;\n"),
            .exit_code = 10,
        },
        // Pointer comparison
        {
            "pointer: comparison", __LINE__,
            SV("int arr[3] = {0};\n"
               "int *a = &arr[0];\n"
               "int *b = &arr[2];\n"
               "return a < b;\n"),
            .exit_code = 1,
        },
        // void* as generic pointer
        {
            "pointer: void star", __LINE__,
            SV("int x = 42;\n"
               "void *v = &x;\n"
               "int *p = (int*)v;\n"
               "return *p;\n"),
            .exit_code = 42,
        },
        // Local variable without initializer
        {
            "decl: uninitialized local", __LINE__,
            SV("int x;\n"
               "x = 7;\n"
               "return x;\n"),
            .exit_code = 7,
        },
        // Adjacent string literal concatenation
        {
            "string: adjacent concat", __LINE__,
            SV("const char *s = \"hel\" \"lo\";\n"
               "return s[3];\n"),
            .exit_code = 108, // 'l'
        },
        // Integer constants: hex and octal
        {
            "constant: hex", __LINE__,
            SV("return 0x2A;\n"),
            .exit_code = 42,
        },
        {
            "constant: octal", __LINE__,
            SV("return 052;\n"),
            .exit_code = 42,
        },
        // Integer promotion: unsigned + signed
        {
            "promotion: unsigned arith", __LINE__,
            SV("unsigned int a = 10;\n"
               "int b = 3;\n"
               "return (int)(a - b);\n"),
            .exit_code = 7,
        },
        // Truncation on narrowing assignment
        {
            "promotion: narrowing truncation", __LINE__,
            SV("int x = 0x1FF;\n"
               "unsigned char c = x;\n"
               "return c;\n"),
            .exit_code = 255,
        },
        // Fibonacci (recursion)
        {
            "recursion: fibonacci", __LINE__,
            SV("int fib(int n){\n"
               "  if(n <= 1) return n;\n"
               "  return fib(n-1) + fib(n-2);\n"
               "}\n"
               "return fib(11);\n"),
            .exit_code = 89,
        },
        // Early return from deep nesting
        {
            "early return from nesting", __LINE__,
            SV("int find(void){\n"
               "  for(int i = 0; i < 10; i++){\n"
               "    for(int j = 0; j < 10; j++){\n"
               "      if(i == 3 && j == 4) return i * 10 + j;\n"
               "    }\n"
               "  }\n"
               "  return -1;\n"
               "}\n"
               "return find();\n"),
            .exit_code = 34,
        },
        // Comma in for-loop
        {
            "for: comma in clauses", __LINE__,
            SV("int a, b;\n"
               "for(a = 0, b = 10; a < 5; a++, b--)\n"
               "  ;\n"
               "return a * 10 + b;\n"),
            .exit_code = 55,
        },
        // Struct: sizeof with padding
        {
            "struct: sizeof", __LINE__,
            SV("struct s { char c; int i; };\n"
               "return sizeof(struct s) >= 5;\n"),
            .exit_code = 1,
        },
        // Union: sizeof is max member
        {
            "union: sizeof", __LINE__,
            SV("union u { char c; int i; };\n"
               "return sizeof(union u) == sizeof(int);\n"),
            .exit_code = 1,
        },
        // Cast between pointer types
        {
            "pointer: cast types", __LINE__,
            SV("int x = 0x01020304;\n"
               "char *cp = (char*)&x;\n"
               "return *cp != 0;\n"),  // just check it doesn't crash; endian-independent
            .exit_code = 1,
        },
        // Sizeof pointer
        {
            "sizeof pointer", __LINE__,
            SV("return sizeof(int*) == sizeof(void*);\n"),
            .exit_code = 1,
        },
        // Sizeof array
        {
            "sizeof array", __LINE__,
            SV("int arr[10];\n"
               "return sizeof arr / sizeof arr[0];\n"),
            .exit_code = 10,
        },
        // Float / double
        {
            "float: basic arith", __LINE__,
            SV("float f = 3.5f;\n"
               "return (int)(f * 2.0f);\n"),
            .exit_code = 7,
        },
        {
            "double: basic arith", __LINE__,
            SV("double d = 6.5;\n"
               "return (int)(d + 0.5);\n"),
            .exit_code = 7,
        },
        {
            "float: truncation to int", __LINE__,
            SV("float f = 9.9f;\n"
               "return (int)f;\n"),
            .exit_code = 9,
        },
        {
            "double: negative truncation", __LINE__,
            SV("double d = -3.7;\n"
               "return (int)d;\n"),
            .exit_code = -3,
        },
        {
            "int to float to int", __LINE__,
            SV("int a = 7;\n"
               "float f = (float)a;\n"
               "return (int)(f + 0.5f);\n"),
            .exit_code = 7,
        },
        {
            "float: comparison", __LINE__,
            SV("float a = 1.5f;\n"
               "float b = 2.5f;\n"
               "return a < b;\n"),
            .exit_code = 1,
        },
        {
            "double: sizeof", __LINE__,
            SV("return sizeof(double);\n"),
            .exit_code = 8,
        },
        {
            "float: sizeof", __LINE__,
            SV("return sizeof(float);\n"),
            .exit_code = 4,
        },
        {
            "float: division", __LINE__,
            SV("float f = 10.0f / 3.0f;\n"
               "return (int)(f * 3.0f);\n"),
            .exit_code = 10,
        },
        {
            "double: mixed arith with int", __LINE__,
            SV("int a = 5;\n"
               "double d = a + 2.5;\n"
               "return (int)d;\n"),
            .exit_code = 7,
        },
        {
            "lambda (IIFE)", __LINE__,
            SV("int x = int(){\n"
               "    return 42;\n"
               "}();\n"
               "return x;\n"),
            .exit_code = 42,
        },
        {
            "lambda (IIFE)", __LINE__,
            SV("return int(){\n"
               "    return 42;\n"
               "}();\n"),
            .exit_code = 42,
        },
        // Atomics
        {
            "atomic: fetch_add", __LINE__,
            SV("int x = 10;\n"
               "int old = __atomic_fetch_add(&x, 5, __ATOMIC_SEQ_CST);\n"
               "return old * 100 + x;\n"),
            .exit_code = 10 * 100 + 15,
        },
        {
            "atomic: fetch_sub", __LINE__,
            SV("int x = 20;\n"
               "int old = __atomic_fetch_sub(&x, 7, __ATOMIC_SEQ_CST);\n"
               "return old * 100 + x;\n"),
            .exit_code = 20 * 100 + 13,
        },
        {
            "atomic: load", __LINE__,
            SV("int x = 42;\n"
               "return __atomic_load_n(&x, __ATOMIC_SEQ_CST);\n"),
            .exit_code = 42,
        },
        {
            "atomic: store", __LINE__,
            SV("int x = 0;\n"
               "__atomic_store_n(&x, 99, __ATOMIC_SEQ_CST);\n"
               "return x;\n"),
            .exit_code = 99,
        },
        {
            "atomic: exchange", __LINE__,
            SV("int x = 10;\n"
               "int old = __atomic_exchange_n(&x, 20, __ATOMIC_SEQ_CST);\n"
               "return old * 100 + x;\n"),
            .exit_code = 10 * 100 + 20,
        },
        {
            "atomic: compare_exchange success", __LINE__,
            SV("int x = 10;\n"
               "int expected = 10;\n"
               "_Bool ok = __atomic_compare_exchange_n(&x, &expected, 20, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);\n"
               "return ok * 100 + x;\n"),
            .exit_code = 1 * 100 + 20,
        },
        {
            "atomic: compare_exchange failure", __LINE__,
            SV("int x = 10;\n"
               "int expected = 99;\n"
               "_Bool ok = __atomic_compare_exchange_n(&x, &expected, 20, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);\n"
               "return ok * 1000 + expected * 10 + x;\n"),
            .exit_code = 0 * 1000 + 10 * 10 + 10,
        },
        {
            "atomic: fetch_add char", __LINE__,
            SV("char x = 10;\n"
               "char old = __atomic_fetch_add(&x, 3, __ATOMIC_SEQ_CST);\n"
               "return old * 100 + x;\n"),
            .exit_code = 10 * 100 + 13,
        },
        // Bitfields
        {
            "bitfield: read", __LINE__,
            SV("struct S { int a : 3; int b : 5; };\n"
               "struct S s = {3, 10};\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 3 * 100 + 10,
        },
        {
            "bitfield: write", __LINE__,
            SV("struct S { int a : 3; int b : 5; };\n"
               "struct S s = {0};\n"
               "s.a = 5;\n"
               "s.b = 17;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 5 * 100 + 17,
        },
        {
            "bitfield: write preserves adjacent", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {7, 0};\n"
               "s.b = 9;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 7 * 100 + 9,
        },
        {
            "bitfield: arrow read", __LINE__,
            SV("struct S { int a : 3; int b : 5; };\n"
               "struct S s = {2, 15};\n"
               "struct S* p = &s;\n"
               "return p->a * 100 + p->b;\n"),
            .exit_code = 2 * 100 + 15,
        },
        {
            "bitfield: arrow write", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {0};\n"
               "struct S* p = &s;\n"
               "p->a = 11;\n"
               "p->b = 6;\n"
               "return p->a * 100 + p->b;\n"),
            .exit_code = 11 * 100 + 6,
        },
        {
            "bitfield: preinc", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {5, 10};\n"
               "int r = ++s.a;\n"
               "return r * 100 + s.a;\n"),
            .exit_code = 6 * 100 + 6,
        },
        {
            "bitfield: postinc", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {5, 10};\n"
               "int r = s.a++;\n"
               "return r * 100 + s.a;\n"),
            .exit_code = 5 * 100 + 6,
        },
        {
            "bitfield: predec", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {5, 10};\n"
               "int r = --s.b;\n"
               "return r * 100 + s.b;\n"),
            .exit_code = 9 * 100 + 9,
        },
        {
            "bitfield: postdec", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {5, 10};\n"
               "int r = s.b--;\n"
               "return r * 100 + s.b;\n"),
            .exit_code = 10 * 100 + 9,
        },
        {
            "bitfield: inc preserves adjacent", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {7, 3};\n"
               "s.b++;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 7 * 100 + 4,
        },
        {
            "bitfield: addassign", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {2, 3};\n"
               "s.a += 5;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 7 * 100 + 3,
        },
        {
            "bitfield: subassign", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {9, 3};\n"
               "s.a -= 4;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 5 * 100 + 3,
        },
        {
            "bitfield: orassign", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {5, 3};\n"
               "s.a |= 2;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 7 * 100 + 3,
        },
        {
            "bitfield: andassign", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {7, 3};\n"
               "s.a &= 5;\n"
               "return s.a * 100 + s.b;\n"),
            .exit_code = 5 * 100 + 3,
        },
        {
            "bitfield: arrow preinc", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {5, 10};\n"
               "struct S* p = &s;\n"
               "int r = ++p->a;\n"
               "return r * 100 + p->b;\n"),
            .exit_code = 6 * 100 + 10,
        },
        {
            "bitfield: arrow addassign", __LINE__,
            SV("struct S { int a : 4; int b : 4; };\n"
               "struct S s = {2, 3};\n"
               "struct S* p = &s;\n"
               "p->a += 5;\n"
               "return p->a * 100 + p->b;\n"),
            .exit_code = 7 * 100 + 3,
        },
        {
            "big string", __LINE__,
            SV("enum {SIZE = 1<<16};\n"
               "char buff[SIZE] = \"hello\";\n"
               "int sum = 0;\n"
               "for(__SIZE_TYPE__ i = 0; i < SIZE && buff[i]; i++)\n"
               "    sum += buff[i];\n"
               "return sum;\n"),
            .exit_code = 'h' + 'e' + 'l' + 'l' + 'o',
        },
        {
            "varargs", __LINE__,
            SV(
               "#define va_start __builtin_va_start\n"
               "#define va_copy __builtin_va_copy\n"
               "#define va_arg __builtin_va_arg\n"
               "#define va_end __builtin_va_end\n"
               "typedef __builtin_va_list va_list;\n"
               "int vsum(int n, va_list ap){\n"
               "    int sum = 0;\n"
               "    for(int i = 0; i < n; i++){\n"
               "        sum += va_arg(ap, int);\n"
               "    }\n"
               "    return sum;\n"
               "}\n"
               "int sum(int n, ...){\n"
               "    va_list ap;\n"
               "    va_start(ap, n);\n"
               "    int result = vsum(n, ap);\n"
               "    va_end(ap);\n"
               "    return result;\n"
               "}\n"
               "int sum2(int n, ...){\n"
               "    va_list ap, ap2;\n"
               "    va_start(ap);\n" // c23
               "    va_copy(ap2, ap);\n"
               "    int result = vsum(n, ap);\n"
               "    result += vsum(n, ap2);\n"
               "    va_end(ap);\n"
               "    va_end(ap2);\n"
               "    return result;\n"
               "}\n"
               "int x = sum(3, 4, 5, 6) + sum2(2, 8, 9);\n"
               "return x;\n"
               ),
            .exit_code = 4+5+6+8+9+8+9,
        },
        // _Generic
        {
            "_Generic: basic int", __LINE__,
            SV("int x = 1;\n"
               "return _Generic(x, int: 10, float: 20, default: 30);\n"),
            .exit_code = 10,
        },
        {
            "_Generic: basic float", __LINE__,
            SV("float x = 1.0f;\n"
               "return _Generic(x, int: 10, float: 20, default: 30);\n"),
            .exit_code = 20,
        },
        {
            "_Generic: default", __LINE__,
            SV("double x = 1.0;\n"
               "return _Generic(x, int: 10, float: 20, default: 30);\n"),
            .exit_code = 30,
        },
        {
            "_Generic: lvalue conversion strips const", __LINE__,
            SV("const int x = 1;\n"
               "return _Generic(x, int: 10, default: 20);\n"),
            .exit_code = 10,
        },
        {
            "_Generic: pointer type", __LINE__,
            SV("int* p = 0;\n"
               "return _Generic(p, int*: 10, float*: 20, default: 30);\n"),
            .exit_code = 10,
        },
        {
            "_Generic: expression result used", __LINE__,
            SV("int x = 5;\n"
               "int y = _Generic(x, int: x * 3, default: 0);\n"
               "return y;\n"),
            .exit_code = 15,
        },
        {
            "_Generic: default comes first", __LINE__,
            SV("int x = 1;\n"
               "return _Generic(x, default: 0, int: 42);\n"),
            .exit_code = 42,
        },
        {
            "_Generic: type-name operand (C23)", __LINE__,
            SV("return _Generic(int, int: 10, float: 20, default: 30);\n"),
            .exit_code = 10,
        },
        {
            "_Generic: type-name preserves const", __LINE__,
            SV("return _Generic(const int, int: 10, const int: 20, default: 30);\n"),
            .exit_code = 20,
        },
        {
            "_Generic: nested in expression", __LINE__,
            SV("int x = 1;\n"
               "return _Generic(x, int: 3, default: 0) + _Generic(x, int: 7, default: 0);\n"),
            .exit_code = 10,
        },
        // __builtin_add_overflow
        {
            "add_overflow: no overflow", __LINE__,
            SV("int r;\n"
               "int ov = __builtin_add_overflow(3, 4, &r);\n"
               "return ov * 100 + r;\n"),
            .exit_code = 7,
        },
        {
            "add_overflow: signed overflow", __LINE__,
            SV("int r;\n"
               "int ov = __builtin_add_overflow(2147483647, 1, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        {
            "add_overflow: unsigned no overflow", __LINE__,
            SV("unsigned r;\n"
               "int ov = __builtin_add_overflow(3u, 4u, &r);\n"
               "return ov * 100 + r;\n"),
            .exit_code = 7,
        },
        {
            "add_overflow: unsigned overflow", __LINE__,
            SV("unsigned char r;\n"
               "int ov = __builtin_add_overflow(200, 200, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        // __builtin_sub_overflow
        {
            "sub_overflow: no overflow", __LINE__,
            SV("int r;\n"
               "int ov = __builtin_sub_overflow(10, 3, &r);\n"
               "return ov * 100 + r;\n"),
            .exit_code = 7,
        },
        {
            "sub_overflow: signed underflow", __LINE__,
            SV("int r;\n"
               "int ov = __builtin_sub_overflow(-2147483647 - 1, 1, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        // __builtin_mul_overflow
        {
            "mul_overflow: no overflow", __LINE__,
            SV("int r;\n"
               "int ov = __builtin_mul_overflow(6, 7, &r);\n"
               "return ov * 100 + r;\n"),
            .exit_code = 42,
        },
        {
            "mul_overflow: overflow", __LINE__,
            SV("int r;\n"
               "int ov = __builtin_mul_overflow(2147483647, 2, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        {
            "add_overflow: negative into unsigned", __LINE__,
            SV("unsigned r;\n"
               "int ov = __builtin_add_overflow(-1, 0, &r);\n"
               "return ov;\n"),
            .exit_code = 1,
        },
        {
            "add_overflow: mixed types no overflow", __LINE__,
            SV("long r;\n"
               "int ov = __builtin_add_overflow((short)100, 200u, &r);\n"
               "return ov * 1000 + (int)r;\n"),
            .exit_code = 300,
        },
        // __builtin_popcount
        {
            "popcount: zero", __LINE__,
            SV("return __builtin_popcount(0);\n"),
            .exit_code = 0,
        },
        {
            "popcount: one", __LINE__,
            SV("return __builtin_popcount(1);\n"),
            .exit_code = 1,
        },
        {
            "popcount: power of two", __LINE__,
            SV("return __builtin_popcount(1024);\n"),
            .exit_code = 1,
        },
        {
            "popcount: all bits", __LINE__,
            SV("return __builtin_popcount(0xFFu);\n"),
            .exit_code = 8,
        },
        {
            "popcount: mixed bits", __LINE__,
            SV("return __builtin_popcount(0b10101010);\n"),
            .exit_code = 4,
        },
        {
            "popcountll", __LINE__,
            SV("return __builtin_popcountll(0xFFFFFFFFull);\n"),
            .exit_code = 32,
        },
        // __builtin_ctz
        {
            "ctz: 1", __LINE__,
            SV("return __builtin_ctz(1);\n"),
            .exit_code = 0,
        },
        {
            "ctz: power of two", __LINE__,
            SV("return __builtin_ctz(8);\n"),
            .exit_code = 3,
        },
        {
            "ctz: trailing zeros", __LINE__,
            SV("return __builtin_ctz(0x100);\n"),
            .exit_code = 8,
        },
        {
            "ctzll", __LINE__,
            SV("return __builtin_ctzll(1ull << 32);\n"),
            .exit_code = 32,
        },
        // __builtin_clz
        {
            "clz: 1", __LINE__,
            SV("return __builtin_clz(1);\n"),
            .exit_code = 31,
        },
        {
            "clz: high bit", __LINE__,
            SV("return __builtin_clz(0x80000000u);\n"),
            .exit_code = 0,
        },
        {
            "clz: 16", __LINE__,
            SV("return __builtin_clz(16);\n"),
            .exit_code = 27,
        },
        {
            "clzll", __LINE__,
            SV("return __builtin_clzll(1ull << 32);\n"),
            .exit_code = 31,
        },
        {
            "typdef func forward decl", __LINE__,
            SV("typedef int f(void);\n"
                "f fn;\n"
                "int x = fn();\n"
                "int fn(void){return 3;}\n"
                "return x;\n"),
            .exit_code = 3,
        },
        {
            "cpy", __LINE__,
            SV("void cpy(void* d, void* s, __SIZE_TYPE__ sz){\n"
                "char *dst = d, *src = s;\n"
                "for(__SIZE_TYPE__ i = 0; i < sz; i++)\n"
                "   dst[i] = src[i];\n"
                "}\n"
                "int x = 4, y = 9;\n"
                "cpy(&x, &y, sizeof y);\n"
                "return x;\n"),
            .exit_code = 9,
        },
        {
            "assign to fla", __LINE__,
            SV( "typedef struct FLA {int x; int vals[];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 8;\n"
                "fla->vals[0] = y;\n"
                "return fla->vals[0];\n"),
            .exit_code = 8,
        },
        {
            "assign to fake fla", __LINE__,
            SV(
                "typedef struct FLA {int x; int vals[0];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 8;\n"
                "fla->vals[0] = y;\n"
                "return fla->vals[0];\n"),
            .exit_code = 8,
        },
        {
            "assign to really fake fla", __LINE__,
            SV( "typedef struct FLA {int x; int vals[1];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 8;\n"
                "fla->vals[1] = y;\n"
                "return fla->vals[1];\n"),
            .exit_code = 8,
        },
        {
            "cpy fla", __LINE__,
            SV("void cpy(void* d, void* s, __SIZE_TYPE__ sz){\n"
                "char *dst = d, *src = s;\n"
                "for(__SIZE_TYPE__ i = 0; i < sz; i++)\n"
                "   dst[i] = src[i];\n"
                "}\n"
                "typedef struct FLA {int x; int vals[];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 7;\n"
                "cpy(fla->vals, &y, sizeof y);\n"
                "return fla->vals[0];\n"),
            .exit_code = 7,
        },
        {
            "cpy fake fla", __LINE__,
            SV("void cpy(void* d, void* s, __SIZE_TYPE__ sz){\n"
                "char *dst = d, *src = s;\n"
                "for(__SIZE_TYPE__ i = 0; i < sz; i++)\n"
                "   dst[i] = src[i];\n"
                "}\n"
                "typedef struct FLA {int x; int vals[0];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 7;\n"
                "cpy(fla->vals, &y, sizeof y);\n"
                "return fla->vals[0];\n"),
            .exit_code = 7,
        },
        {
            "struct with union copy", __LINE__,
            SV("typedef struct {\n"
                "  union { unsigned long _bits; struct { unsigned type: 4; unsigned long _pad: 60; }; };\n"
                "  union { struct { const char* text; unsigned long len; }; };\n"
                "  unsigned long loc;\n"
                "} Tok;\n"
                "Tok arr[2] = {{._bits=1, .text=\"hi\", .len=2, .loc=10},\n"
                "              {._bits=2, .text=\"yo\", .len=2, .loc=20}};\n"
                "Tok t = arr[1];\n"
                "return (int)t.loc;\n"),
            .exit_code = 20,
        },
        {
            "struct copy from pointer subscript", __LINE__,
            SV("typedef struct { long a; long b; long c; long d; } Big;\n"
                "Big g[2] = {{1,2,3,4},{5,6,7,8}};\n"
                "int f(const Big* p){\n"
                "  Big t = p[1];\n"
                "  return (int)(t.a + t.d);\n"
                "}\n"
                "return f(g);\n"),
            .exit_code = 13,
        },
        {
            "struct copy from array", __LINE__,
            SV("typedef struct { long a; long b; long c; long d; } Big;\n"
                "Big arr[2] = {{1,2,3,4},{5,6,7,8}};\n"
                "Big t = arr[1];\n"
                "return (int)(t.a + t.d);\n"),
            .exit_code = 13,
        },
        {
            "struct with union alignment", __LINE__,
            SV("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "} Frame;\n"
                "Frame f = {0};\n"
                "f.cursor = 10;\n"
                "f.cursor++;\n"
                "return (int)f.cursor;\n"),
            .exit_code = 11,
        },
        {
            "struct union cursor via pointer", __LINE__,
            SV("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "} Frame;\n"
                "void advance(Frame* f){ f->cursor++; f->column++; }\n"
                "Frame f = {0};\n"
                "f.cursor = 10;\n"
                "advance(&f);\n"
                "advance(&f);\n"
                "return (int)f.cursor;\n"),
            .exit_code = 12,
        },
        {
            "tokenizer loop pattern", __LINE__,
            SV("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "  const char* text;\n"
                "  __SIZE_TYPE__ length;\n"
                "} Frame;\n"
                "int next(Frame* f){\n"
                "  if(f->cursor == f->length) return -1;\n"
                "  int c = (int)(unsigned char)f->text[f->cursor++];\n"
                "  f->column++;\n"
                "  return c;\n"
                "}\n"
                "Frame f = {.text = \"hello\", .length = 5};\n"
                "int sum = 0;\n"
                "int c;\n"
                "while((c = next(&f)) != -1) sum += c;\n"
                "return sum == ('h'+'e'+'l'+'l'+'o');\n"),
            .exit_code = 1,
        },
        {
            "cppframe layout", __LINE__,
            SV("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "  struct { __SIZE_TYPE__ length; const char* text; } txt;\n"
                "} Frame;\n"
                "Frame f = {0};\n"
                "f.txt.length = 99;\n"
                "f.txt.text = \"hello\";\n"
                "// Check that file_id doesn't alias txt.length\n"
                "f.file_id = 4;\n"
                "return (int)f.txt.length;\n"),
            .exit_code = 99,
        },
        {
            "switch negative case", __LINE__,
            SV("int x = -1;\n"
                "switch(x){\n"
                "  case -1: return 1;\n"
                "  default: return 0;\n"
                "}\n"),
            .exit_code = 1,
        },
        {
            "struct union field write", __LINE__,
            SV("typedef struct {\n"
                "  unsigned file_id;\n"
                "  union {\n"
                "    struct { unsigned line; unsigned column; __SIZE_TYPE__ cursor; };\n"
                "  };\n"
                "} Frame;\n"
                "Frame f = {0};\n"
                "f.file_id = 99;\n"
                "f.line = 1;\n"
                "f.column = 5;\n"
                "f.cursor = 42;\n"
                "return (int)(f.file_id + f.line + f.column + f.cursor);\n"),
            .exit_code = 147,
        },
        {
            "struct copy in loop", __LINE__,
            SV("typedef struct { long a; long b; long c; long d; } Big;\n"
                "Big arr[3] = {{1,2,3,4},{5,6,7,8},{9,10,11,12}};\n"
                "int sum = 0;\n"
                "for(int i = 0; i < 3; i++){\n"
                "  Big t = arr[i];\n"
                "  sum += (int)t.a;\n"
                "}\n"
                "return sum;\n"),
            .exit_code = 15,
        },
        {
            "varargs no extra args", __LINE__,
            SV("int f(int x, ...){\n"
                "  __builtin_va_list va;\n"
                "  __builtin_va_start(va, x);\n"
                "  __builtin_va_end(va);\n"
                "  return x;\n"
                "}\n"
                "return f(42);\n"),
            .exit_code = 42,
        },
        {
            "cpy really fake fla", __LINE__,
            SV("void cpy(void* d, void* s, __SIZE_TYPE__ sz){\n"
                "char *dst = d, *src = s;\n"
                "for(__SIZE_TYPE__ i = 0; i < sz; i++)\n"
                "   dst[i] = src[i];\n"
                "}\n"
                "typedef struct FLA {int x; int vals[1];} FLA;\n"
                "char buff[32];\n"
                "FLA* fla = (FLA*)buff;\n"
                "int y = 7;\n"
                "cpy(fla->vals+1, &y, sizeof y);\n"
                "return fla->vals[1];\n"),
            .exit_code = 7,
        },
        {
            "negative subscript bitfield", __LINE__,
            SV("typedef struct {\n"
                "  unsigned type: 4;\n"
                "  unsigned pad: 28;\n"
                "} Tok;\n"
                "Tok arr[3];\n"
                "arr[0].type = 1;\n"
                "arr[1].type = 2;\n"
                "arr[2].type = 3;\n"
                "Tok* end = arr + 3;\n"
                "return end[-1].type;\n"),
            .exit_code = 3,
        },
        {
            "negative subscript no bitfield", __LINE__,
            SV("int arr[3] = {10, 20, 30};\n"
                "int* end = arr + 3;\n"
                "return end[-1];\n"),
            .exit_code = 30,
        },
        {
            "alloca basic", __LINE__,
            SV("void* __builtin_alloca(__SIZE_TYPE__);\n"
                "void* p = __builtin_alloca(16);\n"
                "int* ip = (int*)p;\n"
                "ip[0] = 42;\n"
                "ip[1] = 58;\n"
                "return ip[0] + ip[1];\n"),
            .exit_code = 100,
        },
        {
            "alloca in loop", __LINE__,
            SV("void* __builtin_alloca(__SIZE_TYPE__);\n"
                "int sum = 0;\n"
                "for(int i = 0; i < 5; i++){\n"
                "  int* p = (int*)__builtin_alloca(sizeof(int));\n"
                "  *p = i;\n"
                "  sum += *p;\n"
                "}\n"
                "return sum;\n"),
            .exit_code = 10,
        },
        {
            "struct return dot", __LINE__,
            SV("typedef struct Foo Foo;\n"
               "struct Foo {struct { struct { struct { int x;} c;} b; } a;} ;\n"
               "Foo foo(){ return (Foo){3};}\n"
               "return foo().a.b.c.x;\n"),
            .exit_code = 3,
        },
        {
            "redecl after def keeps param names", __LINE__,
            SV("static int add(int, int);\n"
               "static int add(int a, int b){ return a + b; }\n"
               "static int add(int, int);\n"
               "return add(17, 25);\n"),
            .exit_code = 42,
        },
        {
            "compound literal self-assign", __LINE__,
            SV("typedef struct v2f v2f;\n"
               "struct v2f { float x; float y; };\n"
               "v2f v = {1.0f, 2.0f};\n"
               "v = (v2f){v.y, v.x};\n"
               "return (int)(v.x * 10 + v.y);\n"),
            .exit_code = 21,
        },
    };
    int err;
    static int idx = 0;
    for(size_t i = test_atomic_increment(&idx); i < sizeof testcases/sizeof testcases[0]; i = test_atomic_increment(&idx)){
        struct tc* tc = &testcases[i];
        if(tc->skip){
            TEST_stats.skipped++;
            continue;
        }
        err = 0;
        TEST_stats.executed++;
        FileCache* fc = fc_create(al);
        if(!fc){err = 1; TestReport("setup failure"); goto finally;}
        MStringBuilder log_sb = {.allocator=al};
        MsbLogger logger_ = {0};
        Logger* logger = msb_logger(&logger_, &log_sb);
        AtomTable at = {.allocator = al};
        Environment env = {.allocator = al, .at=&at};
        CiInterpreter interp = {
            .exit_code = -1,
            .parser = {
                .cpp = {
                    .allocator = al,
                    .fc = fc,
                    .at = &at,
                    .logger = logger,
                    .env = &env,
                    .target = cc_target_funcs[CC_TARGET_TEST](),
                },
                .current = &interp.parser.global,
            },
            .top_frame = {
                .return_buf = &interp.exit_code,
                .return_size = sizeof interp.exit_code,
            },
        };
        LOCK_T_init(&interp.error_lock);
        fc_write_path(fc, __FILE__, sizeof __FILE__ - 1);
        err = fc_cache_file(fc, tc->program);
        if(err){TestReport("setup failure"); goto finally;}
        err = cpp_define_builtin_macros(&interp.parser.cpp);
        if(err){TestReport("setup failure"); goto finally;}
        err = cc_define_builtin_types(&interp.parser);
        if(err){TestReport("setup failure"); goto finally;}
        err = cc_register_pragmas(&interp.parser);
        if(err){TestReport("setup failure"); goto finally;}
        err = ci_register_pragmas(&interp);
        if(err){TestReport("setup failure"); goto finally;}
        err = ci_register_macros(&interp);
        if(err){TestReport("setup failure"); goto finally;}

        err = cpp_include_file_via_file_cache(&interp.parser.cpp, SV(__FILE__));
        if(err) {TestReport("failed to include"); goto finally;}
        ma_tail(interp.parser.cpp.frames).line = tc->line+1;

        err = cc_parse_all(&interp.parser);
        if(err){TestPrintf("%s:%d: failed to parse\n", __FILE__, tc->line); goto finally;}
        err = ci_resolve_refs(&interp, 0);
        if(err){TestPrintf("%s:%d: failed to link\n", __FILE__, tc->line); goto finally;}

        CiInterpFrame* frame = &interp.top_frame;
        frame->stmts = interp.parser.toplevel_statements.data;
        frame->stmt_count = interp.parser.toplevel_statements.count;
        while(frame->pc < frame->stmt_count){
            err = ci_interp_step(&interp, frame);
            if(err) goto finally;
        }
        TEST_stats.executed++;
        if(interp.exit_code != tc->exit_code){
            TEST_stats.failures++;
            TestPrintf("%s:%d: expected (%d) != actual (%d)\n", __FILE__, tc->line, tc->exit_code, interp.exit_code);
        }

        finally:
        if(log_sb.cursor && !log_sb.errored){
            StringView sv = msb_borrow_sv(&log_sb);
            TestPrintf("%.*s\n", sv_p(sv));
        }
        if(err) TEST_stats.failures++;
        ArenaAllocator_free_all(&arena);
        ArenaAllocator_free_all(&interp.parser.cpp.synth_arena);
        ArenaAllocator_free_all(&interp.parser.scratch_arena);
    }
    TESTEND();
}
int main(int argc, char** argv){
    #ifdef USE_TESTING_ALLOCATOR
        testing_allocator_init();
    #endif
    RegisterTestFlags(test_interpreter, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    int err = test_main(argc, argv, NULL);
    #ifdef USE_TESTING_ALLOCATOR
        testing_assert_all_freed();
    #endif
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "ci_interp.c"
#include "../Drp/Allocators/allocator.c"
#include "../Drp/file_cache.c"
#include "cpp_preprocessor.c"
#include "cc_parser.c"
#include "native_call.c"
