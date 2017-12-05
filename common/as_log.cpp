
/******************************************************************************
   版权所有 (C), 2008-2011, M.Kernel

 ******************************************************************************
  文件名          : ASLog.cpp
  版本号          : 1.0
  作者            :
  生成日期        : 2008-08-17
  最近修改        :
  功能描述        : AS日志模块
  函数列表        :
  修改历史        :
  1 日期          :
    作者          :
    修改内容      :
*******************************************************************************/


#include <stdio.h>
#include "as_ring_cache.h"
#include "as_log.h"
#include <time.h>
extern "C"{
#include "as_mutex.h"
#include "as_thread.h"
#include "as_event.h"
#include "as_common.h"
#include "as_time.h"
}

#ifdef WIN32
#include "atlbase.h"
#include "atlstr.h"
#endif


//extern HANDLE g_hDLLModule;                     //动态链接库句柄

/************************** Begin 日志对象实现 ********************************/

#define ASLOG_ERROR_OK             0       //成功
#define ASLOG_ERROR_INIT_CACHE     (-1)    //初始化缓冲区出错
#define ASLOG_ERROR_FILE_NAME      (-2)    //自动生成日志文件名出错
#define ASLOG_ERROR_OPEN_FILE      (-3)    //打开文件出错
#define ASLOG_ERROR_CREATE_EVENT   (-4)    //创建事件出错
#define ASLOG_ERROR_CREATE_THREAD  (-5)    //创建线程出错

//默认缓冲区大小1M
#define DEFAULT_CACHE_SIZE          (1024*1024)
//默认文件切换长度，单位Byte
#define DEFAULT_CHANGE_FILE_LEN     (10*1024*1024)
//默认最大文件切换长度，单位Byte
#define MAX_CHANGE_FILE_LEN         (100*1024*1024)
//默认最小文件切换长度，单位Byte
#define MIN_CHANGE_FILE_LEN         (100*1024)
//日志文件路径名长度
#define MAX_LOG_FILE_PATH_NAME_LEN  1024
//单条日志最大长度
#define MAX_LEN_OF_SINGLE_LOG       2048


//等待退出事件的总时间
#define LOG_WAIT_FOR_EXIT_EVENT     5000
//等待结束间隔
#define LOG_WAIT_FOR_EXIT_EVENT_INTERVAL 50
//等待写间隔
#define LOG_WAIT_FOR_WRITE_OVER_INTERVAL 10


class as_log
{
    private:    //单实例
        as_log();
    public:
        virtual ~as_log();

    public:
        static as_log* GetASLogInstance();    //获取日志对象
        static void DeleteASLogInstance();        //删除日志对象

    public:
        //启动日志
        long Start();
        //设置日志级别
        void SetLevel(long lLevel);
        //设置当前写的日志文件路径名
        bool SetLogFilePathName(const char* szPathName);
        //设置日志文件长度限制，超过此长度时生成新文件，参数单位KB
        void SetFileLengthLimit(unsigned long ulLimitLengthKB);
        //写日志
        long Write(const char* szFile, long lLine,
            long lLevel, const char* format, va_list argp);
        //停止日志
        long Stop();

    private:
        //读缓冲写文件线程
#if AS_APP_OS == AS_OS_WIN32
        //日志写线程入口
        static uint32_t __stdcall ThreadEntry(VOID* lpvoid);
#else
        static VOID* ThreadEntry(VOID* lpvoid);
#endif
        void WriteLogFileThread();

        const char* GetLevelString(long lLevel) const;

    private:
        //日志模块是否被启动
        bool    m_bRun;

        //是否允许写日志
        bool    m_bAllowWrite;

