#ifndef LOG_BUFFER_H__
#define LOG_BUFFER_H__

#include <pthread.h>

typedef struct log_buffer_node
{
    char *buffer;
    int length;
    struct log_buffer_node *next;
}log_buffer_node_t;

extern log_buffer_node_t* create_buffer();
extern void free_buffer(log_buffer_node_t* buffer);

typedef struct log_buffer_pool
{
    log_buffer_node_t* header;
    log_buffer_node_t* current;
    pthread_mutex_t*   mutex;
}log_buffer_pool_t;

#endif//LOG_BUFFER_H__
