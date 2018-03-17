#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#include "as_event.h"
#include "as_common.h"
#if AS_APP_OS == AS_OS_LINUX
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#endif


as_event_t* as_create_event()
{
    int32_t result = AS_ERROR_CODE_OK;

    as_event_t *pstASEvent = NULL;

    pstASEvent = (as_event_t *)malloc(sizeof(as_event_t));
    if (NULL == pstASEvent)
    {
        return NULL;
    }

#if AS_APP_OS == AS_OS_LINUX

    result = pthread_mutex_init(&pstASEvent->EventMutex, 0);
    if( AS_ERROR_CODE_OK != result )
    {
        free(pstASEvent);
        return NULL;
    }

    result = pthread_cond_init(&pstASEvent->EventCond, 0);
    if( AS_ERROR_CODE_OK != result )
    {
        pthread_mutex_destroy(&pstASEvent->EventMutex);
        free(pstASEvent);
        return NULL;
    }

#elif AS_APP_OS == AS_OS_WIN32
    pstASEvent->EventHandle = CreateEvent(0, FALSE, FALSE, 0);
    if (NULL == pstASEvent->EventHandle)
    {
        (void)CloseHandle(pstASEvent->EventHandle);
        free(pstASEvent);
        return NULL;
    }
#endif

    return pstASEvent;
}

int32_t as_wait_event(as_event_t *pstASEvent, int32_t lTimeOut)
{
    int32_t lResult = AS_ERROR_CODE_OK;

#if AS_APP_OS == AS_OS_LINUX
    struct timespec ts;
    struct timeval  tv;

    gettimeofday(&tv, 0);
    ts.tv_sec  = tv.tv_sec  + lTimeOut/1000;
    ts.tv_nsec = (tv.tv_usec + (lTimeOut %1000)*1000) * 1000;


    (void)pthread_mutex_lock(&pstASEvent->EventMutex);
    if( 0 != lTimeOut )
    {
        lResult = pthread_cond_timedwait(&pstASEvent->EventCond,
                                    &pstASEvent->EventMutex,&ts);
    }
    else
    {
        lResult = pthread_cond_wait(&pstASEvent->EventCond,
                                 &pstASEvent->EventMutex);
    }
    (void)pthread_mutex_unlock(&pstASEvent->EventMutex);

    if( AS_ERROR_CODE_OK != lResult )
    {
        switch(lResult)
        {
            case ETIMEDOUT:
            {
                lResult = AS_ERROR_CODE_TIMEOUT;
                break;
            }

            default:
            {
                lResult = AS_ERROR_CODE_SYS;
                break;
            }
        }
    }

#elif AS_APP_OS == AS_OS_WIN32

    uint32_t ulWaitTime = lTimeOut;
    if (0 == lTimeOut)
    {
        ulWaitTime = INFINITE;
    }

    lResult = (long)WaitForSingleObject(pstASEvent->EventHandle, ulWaitTime);
    switch(lResult)
    {
        case WAIT_TIMEOUT:
        {
            lResult = AS_ERROR_CODE_TIMEOUT;
            break;
        }

        case WAIT_ABANDONED:
        {
            lResult = AS_ERROR_CODE_SYS;
            break;
        }
        case WAIT_OBJECT_0:
        {
            lResult = AS_ERROR_CODE_OK;
            break;
        }
        default:
        {
            lResult = AS_ERROR_CODE_SYS;
        }
        break;
    }

#endif

    return lResult;
/*lint -e818*/ //使用公共平台源代码，是否Const不做要求
}

int32_t as_set_event(as_event_t *pstASEvent)
{
    int32_t lResult = AS_ERROR_CODE_OK;

#if AS_APP_OS == AS_OS_LINUX
    lResult = pthread_cond_signal(&pstASEvent->EventCond);
    if(AS_ERROR_CODE_OK != lResult)
    {
        lResult = AS_ERROR_CODE_SYS;
    }

#elif AS_APP_OS == AS_OS_WIN32

    lResult = SetEvent(pstASEvent->EventHandle);

    if (AS_ERROR_CODE_OK != lResult)
    {
        lResult = AS_ERROR_CODE_OK;
    }
    else
    {
        lResult = AS_ERROR_CODE_SYS;
    }

#endif

    return lResult ;
}
int32_t as_reset_event(as_event_t *pstASEvent)
{
    int32_t lResult = AS_ERROR_CODE_OK;

#if AS_APP_OS == AS_OS_LINUX
    lResult = pthread_mutex_init(&pstASEvent->EventMutex, 0);
    if( AS_ERROR_CODE_OK != lResult )
    {
        return AS_ERROR_CODE_SYS;
    }

    lResult = pthread_cond_init(&pstASEvent->EventCond, 0);
    if( AS_ERROR_CODE_OK != lResult )
    {
        pthread_mutex_destroy(&pstASEvent->EventMutex);
        return AS_ERROR_CODE_SYS;
    }
#elif AS_APP_OS == AS_OS_WIN32

    lResult = ResetEvent(pstASEvent->EventHandle);

    if (AS_ERROR_CODE_OK != lResult)
    {
        lResult = AS_ERROR_CODE_OK;
    }
    else
    {
        lResult = AS_ERROR_CODE_SYS;
    }

#endif

    return lResult ;
}

int32_t as_destroy_event(as_event_t *pstASEvent )
{
    if ( NULL == pstASEvent )
    {
        return AS_ERROR_CODE_PARAM;
    }

#if AS_APP_OS == AS_OS_LINUX
    pthread_cond_destroy(&pstASEvent->EventCond);
    pthread_mutex_destroy(&pstASEvent->EventMutex);
#elif AS_APP_OS == AS_OS_WIN32
    (void)CloseHandle(pstASEvent->EventHandle);
#endif

    return AS_ERROR_CODE_OK;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

