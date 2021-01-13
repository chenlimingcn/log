#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

//#include "common/os.h"

#ifdef _WIN32
# include <windows.h>
#else
# include <sys/time.h>
#endif // _WIN32

unsigned int _log_get_thread_id()
{
#ifdef PTW32_VERSION
	return pthread_getw32threadid_np(pthread_self());
#else
	return (unsigned int)pthread_self();
#endif
}

char* _log_get_format_time(char * str_time, int str_length)
{
    const char* fmt = "%04d%02d%02d%02d%02d%02d.%03ld";
    //int64_t sec = 0;
    //int64_t msec = 0; 

#ifdef _WIN32
    SYSTEMTIME systime;
    //struct tm tm;
    //time_t lt;
    GetLocalTime(&systime);
    
    //memset(&tm, 0, sizeof(tm));
    //tm.tm_year = systime.wYear - 1900;
    //tm.tm_mon = systime.wMonth -1;
    //tm.tm_mday = systime.wDay;
    //tm.tm_hour = systime.wHour;
    //tm.tm_min = systime.wMinute;
    //tm.tm_sec = systime.wSecond;
    //lt = mktime(&tm);
    //sec = lt;
    //msec = systime.wMilliseconds;

    _snprintf(str_time, str_length, fmt, 
        systime.wYear,
        systime.wMonth,
        systime.wDay,
        systime.wHour,
        systime.wMinute,
        systime.wSecond,
        systime.wMilliseconds);
#else
    struct timeval tv;
    time_t t;
    struct tm *tnow;
    gettimeofday(&tv,NULL);
    //sec = tv.tv_sec;
    //msec = tv.tv_usec / 1000;
    time(&t);
    tnow = localtime(&t);

    snprintf(str_time, str_length, fmt, 
        tnow->tm_year + 1900,
        tnow->tm_mon + 1,
        tnow->tm_mday,
        tnow->tm_hour,
        tnow->tm_min,
        tnow->tm_sec,
        tv.tv_usec / 1000);
#endif // _WIN32

    return str_time;
}

