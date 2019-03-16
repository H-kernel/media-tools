/******************************************************************************
   版权所有 (C), 2001-2011, M.Kernel

 ******************************************************************************
  文件名          : as_conn_manage.h
  版本号          : 1.0
  作者            : hexin
  生成日期        : 2007-4-02
  最近修改        :
  功能描述        :
  函数列表        :
  修改历史        :
  1 日期          : 2007-4-02
    作者          : hexin
    修改内容      : 生成
*******************************************************************************/



#ifndef CCONNMGR_H_INCLUDE
#define CCONNMGR_H_INCLUDE

#ifdef ENV_LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#endif //AS_OS_LINUX

 #ifdef WIN32
 //#include <winsock2.h>
 #endif
//#pragma comment(lib,"ws2_32.lib")

#include <list>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "as_config.h"
#include "as_basetype.h"
#include "as_common.h"
#include "as.h"

#define InvalidFd -1
#define InvalidSocket -1
#define InvalidIp INADDR_ANY
#define Invalidport 0
#define DEFAULT_TCP_SENDRECV_SIZE (1024 * 1024)
#define DEFAULT_UDP_SENDRECV_SIZE (2 * 1024)

#define SendRecvError       -1//发送或接收错误
#define SendRecvErrorTIMEO  -2//发送或接收超时
#define SendRecvErrorEBADF  -3//socket句柄错误
#define SendRecvErrorEOF    -4//tcp断连

#define MAX_LISTEN_QUEUE_SIZE 2000
#define EPOLL_MAX_EVENT (MAX_LISTEN_QUEUE_SIZE + 1000)
#define MAX_EPOLL_FD_SIZE 3000
#define LINGER_WAIT_SECONDS 1 //LINGER等待时间(seconds)

//定义连接管理器得错误码
#if AS_APP_OS == AS_OS_LINUX
#define CONN_ERR_TIMEO      ETIMEDOUT
#define CONN_ERR_EBADF      EBADF
#elif AS_APP_OS == AS_OS_WIN32
#define CONN_ERR_EBADF      WSAEINTR
#define CONN_ERR_TIMEO      WSAETIMEDOUT
#endif

#if AS_APP_OS == AS_OS_LINUX
#define CLOSESOCK(x) ::close(x)
#define SOCK_OPT_TYPE void
#define CONN_ERRNO errno
#elif AS_APP_OS == AS_OS_WIN32
#define CLOSESOCK(x) closesocket(x)
#define socklen_t int
#define SOCK_OPT_TYPE char
#define CONN_ERRNO WSAGetLastError()
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EINPROGRESS WSAEINPROGRESS
#endif





#if AS_APP_OS == AS_OS_WIN32
enum tagSockEvent
{
    EPOLLIN  = 0x1,
    EPOLLOUT = 0x2
};

#ifndef INET_ADDRSTRLEN
#define  INET_ADDRSTRLEN 16
#endif

#ifndef MSG_WAITALL
#define MSG_WAITALL 0
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifndef socklen_t
typedef int socklen_t;
#endif

#endif //#if win32

class CNetworkAddr
{
  public:
    CNetworkAddr();
    virtual ~CNetworkAddr();
  public:
    long m_lIpAddr;
    USHORT m_usPort;
};

typedef enum tagConnStatus
{
    enIdle = 0,
    enConnecting,
    enConnected,
    enConnFailed,
    enClosed
} ConnStatus;

typedef enum tagEnumSyncAsync
{
    enSyncOp = 1,
    enAsyncOp = 0
} EnumSyncAsync;

typedef enum tagEpollEventType
{
    enEpollRead = 0,
    enEpollWrite = 1
} EpollEventType;

class CHandle;
class CHandleNode;
typedef std::list<CHandleNode *> ListOfHandle;
typedef ListOfHandle::iterator ListOfHandleIte;

class CHandle
{
  public:
    CHandle();
    virtual ~CHandle();

  public:
    virtual long initHandle(void);
    virtual void setHandleSend(AS_BOOLEAN bHandleSend);
    virtual void setHandleRecv(AS_BOOLEAN bHandleRecv);
    ULONG getEvents(void)
    {
        if(m_pMutexHandle != NULL)
        {
            (void)as_mutex_lock(m_pMutexHandle);
        }

        ULONG ulEvents = m_ulEvents;

        if(m_pMutexHandle != NULL)
        {
            (void)as_mutex_unlock(m_pMutexHandle);
        }
        return ulEvents;
    };
    virtual void close(void);

