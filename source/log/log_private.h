#ifndef LOG_PRIVATE_H__
#define LOG_PRIVATE_H__

#include "log/log_defs.h"

LOG_BEGIN_DECLS
unsigned int _log_get_thread_id();

char* _log_get_format_time(char * str_time, int str_length);

LOG_END_DECLS

#endif//LOG_PRIVATE_H__
