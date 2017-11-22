/******************************************************************************
   版权所有 (C), 2001-2011, M.Kernel

 ******************************************************************************
  文件名          : as_timer.h
  版本号          : 1.0
  作者            :
  生成日期        : 2007-4-02
  最近修改        :
  功能描述        :
  函数列表        :
  修改历史        :
  1 日期          : 2007-4-02
    作者          : hexin
    修改内容      : 生成
*******************************************************************************/

#ifndef CTIMER_H_INCLUDE
#define CTIMER_H_INCLUDE

#ifdef WIN32
#pragma warning(disable: 4786)
#endif

#include <list>
#include <map>
extern "C"{
#include "as_config.h"
#include "as_basetype.h"
#include "as_common.h"
}

const ULONG DefaultTimerScale = 100; //缺省定时精度为100ms
const ULONG MinTimerScale = 1; //定时精度最小为1ms

class ITrigger;
class CTimerItem;

class CCmpTimeOut
{
  public:
    bool operator()(const ULONGLONG ullTimerOut1, const ULONGLONG ullTimerOut2) const
    {
        return (ullTimerOut1 < ullTimerOut2);
    };
};
typedef std::multimap<ULONGLONG, CTimerItem *, CCmpTimeOut> ListOfTrigger;
typedef std::pair<ULONGLONG const, CTimerItem *> ListOfTriggerPair;
typedef ListOfTrigger::iterator ListOfTriggerIte;

typedef enum tagTriggerStyle
{
    enOneShot = 0,
    enRepeated = 1
} TriggerStyle;

class ITrigger
{
  public:
    ITrigger()
    {
        m_pTimerItem = NULL;
    };
    virtual ~ITrigger(){};

  public:
    virtual void onTrigger(void *pArg, ULONGLONG ullScales, TriggerStyle enStyle) = 0;

  public:
    CTimerItem *m_pTimerItem;
};

class CTimerItem
{
  public:
    CTimerItem()
    {
        m_pTrigger = NULL;
        m_pArg = NULL;
        m_bRemoved = AS_FALSE;
    };

  public:
    ITrigger *m_pTrigger;
    void *m_pArg;
    ULONG m_ulInitialScales;
    ULONGLONG m_ullCurScales;
    TriggerStyle m_enStyle;
    AS_BOOLEAN m_bRemoved;
};

// 4种日志操作
#define    TIMER_OPERATOR_LOG    16
#define    TIMER_RUN_LOG         17
#define    TIMER_SECURITY_LOG    20
#define    TIMER_USER_LOG        19

// 4种日志级别
enum TIMER_LOG_LEVEL
{
    TIMER_EMERGENCY = 0,
    TIMER_ERROR = 3,
    TIMER_WARNING = 4,
    TIMER_DEBUG = 7
};

//日志打印接口
class ITimerLog
{
public:
    virtual void writeLog(long iType, long ilevel,
        const char *szLogDetail, const long iLogLen) = 0;
};

extern ITimerLog *g_pTimerLog;

class as_timer
{
public:
    static as_timer& instance()
    {
        static as_timer objASTimer;
        return objASTimer;
    }

    virtual ~as_timer();

public:
    virtual long init(ULONG ulTimerScale);
    void setLogWriter(ITimerLog *pTimerLog)
    {
        g_pTimerLog = pTimerLog;
    };
    virtual long run();
    void exit();

public:
     virtual long registerTimer(ITrigger *pRrsTrigger, void *pArg, ULONG nScales,
        TriggerStyle enStyle);
     virtual long cancelTimer(ITrigger *pRrsTrigger);

    void clearTimer( );
protected:
    as_timer();
private:
    static void *invoke(void *argc)
    {
        as_timer *pTimer = (as_timer *)argc;
        pTimer->mainLoop();
        as_thread_exit(NULL);
        return NULL;
    };

    void mainLoop();

private:
    ULONG m_ulTimerScale;
    ULONGLONG m_ullRrsAbsTimeScales;
    ListOfTrigger *m_plistTrigger;
    as_mutex_t *m_pMutexListOfTrigger;
    as_thread_t *m_pASThread;
    volatile AS_BOOLEAN m_bExit;
};


#endif //CTIMER_H_INCLUDE


