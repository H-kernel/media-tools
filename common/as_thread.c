#include "as_thread.h"
#include "as_config.h"
#include "as_common.h"
#if AS_APP_OS == AS_OS_LINUX
#include <pthread.h>
#elif AS_APP_OS == AS_OS_WIN32
#endif


int32_t  as_create_thread( AS_THREAD_FUNC pfnThread, void *args, as_thread_t **pstSVSThread,uint32_t ulStackSize)
{
    as_thread_t *pstThread = NULL ;

    pstThread = (as_thread_t*)(void*)malloc(sizeof(as_thread_t));
    if( NULL == pstThread )
    {
        return AS_ERROR_CODE_MEM;
    }

#if AS_APP_OS == AS_OS_LINUX
    if ( pthread_attr_init(&pstThread->attr) != 0 )
    {
        free(pstThread);
        return AS_ERROR_CODE_FAIL ;
    }

    pthread_attr_setdetachstate(&pstThread->attr, PTHREAD_CREATE_JOINABLE );

    if( 0 == ulStackSize )
    {
        ulStackSize = AS_DEFAULT_STACK_SIZE;
    }
    if (pthread_attr_setstacksize(&pstThread->attr, (size_t)ulStackSize))
    {
        free(pstThread);
        return AS_ERROR_CODE_FAIL ;
    }

    if ( pthread_create(&pstThread->pthead, &pstThread->attr, pfnThread, args) != 0 )
    {
        free(pstThread);
        return AS_ERROR_CODE_FAIL ;
    }
#elif AS_APP_OS == AS_OS_WIN32
    pstThread->pthead = CreateThread(NULL,ulStackSize,pfnThread,args,0,&pstThread->ptheadID);
    if (NULL == pstThread->pthead)
    {
        free(pstThread);
        return AS_ERROR_CODE_FAIL ;
    }
#endif
    *pstSVSThread = pstThread ;

    return AS_ERROR_CODE_OK;
}

int32_t as_join_thread(as_thread_t *pstMKThread)
{
#if AS_APP_OS == AS_OS_LINUX
    pthread_join(pstMKThread->pthead, 0);
#elif AS_APP_OS == AS_OS_WIN32
    (void)WaitForSingleObject(pstMKThread->pthead, 0);
    (void)CloseHandle(pstMKThread->pthead);
#endif
    return AS_ERROR_CODE_OK;
}


