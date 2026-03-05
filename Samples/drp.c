#include "../Drp/cmd_builder.h"
#include "../Drp/cmd_run.h"
#include "../Drp/Allocators/mallocator.h"

int main(){
CmdBuilder cmd = {
    .prog.allocator = MALLOCATOR,
    .allocator = MALLOCATOR,
};
cmd_prog(&cmd, LS("ls"));
char* envp[] = {"PATH=/bin", NULL};
int err = cmd_run(&cmd, envp, NULL);
return err;
}

#include "../Drp/Allocators/allocator.c"
