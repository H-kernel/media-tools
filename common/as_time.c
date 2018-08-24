#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */
#include "as_config.h"
#include <time.h>
#include <errno.h>
#include "as_time.h"

#if AS_APP_OS == AS_OS_WIN32
#include <WinSock2.h>
#include <Mmsystem.h>
#endif

static uint32_t g_ulSysStart = 0 ;

static struct timeval g_startTime;

void  as_start_ticks()
{
    if(g_ulSysStart )
    {
        return ;
    }

    g_ulSysStart = 1 ;

    memset(&g_startTime,0x00,sizeof(g_startTime));

#ifdef WIN32
    g_startTime.tv_sec = (long)timeGetTime();
#else
    gettimeofday(&g_startTime, NULL);
#endif

    return ;
}


uint32_t as_get_ticks ()
{
    ULONG ticks = 0 ;

#ifdef WIN32
    ticks = timeGetTime()/1000;
#else
    struct timeval now;
    gettimeofday(&now, AS_NULL);
    ticks=now.tv_sec;
#endif

    return( ticks );
}

uint32_t as_get_cur_msecond()
{
    ULONG ticks = 0;

#ifdef WIN32
    ticks = timeGetTime();
#else
    struct timeval now;
    gettimeofday(&now, AS_NULL);
    ticks = now.tv_sec*1000+now.tv_usec/1000;
#endif

    return(ticks);
}

void  as_delay (uint32_t ulDelayTimeMs)
{
    LONG was_error;

    struct timeval tv;

    ULONG then, now, elapsed;

    then = as_get_ticks();

    do
    {
        errno = 0;
        /* Calculate the time LONGerval left (in case of LONGerrupt) */
        now = as_get_ticks();
        elapsed = (now-then);
        then = now;
        if ( elapsed >= ulDelayTimeMs )
        {
            break;
        }

        ulDelayTimeMs -= elapsed;
        tv.tv_sec = (long)(ulDelayTimeMs/1000);
        tv.tv_usec = (ulDelayTimeMs%1000)*1000;

        was_error = select(0, AS_NULL, AS_NULL, AS_NULL, &tv);

    } while ( was_error && (errno == EINTR) );
}

/*1000 = 1second*/
void  as_sleep(uint32_t ulMs )
{
#if AS_APP_OS == AS_OS_LINUX
    as_delay( ulMs );
#elif AS_APP_OS == AS_OS_WIN32
    Sleep(ulMs);
#endif

return ;
}

void  as_strftime(char * pszTimeStr, unsigned long ulLens, char* pszFormat, time_t ulTime)
{
    struct tm* tmv;
    time_t uTime = (time_t)ulTime;
    tmv = (struct tm*)localtime(&uTime);

    strftime(pszTimeStr, ulLens, pszFormat, tmv);
    return;
}

struct tm* as_Localtime(time_t* ulTime)
{
    return (struct tm*)localtime(ulTime);/*0~6代表 周日到周六*/
}


//形如20040629182030的时间串转换成以秒为单位的日历时间,
//即自国际标准时间公元1970年1月1日00:00:00以来经过的秒数。
time_t as_str2time(const char *pStr)
{
    struct tm tmvalue;

    (void)memset(&tmvalue, 0, sizeof(tmvalue));

    const char *pch = pStr;
    char tmpstr[8];
    memcpy(tmpstr, pch, 4);
    tmpstr[4] = '\0';
    tmvalue.tm_year = atoi(tmpstr) - 1900;
    pch += 4;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_mon = atoi(tmpstr) - 1;
    pch += 2;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_mday = atoi(tmpstr);
    pch += 2;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_hour = atoi(tmpstr);
    pch += 2;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_min = atoi(tmpstr);
    pch += 2;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_sec = atoi(tmpstr);

    return mktime(&tmvalue);
}


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */



