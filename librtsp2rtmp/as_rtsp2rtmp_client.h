#ifndef __AS_RTSP2RTMP_CLIENT_MANAGE_H__
#define __AS_RTSP2RTMP_CLIENT_MANAGE_H__
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "as_def.h"
#include "as.h"
#include "srs_librtmp.h"
#include "EasyAACEncoderAPI.h"

//#ifndef _BASIC_USAGE_ENVIRONMENT0_HH
//#include "BasicUsageEnvironment0.hh"
//#endif


// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP True
#define REQUEST_STREAMING_OVER_UDP False

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

// Implementation of "ASRtsp2RtmpMediaSink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:




#define DUMMY_SINK_H264_STARTCODE_SIZE 4

#define DUMMY_SINK_VIDEO_RECEIVE_BUFFER_SIZE (1024*1024)
#define DUMMY_SINK_AUDIO_RECEIVE_BUFFER_SIZE 1024


#define RTSP_SOCKET_RECV_BUFFER_SIZE_DEFAULT (1024*1024)

#define H264_PPS_SPS_FRAME_LEN_MAX  1024


// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

#define RTSP_MANAGE_ENV_MAX_COUNT       4
#define RTSP_AGENT_NAME                 "all stream push"

#define RTSP_CLIENT_TIME               5000

typedef enum
{
    H264_NALU_TYPE_UNDEFINED    =0,
    H264_NALU_TYPE_IDR          =5,
    H264_NALU_TYPE_SEI          =6,
    H264_NALU_TYPE_SPS          =7,
    H264_NALU_TYPE_PPS          =8,
    H264_NALU_TYPE_STAP_A       =24,
    H264_NALU_TYPE_STAP_B       =25,
    H264_NALU_TYPE_MTAP16       =26,
    H264_NALU_TYPE_MTAP24       =27,
    H264_NALU_TYPE_FU_A         =28,
    H264_NALU_TYPE_FU_B         =29,
    H264_NALU_TYPE_END
}H264_NALU_TYPE;

typedef struct
{
    //byte 0
    uint8_t TYPE:5;
    uint8_t NRI:2;
    uint8_t F:1;
}H264_FU_INDICATOR; /**//* 1 BYTES */

// NALU
typedef struct _NaluUnit
{
    int type;
    int size;
    unsigned char *data;
}NaluUnit;

#define FILEBUFSIZE (1024 * 1024 * 10) //  10M

typedef enum en_RTMP_CODECID
{
    RTMP_CODECID_H264 = 1,
    RTMP_CODECID_H265 = 2,
    RTMP_CODECID_AAC  = 3,
}RTMP_CODECID;


class ASRtmpHandle: public CNetworkHandle
{
public:
    ASRtmpHandle();
    virtual ~ASRtmpHandle();
    int32_t open(const char* pszUrl);
    void    close();
    int32_t sendH264Frame(char* frames, int frames_size, uint32_t dts, uint32_t pts);
    int32_t sendAacFrame(char sound_format, char sound_rate,char sound_size, char sound_type, 
                         char* frame, int frame_size, uint32_t timestamp);
public:
    virtual long recv(char *pArrayData, CNetworkAddr *pPeerAddr, const ULONG ulDataSize,const EnumSyncAsync bSyncRecv);
    virtual void handle_recv(void);
    virtual void handle_send(void);
private:
    srs_rtmp_t   m_srsRtmpHandle;
    long         m_lSockFD;
};

class ASRtmpConnMgr : public CHandleManager
{
  public:
    ASRtmpConnMgr()
    {
        (void)strncpy(m_szMgrType, "ASRtmpConnMgr", MAX_HANDLE_MGR_TYPE_LEN);
    };
    void lockListOfHandle();
    void unlockListOfHandle();

  protected:
    virtual void checkSelectResult(const EpollEventType enEpEvent,CHandle *pHandle);
};

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class ASRtsp2RtmpStreamState {
public:
  ASRtsp2RtmpStreamState();
  virtual ~ASRtsp2RtmpStreamState();

  void Start();
  void Stop();
  bool Check();

public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};
// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "ASRtsp2RtmpStreamState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "ASRtsp2RtmpStreamState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "ASRtsp2RtmpStreamState" field to the subclass:

