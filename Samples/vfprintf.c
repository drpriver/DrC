#include <stdarg.h>
#include <stdio.h>

void log_twice(const char* fmt, ...){
    va_list va, va2;
    va_start(va);
    va_copy(va2, va);
    char buff[1024];
    // don't do this at home kids
    vprintf((snprintf(buff, sizeof buff, "%s:%d: %s", __FILE__, __LINE__, fmt), buff), va);
    vprintf((snprintf(buff, sizeof buff, "%s:%d: %s", __FILE__, __LINE__, fmt), buff), va2);
    va_end(va);
    va_end(va2);
}
log_twice("Hello %s\n", "world");
log_twice("1. + 2. = %f\n", 1.+2.);
log_twice("%s %f %s\n", "hi", 2.f, "you");
log_twice("Goodbye %s\n", "world");


