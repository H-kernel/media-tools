#ifdef WIN32
#include "stdafx.h"
#endif
#include "liveMedia.hh"
#include "EpollTaskScheduler.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "as_rtsp2rtmp_client.h"
#include "RTSPCommon.hh"
#include "as_lock_guard.h"
#include <time.h>


#if defined(__WIN32__) || defined(_WIN32)
extern "C" int initializeWinsockIfNecessary();
#endif

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

static void rtsp2rtmp_log(int32_t level,const char* szFormat, ...)
{
    va_list args;
    va_start(args, szFormat);
    ASRtsp2RtmpClientManager::instance().write_log(level, szFormat, args);
    va_end(args);
}

ASRtmpHandle::ASRtmpHandle()
{
    m_srsRtmpHandle = NULL;
    m_lSockFD       = InvalidSocket;
}
ASRtmpHandle::~ASRtmpHandle()
{
    
}
int32_t ASRtmpHandle::openHandle(const char* pszUrl)
{
    m_srsRtmpHandle = srs_rtmp_create(pszUrl);
    if(NULL == m_srsRtmpHandle)
    {
        rtsp2rtmp_log(AS_RTSP2RTMP_LOGERROR,"allocate the rtmp handle fail.");
        return -1;
    }
    if (srs_rtmp_handshake(m_srsRtmpHandle) != 0) {
        rtsp2rtmp_log(AS_RTSP2RTMP_LOGERROR,"simple handshake failed.");
        return -1;
    }
    rtsp2rtmp_log(AS_RTSP2RTMP_LOGINFO,"simple handshake success");

    if (srs_rtmp_connect_app(m_srsRtmpHandle) != 0) {
        rtsp2rtmp_log(AS_RTSP2RTMP_LOGERROR,"connect vhost/app failed.");
        return -1;
    }
    rtsp2rtmp_log(AS_RTSP2RTMP_LOGINFO,"connect vhost/app success");

    if (srs_rtmp_publish_stream(m_srsRtmpHandle) != 0) {
        rtsp2rtmp_log(AS_RTSP2RTMP_LOGERROR,"publish stream failed.");
        return -1;
    }

    rtsp2rtmp_log(AS_RTSP2RTMP_LOGINFO,"rtmp stream connect.");

    long lSockFD = (long)srs_rtmp_socket(m_srsRtmpHandle);
    setSockFD(lSockFD);
    if(0 != ASRtsp2RtmpClientManager::instance().reg_rtmp_handle_actor(this))
    {
        rtsp2rtmp_log(AS_RTSP2RTMP_LOGERROR,"register the rtmp socket handle fail.");
        return -1;
    }
    m_lSockFD   = lSockFD;
    srs_rtmp_set_timeout(m_srsRtmpHandle,500,10000);
    setHandleRecv(AS_TRUE);
    return 0;
}
void    ASRtmpHandle::closeHandle()
{
    if(m_srsRtmpHandle)
    {
       srs_rtmp_destroy(m_srsRtmpHandle);
       m_srsRtmpHandle = NULL;
    }
    if(InvalidSocket != m_lSockFD)
    {
        ASRtsp2RtmpClientManager::instance().unreg_rtmp_handle_actor(this);
    }
    rtsp2rtmp_log(AS_RTSP2RTMP_LOGINFO,"rtmp stream close.");
}
void    ASRtmpHandle::close()
{
    /* redefine the virtual function,nothing todo */
}
int32_t ASRtmpHandle::sendH264Frame(char* frames, int frames_size, uint32_t dts, uint32_t pts)
{    
    // send out the h264 packet over RTMP
    int ret = srs_h264_write_raw_frames(m_srsRtmpHandle,frames, frames_size, dts, pts);
    if (ret != 0) {
        if (srs_h264_is_dvbsp_error(ret)) {
            rtsp2rtmp_log(AS_RTSP2RTMP_LOGINFO,"ignore drop video error, code=%d", ret);
        } else if (srs_h264_is_duplicated_sps_error(ret)) {
            rtsp2rtmp_log(AS_RTSP2RTMP_LOGINFO,"ignore duplicated sps, code=%d", ret);
        } else if (srs_h264_is_duplicated_pps_error(ret)) {
            rtsp2rtmp_log(AS_RTSP2RTMP_LOGINFO,"ignore duplicated pps, code=%d", ret);
        } else {
            rtsp2rtmp_log(AS_RTSP2RTMP_LOGWARNING,"send h264 raw data failed. ret=%d", ret);
            return -1;
        }
    }
    return 0;
}
int32_t ASRtmpHandle::sendAacFrame(char sound_format, char sound_rate,char sound_size, char sound_type, 
                        char* frame, int frame_size, uint32_t timestamp)
{    
    int ret = 0;
    if ((ret = srs_audio_write_raw_frame(m_srsRtmpHandle, sound_format, sound_rate, sound_size, sound_type,
                                        frame, frame_size, timestamp)) != 0) {
        rtsp2rtmp_log(AS_RTSP2RTMP_LOGWARNING,"send audio raw data failed. ret=%d", ret);                              
        return 0;
    }
    return 0;
}
long ASRtmpHandle::recv(char *pArrayData, CNetworkAddr *pPeerAddr, const ULONG ulDataSize,const EnumSyncAsync bSyncRecv)
{
    return 0;
}
void ASRtmpHandle::handle_recv(void)
{
    int size;
    char type;
    char* data;
    u_int32_t timestamp;
    rtsp2rtmp_log(AS_RTSP2RTMP_LOGINFO,"ASRtmpHandle::handle_recv.");
        
    if (srs_rtmp_read_packet(m_srsRtmpHandle, &type, &timestamp, &data, &size) == 0) {
        free(data);
    }
        
    setHandleRecv(AS_TRUE);
}
void ASRtmpHandle::handle_send(void)
{
    return;/*nothing to do */
}


