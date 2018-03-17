#ifndef __AS_EVENT_H__
#define __AS_EVENT_H__
#include "as_config.h"
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#if AS_APP_OS == AS_OS_WIN32
#include <windows.h>
#endif


#if AS_APP_OS == AS_OS_LINUX
typedef struct tagASEvent
{
    pthread_mutex_t EventMutex;
    pthread_cond_t  EventCond;
}as_event_t;
#elif AS_APP_OS == AS_OS_WIN32
typedef struct tagASEvent
{
    HANDLE EventHandle;
}as_event_t;
#endif

as_event_t* as_create_event();
int32_t   as_wait_event(as_event_t *pstASEvent, int32_t lTimeOut);
int32_t   as_set_event(as_event_t *pstASEvent);
int32_t   as_reset_event(as_event_t *pstASEvent);
int32_t   as_destroy_event(as_event_t *pstASEvent );

#endif

