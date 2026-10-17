struct Vector {int x, y;} v = {1, 2};
printf("%d, %d\n", v.[0], v.[1]); // 1, 2
static for(size_t i = 0; i < struct Vector.fields; i++){
    v.[i]++;
}
printf("%d, %d\n", v.[0], v.[1]); // 2, 3
