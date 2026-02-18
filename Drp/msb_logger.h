#ifndef DRP_MSB_LOGGER_H
#define DRP_MSB_LOGGER_H
#include "logger.h"
#include "MStringBuilder.h"

typedef struct MsbLogger MsbLogger;
struct MsbLogger {
    Logger logger;
    MStringBuilder* sb;
};
static
void
msb_log(Logger* logger, LogLevel lvl, const void* data, size_t nbytes){
    (void)lvl;
    msb_write_str(((MsbLogger*)logger)->sb, data, nbytes);
}

static 
Logger*
msb_logger(MsbLogger*logger, MStringBuilder* sb){
    logger->sb = sb;
    logger->logger.buff.allocator = sb->allocator;
    logger->logger.sink = msb_log;
    return &logger->logger;
}

#endif
