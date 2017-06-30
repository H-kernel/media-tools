#ifndef __AS_MEDIA_KENERL_THREAD_H__
#define __AS_MEDIA_KENERL_THREAD_H__
#include "as_config.h"
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#if AS_APP_OS == AS_OS_WIN32
#include <windows.h>
#endif

#define  AS_DEFAULT_STACK_SIZE (128*1024)

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

int32_t as_create_thread( AS_THREAD_FUNC pfnThread, void *args,
          as_thread_t **pstMKThread,uint32_t ulStackSize);

int32_t as_join_thread(as_thread_t *pstMKThread);
#endif /* __AS_MEDIA_KENERL_THREAD_H__ */