void ASRtmpConnMgr::lockListOfHandle()
{
    if (AS_ERROR_CODE_OK != as_mutex_lock(m_pMutexListOfHandle))
    {
        return;
    }
    return;
}
void ASRtmpConnMgr::unlockListOfHandle()
{
    if (AS_ERROR_CODE_OK != as_mutex_unlock(m_pMutexListOfHandle))
    {
        return;
    }
    return;
}
void ASRtmpConnMgr::checkSelectResult(const EpollEventType enEpEvent,CHandle *pHandle)
{
    if(NULL == pHandle)
    {
        return;
    }

    ASRtmpHandle *pRtmpConnHandle = dynamic_cast<ASRtmpHandle *>(pHandle);
    if(NULL == pRtmpConnHandle)
    {
        return;
    }

    //处理读事件
    if(enEpollRead == enEpEvent)
    {
        //清除读事件检测
        pRtmpConnHandle->setHandleRecv(AS_FALSE);
        //调用handle处理接收事件
        pRtmpConnHandle->handle_recv();
    }

    //处理写事件
    if(enEpollWrite == enEpEvent)
    {
        //清除写事件检测
        pRtmpConnHandle->setHandleSend(AS_FALSE);
        //调用handle处理写事件
        pRtmpConnHandle->handle_send();
    }

}
// Implementation of "ASRtsp2RtmpClient":