        //是否磁盘满导致日志停止
        bool    m_bDiskFull;
        //当检测到磁盘空间大于1M时，恢复日志打印
        #define MIN_DISK_SPACE_FOR_LOG      (1024*1024) //磁盘空间大于1M时恢复日志
        #define DISK_SPACE_CHECK_INTERVAL   10000       //磁盘空间检测间隔10秒钟
        //上次检测磁盘满的时间(仅在磁盘满导致日志停止时起作用)
        unsigned long   m_dwLastCheckDiskSpaceTime;

        //日志级别，默认为INFO级别
        long    m_lLevel;

        //写文件线程的句柄
        as_thread_t* m_hWriteThread;

        //写日志事件的句柄
        as_event_t*  m_hWriteEvent;

        //写线程退出事件的句柄
        as_event_t*    m_hThreadExitEvent;

        //日志缓冲
        as_ring_cache    m_Cache;

        //日志文件
        FILE*    m_pLogFile;

        //日志文件长度限制，单位Byte
        unsigned long    m_ulFileLenLimit;

        //日志文件路径和名称
        char    m_szLogFilePathName[MAX_LOG_FILE_PATH_NAME_LEN];

    private:
        //单实例日志对象
        static as_log*    g_pASLog;
};

as_log* as_log::g_pASLog = NULL;

as_log::as_log()
{
    m_bRun = false;
    m_bAllowWrite = false;
    m_bDiskFull = false;
    m_dwLastCheckDiskSpaceTime = 0;
    m_lLevel = AS_LOG_INFO;
    m_hWriteThread = NULL;
    m_hWriteEvent = NULL;
    m_hThreadExitEvent = NULL;

    m_pLogFile = NULL;
    m_ulFileLenLimit = DEFAULT_CHANGE_FILE_LEN;
    ::memset(m_szLogFilePathName,0,MAX_LOG_FILE_PATH_NAME_LEN);
}

as_log::~as_log()
{
    try
    {
        //停止日志模块
        (void)Stop();

        //以下是为过PC-LINT
        if(NULL != m_hWriteThread)
        {
            m_hWriteThread = NULL;
        }
        if(NULL != m_hWriteEvent)
        {
            as_destroy_event(m_hWriteEvent);
            m_hWriteEvent = NULL;
        }
        if(NULL != m_hThreadExitEvent)
        {
            as_destroy_event(m_hThreadExitEvent);
            m_hThreadExitEvent = NULL;
        }
        if(NULL != m_pLogFile)
        {
            (void)::fclose(m_pLogFile);
            m_pLogFile = NULL;
        }

    }
    catch(...)
    {
    }
}

//获取日志模块的唯一实例
as_log* as_log::GetASLogInstance()
{
    //若日志实例未生成则申请对象
    if(NULL == g_pASLog)
    {
        g_pASLog = new as_log;
    }

    return g_pASLog;
}

//删除日志模块的实例
void as_log::DeleteASLogInstance()
{
    //若已经申请对象则释放
    if(NULL == g_pASLog)
    {
        //必须要先停止对象
        (void)(g_pASLog->Stop());

        //删除对象
        delete g_pASLog;
        g_pASLog = NULL;
    }
}

