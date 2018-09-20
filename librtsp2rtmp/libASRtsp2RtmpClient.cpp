#ifdef WIN32
#include "stdafx.h"
#endif
#include "libASRtsp2RtmpClient.h"
#include "as_rtsp2rtmp_client.h"
/* init the rtsp2rtmp client libary */
int32_t   as_rtsp2rtmp_init()
{
    return ASRtsp2RtmpClientManager::instance().init();
}
/* release the rtsp2rtmp client bibary */
void      as_rtsp2rtmp_release()
{
    ASRtsp2RtmpClientManager::instance().release();
}

/* set the socket recv buffer size*/
void      as_rtsp2rtmp_set_recv_buffer_size(uint32_t size)
{
    ASRtsp2RtmpClientManager::instance().setRecvBufSize(size);
}
/* get the socket recv buffer size*/
uint32_t as_rtsp2rtmp_get_recv_buffer_size()
{
    return ASRtsp2RtmpClientManager::instance().getRecvBufSize();
}
/* open a rtsp2rtmp client handle */
AS_HANDLE as_rtsp2rtmp_create_handle(char const* rtspURL,char const* rtmpURL, as_rtsp_callback_t* cb, bool bTcp)
{
    return ASRtsp2RtmpClientManager::instance().openURL(rtspURL, rtmpURL,cb, bTcp);
}

/* destory a rtsp2rtmp client handle */
void      as_rtsp2rtmp_destory_handle(AS_HANDLE handle)
{
    ASRtsp2RtmpClientManager::instance().closeURL(handle);
}


