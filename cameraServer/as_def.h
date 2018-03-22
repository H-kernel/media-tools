#ifndef __AS_MEDIA_DEFINE_H__
#define __AS_MEDIA_DEFINE_H__
#if (defined(__WIN32__) || defined(_WIN32)) && !defined(__MINGW32__)
#include "stdafx.h"
#include <winsock2.h>

extern "C" int initializeWinsockIfNecessary();
#else
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#define initializeWinsockIfNecessary() 1
#endif
#if defined(__WIN32__) || defined(_WIN32) || defined(_QNX4)
#else
#include <signal.h>
#define USE_SIGNALS 1
#endif
#include <stdint.h>
#include <stdio.h>
#include <signal.h>

typedef void*    AS_HANDLE;


enum AS_RTSP_DATA_TYPE {
    AS_RTSP_DATA_TYPE_VIDEO   = 0x00,
    AS_RTSP_DATA_TYPE_AUDIO   = 0x01,
    AS_RTSP_DATA_TYPE_OTHER   = 0x02,
};


#endif /*__AS_MEDIA_DEFINE_H__*/