ASRtsp2RtmpClient* ASRtsp2RtmpClient::createNew(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                    int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new ASRtsp2RtmpClient(ulEnvIndex,env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ASRtsp2RtmpClient::ASRtsp2RtmpClient(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                 int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
  m_ulEnvIndex = ulEnvIndex;
  m_bSupportsGetParameter = False;
  m_dStarttime = 0.0;
  m_dEndTime = 0.0;
  m_curStatus = AS_RTSP_STATUS_INIT;
  m_mutex = as_create_mutex();
  m_bRunning = 0;
  m_bTcp = false;
  m_lLastHeartBeat = time(NULL);
}

ASRtsp2RtmpClient::~ASRtsp2RtmpClient() {
    if (NULL != m_mutex) {
        as_destroy_mutex(m_mutex);
        m_mutex = NULL;
    }
    m_hRtmpHandle.closeHandle();
}
int32_t ASRtsp2RtmpClient::open(char const* rtmpURL)
{
    as_lock_guard locker(m_mutex);
    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
    // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
    // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
    scs.Start();

    m_rtmpUlr = strDup(rtmpURL);

    return sendOptionsCommand(continueAfterOPTIONS);
    //return sendDescribeCommand(continueAfterDESCRIBE);
}

void    ASRtsp2RtmpClient::close()
{
    resetTCPSockets();
    StopClient();
}
u_int32_t ASRtsp2RtmpClient::getStatus()
{
    return m_curStatus;
}

void ASRtsp2RtmpClient::report_status(int status)
{
    m_curStatus = status;
}


void ASRtsp2RtmpClient::handleAfterOPTIONS(int resultCode, char* resultString)
{
    if (0 != resultCode) {
        delete[] resultString;
        /* ignore the options result for redirect */
        sendDescribeCommand(continueAfterDESCRIBE);
        return;
    }

    do {
        Boolean serverSupportsGetParameter = RTSPOptionIsSupported("GET_PARAMETER", resultString);
        delete[] resultString;
        SupportsGetParameter(serverSupportsGetParameter);

        sendDescribeCommand(continueAfterDESCRIBE);
        return;
    } while (0);


}
void ASRtsp2RtmpClient::handleAfterDESCRIBE(int resultCode, char* resultString)
{
    if (NULL == scs.streamTimerTask) {
        m_bRunning = 1;
        unsigned uSecsToDelay = (unsigned)(RTSP_CLIENT_TIME * 1000);
        scs.streamTimerTask = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, this);
    }

    if (0 != resultCode) {
        delete[] resultString;
        return;
    }
    do {
        UsageEnvironment& env = envir(); // alias
        if (resultCode != 0) {
            
            break;
        }

        char* const sdpDescription = resultString;

        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        if (scs.session == NULL) {
            break;
        }
        else if (!scs.session->hasSubsessions()) {
            break;
        }

        /* report the status */
        report_status(AS_RTSP_STATUS_INIT);
        /* connect to the rtmp server */
        if (0 != m_hRtmpHandle.openHandle(m_rtmpUlr)) {
            m_hRtmpHandle.closeHandle();
            report_status(AS_RTSP_STATUS_BREAK);
            break;
        }

        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession();
        delete[] resultString;
        return;
    } while (0);
    delete[] resultString;
    // An unrecoverable error occurred with this stream.
}
void ASRtsp2RtmpClient::setupNextSubsession() {
    UsageEnvironment& env = envir(); // alias

    scs.subsession = scs.iter->next();
    if (scs.subsession != NULL) {
        if (!scs.subsession->initiate()) {
            setupNextSubsession(); // give up on this subsession; go to the next one
        }
        else {

            if (scs.subsession->rtpSource() != NULL) {
                // Because we're saving the incoming data, rather than playing
                // it in real time, allow an especially large time threshold
                // (1 second) for reordering misordered incoming packets:
                unsigned const thresh = 1000000; // 1 second
                scs.subsession->rtpSource()->setPacketReorderingThresholdTime(thresh);

                // Set the RTP source's OS socket buffer size as appropriate - either if we were explicitly asked (using -B),
                // or if the desired FileSink buffer size happens to be larger than the current OS socket buffer size.
                // (The latter case is a heuristic, on the assumption that if the user asked for a large FileSink buffer size,
                // then the input data rate may be large enough to justify increasing the OS socket buffer size also.)
                int socketNum = scs.subsession->rtpSource()->RTPgs()->socketNum();
                unsigned curBufferSize = getReceiveBufferSize(env, socketNum);
                unsigned ulRecvBufSize = ASRtsp2RtmpClientManager::instance().getRecvBufSize();
                if (ulRecvBufSize > curBufferSize) {
                    (void)setReceiveBufferTo(env, socketNum, ulRecvBufSize);
                }
            }

            // Continue setting up this subsession, by sending a RTSP "SETUP" command:
            if (m_bTcp) {
                sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
            }
            else {
                sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_UDP);
            }
        }
        return;
    }

    
    /* report the status */
    report_status(AS_RTSP_STATUS_SETUP);
    // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != NULL) {
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
    }
    else {
        scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
        sendPlayCommand(*scs.session, continueAfterPLAY);
    }

    return;
}
void ASRtsp2RtmpClient::handleAfterSETUP(int resultCode, char* resultString)
{
    if (0 != resultCode) {
        delete []resultString;
        report_status(AS_RTSP_STATUS_BREAK);
        return;
    }
    do {
        UsageEnvironment& env = envir(); // alias

        // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
        // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
        // after we've sent a RTSP "PLAY" command.)

        scs.subsession->sink = ASRtsp2RtmpMediaSink::createNew(env, *scs.subsession,&m_hRtmpHandle, url());
        // perhaps use your own custom "MediaSink" subclass instead
        if (scs.subsession->sink == NULL) {
            break;
        }

        scs.subsession->miscPtr = this; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
        scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
            subsessionAfterPlaying, scs.subsession);
        // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
        if (scs.subsession->rtcpInstance() != NULL) {
            scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
        }
    } while (0);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession();
}
void ASRtsp2RtmpClient::handleAfterPLAY(int resultCode, char* resultString)
{
    Boolean success = False;
    if (0 != resultCode) {
        delete []resultString;
        report_status(AS_RTSP_STATUS_BREAK);
        return;
    }
    do {
        //UsageEnvironment& env = envir(); // alias

        // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
        // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
        // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
        // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
        //if (scs.duration > 0) {
        //    unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
        //    scs.duration += delaySlop;
        //    unsigned uSecsToDelay = (unsigned)(scs.duration * 1000000);
        //    scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, this);
        //}

        success = True;
        /* report the status */
        report_status(AS_RTSP_STATUS_PLAY);
        sendGetParameterCommand(*scs.session, continueAfterGET_PARAMETE, "", NULL);
        /*
        if (SupportsGetParameter()) {
        sendGetParameterCommand(*scs.session, continueAfterGET_PARAMETE, "", NULL);
        }
        else {
        sendOptionsCommand(continueAfterHearBeatOption);
        }
        */

    } while (0);
    delete[] resultString;

    if (!success) {
        // An unrecoverable error occurred with this stream.
    }
}
void ASRtsp2RtmpClient::handleAfterGET_PARAMETE(int resultCode, char* resultString)
{
    delete[] resultString;
}
void ASRtsp2RtmpClient::handleAfterPause(int resultCode, char* resultString)
{
    if (0 != resultCode) {
        delete[] resultString;
        return;
    }

    report_status(AS_RTSP_STATUS_PAUSE);

    delete[] resultString;
}
void ASRtsp2RtmpClient::handleAfterSeek(int resultCode, char* resultString)
{
    if (0 != resultCode) {
        delete[] resultString;
        return;
    }

    report_status(AS_RTSP_STATUS_PLAY);
    delete[] resultString;
}
void ASRtsp2RtmpClient::handleAfterTeardown(int resultCode, char* resultString)
{
    delete[] resultString;
}

