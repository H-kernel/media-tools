#include "stdafx.h"
#include "as_media_exchange.h"
#include "as_log.h"
#include "as_lock_guard.h"
#include "as_ini_config.h"
#include "as_timer.h"
#include "as_mem.h"

ASMediaSession::ASMediaSession()
{
    m_ullSessionID  = 0;
    m_ulSessionType = 0;
    m_ulRefCount    = 1;
}
ASMediaSession::~ASMediaSession()
{
}

int32_t ASMediaSession::addReference()
{
    m_ulRefCount++;
    AS_LOG(AS_LOG_DEBUG,"addReference session[%lld] ref[%d].", m_ullSessionID, m_ulRefCount);
    return m_ulRefCount;
}
int32_t ASMediaSession::decReference()
{
    m_ulRefCount--;
    AS_LOG(AS_LOG_DEBUG,"decReference session[%lld] ref[%d].", m_ullSessionID, m_ulRefCount);
    return m_ulRefCount;
}


ASExchange::ASExchange()
{
}
ASExchange::~ASExchange()
{
}
int32_t ASExchange::addSendSession(uint64_t ullSessionID)
{
    return AS_ERROR_CODE_OK;
}
int32_t ASExchange::sendMediaData(MEDIA_DATA_BLOCK **pMbArray, uint32_t MsgCount)
{
    return AS_ERROR_CODE_OK;
}


ASMediaExchangeSvr::ASMediaExchangeSvr()
{
    m_sessionMutex       = NULL;
    m_ullSessionIdx      = 0;

    m_ExchangMapArray    = NULL;
    m_ulTdIndex          = 0;
    m_mutex              = NULL;
    m_bRunning           = false;
    m_ThreadHandleArray  = NULL;
    m_ThreadDealCount    = NULL;
    m_ulThreadCount      = 0;
    m_pDataExchangeQueue = NULL;
}

