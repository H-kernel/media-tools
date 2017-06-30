#ifndef __AS_MEDIA_DEFINE_H__
#define __AS_MEDIA_DEFINE_H__
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h> 


typedef void*    AS_HANDLE;

enum AS_RTSP_STATUS {
    AS_RTSP_STATUS_INIT       = 0x00,
    AS_RTSP_STATUS_SETUP      = 0x01,
    AS_RTSP_STATUS_PLAY       = 0x02,
    AS_RTSP_STATUS_PAUSE      = 0x03,
    AS_RTSP_STATUS_TEARDOWN   = 0x04,
    AS_RTSP_STATUS_INVALID    = 0xFF,
};

enum AS_RTSP_DATA_TYPE {
    AS_RTSP_DATA_TYPE_VIDEO   = 0x00,
    AS_RTSP_DATA_TYPE_AUDIO   = 0x01,
    AS_RTSP_DATA_TYPE_OTHER   = 0x02,
};

typedef struct _tagMediaFrameInfo
{
    AS_RTSP_DATA_TYPE type;                   /* media data type */
    unsigned char     rtpPayloadFormat;       /* media rtp playload */
    uint32_t          rtpTimestampFrequency;  /* media rtp timestamp Frequency*/
    struct timeval    presentationTime;       /* media presentation Time */
    char             *codecName;              /* codec Name */
    char const       *protocolName;           /* protocol Name */
    uint16_t          videoWidth;             /* Video width */
    uint16_t          videoHeight;            /* Video Height */
    uint32_t          videoFPS;               /* Video Frame per second */
    uint32_t          numChannels;            /* Aduio Channels */
}MediaFrameInfo;


typedef void (*rtsp_status_callback)(AS_HANDLE handle,int status,void* ctx);
typedef void (*rtsp_data_callback)(MediaFrameInfo* info,char* data,unsigned int size,void* ctx);
typedef void (*log_callback)(const char* log);

typedef struct  {
    rtsp_status_callback f_status_cb;   /*status callback function*/
    rtsp_data_callback   f_data_cb;     /*data callback function*/
    void                *ctx;           /*user data*/
}as_rtsp_callback_t;

#endif /*__AS_MEDIA_DEFINE_H__*/