  public:
    long m_lSockFD;
    CHandleNode *m_pHandleNode;
    CNetworkAddr m_localAddr;

#if AS_APP_OS == AS_OS_WIN32
    AS_BOOLEAN m_bReadSelected;
    AS_BOOLEAN m_bWriteSelected;
#endif  //#if

#if AS_APP_OS == AS_OS_LINUX
    long m_lEpfd;
#endif  //#if
    ULONG m_ulEvents;
    as_mutex_t *m_pMutexHandle;
};

class CHandleNode
{
  public:
    CHandleNode()
    {
        m_pHandle = NULL;
        m_bRemoved = AS_FALSE;
    };

  public:
    CHandle *m_pHandle;
    AS_BOOLEAN m_bRemoved;
};

class CNetworkHandle : public CHandle
{
  public:
    CNetworkHandle();
    virtual ~CNetworkHandle(){};

  public:
    virtual long initHandle(void);
    long getSockFD(void) const    /*lint -e1714*///接口函数，本类不调用
    {
        return m_lSockFD;
    };
    void setSockFD(long lSockFD)
    {
        m_lSockFD = lSockFD;
    };
#if AS_APP_OS == AS_OS_LINUX
    long sendMsg(const struct msghdr *pMsg);
#endif
    virtual long recv(char *pArrayData, CNetworkAddr *pPeerAddr, const ULONG ulDataSize,
        const EnumSyncAsync bSyncRecv) = 0;

  public:
    virtual void handle_recv(void) = 0;
    virtual void handle_send(void) = 0;
};

class CTcpConnHandle : public CNetworkHandle
{
  public:
    CTcpConnHandle();
    virtual ~CTcpConnHandle();

  public:
    virtual long initHandle(void);
    virtual long conn( const CNetworkAddr *pLocalAddr, const CNetworkAddr *pPeerAddr,
        const EnumSyncAsync bSyncConn, ULONG ulTimeOut);
    virtual long send(const char *pArrayData, const ULONG ulDataSize,
        const EnumSyncAsync bSyncSend);
    virtual long recv(char *pArrayData, CNetworkAddr *pPeerAddr, const ULONG ulDataSize,
        const EnumSyncAsync bSyncRecv);
    virtual long recvWithTimeout(char *pArrayData, CNetworkAddr *pPeerAddr,
        const ULONG ulDataSize, const ULONG ulTimeOut, const ULONG ulSleepTime);
    virtual ConnStatus getStatus(void) const
    {
        return m_lConnStatus;
    };
    virtual void close(void);

  public:
    ConnStatus m_lConnStatus;
    CNetworkAddr m_peerAddr;
};

class CUdpSockHandle : public CNetworkHandle
{
  public:
    virtual long createSock(const CNetworkAddr *pLocalAddr,
                               const CNetworkAddr *pMultiAddr);
    virtual long send(const CNetworkAddr *pPeerAddr, const char *pArrayData,
         const ULONG ulDataSize, const EnumSyncAsync bSyncSend);
    virtual long recv(char *pArrayData, CNetworkAddr *pPeerAddr, const ULONG ulDataSize,
        const EnumSyncAsync bSyncRecv);
    virtual void close(void);
    virtual long recvWithTimeout(char *pArrayData,
                                        CNetworkAddr *pPeerAddr,
                                        const ULONG ulDataSize,
                                        const ULONG ulTimeOut,
                                        const ULONG ulSleepTime);
};

class CTcpServerHandle : public CHandle
{
  public:
    long listen(const CNetworkAddr *pLocalAddr);

  public:
    virtual long handle_accept(const CNetworkAddr *pRemoteAddr,
        CTcpConnHandle *&pTcpConnHandle) = 0;
    virtual void close(void);
};


#define MAX_HANDLE_MGR_TYPE_LEN 20
class  CHandleManager
{
  public:
    CHandleManager();
    virtual ~CHandleManager();

  public:
    long init(const ULONG ulMilSeconds);
    long run();
    void exit();

  public:
    long addHandle(CHandle *pHandle,
                      AS_BOOLEAN bIsListOfHandleLocked = AS_FALSE);
    void removeHandle(CHandle *pHandle);
    virtual void checkSelectResult(const EpollEventType enEpEvent,
        CHandle *pHandle) = 0;

  protected:
    static void *invoke(void *argc);
    void mainLoop();

  protected:
    ListOfHandle m_listHandle;
    as_mutex_t *m_pMutexListOfHandle;

#if AS_APP_OS == AS_OS_LINUX
    long m_lEpfd; //用于epoll的句柄
    struct epoll_event m_epEvents[EPOLL_MAX_EVENT];
#elif AS_APP_OS == AS_OS_WIN32
    fd_set m_readSet;
    fd_set m_writeSet;
    timeval m_stSelectPeriod;            //select周期
#endif

