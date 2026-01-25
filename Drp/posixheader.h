//
// Copyright © 2025, David Priver <david@davidpriver.com>
//
#ifndef POSIXHEADER_H
#define POSIXHEADER_H

#if !defined(_WIN32) && !defined(__wasm__)
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <signal.h>
#ifdef B0
#undef B0
#endif
enum {IS_POSIX=1};
#else
#ifndef __wasm__
typedef long long ssize_t;
#endif
enum {IS_POSIX=0};
#endif

#endif
