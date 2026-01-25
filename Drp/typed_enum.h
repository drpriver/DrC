#ifndef DRP_TYPED_ENUM_H
#define DRP_TYPED_ENUM_H

// Portable typed enums
//
// C23 and Clang allow specifying an enum's underlying type:
//   enum Foo : uint8_t { A, B, C };
//
// These macros provide a fallback for compilers that don't support this.
// On unsupported compilers, the typedef becomes the underlying type directly,
// so bitfields and sizeof() behave consistently.
//
// Usage:
//   enum Foo TYPED_ENUM(uint8_t) { FOO_A = 1, FOO_B = 2 };
//   typedef TYPED_ENUM_T(Foo, uint8_t) Foo;

#if defined(__clang__) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
  #define TYPED_ENUM(type) : type
  #define TYPED_ENUM_T_(name, type) enum name
#else
  #define TYPED_ENUM(type)
  #define TYPED_ENUM_T_(name, type) type
#endif
#define TYPEDEF_ENUM(name, type) typedef TYPED_ENUM_T_(name, type) name

#endif
