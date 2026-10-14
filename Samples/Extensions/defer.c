defer printf("goodbye!\n");
printf("hello!\n");
if(0) return 0;
goto finish;
{
    defer printf("skipped\n");
}
finish:
printf("finishing\n");
