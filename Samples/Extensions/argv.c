#include <stdio.h>
printf("argc: %d\n", __argc);
for(int i = 0; i < __argc; i++){
    printf("%d) %s\n", i, __argv[i]);
}
