#ifndef __AS_RTSP_CLIENT_MANAGE_H__
#define __AS_RTSP_CLIENT_MANAGE_H__
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#ifdef WIN32
#include <time.h>
#include <Winsock2.h>
#include <http.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <event2/evhttp.h>
#else
#include <unistd.h>     //for getopt, fork
#include <sys/time.h>
#include "event2/http.h"
#include "event.h"
#include "event2/buffer.h"
#include "event2/keyvalq_struct.h"
#include "evhttp.h"
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#endif
#include <list>
#include <map>
#include "as_def.h"
#include "as.h"


//#ifndef _BASIC_USAGE_ENVIRONMENT0_HH
//#include "BasicUsageEnvironment0.hh"
//#endif
#define _AS_DEBUG_
#ifndef _AS_DEBUG_
#define RTSP2SIP_CONF_FILE "../conf/rtsp_guard.conf"
#define RTSP2SIP_LOG_FILE  "../logs/rtsp_guard.log"
#else
#define RTSP2SIP_CONF_FILE "E:\\build\\conf\\rtsp_guard.conf"
#define RTSP2SIP_LOG_FILE  "E:\\build\\logs\\rtsp_guard.log"
#endif
#define HTTP_SERVER_URI    "/check/req"

#define GW_SERVER_ADDR                 "0.0.0.0"
#define GW_SERVER_PORT_DEFAULT         8000
#define GW_SIP_PORT_DEFAULT            5060
#define GW_RTP_PORT_START              10000
#define GW_RTP_PORT_END                11000

#define HTTP_OPTION_TIMEOUT            864000
#define HTTP_REQUEST_MAX               4096

#define AC_MSS_PORT_DAFAULT            8080
#define AC_MSS_SIGN_TIME_LEN           16
#define AC_MSS_ERROR_CODE_OK           "00000000"

#define GW_TIMER_SCALE                 1000
#define GW_TIMER_CHECK_TASK            5000



#define GW_REPORT_DEFAULT              60

#define GW_PORT_PAIR_SIZE              4


#define XML_MSG_COMMAND_ADD        "add"
#define XML_MSG_COMMAND_REMOVE     "remove"


#define XML_MSG_NODE_USERNAME      "username"
#define XML_MSG_NODE_PASSWORD      "password"
#define XML_MSG_NODE_DOMAIN        "domain"
#define XML_MSG_NODE_REALM         "realm"
#define XML_MSG_NODE_CAMERAID      "cameraid"
#define XML_MSG_NODE_STREAMTYPE    "streamtype"

#define SIP_STATIC_INTER          60000
#define SIP_SESSION_EXPIRY        1800
#define SIP_LOCAL_IP_LENS          128
#define SIP_SDP_LENS_MAX          4096

#define ORTP_LOG_LENS_MAX        1024

#define MAX_RTP_PKT_LENGTH        1400

#define H264_RTP_TIMESTAMP_FREQUE 3600
#define G711_RTP_TIMESTAMP_FREQUE 400

#define RTSP_CLINET_HANDLE_MAX    200

#define RTSP_CLINET_RUN_DURATION  60




// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP True

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

// Implementation of "ASRtsp2SipStreamSink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE (512*1024)

#define DUMMY_SINK_H264_STARTCODE_SIZE 4

#define DUMMY_SINK_MEDIA_BUFFER_SIZE (DUMMY_SINK_RECEIVE_BUFFER_SIZE+DUMMY_SINK_H264_STARTCODE_SIZE)


#define RTSP_SOCKET_RECV_BUFFER_SIZE_DEFAULT (1024*1024)


// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

#define RTSP_MANAGE_ENV_MAX_COUNT       4

#define RTSP_AGENT_NAME                 "all stream media"


// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:
enum AS_RTSP_STATUS {
    AS_RTSP_STATUS_INIT = 0x00,
    AS_RTSP_STATUS_SETUP = 0x01,
    AS_RTSP_STATUS_PLAY = 0x02,
    AS_RTSP_STATUS_PAUSE = 0x03,
    AS_RTSP_STATUS_TEARDOWN = 0x04,
    AS_RTSP_STATUS_INVALID = 0xFF,
};

class ASRtspStatusObervser
{
public:
    ASRtspStatusObervser(){};
    virtual ~ASRtspStatusObervser(){};
    virtual void NotifyStatus(AS_RTSP_STATUS status) = 0;
};


