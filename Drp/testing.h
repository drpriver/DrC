//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#ifndef TESTING_H
#define TESTING_H
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#ifdef _WIN32
#include <direct.h> // for chdir
#define chdir _chdir
#else
#include <unistd.h>
#endif
#include "stringview.h"
#include "dbg_diff.h"


#ifndef arrlen
#define arrlen(arr) (sizeof(arr)/sizeof(arr[0]))
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline static inline __attribute__((always_inline))
#else
#define force_inline static inline
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#define _Nonnull
#define _Nullable
#define _Null_unspecified
#endif

static void diff_print(void*_Null_unspecified up, const char* fmt, ...);
static DbgDiffPrinter TEST_DIFF_PRINTER = {
#ifndef __DRC__
    .printer=diff_print,
#endif
    .escape_on    = "\033[4m",
    .escape_reset = "\033[24m",
    .deleted      = "\033[94m- ",
    .matched      = "  ",
    .added        = "\033[33m+ ",
    .reset        = "\033[0m",
};

#ifndef TestDebugBreak
    #if defined __DRC__
        #define TestDebugBreak() __builtin_debugtrap()
    #elif defined __clang__
        #define TestDebugBreak() __builtin_debugtrap()
    #elif defined _MSC_VER
        #define __builtin_debugtrap() __debugbreak()
    #elif defined __GNUC__
        #if defined __x86_64__ || defined __i386__
            #define TestDebugBreak() asm("int $3")
        // This is untested, as it requires gcc + arm64
        #elif defined __aarch64__
            #define TestDebugBreak() asm("brk #0xf000")
        #else
            #define TestDebugBreak() __builtin_trap()
        #endif
    #else
        #define TestDebugBreak()
    #endif
#endif


// BUGS
// ----
// We use __auto_type in the testing macros, so this will only compile
// with gcc and clang.
// Maybe one day C will standardize auto.
//
// We also use statement expressions. In theory capturing lambdas
// could replace that if those are added to C23.

// color defs
// ----------
// Internal use color definitions. They will be set to escape codes if
// stderr is detected to be interactive.
static const char* _test_color_gray  = "";
static const char* _test_color_reset = "";
static const char* _test_color_green = "";
static const char* _test_color_red   = "";
_Bool _test_do_debugbreak_on_fail = 0;
#if 0 // Currently these are unused.
static const char* _test_color_blue  = "";
#endif

// TestOutFiles
// ------------
// The files that will be printed to. Don't use this directly, use
// `TestRegisterOutFile`
static FILE*_Null_unspecified TestOutFiles[9] = {0};
static size_t TestOutFileCount = 0;

// TestRegisterOutFile
// -------------------
// This is intended for internal use, but you can use it to register an output
// file that any printing will print to.
static void
TestRegisterOutFile(FILE* fp){
    if(TestOutFileCount >= arrlen(TestOutFiles))
        return;
    TestOutFiles[TestOutFileCount++] = fp;
}


static void TestPrintfv(const char* fmt, va_list);
// TestPrintf
// ----------
// Provides a printf-style interface. Prints to all of the registered
// `TestOutFiles`.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((__format__(__printf__, 1, 2)))
#endif
static
void
TestPrintf(const char* fmt, ...){
    va_list arg_;
    va_start(arg_, fmt);
    TestPrintfv(fmt, arg_);
    va_end(arg_);
}
static
void
TestPrintfv(const char* fmt, va_list arg_){
    char buff[10000];
    assert(TestOutFileCount < arrlen(TestOutFiles));
    for(size_t i = 0; i < TestOutFileCount; i++){
        va_list arg;
        va_copy(arg, arg_);
        FILE* fp = TestOutFiles[i];
        if(fp == stderr || fp == stdout)
            vfprintf(fp, fmt, arg);
        else {
            // janky control code stripper:
            // FIXME:
            // This is too late to strip control codes
            // since the test output could have control codes and thus
            // diffing outputs would be wrong.
            // Oh well.
            int printed = (vsnprintf)(buff, sizeof buff, fmt, arg);
            if(printed > (int)sizeof buff) printed = sizeof buff;
            char* p = buff;
            char* begin = p;
            char* end = p + printed;
            while(p != end){
                if(*p == '\033'){
                    if(p != begin){
                        fwrite(begin, p - begin, 1, fp);
                    }
                    while(p != end && *(p++) != 'm')
                        ;
                    begin = p;
                    continue;
                }
                p++;
            }
            if(begin != end)
                fwrite(begin, end - begin, 1, fp);
        }
        va_end(arg);
    }
}

static void diff_print(void*_Null_unspecified up, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    TestPrintfv(fmt, ap);
    va_end(ap);
    (void)up;
}

#ifndef TestPrintValue

#if defined(__GNUC__) || defined(__clang__)
#define TestPrintFuncs(apply) \
    apply(bool, _Bool, "%s", x?"true":"false")\
    apply(char, char, "%c", x)\
    apply(uchar, unsigned char, "%u", x) \
    apply(schar, signed char, "%d", x) \
    apply(float, float, "%f", (double)x) \
    apply(double, double, "%f", x) \
    apply(short, short, "%d", x) \
    apply(ushort, unsigned short, "%u", x) \
    apply(int, int, "%d", x)\
    apply(uint, unsigned int, "%u", x) \
    apply(long, long, "%ld", x) \
    apply(ulong, unsigned long, "%lu", x) \
    apply(llong, long long, "%lld", x) \
    apply(ullong, unsigned long long, "%llu", x) \
    apply(cstr, char*, "\"%s\"", x) \
    apply(ccstr, const char*, "\"%s\"", x) \
    apply(pvoid, void*, "%p", x) \
    apply(pcvoid, const void*, "%p", x) \
    apply(StringView, StringView, "\"%.*s\"", (int)x.length, x.text)
#else
// MSVC treats char and signed char as the same, even though they should
// be distinct. Sigh. This means we can't distinguish between int8_t and
// char. So drop char from this.
#define TestPrintFuncs(apply) \
    apply(bool, _Bool, "%s", x?"true":"false")\
    apply(uchar, unsigned char, "%u", x) \
    apply(schar, signed char, "%d", x) \
    apply(float, float, "%f", (double)x) \
    apply(double, double, "%f", x) \
    apply(short, short, "%d", x) \
    apply(ushort, unsigned short, "%u", x) \
    apply(int, int, "%d", x)\
    apply(uint, unsigned int, "%u", x) \
    apply(long, long, "%ld", x) \
    apply(ulong, unsigned long, "%lu", x) \
    apply(llong, long long, "%lld", x) \
    apply(ullong, unsigned long long, "%llu", x) \
    apply(cstr, char*, "\"%s\"", x) \
    apply(ccstr, const char*, "\"%s\"", x) \
    apply(pvoid, void*, "%p", x) \
    apply(pcvoid, const void*, "%p", x) \
    apply(StringView, StringView, "\"%.*s\"", (int)x.length, x.text)
#endif
#define TestPrintFunc(suffix, type, unused, ...) type: TestPrintImpl_##suffix,

#define TestPrintValue(str, val) \
    _Generic(val, \
    TestPrintFuncs(TestPrintFunc) \
    struct{int foo;}: 0)(__FILE__, __func__, __LINE__, str, val)

#define TestPrintImpl_(suffix, type, fmt, ...) \
    force_inline void \
    TestPrintImpl_##suffix(const char* file, const char* func, int line, const char* str, type x){ \
        TestPrintf("%s%s:%d: %s%s %s = " fmt "\n",\
                _test_color_gray, file, line, func, _test_color_reset, str, __VA_ARGS__); \
        }
TestPrintFuncs(TestPrintImpl_)

#endif



// TestFunction, TESTBEGIN, TESTEND
// --------------------------------
//
// Macros for defining a test function.
// TESTBEGIN must be paired with TESTEND().
// You should use semicolons after them.
//
// Example use:
//
// TestFunction(TestFooIsTwo){
//     TESTBEGIN();
//     TestExpectEquals(foo(), 2);
//     TESTEND();
// }
//

#define TestFunction(name) static struct TestStats name(void)
#define TESTBEGIN() struct TestStats TEST_stats = {0}; {
#define TESTEND() } return TEST_stats

//
// SUPPRESS_TEST_MAIN
// ------------------
// If this macro is defined, suppresses generating a test_main function. You will
// be responsible for calling run_the_tests yourself and reporting the results.

// #define SUPPRESS_TEST_MAIN


//
// TestStats
// ---------
// Internal use struct to keep track of the number of tests executed, failed,
// etc.
struct TestStats {
    unsigned long long funcs_executed;
    unsigned long long failures;
    unsigned long long executed;
    unsigned long long assert_failures;
    unsigned long long skipped;
};

enum {MAX_TEST_NUM = 1000};

// TestResults
// ----------
// Internal use struct.
struct TestResults {
    size_t funcs_executed;
    size_t failures;
    size_t executed;
    size_t assert_failures;
    size_t skips;
    size_t failed_tests[MAX_TEST_NUM];
    size_t n_failed_tests;
};

// TestFunc
// --------
// The type of a test function.
typedef struct TestStats (TestFunc)(void);

// TestCaseFlags
// -------------
// Flags that can be used to control the behavior of tests, used with `RegisterTestFlags`
enum TestCaseFlags {
    TEST_CASE_FLAGS_NONE = 0x0, // No flags

    // TEST_CASE_FLAGS_SKIP_UNLESS_NAMED
    // ---------------------------------
    // Skip this test unless specifically named on the command line.
    // This is useful for slow or exhaustive tests that don't need to be run
    // on every change, but you want to keep them compiling and in tree.
    TEST_CASE_FLAGS_SKIP_UNLESS_NAMED = 0x1,

    // TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD
    // ------------------------------------------
    // When running multithreaded, each thread will invoke this test function.
    // The test function is expected to use an internal atomic to distribute
    // its test cases across the threads.
    TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD = 0x2,
};

// TestCase
// --------
// Internal use.
typedef struct TestCase TestCase;
struct TestCase {
    StringView test_name;
    TestFunc* test_func;
    enum TestCaseFlags flags;
};


// RegsterTest
// -----------
//
// Register a test for execution.
// Use this before calling test_main
// Implemented as a macro to capture the name of the test
//
// Example:
// --------
//   int main(int argc, char**argv){
//       RegisterTest(TestFooIsTwo);
//       RegisterTest(TestBarIsNotBaz);
//       return test_main(argc, argv);
//   }
//
#define RegisterTest(tf) register_test(SV(#tf), tf, TEST_CASE_FLAGS_NONE)
// RegisterTestFlags
// -----------------
// Ditto, but allows specifying flags.
#define RegisterTestFlags(tf, flags) register_test(SV(#tf), tf, flags)

static inline
void
register_test(StringView test_name, TestFunc* func, enum TestCaseFlags flags);

// test_names
// ----------
// Internal use, use the RegisterTest function to register a test.
// This array is where registered tests are located.
// A single test program can not directly register more than 1000 tests.
static StringView test_names[MAX_TEST_NUM];
static TestCase test_funcs[MAX_TEST_NUM];
// test_funcs_count
// ----------------
// How many were registered. Internal use.
static size_t test_funcs_count;

// register_test
// -------------
// The "raw" version of `RegisterTest`. Generally you should use `RegisterTest`
// or `RegisterTestFlags`, but you might need to dynamically register a test
// and so can use this function instead.
static inline
void
register_test(StringView test_name, TestFunc* func, enum TestCaseFlags flags){
    assert(test_funcs_count < arrlen(test_funcs));
    test_names[test_funcs_count] = test_name;
    test_funcs[test_funcs_count++] = (TestCase){
        .test_name = test_name,
        .test_func=func,
        .flags=flags,
    };
}


