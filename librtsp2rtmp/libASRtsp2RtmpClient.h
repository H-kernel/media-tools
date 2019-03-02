#ifndef __LIB_AS_RTSP_CLINET_H__
#define __LIB_AS_RTSP_CLINET_H__
#ifdef WIN32
#ifdef LIBASRTSP2RTMPCLIENT_EXPORTS
#define AS_API __declspec(dllexport)
#else
#define AS_API __declspec(dllimport)
#endif
#else
#define AS_API
#endif
#include "as_def.h"

extern "C"
{
    /* init the rtsp2rtmp client libary */
    AS_API int32_t   as_rtsp2rtmp_init();
    /* release the rtsp2rtmp client libary */
    AS_API void      as_rtsp2rtmp_release();
    /* set the socket recv buffer size*/
    AS_API void      as_rtsp2rtmp_set_recv_buffer_size(uint32_t size);
    /* get the socket recv buffer size*/
    AS_API uint32_t  as_rtsp2rtmp_get_recv_buffer_size();
    /* open a rtsp2rtmp client handle */
    AS_API AS_HANDLE as_rtsp2rtmp_create_handle(char const* rtspURL,char const* rtmpURL, bool bTcp);
    /* destory a rtsp2rtmp client handle */
    AS_API void      as_rtsp2rtmp_destory_handle(AS_HANDLE handle);
    /* get a rtsp2rtmp client handle status */
    AS_API uint32_t  as_rtsp2rtmp_get_handle_status(AS_HANDLE handle);
    /* set the log call back */
    AS_API void      as_rtsp2rtmp_set_log_callback(uint32_t nLevel,Rtsp2Rtmp_LogCallback cb);
}
#endif /*__LIB_AS_RTSP_CLINET_H__*/
