/*
** Copyright (C) 2020.
** @file os.h
** @brief Crossed system APIs
** @author Rimon Chen
** @date 2021-01-13
** @version 1.0
**
** History:
**   1. 2021-01-13 Create this file
**/
#ifndef LOG_OS_H__
#define LOG_OS_H__

#include "log/log_defs.h"

#ifdef _WIN32
#    if !defined (PACKED)
#        define PACKED
#    endif // !defined (PACKED)

#else
#    if !defined (PACKED)
#        define PACKED __attribute__((aligned(1), packed))
#    endif // !defined (PACKED)

#endif // _WIN32

#ifdef _WIN32
# define E_SOCKET_INTR                WSAEINTR
# define E_SOCKET_INPROGRESS          WSAEINPROGRESS
# define E_SOCKET_WOULDBLOCK          WSAEWOULDBLOCK
# define E_SOCKET_AGAIN               WSATRY_AGAIN
#else
# define E_SOCKET_INTR                EINTR
# define E_SOCKET_INPROGRESS          EINPROGRESS
# define E_SOCKET_WOULDBLOCK          EWOULDBLOCK
# define E_SOCKET_AGAIN               EAGAIN
#endif // _WIN32

#ifdef _WIN32
typedef int      socklen_t;
#endif // _WIN32

LOG_BEGIN_DECLS

#ifdef _WIN32
# include <time.h>
#include <WinSock2.h>
#else
# include <sys/time.h>
# include <time.h>
#endif // _WIN32

typedef struct common_timer_t common_timer;
typedef void* (*common_timer_callback)(void*);

LOG_API const char* common_get_exe_path();

LOG_API struct timeval common_gettimeofday();

LOG_API void common_usleep(unsigned int usec);

LOG_API void common_sleep(unsigned int seconds);

LOG_API common_timer* common_timer_new(int msec, common_timer_callback cbk, void* arg);

LOG_API void common_timer_free(common_timer* timer);

LOG_API int common_get_error_number();

LOG_API char* common_get_str_error(char *error_buffer, int error_buffer_size);

LOG_API int common_init_os_socket();
LOG_API int common_close_socket(int sockfd);
LOG_API void common_clean_os_socket();

LOG_END_DECLS

#endif // LOG_OS_H__
