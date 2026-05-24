const char * dir = __DIR__;
#if __INCLUDE_LEVEL__ == 1
printf("base\n");
#endif
#pragma message "hello"
#warning warning
__print("hi from print");
if(0) printf("random number: %d\n", (int)__RAND__);
#define FOO 1
__where(FOO)
