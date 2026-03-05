#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

// Minimal HTTP GET client using BSD sockets.
// Usage: echo "example.com" | Bin/cc httpget.c
//    or: modify the host/path below.

const char* host = "httpforever.com";
const char* path = "/";
const char* port = "80";

// resolve host
struct addrinfo hints;
memset(&hints, 0, sizeof hints);
hints.ai_family = AF_INET;
hints.ai_socktype = SOCK_STREAM;

struct addrinfo* res = (struct addrinfo*)0;
int err = getaddrinfo(host, port, &hints, &res);
if(err != 0){
    printf("getaddrinfo failed: %d\n", err);
    return 1;
}

int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
if(fd < 0){
    printf("socket failed\n");
    freeaddrinfo(res);
    return 1;
}

if(connect(fd, res->ai_addr, res->ai_addrlen) < 0){
    printf("connect failed\n");
    close(fd);
    freeaddrinfo(res);
    return 1;
}
freeaddrinfo(res);

// send HTTP request
char request[512];
int reqlen = sprintf(request,
    "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
    path, host);
send(fd, request, (unsigned long)reqlen, 0);

// read and print response
char buf[4096];
for(;;){
    long n = recv(fd, buf, sizeof(buf) - 1, 0);
    if(n <= 0) break;
    buf[n] = '\0';
    printf("%s", buf);
}

close(fd);
printf("\n");
