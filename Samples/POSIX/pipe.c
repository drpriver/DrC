#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

// Run a command via pipe+fork+exec, capture output, print stats.
// Usage: Bin/cc Samples/pipe.c "ls -la"

const char* cmd = __ARGV__(1, "ls");
char* buf = strdup(cmd);
enum {MAX_ARGS=64};
char* args[MAX_ARGS];
int nargs = 0;
char* tok = strtok(buf, " \t");
while(tok && nargs < MAX_ARGS-1){
    args[nargs++] = tok;
    tok = strtok(NULL, " \t");
}
args[nargs] = NULL;
if(!nargs){
    fprintf(stderr, "empty command\n");
    return 1;
}
// Create a pipe: child writes to fds[1], parent reads from fds[0]
int fds[2];
if(pipe(fds)){
    perror("pipe");
    return 1;
}

pid_t pid = fork();
if(pid < 0){
    perror("fork");
    return 1;
}

if(pid == 0){
    // Child: redirect stdout to pipe, then exec
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    execvp(args[0], args);
    perror("exec");
    _exit(127);
}

// Parent: read all of child's output into a buffer
close(fds[1]);
size_t cap = 4096;
size_t len = 0;
char* output = malloc(cap);
long n;
while((n = read(fds[0], output + len, cap - len)) > 0){
    len += (size_t)n;
    if(len == cap){
        cap *= 2;
        output = realloc(output, cap);
    }
}
close(fds[0]);

int status = 0;
waitpid(pid, &status, 0);
int exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
// Now do something with the captured output: count lines, words, bytes
int lines = 0;
int words = 0;
int in_word = 0;
for(size_t i = 0; i < len; i++){
    if(output[i] == '\n') lines++;
    if(output[i] == ' ' || output[i] == '\t' || output[i] == '\n'){
        in_word = 0;
    }
    else if(!in_word){
        in_word = 1;
        words++;
    }
}

printf("--- output of '%s' (pid %d, exit %d) ---\n", cmd, (int)pid, exitcode);
fwrite(output, 1, len, stdout);
if(len && output[len - 1] != '\n') putchar('\n');
printf("--- %d lines, %d words, %zu bytes ---\n", lines, words, len);
free(output);
free(buf);