void ASRtsp2RtmpClient::handleHeartBeatOption(int resultCode, char* resultString)
{
    if (0 != resultCode) {
        delete[] resultString;
        return;
    }

    do {
        Boolean serverSupportsGetParameter = RTSPOptionIsSupported("GET_PARAMETER", resultString);
        delete[] resultString;
        SupportsGetParameter(serverSupportsGetParameter);
        return;
    } while (0);
    return;
}
void ASRtsp2RtmpClient::handleHeartGET_PARAMETE(int resultCode, char* resultString)
{
    /* noting to do*/
    return;
}

void ASRtsp2RtmpClient::handlesubsessionAfterPlaying(MediaSubsession* subsession)
{
    // Begin by closing this subsession's stream:
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession& session = subsession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subsession = iter.next()) != NULL) {
        if (subsession->sink != NULL) return; // this subsession is still active
    }

    // All subsessions' streams have now been closed, so shutdown the client:
}
void ASRtsp2RtmpClient::handlesubsessionByeHandler(MediaSubsession* subsession)
{
    // Begin by closing this subsession's stream:
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession& session = subsession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subsession = iter.next()) != NULL) {
        if (subsession->sink != NULL) return; // this subsession is still active
    }

    // All subsessions' streams have now been closed, so shutdown the client:
}
void ASRtsp2RtmpClient::handlestreamTimerHandler()
{
    scs.streamTimerTask = NULL;

    // Shut down the stream:
    if ((!checkStop())) {

        shutdownStream();
        return;
    }

    time_t now = time(NULL);
    if ((now > m_lLastHeartBeat) && (10 <= (now - m_lLastHeartBeat))) {
        if (!m_bTcp) {
            if (m_bSupportsGetParameter) {
                sendGetParameterCommand(*scs.session, continueAfterHearBeatGET_PARAMETE, "", NULL);
            }
            else {
                sendOptionsCommand(continueAfterHearBeatOption);
            }
        }
        m_lLastHeartBeat = time(NULL);
    }

    unsigned uSecsToDelay = (unsigned)(RTSP_CLIENT_TIME * 1000);
    scs.streamTimerTask = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, this);
}

// Implementation of the RTSP 'response handlers':
void ASRtsp2RtmpClient::continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleAfterOPTIONS(resultCode, resultString);
}

void ASRtsp2RtmpClient::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleAfterDESCRIBE(resultCode, resultString);
}




void ASRtsp2RtmpClient::continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleAfterSETUP(resultCode, resultString);
}

void ASRtsp2RtmpClient::continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleAfterPLAY(resultCode, resultString);
}

void ASRtsp2RtmpClient::continueAfterGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString) {
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleAfterGET_PARAMETE(resultCode, resultString);
}

void ASRtsp2RtmpClient::continueAfterPause(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleAfterPause(resultCode, resultString);
}
void ASRtsp2RtmpClient::continueAfterSeek(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleAfterSeek(resultCode, resultString);
}

void ASRtsp2RtmpClient::continueAfterTeardown(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleAfterTeardown(resultCode, resultString);
}

void ASRtsp2RtmpClient::continueAfterHearBeatOption(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleHeartBeatOption(resultCode, resultString);

}

void ASRtsp2RtmpClient::continueAfterHearBeatGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handleHeartGET_PARAMETE(resultCode, resultString);
}
// Implementation of the other event handlers:

void ASRtsp2RtmpClient::subsessionAfterPlaying(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;
    pAsRtspClient->handlesubsessionAfterPlaying(subsession);
}

void ASRtsp2RtmpClient::subsessionByeHandler(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)rtspClient;

    // Now act as if the subsession had closed:
    pAsRtspClient->handlesubsessionByeHandler(subsession);
}

void ASRtsp2RtmpClient::streamTimerHandler(void* clientData) {
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)clientData;
    pAsRtspClient->handlestreamTimerHandler();
}

void ASRtsp2RtmpClient::shutdownStream() {
    //UsageEnvironment& env = envir(); // alias


    // First, check whether any subsessions have still to be closed:
    if (scs.session != NULL) {
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession* subsession;

        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                Medium::close(subsession->sink);
                subsession->sink = NULL;
                if (subsession->rtcpInstance() != NULL) {
                    subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
                }
                someSubsessionsWereActive = True;
            }
        }

        if (someSubsessionsWereActive) {
            // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
            // Don't bother handling the response to the "TEARDOWN".
            //sendTeardownCommand(*scs.session, NULL);
        }
    }

    /* report the status */
    if (m_bRunning) {
        report_status(AS_RTSP_STATUS_TEARDOWN);
    }

    /* not close here ,it will be closed by the close URL */
    //Modified by Chris@201712011000;
    // Note that this will also cause this stream's "ASRtsp2RtmpStreamState" structure to get reclaimed.
}

void ASRtsp2RtmpClient::StopClient()
{
    scs.Stop();
    m_bRunning = 0;
    (void)as_mutex_lock(m_mutex);
    shutdownStream();
    if (NULL != scs.streamTimerTask) {
        envir().taskScheduler().unscheduleDelayedTask(scs.streamTimerTask);
        scs.streamTimerTask = NULL;
    }
    (void)as_mutex_unlock(m_mutex);
    Medium::close(this);

}
bool ASRtsp2RtmpClient::checkStop()
{
    as_lock_guard locker(m_mutex);
    if (0 == m_bRunning) {
        return false;
    }
    if(!scs.Check()) {
        return false;
    }
    return true;
}

