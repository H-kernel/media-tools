#include "stdafx.h"
#include "liveMedia.hh"
#include "RTSPCommon.hh"
#include "Base64.hh"
#include "GroupsockHelper.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "as_camera_server.h"
#include "RTSPCommon.hh"
#include "as_log.h"
#include "as_lock_guard.h"
#include "as_ini_config.h"
#include "as_timer.h"
#include "as_mem.h"

using namespace tinyxml2;


#if defined(__WIN32__) || defined(_WIN32)
extern "C" int initializeWinsockIfNecessary();
#endif



ASLens::ASLens()
{
    m_Status = AS_DEV_STATUS_OFFLIEN;
    m_strCameraID = "";
}
ASLens::~ASLens()
{
}


ASDevice::ASDevice()
{
    m_strDevID = "";
    m_Status = AS_DEV_STATUS_OFFLIEN;
}


ASDevice::~ASDevice()
{

}
void ASDevice::DevID(std::string& strDveID)
{
    m_strDevID = strDveID;
}

void ASDevice::setDevInfo(std::string& strHost,std::string& strPort)
{
    m_stHost   = strHost;
    m_strPort = strPort;
    m_strTo = "sip:" + m_strDevID + "@" + m_stHost + ":" + m_strPort;
    AS_LOG(AS_LOG_INFO,"ASDevice::setDevInfo,host:[%s],port:[%s].",
                                          m_stHost.c_str(),m_strPort.c_str());
}
void ASDevice::handleMessage(std::string& strMsg)
{
    AS_LOG(AS_LOG_DEBUG,"ASDevice::handleMessage begin.");
    XMLDocument doc;
    XMLError xmlerr = doc.Parse(strMsg.c_str(),strMsg.length());
    if(XML_SUCCESS != xmlerr)
    {
        AS_LOG(AS_LOG_WARNING, "CManscdp::parse,parse xml msg:[%s] fail.",strMsg.c_str());
        return ;
    }


    XMLElement *req = doc.RootElement();
    if (NULL == req)
    {
        AS_LOG(AS_LOG_ERROR, "Parse XML failed, content is %s.", strMsg.c_str());
        return ;
    }

    int32_t nResult = AS_ERROR_CODE_OK;

    do
    {
        if (0 == strcmp(req->Name(), "Notify"))
        {
            nResult = parseNotify(*req);
            break;
        }
        else if (0 == strcmp(req->Name(), "Response"))
        {
            nResult = parseResponse(*req);
            break;
        }
    }while(0);

    if (0 != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "Parse XML failed, content is %s.", strMsg.c_str());
        return ;
    }


    AS_LOG(AS_LOG_DEBUG,"ASDevice::handleMessage end.");
    return ;
}

DEV_STATUS ASDevice::Status()
{
    return m_Status;
}
int32_t ASDevice::parseNotify(const XMLElement &rRoot)
{
    const XMLElement* pCmdType = rRoot.FirstChildElement("CmdType");
    if (NULL == pCmdType)
    {
        AS_LOG(AS_LOG_ERROR, "Parse notify failed. Can't find 'CmdType'.");
        return AS_ERROR_CODE_FAIL;
    }

    AS_LOG(AS_LOG_INFO, "Receive notify %s.", pCmdType->GetText());

    if (0 == strcmp(pCmdType->GetText(), "Keepalive"))
    {
        AS_LOG(AS_LOG_INFO, "Receive Notify Keepalive.");
    }

    return AS_ERROR_CODE_OK;
}

int32_t ASDevice::parseResponse(const XMLElement &rRoot)
{
    const XMLElement* pCmdType = rRoot.FirstChildElement("CmdType");
    if (NULL == pCmdType)
    {
        AS_LOG(AS_LOG_ERROR, "Parse response failed. Can't find 'CmdType'.");
        return AS_ERROR_CODE_FAIL;
    }

    AS_LOG(AS_LOG_INFO, "Receive response %s.", pCmdType->GetText());

    int32_t nResult = 0;
    if (0 == strcmp(pCmdType->GetText(), "Catalog"))
    {
        nResult = parseQueryCatalog(rRoot);
    }

    if (0 != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "Parse response %s failed.", pCmdType->GetText());
        return -1;
    }

    return AS_ERROR_CODE_OK;
}

int32_t ASDevice::parseQueryCatalog(const XMLElement &rRoot)
{
    const XMLElement* pDeviceList = rRoot.FirstChildElement("DeviceList");
    if (NULL == pDeviceList)
    {
        AS_LOG(AS_LOG_ERROR, "Parse response failed. Can't find 'DeviceList'.");
        return AS_ERROR_CODE_FAIL;
    }

    int32_t nDeviceNum = 0;
    int32_t nReulst = pDeviceList->QueryIntAttribute("Num", &nDeviceNum);
    if (XML_SUCCESS != nReulst)
    {
        AS_LOG(AS_LOG_ERROR, "Parse Num of DeviceList failed, error code is %d.", nReulst);
        return AS_ERROR_CODE_FAIL;
    }

    if (0 >= nDeviceNum)
    {
        AS_LOG(AS_LOG_INFO, "Num of DeviceList is 0.");
        return AS_ERROR_CODE_OK;
    }

    int32_t nItemNum = 0;
    const XMLElement* pItem = pDeviceList->FirstChildElement("Item");
    while (NULL != pItem)
    {
        nReulst = parseDeviceItem(*pItem);
        if (0 != nReulst)
        {
            AS_LOG(AS_LOG_ERROR, "Parse item of device list failed.");
            return AS_ERROR_CODE_FAIL;
        }

        nItemNum++;
        pItem = pItem->NextSiblingElement();
    }

    if (nDeviceNum != nItemNum)
    {
        AS_LOG(AS_LOG_ERROR, "Real item num(%d) of device list is not the same as num(%d) of device list.",
            nItemNum, nDeviceNum);
        return AS_ERROR_CODE_FAIL;
    }

    return AS_ERROR_CODE_OK;
}

