#include "log/log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

#include "os.h"
#include "log_private.h"
#include "macros.h"
#include "buffer.h"

#ifdef _WIN32
# include <windows.h>
#else
# include <sys/time.h>
#endif // _WIN32

typedef struct log_file_node
{
	char filename[FILENAME_MAX];
	struct log_file_node* next;
}log_file_node_t;

typedef struct log_context
{
    log_mode lm;
    log_level ll;
    
    log_file_buffer_t* log_buffer;
    log_file_buffer_t* log_curr_buffer;

    pthread_mutex_t log_buffer_mutex;
    pthread_cond_t  log_buffer_cond;
    common_timer*   log_timer;
    
    char app_name[FILENAME_MAX];
    char log_path[FILENAME_MAX];
    log_file_node_t* log_file_list;
    log_file_node_t* curr_file;
    int log_file_count;
	
    FILE* log_file;
    long  log_file_length;
    pthread_t log_file_tid;
}log_context_t;

int _modify_mkdir(const char* path)
{
	size_t len = strlen(path);
	int ret;
	char pathname[255];
	memset(pathname, 0, sizeof(pathname));

	while (path[len] != '/')
	{
		len--;
	}

	strncpy(pathname, path, len);

	if (access(pathname, F_OK) == 0)
	{
		if ((ret = mkdir(path, 0777)) != 0)
		{
			return -1;
		}
	}
	else
	{
		_modify_mkdir(pathname);
		if ((ret = mkdir(path, 0777)) != 0)
		{
			return -1;
		}

	}
	return 0;
}

void _log_generate_filename(log_context_t* ctx, char* filename, int length)
{
    time_t t;
    time(&t);
    char tmp[64] = { 0 };
    strftime(tmp, sizeof(tmp), "%Y%m%d%H%M%S", localtime(&t));
    snprintf(filename, FILENAME_MAX, "%s/%s_%s.log", ctx->log_path, ctx->app_name, tmp);
}

static int _log_load(log_context_t* ctx)
{
	if (access(ctx->log_path, 0) == -1)
	{
		if (!_modify_mkdir(ctx->log_path))
		{
			return -1;
		}
	}
	DIR* dir;
	struct dirent* ptr;
	if ((dir = opendir(ctx->log_path)) == NULL)
	{
		return -1;
	}

	while ((ptr = readdir(dir)) != NULL)
	{
		if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
			continue;
		else if (ptr->d_type == DT_REG)
		{
			const char* filename = ptr->d_name;
            char *temp = strstr(filename, ctx->app_name);
            char *subfixlog = strstr(filename, ".log");
            if (temp == NULL || subfixlog == NULL)
            {
                continue;
            }
			else
			{
                ctx->curr_file = (log_file_node_t*)malloc(sizeof(log_file_node_t));
                ctx->curr_file->next = NULL;
				strcat(ctx->curr_file->filename, ctx->log_path);
                strcat(ctx->curr_file->filename, "/");
                strcat(ctx->curr_file->filename, filename);
                if (ctx->log_file_list == NULL)
                {
                    ctx->log_file_list = ctx->curr_file;
                }
                else
                {
                    log_file_node_t* tmp = ctx->log_file_list;
                    while (tmp->next)
                    {
                        tmp = tmp->next;
                    }
				    tmp->next = ctx->curr_file;
                }
                ++ctx->log_file_count;
			}
		}
		else if (ptr->d_type == DT_DIR)
		{
			continue;
		}
	}
	closedir(dir);

	if (ctx->curr_file != NULL)
	{
        struct stat st;
        if (stat(ctx->curr_file->filename, &st) == -1)
        {
            return -1;
        }
        ctx->log_file_length = st.st_size;
		ctx->log_file = fopen(ctx->curr_file->filename, "a+");
        if (ctx->log_file == NULL)
        {
            return -1;
        }
	}
    else
    {
        ctx->curr_file = (log_file_node_t*)malloc(sizeof(log_file_node_t));
        ctx->curr_file->next = NULL;
        _log_generate_filename(ctx, ctx->curr_file->filename, FILENAME_MAX);
        ctx->log_file_length = 0;
		ctx->log_file = fopen(ctx->curr_file->filename, "a+");
        if (ctx->log_file == NULL)
        {
            free(ctx->curr_file);
            ctx->curr_file = NULL;
            return -1;
        }
    }

	return 0;
}

