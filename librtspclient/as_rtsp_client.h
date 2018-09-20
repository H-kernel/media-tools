#ifndef __AS_RTSP_CLIENT_MANAGE_H__
#define __AS_RTSP_CLIENT_MANAGE_H__
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "as_def.h"
extern "C"{
#include "as_common.h"
}
//#ifndef _BASIC_USAGE_ENVIRONMENT0_HH
//#include "BasicUsageEnvironment0.hh"
//#endif


// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP True
#define REQUEST_STREAMING_OVER_UDP False

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

// Implementation of "ASStreamSink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#pragma pack(push,1)
typedef struct _SVS_MEDIA_FRAME_HEADER
{
    uint16_t          nWidth;
    uint16_t          nHeight;
    uint16_t          nVideoEncodeFormat;
    uint16_t          nMotionBlocks;

    uint8_t           nID[4];
    uint32_t          nVideoSize;
    uint32_t          nTimeTick;
    uint16_t          nAudioSize;
    uint8_t           bKeyFrame;
    uint8_t           bVolHead;

}
SVS_MEDIA_FRAME_HEADER, *PSVS_MEDIA_FRAME_HEADER;
#pragma pack(pop)

#define DUMMY_SINK_RECEIVE_BUFFER_SIZE /*(2*1024*1024)*/(512*1024)

#define DUMMY_SINK_H264_STARTCODE_SIZE 4

#define DUMMY_SINK_MEDIA_BUFFER_SIZE (DUMMY_SINK_RECEIVE_BUFFER_SIZE+DUMMY_SINK_H264_STARTCODE_SIZE + sizeof(SVS_MEDIA_FRAME_HEADER))


#define RTSP_SOCKET_RECV_BUFFER_SIZE_DEFAULT (1024*1024)


// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

#define RTSP_MANAGE_ENV_MAX_COUNT       4
#define RTSP_AGENT_NAME                 "all stream media"

#define RTSP_CLIENT_TIME               5000

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class ASRtspStreamState {
public:
  ASRtspStreamState();
  virtual ~ASRtspStreamState();

  void Start();
  void Stop();

public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};
class ASStreamReport
{
public:
    ASStreamReport(){};
    virtual ~ASStreamReport(){};

    virtual void report_stream(MediaFrameInfo* info, char* data, unsigned int size) = 0;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "ASRtspStreamState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "ASRtspStreamState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "ASRtspStreamState" field to the subclass:

class ASRtspClient : public RTSPClient, ASStreamReport {
public:
    static ASRtspClient* createNew(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                  int verbosityLevel = 0,
                  char const* applicationName = NULL,
                  portNumBits tunnelOverHTTPPortNum = 0);
protected:
    ASRtspClient(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
            int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
    virtual ~ASRtspClient();
public:
    int32_t open(as_rtsp_callback_t* cb);
    void    close();
    void    setMediaTcp(bool bTcp) { m_bTcp = bTcp; };
    double  getDuration();
    void    seek(double start);
    void    pause();
    void    play();
    u_int32_t index(){return m_ulEnvIndex;};
    void    report_status(int status);
    void    SupportsGetParameter(Boolean bSupportsGetParameter) {m_bSupportsGetParameter = bSupportsGetParameter;};
    Boolean SupportsGetParameter(){return m_bSupportsGetParameter;};
    as_rtsp_callback_t* get_cb(){return m_cb;};
    virtual void report_stream(MediaFrameInfo* info, char* data, unsigned int size);
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
    // send the hik key frame request
    void sendHikKeyFrame(MediaSession& session);
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
    ASRtspStreamState   scs;
private:
    u_int32_t           m_ulEnvIndex;
    as_rtsp_callback_t *m_cb;
    Boolean             m_bSupportsGetParameter;
    double              m_dStarttime;
    double              m_dEndTime;
    int                 m_curStatus;
    as_mutex_t         *m_mutex;
    uint32_t            m_bRunning;
    uint32_t            m_ulTryTime;
    bool                m_bTcp;
    time_t              m_lLastHeartBeat;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class ASStreamSink: public MediaSink {
public:
  static ASStreamSink* createNew(UsageEnvironment& env,
                  MediaSubsession& subsession, // identifies the kind of data that's being received
                  char const* streamId = NULL,
                  ASStreamReport* cb = NULL); // identifies the stream itself (optional)

  void Start();
  void Stop();

private:
    ASStreamSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId, ASStreamReport* cb);
    // called only by "createNew()"
public:
  virtual ~ASStreamSink();

  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
                struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
             struct timeval presentationTime, unsigned durationInMicroseconds);

private:
  // redefined virtual functions:
  virtual Boolean continuePlaying();

private:
  u_int8_t* fReceiveBuffer;
  u_int8_t  fMediaBuffer[DUMMY_SINK_MEDIA_BUFFER_SIZE];
  u_int32_t prefixSize;
  MediaSubsession& fSubsession;
  char* fStreamId;
  ASStreamReport     *m_StreamReport;
  MediaFrameInfo      m_MediaInfo;

  volatile bool m_bRunning;
};


class ASRtspClientManager
{
public:
    static ASRtspClientManager& instance()
    {
        static ASRtspClientManager objASRtspClientManager;
        return objASRtspClientManager;
    }
    virtual ~ASRtspClientManager();
public:
    // init the live Environment
    int32_t init(u_int32_t model);
    void    release();
    u_int32_t getRunModel();
    // The main streaming routine (for each "rtsp://" URL):
    AS_HANDLE openURL(char const* rtspURL, as_rtsp_callback_t* cb, bool bTcp);
    void      closeURL(AS_HANDLE handle);
    double    getDuration(AS_HANDLE handle);
    void      seek(AS_HANDLE handle,double start);
    void      pause(AS_HANDLE handle);
    void      play(AS_HANDLE handle);
    void      run(AS_HANDLE handle,char* LoopWatchVar);
    // option set function
    void      setRecvBufSize(u_int32_t ulSize);
    u_int32_t getRecvBufSize();
public:
    void rtsp_env_thread();

protected:
    ASRtspClientManager();
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
    u_int32_t         m_ulModel;
    u_int32_t         m_ulTdIndex;
    as_mutex_t       *m_mutex;
    char              m_LoopWatchVar;
    as_thread_t      *m_ThreadHandle[RTSP_MANAGE_ENV_MAX_COUNT];
    UsageEnvironment *m_envArray[RTSP_MANAGE_ENV_MAX_COUNT];
    u_int32_t         m_clCountArray[RTSP_MANAGE_ENV_MAX_COUNT];
    u_int32_t         m_ulRecvBufSize;
};
#endif /* __AS_RTSP_CLIENT_MANAGE_H__ */