int32_t ASDevice::parseDeviceItem(const XMLElement &rItem)
{
    const XMLElement* pDeviceID = rItem.FirstChildElement("DeviceID");
    if (NULL == pDeviceID)
    {
        AS_LOG(AS_LOG_ERROR, "Parse response failed. Can't find 'DeviceID' of 'Item'.");
        return AS_ERROR_CODE_FAIL;
    }

    const XMLElement* pName = rItem.FirstChildElement("Name");
    if (NULL == pName)
    {
        AS_LOG(AS_LOG_ERROR, "Parse response failed. Can't find 'Name' of 'Item'.");
        return AS_ERROR_CODE_FAIL;
    }

    const XMLElement* pManufacturer = rItem.FirstChildElement("Manufacturer");
    if (NULL == pManufacturer)
    {
        AS_LOG(AS_LOG_ERROR, "Parse response failed. Can't find 'Manufacturer' of 'Item'.");
        return AS_ERROR_CODE_FAIL;
    }

    const XMLElement* pModel = rItem.FirstChildElement("Model");
    if (NULL == pModel)
    {
        AS_LOG(AS_LOG_ERROR, "Parse response failed. Can't find 'Model' of 'Item'.");
        return AS_ERROR_CODE_FAIL;
    }

    const XMLElement* pStatus = rItem.FirstChildElement("Status");
    if (NULL == pStatus)
    {
        AS_LOG(AS_LOG_ERROR, "Parse response failed. Can't find 'Status' of 'Item'.");
        return AS_ERROR_CODE_FAIL;
    }

    std::string strLensId = pDeviceID->GetText();

    ASLens* pLens = NULL;

    LENSINFOMAPITRT iter = m_LensMap.find(strLensId);
    if(iter != m_LensMap.end())
    {
        pLens = iter->second;
    }
    else
    {
        pLens = AS_NEW(pLens);
        m_LensMap.insert(LENSINFOMAP::value_type(strLensId,pLens));
    }
    pLens->m_strCameraID =  strLensId;
    pLens->m_enDeviceType = AS_DEV_TYPE_GB28181;
    pLens->m_Status = (0 == strcmp(pStatus->GetText(), "ON"))
                                ? AS_DEV_STATUS_ONLINE
                                : AS_DEV_STATUS_OFFLIEN;
    pLens->m_strCameraName = pName->GetText();

    return ASCameraSvrManager::instance().reg_lens_dev_map(strLensId,m_strDevID);
}

std::string ASDevice::createQueryCatalog()
{
    XMLDocument XmlDoc;
    XMLPrinter printer;
    try
    {
        XMLDeclaration *declare = XmlDoc.NewDeclaration();
        XmlDoc.LinkEndChild(declare);

        XMLElement *xmlQuery = XmlDoc.NewElement("Query");
        XmlDoc.LinkEndChild(xmlQuery);

        XMLElement *xmlCmdType = XmlDoc.NewElement("CmdType");
        xmlCmdType->SetText("Catalog");
        xmlQuery->LinkEndChild(xmlCmdType);

        XMLElement *xmlSN = XmlDoc.NewElement("SN");
        xmlSN->SetText("17430");
        xmlQuery->LinkEndChild(xmlSN);

        XMLElement *xmlDeviceID = XmlDoc.NewElement("DeviceID");
        xmlSN->SetText(m_strDevID.c_str());
        xmlQuery->LinkEndChild(xmlDeviceID);
    }
    catch(...)
    {
        AS_LOG(AS_LOG_ERROR, "Create query catalog xml failed.");
        return "";
    }

    XmlDoc.Accept(&printer);
    std::string strMsg = printer.CStr();

    return strMsg;
}


ASEvLiveHttpClient::ASEvLiveHttpClient()
{
    m_reqPath = "/";
    m_strRespMsg = "";
}
ASEvLiveHttpClient::~ASEvLiveHttpClient()
{
}
int32_t ASEvLiveHttpClient::send_live_url_request(std::string& strCameraID,
                                               std::string& strStreamType,std::string& strRtspUrl)
{
    std::string strLiveUrl;//   = ASCameraSvrManager::instance().getLiveUrl();

    AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::send_live_url_request begin.");

    std::string strSign    = "all stream";

    /* 1.build the request json message */

    cJSON* root = cJSON_CreateObject();

    //cJSON_AddItemToObject(root, "appID", cJSON_CreateString(strAppID.c_str()));
    /* TODO : account ,how to set */
    //cJSON_AddItemToObject(root, "account", cJSON_CreateString(strAppID.c_str()));
    /* TODO : sign ,sign */
    cJSON_AddItemToObject(root, "sign", cJSON_CreateString(strSign.c_str()));

    //time_t ulTick = time(NULL);
    //char szTime[AC_MSS_SIGN_TIME_LEN] = { 0 };
    //as_strftime((char*)&szTime[0], AC_MSS_SIGN_TIME_LEN, "%Y%m%d%H%M%S", ulTick);
    //cJSON_AddItemToObject(root, "msgtimestamp", cJSON_CreateString((char*)&szTime[0]));

    cJSON_AddItemToObject(root, "cameraId", cJSON_CreateString(strCameraID.c_str()));
    cJSON_AddItemToObject(root, "streamType", cJSON_CreateString(strStreamType.c_str()));
    cJSON_AddItemToObject(root, "urlType", cJSON_CreateString("1"));

    std::string strReqMSg = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    /* sent the http request */
    if(AS_ERROR_CODE_OK != send_http_request(strLiveUrl,strReqMSg)) {
        AS_LOG(AS_LOG_WARNING,"ASEvLiveHttpClient::send_live_url_request fail.");
        return AS_ERROR_CODE_FAIL;
    }

    if(0 == m_strRespMsg.length()) {
        AS_LOG(AS_LOG_WARNING,"ASEvLiveHttpClient::send_live_url_request, message is empty.");
        return AS_ERROR_CODE_FAIL;
    }

    /* 2.parse the response */
    root = cJSON_Parse(m_strRespMsg.c_str());
    if (NULL == root) {
        AS_LOG(AS_LOG_WARNING,"ASEvLiveHttpClient::send_live_url_request, json message parser fail.");
        return AS_ERROR_CODE_FAIL;
    }

    cJSON *resultCode = cJSON_GetObjectItem(root, "resultCode");
    if(NULL == resultCode) {
        cJSON_Delete(root);
        AS_LOG(AS_LOG_WARNING,"ASEvLiveHttpClient::send_live_url_request, json message there is no resultCode.");
        return AS_ERROR_CODE_FAIL;
    }

    if(0 != strncmp(AC_MSS_ERROR_CODE_OK,resultCode->valuestring,strlen(AC_MSS_ERROR_CODE_OK))) {
        cJSON_Delete(root);
        AS_LOG(AS_LOG_WARNING,"ASEvLiveHttpClient::send_live_url_request, resultCode:[%s] is not success.",resultCode->valuestring);
        return AS_ERROR_CODE_FAIL;
    }

    cJSON *url = cJSON_GetObjectItem(root, "url");
    if(NULL == url) {
        cJSON_Delete(root);
        AS_LOG(AS_LOG_WARNING,"ASEvLiveHttpClient::send_live_url_request, json message there is no url.");
        return AS_ERROR_CODE_FAIL;
    }
    strRtspUrl = url->valuestring;
    cJSON_Delete(root);
    AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::send_live_url_request end,url:[%s].",strRtspUrl.c_str());
    return AS_ERROR_CODE_OK;
}
void    ASEvLiveHttpClient::report_check_msg(std::string& strUrl,std::string& strMsg)
{
    AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::report_check_msg begin.");
    AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::report_check_msg,url:[%s],msg:[%s].",
                                            strUrl.c_str(),strMsg.c_str());
    if (AS_ERROR_CODE_OK != send_http_request(strUrl,strMsg,EVHTTP_REQ_POST,HTTP_CONTENT_TYPE_JSON)) {
        AS_LOG(AS_LOG_WARNING,"ASEvLiveHttpClient::report_check_msg,send msg fail.url:[%s],msg:[%s].",
                                            strUrl.c_str(),strMsg.c_str());
        return ;
    }
    AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::report_check_msg end.");
    return;
}
void ASEvLiveHttpClient::handle_remote_read(struct evhttp_request* remote_rsp)
{
    if (NULL == remote_rsp){
        //event_base_loopexit(m_pBase, NULL);
        //event_base_loopbreak(m_pBase);
        return;
    }

    size_t len = evbuffer_get_length(remote_rsp->input_buffer);
    const char * str = (const char*)evbuffer_pullup(remote_rsp->input_buffer, len);
    if ((0 == len) || (NULL == str)) {
        //m_strRespMsg = "";
        //event_base_loopexit(m_pBase, NULL);
        return;
    }
    m_strRespMsg.append(str, 0, len);
    AS_LOG(AS_LOG_INFO,"ASEvLiveHttpClient::handle_remote_read,msg:[%s] .",m_strRespMsg.c_str());
    //event_base_loopexit(m_pBase, NULL);
    //event_base_loopbreak(m_pBase);
}

