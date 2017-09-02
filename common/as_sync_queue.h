#ifndef __AS_MEDIA_KENERL_BUFFER_QUEUE_H__
#define __AS_MEDIA_KENERL_BUFFER_QUEUE_H__
#include "as_config.h"
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#if AS_APP_OS == AS_OS_WIN32
#include <windows.h>
#endif
#include "as_mutex.h"

typedef struct _tagAsNode
{
    AsNode   *next;
    int8_t   *buffer;
}AsNode;

typedef struct _tagAsBufferQueue
{
    AsNode      *head;
    AsNode      *tail;
    uint32_t     ulCount;
    uint32_t     ulFree;
    as_mutex_t  *h_lock;
    as_mutex_t  *t_lock;
}AsBufferQueue;


int32_t   as_create_buffer_queue(AsBufferQueue** queue,uint32_t ulMaxCount);

int32_t   as_destory_buffer_queue(AsBufferQueue* queue);

int32_t   as_destory_buffer_enqueue(AsBufferQueue* queue,void* pBuffer);

int32_t   as_destory_buffer_dequeue(AsBufferQueue* queue,void** pBuffer);

uint32_t  as_get_buffer_queue_size(AsBufferQueue* queue);

#endif /* __AS_MEDIA_KENERL_BUFFER_QUEUE_H__ */
