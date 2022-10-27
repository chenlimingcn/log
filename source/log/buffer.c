#include "buffer.h"

#include <string.h>

#include "macros.h"

extern log_buffer_node_t* create_buffer()
{
    log_buffer_node_t* buffer = (log_buffer_node_t*)malloc(sizeof(log_buffer_node_t));
    buffer->buffer = (char*)malloc(LOG_BUFFER_SIZE + 1);
    buffer->length = 0;
    buffer->next = NULL;

    return buffer;
}

extern void free_buffer(log_buffer_node_t* buffer)
{
    free(buffer->buffer);
    free(buffer);
}