class ASRtsp2RtmpClient : public RTSPClient {
public:
    static ASRtsp2RtmpClient* createNew(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                  int verbosityLevel = 0,
                  char const* applicationName = NULL,
                  portNumBits tunnelOverHTTPPortNum = 0);
protected:
    ASRtsp2RtmpClient(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
            int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
    virtual ~ASRtsp2RtmpClient();
public:
    int32_t open(char const* rtmpURL);
    void    close();
    u_int32_t getStatus();
    void    setMediaTcp(bool bTcp) { m_bTcp = bTcp; };
    u_int32_t index(){return m_ulEnvIndex;};
    void    report_status(int status);
    void    SupportsGetParameter(Boolean bSupportsGetParameter) {m_bSupportsGetParameter = bSupportsGetParameter;};
    Boolean SupportsGetParameter(){return m_bSupportsGetParameter;};
public:
    void handleAfterOPTIONS(int resultCode, char* resultString);
    void handleAfterDESCRIBE(int resultCode, char* resultString);
    void handleAfterSETUP(int resultCode, char* resultString);
    void handleAfterPLAY(int resultCode, char* resultString);
    void handleAfterGET_PARAMETE(int resultCode, char* resultString);
    void handleAfterPause(int resultCode, char* resultString);
    void handleAfterSeek(int resultCode, char* resultString);
    void handleAfterTeardown(int resultCode, char* resultString);
    void handleHeartBeatOption(int resultCode, char* resultString);
    void handleHeartGET_PARAMETE(int resultCode, char* resultString);

    // Other event handler functions:
    void handlesubsessionAfterPlaying(MediaSubsession* subsession); // called when a stream's subsession (e.g., audio or video substream) ends
    void handlesubsessionByeHandler(MediaSubsession* subsession); // called when a RTCP "BYE" is received for a subsession
    void handlestreamTimerHandler();

    // Used to iterate through each stream's 'subsessions', setting up each one:
    void setupNextSubsession();
private:
    // Used to shut down and close a stream (including its "RTSPClient" object):
    void shutdownStream();
    //
    void StopClient();
    bool checkStop();
public:
    // RTSP 'response handlers':
    static void continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterPause(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterSeek(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterTeardown(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterHearBeatOption(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterHearBeatGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString);
    // Other event handler functions:
    static void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
    static void subsessionByeHandler(void* clientData); // called when a RTCP "BYE" is received for a subsession
    static void streamTimerHandler(void* clientData);
public:
    ASRtsp2RtmpStreamState   scs;
private:
    u_int32_t           m_ulEnvIndex;
    Boolean             m_bSupportsGetParameter;
    double              m_dStarttime;
    double              m_dEndTime;
    int                 m_curStatus;
    as_mutex_t         *m_mutex;
    uint32_t            m_bRunning;
    bool                m_bTcp;
    time_t              m_lLastHeartBeat;
private:
    ASRtmpHandle        m_hRtmpHandle;
    char               *m_rtmpUlr;
    char               *app;
    char               *conn;
    char               *subscribe;
    char               *playpath;
    char               *tcurl;
    char               *flashver;
    char               *swfurl;
    char               *swfverify;
    char               *pageurl;
    char               *client_buffer_time;
    int                 live;
    char               *temp_filename;
    int                 buffer_size;
};



// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class ASRtsp2RtmpMediaSink: public MediaSink {
public:
    static ASRtsp2RtmpMediaSink* createNew(UsageEnvironment& env,
                  MediaSubsession& subsession, // identifies the kind of data that's being received
                  ASRtmpHandle* rtmpHandle,
                  char const* streamId = NULL); // identifies the stream itself (optional)

    void Start();
    void Stop();
    bool Check();

private:
    ASRtsp2RtmpMediaSink(UsageEnvironment& env, MediaSubsession& subsession,ASRtmpHandle* rtmpHandle, char const* streamIdb);
    // called only by "createNew()"
public:
    virtual ~ASRtsp2RtmpMediaSink();

    static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
                                struct timeval presentationTime,
                                unsigned durationInMicroseconds);
    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                           struct timeval presentationTime,
                           unsigned durationInMicroseconds);

private:
    // redefined virtual functions:
    virtual Boolean continuePlaying();

    // send the H264 frame
    void sendH264Frame(unsigned frameSize, unsigned numTruncatedBytes,
             struct timeval presentationTime, unsigned durationInMicroseconds);
    // send the H265 frame
    void sendH265Frame();
    // send the Audio Fram
    void sendAudioFrame(unsigned frameSize, unsigned numTruncatedBytes,
             struct timeval presentationTime, unsigned durationInMicroseconds);
private:
    u_int8_t*             fMediaBuffer;
    u_int8_t*             fReceiveBuffer;
    u_int32_t             ulRecvBufLens;
    u_int32_t             prefixSize;
    MediaSubsession&      fSubsession;
    char*                 fStreamId;
    ASRtmpHandle         *m_rtmpHandle;
    RTMP_CODECID          m_enVideoID;
    volatile bool         m_bRunning;
    EasyAACEncoder_Handle m_hAacEncoder;
    InitParam             m_hEncoderParam;
    u_int8_t*             m_pAacEncodeBuf;
};


class Rtsp2RtmpEnvironment: public BasicUsageEnvironment {
public:
  static Rtsp2RtmpEnvironment* createNew(TaskScheduler& taskScheduler);