//启动日志模块
long as_log::Start()
{
    //如果已经启动则直接返回
    if(m_bRun)
    {
        m_bAllowWrite = true;
        return ASLOG_ERROR_OK;
    }

    //初始化缓冲区为2M
    unsigned long ulCacheSize = m_Cache.SetCacheSize(DEFAULT_CACHE_SIZE);
    if(DEFAULT_CACHE_SIZE != ulCacheSize)
    {
        //缓冲区申请失败
        return ASLOG_ERROR_INIT_CACHE;
    }

    //检查文件名
    if(::strlen(m_szLogFilePathName) == 0)
    {
#if AS_APP_OS == AS_OS_WIN32
        //根据中间件模块句柄得到路径
        (void)::GetModuleFileName(NULL, CA2W(m_szLogFilePathName), MAX_LOG_FILE_PATH_NAME_LEN - 1);
        //    (void)::GetModuleFileName((HMODULE)g_hDLLModule, m_szLogFilePathName, MAX_LOG_FILE_PATH_NAME_LEN-1);
        char* pszFind = ::strrchr(m_szLogFilePathName, '.');
        if (NULL == pszFind)
        {
            //异常
            return ASLOG_ERROR_FILE_NAME;
        }
        //添加后缀
        (void)::sprintf(pszFind, ".log");
#elif AS_APP_OS == AS_OS_LINUX
        (void)::sprintf(m_szLogFilePathName, "AS_Module.log");
#endif
    }
    //打开文件
    m_pLogFile = ::fopen(m_szLogFilePathName, "a+");
    if(NULL == m_pLogFile)
    {
        //文件无法打开
        return ASLOG_ERROR_OPEN_FILE;
    }

    //初始化写事件
    m_hWriteEvent = as_create_event();
    if (NULL == m_hWriteEvent)
    {
        //写事件创建失败

        //关闭文件
        (void)::fclose(m_pLogFile);
        m_pLogFile = NULL;
        return ASLOG_ERROR_CREATE_EVENT;
    }

    //初始化写线程退出事件
    m_hThreadExitEvent = as_create_event();
    if (NULL == m_hThreadExitEvent)
    {
        //写线程退出事件创建失败

        //关闭文件
        (void)::fclose(m_pLogFile);
        m_pLogFile = NULL;

        //关闭写事件句柄
        as_destroy_event(m_hWriteEvent);
        m_hWriteEvent = NULL;
        return ASLOG_ERROR_CREATE_EVENT;
    }

    //设置日志启动标志
    m_bRun = true;

    //创建读缓冲写文件线程
    long lResult = as_create_thread(ThreadEntry, this, &m_hWriteThread, AS_DEFAULT_STACK_SIZE);
    if(NULL == m_hWriteThread)
    {
        //写线程创建失败，清理资源
        m_bRun = false;

        //关闭文件
        (void)::fclose(m_pLogFile);
        m_pLogFile = NULL;

        //关闭写事件句柄
        as_destroy_event(m_hWriteEvent);
        m_hWriteEvent = NULL;

        //关闭写线程退出事件句柄
        as_destroy_event(m_hThreadExitEvent);
        m_hThreadExitEvent = NULL;
        return ASLOG_ERROR_CREATE_THREAD;
    }

    //开始接收日志
    m_bAllowWrite = true;

    return ASLOG_ERROR_OK;
}
//设置日志级别
void as_log::SetLevel(long lLevel)
{
    switch(lLevel)
    {
        case AS_LOG_EMERGENCY:
        case AS_LOG_ALERT:
        case AS_LOG_CRITICAL:
        case AS_LOG_ERROR:
        case AS_LOG_WARNING:
        case AS_LOG_NOTICE:
        case AS_LOG_INFO:
        case AS_LOG_DEBUG:
            m_lLevel = lLevel;
            break;
        default:
            break;
    }
}

//设置当前写的日志文件路径名
bool as_log::SetLogFilePathName(const char* szPathName)
{
    bool bSetOk = false;
    //参数检查
    if((NULL!=szPathName) && ::strlen(szPathName)<MAX_LOG_FILE_PATH_NAME_LEN)
    {
        (void)::sprintf(m_szLogFilePathName, "%s", szPathName);
        //文件目录创建完成
        bSetOk = true;
    }

    return bSetOk;
}

//设置日志文件长度限制，超过此长度时生成新文件，参数单位KB
void as_log::SetFileLengthLimit(unsigned long ulLimitLengthKB)
{
    //KB转为Byte
    unsigned long ulNewLimitLength = ulLimitLengthKB * 1024;

    //范围确定
    if(ulNewLimitLength < MIN_CHANGE_FILE_LEN)
    {
        //小于最小值使用最小值
        m_ulFileLenLimit = MIN_CHANGE_FILE_LEN;
    }
    else if(ulNewLimitLength < MAX_CHANGE_FILE_LEN)
    {
        //正常范围
        m_ulFileLenLimit = ulNewLimitLength;
    }
    else
    {
        //大于最大值使用最大值
        m_ulFileLenLimit = MAX_CHANGE_FILE_LEN;
    }
}

