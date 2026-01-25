#ifndef DRP_LOGGER_H
#define DRP_LOGGER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include "MStringBuilder.h"
#include "msb_sprintf.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Nullable
#define _Nullable
#endif
#endif

#ifdef __GNUC__
#define LOG_PRINTF(a, b) __attribute__((format(printf, a, b)))
#else
#define LOG_PRINTF(a, b)
#endif


enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
    LOG_PRINT = 4, // Use for messages printed at any log level (except for off)
    LOG_OFF   = 5, // set logger to this level to disable all logging
};
typedef enum LogLevel LogLevel;
typedef struct Logger Logger;
struct Logger {
    LogLevel level;
    // You can write to this and either call flush yourself, or call one of the
    // subsequent logging functions to have what you wrote to this pre-pended.
    MStringBuilder buff;
    // user pointer for `sink`
    void*_Null_unspecified up;
    // sink returns void as we ignore errors in the logging itself.
    void (*_Nullable sink)(void*_Null_unspecified, const void*_Nonnull data, size_t length);
};


//
// Flushes the current messages buffered in the logger to the sink, according
// to the given log level.
// The intention of exposing this API is so you can build up a message by
// manipulating buff if your message is too complicated to write in a single
// printf style format string.
//
static void log_flush(Logger* logger, LogLevel level);
//
// Log at the given level, with the given message.
// For logging without going through printf-machinery.
//
static void log_log(Logger* logger, LogLevel level, const char* data, size_t length);

LOG_PRINTF(3, 4)
// Log a formatted string, at the given level.
static void log_logf(Logger* logger, LogLevel level, const char* fmt, ...);
// Log a formatted string, but with a va_list if you write your own wrapper
// functions.
static void log_logv(Logger* logger, LogLevel level, const char* fmt, va_list va);



//
// Log a formatted string at a predefined log level.
//
LOG_PRINTF(2, 3)
static void log_debug(Logger* logger, const char* fmt, ...);
LOG_PRINTF(2, 3)
static void log_info(Logger* logger, const char* fmt, ...);
LOG_PRINTF(2, 3)
static void log_warn(Logger* logger, const char* fmt, ...);
LOG_PRINTF(2, 3)
static void log_error(Logger* logger, const char* fmt, ...);

static
void
log_flush(Logger* logger, LogLevel level){
    if(level >= logger->level && logger->sink && logger->buff.cursor){
        if(msb_peek(&logger->buff) != '\n')
            msb_write_char(&logger->buff, '\n');
        if(!logger->buff.errored){
            StringView sv = msb_borrow_sv(&logger->buff);
            logger->sink(logger->up, sv.text, sv.length);
        }
    }
    msb_reset(&logger->buff);
}

static
void
log_log(Logger* logger, LogLevel level, const char* data, size_t length){
    if(level >= logger->level && logger->sink){
        if(logger->buff.cursor || length){
            msb_write_str(&logger->buff, data, length);
            log_flush(logger, level);
        }
    }
    msb_reset(&logger->buff);
}

static
void
log_logf(Logger* logger, LogLevel level, const char* fmt, ...){
    if(level >= logger->level && logger->sink){
        va_list va;
        va_start(va, fmt);
        log_logv(logger, level, fmt, va);
        va_end(va);
    }
    msb_reset(&logger->buff);
}
static
void
log_logv(Logger* logger, LogLevel level, const char* fmt, va_list va){
    if(level >= logger->level && logger->sink){
        msb_vsprintf(&logger->buff, fmt, va);
        log_flush(logger, level);
    }
    msb_reset(&logger->buff);
}

static
void
log_debug(Logger* logger, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    log_logv(logger, LOG_DEBUG, fmt, va);
    va_end(va);
}
static
void
log_info(Logger* logger, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    log_logv(logger, LOG_INFO, fmt, va);
    va_end(va);
}
static
void
log_warn(Logger* logger, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    log_logv(logger, LOG_WARN, fmt, va);
    va_end(va);
}
static
void
log_error(Logger* logger, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    log_logv(logger, LOG_ERROR, fmt, va);
    va_end(va);
}
static
void
log_printf(Logger* logger, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    log_logv(logger, LOG_PRINT, fmt, va);
    va_end(va);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
