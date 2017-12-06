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
extern "C"{
#include <ortp/ortp.h>
#include <eXosip2/eXosip.h>
}
#include "as_def.h"
#include "as.h"


//#ifndef _BASIC_USAGE_ENVIRONMENT0_HH
//#include "BasicUsageEnvironment0.hh"
//#endif
#define _AS_DEBUG_
#ifndef _AS_DEBUG_
#define RTSP2SIP_CONF_FILE "../conf/rtsp2sip_gw.conf"
#define RTSP2SIP_LOG_FILE  "../logs/rtsp2sip_gw.log"
#else
#define RTSP2SIP_CONF_FILE "E:\\build\\conf\\rtsp2sip_gw.conf"
#define RTSP2SIP_LOG_FILE  "E:\\build\\logs\\rtsp2sip_gw.log"
#endif
#define HTTP_SERVER_URI    "/session/req"

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
#define GW_TIMER_SIPSESSION            5

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

class CRtpPortPair
{
public:
    CRtpPortPair(){};
    virtual ~CRtpPortPair(){};
    void init(unsigned short usVRtpPort,unsigned short usVRtcpPort,
              unsigned short usARtpPort,unsigned short usARtcpPort)
              {
                m_usVideoRtpPort = usVRtpPort;
                m_usVideoRtcpPort = usVRtcpPort;
                m_usAudioRtpPort = usARtpPort;
                m_usAudioRtcpPort = usARtcpPort;
              };

    unsigned short getVRtpPort(){ return m_usVideoRtpPort; };
    unsigned short getVRtcpPort(){ return m_usVideoRtcpPort; };
    unsigned short getARtpPort(){ return m_usAudioRtpPort; };
    unsigned short getARtcpPort(){ return m_usAudioRtcpPort; };
private:
    unsigned short m_usVideoRtpPort;
    unsigned short m_usVideoRtcpPort;
    unsigned short m_usAudioRtpPort;
    unsigned short m_usAudioRtcpPort;
};

typedef std::list<CRtpPortPair*> RTPPORTPAIRLIST;
typedef std::map<int, CRtpPortPair*> CALLPORTPAIREMAP;

class CRtpDestinations
{
public:
    CRtpDestinations()
    {
        m_bSet = false;
    };
    virtual ~CRtpDestinations(){};
    void init(std::string &strVideoSeverAddr,unsigned short usVideoSeverPort,
             std::string  &strAudioSeverAddr,unsigned short usAudioSeverPort)
    {
        m_strVideoSeverAddr = strVideoSeverAddr;
        m_usVideoSeverPort  = usVideoSeverPort;
        m_strAudioSeverAddr = strAudioSeverAddr;
        m_usAudioSeverPort  = usAudioSeverPort;
        m_bSet = true;
    };

    bool bSet(){return m_bSet;};
    std::string ServerVideoAddr(){return m_strVideoSeverAddr;};
    unsigned short ServerVideoPort(){return m_usVideoSeverPort;};
    std::string ServerAudioAddr(){return m_strAudioSeverAddr;};
    unsigned short ServerAudioPort(){return m_usAudioSeverPort;};
private:
    bool             m_bSet;
    std::string      m_strVideoSeverAddr;
    unsigned short   m_usVideoSeverPort;
    std::string      m_strAudioSeverAddr;
    unsigned short   m_usAudioSeverPort;
};



// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:


class ASRtsp2SipStreamState {
public:
  ASRtsp2SipStreamState();
  virtual ~ASRtsp2SipStreamState();
  void open(CRtpPortPair* local_ports);
public:
  MediaSubsessionIterator* iter;
  MediaSession*            session;
  MediaSubsession*         subsession;
  TaskToken                streamTimerTask;
  double                   duration;
  Groupsock*               m_VideoRtpGp;
  Groupsock*               m_VideoRtcpGp;
  Groupsock*               m_AudioRtpGp;
  Groupsock*               m_AudioRtcpGp;
  RTCPInstance*            m_VideoRTCPInstance;
  RTCPInstance*            m_AudioRTCPInstance;
};

