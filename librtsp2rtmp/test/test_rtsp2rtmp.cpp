#include<stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include "../as_def.h"
#include "../libASRtsp2RtmpClient.h"

int main(int agrc,char* agrv[])
{
    char* prtspUrl = "rtsp://121.42.227.54:554/pag://121.42.227.54:7302:11111111001310000001:1:MAIN:TCP?cnid=1&pnid=1&auth=50&streamform=rtp";
    char* prtmpUrl = "rtsp://121.42.227.54:554/pag://121.42.227.54:7302:11111111001310000001:1:MAIN:TCP?cnid=1&pnid=1&auth=50&streamform=rtp";
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