// Implementation of "ASRtsp2RtmpStreamState":

ASRtsp2RtmpStreamState::ASRtsp2RtmpStreamState()
  : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

ASRtsp2RtmpStreamState::~ASRtsp2RtmpStreamState() {
    delete iter;
    if (session != NULL) {
        // We also need to delete "session", and unschedule "streamTimerTask" (if set)
        UsageEnvironment& env = session->envir(); // alias

        env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
        streamTimerTask = NULL;
        Medium::close(session);
    }
}
void ASRtsp2RtmpStreamState::Start()
{
    if (session != NULL) {
        MediaSubsessionIterator iter(*session);
        MediaSubsession* subsession;
        ASRtsp2RtmpMediaSink* sink = NULL;
        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                sink = (ASRtsp2RtmpMediaSink*)subsession->sink;
                if (sink != NULL) {
                    sink->Start();
                }
            }
        }
    }
}
void ASRtsp2RtmpStreamState::Stop()
{
    if (session != NULL) {
        MediaSubsessionIterator iter(*session);
        MediaSubsession* subsession;
        ASRtsp2RtmpMediaSink* sink = NULL;
        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                sink = (ASRtsp2RtmpMediaSink*)subsession->sink;
                if (sink != NULL) {
                    sink->Stop();
                }
            }
        }
    }
}