class IRtspChannelObserver
{
public:
    IRtspChannelObserver(){};
    virtual ~IRtspChannelObserver(){};
    virtual void OnOptions(int nCallId) = 0;
    virtual void OnDescribe(int nCallId,int nTransID,std::string& sdp) = 0;
    virtual void OnSetUp(int nCallId,int nTransID,CRtpPortPair* local_ports) = 0;
    virtual void OnPlay(int nCallId) = 0;
    virtual void OnTearDown(int nCallId) = 0;
};
// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "ASRtsp2SipStreamState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "ASRtsp2SipStreamState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "ASRtsp2SipStreamState" field to the subclass:
enum AS_RTSP_STATUS {
    AS_RTSP_STATUS_INIT = 0x00,
    AS_RTSP_STATUS_SETUP = 0x01,
    AS_RTSP_STATUS_PLAY = 0x02,
    AS_RTSP_STATUS_PAUSE = 0x03,
    AS_RTSP_STATUS_TEARDOWN = 0x04,
    AS_RTSP_STATUS_INVALID = 0xFF,
};
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
    int32_t open(int nCallId,int nTransID,CRtpPortPair* local_ports,IRtspChannelObserver* pObserver = NULL);
    void    close();
    void    play();
    u_int32_t index(){return m_ulEnvIndex;};
    void    SupportsGetParameter(Boolean bSupportsGetParameter) {m_bSupportsGetParameter = bSupportsGetParameter;};
    Boolean SupportsGetParameter(){return m_bSupportsGetParameter;};
    void    SetDestination(CRtpDestinations& des);
public:
    void    handle_after_options(int resultCode, char* resultString);
    void    handle_after_describe(int resultCode, char* resultString);
    void    handle_after_setup(int resultCode, char* resultString);
    void    handle_after_play(int resultCode, char* resultString);
    void    handle_after_teardown(int resultCode, char* resultString);
    void    handle_subsession_after_playing(MediaSubsession* subsession);
    RTPSink*  createNewRTPSink(MediaSubsession& subsession,Groupsock* rtpGroupsock);
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
    IRtspChannelObserver* m_pObserver;
    int                   m_nCallId;
    int                   m_nTransID;
    CRtpPortPair*         m_LocalPorts;
    CRtpDestinations      m_DestinInfo;
    AS_RTSP_STATUS        m_enStatus;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class ASRtsp2SipVideoSink: public MediaSink {
public:
  static ASRtsp2SipVideoSink* createNew(UsageEnvironment& env, MediaSubsession& subsession,
                                  CRtpPortPair* local_ports,CRtpDestinations* des); // identifies the stream itself (optional)

private:
  ASRtsp2SipVideoSink(UsageEnvironment& env, MediaSubsession& subsession,
                                  CRtpPortPair* local_ports,CRtpDestinations* des);
    // called only by "createNew()"
  virtual ~ASRtsp2SipVideoSink();

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
  RtpSession*      m_pVideoSession;
  u_int32_t        m_rtpTimestampdiff;
  u_int32_t        m_lastTS;
};

class ASRtsp2SipAudioSink: public MediaSink {
public:
  static ASRtsp2SipAudioSink* createNew(UsageEnvironment& env, MediaSubsession& subsession,
                                  CRtpPortPair* local_ports,CRtpDestinations* des); // identifies the stream itself (optional)

private:
  ASRtsp2SipAudioSink(UsageEnvironment& env, MediaSubsession& subsession,
                                  CRtpPortPair* local_ports,CRtpDestinations* des);
    // called only by "createNew()"
  virtual ~ASRtsp2SipAudioSink();

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
  RtpSession*      m_pAudioSession;
  u_int32_t        m_rtpTimestampdiff;
  u_int32_t        m_lastTS;
};

enum SIP_SESSION_STATUS
{
    SIP_SESSION_STATUS_ADD = 0, /* the session is add */
    SIP_SESSION_STATUS_REG = 1, /*registing*/
    SIP_SESSION_STATUS_RUNING = 2, /*online or wait for call */
    SIP_SESSION_STATUS_REMOVE = 3, /*the session need remove */
};

