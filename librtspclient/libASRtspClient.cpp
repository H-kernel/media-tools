#ifdef WIN32
#include "stdafx.h"
#endif
#include "libASRtspClient.h"
#include "as_rtsp_client.h"
/* init the rtsp client libary */
int32_t   as_lib_init(uint32_t model)
{
    return ASRtspClientManager::instance().init(model);
}
/* release the rtsp client bibary */
void      as_lib_release()
{
    ASRtspClientManager::instance().release();
}
/* set the socket recv buffer size*/
void      as_lib_set_recv_buffer_size(uint32_t size)
{
    ASRtspClientManager::instance().setRecvBufSize(size);
}
/* get the socket recv buffer size*/
uint32_t as_lib_get_recv_buffer_size()
{
    return ASRtspClientManager::instance().getRecvBufSize();
}
/* open a rtsp client handle */
AS_HANDLE as_create_handle(char const* rtspURL,as_rtsp_callback_t* cb)
{
    return ASRtspClientManager::instance().openURL(rtspURL,cb);
}
/* destory a rtsp client handle */
void      as_destory_handle(AS_HANDLE handle)
{
    ASRtspClientManager::instance().closeURL(handle);
}

/* get the rtsp client play range */
double      as_get_play_duration(AS_HANDLE handle)
{
    return ASRtspClientManager::instance().getDuration(handle);
}
/* seek the play */
void      as_seek(AS_HANDLE handle,double start)
{
    ASRtspClientManager::instance().seek(handle,start);
}
/* pause the play */
void      as_pause(AS_HANDLE handle)
{
    ASRtspClientManager::instance().pause(handle);
}
/* continue the play */
void      as_continue(AS_HANDLE handle)
{
    ASRtspClientManager::instance().play(handle);
}
/* run by the caller on the  single model */
void      as_run(AS_HANDLE handle,char* LoopWatchVar)
{
    ASRtspClientManager::instance().run(handle, LoopWatchVar);
}