//写一条日志到缓冲区，如下:
//2008-08-17 10:45:45.939[DEBUG|Log.cpp:152|PID:772|TID:2342|Err:0]程序启动...
long as_log::Write(const char* szFile, long lLine,
                long lLevel, const char* format, va_list argp)
{
    if(!m_bRun)
    {
        //未启动
        return 0;
    }

    //未允许写
    if(!m_bAllowWrite)
    {
        //磁盘空间达到恢复标准，恢复日志输入，清除磁盘满标志
        m_dwLastCheckDiskSpaceTime = 0;
        m_bDiskFull = false;
        m_bAllowWrite = true;

        //如果文件未打开，则要重新打开日志文件，写入新日志信息
        if(NULL == m_pLogFile)
        {
            m_pLogFile = ::fopen(m_szLogFilePathName, "a+");
            if(NULL == m_pLogFile)
            {
                //仍然有问题，恢复错误标记，下次再尝试打开
                m_bDiskFull = true;
                m_bAllowWrite = false;
                return 0;
            }
            //通知写线程写日志
            as_set_event(m_hWriteEvent);
        }
    }

    //日志级别限制
    if(lLevel > m_lLevel)
    {
        return 0;
    }

//额外信息准备
    //日志时间
    time_t ulTick = time(NULL);
    //日志级别
    const char* pszLevel = GetLevelString(lLevel);
    //文件名
    const char* pszFileName = ::strrchr(szFile, '\\');
    if(NULL != pszFileName)
    {
        //越过斜杠
        pszFileName += 1;
    }
    else
    {
        pszFileName = szFile;
    }

    //线程ID
    unsigned long ulThreadID = as_get_threadid();
    //当前错误码
#if AS_APP_OS == AS_OS_LINUX
    unsigned long ulErr = 0;
#elif AS_APP_OS == AS_OS_WIN32
    unsigned long ulErr = GetLastError();
#endif
//额外信息准备完成

    //不能使用成员变量，多线程会出现问题
    char szLogTmp [MAX_LEN_OF_SINGLE_LOG] = {0};
    char szLogTime[MAX_LEN_OF_SINGLE_LOG] = { 0 };
    as_strftime(szLogTime, MAX_LEN_OF_SINGLE_LOG, "%Y-%m-%d %H:%M:%S", ulTick);
    //首先向最终日志信息中加入额外信息
    (void)::sprintf(szLogTmp,"%s[%s|%20s:%05d|TID:0x%04X|Err:0x%04X] ",
        szLogTime, pszLevel, pszFileName, lLine, ulThreadID, ulErr);

    //将用户信息接在额外信息后写进日志中
    unsigned long ulLen = ::strlen(szLogTmp);
#if AS_APP_OS == AS_OS_LINUX
    (void)::vsnprintf(szLogTmp + ulLen, (MAX_LEN_OF_SINGLE_LOG - ulLen) - 1, format, argp);
#elif AS_APP_OS == AS_OS_WIN32
    (void)::_vsnprintf(szLogTmp + ulLen, (MAX_LEN_OF_SINGLE_LOG - ulLen) - 1, format, argp);
#endif
    szLogTmp[MAX_LEN_OF_SINGLE_LOG-1] = '\0';
    ulLen = ::strlen(szLogTmp);
    if((ulLen+2) < MAX_LEN_OF_SINGLE_LOG)
    {//自动增加一个换行
        szLogTmp[ulLen] = '\n';
        szLogTmp[ulLen+1] = '\0';
    }

    //将日志写进缓冲区
    unsigned long ulWriteLen = m_Cache.Write(szLogTmp, ::strlen(szLogTmp));
    while(0 == ulWriteLen)
    {
        //通知写线程写日志
        as_set_event(m_hWriteEvent);
        //等待数据写入缓冲
        as_sleep(LOG_WAIT_FOR_WRITE_OVER_INTERVAL);
        ulWriteLen = m_Cache.Write(szLogTmp, ::strlen(szLogTmp));
    }

    //通知写线程写日志
    as_set_event(m_hWriteEvent);
    return 0;
}