void ASEvLiveHttpClient::handle_readchunk(struct evhttp_request* remote_rsp)
{
    if (NULL == remote_rsp){
        //event_base_loopexit(m_pBase, NULL);
        //event_base_loopbreak(m_pBase);
        return;
    }

    size_t len = evbuffer_get_length(remote_rsp->input_buffer);
    const char * str = (const char*)evbuffer_pullup(remote_rsp->input_buffer, len);
    if ((0 == len) || (NULL == str)) {
        //event_base_loopexit(m_pBase, NULL);
        return;
    }
    m_strRespMsg.append(str, len);
    AS_LOG(AS_LOG_INFO,"ASEvLiveHttpClient::handle_readchunk,msg:[%s] .",m_strRespMsg.c_str());
}

void ASEvLiveHttpClient::handle_remote_connection_close(struct evhttp_connection* connection)
{
    //event_base_loopexit(m_pBase, NULL);
}

void ASEvLiveHttpClient::remote_read_cb(struct evhttp_request* remote_rsp, void* arg)
{
    ASEvLiveHttpClient* client = (ASEvLiveHttpClient*)arg;
    client->handle_remote_read(remote_rsp);
    return;
}

void ASEvLiveHttpClient::readchunk_cb(struct evhttp_request* remote_rsp, void* arg)
{
    ASEvLiveHttpClient* client = (ASEvLiveHttpClient*)arg;
    client->handle_readchunk(remote_rsp);
    return;
}

void ASEvLiveHttpClient::remote_connection_close_cb(struct evhttp_connection* connection, void* arg)
{
    ASEvLiveHttpClient* client = (ASEvLiveHttpClient*)arg;
    client->handle_remote_connection_close(connection);
    return;
}


int32_t ASEvLiveHttpClient::send_http_request(std::string& strUrl,std::string& strMsg,
                                           evhttp_cmd_type type,std::string strContentType)
{
    AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::send_http_request begin.");

    struct evhttp_request   *pReq  = NULL;
    struct event_base       *pBase = NULL;
    struct evhttp_connection*pConn = NULL;
    struct evdns_base       *pDnsbase = NULL;
    char   Digest[HTTP_DIGEST_LENS_MAX] = {0};
    unsigned char Password[RTSP_CHECK_TMP_BUF_SIZE] = {0};

    if (-1 == as_digest_init(&m_Authen,0)) {
        return AS_ERROR_CODE_FAIL;
    }
    std::string strUserName;//  = ASCameraSvrManager::instance().getUserName();
    std::string strPassword;//  = ASCameraSvrManager::instance().getPassword();

    if(BASE64_OK != as_base64_decode(strPassword.c_str(), strPassword.length(), Password)) {
        AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::send_http_request username:[%s] password:[%s]"
                            " base64 decode fail.",
                            strUserName.c_str(),strPassword.c_str());
        return AS_ERROR_CODE_FAIL;
    }

    AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::send_http_request username:[%s] password:[%s]"
                        " base64 decode password:[%s].",
                        strUserName.c_str(),strPassword.c_str(),Password);

    struct evhttp_uri* uri = evhttp_uri_parse(strUrl.c_str());
    if (!uri)
    {
        return AS_ERROR_CODE_FAIL;
    }

    int port = evhttp_uri_get_port(uri);
    if (port < 0) {
        port = AC_MSS_PORT_DAFAULT;
    }
    const char *host = evhttp_uri_get_host(uri);
    const char *path = evhttp_uri_get_path(uri);
    if (NULL == host)
    {
        evhttp_uri_free(uri);
        return AS_ERROR_CODE_FAIL;
    }
    if (path == NULL || strlen(path) == 0)
    {
        m_reqPath = "/";
    }
    else
    {
        m_reqPath = path;
    }

    int32_t nRet    = AS_ERROR_CODE_OK;
    Boolean bAuth   = False;
    char lenbuf[RTSP_CHECK_TMP_BUF_SIZE] = { 0 };


   int nMethod = DIGEST_METHOD_POST;

    std::string strCmd = "POST";
    if(EVHTTP_REQ_GET == type)
    {
        strCmd = "GET";
        nMethod = DIGEST_METHOD_GET;
    }

    int32_t nTryTime = 0;

    do {
        if(3 < nTryTime)
        {
            break;
        }
        nTryTime++;

        pReq = evhttp_request_new(remote_read_cb, this);
        evhttp_add_header(pReq->output_headers, "Content-Type", strContentType.c_str());
        snprintf(lenbuf, 32, "%lu", strMsg.length());
        evhttp_add_header(pReq->output_headers, "Content-length", lenbuf); //content length
        snprintf(lenbuf, 32, "%s:%d", host,port);
        evhttp_add_header(pReq->output_headers, "Host", lenbuf);
        evhttp_add_header(pReq->output_headers, "Connection", "close");
        evhttp_request_set_chunked_cb(pReq, readchunk_cb);
        pBase = event_base_new();
        if (!pBase)
        {
            nRet = AS_ERROR_CODE_FAIL;
            break;
        }

        if(bAuth)
        {
            as_digest_attr_value_t value;
            value.string = (char*)strUserName.c_str();
            as_digest_set_attr(&m_Authen, D_ATTR_USERNAME,value);
            value.string = (char*)&Password[0];
            as_digest_set_attr(&m_Authen, D_ATTR_PASSWORD,value);
            value.string = (char*)m_reqPath.c_str();
            as_digest_set_attr(&m_Authen, D_ATTR_URI,value);
            value.number = nMethod;
            as_digest_set_attr(&m_Authen, D_ATTR_METHOD, value);

            if (-1 == as_digest_client_generate_header(&m_Authen, Digest, sizeof (Digest))) {
                nRet = AS_ERROR_CODE_FAIL;
                break;
            }
            AS_LOG(AS_LOG_INFO,"generate digest :[%s].",Digest);
            evhttp_add_header(pReq->output_headers, HTTP_AUTHENTICATE,Digest);
        }
        else if(1 == nTryTime)
        {
            evhttp_add_header(pReq->output_headers, HTTP_AUTHENTICATE,"Authorization: Digest username=test,realm=test, nonce =0001,"
                                                              "uri=/api/alarm/list,response=c76a6680f0f1c8e26723d81b44977df0,"
                                                              "cnonce=00000001,opaque=000001,qop=auth,nc=00000001");
        }

        pDnsbase = evdns_base_new(pBase, 0);
        if (NULL == pDnsbase)
        {
            nRet = AS_ERROR_CODE_FAIL;
            break;
        }

        pConn = evhttp_connection_base_new(pBase,pDnsbase, host, port);
        //pConn = evhttp_connection_new( host, port);
        if (!pConn)
        {
            nRet = AS_ERROR_CODE_FAIL;
            break;
        }
        //evhttp_connection_set_base(pConn, pBase);
        evhttp_connection_set_closecb(pConn, remote_connection_close_cb, this);

        //struct evbuffer *buf = NULL;
        //buf = evbuffer_new();
        //if (NULL == buf)
        //{
        //    nRet = AS_ERROR_CODE_FAIL;
        //    break;
        //}
        pReq->output_buffer = evbuffer_new();
        if (NULL == pReq->output_buffer)
        {
            nRet = AS_ERROR_CODE_FAIL;
            break;
        }


        //evbuffer_add_printf(pReq->output_buffer, "%s", strMsg.c_str());

        //evbuffer_add_printf(buf, "%s", strMsg.c_str());
        //evbuffer_add_buffer(pReq->output_buffer, buf);
        evbuffer_add(pReq->output_buffer,strMsg.c_str(),strMsg.length());
        pReq->flags = EVHTTP_USER_OWNED;
        evhttp_make_request(pConn, pReq, type, m_reqPath.c_str());
        evhttp_connection_set_timeout(pReq->evcon, 600);
        event_base_dispatch(pBase);

        int32_t nRespCode = evhttp_request_get_response_code(pReq);
        AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::send_http_request,http result:[%d].",nRespCode);
        if(HTTP_CODE_OK == nRespCode) {
            break;
        }
        else if(HTTP_CODE_AUTH == nRespCode) {
            const char* pszAuthInfo = evhttp_find_header(pReq->input_headers,HTTP_WWW_AUTH);
            if(NULL == pszAuthInfo) {
                nRet = AS_ERROR_CODE_FAIL;
                AS_LOG(AS_LOG_WARNING,"find WWW-Authenticate header fail.");
                break;
            }
            AS_LOG(AS_LOG_INFO,"ASEvLiveHttpClient::send_http_request,handle authInfo:[%s].",pszAuthInfo);
            if (-1 == as_digest_is_digest(pszAuthInfo)) {
                nRet = AS_ERROR_CODE_FAIL;
                AS_LOG(AS_LOG_WARNING,"the WWW-Authenticate is not digest.");
                break;
            }

            if (0 == as_digest_client_parse(&m_Authen, pszAuthInfo)) {
                nRet = AS_ERROR_CODE_FAIL;
                AS_LOG(AS_LOG_WARNING,"parser WWW-Authenticate info fail.");
                break;
            }
            AS_LOG(AS_LOG_INFO,"ASEvLiveHttpClient::send_http_request,parser authInfo ok try again.");
            m_strRespMsg =""; //reset the response message
            bAuth = True;
        }
        else {
            nRet = AS_ERROR_CODE_FAIL;
            break;
        }

        evhttp_connection_free(pConn);
        event_base_free(pBase);
    }while(true);

    evhttp_uri_free(uri);
    AS_LOG(AS_LOG_DEBUG,"ASEvLiveHttpClient::send_http_request end.");
    return nRet;
}



