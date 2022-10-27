#ifndef LOG_BUFFER_H__
#define LOG_BUFFER_H__

typedef struct log_file_buffer
{
    char *buffer;
    int length;
    struct log_file_buffer *next;
}log_file_buffer_t;

extern log_file_buffer_t* create_buffer();
extern void free_buffer(log_file_buffer_t* buffer);

#endif//LOG_BUFFER_H__