class ASRtspCheckStreamState {
public:
  ASRtspCheckStreamState();
  virtual ~ASRtspCheckStreamState();
public:
  MediaSubsessionIterator* iter;
  MediaSession*            session;
  MediaSubsession*         subsession;
  TaskToken                streamTimerTask;
  double                   duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "ASRtsp2SipStreamState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "ASRtsp2SipStreamState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "ASRtsp2SipStreamState" field to the subclass:

class ASRtsp2RtpChannel: public RTSPClient {
public:
    static ASRtsp2RtpChannel* createNew(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                  int verbosityLevel = 0,
                  char const* applicationName = NULL,
                  portNumBits tunnelOverHTTPPortNum = 0);
protected:
    ASRtsp2RtpChannel(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
            int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
    virtual ~ASRtsp2RtpChannel();
public:
    int32_t open(uint32_t ulDuration,ASRtspStatusObervser* observer);
    void    close();
    void    play();
    u_int32_t index(){return m_ulEnvIndex;};
    void    SupportsGetParameter(Boolean bSupportsGetParameter) {m_bSupportsGetParameter = bSupportsGetParameter;};
    Boolean SupportsGetParameter(){return m_bSupportsGetParameter;};
public:
    void    handle_after_options(int resultCode, char* resultString);
    void    handle_after_describe(int resultCode, char* resultString);
    void    handle_after_setup(int resultCode, char* resultString);
    void    handle_after_play(int resultCode, char* resultString);
    void    handle_after_teardown(int resultCode, char* resultString);
    void    handle_subsession_after_playing(MediaSubsession* subsession);
    // Used to iterate through each stream's 'subsessions', setting up each one:
     void setupNextSubsession();
    // Used to shut down and close a stream (including its "RTSPClient" object):
    void shutdownStream(int exitCode = 1);
public:
    // RTSP 'response handlers':
    static void continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString);
    static void continueAfterTeardown(RTSPClient* rtspClient, int resultCode, char* resultString);

