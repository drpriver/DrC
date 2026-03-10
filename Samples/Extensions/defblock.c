#include <stdio.h>
#include <stdlib.h>
#defblock ASSERT(x)
// Look ma, C++ style comments within a macro!
do {
    if(!x){
        fprintf(stderr, "%s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #x);
        abort();
    }
}while(0)
#endblock
ASSERT(1 == 1);
ASSERT(1 == 0);