ASCameraSvrManager::ASCameraSvrManager()
{
    m_ulTdIndex        = 0;
    m_LoopWatchVar     = 0;
    m_ulRecvBufSize    = RTSP_SOCKET_RECV_BUFFER_SIZE_DEFAULT;
    m_HttpThreadHandle = NULL;
    m_SipThreadHandle  = NULL;
    m_httpBase         = NULL;
    m_httpServer       = NULL;
    m_httpListenPort   = GW_SERVER_PORT_DEFAULT;
    m_mutex            = NULL;
    memset(m_ThreadHandle,0,sizeof(as_thread_t*)*RTSP_MANAGE_ENV_MAX_COUNT);
    memset(m_envArray,0,sizeof(UsageEnvironment*)*RTSP_MANAGE_ENV_MAX_COUNT);
    memset(m_clCountArray,0,sizeof(u_int32_t)*RTSP_MANAGE_ENV_MAX_COUNT);
    m_ulLogLM          = AS_LOG_WARNING;
}

ASCameraSvrManager::~ASCameraSvrManager()
{
}

int32_t ASCameraSvrManager::init()
{
    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::init begin");
    /* read the config file */
    if (AS_ERROR_CODE_OK != read_conf()) {
        AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::init ,read conf fail");
        return AS_ERROR_CODE_FAIL;
    }

    /* start the log module */
    ASSetLogLevel(m_ulLogLM);
    ASSetLogFilePathName(CAMERASVR_LOG_FILE);
    ASStartLog();

    /* init the sip service */
    if (AS_ERROR_CODE_OK != init_Exosip()) {
        AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::init ,init sip fail");
        return AS_ERROR_CODE_FAIL;
    }

    event_init();


    m_mutex = as_create_mutex();
    if(NULL == m_mutex) {
        AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::init ,create mutex fail");
        return AS_ERROR_CODE_FAIL;
    }

    m_devMutex = as_create_mutex();
    if(NULL == m_devMutex) {
        AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::init ,create dev mutex fail");
        return AS_ERROR_CODE_FAIL;
    }

    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::init end");

    return AS_ERROR_CODE_OK;
}
void    ASCameraSvrManager::release()
{
    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::release begin");
    m_LoopWatchVar = 1;
    as_destroy_mutex(m_mutex);
    m_mutex = NULL;
    ASStopLog();
    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::release end");
}

