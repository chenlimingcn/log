#include "os.h"

#ifdef linux

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <linux/limits.h>
#include <errno.h>
#include <unistd.h>

typedef struct common_timer_t
{
    timer_t timer_handle;
    struct sigevent sev;
    common_timer_callback cbk;
    void*                  arg;
}common_timer;

const char* common_get_exe_path()
{
    static char full_path[PATH_MAX] = {0};
    int len = 0;
    int pos = 0;
	
    if (full_path[0] != 0)
    {
        return full_path;
    }

    len = readlink("/proc/self/exe", full_path, PATH_MAX);
    pos = len - 1;
    while (pos >= 0)
    {
        if (full_path[pos] == '/')
        {
            pos = (pos == 0) ? 1 : pos;
            full_path[pos] = 0;
            break;
        }
        --pos;
    }
	
	return full_path;
}

struct timeval common_gettimeofday()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);

    return tv;
}

void common_usleep(unsigned int usec)
{    
    do
    {
        usec = usleep(usec);
    }while (usec && errno == EINTR);
}

void common_sleep(unsigned int seconds)
{
    int left_sec = seconds;
    
    do
    {
        left_sec = sleep(left_sec);
    }while (left_sec && errno == EINTR);
}

static void _common_sigev_notify_function(union sigval sigv)
{
    common_timer* timer = NULL;
    
    timer = (common_timer*) sigv.sival_ptr;
    if (timer && timer->cbk)
    {
        timer->cbk(timer->arg);
    }
}
common_timer* common_timer_new(int msec, common_timer_callback cbk, void* arg)
{
    struct itimerspec its;
    common_timer* timer = (common_timer*)malloc(sizeof(common_timer));

    timer->timer_handle = 0;
    timer->cbk = cbk;
    timer->arg = arg;
    timer->sev.sigev_notify = SIGEV_THREAD;
    timer->sev.sigev_signo = SIGRTMIN;
    timer->sev.sigev_value.sival_ptr = timer;
    timer->sev.sigev_notify_function = _common_sigev_notify_function;
    if (timer_create(CLOCK_REALTIME, &timer->sev, &timer->timer_handle) == -1)
    {
        free(timer);
        return NULL;
    }

    its.it_value.tv_sec = msec / 1000;
    its.it_value.tv_nsec = (msec % 1000) * 1000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    if (timer_settime(timer->timer_handle, 0, &its, NULL) == -1)
    {
        free(timer);
        perror("timer_settime");
        return NULL;
    }
    
    return timer;
}

void common_timer_free(common_timer* timer)
{
    free(timer);
}

int common_get_error_number()
{
    return errno;
}

char* common_get_str_error(char *error_buffer, int error_buffer_size)
{
    snprintf(error_buffer, error_buffer_size, "error: %s", strerror(errno));
    error_buffer[error_buffer_size-1] = 0;
    return error_buffer;
}

int common_init_os_socket()
{
    // Do nothing
    return 0;
}

int common_close_socket(int sockfd)
{
    return close(sockfd);
}

void common_clean_os_socket()
{
    // Do nothing
}


#endif // linux