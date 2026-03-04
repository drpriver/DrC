#include <stdio.h>
#include <sys/types.h>
#include <sys/event.h>
#include <fcntl.h>
#include <unistd.h>

// Watch a directory for file changes using kqueue (macOS).
// This is the kind of thing that normally requires fswatch/inotifywait
// or a compiled program.

const char* path = "/tmp";

int fd = open(path, O_RDONLY);
if(fd < 0){
    printf("error: cannot open '%s'\n", path);
    return 1;
}

int kq = kqueue();
if(kq < 0){
    printf("error: kqueue failed\n");
    close(fd);
    return 1;
}

// register interest in vnode events on the fd
struct kevent change = {
    .ident  = (unsigned long)fd,
    .filter = EVFILT_VNODE,
    .flags  = EV_ADD | EV_CLEAR,
    .fflags = NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK | NOTE_RENAME | NOTE_REVOKE,
    .data   = 0,
    .udata  = nullptr,
};

printf("watching '%s' for changes (ctrl-c to stop)...\n", path);

for(;;){
    struct kevent event;
    int n = kevent(kq, &change, 1, &event, 1, (const struct timespec*)0);
    if(n < 0){
        printf("error: kevent failed\n");
        break;
    }
    if(n == 0) continue;

    unsigned int ff = event.fflags;
    printf("event:");
    if(ff & NOTE_WRITE)  printf(" WRITE");
    if(ff & NOTE_EXTEND) printf(" EXTEND");
    if(ff & NOTE_DELETE) printf(" DELETE");
    if(ff & NOTE_RENAME) printf(" RENAME");
    if(ff & NOTE_ATTRIB) printf(" ATTRIB");
    if(ff & NOTE_LINK)   printf(" LINK");
    if(ff & NOTE_REVOKE) printf(" REVOKE");
    printf("\n");
}

close(kq);
close(fd);