//停止日志，终止写线程
long as_log::Stop()
{
    //不接受写日志
    m_bAllowWrite = false;
    m_bDiskFull = false;
    m_dwLastCheckDiskSpaceTime = 0;

    //若已经停止或者未启动，直接退出
    if(!m_bRun)
    {
        return ASLOG_ERROR_OK;
    }

    //等缓冲中数据都写到文件中(等待5秒)
    long lWaitTime = LOG_WAIT_FOR_EXIT_EVENT;
    while(lWaitTime >= 0)
    {
        if(0 == m_Cache.GetDataSize())
        {
            //缓冲中日志已经都写到文件中了
            break;
        }

        //触发写事件
        as_set_event(m_hWriteEvent);

        as_sleep(LOG_WAIT_FOR_WRITE_OVER_INTERVAL);
        lWaitTime -= LOG_WAIT_FOR_WRITE_OVER_INTERVAL;
    }

    //设置日志标志为停止，触发写事件，让线程自己退出
    //等待5秒，若仍未退出，则强制中止
    m_bRun = false;
    lWaitTime = LOG_WAIT_FOR_EXIT_EVENT;
    while(lWaitTime >= 0)
    {
        //触发写事件，让线程自己退出
        as_set_event(m_hWriteEvent);

        if (AS_ERROR_CODE_TIMEOUT != as_wait_event(m_hThreadExitEvent, LOG_WAIT_FOR_EXIT_EVENT_INTERVAL))
        {
            //线程结束
            //begin delete for AQ1D01618 by xuxin
            //m_hThreadExitEvent = NULL; //下面有CloseHandle清理，这里不能置空
            //end delete for AQ1D01618 by xuxin
            break;
        }

        lWaitTime -= LOG_WAIT_FOR_EXIT_EVENT_INTERVAL;
    }

    if(NULL != m_hWriteThread)
    {
        //强行中止写线程
        (void)::as_thread_exit(m_hWriteThread);
        m_hWriteThread = NULL;
    }

    //清理写事件
    if(NULL != m_hWriteEvent)
    {
        (void)as_destroy_event(m_hWriteEvent);
        m_hWriteEvent = NULL;
    }

    //清理写线程退出事件
    if(NULL != m_hThreadExitEvent)
    {
        as_destroy_event(m_hThreadExitEvent);
        m_hThreadExitEvent = NULL;
    }

    //关闭文件
    if(NULL != m_pLogFile)
    {
        (void)::fclose(m_pLogFile);
        m_pLogFile = NULL;
    }

    //清空缓冲
    m_Cache.Clear();

    return ASLOG_ERROR_OK;
}

#if AS_APP_OS == AS_OS_WIN32
//日志写线程入口
uint32_t __stdcall as_log::ThreadEntry(VOID* lpvoid)
#else
VOID* as_log::ThreadEntry(VOID* lpvoid)
#endif
{
    if(NULL != lpvoid)
    {
        //调用写线程函数体
        as_log* pASLog = (as_log *)lpvoid;
        pASLog->WriteLogFileThread();
    }

    return 0;
}

