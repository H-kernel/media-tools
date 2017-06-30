#include "libASRtspClient.h"
#include "as_common.h"
#include "as_rtsp_client.h"

/* init the rtsp client libary */
int32_t   as_lib_init()
{
    return ASRtspClientManager::instance().init();
}
/* release the rtsp client bibary */
void      as_lib_release()
{
    ASRtspClientManager::instance().release();
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


