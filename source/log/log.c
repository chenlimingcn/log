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
    
    log_buffer_pool_t* buffer_pool[100];
    
    char app_name[FILENAME_MAX];
    char log_path[FILENAME_MAX];
    log_file_node_t* file_list;
    log_file_node_t* curr_file_node;
    int file_count;
	
    FILE* file;
    long  file_length;
    pthread_t file_tid;
}log_context_t;

static log_buffer_pool_t* _get_log_buffer_from_pool(log_context_t* ctx)
{
    for (int i = 0; i < 100; ++i)
    {
        log_buffer_pool_t* p = ctx->buffer_pool[i]; 
        if (pthread_mutex_trylock(p->mutex) == 0)
        {
            return p;
        }
    }

    return NULL;
}

static void _return_log_buuffer_to_pool(log_buffer_pool_t* p)
{
    pthread_mutex_unlock(p->mutex);
}

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
                ctx->curr_file_node = (log_file_node_t*)malloc(sizeof(log_file_node_t));
                ctx->curr_file_node->next = NULL;
				strcat(ctx->curr_file_node->filename, ctx->log_path);
                strcat(ctx->curr_file_node->filename, "/");
                strcat(ctx->curr_file_node->filename, filename);
                if (ctx->file_list == NULL)
                {
                    ctx->file_list = ctx->curr_file_node;
                }
                else
                {
                    log_file_node_t* tmp = ctx->file_list;
                    while (tmp->next)
                    {
                        tmp = tmp->next;
                    }
				    tmp->next = ctx->curr_file_node;
                }
                ++ctx->file_count;
			}
		}
		else if (ptr->d_type == DT_DIR)
		{
			continue;
		}
	}
	closedir(dir);

	if (ctx->curr_file_node != NULL)
	{
        struct stat st;
        if (stat(ctx->curr_file_node->filename, &st) == -1)
        {
            return -1;
        }
        ctx->file_length = st.st_size;
		ctx->file = fopen(ctx->curr_file_node->filename, "a+");
        if (ctx->file == NULL)
        {
            return -1;
        }
	}
    else
    {
        ctx->curr_file_node = (log_file_node_t*)malloc(sizeof(log_file_node_t));
        ctx->curr_file_node->next = NULL;
        _log_generate_filename(ctx, ctx->curr_file_node->filename, FILENAME_MAX);
        ctx->file_length = 0;
		ctx->file = fopen(ctx->curr_file_node->filename, "a+");
        if (ctx->file == NULL)
        {
            free(ctx->curr_file_node);
            ctx->curr_file_node = NULL;
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
        
        for (int i = 0; i < 100; ++i)
        {
            ctx->buffer_pool[i] = malloc(sizeof(log_buffer_pool_t));
            ctx->buffer_pool[i]->current = create_buffer();
            ctx->buffer_pool[i]->header = ctx->buffer_pool[i]->current;

            ctx->buffer_pool[i]->mutex = malloc(sizeof(pthread_mutex_t));
            pthread_mutex_init(ctx->buffer_pool[i]->mutex, NULL);
        }
        
        ctx->lm = LOG_MODE_FILE;
        ctx->ll = LOG_LEVEL_WARNING;

        ctx->file_list = NULL;
        ctx->curr_file_node = NULL;
        ctx->file_count = 0;
        ctx->file = NULL;
        ctx->file_length = 0;
    }

    return ctx;
}

static void _log_out_prepare(log_context_t* ctx, long length)
{
    if (length + ctx->file_length >= LOG_FILE_MAX_SIZE)
    {
        fclose(ctx->file);
        while (ctx->file_count > 100)
        {
            log_file_node_t *file = ctx->file_list;
            (void)remove(file->filename);
            ctx->file_list = file->next;
            free(file);
        }
        log_file_node_t *file = (log_file_node_t*)malloc(sizeof(log_file_node_t));
        file->next = NULL;

        _log_generate_filename(ctx, file->filename, FILENAME_MAX);
        ctx->file = fopen(file->filename, "a+");
        ctx->file_length = 0;
        if (ctx->file == NULL)
        {
            free(file);
        }
        else
        {
            if (ctx->curr_file_node)
            {
                ctx->curr_file_node->next = file;
            }
            ctx->curr_file_node = file;
            ++ctx->file_count;
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
    //_log_out_prepare(ctx->file_length);
    //ret = fprintf(ctx->file, strmsg);
    //if (ret > 0)
    //{
    //    ctx->file_length += ret;
    //}
    //return ;
        
    log_buffer_pool_t* p = _get_log_buffer_from_pool(ctx);
    if (p->current->length && length + p->current->length >= LOG_BUFFER_SIZE) // TODO: send buffer full sinal to save file thread and wait for complete signal
    {
        log_buffer_node_t* log_buffer = create_buffer();

        if (p->header == NULL)
        {
            p->header = log_buffer;
        }
        else
        {
            p->current->next = log_buffer;
        }
        p->current = log_buffer;

        // common_usleep(1);
    }

    memcpy(p->current->buffer + p->current->length, strmsg, length);
    p->current->length += length;

    _return_log_buuffer_to_pool(p);
}

static void* _log_flush_thread(void* arg)
{
    log_context_t* ctx = (log_context_t*)arg;
    int ret = 0;
    // TODO: start a timer in this thread. when time is reaching, send timeout signal in timer function

    while (1)
    {
        for (int i = 0; i < 100; ++i)
        {
            log_buffer_pool_t* p = ctx->buffer_pool[i];
            if (pthread_mutex_trylock(p->mutex) != 0)
            {
                usleep(500000);
                continue;
            }

            if (p == NULL || p->header ==  NULL || p->header->length <= 0)
            {
                pthread_mutex_unlock(p->mutex);
                usleep(500000);
                continue;
            }

            log_buffer_node_t *buffer = p->header;
            if (p->current == buffer)
            {
                log_buffer_node_t* tmp = create_buffer();
                p->header = tmp;
                p->current = tmp;
            }
            else
            {
                p->header = p->header->next;
            }

            // If the size of the file is greater than LOG_FILE_MAX_SIZE
            _log_out_prepare(ctx, strlen(buffer->buffer));
            if (buffer->length > 0)
            {
                if (ctx->file)
                {
                    ret = fprintf(ctx->file, "%s", buffer->buffer);
                    if (ret > 0)
                    {
                        ctx->file_length += ret;
                    }
                }

                free(buffer->buffer);
                free(buffer);
            }
            else
            {
                buffer->next = p->header;
                p->header = buffer;
            }

            pthread_mutex_unlock(p->mutex);
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
    if (ctx->lm & LOG_MODE_FILE)
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

        pthread_create(&ctx->file_tid, NULL, _log_flush_thread, ctx);
    }
}
