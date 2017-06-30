#include<stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include "../as_def.h"
#include "../libASRtspClient.h"

static void status_callback(AS_HANDLE handle,int status,void* ctx)
{
    printf("handle rtsp status report,status:[%d]\n",status);
}

static void data_callback(MediaFrameInfo* info,char* data,unsigned int size,void* ctx)
{
	printf("handle Media data size:[%d],codecName:%s,rtpPayloadFormat:%d,rtpTimestampFrequency:%d\n",size,info->codecName,info->rtpPayloadFormat,info->rtpTimestampFrequency);
}
int main(int agrc,char* agrv[])
{
    as_rtsp_callback_t cb;
	cb.f_status_cb = status_callback;
	cb.f_data_cb   = data_callback;
	cb.ctx         = NULL;
    char* pUrl      = "rtsp://121.42.227.54:554/pag://121.42.227.54:7302:11111111001310000001:1:MAIN:TCP?cnid=1&pnid=1&auth=50&streamform=rtp";
	as_lib_init();

    AS_HANDLE handle = as_create_handle(pUrl,&cb);

	if(NULL == handle)
	{
		printf("create the resp handle fail\n");
		return -1;
	}
    

	for(int i = 0;i< 100000;i++)
	{
		sleep(10);
	}

	as_destory_handle(handle);
	as_lib_release();
}
