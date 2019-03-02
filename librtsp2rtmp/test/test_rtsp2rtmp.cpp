#include<stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include "../as_def.h"
#include "../libASRtsp2RtmpClient.h"

static void log_callbck(int32_t level, const char *fmt, va_list args)
{
    vprintf(fmt,args);
    printf("\n");
}

int main(int agrc,char* agrv[])
{
    char* prtspUrl = "rtsp://119.3.79.46:554/live/32010000000000001501?streamtype=0&devtype=3&starttime=no&endtime=no&timestamp=20181029140614&timeout=30&encrypt=f752dfcd32cafa0d6831a944c237e688";
    char* prtmpUrl = "rtmp://118.190.44.21:1935/live/hx_test";
    int nLevel = AS_RTSP2RTMP_LOGDEBUG;
    as_rtsp2rtmp_set_log_callback(nLevel,log_callbck);
    as_rtsp2rtmp_init();
    

    

    AS_HANDLE handle = as_rtsp2rtmp_create_handle(prtspUrl,prtmpUrl,true);

    if(NULL == handle)
    {
        printf("create the resp handle fail\n");
        return -1;
    }

    uint32_t ulStatus = 0;
    for(int i = 0;i< 100000;i++)
    {
        ulStatus = as_rtsp2rtmp_get_handle_status(handle);
        printf("handle status:[%d]\n",ulStatus);
        sleep(10);
    }

    as_rtsp2rtmp_destory_handle(handle);
    as_rtsp2rtmp_release();
}
