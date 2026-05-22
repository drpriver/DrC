#pragma once
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
SEL sel(const char* name){ return sel_registerName(name); }
id cls(const char* name){  return (id)objc_getClass(name); }
#define T(t, n) t
#define N(t, n) n
#define PARAM(a)  , T a N a
#define CAST_T(a) , T a
#define FWD(a)    , N a

#ifdef __DRC__
// drc provides better preprocessor features
#defblock MSG(ret, name, ...)
ret name(id self, const char* s __map(PARAM, __VA_ARGS__)){
    return ((ret(*)(id, SEL __map(CAST_T, __VA_ARGS__)))objc_msgSend)(self, sel(s) __map(FWD, __VA_ARGS__));
}
#endblock
#else
// You can do it without it, but it's hacky
#ifndef __map
#define CC_CAT(a, b) CC_CAT_(a, b)
#define CC_CAT_(a, b) a##b
#define CC_NARG(...) CC_NARG_(__VA_ARGS__ __VA_OPT__(,) 8,7,6,5,4,3,2,1,0)
#define CC_NARG_(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N
#define CC_MAP_0(m)
#define CC_MAP_1(m, x)      m(x)
#define CC_MAP_2(m, x, ...) m(x) CC_MAP_1(m, __VA_ARGS__)
#define CC_MAP_3(m, x, ...) m(x) CC_MAP_2(m, __VA_ARGS__)
#define CC_MAP_4(m, x, ...) m(x) CC_MAP_3(m, __VA_ARGS__)
#define CC_MAP_5(m, x, ...) m(x) CC_MAP_4(m, __VA_ARGS__)
#define CC_MAP_6(m, x, ...) m(x) CC_MAP_5(m, __VA_ARGS__)
#define CC_MAP_7(m, x, ...) m(x) CC_MAP_6(m, __VA_ARGS__)
#define CC_MAP_8(m, x, ...) m(x) CC_MAP_7(m, __VA_ARGS__)
#define __map(m, ...) CC_CAT(CC_MAP_, CC_NARG(__VA_ARGS__))(m __VA_OPT__(,) __VA_ARGS__)
#endif

#define MSG(ret, name, ...) \
    ret name(id self, const char* s __map(PARAM, __VA_ARGS__)){ \
        return ((ret(*)(id, SEL __map(CAST_T, __VA_ARGS__)))objc_msgSend)(self, sel(s) __map(FWD, __VA_ARGS__)); }
#endif

MSG(id,          msg)
MSG(id,          msg_id,     (id, a))
MSG(void,        msgv)
MSG(void,        msgv_id,    (id, a))
MSG(void,        msgv_bool,  (BOOL, a))
MSG(void,        msgv_ulong,  (unsigned long, a))
MSG(long,        msgl)
MSG(void,        msgv_long, (long, a))
MSG(id,          msg_str,    (const char*, a))
MSG(const char*, msg_cstr)
MSG(id,          msg_double, (double, a))
MSG(id,          msg_double2,(double, a), (double, b))
id nsstr(const char* s){ return msg_str(cls("NSString"), "stringWithUTF8String:", s); }
