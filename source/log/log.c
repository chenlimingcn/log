#include "log/log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

#include "os.h"
#include "log_private.h"

#ifdef _WIN32
# include <windows.h>
#else
# include <sys/time.h>
#endif // _WIN32

#ifndef FILENAME_MAX
# define FILENAME_MAX   260
#endif // FILENAME_MAX


#define LOG_MESSAGE_MAX_SIZE       2048
#define LOG_BUFFER_SIZE            1048576   // 3M debug2048
#define LOG_FLUSH_INTERVAL_MSEC    5000            // 5s
#define LOG_FILE_MAX_SIZE          104857600 // 100M debug10240//

typedef struct
{
    log_mode lm;
    log_level ll;
    
    char *log_buffer;
    int log_buffer_length;
    pthread_mutex_t log_buffer_mutex;
    pthread_cond_t  log_buffer_cond;
    common_timer*   log_timer;
    
    char log_filename[FILENAME_MAX];
    char log_bak_filename[FILENAME_MAX];
    FILE* log_file;
    long  log_file_length;
    pthread_t log_file_tid;
}log_context;

static log_context* _log_ctx()
{
    static log_context *ctx = NULL;
    
    if (ctx == NULL)
    {
        ctx = (log_context*)malloc(sizeof(log_context));

        pthread_mutex_init(&ctx->log_buffer_mutex, NULL);
        pthread_cond_init(&ctx->log_buffer_cond, NULL);
        ctx->lm = LOG_MODE_FILE;
        ctx->ll = LOG_LEVEL_WARNING;
        ctx->log_buffer = (char*)malloc(LOG_BUFFER_SIZE + 1);
        ctx->log_buffer[LOG_BUFFER_SIZE] = 0;
        ctx->log_buffer_length = 0;
        ctx->log_filename[0] = 0;
        ctx->log_bak_filename[0] = 0;
        ctx->log_file = NULL;
        ctx->log_file_length = 0;
    }

    return ctx;
}

static void _log_out_prepare(long length)
{
    if (length + _log_ctx()->log_file_length >= LOG_FILE_MAX_SIZE)
    {
        fclose(_log_ctx()->log_file);
        (void)remove(_log_ctx()->log_bak_filename);
        (void)rename(_log_ctx()->log_filename, _log_ctx()->log_bak_filename);
        _log_ctx()->log_file = fopen(_log_ctx()->log_filename, "a+");
        _log_ctx()->log_file_length = 0;
    }

}


static void _log_write_buffer(const char *strmsg)
{
    int length = 0;
    int ret = 0;

    length = strlen(strmsg);
    if (length <= 0)
    {
        return ;
    }
    (void)ret;
    //_log_out_prepare(_log_ctx()->log_file_length);
    //ret = fprintf(_log_ctx()->log_file, strmsg);
    //if (ret > 0)
    //{
    //    _log_ctx()->log_file_length += ret;
    //}
    //return ;
        


    while (_log_ctx()->log_buffer_length && length + _log_ctx()->log_buffer_length >= LOG_BUFFER_SIZE) // TODO: send buffer full sinal to save file thread and wait for complete signal
    {
        pthread_mutex_lock(&_log_ctx()->log_buffer_mutex);
        pthread_cond_signal(&_log_ctx()->log_buffer_cond);
        pthread_mutex_unlock(&_log_ctx()->log_buffer_mutex);
        common_usleep(1);
    }


    pthread_mutex_lock(&_log_ctx()->log_buffer_mutex);

    memcpy(_log_ctx()->log_buffer + _log_ctx()->log_buffer_length, strmsg, length);
    _log_ctx()->log_buffer_length += length;

    pthread_mutex_unlock(&_log_ctx()->log_buffer_mutex);
}

static void* _log_timer_func(void* arg)
{
    pthread_mutex_lock(&_log_ctx()->log_buffer_mutex);
    pthread_cond_signal(&_log_ctx()->log_buffer_cond);
    pthread_mutex_unlock(&_log_ctx()->log_buffer_mutex);
    return arg;
}

