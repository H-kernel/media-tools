#ifndef __AS_RTSP_CLIENT_MANAGE_H__
#define __AS_RTSP_CLIENT_MANAGE_H__
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#ifdef WIN32
#include <time.h>
#include <Winsock2.h>
extern "C"{
#include <http.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <event2/evhttp.h>
#include "event2/dns.h"
#include "event2/thread.h"

//#include <ortp/ortp.h>
#include <eXosip2/eXosip.h>

}
#else
#include <unistd.h>     //for getopt, fork
#include <sys/time.h>
extern "C"{
#include "event2/http.h"
#include "event.h"
#include "event2/buffer.h"
#include "event2/keyvalq_struct.h"
#include "evhttp.h"
#include "event2/dns.h"
#include "event2/thread.h"

//#include <ortp/ortp.h>
#include <eXosip2/eXosip.h>
}

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
//#define _AS_DEBUG_
#ifndef _AS_DEBUG_
#define CAMERASVR_CONF_FILE "../conf/camera_gw.conf"
#define CAMERASVR_LOG_FILE  "../logs/camera_gw.log"
#else
#define CAMERASVR_CONF_FILE "E:\\build\\conf\\camera_gw.conf"
#define CAMERASVR_LOG_FILE  "E:\\build\\logs\\camera_gw.log"
#endif
#define HTTP_SERVER_URI    "/check/req"

#define HTTP_CONTENT_TYPE_JSON   std::string("application/json")
#define HTTP_CONTENT_TYPE_XML    std::string("application/xml")


#define AS_DEVICEID_LEN                20
#define AS_IP_LENS                     16

#define GW_SERVER_ADDR                 "0.0.0.0"
#define GW_SERVER_PORT_DEFAULT         8000
#define GW_SIP_PORT_DEFAULT            5060
#define GW_RTP_PORT_START              10000
#define GW_RTP_PORT_END                11000

#define HTTP_OPTION_TIMEOUT            864000
#define HTTP_REQUEST_MAX               4096
#define HTTP_CODE_OK                   200
#define HTTP_CODE_AUTH                 401
#define HTTP_DIGEST_LENS_MAX           4096


#define HTTP_WWW_AUTH                   "WWW-Authenticate"
#define HTTP_AUTHENTICATE               "Authorization"

#define AC_MSS_PORT_DAFAULT            8080
#define AC_MSS_SIGN_TIME_LEN           16
#define AC_MSS_ERROR_CODE_OK           "00000000"

#define GW_TIMER_SCALE                 1000
#define GW_TIMER_CHECK_TASK            5000



#define GW_REPORT_DEFAULT              60

#define GW_PORT_PAIR_SIZE              4


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

#define RTSP_CHECK_TMP_BUF_SIZE   256


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

#define ALLCAM_AGENT_NAME                 "all camera server"



typedef enum AS_DEV_STATUS
{
    AS_DEV_STATUS_OFFLIEN   = 0,
    AS_DEV_STATUS_ONLINE    = 1,
    AS_DEV_STATUS_MAX
}DEV_STATUS;

typedef enum AS_DEV_TYPE
{
    AS_DEV_TYPE_GB28181    = 1,
    AS_DEV_TYPE_VMS        = 2,
    AS_DEV_TYPE_MAX
}DEV_TYPE;


class ASLens
{
public:
    ASLens();
    virtual ~ASLens();
public:
    std::string    m_strCameraID;
    std::string    m_strCameraName;
    DEV_STATUS     m_Status;
    DEV_TYPE       m_enDeviceType;
};

typedef std::map<std::string,ASLens*>        LENSINFOMAP;
typedef LENSINFOMAP::iterator                LENSINFOMAPITRT;

class ASDevice
{
public:
    ASDevice();
    virtual ~ASDevice();
    void DevID(std::string& strDveID);
    void setDevInfo(std::string& strHost,std::string& strPort);
    std::string getSendTo(){return m_strTo;};
    std::string getDevId(){return m_strDevID;};
    void handleMessage(std::string& strMsg);
    DEV_STATUS Status();
    std::string createQueryCatalog();
private:
    int32_t parseNotify(const XMLElement &rRoot);
    int32_t parseResponse(const XMLElement &rRoot);
    int32_t parseQueryCatalog(const XMLElement &rRoot);
    int32_t parseDeviceItem(const XMLElement &rItem);
private:
    std::string   m_strDevID;
    std::string   m_stHost;
    std::string   m_strPort;
    std::string   m_strTo;
    LENSINFOMAP   m_LensMap;
    DEV_STATUS    m_Status;
};