int32_t ASCameraSvrManager::open()
{
    // Begin by setting up our usage environment:
    u_int32_t i = 0;

    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::open begin.");

    m_LoopWatchVar = 0;
    /* start the http server deal thread */
    if (AS_ERROR_CODE_OK != as_create_thread((AS_THREAD_FUNC)http_env_invoke,
        this, &m_HttpThreadHandle, AS_DEFAULT_STACK_SIZE)) {
        AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::open,create the http server thread fail.");
        return AS_ERROR_CODE_FAIL;
    }

    /* start the rtsp client deal thread */
    for(i = 0;i < RTSP_MANAGE_ENV_MAX_COUNT;i++) {
        m_envArray[i] = NULL;
        m_clCountArray[i] = 0;
    }

    for(i = 0;i < RTSP_MANAGE_ENV_MAX_COUNT;i++) {
        if( AS_ERROR_CODE_OK != as_create_thread((AS_THREAD_FUNC)rtsp_env_invoke,
                                                 this,&m_ThreadHandle[i],AS_DEFAULT_STACK_SIZE)) {
            AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::open,create the rtsp server thread fail.");
            return AS_ERROR_CODE_FAIL;
        }

    }

    /* start the notify deal thread */
    if (AS_ERROR_CODE_OK != as_create_thread((AS_THREAD_FUNC)notify_evn_invoke,
        this, &m_notifyThreadHandle, AS_DEFAULT_STACK_SIZE)) {
        AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::open,create the notify deal thread fail.");
        return AS_ERROR_CODE_FAIL;
    }

    /* start sip deal thread */
    if (AS_ERROR_CODE_OK != as_create_thread((AS_THREAD_FUNC)sip_evn_invoke,
        this, &m_SipThreadHandle, AS_DEFAULT_STACK_SIZE)) {
        AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::open,create the sip deal thread fail.");
        return AS_ERROR_CODE_FAIL;
    }

    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::open end.");
    return 0;
}

void ASCameraSvrManager::close()
{
    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::close.");
    m_LoopWatchVar = 1;

    return;
}

int32_t ASCameraSvrManager::read_conf()
{
    if(AS_ERROR_CODE_OK != read_system_conf()) {
        return AS_ERROR_CODE_FAIL;
    }
     if(AS_ERROR_CODE_OK != read_sip_conf()) {
        return AS_ERROR_CODE_FAIL;
    }

    return AS_ERROR_CODE_OK;
}


int32_t ASCameraSvrManager::read_system_conf()
{
    as_ini_config config;
    std::string   strValue="";
    if(INI_SUCCESS != config.ReadIniFile(CAMERASVR_CONF_FILE))
    {
        AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::read_system_conf,load conf file fail.");
        return AS_ERROR_CODE_FAIL;
    }

    /* log level */
    if(INI_SUCCESS == config.GetValue("LOG_CFG","LogLM",strValue))
    {
        m_ulLogLM = atoi(strValue.c_str());
    }
    return AS_ERROR_CODE_OK;
}

void  ASCameraSvrManager::http_callback(struct evhttp_request *req, void *arg)
{
    ASCameraSvrManager* pManage = (ASCameraSvrManager*)arg;
    pManage->handle_http_req(req);
}

void *ASCameraSvrManager::http_env_invoke(void *arg)
{
    ASCameraSvrManager* manager = (ASCameraSvrManager*)(void*)arg;
    manager->http_env_thread();
    return NULL;
}

void *ASCameraSvrManager::rtsp_env_invoke(void *arg)
{
    ASCameraSvrManager* manager = (ASCameraSvrManager*)(void*)arg;
    manager->rtsp_env_thread();
    return NULL;
}
void *ASCameraSvrManager::sip_evn_invoke(void *arg)
{
    ASCameraSvrManager* manager = (ASCameraSvrManager*)(void*)arg;
    manager->sip_env_thread();
    return NULL;
}
void *ASCameraSvrManager::notify_evn_invoke(void *arg)
{
    ASCameraSvrManager* manager = (ASCameraSvrManager*)(void*)arg;
    manager->notify_env_thread();
    return NULL;
}


void ASCameraSvrManager::http_env_thread()
{
    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::http_env_thread begin.");
    m_httpBase = event_base_new();
    if (NULL == m_httpBase)
    {
        AS_LOG(AS_LOG_CRITICAL,"ASCameraSvrManager::http_env_thread,create the event base fail.");
        return;
    }
    m_httpServer = evhttp_new(m_httpBase);
    if (NULL == m_httpServer)
    {
        AS_LOG(AS_LOG_CRITICAL,"ASCameraSvrManager::http_env_thread,create the http base fail.");
        return;
    }

    int ret = evhttp_bind_socket(m_httpServer, GW_SERVER_ADDR, m_httpListenPort);
    if (0 != ret)
    {
        AS_LOG(AS_LOG_CRITICAL,"ASCameraSvrManager::http_env_thread,bind the http socket fail.");
        return;
    }

    evhttp_set_timeout(m_httpServer, HTTP_OPTION_TIMEOUT);
    evhttp_set_gencb(m_httpServer, http_callback, this);
    event_base_dispatch(m_httpBase);

    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::http_env_thread end.");
    return;
}
void ASCameraSvrManager::rtsp_env_thread()
{
    u_int32_t index = thread_index();
    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::rtsp_env_thread,index:[%d] begin.",index);

    TaskScheduler* scheduler = NULL;
    UsageEnvironment* env = NULL;


    if(RTSP_MANAGE_ENV_MAX_COUNT <= index) {
        return;
    }
    scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);
    m_envArray[index] = env;
    m_clCountArray[index] = 0;


    // All subsequent activity takes place within the event loop:
    env->taskScheduler().doEventLoop(&m_LoopWatchVar);

    // LOOP EXIST
    env->reclaim();
    env = NULL;
    delete scheduler;
    scheduler = NULL;
    m_envArray[index] = NULL;
    m_clCountArray[index] = 0;
    AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::rtsp_env_thread,index:[%d] end.",index);
    return;
}

void ASCameraSvrManager::sip_env_thread()
{
    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::sip_env_thread begin.");

    eXosip_event_t *pEvent = NULL;

    while (0 == m_LoopWatchVar)
    {
        pEvent = eXosip_event_wait(m_pEXosipCtx, 0, 100);
        if (NULL == pEvent)
        {
            continue;
        }

        int32_t nResult = 0;

        switch (pEvent->type)
        {
        case EXOSIP_MESSAGE_NEW:
            AS_LOG(AS_LOG_INFO, "receivce message %s.", pEvent->request->sip_method);

            if (MSG_IS_REGISTER(pEvent->request))
            {
                nResult = handleRegisterReq(*pEvent);
            }
            else if (MSG_IS_MESSAGE(pEvent->request))
            {
                nResult = handleMessageReq(*pEvent);
            }
            break;
        default:
            AS_LOG(AS_LOG_INFO, "eXosip_event_wait OK. type is %d.", pEvent->type);
            break;
        }

        if (0 != nResult)
        {
            AS_LOG(AS_LOG_ERROR, "Handle %s failed.", pEvent->request->sip_method);
        }

        eXosip_event_free(pEvent);
    }
    AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::sip_env_thread end.");
}
void ASCameraSvrManager::notify_env_thread()
{
    AS_LOG(AS_LOG_DEBUG,"ASCameraSvrManager::notify_env_thread begin.");
    while(0 == m_LoopWatchVar)
    {
        as_sleep(GW_TIMER_CHECK_TASK);
    }
    AS_LOG(AS_LOG_ERROR,"ASCameraSvrManager::notify_env_thread end.");
}