bool ASRtsp2RtmpStreamState::Check()
{
    if (session != NULL) {
        MediaSubsessionIterator iter(*session);
        MediaSubsession* subsession;
        ASRtsp2RtmpMediaSink* sink = NULL;
        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                sink = (ASRtsp2RtmpMediaSink*)subsession->sink;
                if (sink != NULL) {
                    if(!sink->Check()) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}




ASRtsp2RtmpMediaSink* ASRtsp2RtmpMediaSink::createNew(UsageEnvironment& env, MediaSubsession& subsession,
    ASRtmpHandle* rtmpHandle,char const* streamId) {
    ASRtsp2RtmpMediaSink* pSink = NULL;
    try{
        pSink = new ASRtsp2RtmpMediaSink(env, subsession,rtmpHandle, streamId);
    }
    catch (...){
        pSink = NULL;
    }
    return pSink;
}

ASRtsp2RtmpMediaSink::ASRtsp2RtmpMediaSink(UsageEnvironment& env, MediaSubsession& subsession,
    ASRtmpHandle* rtmpHandle,char const* streamId)
    : MediaSink(env), fSubsession(subsession),m_rtmpHandle(rtmpHandle) {
    fStreamId            = strDup(streamId);
    fReceiveBuffer       = NULL;
    fMediaBuffer         = NULL;
    ulRecvBufLens        = 0;
    m_hAacEncoder        = NULL;
    m_pAacEncodeBuf      = NULL;
    m_bRunning           = false;
    memset(&m_hEncoderParam,0,sizeof(InitParam));

    if (!strcmp(fSubsession.mediumName(), "video")) {
        fMediaBuffer = new u_int8_t[DUMMY_SINK_VIDEO_RECEIVE_BUFFER_SIZE];
        ulRecvBufLens = DUMMY_SINK_VIDEO_RECEIVE_BUFFER_SIZE;
        if (!strcmp(fSubsession.codecName(), "H264")) {
            fReceiveBuffer = (u_int8_t*)&fMediaBuffer[DUMMY_SINK_H264_STARTCODE_SIZE];
            fMediaBuffer[0] = 0x00;
            fMediaBuffer[1] = 0x00;
            fMediaBuffer[2] = 0x00;
            fMediaBuffer[3] = 0x01;
            prefixSize      = DUMMY_SINK_H264_STARTCODE_SIZE;
            ulRecvBufLens   = DUMMY_SINK_VIDEO_RECEIVE_BUFFER_SIZE -  DUMMY_SINK_H264_STARTCODE_SIZE;
            m_enVideoID = RTMP_CODECID_H264;
        }
        // TODO H265
    }
    else if (!strcmp(fSubsession.mediumName(), "audio")){
        fMediaBuffer   = new u_int8_t[DUMMY_SINK_AUDIO_RECEIVE_BUFFER_SIZE];
        fReceiveBuffer = fMediaBuffer;
        ulRecvBufLens  = DUMMY_SINK_AUDIO_RECEIVE_BUFFER_SIZE;
        m_enVideoID = RTMP_CODECID_AAC;
   
        /* init the aac encoder */
        m_hEncoderParam.u32AudioSamplerate=8000;
        m_hEncoderParam.ucAudioChannel=1;
        m_hEncoderParam.u32PCMBitSize=16;
        do {
            if ( 0 == strcmp(fSubsession.codecName(), "PCMU")) {
                // PCM u-law audio
                m_hEncoderParam.u32AudioSamplerate=8000;
                m_hEncoderParam.ucAudioChannel=1;
                m_hEncoderParam.u32PCMBitSize=16;
                m_hEncoderParam.ucAudioCodec = Law_ULaw;
            }
            else if(0 == strcmp(fSubsession.codecName(), "PCMA")){
                // PCM a-law audio
                m_hEncoderParam.u32AudioSamplerate=8000;
                m_hEncoderParam.ucAudioChannel=1;
                m_hEncoderParam.u32PCMBitSize=16;
                m_hEncoderParam.ucAudioCodec = Law_ALaw;
            }
            else if(0 == strcmp(fSubsession.codecName(), "G726-16")) {
                // G.726, 16 kbps
                m_hEncoderParam.u32AudioSamplerate=8000;
                m_hEncoderParam.ucAudioChannel=1;
                m_hEncoderParam.u32PCMBitSize=16;
                m_hEncoderParam.ucAudioCodec = Law_G726;
                m_hEncoderParam.g726param.ucRateBits =Rate16kBits;
            }
            else if(0 == strcmp(fSubsession.codecName(), "G726-24")) {
                // G.726, 24 kbps
                m_hEncoderParam.u32AudioSamplerate=8000;
                m_hEncoderParam.ucAudioChannel=1;
                m_hEncoderParam.u32PCMBitSize=16;
                m_hEncoderParam.ucAudioCodec = Rate24kBits;
            }
            else if(0 == strcmp(fSubsession.codecName(), "G726-32")) {
                // G.726, 32 kbps
                m_hEncoderParam.u32AudioSamplerate=8000;
                m_hEncoderParam.ucAudioChannel=1;
                m_hEncoderParam.u32PCMBitSize=16;
                m_hEncoderParam.ucAudioCodec = Rate32kBits;
            }
            else if(0 == strcmp(fSubsession.codecName(), "G726-40")) {
                // G.726, 40 kbps
                m_hEncoderParam.u32AudioSamplerate=8000;
                m_hEncoderParam.ucAudioChannel=1;
                m_hEncoderParam.u32PCMBitSize=16;
                m_hEncoderParam.ucAudioCodec = Rate40kBits;
            }
            else {
                break;
            }
            
            //initParam.ucAudioCodec = Law_ULaw;
            m_hAacEncoder   = Easy_AACEncoder_Init(m_hEncoderParam);
            m_pAacEncodeBuf = new u_int8_t[4*DUMMY_SINK_AUDIO_RECEIVE_BUFFER_SIZE];
        } while(false);
    }
    m_bRunning = true;
}

ASRtsp2RtmpMediaSink::~ASRtsp2RtmpMediaSink() {
    if(NULL != fMediaBuffer) {
        delete[] fMediaBuffer;
        fMediaBuffer = NULL;
    }
    fReceiveBuffer = NULL;
    if(NULL == fStreamId) {
        delete[] fStreamId;
        fStreamId = NULL;
    }
    if(NULL != m_hAacEncoder) {
        Easy_AACEncoder_Release(m_hAacEncoder);
        m_hAacEncoder = NULL;
    }
    if(NULL == m_pAacEncodeBuf) {
        delete[] m_pAacEncodeBuf;
        m_pAacEncodeBuf = NULL;
    }
}

void ASRtsp2RtmpMediaSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {
    ASRtsp2RtmpMediaSink* sink = (ASRtsp2RtmpMediaSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ASRtsp2RtmpMediaSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {

    if(!m_bRunning) {
        continuePlaying();
        return;
    }

    if(RTMP_CODECID_H264 == m_enVideoID) {
        sendH264Frame(frameSize,numTruncatedBytes,presentationTime,durationInMicroseconds);
    }
    else if(RTMP_CODECID_H265 == m_enVideoID) {
        sendH265Frame();
    }
    else if(RTMP_CODECID_AAC == m_enVideoID) {
        sendAudioFrame(frameSize,numTruncatedBytes,presentationTime,durationInMicroseconds);
    }
    // Then continue, to request the next frame of data:
    continuePlaying();
}

Boolean ASRtsp2RtmpMediaSink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, ulRecvBufLens,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}

// send the H264 frame
void ASRtsp2RtmpMediaSink::sendH264Frame(unsigned frameSize, unsigned numTruncatedBytes,
             struct timeval presentationTime, unsigned durationInMicroseconds)
{
    if(NULL == m_rtmpHandle) {
        return;
    }
    unsigned int nTimeStamp = presentationTime.tv_sec * 1000 + presentationTime.tv_usec / 1000;
    
    uint32_t ulSize = frameSize + prefixSize;

    int32_t nRet = m_rtmpHandle->sendH264Frame((char*)fMediaBuffer, ulSize, nTimeStamp, nTimeStamp);
    
    if (nRet != 0) {
        rtsp2rtmp_log(AS_RTSP2RTMP_LOGWARNING,"send h264 frame data failed. ret=%d", nRet);
        m_bRunning = false;
    }
    return;
}

// send the H265 frame
void ASRtsp2RtmpMediaSink::sendH265Frame()
{
    //todo:
}
void ASRtsp2RtmpMediaSink::sendAudioFrame(unsigned frameSize, unsigned numTruncatedBytes,
             struct timeval presentationTime, unsigned durationInMicroseconds)
{
    if(NULL == m_rtmpHandle) {
        return;
    }

    if(NULL == m_hAacEncoder || NULL == m_pAacEncodeBuf) {
        return;
    }
    int size = frameSize + prefixSize;
	unsigned int out_len = 0;
  
	if(Easy_AACEncoder_Encode(m_hAacEncoder, fMediaBuffer, size, m_pAacEncodeBuf, &out_len) <= 0)
	{
		return;
	}
    if(0 == out_len)
    {
        return;
    }
    /* send the aac data with rtmp */
    // 0 = Linear PCM, platform endian
    // 1 = ADPCM
    // 2 = MP3
    // 7 = G.711 A-law logarithmic PCM
    // 8 = G.711 mu-law logarithmic PCM
    // 10 = AAC
    // 11 = Speex
    char sound_format = 10;/* AAC */
    // 2 = 22 kHz
    char sound_rate = 2;
    // 1 = 16-bit samples
    char sound_size = 1;
    // 1 = Stereo sound
    char sound_type = 1;
        
    unsigned int nTimeStamp = presentationTime.tv_sec * 1000 + presentationTime.tv_usec / 1000;
    
    int ret = 0;
    if ((ret = m_rtmpHandle->sendAacFrame(sound_format, sound_rate, sound_size, sound_type,
                                         (char*)m_pAacEncodeBuf, out_len, nTimeStamp)) != 0) {
        rtsp2rtmp_log(AS_RTSP2RTMP_LOGWARNING,"send audio raw data failed. ret=%d", ret);                              
        return;
    }
    return;
}

void ASRtsp2RtmpMediaSink::Start()
{
    m_bRunning = true;
}
void ASRtsp2RtmpMediaSink::Stop()
{
    m_bRunning = false;
}
bool ASRtsp2RtmpMediaSink::Check()
{
    return m_bRunning;
}


Rtsp2RtmpEnvironment::Rtsp2RtmpEnvironment(TaskScheduler& taskScheduler)
: BasicUsageEnvironment(taskScheduler) {
}

Rtsp2RtmpEnvironment::~Rtsp2RtmpEnvironment() {
}

Rtsp2RtmpEnvironment*
Rtsp2RtmpEnvironment::createNew(TaskScheduler& taskScheduler) {
  return new Rtsp2RtmpEnvironment(taskScheduler);
}

UsageEnvironment& Rtsp2RtmpEnvironment::operator<<(char const* str) {
  if (str == NULL) str = "(NULL)"; // sanity check
  ASRtsp2RtmpClientManager::instance().rtsp_write_log("%s", str);
  return *this;
}

UsageEnvironment& Rtsp2RtmpEnvironment::operator<<(int i) {
  ASRtsp2RtmpClientManager::instance().rtsp_write_log("%d", i);
  return *this;
}

UsageEnvironment& Rtsp2RtmpEnvironment::operator<<(unsigned u) {
  ASRtsp2RtmpClientManager::instance().rtsp_write_log("%u", u);
  return *this;
}

UsageEnvironment& Rtsp2RtmpEnvironment::operator<<(double d) {
  ASRtsp2RtmpClientManager::instance().rtsp_write_log("%f", d);
  return *this;
}

UsageEnvironment& Rtsp2RtmpEnvironment::operator<<(void* p) {
  ASRtsp2RtmpClientManager::instance().rtsp_write_log("%p", p);
  return *this;
}


ASRtsp2RtmpClientManager::ASRtsp2RtmpClientManager()
{
    m_ulTdIndex     = 0;
    m_LoopWatchVar  = 0;
    m_ulRecvBufSize = RTSP_SOCKET_RECV_BUFFER_SIZE_DEFAULT;
    m_LogCb         = NULL;
    m_nLogLevel     = AS_RTSP2RTMP_LOGWARNING;
}

ASRtsp2RtmpClientManager::~ASRtsp2RtmpClientManager()
{
}

int32_t ASRtsp2RtmpClientManager::init()
{
    // Begin by setting up our usage environment:
    u_int32_t i = 0;
    TaskScheduler* scheduler = NULL;
    UsageEnvironment* env = NULL;

    m_mutex = as_create_mutex();
    if(NULL == m_mutex) {
        return -1;
    }

    for(i = 0;i < RTSP_MANAGE_ENV_MAX_COUNT;i++) {
#if AS_APP_OS == AS_OS_LINUX
        scheduler = EpollTaskScheduler::createNew();
#elif AS_APP_OS == AS_OS_WIN32
        scheduler = BasicTaskScheduler::createNew();
#endif
        env = Rtsp2RtmpEnvironment::createNew(*scheduler);
        m_envArray[i] = env;
        m_clCountArray[i] = 0;
    }
    m_LoopWatchVar = 0;
    for(i = 0;i < RTSP_MANAGE_ENV_MAX_COUNT;i++) {
        if( AS_ERROR_CODE_OK != as_create_thread((AS_THREAD_FUNC)rtsp_env_invoke,
                                                 this,&m_ThreadHandle[i],AS_DEFAULT_STACK_SIZE)) {
            return -1;
        }

    }

    /* start the tcp connect manager for rtmp */
    if(AS_ERROR_CODE_OK != m_TcpConnMgr.init(30)) {
        return -1;
    }
    if(AS_ERROR_CODE_OK != m_TcpConnMgr.run()) {
        return -1;
    }

    return 0;
}
void    ASRtsp2RtmpClientManager::release()
{
    m_LoopWatchVar = 1;
    as_destroy_mutex(m_mutex);
    m_mutex = NULL;
}

void ASRtsp2RtmpClientManager::rtsp_write_log(const char *fmt, ...)
{
    if(NULL == m_LogCb)
    {
        return;
    }

    if(AS_RTSP2RTMP_LOGINFO > m_nLogLevel)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    m_LogCb(AS_RTSP2RTMP_LOGINFO, fmt, args);
    va_end(args);
}

void ASRtsp2RtmpClientManager::write_log(int32_t level, const char *fmt, va_list args)
{
    if(NULL == m_LogCb)
    {
        return;
    }
    if(level > m_nLogLevel)
    {
        return;
    }

    m_LogCb(AS_RTSP2RTMP_LOGINFO, fmt, args);
}

int32_t ASRtsp2RtmpClientManager::reg_rtmp_handle_actor(ASRtmpHandle* pHandle)
{
    
    return m_TcpConnMgr.addHandle(pHandle);
}
void    ASRtsp2RtmpClientManager::unreg_rtmp_handle_actor(ASRtmpHandle* pHandle)
{
    m_TcpConnMgr.removeHandle(pHandle);
}


void *ASRtsp2RtmpClientManager::rtsp_env_invoke(void *arg)
{
    ASRtsp2RtmpClientManager* manager = (ASRtsp2RtmpClientManager*)(void*)arg;
    manager->rtsp_env_thread();
    return NULL;
}
void ASRtsp2RtmpClientManager::rtsp_env_thread()
{
    u_int32_t index = thread_index();

    if(RTSP_MANAGE_ENV_MAX_COUNT <= index) {
        return;
    }

    UsageEnvironment* env = m_envArray[index];
    TaskScheduler* scheduler = &env->taskScheduler();
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
    return;
}

u_int32_t ASRtsp2RtmpClientManager::find_beast_thread()
{

    u_int32_t index = 0;
    u_int32_t count = 0xFFFFFFFF;
    for(u_int32_t i = 0; i < RTSP_MANAGE_ENV_MAX_COUNT;i++) {
        if(count > m_clCountArray[i]) {
            index = i;
            count = m_clCountArray[i];
        }
    }

    return index;
}



AS_HANDLE ASRtsp2RtmpClientManager::openURL(char const* rtspURL,char const* rtmpURL, bool bTcp) {

    as_mutex_lock(m_mutex);
    UsageEnvironment* env = NULL;
    u_int32_t index =  0;

    index = find_beast_thread();
    env = m_envArray[index];

    RTSPClient* rtspClient = ASRtsp2RtmpClient::createNew(index,*env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, RTSP_AGENT_NAME);
    if (rtspClient == NULL) {
        as_mutex_unlock(m_mutex);
        return NULL;
    }
    m_clCountArray[index]++;

    ASRtsp2RtmpClient* pRtsp2RtmpClient = (ASRtsp2RtmpClient*)rtspClient;
    pRtsp2RtmpClient->setMediaTcp(bTcp);
    if (AS_ERROR_CODE_OK == pRtsp2RtmpClient->open(rtmpURL))
    {
        Medium::close(pRtsp2RtmpClient);
        as_mutex_unlock(m_mutex);
        return NULL;
    }
    as_mutex_unlock(m_mutex);
    return (AS_HANDLE)pRtsp2RtmpClient;
}


void      ASRtsp2RtmpClientManager::closeURL(AS_HANDLE handle)
{
    as_mutex_lock(m_mutex);

    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)handle;
    u_int32_t index = pAsRtspClient->index();
    pAsRtspClient->close();
    m_clCountArray[index]--;

    as_mutex_unlock(m_mutex);
    return;
}

u_int32_t  ASRtsp2RtmpClientManager::getStatus(AS_HANDLE handle)
{
    as_mutex_lock(m_mutex);
    ASRtsp2RtmpClient* pAsRtspClient = (ASRtsp2RtmpClient*)handle;
    u_int32_t uStatus = pAsRtspClient->getStatus();
    as_mutex_unlock(m_mutex);
    return uStatus;
}


void ASRtsp2RtmpClientManager::setRecvBufSize(u_int32_t ulSize)
{
    m_ulRecvBufSize = ulSize;
}
u_int32_t ASRtsp2RtmpClientManager::getRecvBufSize()
{
    return m_ulRecvBufSize;
}
void ASRtsp2RtmpClientManager::setLogCallBack(uint32_t nLevel,Rtsp2Rtmp_LogCallback cb)
{
    m_nLogLevel = nLevel;
    m_LogCb     = cb;
}








