#ifndef C_CC_KEYWORDS_H
#define C_CC_KEYWORDS_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define CCKWS2(X) \
    X(do, do) \
    X(if, if) \

#define CCKWS3(X) \
    X(for, for) \
    X(int, int) \
    X(asm, asm) \

#define CCKWS4(X) \
    X(true, true) \
    X(long, long) \
    X(char, char) \
    X(auto, auto) \
    X(bool, bool) \
    X(else, else) \
    X(enum, enum) \
    X(case, case) \
    X(goto, goto) \
    X(void, void) \

#define CCKWS5(X) \
    X(__asm, asm) \
    X(break, break) \
    X(false, false) \
    X(float, float) \
    X(const, const) \
    X(short, short) \
    X(union, union) \
    X(while, while) \
    X(_Bool, bool) \
    X(_Type, _Type) \

#define CCKWS6(X) \
    X(double, double) \
    X(extern, extern) \
    X(inline, inline) \
    X(return, return) \
    X(signed, signed) \
    X(sizeof, sizeof) \
    X(static, static) \
    X(struct, struct) \
    X(switch, switch) \
    X(typeof, typeof) \

#define CCKWS7(X) \
    X(__asm__, asm) \
    X(alignas, alignas) \
    X(alignof, alignof) \
    X(default, default) \
    X(typedef, typedef) \
    X(nullptr, nullptr) \
    X(_Atomic, _Atomic) \
    X(_BitInt, _BitInt) \
    X(countof, _Countof) \
    X(__const, const) \

#define CCKWS8(X) \
    X(_Complex, _Complex) \
    X(continue, continue) \
    X(register, register) \
    X(restrict, restrict) \
    X(unsigned, unsigned) \
    X(volatile, volatile) \
    X(_Generic, _Generic) \
    X(_Countof, _Countof) \
    X(_Float16, _Float16) \
    X(_Float32, _Float32) \
    X(_Float64, _Float64) \
    X(_Alignas, alignas) \
    X(_Alignof, alignof) \
    X(__inline, inline) \
    X(__signed, signed) \
    X(__thread, thread_local) \
    X(noreturn, _Noreturn)\
    X(__typeof, typeof) \
    X(__int128, __int128) \

#define CCKWS9(X) \
    X(constexpr, constexpr) \
    X(_Noreturn, _Noreturn) \
    X(_Float128, _Float128) \
    X(_Float32x, _Float32x) \
    X(_Float64x, _Float64x) \
    X(__const__, const) \
    X(__alignof, alignof) \

#define CCKWS10(X) \
    X(_Imaginary, _Imaginary) \
    X(_Decimal32, _Decimal32) \
    X(_Decimal64, _Decimal64) \
    X(__inline__, inline) \
    X(__volatile, volatile) \
    X(__typeof__, typeof) \
    X(__restrict, restrict) \
    X(__signed__, signed) \
    X(__declspec, __declspec) \

#define CCKWS11(X) \
    X(_Decimal128, _Decimal128) \
    X(__auto_type, __auto_type) \
    X(__attribute, __attribute__) \
    X(__alignof__, alignof) \

#define CCKWS12(X) \
    X(thread_local, thread_local) \
    X(__restrict__, restrict) \

#define CCKWS13(X) \
    X(static_assert, static_assert) \
    X(typeof_unqual, typeof_unqual) \
    X(_Thread_local, thread_local) \
    X(__attribute__, __attribute__) \
    X(__forceinline, inline) \

#define CCKWS14(X) \
    X(_Static_assert, static_assert) \

#define CCKWS17(X) \
    X(__typeof_unqual__, typeof_unqual) \

#define CCKWS(X) \
    CCKWS2(X) \
    CCKWS3(X) \
    CCKWS4(X) \
    CCKWS5(X) \
    CCKWS6(X) \
    CCKWS7(X) \
    CCKWS8(X) \
    CCKWS9(X) \
    CCKWS10(X) \
    CCKWS11(X) \
    CCKWS12(X) \
    CCKWS13(X) \
    CCKWS14(X) \
    CCKWS17(X) \

#endif
