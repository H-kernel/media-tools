#ifndef __LIB_AS_RTSP_CLINET_H__
#define __LIB_AS_RTSP_CLINET_H__
#ifdef WIN32
#ifdef LIBASRTSPCLIENT_EXPORTS
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
    /* init the rtsp client libary */
    AS_API int32_t   as_lib_init();
    /* release the rtsp client bibary */
    AS_API void      as_lib_release();
    /* set the socket recv buffer size*/
    AS_API void      as_lib_set_recv_buffer_size(uint32_t size);
    /* get the socket recv buffer size*/
    AS_API uint32_t  as_lib_get_recv_buffer_size();
    /* open a rtsp client handle */
    AS_API AS_HANDLE as_create_handle(char const* rtspURL,as_rtsp_callback_t* cb);
    /* destory a rtsp client handle */
    AS_API void      as_destory_handle(AS_HANDLE handle);
    /* get the rtsp client play range */
    AS_API double    as_get_play_duration(AS_HANDLE handle);
    /* seek the play */
    AS_API void      as_seek(AS_HANDLE handle,double start);
    /* pause the play */
    AS_API void      as_pause(AS_HANDLE handle);
    /* continue the play */
    AS_API void      as_continue(AS_HANDLE handle);
}
#endif /*__LIB_AS_RTSP_CLINET_H__*/
