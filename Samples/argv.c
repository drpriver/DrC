int printf(const char*, ...);
#define COMMA ,
const char** args[] = {
    __let(arg(i), __argv(i) COMMA,
        __for(0, __ARGC__, arg)
    )
};
printf("sizeof args: %zu\n", sizeof args);
for(int i = 0; i < sizeof args / sizeof args[0]; i++){
    printf("%d) %s\n", i, args[i]);
}