ASMediaExchangeSvr::~ASMediaExchangeSvr()
{
}
int32_t   ASMediaExchangeSvr::open(uint32_t ulThreadCount)
{
    AS_LOG(AS_LOG_DEBUG,"ASMediaExchangeSvr::open,begin.");
    m_ulThreadCount = ulThreadCount;
    m_mutex = as_create_mutex();
    if(NULL == m_mutex) {
        AS_LOG(AS_LOG_CRITICAL,"ASMediaExchangeSvr::open,create the mutex fail.");
        return AS_ERROR_CODE_FAIL;
    }
    m_sessionMutex = as_create_mutex();
    if(NULL == m_sessionMutex) {
        AS_LOG(AS_LOG_CRITICAL,"ASMediaExchangeSvr::open,create the session mutex fail.");
        return AS_ERROR_CODE_FAIL;
    }

    m_ExchangMapArray = AS_NEW(m_ExchangMapArray,m_ulThreadCount);
    if(NULL == m_ExchangMapArray) {
        AS_LOG(AS_LOG_CRITICAL,"ASMediaExchangeSvr::open,create exchange map array fail.");
        return AS_ERROR_CODE_FAIL;
    }
    uint32_t i = 0;
    EXCHANGEMAP* pMap = NULL;
    for(i = 0;i < m_ulThreadCount;i++) {
        pMap = AS_NEW(pMap);
        if(NULL == pMap) {
            AS_LOG(AS_LOG_CRITICAL,"ASMediaExchangeSvr::open,create map[%d] fail.",i);
            return AS_ERROR_CODE_FAIL;
        }
        m_ExchangMapArray[i] = pMap;
    }

    m_pDataExchangeQueue = AS_NEW(m_pDataExchangeQueue,m_ulThreadCount);
    if(NULL == m_ThreadDealCount) {
        AS_LOG(AS_LOG_CRITICAL,"ASMediaExchangeSvr::open,create queue array fail.");
        return AS_ERROR_CODE_FAIL;
    }
    CMediaDataQueue* pQueue = NULL;
    for(i = 0;i < m_ulThreadCount;i++) {
        pQueue = AS_NEW(pQueue);
        if(NULL == pQueue) {
            AS_LOG(AS_LOG_CRITICAL,"ASMediaExchangeSvr::open,create queue[%d] fail.",i);
            return AS_ERROR_CODE_FAIL;
        }
        m_pDataExchangeQueue[i] = pQueue;
    }

    m_ThreadDealCount = AS_NEW(m_ThreadDealCount,m_ulThreadCount);
    if(NULL == m_ThreadDealCount) {
        AS_LOG(AS_LOG_CRITICAL,"ASMediaExchangeSvr::open,create thread deal count array fail.");
        return AS_ERROR_CODE_FAIL;
    }
    m_ThreadHandleArray = AS_NEW(m_ThreadHandleArray,m_ulThreadCount);
    if(NULL == m_ThreadHandleArray) {
        AS_LOG(AS_LOG_CRITICAL,"ASMediaExchangeSvr::open,create thread handle array fail.");
        return AS_ERROR_CODE_FAIL;
    }
    m_bRunning = true;
    /* start the deal thread */
    for(i = 0;i < m_ulThreadCount;i++) {
        if( AS_ERROR_CODE_OK != as_create_thread((AS_THREAD_FUNC)exchange_invoke,
                                                 this,&m_ThreadHandleArray[i],AS_DEFAULT_STACK_SIZE)) {
            AS_LOG(AS_LOG_ERROR,"ASMediaExchangeSvr::open,create the exchaneg thread fail.");
            return AS_ERROR_CODE_FAIL;
        }
        m_ThreadDealCount[i] = 0;
    }
    AS_LOG(AS_LOG_DEBUG,"ASMediaExchangeSvr::open,end.");
    return AS_ERROR_CODE_OK;
}
void      ASMediaExchangeSvr::close()
{
    m_bRunning = false;
    return;
}
int32_t   ASMediaExchangeSvr::regFactory(const ASSessionFactory* factory)
{
    as_lock_guard locker(m_sessionMutex);
    m_factMap.insert(FACTORYMAP::value_type(factory->getSessionType(),factory));
    return AS_ERROR_CODE_OK;
}
ASMediaSession* ASMediaExchangeSvr::createSession(uint32_t ulType)
{
    ASMediaSession* pSession = NULL;
    ASSessionFactory* factory = NULL;
    as_lock_guard locker(m_sessionMutex);
    FACTORYMAP::iterator iter = m_factMap.find(ulType);
    if(iter == m_factMap.end()) {
        AS_LOG(AS_LOG_DEBUG,"ASMediaExchangeSvr::createSession,not find the factory,type:[%d].",ulType);
        return NULL;
    }
    factory = iter->second;
    pSession = factory->createSession();
    if(NULL == pSession) {
        AS_LOG(AS_LOG_DEBUG,"ASMediaExchangeSvr::createSession,create session fail,type:[%d].",ulType);
        return NULL;
    }
    pSession->setSessionID(m_ullSessionIdx++);
    m_sessionMap.insert(SESSIONMAP::value_type(pSession->getSessionID(),pSession));
    return NULL;
}
ASMediaSession* ASMediaExchangeSvr::findSession(uint64_t ullSessionID)
{
    ASMediaSession* pSession = NULL;
    as_lock_guard locker(m_sessionMutex);
    SESSIONMAP::iterator iter = m_sessionMap.find(ullSessionID);
    if(iter == m_sessionMap.end()) {
        AS_LOG(AS_LOG_DEBUG,"ASMediaExchangeSvr::findSession, find the session:[%ld] fail.",ullSessionID);
        return NULL;
    }
    pSession = iter->second;
    (void)pSession->addReference();
    return pSession;
}
void ASMediaExchangeSvr::releaseSession(ASMediaSession* pSession)
{
    if(NULL == pSession)
    {
        return;
    }

    uint64_t ullSessionID = pSession->getSessionID();

    releaseSession(ullSessionID);

    return;
}
void ASMediaExchangeSvr::releaseSession(uint64_t ullSessionID)
{
    ASMediaSession* pSession = NULL;
    as_lock_guard locker(m_sessionMutex);
    SESSIONMAP::iterator iter = m_sessionMap.find(ullSessionID);
    if(iter == m_sessionMap.end()) {
        AS_LOG(AS_LOG_DEBUG,"ASMediaExchangeSvr::releaseSession, find the session:[%ld] fail.",ullSessionID);
        return ;
    }
    pSession = iter->second;
    if(0 < pSession->decReference())
    {
        return;
    }

    //need delete
    m_sessionMap.erase(iter);

    ASSessionFactory* factory = NULL;
    FACTORYMAP::iterator iter = m_factMap.find(pSession->getSessionType());
    if(iter == m_factMap.end()) {
        AS_LOG(AS_LOG_DEBUG,"ASMediaExchangeSvr::releaseSession,not find the factory,type:[%d].",pSession->getSessionType());
        AS_DELETE(pSession);
        return ;
    }
    factory = iter->second;
    pSession = factory->destorySession(pSession);

    return;
}

int32_t   ASMediaExchangeSvr::addData(MEDIA_DATA_BLOCK* pBlock)
{
    return AS_ERROR_CODE_OK;
}
int32_t   ASMediaExchangeSvr::regExChanger(uint64_t ullRecvID,uint64_t ullSendID)
{
    return AS_ERROR_CODE_OK;
}
int32_t   ASMediaExchangeSvr::unRegExChanger(uint64_t ullRecvID,uint64_t ullSendID)
{
    return AS_ERROR_CODE_OK;
}
void *ASMediaExchangeSvr::exchange_invoke(void *arg)
{
    ASMediaExchangeSvr* pExchange = (ASMediaExchangeSvr*)arg;
    pExchange->exchange_thread();
    return NULL;
}

void      ASMediaExchangeSvr::exchange_thread()
{
    uint32_t ulThreadIndex = thread_index();
    AS_LOG(AS_LOG_DEBUG,"ASMediaExchangeSvr::exchange_thread,thread:[%d],begin.",ulThreadIndex);

    CMediaDataQueue* pQueue = m_pDataExchangeQueue[ulThreadIndex];

    CMediaDataBlock* mb = NULL;

    while(m_bRunning)
    {
        (void)pQueue->dequeue_head(mb, &timeout);
        if (NULL == mb)
        {
            continue;
        }
    }
    AS_LOG(AS_LOG_DEBUG,"ASMediaExchangeSvr::exchange_thread,thread:[%d],end.",ulThreadIndex);
    return;
}


