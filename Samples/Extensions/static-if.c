static if(__shell("date")[0] == 'W'){
  enum {IT_IS_WEDNESDAY=1};
}
printf("IT_IS_WEDNESDAY: %d\n", IT_IS_WEDNESDAY); // only compiles on wednesday