//写日志线程
void as_log::WriteLogFileThread()
{
    //线程中变量准备
    char szNewFileName[MAX_LOG_FILE_PATH_NAME_LEN] = {0};
    unsigned long ulLogDataLen = 0;
    unsigned long ulCurFileLen = 0;
    char* pLogInfo = NULL;
    try
    {
        //申请读数据空间
        pLogInfo = new char[m_Cache.GetCacheSize()];
    }
    catch(...)
    {
    }
    if(NULL == pLogInfo)
    {
        //释放并赋空值
        as_thread_exit(m_hWriteThread);
        m_hWriteThread = NULL;

        //线程退出事件通知
        as_set_event(m_hThreadExitEvent);

        return ;
    }

    //启动读缓冲循环
    while(m_bRun)
    {
        //等待写文件事件
        as_wait_event(m_hWriteEvent, 0);

        //磁盘满文件未打开
        if(m_bDiskFull || NULL == m_pLogFile)
        {
            //可能是磁盘满，等待
            if(m_bDiskFull)
            {
                continue;
            }
            else
            {   //不是因为磁盘满造成的，退出
                break;
            }
        }

        //读缓冲
        ulLogDataLen = m_Cache.GetCacheSize();
        ulLogDataLen= m_Cache.Read(pLogInfo, ulLogDataLen);
        if(0 == ulLogDataLen)
        {
            //缓冲为空未读到数据
            continue;
        }

        //写数据到文件
        if(1 != fwrite(pLogInfo, ulLogDataLen, 1, m_pLogFile))
        {
#if AS_APP_OS == AS_OS_WIN32
            //写数据到文件中出现异常，若是磁盘满则暂停日志，等待空间
            if(ERROR_DISK_FULL == ::GetLastError())
            {
                m_bAllowWrite = false;
                m_bDiskFull = true;
                continue;
            }
            else
            {//其它错误则退出写日志
                break;
            }
#elif AS_APP_OS == AS_OS_LINUX
            break;
#endif
        }
        //将文件缓冲中数据写到硬盘
        if(fflush(m_pLogFile) != 0)
        {
#if AS_APP_OS == AS_OS_WIN32
            //写数据到硬盘中出现异常，若是磁盘满则暂停日志，等待空间
            if(ERROR_DISK_FULL == ::GetLastError())
            {
                m_bAllowWrite = false;
                m_bDiskFull = true;
                continue;
            }
            else
            {//其它错误则退出写日志
                break;
            }
#elif AS_APP_OS == AS_OS_LINUX
            break;
#endif
        }

        //检查文件长度是否超过限制
        ulCurFileLen = (unsigned long)::ftell(m_pLogFile);
        if(ulCurFileLen < m_ulFileLenLimit)
        {
            continue;
        }

        if(::strlen(m_szLogFilePathName) >= (MAX_LOG_FILE_PATH_NAME_LEN-20))
        {
            //文件名太长，无法修改文件名为备份文件名
            continue;
        }

        //关闭当前日志文件
        (void)::fclose(m_pLogFile);
        m_pLogFile = NULL;

        ULONG ulTick = as_get_ticks();

        //生成备份文件名
        (void)::sprintf(szNewFileName, "%s.%4d",m_szLogFilePathName, ulTick);

        //修改当前日志文件为备份文件，修改文件名失败也没有关系，可以再打开它继续往该文件中写
        (void)::rename(m_szLogFilePathName, szNewFileName);

        //重新打开日志文件
        m_pLogFile = ::fopen(m_szLogFilePathName, "a+");
        if(NULL != m_pLogFile)
        {
            //文件打开正常
            continue;
        }
#if AS_APP_OS == AS_OS_WIN32
        //新日志文件打开失败
        if(ERROR_DISK_FULL != ::GetLastError())
        {
            //异常，停止日志
            break;
        }
#endif
        //若是磁盘满则暂停日志，等待空间
        m_bAllowWrite = false;
        m_bDiskFull = true;
    }

    //恢复状态
    m_bRun = false;
    m_bAllowWrite = false;
    m_bDiskFull = false;
    m_dwLastCheckDiskSpaceTime = 0;

    //释放并赋空值
    as_thread_exit(m_hWriteThread);
    m_hWriteThread = NULL;

    //关闭当前日志文件
    if(NULL != m_pLogFile)
    {
        (void)::fclose(m_pLogFile);
        m_pLogFile = NULL;
    }

    //释放临时空间
   try
    {
        delete[] pLogInfo;
    }
    catch(...)
    {
    }
    pLogInfo = NULL;

    //线程退出事件通知
    as_set_event(m_hThreadExitEvent);
}

