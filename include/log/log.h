/*
** Copyright (C) 2021.
** @file log.h
** @brief Log system APIs
** @author Rimon Chen
** @date 2021-01-13
** @version 1.0
**
** History:
**   1. 2021-01-13 Create this file
**/
#ifndef LOG_LOG_H__
#define LOG_LOG_H__

#include <stdio.h>
#include <stdint.h>

#include "log/log_defs.h"

LOG_BEGIN_DECLS

/*
** log mode:
**     no log
**     log to console
**     log to file
** As you know, this three value can be used or(|) to combine the mutiple value.
**/
typedef enum
{
    LOG_MODE_NONE    = 0x00,
    LOG_MODE_CONSOLE = 0x01,
    LOG_MODE_FILE    = 0x02
}log_mode;

/*
** Log level definion.
** Raw data output is no level.
**/
typedef enum
{
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR   = 0x01,        /* output error   */
    LOG_LEVEL_WARNING,               /* output warning */
    LOG_LEVEL_INFO,                  /* output info    */
    LOG_LEVEL_DEBUG,                 /* output debug   */
}log_level;

#ifdef _WIN32 // #if defined (__MSDOS__) || defined (_MSC_VER)
#define __func__                               __FUNCTION__
#endif // _WIN32

/*
** Just print function name with "() start" as debug log message
**/
#define PRINT_FUNCTION_START                     \
{                                                \
    static char _strbuf[255];                    \
                                                 \
    sprintf(_strbuf, "%s() start",__func__);     \
    log_debug(_strbuf);                          \
}

#define PRINT_FUNCTION_END                       \
{                                                \
    static char _strbuf[255];                    \
                                                 \
    sprintf(_strbuf, "%s() end",__func__);       \
    log_debug(_strbuf);                          \
}


/*
** init the log system backend
** @param _log_mode where the log messages are sent, file, console or both of them
** @param _log_level The log critical level, error warning info debug
** @param filename If the LOG_LOG_MODE_FILE is set, you must specify where to save log file(this is a single file)
** @param path The path is used for saving logs
** @param app_name The app name
**/
LOG_API void log_init(int _log_mode, int _log_level, const char* path, const char* app_name);

/*
** output error log message from this API
** @param format just like printf
**/
LOG_API void log_error(const char* format, ...);

/*
** output warning log message from this API
** @param format just like printf
**/
LOG_API void log_warning(const char* format, ...);

/*
** output info log message from this API
** @param format just like printf
**/
LOG_API void log_info(const char* format, ...);

/*
** output debug log message from this API
** @param format just like printf
**/
LOG_API void log_debug(const char* format, ...);

/*
**  Following macros is for convenience, It will print the function name with '()' before the log message
**/
#define LOG_ERROR(fmt, ...)         log_error("%s " fmt , __func__, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)       log_warning("%s " fmt , __func__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)          log_info("%s " fmt , __func__, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)         log_debug("%s " fmt , __func__, ##__VA_ARGS__)


LOG_END_DECLS

#endif // LOG_LOG_H__