static void* _log_flush_thread(void* arg)
{
    char *buffer = NULL;
    int length = 0;
    int ret = 0;
    // TODO: start a timer in this thread. when time is reaching, send timeout signal in timer function
    _log_ctx()->log_timer = common_timer_new(LOG_FLUSH_INTERVAL_MSEC, _log_timer_func, NULL);

    buffer = (char*)malloc(LOG_BUFFER_SIZE);

    while (1)
    {
        // TODO: wait for timeout/buffer full sinal
        pthread_mutex_lock(&_log_ctx()->log_buffer_mutex);
        pthread_cond_wait(&_log_ctx()->log_buffer_cond, &_log_ctx()->log_buffer_mutex);

        if (_log_ctx()->log_buffer_length > 0)
        {
            strncpy(buffer, _log_ctx()->log_buffer, _log_ctx()->log_buffer_length);
            buffer[_log_ctx()->log_buffer_length] = 0;
            length = _log_ctx()->log_buffer_length;
            _log_ctx()->log_buffer_length = 0;
        }

        pthread_mutex_unlock(&_log_ctx()->log_buffer_mutex);

        // If the size of the file is greater than LOG_FILE_MAX_SIZE
        _log_out_prepare(strlen(buffer));
        if (length)
        {
            ret = fprintf(_log_ctx()->log_file, "%s", buffer);
            if (ret > 0)
            {
                _log_ctx()->log_file_length += ret;
            }
            fflush(_log_ctx()->log_file);
        }
    }

    return arg;
}

static void _log_out(int prio, const char* strmsg)
{
    int ret = 0;
    char output[LOG_MESSAGE_MAX_SIZE+1] = {0};
    const char* fmt = "[0x%08X][%s] %s - %s\n";
    char str_time[50] = {0};
    static const char level_name[LOG_LEVEL_DEBUG+2][10] = {"", "error", "warn ", "info ", "debug"};
    FILE* out_console[LOG_LEVEL_DEBUG+2] = {stdout, stderr, stdout, stdout, stdout};

    _log_get_format_time(str_time, 50);


#ifdef _WIN32
    ret = _snprintf(output, LOG_MESSAGE_MAX_SIZE + 1, fmt, _log_get_thread_id(), str_time, level_name[prio], strmsg);
#else
    ret = snprintf(output, LOG_MESSAGE_MAX_SIZE + 1, fmt, _log_get_thread_id(), str_time, level_name[prio], strmsg);
#endif // _WIN32
    if (ret < 0 || ret > LOG_MESSAGE_MAX_SIZE)
    {
        output[LOG_MESSAGE_MAX_SIZE - 4] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 3] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 2] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 1] = '\n';
        output[LOG_MESSAGE_MAX_SIZE + 0] = '\0';
    }

    if (_log_ctx()->lm & LOG_MODE_CONSOLE)
    {
        fprintf(out_console[prio], "%s", output);
    }
    if ( (_log_ctx()->lm & LOG_MODE_FILE) 
            && _log_ctx()->log_file != NULL)
    {
        _log_write_buffer(output);
    }
}

void log_error(const char* format, ...)
{
    int ret = 0;
    char output[LOG_MESSAGE_MAX_SIZE+1] = {0};
    va_list args;
    va_start(args, format);

    if (LOG_LEVEL_ERROR > (int)_log_ctx()->ll)
    {
        return ;
    }

#ifdef _WIN32
    ret = _vsnprintf(output, LOG_MESSAGE_MAX_SIZE+1, format, args);
#else
    ret = vsnprintf(output, LOG_MESSAGE_MAX_SIZE+1, format, args);
#endif // _WIN32
    if (ret < 0 || ret > LOG_MESSAGE_MAX_SIZE)
    {
        output[LOG_MESSAGE_MAX_SIZE - 4] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 3] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 2] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 1] = '\n';
        output[LOG_MESSAGE_MAX_SIZE + 0] = '\0';
    }
    _log_out(LOG_LEVEL_ERROR, output);
    va_end(args);
}