u_int32_t ASCameraSvrManager::find_beast_thread()
{
    as_mutex_lock(m_mutex);
    u_int32_t index = 0;
    u_int32_t count = 0xFFFFFFFF;
    for(u_int32_t i = 0; i < RTSP_MANAGE_ENV_MAX_COUNT;i++) {
        if(count > m_clCountArray[i]) {
            index = i;
            count = m_clCountArray[i];
        }
    }
    as_mutex_unlock(m_mutex);
    return index;
}
UsageEnvironment* ASCameraSvrManager::get_env(u_int32_t index)
{
    UsageEnvironment* env = m_envArray[index];
    m_clCountArray[index]++;
    return env;
}
void ASCameraSvrManager::releas_env(u_int32_t index)
{
    if (0 == m_clCountArray[index])
    {
        return;
    }
    m_clCountArray[index]--;
}


void ASCameraSvrManager::handle_http_req(struct evhttp_request *req)
{
    AS_LOG(AS_LOG_DEBUG, "ASCameraSvrManager::handle_http_req begin");

    if (NULL == req)
    {
        return;
    }

    string uri_str = req->uri;
    string::size_type pos = uri_str.find_last_of(HTTP_SERVER_URI);

    if(pos == string::npos) {
         evhttp_send_error(req, 404, "service was not found!");
         return;
    }
    AS_LOG(AS_LOG_DEBUG, "ASCameraSvrManager::handle_http_req request path[%s].", uri_str.c_str());

    evbuffer *pbuffer = req->input_buffer;
    string post_str;
    int n = 0;
    char  szBuf[HTTP_REQUEST_MAX + 1] = { 0 };
    while ((n = evbuffer_remove(pbuffer, &szBuf, HTTP_REQUEST_MAX - 1)) > 0)
    {
        szBuf[n] = '\0';
        post_str.append(szBuf, n);
    }

    AS_LOG(AS_LOG_INFO, "ASCameraSvrManager::handle_http_req, msg[%s]", post_str.c_str());

    std::string strResp = "";

    if(AS_ERROR_CODE_OK != handle_http_message(post_str,strResp))
    {
        evhttp_send_error(req, 404, "call service fail!");
        return;
    }

    struct evbuffer* evbuf = evbuffer_new();
    if (NULL == evbuf)
    {
        return;
    }
    evbuffer_add_printf(evbuf, "%s", strResp.c_str());

    evhttp_send_reply(req, HTTP_OK, "OK", evbuf);
    evbuffer_free(evbuf);
    AS_LOG(AS_LOG_DEBUG, "ASCameraSvrManager::handle_http_req end");
}

ASDevice* ASCameraSvrManager::find_device(std::string& strDevID)
{
    as_lock_guard locker(m_devMutex);
    return NULL;
}
void ASCameraSvrManager::release_device(ASDevice* pDev)
{
    as_lock_guard locker(m_devMutex);
}


int32_t ASCameraSvrManager::handle_http_message(std::string& strReq,std::string& strResp)
{
    std::string strCheckID  = "";

    int32_t ret = AS_ERROR_CODE_OK;

    AS_LOG(AS_LOG_INFO, "ASCameraSvrManager::handle_http_message,msg:[%s].",strReq.c_str());



    XMLDocument doc;
    XMLError xmlerr = doc.Parse(strReq.c_str(),strReq.length());
    if(XML_SUCCESS != xmlerr)
    {
        AS_LOG(AS_LOG_WARNING, "ASCameraSvrManager::handle_http_message,parse xml msg:[%s] fail.",strReq.c_str());
        return AS_ERROR_CODE_FAIL;
    }

    XMLElement *req = doc.RootElement();
    if(NULL == req)
    {
        AS_LOG(AS_LOG_WARNING, "ASCameraSvrManager::handle_http_message,get xml req node fail.");
        return AS_ERROR_CODE_FAIL;
    }

    XMLDocument resp;
    XMLPrinter printer;
    XMLDeclaration *declare = resp.NewDeclaration();
    XMLElement *respEle = resp.NewElement("resp");
    resp.InsertEndChild(declare);
    resp.InsertEndChild(respEle);
    respEle->SetAttribute("version", "1.0");
    XMLElement *SesEle = resp.NewElement("check");
    respEle->InsertEndChild(SesEle);

    if(AS_ERROR_CODE_OK == ret)
    {
        respEle->SetAttribute("err_code","0");
        respEle->SetAttribute("err_msg","success");
    }
    else
    {
        respEle->SetAttribute("err_code","-1");
        respEle->SetAttribute("err_msg","fail");
    }

    resp.Accept(&printer);
    strResp = printer.CStr();

    AS_LOG(AS_LOG_DEBUG, "ASCameraSvrManager::handle_http_message,end");
    return AS_ERROR_CODE_OK;
}



void ASCameraSvrManager::setRecvBufSize(u_int32_t ulSize)
{
    m_ulRecvBufSize = ulSize;
}
u_int32_t ASCameraSvrManager::getRecvBufSize()
{
    return m_ulRecvBufSize;
}
int32_t ASCameraSvrManager::reg_lens_dev_map(std::string& strLensID,std::string& strDevID)
{
    LENS_DEV_MAP::iterator iter = m_LensDevMap.find(strLensID);

    if(iter != m_LensDevMap.end())
    {
        m_LensDevMap.erase(iter);
    }
    m_LensDevMap.insert(LENS_DEV_MAP::value_type(strLensID,strDevID));

    return AS_ERROR_CODE_OK;
}


/*************************************SIP**********************************************************/
int32_t ASCameraSvrManager::read_sip_conf()
{
    as_ini_config config;
    std::string   strValue="";
    if(INI_SUCCESS != config.ReadIniFile(CAMERASVR_CONF_FILE))
    {
        return AS_ERROR_CODE_FAIL;
    }

    /* Sip LocalIP */
    if(INI_SUCCESS == config.GetValue("SIP_CFG","LocalIP",strValue))
    {
        m_strLocalIP = strValue;
    }
    /* Sip SipPort */
    if(INI_SUCCESS == config.GetValue("SIP_CFG","SipPort",strValue))
    {
        m_usPort = atoi(strValue.c_str());
    }
    /* Sip FireWallIP */
    if(INI_SUCCESS == config.GetValue("SIP_CFG","FireWallIP",strValue))
    {
        m_strFireWallIP = strValue;
    }
    /* Sip Transport */
    if(INI_SUCCESS == config.GetValue("SIP_CFG","Transport",strValue))
    {
        m_strTransPort = strValue;
    }
    /* Sip ProxyAddress */
    if(INI_SUCCESS == config.GetValue("SIP_CFG","ProxyAddress",strValue))
    {
        m_strProxyAddr = strValue;
    }
    /* Sip ProxyPort */
    if(INI_SUCCESS == config.GetValue("SIP_CFG","ProxyPort",strValue))
    {
        m_usProxyPort = atoi(strValue.c_str());
    }
    return AS_ERROR_CODE_OK;
}

