#include "as_mutex.h"
#include "as_config.h"
#include "as_common.h"
#if AS_APP_OS == AS_OS_LINUX
#include <pthread.h>
#elif AS_APP_OS == AS_OS_WIN32
#endif


as_mutex_t *as_create_mutex()
{
    int32_t ulResult = AS_ERROR_CODE_OK ;

    as_mutex_t *pstMutex = NULL ;

    pstMutex = (as_mutex_t *)(void*) malloc(sizeof(as_mutex_t));
    if ( NULL == pstMutex  )
    {
        return NULL ;
    }
#if AS_APP_OS == AS_OS_LINUX
    if(pthread_mutexattr_init(&pstMutex->attr) != 0)
    {
        return NULL ;
    }
    pthread_mutexattr_settype(&pstMutex->attr, PTHREAD_MUTEX_RECURSIVE);

    ulResult = (int32_t)pthread_mutex_init( &pstMutex->mutex, &pstMutex->attr);
    if( AS_ERROR_CODE_OK != ulResult )
    {
        free( pstMutex );
        return NULL ;
    }
#elif AS_APP_OS == AS_OS_WIN32
    pstMutex->mutex = CreateMutex(NULL,0,NULL);
    if (NULL == pstMutex->mutex)
    {
        free( pstMutex );
        return NULL ;
    }
    (void)ulResult; //¹ýPCLINT
#endif
    return pstMutex ;
}


int32_t as_destroy_mutex( as_mutex_t *pstMutex )
{
    int32_t ulResult = AS_ERROR_CODE_OK ;

#if AS_APP_OS == AS_OS_LINUX
    pthread_mutex_destroy( &pstMutex->mutex );
#elif AS_APP_OS == AS_OS_WIN32
    (void)CloseHandle(pstMutex->mutex);
#endif
    free( pstMutex );

    return ulResult ;
}


int32_t as_mutex_lock( as_mutex_t *pstMutex )
{
    int32_t ulResult = AS_ERROR_CODE_OK;

    if(NULL == pstMutex)
    {
        return AS_ERROR_CODE_FAIL;
    }

#if AS_APP_OS == AS_OS_LINUX
    ulResult = (int32_t)pthread_mutex_lock(&pstMutex->mutex);
    if( AS_ERROR_CODE_OK != ulResult )
    {
        return ulResult ;
    }
#elif AS_APP_OS == AS_OS_WIN32
    ulResult = WaitForSingleObject(pstMutex->mutex,INFINITE);
    if(WAIT_OBJECT_0 != ulResult)
    {
        return AS_ERROR_CODE_FAIL;
    }
#endif
    return AS_ERROR_CODE_OK ;
}

int32_t as_mutex_unlock( as_mutex_t *pstMutex )
{
    int32_t ulResult = AS_ERROR_CODE_OK ;

#if AS_APP_OS == AS_OS_LINUX
    ulResult = (int32_t)pthread_mutex_unlock(&pstMutex->mutex);
    if( AS_ERROR_CODE_OK != ulResult )
    {
        return ulResult ;
    }
#elif AS_APP_OS == AS_OS_WIN32
    if((NULL == pstMutex)
        || (TRUE != ReleaseMutex(pstMutex->mutex)))
    {
        ulResult = AS_ERROR_CODE_FAIL;
        return ulResult;
    }
#endif
    return AS_ERROR_CODE_OK ;
}



