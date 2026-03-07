#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

// Directory listing (like ls -l)
// Usage: Bin/cc Samples/ls.c [path]

const char* dir = __ARGV__(1, ".");

DIR* d = opendir(dir);
if(!d){
    fprintf(stderr, "ls: cannot open '%s'\n", dir);
    return 1;
}

void print_mode(mode_t m){
    putchar(S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : '-');
    putchar(m & S_IRUSR ? 'r' : '-');
    putchar(m & S_IWUSR ? 'w' : '-');
    putchar(m & S_IXUSR ? 'x' : '-');
    putchar(m & S_IRGRP ? 'r' : '-');
    putchar(m & S_IWGRP ? 'w' : '-');
    putchar(m & S_IXGRP ? 'x' : '-');
    putchar(m & S_IROTH ? 'r' : '-');
    putchar(m & S_IWOTH ? 'w' : '-');
    putchar(m & S_IXOTH ? 'x' : '-');
}

void print_size(long long sz){
    if(sz < 1024)
        printf("%6lld", sz);
    else if(sz < 1024 * 1024)
        printf("%5lldK", sz / 1024);
    else if(sz < 1024 * 1024 * 1024)
        printf("%5lldM", sz / (1024 * 1024));
    else
        printf("%5lldG", sz / (1024 * 1024 * 1024));
}

struct dirent* ent;
while((ent = readdir(d))){
    if(ent->d_name[0] == '.') continue;
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
    struct stat st;
    if(stat(path, &st)) continue;

    print_mode(st.st_mode);

    struct passwd* pw = getpwuid(st.st_uid);
    struct group* gr = getgrgid(st.st_gid);
    printf(" %-8s %-8s", pw ? pw->pw_name : "?", gr ? gr->gr_name : "?");

    printf(" ");
    print_size((long long)st.st_size);

    struct tm* tm = localtime(&st.st_mtime);
    char timebuf[32];
    strftime(timebuf, sizeof timebuf, "%b %e %H:%M", tm);
    printf("  %s", timebuf);

    printf("  %s", ent->d_name);
    if(S_ISDIR(st.st_mode)) putchar('/');
    putchar('\n');
}
closedir(d);