  virtual UsageEnvironment& operator<<(char const* str);
  virtual UsageEnvironment& operator<<(int i);
  virtual UsageEnvironment& operator<<(unsigned u);
  virtual UsageEnvironment& operator<<(double d);
  virtual UsageEnvironment& operator<<(void* p);

protected:
  Rtsp2RtmpEnvironment(TaskScheduler& taskScheduler);
      // called only by "createNew()" (or subclass constructors)
  virtual ~Rtsp2RtmpEnvironment();
};



class ASRtsp2RtmpClientManager {
public:
    static ASRtsp2RtmpClientManager& instance()
    {
        static ASRtsp2RtmpClientManager objASRtsp2RtmpClientManager;
        return objASRtsp2RtmpClientManager;
    }
    virtual ~ASRtsp2RtmpClientManager();
public:
    // init the live Environment
    int32_t init();
    void    release();
    AS_HANDLE openURL(char const* rtspURL,char const* rtmpURL, bool bTcp);
    void      closeURL(AS_HANDLE handle);
    u_int32_t getStatus(AS_HANDLE handle);
    void      setRecvBufSize(u_int32_t ulSize);
    u_int32_t getRecvBufSize();
    void      setLogCallBack(uint32_t nLevel,Rtsp2Rtmp_LogCallback cb);
public:
    void rtsp_env_thread();
    void rtsp_write_log(const char *fmt, ...);
    void write_log(int32_t level, const char *fmt, va_list args);
    int32_t reg_rtmp_handle_actor(ASRtmpHandle* pHandle);
    void    unreg_rtmp_handle_actor(ASRtmpHandle* pHandle);
protected:
    ASRtsp2RtmpClientManager();
private:
    static void *rtsp_env_invoke(void *arg);
    u_int32_t thread_index()
    {
        as_mutex_lock(m_mutex);
        u_int32_t index = m_ulTdIndex;
        m_ulTdIndex++;
        as_mutex_unlock(m_mutex);
        return index;
    }
    u_int32_t find_beast_thread();
private:
    u_int32_t         m_ulTdIndex;
    as_mutex_t       *m_mutex;
    char              m_LoopWatchVar;
    as_thread_t      *m_ThreadHandle[RTSP_MANAGE_ENV_MAX_COUNT];
    UsageEnvironment *m_envArray[RTSP_MANAGE_ENV_MAX_COUNT];
    u_int32_t         m_clCountArray[RTSP_MANAGE_ENV_MAX_COUNT];
    u_int32_t         m_ulRecvBufSize;
    Rtsp2Rtmp_LogCallback m_LogCb;
    int32_t           m_nLogLevel;
private:
    ASRtmpConnMgr     m_TcpConnMgr;
};
#endif /* __AS_RTSP2RTMP_CLIENT_MANAGE_H__ */
