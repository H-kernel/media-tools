#include "as_thread.h"
#include "as_config.h"
#include "as_common.h"
#if AS_APP_OS == AS_OS_LINUX
#include <pthread.h>
#elif AS_APP_OS == AS_OS_WIN32
#endif

/**********************************************************************************************
************************************1.init/empty queue*****************************************
head/tail/barrier
       ¡ý
       ¨ˆ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ

       enqueue:
               new = barrier->pre;
               head = tail = new;
               barrier->pre = new->pre;
               new->pre = barrier;
head/tail barrier
       ¡ý¡ý
       ¡ö¨ˆ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ

       dequeue:
              if(NULL == head->next) {
                   dequeue wait not empty
               }

************************************2.queue half**********************************************
      tail                  head barrier
       ¡ý                      ¡ý¡ý
       ¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¨ˆ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ

       enqueue:
               new = barrier->pre;
               head = tail = new;
               barrier->next = new->next;
               new->next = barrier;
      tail                    head barrier
       ¡ý                        ¡ý¡ý
       ¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¨ˆ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ

       dequeue:
               delete = tail;
               head = tail = new;
               barrier->next = new->next;
               new->next = barrier;
      tail                    head barrier
       ¡ý                        ¡ý¡ý
       ¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¨ˆ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ

3.queue full;enqueue wait not full
   tail/barrier                                   head
       ¡ý                                          ¡ý
       ¨ˆ¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö¡ö


***********************************************************************************************/

int32_t   as_create_buffer_queue(AsBufferQueue** queue,uint32_t ulMaxCount)
{
    AsNode* pNode = NULL;
    uint32_t i = 0;

    *queue = (AsBufferQueue *)(void*) malloc(sizeof(AsBufferQueue));
    if ( NULL == *queue  )
    {
        return AS_ERROR_CODE_MEM ;
    }

    /* init queue */
    *queue->head = NULL;
    *queue->tail = NULL;
    *queue->free = NULL;
    *queue->ulCount = 0;
    *queue->ulFree = 0;
    *queue->h_lock = NULL;
    *queue->t_lock = NULL;

    /* create the lockers for queue */
    *queue->h_lock = as_create_mutex();
    if ( NULL == *queue->h_lock  )
    {
        free(*queue);
        return AS_ERROR_CODE_MEM ;
    }
    *queue->t_lock = as_create_mutex();
    if ( NULL == *queue->t_lock  )
    {
        free(*queue);
        return AS_ERROR_CODE_MEM ;
    }

    /* create the barrier for queue */
    AsNode* Barrier = (AsNode *)(void*) malloc(sizeof(AsNode));
    if ( NULL == Barrier  )
    {
        free(*queue);
        return AS_ERROR_CODE_MEM ;
    }
    Barrier->next = NULL;
    Barrier->buffer = NULL;

    *queue->head = Barrier;
    *queue->tail = Barrier;

    /* create the queue node list */

    for(i = 0;i < ulMaxCount;i++)
    {
        pNode = (AsNode *)(void*) malloc(sizeof(AsNode));
        if ( NULL == pNode  )
        {
            continue;
        }
        pNode->next   = *queue->tail;
        pNode->buffer = NULL;
        Barrier->next = pNode;
        Barrier       = pNode;
    }
    *queue->ulCount = i;
    *queue->ulFree = i;
    return AS_ERROR_CODE_OK;
}

int32_t   as_destory_buffer_queue(AsBufferQueue* queue)
{
    AsNode* pNode = NULL;
    AsNode* pNext = NULL;
    uint32_t i = 0;

    if(NULL == queue)
    {
        return AS_ERROR_CODE_FAIL;
    }
    pNode = queue->head;
    pNext = pNode->next;
    pNode->next = NULL;
    pNode = pNext;
    while(NULL != pNode)
    {
        pNext  = pNode->next
        if ( NULL == pNode  )
        {
            continue;
        }
        free(pNode);
        pNode = pNext;
    }

    as_destroy_mutex(queue->h_lock);
    as_destroy_mutex(queue->t_lock);
    free(queue);
    return AS_ERROR_CODE_OK;
}

int32_t   as_buffer_enqueue(AsBufferQueue* queue,void* pBuffer)
{
    AsNode* pNode = queue->tail;
    if(NULL == pNode->buffer)
    {
        /* queue full return error */
        return AS_ERROR_CODE_FAIL;
    }

    if(AS_ERROR_CODE_OK != as_mutex_lock(queue->t_lock))
    {
        return;
    }
    pNode->buffer = pBuffer;
    return AS_ERROR_CODE_OK;
}

int32_t   as_buffer_dequeue(AsBufferQueue* queue,void** pBuffer)
{
    return AS_ERROR_CODE_OK;
}

uint32_t  as_get_buffer_queue_size(AsBufferQueue* queue)
{
    return 0;
}