void ASCameraSvrManager::osip_trace_log_callback(char *fi, int li, osip_trace_level_t level, char *chfr, va_list ap)
{
    char szLogTmp[ORTP_LOG_LENS_MAX] = { 0 };
#if AS_APP_OS == AS_OS_LINUX
    (void)::vsnprintf(szLogTmp, ORTP_LOG_LENS_MAX, chfr, ap);
#elif AS_APP_OS == AS_OS_WIN32
    (void)::_vsnprintf(szLogTmp, ORTP_LOG_LENS_MAX, chfr, ap);
#endif
    if (OSIP_BUG == level || OSIP_INFO1 == level || OSIP_INFO2 == level
        || OSIP_INFO3 == level || OSIP_INFO4 == level) {
        AS_LOG(AS_LOG_DEBUG, "osip:[%s:%d][%s]", fi, li,szLogTmp);
    }
    else if (OSIP_WARNING == level) {
        AS_LOG(AS_LOG_WARNING, "osip:[%s:%d][%s]", fi, li, szLogTmp);
    }
    else if (OSIP_ERROR == level) {
        AS_LOG(AS_LOG_ERROR, "osip:[%s:%d][%s]", fi, li, szLogTmp);
    }
    else if (OSIP_FATAL == level) {
        AS_LOG(AS_LOG_CRITICAL, "osip:[%s:%d][%s]", fi, li, szLogTmp);
    }
    else {
        AS_LOG(AS_LOG_DEBUG, "osip:[%s:%d][%s]", fi, li, szLogTmp);
    }
}


int32_t ASCameraSvrManager::init_Exosip()
{
    osip_trace_level_t oSipLogLevel = OSIP_WARNING;

    if (AS_LOG_DEBUG == m_ulLogLM) {
        oSipLogLevel = OSIP_INFO4;
    }
    else if (AS_LOG_INFO == m_ulLogLM) {
        oSipLogLevel = OSIP_INFO3;
    }
    else if (AS_LOG_NOTICE == m_ulLogLM) {
        oSipLogLevel = OSIP_INFO1;
    }
    else if (AS_LOG_WARNING == m_ulLogLM) {
        oSipLogLevel = OSIP_WARNING;
    }
    else if (AS_LOG_ERROR == m_ulLogLM) {
        oSipLogLevel = OSIP_ERROR;
    }
    else if (AS_LOG_CRITICAL == m_ulLogLM) {
        oSipLogLevel = OSIP_BUG;
    }
    else if (AS_LOG_ALERT == m_ulLogLM) {
        oSipLogLevel = OSIP_FATAL;
    }
    else if (AS_LOG_EMERGENCY == m_ulLogLM) {
        oSipLogLevel = OSIP_FATAL;
    }
    /* init the sip context */
    m_pEXosipCtx = eXosip_malloc();
    TRACE_ENABLE_LEVEL(oSipLogLevel);
    osip_trace_initialize_func(oSipLogLevel, osip_trace_log_callback);
    if (eXosip_init (m_pEXosipCtx)) {
        return AS_ERROR_CODE_FAIL;
    }
    int32_t lResult = AS_ERROR_CODE_OK;
    if (osip_strcasecmp (m_strTransPort.c_str(), "UDP") == 0) {
        lResult = eXosip_listen_addr (m_pEXosipCtx, IPPROTO_UDP, NULL, m_usPort, AF_INET, 0);
    }
    else if (osip_strcasecmp (m_strTransPort.c_str(), "TCP") == 0) {
        lResult = eXosip_listen_addr (m_pEXosipCtx, IPPROTO_TCP, NULL, m_usPort, AF_INET, 0);
    }
    else if (osip_strcasecmp (m_strTransPort.c_str(), "TLS") == 0) {
        lResult = eXosip_listen_addr (m_pEXosipCtx, IPPROTO_TCP, NULL, m_usPort, AF_INET, 1);
    }
    else if (osip_strcasecmp (m_strTransPort.c_str(), "DTLS") == 0) {
        lResult = eXosip_listen_addr (m_pEXosipCtx, IPPROTO_UDP, NULL, m_usPort, AF_INET, 1);
    }

    if (lResult) {
        AS_LOG(AS_LOG_ERROR, "ASCameraSvrManager::Init,sip listens fail.");
        return AS_ERROR_CODE_FAIL;
    }

    if (0 < m_strLocalIP.length()) {
        eXosip_masquerade_contact (m_pEXosipCtx, m_strLocalIP.c_str(), m_usPort);
    }

    if (0 < m_strFireWallIP.length()) {
        eXosip_masquerade_contact (m_pEXosipCtx, m_strFireWallIP.c_str(), m_usPort);
    }

    eXosip_set_user_agent(m_pEXosipCtx, ALLCAM_AGENT_NAME);

    //eXosip_set_proxy_addr(m_pEXosipCtx,(char*)m_strProxyAddr.c_str(), m_usProxyPort);
    return AS_ERROR_CODE_OK;
}

int32_t ASCameraSvrManager::send_sip_response(eXosip_event_t& rEvent, int32_t nRespCode)
{
    osip_message_t *pAnswer = NULL;
    int32_t nResult = OSIP_SUCCESS;
    /*若对pAnswer无特殊处理，eXosip_message_send_answer中会自动调用eXosip_message_build_answer
    nResult = eXosip_message_build_answer(m_pEXosipCtx, rEvent.tid, nRespCode, &pAnswer);
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Build %s response failed, error code is %d.",
            rEvent.request->sip_method, nResult));
        return -1;
    }
    */

    nResult = eXosip_message_send_answer(m_pEXosipCtx, rEvent.tid, nRespCode, pAnswer);
    if (OSIP_SUCCESS != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "Send %s response failed, error code is %d.",
            rEvent.request->sip_method, nResult);
        return -1;
    }

    AS_LOG(AS_LOG_INFO, "Send %s response success, response code is %d.",
            rEvent.request->sip_method, nRespCode);
    return AS_ERROR_CODE_OK;
}

