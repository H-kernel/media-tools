#ifndef __AS_MEDIA_KENERL_THREAD_H__
#define __AS_MEDIA_KENERL_THREAD_H__
#include "as_config.h"
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#if AS_APP_OS == AS_OS_WIN32
#include <windows.h>
#endif

#define  AS_DEFAULT_STACK_SIZE (2*1024*1024)

#if AS_APP_OS == AS_OS_LINUX
typedef  void* ( * AS_THREAD_FUNC)(void *);
typedef struct tagMKThread
{
    pthread_attr_t attr;
    pthread_t pthead;
}as_thread_t;

#elif AS_APP_OS == AS_OS_WIN32
typedef  uint32_t (__stdcall * AS_THREAD_FUNC)(void *);
typedef struct tagMKThread
{
    uint32_t ptheadID;
    HANDLE pthead;
}as_thread_t;
#endif

int32_t  as_create_thread( AS_THREAD_FUNC pfnThread, void *args,
          as_thread_t **pstMKThread,uint32_t ulStackSize);

int32_t  as_join_thread(as_thread_t *pstMKThread);
void     as_thread_exit(void *retval);
uint32_t as_get_threadid();
#if AS_APP_OS == AS_OS_LINUX
pthread_t  as_thread_self();
#elif AS_APP_OS == AS_OS_WIN32
HANDLE as_thread_self(void);
#endif
#endif /* __AS_MEDIA_KENERL_THREAD_H__ */
