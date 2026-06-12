#include <stdio.h>
printf("argc: %d\n", _Argc);
for(int i = 0; i < _Argc; i++){
    printf("%d) %s\n", i, _Argv[i]);
}