static log_context_t* _log_ctx()
{
    static log_context_t *ctx = NULL;
    
    if (ctx == NULL)
    {
        ctx = (log_context_t*)malloc(sizeof(log_context_t));
        memset(ctx, 0, sizeof(log_context_t));
        
        pthread_mutex_init(&ctx->log_buffer_mutex, NULL);
        pthread_cond_init(&ctx->log_buffer_cond, NULL);
        ctx->lm = LOG_MODE_FILE;
        ctx->ll = LOG_LEVEL_WARNING;
        log_file_buffer_t* log_buffer = create_buffer();

        if (ctx->log_buffer == NULL)
        {
            ctx->log_buffer = log_buffer;
        }
        else
        {
            ctx->log_curr_buffer->next = log_buffer;
        }
        ctx->log_curr_buffer = log_buffer;

        ctx->log_file_list = NULL;
        ctx->curr_file = NULL;
        ctx->log_file_count = 0;
        ctx->log_file = NULL;
        ctx->log_file_length = 0;
    }

    return ctx;
}

static void _log_out_prepare(log_context_t* ctx, long length)
{
    if (length + ctx->log_file_length >= LOG_FILE_MAX_SIZE)
    {
        fclose(ctx->log_file);
        while (ctx->log_file_count > 100)
        {
            log_file_node_t *file = ctx->log_file_list;
            (void)remove(file->filename);
            ctx->log_file_list = file->next;
            free(file);
        }
        log_file_node_t *file = (log_file_node_t*)malloc(sizeof(log_file_node_t));
        file->next = NULL;

        _log_generate_filename(ctx, file->filename, FILENAME_MAX);
        ctx->log_file = fopen(file->filename, "a+");
        ctx->log_file_length = 0;
        if (ctx->log_file == NULL)
        {
            free(file);
        }
        else
        {
            if (ctx->curr_file)
            {
                ctx->curr_file->next = file;
            }
            ctx->curr_file = file;
            ++ctx->log_file_count;
        }
    }

}


static void _log_write_buffer(log_context_t* ctx, const char *strmsg)
{
    int length = 0;
    int ret = 0;

    length = strlen(strmsg);
    if (length <= 0)
    {
        return ;
    }
    (void)ret;
    //_log_out_prepare(ctx->log_file_length);
    //ret = fprintf(ctx->log_file, strmsg);
    //if (ret > 0)
    //{
    //    ctx->log_file_length += ret;
    //}
    //return ;
        
    pthread_mutex_lock(&ctx->log_buffer_mutex);
    if (ctx->log_curr_buffer->length && length + ctx->log_curr_buffer->length >= LOG_BUFFER_SIZE) // TODO: send buffer full sinal to save file thread and wait for complete signal
    {
        //pthread_cond_signal(&ctx->log_buffer_cond);

        log_file_buffer_t* log_buffer = create_buffer();

        if (ctx->log_buffer == NULL)
        {
            ctx->log_buffer = log_buffer;
        }
        else
        {
            ctx->log_curr_buffer->next = log_buffer;
        }
        ctx->log_curr_buffer = log_buffer;

        // common_usleep(1);
    }

    memcpy(ctx->log_curr_buffer->buffer + ctx->log_curr_buffer->length, strmsg, length);
    ctx->log_curr_buffer->length += length;

    pthread_mutex_unlock(&ctx->log_buffer_mutex);
}

static void* _log_timer_func(log_context_t* ctx, void* arg)
{
    pthread_mutex_lock(&ctx->log_buffer_mutex);
    pthread_cond_signal(&ctx->log_buffer_cond);
    pthread_mutex_unlock(&ctx->log_buffer_mutex);
    return arg;
}

