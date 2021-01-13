#include "os.h"

#ifdef _WIN32

#include <stdio.h>

#include "common/log.h"

typedef struct common_timer_t
{
    HANDLE timer_handle;

    common_timer_callback cbk;
    void*                  arg;
}common_timer;

const char* common_get_exe_path()
{
    char *full_path = NULL;
    char drive[_MAX_PATH];
	char dir[_MAX_PATH];
	static char name[_MAX_PATH] = {0};
	char ext[_MAX_PATH];
    int len = 0;
    size_t i = 0;
	
    if (name[0] != 0)
    {
        return name;
    }

    //GetModuleFileName
    _get_pgmptr(&full_path);
	
	_splitpath_s(full_path, drive, _MAX_PATH, dir, _MAX_PATH, name, _MAX_PATH, ext, _MAX_PATH);
	
    _snprintf_s(name, _MAX_PATH, _MAX_PATH, "%s%s", drive, dir);

    len = strlen(name);
    name[len-1] = 0;

    for (i = 0; i < strlen(name); ++i)
    {
        if (name[i] == '\\')
        {
            name[i] = '/';
        }
    }

	return name;
}

struct timeval common_gettimeofday()
{
    struct timeval tv;

    SYSTEMTIME systime;
    struct tm tm;
    time_t lt;
    GetLocalTime(&systime);
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = systime.wYear - 1900;
    tm.tm_mon = systime.wMonth -1;
    tm.tm_mday = systime.wDay;
    tm.tm_hour = systime.wHour;
    tm.tm_min = systime.wMinute;
    tm.tm_sec = systime.wSecond;
    lt = mktime(&tm);
    tv.tv_sec = (long)lt;
    tv.tv_usec = systime.wMilliseconds * 1000;

    return tv;
}

void common_usleep(unsigned int usec)
{    
    HANDLE timer; 
    LARGE_INTEGER ft; 
    __int32 value = 0;
    value = 0 - 10 * usec; // Convert to 100 nanosecond interval, negative value indicates relative time
    ft.QuadPart = value;

    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer);
}

void common_sleep(unsigned int seconds)
{
    Sleep(seconds * 1000);
}

static  void NTAPI _common_TimerProc(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
    common_timer* mgr = NULL;
    
    (void)TimerOrWaitFired;

    mgr = (common_timer*) lpParameter;
    mgr->cbk(mgr->arg);
}

common_timer* common_timer_new(int msec, common_timer_callback cbk, void* arg)
{
    common_timer* timer = (common_timer*)malloc(sizeof(common_timer));
    timer->cbk = cbk;
    timer->arg = arg;
    CreateTimerQueueTimer(&timer->timer_handle, NULL, _common_TimerProc, (PVOID)timer, msec, msec, WT_EXECUTEDEFAULT);
    return timer;
}

void common_timer_free(common_timer* timer)
{
    DeleteTimerQueueTimer(NULL, timer->timer_handle, 0);//INVALID_HANDLE_VALUE);
    timer->timer_handle = NULL;

    free(timer);
}

int common_get_error_number()
{
    return GetLastError();
}

char* common_get_str_error(char *error_buffer, int error_buffer_size)
{
    // just ouput the error number
    // TODO: map to string desc https://blog.csdn.net/haelang/article/details/45147121
    _snprintf(error_buffer, error_buffer_size, "error number is %d", GetLastError());
    error_buffer[error_buffer_size-1] = 0;
    return error_buffer;
}

int common_init_os_socket()
{
    static uint8_t is_init_socket = 0;
    WSADATA wsadata;
    int ret = 0;

    if (is_init_socket)
    {
        return 0;
    }

    if ( (ret =WSAStartup(MAKEWORD(2, 2), &wsadata)) != 0)
    {
        LOG_ERROR("WSAStartup error: %d", ret);
        return -1;
    }

    is_init_socket = 1;
    return 0;
}

int common_close_socket(int sockfd)
{
    return closesocket(sockfd);
}

void common_clean_os_socket()
{
    WSACleanup();
}

#endif // _WIN32