int32_t ASCameraSvrManager::handleRegisterReq(eXosip_event_t& rEvent)
{
    int32_t nResult = 0;
    int32_t nRespCode = SIP_OK;

    do
    {
        //expires字段
        osip_header_t* pExpires = NULL;
        osip_message_get_expires(rEvent.request, 0, &pExpires);
        if (NULL == pExpires)
        {
            AS_LOG(AS_LOG_ERROR, "Parse %s expires failed.", rEvent.request->sip_method);
            nRespCode = SIP_BAD_REQUEST;
            break;
        }

        if (pExpires->hvalue[0] != '0') //注册
        {
            nResult = handleDeviceRegister(rEvent);
        }
        else    //注销
        {
            nResult = handleDeviceUnRegister(rEvent);
        }

        if (0 != nResult)
        {
            nRespCode = SIP_BAD_REQUEST;
        }

    }while(0);


    return AS_ERROR_CODE_OK;
}
int32_t ASCameraSvrManager::handleDeviceRegister(eXosip_event_t& rEvent)
{
    //contact字段
    osip_contact_t* pContact = NULL;
    osip_message_get_contact(rEvent.request, 0, &pContact);
    if (NULL == pContact)
    {
        AS_LOG(AS_LOG_ERROR, "Parse %s contact failed.", rEvent.request->sip_method);
        send_sip_response(rEvent,SIP_BAD_REQUEST);
        return AS_ERROR_CODE_FAIL;
    }

    osip_via_t *pVia = (osip_via_t *)osip_list_get (&rEvent.request->vias, 0);
    if (NULL == pVia)
    {
        AS_LOG(AS_LOG_ERROR, "Parse %s via failed.", rEvent.request->sip_method);
        send_sip_response(rEvent,SIP_BAD_REQUEST);
        return AS_ERROR_CODE_FAIL;
    }

    osip_generic_param_t *pReceived = NULL;
    osip_generic_param_t *pRport = NULL;
    osip_via_param_get_byname (pVia, "received", &pReceived);
    if (NULL != pReceived)
    {
        osip_via_param_get_byname (pVia, "rport", &pRport);
        if (NULL == pRport)
        {
            AS_LOG(AS_LOG_ERROR, "Parse %s rport failed.", rEvent.request->sip_method);
            send_sip_response(rEvent,SIP_BAD_REQUEST);
            return AS_ERROR_CODE_FAIL;
        }
    }

    ASDevice* pDev = NULL;
    std::string strDevID = pContact->url->username;
    std::string strHost  = "";
    std::string strPort  = "";
    pDev = find_device(strDevID);
    if(NULL == pDev)
    {
        AS_LOG(AS_LOG_ERROR, "find the device %s failed.", pContact->url->username);
        send_sip_response(rEvent,SIP_INTERNAL_SERVER_ERROR);
        return AS_ERROR_CODE_FAIL;
    }
    if (NULL != pReceived)
    {
        strHost = pReceived->gvalue;
        strPort = pRport->gvalue;
    }
    else
    {
        strHost = pContact->url->host;
        strPort = pContact->url->port;
    }
    pDev->setDevInfo(strHost,strPort);



    AS_LOG(AS_LOG_INFO, "Register new user \"%s\", host is %s, port is %s.",
                strDevID.c_str(), strHost.c_str(), strPort.c_str());
    send_sip_response(rEvent,SIP_OK);

    /* send the catalog req */
    if(AS_ERROR_CODE_OK != send_catalog_Req(pDev))
    {
        AS_LOG(AS_LOG_INFO, "send catalog to devr \"%s\" fail.",strDevID.c_str());
    }
    release_device(pDev);
    return AS_ERROR_CODE_OK;
}

int32_t ASCameraSvrManager::handleDeviceUnRegister(eXosip_event_t& rEvent)
{
    //contact字段
    osip_contact_t* pContact = NULL;
    osip_message_get_contact(rEvent.request, 0, &pContact);
    if (NULL == pContact)
    {
        AS_LOG(AS_LOG_ERROR, "Parse %s contact failed.", rEvent.request->sip_method);
        return AS_ERROR_CODE_FAIL;
    }
    std::string strDevID = pContact->url->username;
    ASDevice* pDev = NULL;

    pDev = find_device(strDevID);
    if(NULL == pDev)
    {
        AS_LOG(AS_LOG_ERROR, "find the device %s failed.", pContact->url->username);
        send_sip_response(rEvent,SIP_INTERNAL_SERVER_ERROR);
        return AS_ERROR_CODE_FAIL;
    }
    release_device(pDev);
    release_device(pDev);

    AS_LOG(AS_LOG_INFO, "Unregister user \"%s\".", pContact->url->username);
    return AS_ERROR_CODE_OK;
}

int32_t ASCameraSvrManager::handleMessageReq(eXosip_event_t& rEvent)
{

    int32_t nResult = 0;
    int32_t nRespCode = SIP_OK;
    std::string strDevID;
    ASDevice* pDev = NULL;

    do
    {
        osip_from_t *pFrom = osip_message_get_from(rEvent.request);
        if (NULL == pFrom)
        {
            AS_LOG(AS_LOG_ERROR, "Parse %s from failed.", rEvent.request->sip_method);
            nRespCode = SIP_BAD_REQUEST;
            break;
        }

        strDevID = pFrom->url->username;

        pDev = find_device(strDevID);
        if(NULL == pDev)
        {
            AS_LOG(AS_LOG_ERROR, "find the device %s failed.", pFrom->url->username);
            nRespCode = SIP_NOT_FOUND;
            break;
        }

        osip_body_t *pBody = NULL;
        osip_message_get_body(rEvent.request, 0, &pBody);
        if (NULL == pBody)
        {
            AS_LOG(AS_LOG_ERROR, "Parse %s body failed.", rEvent.request->sip_method);
            nRespCode = SIP_BAD_REQUEST;
            break;
        }

        std::string strMsg = pBody->body;

        pDev->handleMessage(strMsg);

        AS_LOG(AS_LOG_INFO, "Parse %s body OK, content is %s.", rEvent.request->sip_method, pBody->body);
    }while(0);

    if(NULL != pDev)
    {
        release_device(pDev);
    }


    nResult = send_sip_response(rEvent, nRespCode);
    if (0 != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "Response %s failed.", rEvent.request->sip_method);
        return AS_ERROR_CODE_FAIL;
    }

    return AS_ERROR_CODE_OK;
}

int32_t ASCameraSvrManager::send_catalog_Req(ASDevice* pDev)
{
    osip_message_t *pRequest = NULL;
    std::string strFrom = "sip:" + m_strServerID+ "@" + m_strLocalIP;
    if(0 <  m_strFireWallIP.length())
    {
        strFrom = "sip:" + m_strServerID+ "@" + m_strFireWallIP;
    }
    std::string strTo = pDev->getSendTo();
    int32_t nResult = eXosip_message_build_request(m_pEXosipCtx, &pRequest,
                                                  "MESSAGE", strTo.c_str(),
                                                  strFrom.c_str(),NULL);
    if (OSIP_SUCCESS != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "eXosip_message_build_request Message failed, error code is %d.", nResult);
        return -1;
    }
    std::string strMsg = pDev->createQueryCatalog();

    nResult = osip_message_set_body(pRequest, strMsg.c_str(), strMsg.length());
    if (OSIP_SUCCESS != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "osip_message_set_body Message failed, error code is %d.", nResult);
        return -1;
    }
    nResult = osip_message_set_content_type (pRequest, "Application/MANSCDP+xml");
    if (OSIP_SUCCESS != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "osip_message_set_body Message failed, error code is %d.", nResult);
        return -1;
    }

    nResult = eXosip_message_send_request(m_pEXosipCtx, pRequest);
    if (OSIP_SUCCESS >= nResult)    //大于0表示成功
    {
        AS_LOG(AS_LOG_ERROR, "eXosip_message_send_request Message failed, error code is %d.", nResult);
        return -1;
    }

    AS_LOG(AS_LOG_INFO, "eXosip_message_send_request Message success, tid is %d.", nResult);
    return 0;
}




