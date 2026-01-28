#ifndef DRP_LOGGER_TI_PRINTER_H
#define DRP_LOGGER_TI_PRINTER_H
#include <stdarg.h>
#include "logger.h"
#include "typeinfo_print.h"
#include "msb_sprintf.h"

static
int
ti_print_logger_(void* up, const char* fmt, ...){
    Logger* logger = up;
    MStringBuilder* sb = &logger->buff;
    va_list va;
    va_start(va, fmt);
    msb_vsprintf(sb, fmt, va);
    va_end(va);
    return 0;
}

static
void
ti_print_logger(const void* src, const TypeInfo*ti, Logger* logger){
    TiPrinter printer = {
        .printer = ti_print_logger_,
        .ctx = logger,
    };
    ti_print_any(src, ti, &printer);
    msb_write_char(&logger->buff, '\n');
    log_flush(logger, LOG_PRINT);
}

#endif
