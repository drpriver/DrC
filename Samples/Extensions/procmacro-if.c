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
