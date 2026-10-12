#pragma typedef on
enum Fruit { APPLES, ORANGES, PEARS };
const char* fruit_name(Fruit f){
    switch(f){
        static for(size_t i = 0; i < Fruit.enumerators; i++){
            {
                constexpr e = Fruit.enumerator(i);
                constexpr name = e.name.data;
                case e.value: return name;
            }
        }
        default: return "Unknown fruit";
    }
}
printf("%s\n", fruit_name(APPLES));
printf("%s\n", fruit_name((enum Fruit)4));

printf("----\n");
static for(Fruit i = 0; ; i++){
    static if(i & 1) {static continue;}
    printf("%s (%d)\n", fruit_name(i), (int)i);
    static if(i == PEARS) {static break;}
}