class ASEvLiveHttpClient
{
public:
    ASEvLiveHttpClient();
    virtual ~ASEvLiveHttpClient();
    int32_t send_live_url_request(std::string& strCameraID,std::string& strStreamType,std::string& strRtspUrl);
    void    report_sip_session_status(std::string& strUrl,std::string& strSessionID,SIP_SESSION_STATUS enStatus);
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




typedef std::map<int,ASRtsp2RtpChannel*>  CALLRTSPCHANNELMAP;
typedef std::map<int,std::string>         TRANSSDPMAP;

class CSipSession:public IRtspChannelObserver
{
public:
    CSipSession();
    virtual ~CSipSession();
    void Init(std::string &strSessionID);
    void SetSipRegInfo(bool bRegister,std::string &strUsername,std::string &strPasswd,std::string &strDomain,std::string &strRealM);
    void SetCameraInfo(std::string &strCameraID,std::string &strStreamType);
    SIP_SESSION_STATUS SessionStatus();
    void SessionStatus(SIP_SESSION_STATUS enStatus);
    int  RegID(){return m_nRegID;};
    void RegID(int nRegID){m_nRegID = nRegID;};
    bool  bRegister(){return m_bRegister;};
    std::string UserName(){return m_strUsername;};
    std::string Password(){return m_strPasswd;};
    std::string Domain(){return m_strDomain;};
    std::string RealM(){return m_strRealM;};
    u_int32_t  RepInterval(){return m_ulRepInterval;};
    void RepInterval(u_int32_t ulRepInterval){m_ulRepInterval = ulRepInterval;};
    void ReportUrl(std::string& strReportUrl){m_strReportUrl = strReportUrl;};
    void SendStatusReport();
public:
    int32_t handle_invite(int nCallId,int nTransID,CRtpPortPair* local_ports,sdp_message_t *remote_sdp = NULL);
    int32_t handle_bye(int nCallId);
    void    handle_ack(int nCallId,sdp_message_t    *remote_sdp = NULL);
    void    close_all();
public:
    virtual void OnOptions(int nCallId);
    virtual void OnDescribe(int nCallId,int nTransID,std::string& sdp);
    virtual void OnSetUp(int nCallId,int nTransID,CRtpPortPair* local_ports);
    virtual void OnPlay(int nCallId);
    virtual void OnTearDown(int nCallId);
private:
    as_mutex_t         *m_mutex;
    SIP_SESSION_STATUS  m_enStatus;
    bool                m_bRegister;
    int                 m_nRegID;
    std::string         m_strSessionID;
    std::string         m_strUsername;
    std::string         m_strPasswd;
    std::string         m_strDomain;
    std::string         m_strRealM;
    std::string         m_strCameraID;
    std::string         m_strStreamType;
    u_int32_t           m_ulRepInterval;
    std::string         m_strReportUrl;
    CALLRTSPCHANNELMAP  m_callRtspMap;
    TRANSSDPMAP         m_callRtspSdpMap;
};

class CSipSessionTimer:public ITrigger
{
public:
    CSipSessionTimer(){};
    virtual ~CSipSessionTimer(){};
    virtual void onTrigger(void *pArg, ULONGLONG ullScales, TriggerStyle enStyle);
};

typedef std::map<std::string, CSipSession*> SIPSESSIONMAP;
typedef std::map<int, CSipSession*> REGSESSIONMAP;


class ASRtsp2SiptManager
{
public:
    static ASRtsp2SiptManager& instance()
    {
        static ASRtsp2SiptManager objASRtsp2SiptManager;
        return objASRtsp2SiptManager;
    }
    virtual ~ASRtsp2SiptManager();
public:
    // init the live Environment
    int32_t init();
    void    release();
    int32_t open();
    void    close();;
    void      setRecvBufSize(u_int32_t ulSize);
    u_int32_t getRecvBufSize();
    std::string getAppID(){return m_strAppID;};
    std::string getAppSecret(){return m_strAppSecret;};
    std::string getAppKey(){return m_strAppKey;};
    std::string getLiveUrl(){return m_strLiveUrl;};
public:
    void http_env_thread();
    void sip_env_thread();
    void rtsp_env_thread();
    u_int32_t find_beast_thread();
    UsageEnvironment* get_env(u_int32_t index);
    void releas_env(u_int32_t index);
public:
    void handle_http_req(struct evhttp_request *req);
    void check_all_sip_session();
    void send_invit_200_ok(int nTransID,CRtpPortPair*local_ports,std::string& strSdp);
    static void ortp_log_callback(OrtpLogLevel lev, const char *fmt, va_list args);
    static void osip_trace_log_callback(char *fi, int li, osip_trace_level_t level, char *chfr, va_list ap);
protected:
    ASRtsp2SiptManager();
private:
    int32_t      read_system_conf();
    static void  http_callback(struct evhttp_request *req, void *arg);
private:
    static void *http_env_invoke(void *arg);
    static void *sip_env_invoke(void *arg);
    static void *rtsp_env_invoke(void *arg);
    u_int32_t thread_index()
    {
        as_mutex_lock(m_mutex);
        u_int32_t index = m_ulTdIndex;
        m_ulTdIndex++;
        as_mutex_unlock(m_mutex);
        return index;
    }

private:
    int32_t handle_session(std::string &strReqMsg,std::string &strRespMsg);
    int32_t handle_add_session(const XMLElement *session);
    void    handle_remove_session(std::string &strSessionID);
    void    send_sip_regsiter(CSipSession* pSession);
    void    send_sip_unregsiter(CSipSession* pSession);
    void    send_sip_check_timeout(CSipSession* pSession);
    void    send_sip_option(CSipSession* pSession);
    void deal_sip_event(eXosip_event_t *event);
    void deal_regsiter_success(eXosip_event_t *event);
    void deal_regsiter_fail(eXosip_event_t *event);
    void deal_call_invite_req(eXosip_event_t *event);
    void deal_call_ack_req(eXosip_event_t *event);
    void deal_call_close_req(eXosip_event_t *event);
    void deal_call_cancelled_req(eXosip_event_t *event);
    void deal_message_req(eXosip_event_t *event);
private:
    int32_t       init_port_pairs();
    CRtpPortPair* get_free_port_pair(int nCallId);
    void          free_port_pair(int nCallId);
private:
    u_int32_t         m_ulTdIndex;
    as_mutex_t       *m_mutex;
    char              m_LoopWatchVar;
    struct event_base*m_httpBase;
    struct evhttp    *m_httpServer;
    as_thread_t      *m_HttpThreadHandle;
    u_int32_t         m_httpListenPort;
    as_thread_t      *m_SipThreadHandle;
    u_int32_t         m_ulRtpStartPort;
    u_int32_t         m_ulRtpEndPort;
    RTPPORTPAIRLIST   m_freePortList;
    CALLPORTPAIREMAP  m_callPortMap;
    as_thread_t      *m_ThreadHandle[RTSP_MANAGE_ENV_MAX_COUNT];
    UsageEnvironment *m_envArray[RTSP_MANAGE_ENV_MAX_COUNT];
    u_int32_t         m_clCountArray[RTSP_MANAGE_ENV_MAX_COUNT];
    u_int32_t         m_ulRecvBufSize;
    u_int32_t         m_ulLogLM;
private:
    SIPSESSIONMAP     m_SipSessionMap;
    REGSESSIONMAP     m_RegSessionMap;
    struct eXosip_t  *m_pEXosipCtx;

    std::string       m_strLocalIP;
    unsigned short    m_usPort;
    std::string       m_strFireWallIP;
    std::string       m_strTransPort;
    std::string       m_strProxyAddr;
    unsigned short    m_usProxyPort;

    CSipSessionTimer  m_SipSessionTimer;
private:
    std::string       m_strAppID;
    std::string       m_strAppSecret;
    std::string       m_strAppKey;
    std::string       m_strLiveUrl;
};
#endif /* __AS_RTSP_CLIENT_MANAGE_H__ */
