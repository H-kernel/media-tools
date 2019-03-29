#ifndef __AS_RTSP_CLIENT_H__
#define __AS_RTSP_CLIENT_H__

typedef void*    AS_HANDLE;

enum AS_RTSP_ERROR {
    AS_RTSP_ERROR_FAIL      = -1,
    AS_RTSP_ERROR_OK        = 0,
};


enum AS_RTSP_STATUS {
    AS_RTSP_STATUS_INIT       = 0x00,
    AS_RTSP_STATUS_SETUP      = 0x01,
    AS_RTSP_STATUS_PLAY       = 0x02,
    AS_RTSP_STATUS_PAUSE      = 0x03,
    AS_RTSP_STATUS_TEARDOWN   = 0x04,
    AS_RTSP_STATUS_BREAK      = 0x05,
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

class ASRtspClientHandle
{
public:
     ASRtspClientHandle();
     virtual ~ASRtspClientHandle();
     int32_t open(char* pszUrl);
     void    close();
public:
    virtual void handleStatus(AS_RTSP_STATUS eStatus) = 0;
    virtual int32_t allocMediaRecvBuf(AS_RTSP_DATA_TYPE eType,char*& pBuf,uint32_t& ulSize) = 0;
    virtual void handleMediaData(MediaFrameInfo* info,char* data,unsigned int size) = 0;
    
private:
    AS_HANDLE m_hRtspClient;
};

class ASRtspClientStack
{
public:
    static ASRtspClientStack& instance()
    {
        static ASRtspClientStack objASRtspClientStack;
        return objASRtspClientStack;
    }
    virtual ~ASRtspClientStack();
public:
    int32_t  init();
    void     release();
    void     setRecvBufSize(u_int32_t ulSize);
    uint32_t getRecvBufSize();
protected:
    ASRtspClientStack();
};
#endif /* __AS_RTSP_CLIENT_H__*/