    ULONG m_ulSelectPeriod;
    as_thread_t *m_pSVSThread;
    AS_BOOLEAN m_bExit;
    char m_szMgrType[MAX_HANDLE_MGR_TYPE_LEN+1];
};

class CTcpConnMgr : public CHandleManager
{
  public:
    CTcpConnMgr()
    {
        (void)strncpy(m_szMgrType, "CTcpConnMgr", MAX_HANDLE_MGR_TYPE_LEN);
    };
    void lockListOfHandle();
    void unlockListOfHandle();

  protected:
    virtual void checkSelectResult(const EpollEventType enEpEvent,
                            CHandle *pHandle);  /*lint !e1768*///需要对外屏蔽该接口
};

class CUdpSockMgr : public CHandleManager
{
  public:
    CUdpSockMgr()
    {
        (void)strncpy(m_szMgrType, "CUdpSockMgr", MAX_HANDLE_MGR_TYPE_LEN);
    };

  protected:
    virtual void checkSelectResult(const EpollEventType enEpEvent,
        CHandle *pHandle);  /*lint !e1768*///需要对外屏蔽该接口
};

class CTcpServerMgr : public CHandleManager
{
  public:
    CTcpServerMgr()
    {
        m_pTcpConnMgr = NULL;
        (void)strncpy(m_szMgrType, "CTcpServerMgr", MAX_HANDLE_MGR_TYPE_LEN);
    };

  public:
    void setTcpClientMgr(CTcpConnMgr *pTcpConnMgr)
    {
        m_pTcpConnMgr = pTcpConnMgr;
    };

  protected:
    virtual void checkSelectResult(const EpollEventType enEpEvent,
        CHandle *pHandle);  /*lint !e1768*///需要对外屏蔽该接口

  protected:
    CTcpConnMgr *m_pTcpConnMgr;
};

#define DEFAULT_SELECT_PERIOD 20

// 4种日志操作
#define    CONN_OPERATOR_LOG    16
#define    CONN_RUN_LOG         17
#define    CONN_SECURITY_LOG    20
#define    CONN_USER_LOG        19


// 4种日志级别
enum CONN_LOG_LEVEL
{
    CONN_EMERGENCY = 0,
    CONN_ERROR = 3,
    CONN_WARNING = 4,
    CONN_DEBUG = 6
};

class IConnMgrLog
{
  public:
    virtual void writeLog(long lType, long llevel,
        const char *szLogDetail, const long lLogLen) = 0;
};

extern IConnMgrLog *g_pConnMgrLog;

class CConnMgr
{
  public:
    static CConnMgr& Instance()
    {
      static CConnMgr objConnMgr;
      return objConnMgr;
    }

    virtual ~CConnMgr();
protected:
    CConnMgr();
public:
    virtual long init(const ULONG ulSelectPeriod, const AS_BOOLEAN bHasUdpSock,
        const AS_BOOLEAN bHasTcpClient, const AS_BOOLEAN bHasTcpServer);
    virtual void setLogWriter(IConnMgrLog *pConnMgrLog) const
    {
        g_pConnMgrLog = pConnMgrLog;
    };
    virtual long run(void);
    virtual void exit(void);

public:
    virtual void setDefaultLocalAddr(const char *szLocalIpAddr);
    virtual long regTcpClient( const CNetworkAddr *pLocalAddr,
        const CNetworkAddr *pPeerAddr, CTcpConnHandle *pTcpConnHandle,
        const EnumSyncAsync bSyncConn, ULONG ulTimeOut);
    virtual void removeTcpClient(CTcpConnHandle *pTcpConnHandle);
    virtual long regTcpServer(const CNetworkAddr *pLocalAddr,
        CTcpServerHandle *pTcpServerHandle);
    virtual void removeTcpServer(CTcpServerHandle *pTcpServerHandle);
    virtual long regUdpSocket(const CNetworkAddr *pLocalAddr,
                                 CUdpSockHandle *pUdpSockHandle,
                                 const CNetworkAddr *pMultiAddr= NULL);
    virtual void removeUdpSocket(CUdpSockHandle *pUdpSockHandle);

  protected:
    long m_lLocalIpAddr;
    CTcpConnMgr *m_pTcpConnMgr;
    CUdpSockMgr *m_pUdpSockMgr;
    CTcpServerMgr *m_pTcpServerMgr;
};

#endif //CCONNMGR_H_INCLUDE