//
// TestReport
// ----------
// Internal use macro to report test results within a failed condition.
// You can use it if you want in your test functions if you need more reporting.
// It's an fprintf wrapper (appends a newline though).
//
#define TestReport(fmt, ...) \
    TestPrintf("%s%s:%d: %s %s" fmt "\n",\
        _test_color_gray, __FILE__, __LINE__, __func__, \
        _test_color_reset, ##__VA_ARGS__);

//
// Test Conditions
// --------------
// These macros are for expressing the test conditions.
// They only work within a test function.
//
// See:
// ----
// * `TestExpect`
// * `TestExpectEquals`
// * `TestExpectEquals2`
// * `TestExpectNotEquals`
// * `TestExpectNotEqual2`
// * `TestExpectTrue`
// * `TestExpectFalse`
// * `TestExpectSuccess`
// * `TestExpectFailure`
//
// Beware:
// -------
// With clang and gcc, we can use typeof or __auto_type to turn the lhs and rhs
// of these conditions into local variables so that we don't have to worry
// about multiple evaluation of the arguments.
//
// However, other compilers (MSVC in C mode) don't support that, so we are
// faced between the choice of requiring the user to pass in the type as one of
// the macro args (which is error prone) or allow double evaluation. I have
// chosen to allow double evaluation with MSVC. Tests with side effects is
// a bad idea anyway.

  // TestExpect
  // ----------------
  // Expects lhs op rhs, using the op operator
  //
#if !defined(__IMPORTC__) && (defined(__GNUC__) || defined(__clang__))
  #define TestExpect(lhs, op, rhs) do {\
          __auto_type _lhs = lhs; \
          typeof(lhs) _rhs = rhs; \
          TEST_stats.executed++;\
          if (!(_lhs op _rhs)) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s %s %s", #lhs, #op, #rhs); \
              TestPrintValue(#lhs, _lhs);\
              TestPrintValue(#rhs, _rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      }while(0)
#else
  #define TestExpect(lhs, rhs) do {\
          TEST_stats.executed++;\
          if (!((lhs) op (rhs))) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s %s %s", #lhs, #op, #rhs); \
              TestPrintValue(#lhs, lhs);\
              TestPrintValue(#rhs, rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      }while(0)
#endif

static
_Bool
test_expect_equals_sv(StringView lhs, StringView rhs, const char* lhs_, const char* rhs_, struct TestStats* stats, const char* file, const char* func, int line){
    stats->executed++;
    if(!sv_equals(lhs, rhs)){
        stats->failures++;
        TestPrintf("%s%s:%d: %s %s" "Test condition failed" "\n",
            _test_color_gray, file, line, func,
            _test_color_reset);
        TestPrintf("%s%s:%d: %s %s" "%s == %s" "\n",
            _test_color_gray, file, line, func,
            _test_color_reset, lhs_, rhs_);
        dbg_diff(&TEST_DIFF_PRINTER, lhs, rhs);
        if(_test_do_debugbreak_on_fail)
            TestDebugBreak();
        return 0;
    }
    return 1;
}
#define TestExpectEqualsSv(lhs, rhs) test_expect_equals_sv(lhs, rhs, #lhs, #rhs, &TEST_stats, __FILE__, __func__, __LINE__)
  //
  // TestExpectEquals
  // ----------------
  // Expects lhs == rhs, using the == operator
  //
#if !defined(__IMPORTC__) && (defined(__GNUC__) || defined(__clang__))
  #define TestExpectEquals(lhs, rhs) do {\
          __auto_type _lhs = lhs; \
          typeof(lhs) _rhs = rhs; \
          TEST_stats.executed++;\
          if (!(_lhs == _rhs)) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s == %s", #lhs, #rhs); \
              TestPrintValue(#lhs, _lhs);\
              TestPrintValue(#rhs, _rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      }while(0)
#else
  #define TestExpectEquals(lhs, rhs) do {\
          TEST_stats.executed++;\
          if (!((lhs) == (rhs))) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s == %s", #lhs, #rhs); \
              TestPrintValue(#lhs, lhs);\
              TestPrintValue(#rhs, rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      }while(0)
#endif
  //
  // TestExpectEquals2
  // -----------------
  // Expects lhs == rhs, using the passed in binary function instead of == operator
  //
#if !defined(__IMPORTC__) && (defined(__GNUC__) || defined(__clang__))
  #define TestExpectEquals2(func, lhs, rhs) do {\
          __auto_type _lhs = lhs; \
          __auto_type _rhs = rhs; \
          TEST_stats.executed++;\
          if (!(func(_lhs, _rhs))) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("!%s(%s, %s)", #func, #lhs, #rhs); \
              TestPrintValue(#lhs, _lhs);\
              TestPrintValue(#rhs, _rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      } while(0)
#else
  #define TestExpectEquals2(func, lhs, rhs) do {\
          TEST_stats.executed++;\
          if (!(func(lhs, rhs))) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("!%s(%s, %s)", #func, #lhs, #rhs); \
              TestPrintValue(#lhs, lhs);\
              TestPrintValue(#rhs, rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      } while(0)
#endif

  //
  // TestExpectNotEquals
  // -------------------
  // Expects lhs != rhs, using the != operator
  //
#if !defined(__IMPORTC__) && (defined(__GNUC__) || defined(__clang__))
  #define TestExpectNotEquals(lhs, rhs) do {\
          __auto_type _lhs = lhs; \
          typeof(lhs) _rhs = rhs; \
          TEST_stats.executed++;\
          if (!(_lhs != _rhs)) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s != %s", #lhs, #rhs); \
              TestPrintValue(#lhs, _lhs);\
              TestPrintValue(#rhs, _rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      }while(0)
#else
  #define TestExpectNotEquals(lhs, rhs) do {\
          TEST_stats.executed++;\
          if (!((lhs) != (rhs))) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s != %s", #lhs, #rhs); \
              TestPrintValue(#lhs, lhs);\
              TestPrintValue(#rhs, rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      }while(0)
#endif

  //
  // TestExpectNotEqual2
  // -------------------
  // Checks for func(lhs, rhs) == 0
  //
#if !defined(__IMPORTC__) && (defined(__GNUC__) || defined(__clang__))
  #define TestExpectNotEqual2(func, lhs, rhs) do{\
          __auto_type _lhs = lhs; \
          __auto_type _rhs = rhs; \
          TEST_stats.executed++;\
          if (func(_lhs, _rhs)) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s(%s, %s)", #func, #lhs, #rhs); \
              TestPrintValue(#lhs, _lhs);\
              TestPrintValue(#rhs, _rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
              }\
          }while(0)
#else
  #define TestExpectNotEqual2(func, lhs, rhs) do{\
          TEST_stats.executed++;\
          if (func(lhs, rhs)) {\
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s(%s, %s)", #func, #lhs, #rhs); \
              TestPrintValue(#lhs, lhs);\
              TestPrintValue(#rhs, rhs);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
              }\
          }while(0)
#endif

  //
  // TestExpectTrue
  // --------------
  // Expects the condition is truthy (for the usual C definition of truth).
  //
  #define TestExpectTrue(cond) do {\
          TEST_stats.executed++;\
          _Bool cond_ = !!(cond); \
          if (! (cond_)){ \
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s", #cond);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      }while(0)

  //
  // TestExpectFalse
  // ---------------
  // Expects the condition is falsey (for the usual C definition of truth).
  //
  #define TestExpectFalse(cond) do{\
          _Bool cond_ = !!(cond); \
          TEST_stats.executed++;\
          if (cond_){ \
              TEST_stats.failures++; \
              TestReport("Test condition failed (expected falsey)");\
              TestPrintValue(#cond, cond);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      }while(0)

  //
  // TestExpectSuccess
  // -----------------
  // For an errorable (struct with .errored field), expects .errored is 0
  //
  #define TestExpectSuccess(cond) do{\
          TEST_stats.executed++;\
          if ((cond).errored){ \
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s = %d", #cond, (cond).errored);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
          }\
      }while(0)

  //
  // TestExpectFailure
  // -----------------
  // For an errorable (struct with .errored field), expects .errored is not 0
  //
  #define TestExpectFailure(cond) do{\
          TEST_stats.executed++;\
          if (!(cond).errored){ \
              TEST_stats.failures++; \
              TestReport("Test condition failed");\
              TestReport("%s = %d", #cond, (cond).errored);\
              if(_test_do_debugbreak_on_fail) \
                  TestDebugBreak(); \
              }\
          }while(0)

  //
  // TestArrayContains
  // -----------------
  // Check that a value is in an array, using `==`.
  //
  #define TestArrayContains(type, begin, end, item) do {\
      _Bool found = 0; \
      type item_ = item; \
      for(type* it = begin; it != end; it++){ \
          if(*it == item_){ \
              found = 1; \
              break; \
          } \
      } \
      TEST_stats.executed++; \
      if(!found){ \
          TEST_stats.failures++; \
          TestReport("Test condition failed"); \
          TestReport("%s not in [%s, %s)\n", #item, #begin, #end); \
          if(_test_do_debugbreak_on_fail) \
              TestDebugBreak(); \
      } \
  }while(0)
  //
  // TestArrayContains2
  // -----------------
  // Check that a value is in an array, using provided func
  //
  #define TestArrayContains2(func, type, begin, end, item) do {\
      _Bool found = 0; \
      type item_ = item; \
      for(type* it = begin; it != end; it++){ \
          if(func(*it, item_)){ \
              found = 1; \
              break; \
          } \
      } \
      TEST_stats.executed++; \
      if(!found){ \
          TEST_stats.failures++; \
          TestReport("Test condition failed"); \
          TestReport("%s not in [%s, %s)\n", #item, #begin, #end); \
          if(_test_do_debugbreak_on_fail) \
              TestDebugBreak(); \
      } \
  }while(0)

// TestAsserts
// -----------
// Unlike the TestExpect* family of macros, TestAssert* macros immediately end
// execution of the test function.
// The program is not halted, just the test function immediately returns.
// You will need to cleanup any state. In the future we might have a
// cleanup section in test functions.
//
// See:
// ----
// * `TestAssert`
// * `TestAssertFalse`
// * `TestAssertEquals`
// * `TestAssertEquals2`
// * `TestAssertSuccess`
// * `TestAssertFailure`
// * `EndTest`

//
// TestAssert
// ----------
// Asserts the condition is truthy.
//
#define TestAssert(cond) do{\
        TEST_stats.executed++;\
        if (! (cond)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s", #cond); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)

//
// TestAssertFalse
// ---------------
// Ditto, but for falsey
#define TestAssertFalse(cond) do{\
        TEST_stats.executed++;\
        if ((cond)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s", #cond); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)

//
// TestAssertEquals
// ----------------
// Asserts lhs is equal to rhs, using ==
//
#if !defined(__IMPORTC__) && (defined(__GNUC__) || defined(__clang__))
  #define TestAssertEquals(lhs, rhs) do{\
        __auto_type _lhs = lhs; \
        typeof(lhs) _rhs = rhs; \
        TEST_stats.executed++;\
        if (! (_lhs==_rhs)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s == %s", #lhs, #rhs); \
            TestPrintValue(#lhs, _lhs);\
            TestPrintValue(#rhs, _rhs); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)
#else
  #define TestAssertEquals(lhs, rhs) do{\
        TEST_stats.executed++;\
        if (! ((lhs)==(rhs))){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s == %s", #lhs, #rhs); \
            TestPrintValue(#lhs, lhs);\
            TestPrintValue(#rhs, rhs); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)
#endif
//
// TestAssertNotEqual
// ----------------
// Asserts lhs is equal to rhs, using ==
//
#if !defined(__IMPORTC__) && (defined(__GNUC__) || defined(__clang__))
  #define TestAssertNotEqual(lhs, rhs) do{\
        __auto_type _lhs = lhs; \
        typeof(lhs) _rhs = rhs; \
        TEST_stats.executed++;\
        if (! (_lhs!=_rhs)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s != %s", #lhs, #rhs); \
            TestPrintValue(#lhs, _lhs);\
            TestPrintValue(#rhs, _rhs); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)
#else
  #define TestAssertNotEqual(lhs, rhs) do{\
        TEST_stats.executed++;\
        if (! ((lhs)!=(rhs))){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s != %s", #lhs, #rhs); \
            TestPrintValue(#lhs, lhs);\
            TestPrintValue(#rhs, rhs); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)
#endif
//
// TestAssertEquals2
// ----------------
// Asserts lhs is equal to rhs, using ==
//
#if !defined(__IMPORTC__) && (defined(__GNUC__) || defined(__clang__))
  #define TestAssertEquals2(func, lhs, rhs) do{\
        __auto_type _lhs = lhs; \
        typeof(lhs) _rhs = rhs; \
        TEST_stats.executed++;\
        if (!func(_lhs, _rhs)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s == %s", #lhs, #rhs); \
            TestPrintValue(#lhs, _lhs);\
            TestPrintValue(#rhs, _rhs); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)
#else
  #define TestAssertEquals2(func, lhs, rhs) do{\
        TEST_stats.executed++;\
        if (!func(lhs, rhs)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s == %s", #lhs, #rhs); \
            TestPrintValue(#lhs, lhs);\
            TestPrintValue(#rhs, rhs); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)
#endif

//
// TestAssertSuccess
// -----------------
// For an errorable (struct with .errored field), asserts .errored is 0
//
#define TestAssertSuccess(cond) do{\
        TEST_stats.executed++;\
        if ((cond).errored){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s = %d", #cond, (cond).errored); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)

//
// TestAssertFailure
// -----------------
// For an errorable (struct with .errored field), asserts .errored is not 0
//
#define TestAssertFailure(cond) do{\
        TEST_stats.executed++;\
        if ((!cond.errored)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s = %d", #cond, (cond).errored); \
            if(_test_do_debugbreak_on_fail) \
                TestDebugBreak(); \
            return TEST_stats;\
        }\
    }while(0)

//
// EndTest
// -------
// Immediately ends the test function, counting as an early termination of
// the test and reports the message given by reason.
//
#define EndTest(reason) do{\
        TestReport("Test ended early"); \
        TestReport("Reason: %s", reason); \
        TEST_stats.assert_failures++;\
        return TEST_stats;\
    }while(0)

//
// run_the_tests
// -------------
// The actual test runner.
// You can call this in your own test_main or other function.
// Otherwise, don't call this directly.
//
// Arguments:
// ----------
//   which_tests:
//     A pointer to an array of indexes into the registered `test_funcs` table.
//
//   test_count:
//     The length of the array pointed to by which_tests
//
//   result:
//      A `TestResults` structure with the number of passess, fails, etc.
//      Results will be written into these.
//
static
void
run_the_tests(size_t*_Nonnull which_tests, int test_count, struct TestResults* result){
    for(int i = 0; i < test_count; i++){
        size_t idx = which_tests[i];
        TestFunc* func = test_funcs[idx].test_func;
        assert(func);
        struct TestStats func_result = func();
        result->funcs_executed++;
        result->failures += func_result.failures;
        result->executed += func_result.executed;
        result->assert_failures += func_result.assert_failures;
        if(func_result.assert_failures || func_result.failures)
            result->failed_tests[result->n_failed_tests++] = idx;
        result->skips += func_result.skipped;
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

// SUPPRESS_TEST_MAIN
// ------------------
// If you define this, `test_main` will not be defined and you will need to
// implement your own test_main.
#ifndef SUPPRESS_TEST_MAIN
#include "argument_parsing.h"
#include "term_util.h"
#include "thread_utils.h"

// shuffling stuff
#ifdef __linux__
// getrandom
#include <sys/random.h>
#elif defined(__APPLE__)
#include <stdlib.h> // arc4random
#elif defined(_WIN32)
//
#include <ntsecapi.h> // RtlGenRandom
#elif defined(__wasm__)

#else
#error "Don't know how to get entropy on this system"
#endif
static uint64_t _testing_rng_inc;
static uint64_t _testing_rng_state;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning( disable : 4146 )
#endif

static inline
uint32_t testing_rng_random(void){
    uint64_t oldstate = _testing_rng_state;
    _testing_rng_state = oldstate * 6364136223846793005ULL + _testing_rng_inc;
    uint32_t xorshifted = (uint32_t) ( ((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

static inline
void
testing_seed_rng(uint64_t*_Nonnull seed_){
    uint64_t seed = *seed_;
    while(!seed){
#if defined(__APPLE__)
        arc4random_buf(&seed, sizeof seed);
#elif defined(__linux__)
        ssize_t r = getrandom(&seed, sizeof seed, 0);
        (void)r;
#elif defined(__wasm__)
        seed = 1234;
#elif defined(_WIN32)
        // Apparently this has to be dynamically loaded
        HMODULE lib = LoadLibraryW(L"Advapi32.dll");
        assert(lib);
        typedef BOOLEAN(RtlGenRandomT)(PVOID, ULONG);
        RtlGenRandomT* gr = (RtlGenRandomT*)GetProcAddress(lib, "SystemFunction036"); // RtlGenRandom
        BOOLEAN r = gr(&seed, sizeof seed);
        (void)r;
        FreeLibrary(lib);
#else
#error "Don't know how to get entropy on this system"
#endif
    }
    _testing_rng_inc = (16149396009930002229u<<1u)|1u;
    _testing_rng_state = 0;
    (void)testing_rng_random();
    _testing_rng_state += seed;
    (void)testing_rng_random();
    *seed_ = seed;
}

static inline
void
shuffle_tests(size_t*_Nonnull which_tests, int test_count){
    if(test_count < 2) return;
    for(int i = 0; i < test_count; i++){
        size_t j = (testing_rng_random() % (test_count-i)) + i;
        size_t tmp = which_tests[i];
        which_tests[i] = which_tests[j];
        which_tests[j] = tmp;
    }
}

struct TestJobData {
    size_t* _Nonnull which_tests;
    int test_count;
    int*_Nonnull test_idx;
    struct TestResults result;
};


#if defined(__GNUC__)
force_inline
int
test_atomic_increment(int*_Nonnull p){
    return __atomic_fetch_add(p, 1, __ATOMIC_SEQ_CST);
}

#elif defined(_MSC_VER)
force_inline
int
test_atomic_increment(int*_Nonnull p){
    // _Static_assert(sizeof(LONG) == sizeof(int), "");
    return InterlockedIncrement((LONG volatile*)p)-1;
}
#else
#include <stdatomic.h>
force_inline
int
test_atomic_increment(int*_Nonnull p){
    _Static_assert(ATOMIC_INT_LOCK_FREE, "");
    return atomic_fetch_add((_Atomic(int)*)p, 1);
}
#endif

static
ThreadReturnValue
test_thread_worker(void*_Nonnull thread_arg){
    struct TestJobData* jd = thread_arg;
    // First: run all DUPLICATE_FOR_EACH_THREAD tests (each thread runs all of them)
    for(int i = 0; i < jd->test_count; i++){
        size_t which_test = jd->which_tests[i];
        if(!(test_funcs[which_test].flags & TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD))
            continue;
        TestFunc* func = test_funcs[which_test].test_func;
        assert(func);
        struct TestStats func_result = func();
        jd->result.failures += func_result.failures;
        jd->result.executed += func_result.executed;
        jd->result.assert_failures += func_result.assert_failures;
        if(func_result.assert_failures || func_result.failures)
            jd->result.failed_tests[jd->result.n_failed_tests++] = i;
    }
    // Then: work-steal non-DUPLICATE tests
    for(;;){
        int idx = test_atomic_increment(jd->test_idx);
        if(idx >= jd->test_count)
            break;
        size_t which_test = jd->which_tests[idx];
        if(test_funcs[which_test].flags & TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD)
            continue;
        TestFunc* func = test_funcs[which_test].test_func;
        assert(func);
        struct TestStats func_result = func();
        jd->result.failures += func_result.failures;
        jd->result.executed += func_result.executed;
        jd->result.assert_failures += func_result.assert_failures;
        if(func_result.assert_failures || func_result.failures)
            jd->result.failed_tests[jd->result.n_failed_tests++] = idx;
    }
    return 0;
}

static
void
run_the_tests_multithreaded(size_t*_Nonnull which_tests, int test_count, struct TestResults*_Nonnull result, int num_threads){
    struct TestJobData jds[32];
    ThreadHandle handles[32];
    if(num_threads > 32) num_threads = 32;
    if(num_threads < 1) num_threads = 1;
    int test_idx = 0;
    for(int i = 0; i < num_threads-1; i++){
        jds[i] = (struct TestJobData){
            .which_tests = which_tests,
            .test_count = test_count,
            .test_idx = &test_idx,
            .result = {0},
        };
        int err = create_thread(&handles[i], &test_thread_worker, &jds[i]);
        if(err) {
            fprintf(stderr, "error spawning thread\n");
            abort();
        }
    }
    jds[num_threads-1] = (struct TestJobData){
        .which_tests = which_tests,
        .test_count = test_count,
        .test_idx = &test_idx,
        .result = {0},
    };
    test_thread_worker(&jds[num_threads-1]);
    for(int i = 0 ;i < num_threads-1; i++){
        join_thread(handles[i]);
    }
    result->funcs_executed += test_count;
    for(int i = 0; i < num_threads; i++){
        struct TestResults* r = &jds[i].result;
        result->failures += r->failures;
        result->executed += r->executed;
        result->assert_failures += r->assert_failures;
        result->skips += r->skips;
        for(size_t n = 0; n < r->n_failed_tests; n++){
            size_t ft = r->failed_tests[n];
            for(size_t m = 0; m < result->n_failed_tests; m++)
                if(result->failed_tests[m] == ft)
                    goto already;
            result->failed_tests[result->n_failed_tests++] = ft;
            already:;
        }
    }
}

//
// test_main
// ------------------
// The default test_main implementation if you don't suppress it.
// Executes `run_the_tests` and pretty prints the results to the terminal.
// NOTE: you will need to register at least one file pointer for TestPrintf if you
// don't use this function.
//
static
int
test_main(int argc, char*_Nonnull *_Nonnull argv, const ArgParseKwParams*_Nullable extra_kwargs){
    if(argc < 1){
        fprintf(stderr, "Somehow this program was called without an argv.\n");
        return 1;
    }
    const char* filename = argv[0];
    _Bool no_colors = 0;
    _Bool force_colors = 0;
    _Bool run_all = 0;
    #ifndef __wasm__
    StringView directory = {0};
    #endif
    size_t tests_to_run[arrlen(test_funcs)] = {0};
    struct ArgParseEnumType targets = {
        .enum_size = sizeof(size_t),
        .enum_count = test_funcs_count,
        .enum_names = test_names,
    };
    #ifndef __wasm__
    StringView outfile = {0};
    StringView extrafiles[8] = {0};
    _Bool append = 0;
    _Bool print_pid = 0;
    _Bool should_wait = 0;
    #endif
    int nreps = 1;
    _Bool shuffle = 0;
    _Bool silent = 0;
    uint64_t seed = 0;
    #ifndef __wasm__
    enum {TEE_INDEX=7, TARGET_INDEX=3};
    #else
    enum {TARGET_INDEX=2};
    #endif
    #if !defined(__wasm__)
    _Bool multithreaded = 0;
    #endif
    int n_threads = 0;
    int n_cpus = num_cpus();
    char j_help[128];
    (snprintf)(j_help, sizeof j_help, "Use as many threads as cpus (%d)", n_cpus);

    ArgToParse kw_args[] = {
        #ifndef __wasm__
        {
            .name = SV("-C"),
            .altname1 = SV("--change-directory"),
            .dest = ARGDEST(&directory),
            .help = "Directory to change the working directory to before "
                    "executing tests.",
        },
        #endif
        {
            .name = SV("--no-colors"),
            .dest = ARGDEST(&no_colors),
            .help = "Dont use ANSI escape codes to print colors in reporting.",
        },
        {
            .name = SV("--force-colors"),
            .dest = ARGDEST(&force_colors),
            .help = "Always use ANSI escape codes to print colors in reporting, "
                    "even if output is not a tty.",
        },
        [TARGET_INDEX] = {
            .name = SV("-t"),
            .altname1 = SV("--target"),
            .max_num = test_funcs_count,
            .dest = ArgEnumDest(tests_to_run, &targets),
            .help = "If given, only run the named test function. If not given, "
                    "all tests will be run. Specify by name or by number.",
        },
        {
            .name = SV("--all"),
            .dest = ARGDEST(&run_all),
            .help = "Run all tests, including those which are disabled by default.",
        },
        {
            .name = SV("-s"),
            .altname1 = SV("--silent"),
            .dest = ARGDEST(&silent),
            .help = "Don't print to stderr.",
        },
        #ifndef __wasm__
        {
            .name = SV("-o"),
            .altname1 = SV("--outfile"),
            .dest = ARGDEST(&outfile),
            .help = "Where to print test results outputs to. If not given, defaults to "
                    "stderr. Implies --no-colors. If you want to output to stderr and "
                    "also to file, use --tee instead.",
        },
        [TEE_INDEX] = {
            .name = SV("--tee"),
            .max_num = arrlen(extrafiles),
            .dest = ARGDEST(extrafiles),
            .help = "In addition to the primary output (either stderr or the file "
                    "given to --outfile), also print results to this file.",
        },
        {
            .name = SV("--append"),
            .dest = ARGDEST(&append),
            .help = "Open the files indicated by --outfile or --tee in append mode.",
        },
        {
            .name = SV("-p"),
            .altname1 = SV("--print-pid"),
            .help = "Print the pid of this process",
            .dest = ARGDEST(&print_pid),
        },
        {
            .name = SV("-w"),
            .altname1 = SV("--wait"),
            .help = "Do a getchar() before running the tests to give time to attach or whatever",
            .dest = ARGDEST(&should_wait),
        },
        #endif
        {
            .name = SV("-r"),
            .altname1 = SV("--repeat"),
            .help = "Run all the tests this many times in a loop.",
            .dest = ARGDEST(&nreps),
            .show_default = 1,
        },
        {
            .name = SV("--shuffle"),
            .help = "Run the tests in a random order.",
            .dest = ARGDEST(&shuffle),
        },
        {
            .name = SV("--seed"),
            .help = "Seed for the rng (only used if --shuffle is passed).\n"
                    "0 means to seed using system rng.",
            .dest = ARGDEST(&seed),
            .show_default = 1,
        },
        #if !defined(__wasm__)
            {
                .name = SV("-j"),
                .altname1 = SV("--multithreaded"),
                .dest = ARGDEST(&multithreaded),
                .help = j_help,
            },
            {
                .name = SV("--num-threads"),
                .dest = ARGDEST(&n_threads),
                .help = "Run the tests in this many threads.",
            },
        #endif
        {
            .name = SV("-d"),
            .altname1 = SV("--break-on-failure"),
            .dest = ARGDEST(&_test_do_debugbreak_on_fail),
            .help = "Do a debugbreak on failed test.",
        },
    };
    enum {HELP=0, LIST=1};
    ArgToParse early_args[] = {
        [HELP] = {
            .name = SV("-h"),
            .altname1 = SV("--help"),
            .help = "Print this help and exit.",
        },
        [LIST] = {
            .name = SV("-l"),
            .altname1 = SV("--list"),
            .help = "List the names of the test functions and exit.",
        },
    };
    ArgParser argparser = {
        .name = argc?argv[0]:"(Unnamed program)",
        .description = "A test runner.",
        .keyword={
            .args = kw_args,
            .count = arrlen(kw_args),
            .next = extra_kwargs,
        },
        .early_out={
            .args = early_args,
            .count = arrlen(early_args),
        },
        .styling={.plain = !isatty(fileno(stdout)),},
    };
    Args args = argc?(Args){argc-1, (const char*const*)argv+1}: (Args){0, 0};
    switch(check_for_early_out_args(&argparser, &args)){
        case HELP:{
            int columns = get_terminal_size().columns;
            if(columns > 80)
                columns = 80;
            print_argparse_help(&argparser, columns);
        } return 1;
        case LIST:
            for(size_t i = 0; i < test_funcs_count; i++){
                fprintf(stdout, "%s\t", test_funcs[i].test_name.text);
                if(test_funcs[i].flags & TEST_CASE_FLAGS_SKIP_UNLESS_NAMED){
                    fprintf(stdout, "Will-Skip");
                }
                fputc('\n', stdout);
            }
            return 1;
        default:
            break;
    }
    enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
    if(e){
        print_argparse_error(&argparser, e);
        fprintf(stderr, "Use --help to see usage.\n");
        return (int)e;
    }
    if(nreps < 0){
        fprintf(stderr, "Reps must be >= 0\n");
        return 1;
    }
    #if defined(__wasm__)
        n_threads = 1;
    #else
        if(n_threads < 0)
            n_threads = num_cpus();
        if(n_threads == 0){
            if(multithreaded)
                n_threads = num_cpus();
            else
                n_threads = 1;
        }
    #endif
    #ifndef __wasm__
    // Register primary output file
    if(outfile.length){
        no_colors = 1;
        FILE* fp = fopen(outfile.text, append?"ab":"wb");
        if(!fp){
            fprintf(stderr, "Unable to open '%s': %s\n", outfile.text, strerror(errno));
            return 1;
        }
        TestRegisterOutFile(fp);
    }
    #endif
    if(!TestOutFileCount && !silent){
        TestRegisterOutFile(stderr);
    }
    #ifndef __wasm__
    // Register extras
    for(int i = 0; i < kw_args[TEE_INDEX].num_parsed; i++){
        FILE* fp = fopen(extrafiles[i].text, append?"ab":"wb");
        if(!fp){
            fprintf(stderr, "Unable to open '%s': %s\n", extrafiles[i].text, strerror(errno));
            return 1;
        }
        TestRegisterOutFile(fp);
    }
    if(directory.length){
        int changed = chdir(directory.text);
        if(changed != 0){
            fprintf(stderr, "Failed to change directory to '%s': %s.\n",
                    directory.text, strerror(errno));
            return changed;
        }
    }
    #endif
    filename = strrchr(filename, '/')? strrchr(filename, '/')+1 : filename;
    #ifdef _WIN32
    filename = strrchr(filename, '\\')? strrchr(filename, '\\')+1 : filename;
    #endif
    _Bool use_colors = force_colors || (!no_colors && isatty(fileno(stderr)));
    const char* gray  = use_colors? "\033[97m"    : "";
    const char* blue  = use_colors? "\033[94m"    : "";
    const char* green = use_colors? "\033[92m"    : "";
    const char* red   = use_colors? "\033[91m"    : "";
    const char* reset = use_colors? "\033[39;49m" : "";
    const char* bold  = use_colors? "\033[1m"     : "";
    const char* nobold  = use_colors? "\033[0m"   : "";
    _test_color_gray = gray;
    _test_color_reset = reset;
    _test_color_green = green;
    _test_color_red = red;
#if 0
    _test_color_blue = blue;
#endif

    if(run_all){
        for(size_t i = 0; i < test_funcs_count; i++)
            tests_to_run[i] = i;
    }

    size_t num_to_run = run_all? test_funcs_count : (size_t)kw_args[TARGET_INDEX].num_parsed;
    if(!num_to_run){
        for(size_t i = 0; i < test_funcs_count; i++){
            if(test_funcs[i].flags & TEST_CASE_FLAGS_SKIP_UNLESS_NAMED)
                continue;
            tests_to_run[num_to_run++] = i;
        }
    }


    #ifndef __wasm__
    if(print_pid){
        #ifdef _WIN32
        fprintf(stderr, "pid: %d\n", (int)GetCurrentProcessId());
        #else
        fprintf(stderr, "pid: %d\n", getpid());
        #endif
    }
    if(should_wait){
        getchar();
    }
    #endif
    if(shuffle)
        testing_seed_rng(&seed);
    struct TestResults result = {0};
    for(int i = 0; i < nreps; i++){
        if(shuffle) shuffle_tests(tests_to_run, num_to_run);
        if(n_threads > 1)
            run_the_tests_multithreaded(tests_to_run, num_to_run, &result, n_threads);
        else
            run_the_tests(tests_to_run, num_to_run, &result);
        if(result.assert_failures || result.failures)
            break;
    }

    const char* text = test_funcs_count == 1?
        "test function executed"
        : "test functions executed";
    TestPrintf("%s%s%s: %s%zu%s / %s%zu%s %s\n",
            gray, filename, reset,
            blue, result.funcs_executed, reset,
            blue, test_funcs_count, reset,
            text);

    text = result.executed == 1? "test statement executed" : "test statements executed";
    TestPrintf("%s%s%s: %s%zu%s %s\n",
            gray, filename, reset,
            blue, result.executed, reset,
            text);

    text = result.assert_failures == 1?
        "test function aborted early"
        : "test functions aborted early";
    const char* color = result.assert_failures?red:green;
    TestPrintf("%s%s%s: %s%zu%s %s\n",
            gray, filename, reset,
            color, result.assert_failures, reset,
            text);

    color = result.failures?red:green;
    text = result.failures == 1? "test failed" : "tests failed";
    TestPrintf("%s%s%s: %s%zu%s %s\n",
            gray, filename, reset,
            color, result.failures, reset,
            text);
    if(result.skips){
        color = result.skips?green:red;
        text = result.skips == 1? "test skipped" : "tests skipped";
        TestPrintf("%s%s%s: %s%zu%s %s\n",
                gray, filename, reset,
                color, result.skips, reset,
                text);
    }
    for(size_t i = 0 ; i < TestOutFileCount; i++){
        if(TestOutFiles[i] != stderr)
            fclose(TestOutFiles[i]);
    }
    for(size_t i = 0; i < result.n_failed_tests; i++){
        StringView name = test_funcs[result.failed_tests[i]].test_name;
        TestPrintf("%s%.*s%s %sfailed%s.\n", bold, (int)name.length, name.text, nobold, red, reset);
    }
    if(result.n_failed_tests && argc){
        fprintf(stderr, "To rerun the failed test%s, run:\n", result.n_failed_tests==1?"":"s");
        if(shuffle){
            fprintf(stdout, "'%s' --shuffle --seed %llu", argv[0], (unsigned long long)seed);
        }
        else {
            fprintf(stdout, "'%s' -t", argv[0]);
            for(size_t i = 0; i < result.n_failed_tests; i++){
                fprintf(stdout, " %zu", result.failed_tests[i]);
            }
        }
        fprintf(stdout, "\n");
    }
    return result.failures + result.assert_failures == 0? 0 : 1;
}

#endif

#endif
