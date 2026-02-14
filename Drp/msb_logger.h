#ifndef DRP_MSB_LOGGER_H
#define DRP_MSB_LOGGER_H
#include "logger.h"
#include "MStringBuilder.h"

static 
Logger 
msb_logger(MStringBuilder* sb){
    Logger l = {
        .buff.allocator = sb->allocator,
        .up = sb,
        .sink = (void(*)(void*, const void*, size_t))msb_write_str,
    };
    return l;
}

#endif
