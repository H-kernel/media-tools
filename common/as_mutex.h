#ifndef __AS_MEDIA_KENERL_MUTEX_H__
#define __AS_MEDIA_KENERL_MUTEX_H__
#include "as_config.h"
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#if AS_APP_OS == AS_OS_WIN32
#include <windows.h>
#endif
#if AS_APP_OS == AS_OS_LINUX
typedef struct tagMKMutex
{
    pthread_mutex_t     mutex;
    pthread_mutexattr_t attr;
}as_mutex_t;

#elif AS_APP_OS == AS_OS_WIN32
typedef struct tagMKMutex
{
    HANDLE              mutex;
}as_mutex_t;
#endif

as_mutex_t *as_create_mutex();
int32_t     as_destroy_mutex( as_mutex_t *pstMutex );
int32_t     as_mutex_lock( as_mutex_t *pstMutex );
int32_t     as_mutex_unlock( as_mutex_t *pstMutex );

#endif /* __AS_MEDIA_KENERL_MUTEX_H__ */