    // Other event handler functions:
    static void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
    static void subsessionByeHandler(void* clientData); // called when a RTCP "BYE" is received for a subsession
    static void streamTimerHandler(void* clientData);

public:
    ASRtsp2SipStreamState   scs;
private:
    u_int32_t             m_ulEnvIndex;
    Boolean               m_bSupportsGetParameter;
    std::string           m_strRtspSdp;
    AS_RTSP_STATUS        m_enStatus;
    ASRtspStatusObervser *m_bObervser;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class ASRtspCheckVideoSink: public MediaSink {
public:
  static ASRtspCheckVideoSink* createNew(UsageEnvironment& env, MediaSubsession& subsession); // identifies the stream itself (optional)

private:
  ASRtspCheckVideoSink(UsageEnvironment& env, MediaSubsession& subsession);
    // called only by "createNew()"
  virtual ~ASRtspCheckVideoSink();

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
  u_int8_t*        fReceiveBuffer;
  u_int8_t         fMediaBuffer[DUMMY_SINK_MEDIA_BUFFER_SIZE];
  u_int32_t        prefixSize;
  MediaSubsession& fSubsession;
  u_int32_t        m_rtpTimestampdiff;
  u_int32_t        m_lastTS;
};

class ASRtspCheckAudioSink: public MediaSink {
public:
  static ASRtspCheckAudioSink* createNew(UsageEnvironment& env, MediaSubsession& subsession); // identifies the stream itself (optional)

private:
  ASRtspCheckAudioSink(UsageEnvironment& env, MediaSubsession& subsession);
    // called only by "createNew()"
  virtual ~ASRtspCheckAudioSink();

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
  u_int8_t         fMediaBuffer[DUMMY_SINK_MEDIA_BUFFER_SIZE];
  MediaSubsession& fSubsession;
  u_int32_t        m_rtpTimestampdiff;
  u_int32_t        m_lastTS;
};


typedef enum AS_RTSP_CHECK_STATUS
{
    AS_RTSP_CHECK_STATUS_WAIT   = 0,
    AS_RTSP_CHECK_STATUS_RUN    = 1,
    AS_RTSP_CHECK_STATUS_END    = 2
}CHECK_STATUS;

class ASLensInfo:public ASRtspStatusObervser
{
public:
    ASLensInfo();
    virtual ~ASLensInfo();
    void setLensInfo(std::string& strCameraID,std::string& strStreamType);
    void check();
    CHECK_STATUS Status();
    virtual void NotifyStatus(AS_RTSP_STATUS status);
private:
    int32_t StartRtspCheck();
    void    stopRtspCheck();
private:
    std::string    m_strCameraID;
    std::string    m_strStreamType;
    AS_HANDLE      m_handle;
    CHECK_STATUS   m_Status;
    time_t         m_time;
};

typedef std::list<ASLensInfo*>    LENSINFOLIST;
typedef LENSINFOLIST::iterator    LENSINFOLISTITRT;



class ASRtspCheckTask
{
public:
    ASRtspCheckTask();
    virtual ~ASRtspCheckTask();
    void setTaskInfo(std::string& strCheckID,std::string& strReportUrl);
    void addCamera(std::string& strCameraID,std::string& strStreamTye);
    void checkTask();
    CHECK_STATUS TaskStatus();
private:
    void checkAllLensStatus();
    void ReportTaskStatus();
private:
    std::string   m_strCheckID;
    std::string   m_strReportUrl;
    LENSINFOLIST  m_LensList;
    CHECK_STATUS  m_Status;
};



typedef std::list<ASRtspCheckTask*>  ASCHECKTASKLIST;
typedef ASCHECKTASKLIST::iterator    ASCHECKTASKLISTITER;

class ASEvLiveHttpClient
{
public:
    ASEvLiveHttpClient();
    virtual ~ASEvLiveHttpClient();
    int32_t send_live_url_request(std::string& strCameraID,std::string& strStreamType,std::string& strRtspUrl);
    void    report_check_task_status(std::string& strUrl,ASRtspCheckTask* task);
public:
    void handle_remote_read(struct evhttp_request* remote_rsp);
    void handle_readchunk(struct evhttp_request* remote_rsp);
    void handle_remote_connection_close(struct evhttp_connection* connection);
private:
    static void remote_read_cb(struct evhttp_request* remote_rsp, void* arg);
    static void readchunk_cb(struct evhttp_request* remote_rsp, void* arg);
    static void remote_connection_close_cb(struct evhttp_connection* connection, void* arg);
private:
    int32_t open_http_by_url(std::string& strUrl);
    int32_t send_http_post_request(std::string& strMsg);
    int32_t send_http_get_request(std::string& strMsg);
private:
    struct evhttp_request   *m_pReq;
    struct event_base       *m_pBase;
    struct evhttp_connection*m_pConn;
    std::string              m_reqPath;
    std::string              m_strRespMsg;
};



class ASRtspGuardManager
{
public:
    static ASRtspGuardManager& instance()
    {
        static ASRtspGuardManager objASRtspGuardManager;
        return objASRtspGuardManager;
    }
    virtual ~ASRtspGuardManager();
public:
    // init the live Environment
    int32_t init();
    void    release();
    int32_t open();
    void    close();
    AS_HANDLE openURL(char const* rtspURL,ASRtspStatusObervser* observer);
    void      closeURL(AS_HANDLE handle);
    void      setRecvBufSize(u_int32_t ulSize);
    u_int32_t getRecvBufSize();
    std::string getAppID(){return m_strAppID;};
    std::string getAppSecret(){return m_strAppSecret;};
    std::string getAppKey(){return m_strAppKey;};
    std::string getLiveUrl(){return m_strLiveUrl;};
    uint32_t    getRtspHandleCount(){return m_ulRtspHandlCount;};
public:
    void http_env_thread();
    void rtsp_env_thread();
    void check_task_thread();
    u_int32_t find_beast_thread();
    UsageEnvironment* get_env(u_int32_t index);
    void releas_env(u_int32_t index);
public:
    void handle_http_req(struct evhttp_request *req);
protected:
    ASRtspGuardManager();
private:
    int32_t      read_system_conf();
    static void  http_callback(struct evhttp_request *req, void *arg);
private:
    static void *http_env_invoke(void *arg);
    static void *rtsp_env_invoke(void *arg);
    static void *check_task_invoke(void *arg);
    u_int32_t thread_index()
    {
        as_mutex_lock(m_mutex);
        u_int32_t index = m_ulTdIndex;
        m_ulTdIndex++;
        as_mutex_unlock(m_mutex);
        return index;
    }

private:
    int32_t handle_check(std::string &strReqMsg,std::string &strRespMsg);
    int32_t handle_check_task(const XMLElement *check);
    void    check_task_status();
private:
    u_int32_t         m_ulTdIndex;
    as_mutex_t       *m_mutex;
    char              m_LoopWatchVar;
    struct event_base*m_httpBase;
    struct evhttp    *m_httpServer;
    as_thread_t      *m_HttpThreadHandle;
    u_int32_t         m_httpListenPort;
    as_thread_t      *m_ThreadHandle[RTSP_MANAGE_ENV_MAX_COUNT];
    UsageEnvironment *m_envArray[RTSP_MANAGE_ENV_MAX_COUNT];
    u_int32_t         m_clCountArray[RTSP_MANAGE_ENV_MAX_COUNT];
    u_int32_t         m_ulRtspHandlCount;
    u_int32_t         m_ulRecvBufSize;
    u_int32_t         m_ulLogLM;
private:
    std::string       m_strAppID;
    std::string       m_strAppSecret;
    std::string       m_strAppKey;
    std::string       m_strLiveUrl;
    ASCHECKTASKLIST   m_TaskList;
};
#endif /* __AS_RTSP_CLIENT_MANAGE_H__ */