static void* _log_flush_thread(void* arg)
{
    log_context_t* ctx = (log_context_t*)arg;
    int ret = 0;
    // TODO: start a timer in this thread. when time is reaching, send timeout signal in timer function
    //ctx->log_timer = common_timer_new(LOG_FLUSH_INTERVAL_MSEC, _log_timer_func, NULL);

    while (1)
    {
        log_file_buffer_t* tmp = create_buffer();
        int isFreeTmp = 0;

        // TODO: wait for timeout/buffer full sinal
        pthread_mutex_lock(&ctx->log_buffer_mutex);
        // pthread_cond_wait(&ctx->log_buffer_cond, &ctx->log_buffer_mutex);
        if (ctx->log_buffer ==  NULL && ctx->log_buffer <= 0)
        {
            pthread_mutex_unlock(&ctx->log_buffer_mutex);
            usleep(500000);
            continue;
        }

        log_file_buffer_t *buffer = ctx->log_buffer;
        if (ctx->log_curr_buffer == buffer)
        {
            ctx->log_buffer = tmp;
            ctx->log_curr_buffer = tmp;
        }
        else
        {
            ctx->log_buffer = ctx->log_buffer->next;
            isFreeTmp = 1;
        }

        pthread_mutex_unlock(&ctx->log_buffer_mutex);

        if (isFreeTmp)
        {
            free_buffer(tmp);
        }
        // If the size of the file is greater than LOG_FILE_MAX_SIZE
        _log_out_prepare(ctx, strlen(buffer->buffer));
        if (buffer->length > 0 && ctx->log_file)
        {
            ret = fprintf(ctx->log_file, "%s", buffer->buffer);
            if (ret > 0)
            {
                ctx->log_file_length += ret;
            }

            free(buffer->buffer);
            free(buffer);
        }
    }

    return arg;
}

static void _log_out(log_context_t* ctx, int prio, const char* strmsg)
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

    if (ctx->lm & LOG_MODE_CONSOLE)
    {
        fprintf(out_console[prio], "%s", output);
    }
    if ( (ctx->lm & LOG_MODE_FILE) 
            && ctx->log_file != NULL)
    {
        _log_write_buffer(ctx, output);
    }
}

void log_error(const char* format, ...)
{
    log_context_t* ctx = _log_ctx();
    int ret = 0;
    char output[LOG_MESSAGE_MAX_SIZE+1] = {0};
    va_list args;
    va_start(args, format);

    if (LOG_LEVEL_ERROR > (int)ctx->ll)
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
    _log_out(ctx, LOG_LEVEL_ERROR, output);
    va_end(args);
}

void log_warning(const char* format, ...)
{
    log_context_t* ctx = _log_ctx();
    int ret = 0;
    char output[LOG_MESSAGE_MAX_SIZE+1] = {0};
    va_list args;

    if (LOG_LEVEL_WARNING > (int)ctx->ll)
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
    _log_out(ctx, LOG_LEVEL_WARNING, output);
    va_end(args);
}

void log_info(const char* format, ...)
{
    log_context_t* ctx = _log_ctx();
    int ret = 0;
    char output[LOG_MESSAGE_MAX_SIZE+1] = {0};
    va_list args;

    if (LOG_LEVEL_INFO > (int)ctx->ll)
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
    _log_out(ctx, LOG_LEVEL_INFO, output);
    va_end(args);
}

void log_debug(const char* format, ...)
{
    log_context_t* ctx = _log_ctx();
    int ret = 0;
    char output[LOG_MESSAGE_MAX_SIZE+1] = {0};
    va_list args;

    if (LOG_LEVEL_DEBUG > (int)ctx->ll)
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
    _log_out(ctx, LOG_LEVEL_DEBUG, output);
    va_end(args);
}


void log_init(int _log_mode, int _log_level, const char* path, const char* app_name)
{
    log_context_t* ctx = _log_ctx();

    ctx->lm = (log_mode)_log_mode;
    ctx->ll = (log_level)_log_level;

     if (ctx->lm & LOG_MODE_FILE)
    {
        if (path == NULL)
        {
            fprintf(stderr, "path is not specified while log mode is file mode");
            return ;
        }

        if (app_name == NULL)
        {
            fprintf(stderr, "app is not specified");
            return ;
        }

        strncpy(ctx->log_path, path, FILENAME_MAX);
        strncpy(ctx->app_name, app_name, FILENAME_MAX);

       if (-1 == _log_load( ctx))
       {
            fprintf(stderr, "log load fail!");
            return ;
       }

        pthread_create(&ctx->log_file_tid, NULL, _log_flush_thread, ctx);
    }
}