class ASEvLiveHttpClient
{
public:
    ASEvLiveHttpClient();
    virtual ~ASEvLiveHttpClient();
    int32_t send_live_url_request(std::string& strCameraID,std::string& strStreamType,std::string& strRtspUrl);
    void    report_check_msg(std::string& strUrl,std::string& strMsg);
public:
    void handle_remote_read(struct evhttp_request* remote_rsp);
    void handle_readchunk(struct evhttp_request* remote_rsp);
    void handle_remote_connection_close(struct evhttp_connection* connection);
private:
    static void remote_read_cb(struct evhttp_request* remote_rsp, void* arg);
    static void readchunk_cb(struct evhttp_request* remote_rsp, void* arg);
    static void remote_connection_close_cb(struct evhttp_connection* connection, void* arg);
private:
    int32_t send_http_request(std::string& strUrl,std::string& strMsg,evhttp_cmd_type type = EVHTTP_REQ_POST,
                              std::string strContentType = HTTP_CONTENT_TYPE_JSON);
private:
    std::string              m_reqPath;
    std::string              m_strRespMsg;
    as_digest_t              m_Authen;
};


class ASCameraSvrManager
{
public:
    static ASCameraSvrManager& instance()
    {
        static ASCameraSvrManager objASCameraSvrManager;
        return objASCameraSvrManager;
    }
    virtual ~ASCameraSvrManager();
public:
    // init the live Environment
    int32_t init();
    void    release();
    int32_t open();
    void    close();
    void      setRecvBufSize(u_int32_t ulSize);
    u_int32_t getRecvBufSize();
    int32_t reg_lens_dev_map(std::string& strLensID,std::string& strDevID);
public:
    void http_env_thread();
    void rtsp_env_thread();
    void sip_env_thread();
    void notify_env_thread();
    u_int32_t find_beast_thread();
    UsageEnvironment* get_env(u_int32_t index);
    void releas_env(u_int32_t index);
public:
    void handle_http_req(struct evhttp_request *req);
protected:
    ASCameraSvrManager();
private:
    int32_t       read_conf();
    int32_t      read_system_conf();
    static void  http_callback(struct evhttp_request *req, void *arg);
private:
    static void *http_env_invoke(void *arg);
    static void *rtsp_env_invoke(void *arg);
    static void *sip_evn_invoke(void *arg);
    static void *notify_evn_invoke(void *arg);
    u_int32_t thread_index()
    {
        as_mutex_lock(m_mutex);
        u_int32_t index = m_ulTdIndex;
        m_ulTdIndex++;
        as_mutex_unlock(m_mutex);
        return index;
    }
private:
    // function for the sip deal
    int32_t read_sip_conf();
    static void osip_trace_log_callback(char *fi, int li, osip_trace_level_t level, char *chfr, va_list ap);
    int32_t init_Exosip();
    int32_t send_sip_response(eXosip_event_t& rEvent, int32_t nRespCode);
    int32_t handleRegisterReq(eXosip_event_t& rEvent);
    int32_t handleDeviceRegister(eXosip_event_t& rEvent);
    int32_t handleDeviceUnRegister(eXosip_event_t& rEvent);
    int32_t handleMessageReq(eXosip_event_t& rEvent);
    int32_t send_catalog_Req(ASDevice* pDev);//for GB28181

private:
    ASDevice* find_device(std::string& strDevID);
    void release_device(ASDevice* pDev);
    int32_t handle_http_message(std::string& strReq,std::string& strResp);
private:
    u_int32_t         m_ulTdIndex;
    as_mutex_t       *m_mutex;
    char              m_LoopWatchVar;
    u_int32_t         m_ulRecvBufSize;
    u_int32_t         m_ulLogLM;
private:
    typedef std::map<std::string, ASDevice*> DEV_MAP;
    DEV_MAP           m_devMap;
    as_mutex_t       *m_devMutex;
    typedef std::map<std::string, std::string> LENS_DEV_MAP;
    LENS_DEV_MAP      m_LensDevMap;
private:
    //GB28181 SIP Service
    struct eXosip_t  *m_pEXosipCtx;
    as_thread_t      *m_SipThreadHandle;
    std::string       m_strLocalIP;
    unsigned short    m_usPort;
    std::string       m_strFireWallIP;
    std::string       m_strTransPort;
    std::string       m_strProxyAddr;
    unsigned short    m_usProxyPort;
    std::string       m_strServerID;
private:
    //HTTP Control Service
    struct event_base*m_httpBase;
    struct evhttp    *m_httpServer;
    as_thread_t      *m_HttpThreadHandle;
    u_int32_t         m_httpListenPort;
private:
    //Msg notify(http,redis) service
    as_thread_t      *m_notifyThreadHandle;
    std::string       m_strDevNotifyeUrl;
    std::string       m_strStreamNotifyeUrl;
    std::string       m_strAlarmNotifyUrl;
private:
    //Stream service
    as_thread_t      *m_ThreadHandle[RTSP_MANAGE_ENV_MAX_COUNT];
    UsageEnvironment *m_envArray[RTSP_MANAGE_ENV_MAX_COUNT];
    u_int32_t         m_clCountArray[RTSP_MANAGE_ENV_MAX_COUNT];
};
#endif /* __AS_RTSP_CLIENT_MANAGE_H__ */
