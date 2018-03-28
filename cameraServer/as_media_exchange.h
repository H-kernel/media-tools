#ifndef __AS_MEDIA_EXCHANGE_SERVER_H__
#define __AS_MEDIA_EXCHANGE_SERVER_H__

#include <list>
#include <map>
#include "as_def.h"
#include "as.h"


typedef enum AS_MEDIA_DATA_TYPE
{
    AS_MEDIA_DATA_TYPE_ADD   = 0,  /* ADD THE SESSION */
    AS_MEDIA_DATA_TYPE_DEL   = 1,  /* DEL THE SESSION */
    AS_MEDIA_DATA_TYPE_DATA  = 2,  /* MEDIA FRAME DATA */
    AS_MEDIA_DATA_TYPE_MAX
}MEDIA_DATA_TYPE;


typedef struct tagMEDIA_DATA_BLOCK
{
    MEDIA_DATA_TYPE    enType;
    uint64_t           ullRecvID;    //recv session ID
    union dataValue {
        uint64_t       ullSendID;    //send session ID
        uint32_t       ulSize;       //Data Size
    } value;
    uint8_t            szData[1];
}MEDIA_DATA_BLOCK;


class ASMediaSession
{
    friend class ASMediaExchangeSvr;
public:
    ASMediaSession();
    virtual ~ASMediaSession();

    void setSessionID(uint64_t ullSessionID){m_ullSessionID = ullSessionID;};

    uint64_t getSessionID(){return m_ullSessionID;};

    virtual uint32_t getSessionType(){ return m_ulSessionType;};

    virtual int32_t sendMediaData(MEDIA_DATA_BLOCK **pMbArray, uint32_t MsgCount) = 0;
protected:
    int32_t addReference();
    int32_t decReference();
protected:
    uint64_t     m_ullSessionID;
    uint32_t     m_ulSessionType;
    int32_t      m_ulRefCount;
};

class ASSessionFactory
{
public:
    ASSessionFactory();
    virtual ~ASSessionFactory();
    virtual ASMediaSession* createSession() = 0;
    virtual void destorySession(ASMediaSession* pSession) = 0;
    virtual uint32_t getSessionType() = 0;
};

class ASExchange
{
public:
    ASExchange();
    virtual ~ASExchange();
    int32_t addSendSession(uint64_t ullSessionID);
    int32_t sendMediaData(MEDIA_DATA_BLOCK **pMbArray, uint32_t MsgCount);
private:
    typedef std::list<ASMediaSession*>    SENDSESSSIONLIST;
    SENDSESSSIONLIST   m_SendSessionList;
};

class ASMediaExchangeSvr
{
public:
    static ASMediaExchangeSvr& instance()
    {
        static ASMediaExchangeSvr objASMediaExchangeSvr;
        return objASMediaExchangeSvr;
    }
    virtual ~ASMediaExchangeSvr();
public:
    int32_t   open(uint32_t ulThreadCount);
    void      close();
    int32_t   regFactory(const ASSessionFactory* factory);
    ASMediaSession* createSession(uint32_t ulType);
    ASMediaSession* findSession(uint64_t ullSessionID);
    void releaseSession(ASMediaSession* pSession);
    void releaseSession(uint64_t ullSessionID);
public:
    int32_t   addData(MEDIA_DATA_BLOCK* pBlock);
    int32_t   regExChanger(uint64_t ullRecvID,uint64_t ullSendID);
    int32_t   unRegExChanger(uint64_t ullRecvID,uint64_t ullSendID);
public:
    void      exchange_thread();
protected:
    ASMediaExchangeSvr();
private:
    static void *exchange_invoke(void *arg);
    u_int32_t thread_index()
    {
        as_mutex_lock(m_mutex);
        u_int32_t index = m_ulTdIndex;
        m_ulTdIndex++;
        as_mutex_unlock(m_mutex);
        return index;
    }
private:
    typedef std::map<uint32_t,ASSessionFactory*>  FACTORYMAP;
    FACTORYMAP        m_factMap;
    typedef std::map<uint64_t,ASMediaSession*>    SESSIONMAP;
    SESSIONMAP        m_sessionMap;
    as_mutex_t       *m_sessionMutex;
    uint64_t          m_ullSessionIdx;
private:
    typedef std::map<uint64_t,ASExchange*>        EXCHANGEMAP;
    EXCHANGEMAP     **m_ExchangMapArray;
    u_int32_t         m_ulTdIndex;
    as_mutex_t       *m_mutex;
    bool              m_bRunning;
    as_thread_t     **m_ThreadHandleArray;
    u_int32_t        *m_ThreadDealCount;
    CMediaDataQueue** m_pDataExchangeQueue;
    uint32_t          m_ulThreadCount;
};
#endif /* __AS_MEDIA_EXCHANGE_SERVER_H__ */
