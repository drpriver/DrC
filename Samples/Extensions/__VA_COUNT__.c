#include <stdio.h>
#define X(...) printf("%s:%d: __VA_COUNT__(%s) -> %d args\n", __FILE__, __LINE__, #__VA_ARGS__, __VA_COUNT__)
X(a, b, c);
X(1);
X();
