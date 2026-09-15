//
// Copyright © 2025-2026, David Priver <david@davidpriver.com>
//
#ifndef POSIXHEADER_H
#define POSIXHEADER_H

#if !defined _WIN32 && !defined __wasm__
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
#ifdef __APPLE__
#include <sys/param.h>
#undef MAX
#undef MIN
#undef powerof2
#undef roundup
#undef howmany
#endif
#ifdef B0
#undef B0
#endif
enum {IS_POSIX=1};
typedef int OsFileHandle;
#else
#ifndef __wasm__
typedef long long ssize_t;
#endif
enum {IS_POSIX=0};
#endif

#ifdef __linux__
enum {IS_LINUX=1};
#else
enum {IS_LINUX=0};
#endif

#ifdef __APPLE__
enum {IS_APPLE=1};
#else
enum {IS_APPLE=0};
#endif

#endif