void log_warning(const char* format, ...)
{
    int ret = 0;
    char output[LOG_MESSAGE_MAX_SIZE+1] = {0};
    va_list args;

    if (LOG_LEVEL_WARNING > (int)_log_ctx()->ll)
    {
        return ;
    }


    va_start(args, format);
#ifdef _WIN32
    ret = _vsnprintf(output, LOG_MESSAGE_MAX_SIZE, format, args);
#else
    ret = vsnprintf(output, LOG_MESSAGE_MAX_SIZE, format, args);
#endif // _WIN32
    if (ret < 0 || ret > LOG_MESSAGE_MAX_SIZE)
    {
        output[LOG_MESSAGE_MAX_SIZE - 4] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 3] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 2] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 1] = '\n';
        output[LOG_MESSAGE_MAX_SIZE + 0] = '\0';
    }
    _log_out(LOG_LEVEL_WARNING, output);
    va_end(args);
}

void log_info(const char* format, ...)
{
    int ret = 0;
    char output[LOG_MESSAGE_MAX_SIZE+1] = {0};
    va_list args;

    if (LOG_LEVEL_INFO > (int)_log_ctx()->ll)
    {
        return ;
    }

    va_start(args, format);
#ifdef _WIN32
    ret = _vsnprintf(output, LOG_MESSAGE_MAX_SIZE-1, format, args);
#else
    ret = vsnprintf(output, LOG_MESSAGE_MAX_SIZE-1, format, args);
#endif // _WIN32
    if (ret < 0 || ret > LOG_MESSAGE_MAX_SIZE)
    {
        output[LOG_MESSAGE_MAX_SIZE - 4] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 3] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 2] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 1] = '\n';
        output[LOG_MESSAGE_MAX_SIZE + 0] = '\0';
    }
    _log_out(LOG_LEVEL_INFO, output);
    va_end(args);
}

void log_debug(const char* format, ...)
{
    int ret = 0;
    char output[LOG_MESSAGE_MAX_SIZE+1] = {0};
    va_list args;

    if (LOG_LEVEL_DEBUG > (int)_log_ctx()->ll)
    {
        return ;
    }

    va_start(args, format);
#ifdef _WIN32
    ret = _vsnprintf(output, LOG_MESSAGE_MAX_SIZE-1, format, args);
#else
    ret = vsnprintf(output, LOG_MESSAGE_MAX_SIZE-1, format, args);
#endif // _WIN32
    if (ret < 0 || ret > LOG_MESSAGE_MAX_SIZE)
    {
        output[LOG_MESSAGE_MAX_SIZE - 4] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 3] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 2] = '.';
        output[LOG_MESSAGE_MAX_SIZE - 1] = '\n';
        output[LOG_MESSAGE_MAX_SIZE + 0] = '\0';
    }
    _log_out(LOG_LEVEL_DEBUG, output);
    va_end(args);
}


void log_init(int _log_mode, int _log_level, const char* filename)
{
    _log_ctx()->lm = (log_mode)_log_mode;
    _log_ctx()->ll = (log_level)_log_level;

     if (_log_ctx()->lm & LOG_MODE_FILE)
    {
        if (filename == NULL)
        {
            fprintf(stderr, "filename is not specified while log mode is file mode");
            return ;
        }

        strncpy(_log_ctx()->log_bak_filename, filename, FILENAME_MAX);
        strcat(_log_ctx()->log_bak_filename, ".bak");
        _log_ctx()->log_bak_filename[FILENAME_MAX-1] = 0;

        strncpy(_log_ctx()->log_filename, filename, FILENAME_MAX);
        _log_ctx()->log_filename[FILENAME_MAX-1] = 0;
        _log_ctx()->log_file = fopen(_log_ctx()->log_filename, "a+");
        fseek(_log_ctx()->log_file,0,SEEK_END);
        _log_ctx()->log_file_length = ftell(_log_ctx()->log_file);
        pthread_create(&_log_ctx()->log_file_tid, NULL, _log_flush_thread, NULL);
    }
    

}
