int printf(const char*, ...);
int main(int argc, char** argv){
    printf("Hello from main!\n");
    for(int i = 0; i < argc; i++){
        printf("%d) %s\n", i, argv[i]);
    }
    return 0;
}
