#ifndef __AS_RTSP_CLIENT_MANAGE_H__
#define __AS_RTSP_CLIENT_MANAGE_H__
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "as_def.h"
extern "C"{
#include "as_common.h"
#include <librtmp/rtmp.h>
#include <librtmp/log.h>
#include <librtmp/http.h>
#include <librtmp/amf.h>
}
//#ifndef _BASIC_USAGE_ENVIRONMENT0_HH
//#include "BasicUsageEnvironment0.hh"
//#endif


// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP True
#define REQUEST_STREAMING_OVER_UDP False

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

// Implementation of "ASRtsp2RtmpStreamSink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:

#define DUMMY_SINK_RECEIVE_BUFFER_SIZE /*(2*1024*1024)*/(512*1024)

#define DUMMY_SINK_H264_STARTCODE_SIZE 4

//定义包头长度，RTMP_MAX_HEADER_SIZE=18
#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket)+RTMP_MAX_HEADER_SIZE)


#define DUMMY_SINK_MEDIA_BUFFER_SIZE (RTMP_HEAD_SIZE + DUMMY_SINK_RECEIVE_BUFFER_SIZE+DUMMY_SINK_H264_STARTCODE_SIZE)


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

typedef struct _RTMPMetadata
{
    // video, must be h264 type
    unsigned int    nWidth;
    unsigned int    nHeight;
    unsigned int    nFrameRate;        // fps
    unsigned int    nVideoDataRate;    // bps
    unsigned int    nvideocodecid;
    unsigned int    nSpsLen;
    unsigned char   Sps[H264_PPS_SPS_FRAME_LEN_MAX];
    unsigned int    nPpsLen;
    unsigned char   Pps[H264_PPS_SPS_FRAME_LEN_MAX];

    // audio, must be aac type
    bool            bHasAudio;
    unsigned int    nAudioDatarate;
    unsigned int    nAudioSampleRate;
    unsigned int    nAudioSampleSize;
    int             nAudioFmt;
    unsigned int    nAudioChannels;
    char            pAudioSpecCfg;
    unsigned int    nAudioSpecCfgLen;

} RTMPMetadata,*LPRTMPMetadata;

#define FILEBUFSIZE (1024 * 1024 * 10) //  10M

enum
{
    FLV_CODECID_H264 = 7,
    FLV_CODECID_H265 = 12,
};


typedef enum RTSP2RTMP_PAYLOAD_TYPE
{
    RTSP2RTMP_PAYLOAD_TYPE_H264      = 0x01,
    RTSP2RTMP_PAYLOAD_TYPE_H265      = 0x02
}PAYLOAD_TYPE;



class RTMPStream
{
public:
    RTMPStream(void);
    virtual ~RTMPStream(void);
public:
    bool Connect(const char* url);
    void Close();
    bool SendMetadata(LPRTMPMetadata lpMetaData);
    bool SendH264Packet(unsigned char *data,unsigned int size,bool bIsKeyFrame,unsigned int nTimeStamp);
    bool SendAACPacket(unsigned char* data,unsigned int size,unsigned int nTimeStamp );
private:
    int InitSockets();
    void CleanupSockets();
    char * put_byte( char *output, uint8_t nVal ) ;
    char * put_be16(char *output, uint16_t nVal ) ;
    char * put_be24(char *output,uint32_t nVal )  ;
    char * put_be32(char *output, uint32_t nVal ) ;
    char * put_be64( char *output, uint64_t nVal );
    char * put_amf_string( char *c, const char *str );
    char * put_amf_double( char *c, double d );
    // ????
    int SendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp);
private:

    RTMP* m_pRtmp;

};


// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class ASRtsp2RtmpStreamState {
public:
  ASRtsp2RtmpStreamState();
  virtual ~ASRtsp2RtmpStreamState();

  void Start();
  void Stop();

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
    void tryReqeust();
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
    uint32_t            m_ulTryTime;
    bool                m_bTcp;
    time_t              m_lLastHeartBeat;
private:
    RTMPStream          m_RtmpStream;
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

class ASRtsp2RtmpStreamSink: public MediaSink {
public:
  static ASRtsp2RtmpStreamSink* createNew(UsageEnvironment& env,
                  MediaSubsession& subsession, // identifies the kind of data that's being received
                  RTMPStream* pRtmpStream,
                  char const* streamId = NULL); // identifies the stream itself (optional)

  void Start();
  void Stop();

private:
    ASRtsp2RtmpStreamSink(UsageEnvironment& env, MediaSubsession& subsession,RTMPStream* pRtmpStream, char const* streamIdb);
    // called only by "createNew()"
public:
  virtual ~ASRtsp2RtmpStreamSink();

  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
                struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
             struct timeval presentationTime, unsigned durationInMicroseconds);

private:
  // redefined virtual functions:
  virtual Boolean continuePlaying();

  // send the H264 frame
  void sendH264Frame(unsigned frameSize, unsigned numTruncatedBytes,
             struct timeval presentationTime, unsigned durationInMicroseconds);
  void sendH264KeyFrame(unsigned frameSize, unsigned int nTimeStamp);
  // send the H265 frame
  void sendH265Frame();
private:
  u_int8_t* fReceiveBuffer;
  u_int8_t  fMediaBuffer[DUMMY_SINK_MEDIA_BUFFER_SIZE];
  u_int32_t prefixSize;
  MediaSubsession& fSubsession;
  char* fStreamId;
  RTMPStream*         m_pRtmpStream;
  RTMPMetadata        m_stMetadata;
  bool                m_bWaitFirstKeyFrame;
  volatile bool       m_bRunning;
};


class ASRtsp2RtmpClientManager
{
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
public:
    void rtsp_env_thread();

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
};
#endif /* __AS_RTSP_CLIENT_MANAGE_H__ */