//获取日志级别字符串
const char* as_log::GetLevelString(long lLevel) const
{
    const char* pstrLevel = NULL;
    switch(lLevel)
    {
        case AS_LOG_EMERGENCY:
            pstrLevel = "EMERGENCY";
            break;
        case AS_LOG_ALERT:
            pstrLevel = "ALERT";
            break;
        case AS_LOG_CRITICAL:
            pstrLevel = "CRITICAL";
            break;
        case AS_LOG_ERROR:
            pstrLevel = "ERROR";
            break;
        case AS_LOG_WARNING:
            pstrLevel = "WARNING";
            break;
        case AS_LOG_NOTICE:
            pstrLevel = "NOTICE";
            break;
        case AS_LOG_INFO:
            pstrLevel = "INFO";
            break;
        case AS_LOG_DEBUG:
            pstrLevel = "DEBUG";
            break;
        default:
            pstrLevel = "NOLEVEL";
            break;
    }

    return pstrLevel;
}
/*************************** End 日志对象实现 *********************************/


/********************** Begin 日志模块用户接口实现 ****************************/
//启动AS日志模块
ASLOG_API void ASStartLog(void)
{
    //获取日志实例
    as_log* pASLog = as_log::GetASLogInstance();
    //获取日志实例
    (void)(pASLog->Start());
    va_list arg;
    va_end(arg);
    //写启动信息
    (void)(pASLog->Write(__FILE__, __LINE__,
        AS_LOG_INFO, "AS Log Module Start!", arg));
}

//写一条日志
ASLOG_API void __ASWriteLog(const char* szFileName, long lLine,
                      long lLevel, const char* format, va_list argp)
{
    //获取日志实例
    as_log* pASLog = as_log::GetASLogInstance();
    //写一条日志
    (void)(pASLog->Write(szFileName, lLine, lLevel, format, argp));
}

//停止AS日志模块
ASLOG_API void ASStopLog(void)
{
    //获取日志实例
    as_log* pASLog = as_log::GetASLogInstance();

    va_list arg;
    va_end(arg);
    //写停止信息
    (void)(pASLog->Write(__FILE__, __LINE__,
        AS_LOG_INFO, "AS Log Module Stop!\n\n\n\n", arg));
    //停止日志
    (void)(pASLog->Stop());
    //删除日志对象
    as_log::DeleteASLogInstance();
}

//设置日志级别
ASLOG_API void ASSetLogLevel(long lLevel)
{
    //获取日志实例
    as_log* pASLog = as_log::GetASLogInstance();
    //设置日志级别
    pASLog->SetLevel(lLevel);
}

//设置当前写的日志文件路径名(完整路径或相对路径)
ASLOG_API bool ASSetLogFilePathName(const char* szPathName)
{
    //获取日志实例
    as_log* pASLog = as_log::GetASLogInstance();
    //设置文件名
    bool bSetOk = pASLog->SetLogFilePathName(szPathName);
    return bSetOk;
}

//设置日志文件长度限制，超过此长度时生成新文件，单位KB(100K到100M之间,默认是10M)
ASLOG_API void ASSetLogFileLengthLimit(unsigned long ulLimitLengthKB)
{
    //获取日志实例
    as_log* pASLog = as_log::GetASLogInstance();
    //设置文件长度限制
    pASLog->SetFileLengthLimit(ulLimitLengthKB);
}
/************************ End 日志模块用户接口实现 ****************************/

