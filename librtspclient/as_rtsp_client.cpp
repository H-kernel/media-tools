#ifdef WIN32
#include "stdafx.h"
#endif
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "EpollTaskScheduler.hh"
#include "GroupsockHelper.hh"
#include "as_rtsp_client.h"
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


// Implementation of "ASRtspClient":

ASRtspClient* ASRtspClient::createNew(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                    int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new ASRtspClient(ulEnvIndex,env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ASRtspClient::ASRtspClient(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                 int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
  m_ulEnvIndex = ulEnvIndex;
  m_bSupportsGetParameter = False;
  m_dStarttime = 0.0;
  m_dEndTime = 0.0;
  m_curStatus = AS_RTSP_STATUS_INIT;
  m_mutex = as_create_mutex();
  m_bRunning = 0;
  m_ulTryTime = 0;
  m_bTcp = false;
  m_lLastHeartBeat = time(NULL);
}

ASRtspClient::~ASRtspClient() {
    if (NULL != m_mutex) {
        as_destroy_mutex(m_mutex);
        m_mutex = NULL;
    }

}

int32_t ASRtspClient::open(ASRtspClientHandle* handle)
{
    as_lock_guard locker(m_mutex);
    m_handle = handle;
    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
    // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
    // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
    scs.Start();
    return sendOptionsCommand(continueAfterOPTIONS);
    //return sendDescribeCommand(continueAfterDESCRIBE);
}

void    ASRtspClient::close()
{
    resetTCPSockets();
    StopClient();
}

void ASRtspClient::handleAfterOPTIONS(int resultCode, char* resultString)
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
void ASRtspClient::handleAfterDESCRIBE(int resultCode, char* resultString)
{
    if (NULL == scs.streamTimerTask) {
        m_bRunning = 1;
        unsigned uSecsToDelay = (unsigned)(RTSP_CLIENT_TIME * 1000);
        scs.streamTimerTask = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, this);
    }

    if (0 != resultCode) {
        delete[] resultString;
        m_handle->handleStatus(AS_RTSP_STATUS_BREAK);
        return;
    }
    do {
        UsageEnvironment& env = envir(); // alias

        char* const sdpDescription = resultString;

        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        delete[] sdpDescription; // because we don't need it anymore
        if (scs.session == NULL) {
            break;
        }
        else if (!scs.session->hasSubsessions()) {
            break;
        }

        /* report the status */
        m_handle->handleStatus(AS_RTSP_STATUS_INIT);

        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession();

        return;
    } while (0);

    m_handle->handleStatus(AS_RTSP_STATUS_BREAK);
}
void ASRtspClient::setupNextSubsession() {
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
                unsigned ulRecvBufSize = ASRtspClientManager::instance().getRecvBufSize();
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
    m_handle->handleStatus(AS_RTSP_STATUS_SETUP);
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
void ASRtspClient::handleAfterSETUP(int resultCode, char* resultString)
{
    if (0 != resultCode) {
        delete[] resultString;
        m_handle->handleStatus(AS_RTSP_STATUS_BREAK);
        return;
    }
    do {
        UsageEnvironment& env = envir(); // alias

        // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
        // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
        // after we've sent a RTSP "PLAY" command.)

        scs.subsession->sink = ASStreamSink::createNew(env, *scs.subsession, url(), m_handle);
        // perhaps use your own custom "MediaSink" subclass instead
        if (scs.subsession->sink == NULL) {
            break;
        }

        scs.subsession->miscPtr = this; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
        scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),subsessionAfterPlaying, scs.subsession);
        // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
        if (scs.subsession->rtcpInstance() != NULL) {
            scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
        }
    } while (0);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession();
}
void ASRtspClient::handleAfterPLAY(int resultCode, char* resultString)
{
    if (0 != resultCode) {
        delete[] resultString;
        m_handle->handleStatus(AS_RTSP_STATUS_BREAK);
        return;
    }
    do {

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

        /* report the status */
        m_handle->handleStatus(AS_RTSP_STATUS_PLAY);
     
        if (SupportsGetParameter()) {
            sendGetParameterCommand(*scs.session, continueAfterGET_PARAMETE, "", NULL);
        }

    } while (0);
    delete[] resultString;
}
void ASRtspClient::handleAfterGET_PARAMETE(int resultCode, char* resultString)
{
    delete[] resultString;
}

void ASRtspClient::handleAfterTeardown(int resultCode, char* resultString)
{
    delete[] resultString;
}

void ASRtspClient::handleHeartBeatOption(int resultCode, char* resultString)
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
void ASRtspClient::handleHeartGET_PARAMETE(int resultCode, char* resultString)
{
    /* noting to do*/
    return;
}

void ASRtspClient::handlesubsessionAfterPlaying(MediaSubsession* subsession)
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
void ASRtspClient::handlesubsessionByeHandler(MediaSubsession* subsession)
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
void ASRtspClient::handlestreamTimerHandler()
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
void ASRtspClient::continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
    pAsRtspClient->handleAfterOPTIONS(resultCode, resultString);
}

void ASRtspClient::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
    pAsRtspClient->handleAfterDESCRIBE(resultCode, resultString);
}




void ASRtspClient::continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
    pAsRtspClient->handleAfterSETUP(resultCode, resultString);
}

void ASRtspClient::continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
    pAsRtspClient->handleAfterPLAY(resultCode, resultString);
}

void ASRtspClient::continueAfterGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString) {
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
    pAsRtspClient->handleAfterGET_PARAMETE(resultCode, resultString);
}


void ASRtspClient::continueAfterTeardown(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
    pAsRtspClient->handleAfterTeardown(resultCode, resultString);
}

void ASRtspClient::continueAfterHearBeatOption(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
    pAsRtspClient->handleHeartBeatOption(resultCode, resultString);

}

void ASRtspClient::continueAfterHearBeatGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
    pAsRtspClient->handleHeartGET_PARAMETE(resultCode, resultString);
}
// Implementation of the other event handlers:

void ASRtspClient::subsessionAfterPlaying(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
    pAsRtspClient->handlesubsessionAfterPlaying(subsession);
}

void ASRtspClient::subsessionByeHandler(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;

    // Now act as if the subsession had closed:
    pAsRtspClient->handlesubsessionByeHandler(subsession);
}

void ASRtspClient::streamTimerHandler(void* clientData) {
    ASRtspClient* pAsRtspClient = (ASRtspClient*)clientData;
    pAsRtspClient->handlestreamTimerHandler();
}

void ASRtspClient::shutdownStream() {
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
        m_handle->handleStatus(AS_RTSP_STATUS_TEARDOWN);
    }

    /* not close here ,it will be closed by the close URL */
    //Modified by Chris@201712011000;
    // Note that this will also cause this stream's "ASRtspStreamState" structure to get reclaimed.
}


void ASRtspClient::StopClient()
{
    scs.Stop();
    m_handle   = NULL;
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
bool ASRtspClient::checkStop()
{
    as_lock_guard locker(m_mutex);
    if (0 == m_bRunning) {
        return false;
    }
    return true;
}


// Implementation of "ASRtspStreamState":

ASRtspStreamState::ASRtspStreamState()
  : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

ASRtspStreamState::~ASRtspStreamState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    UsageEnvironment& env = session->envir(); // alias

    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    streamTimerTask = NULL;
    Medium::close(session);
  }
}
void ASRtspStreamState::Start()
{
    if (session != NULL) {
        MediaSubsessionIterator iter(*session);
        MediaSubsession* subsession;
        ASStreamSink* sink = NULL;
        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                sink = (ASStreamSink*)subsession->sink;
                if (sink != NULL) {
                    sink->Start();
                }
            }
        }
    }
}
void ASRtspStreamState::Stop()
{
    if (session != NULL) {
        MediaSubsessionIterator iter(*session);
        MediaSubsession* subsession;
        ASStreamSink* sink = NULL;
        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                sink = (ASStreamSink*)subsession->sink;
                if (sink != NULL) {
                    sink->Stop();
                }
            }
        }
    }
}



ASStreamSink* ASStreamSink::createNew(UsageEnvironment& env, MediaSubsession& subsession,
    char const* streamId, ASRtspClientHandle *handle) {
    ASStreamSink* pSink = NULL;
    try{
        pSink = new ASStreamSink(env, subsession, streamId, handle);
    }
    catch (...){
        pSink = NULL;
    }
    return pSink;
}

ASStreamSink::ASStreamSink(UsageEnvironment& env, MediaSubsession& subsession,
    char const* streamId, ASRtspClientHandle *handle)
    : MediaSink(env), fSubsession(subsession), m_handle(handle) {
    fStreamId = strDup(streamId);

    

    m_bRunning = true;

}

ASStreamSink::~ASStreamSink() {
    fReceiveBuffer = NULL;
    if(NULL == fStreamId) {
        delete[] fStreamId;
        fStreamId = NULL;
    }
}

void ASStreamSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {
    ASStreamSink* sink = (ASStreamSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ASStreamSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {

    if(!m_bRunning) {
        continuePlaying();
        return;
    }

    m_MediaInfo.rtpPayloadFormat = fSubsession.rtpPayloadFormat();
    m_MediaInfo.rtpTimestampFrequency = fSubsession.rtpTimestampFrequency();
    m_MediaInfo.presentationTime = presentationTime;
    m_MediaInfo.codecName = (char*)fSubsession.codecName();
    m_MediaInfo.protocolName = (char*)fSubsession.protocolName();
    m_MediaInfo.videoWidth = fSubsession.videoWidth();
    m_MediaInfo.videoHeight = fSubsession.videoHeight();
    m_MediaInfo.videoFPS = fSubsession.videoFPS();
    m_MediaInfo.numChannels = fSubsession.numChannels();

    if (NULL != m_handle) {
        m_handle->handleMediaData(&m_MediaInfo,(char*)fMediaBuffer,m_ulBufSize);
    }
    // Then continue, to request the next frame of data:
    continuePlaying();
}

Boolean ASStreamSink::continuePlaying() {
    if (fSource == NULL) {
        return False; // sanity check (should not happen)
    }

    int32_t nResult    = AS_RTSP_ERROR_OK;
    uint32_t ulSize    = 0;

    char* pszBuf = NULL;

    if (!strcmp(fSubsession.mediumName(), "video")) {
        nResult = m_handle->allocMediaRecvBuf(AS_RTSP_DATA_TYPE_VIDEO,pszBuf,m_ulBufSize);
        if(AS_RTSP_ERROR_OK != nResult) {
            return False;
        }
        fMediaBuffer = (uint8_t*)pszBuf;
        fMediaBuffer[0] = 0x00;
        fMediaBuffer[1] = 0x00;
        fMediaBuffer[2] = 0x00;
        fMediaBuffer[3] = 0x01;
        fReceiveBuffer = (u_int8_t*)&fMediaBuffer[DUMMY_SINK_H264_STARTCODE_SIZE];
        ulSize  = m_ulBufSize - DUMMY_SINK_H264_STARTCODE_SIZE;
    }
    else if (!strcmp(fSubsession.mediumName(), "audio")){
        
        nResult = m_handle->allocMediaRecvBuf(AS_RTSP_DATA_TYPE_AUDIO,pszBuf,m_ulBufSize);
        if(AS_RTSP_ERROR_OK != nResult) {
            return False;
        }
        fMediaBuffer = (uint8_t*)pszBuf;
        fReceiveBuffer = fMediaBuffer;
        ulSize         = m_ulBufSize;
    }

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, ulSize,afterGettingFrame, this,onSourceClosure, this);
  return True;
}

void ASStreamSink::Start()
{
    m_bRunning = true;
}
void ASStreamSink::Stop()
{
    m_bRunning = false;
}


ASRtspClientManager::ASRtspClientManager()
{
    m_ulTdIndex     = 0;
    m_LoopWatchVar  = 0;
    m_ulRecvBufSize = RTSP_SOCKET_RECV_BUFFER_SIZE_DEFAULT;
}

ASRtspClientManager::~ASRtspClientManager()
{
}

int32_t ASRtspClientManager::init()
{
    // Begin by setting up our usage environment:
    u_int32_t i = 0;

    m_mutex = as_create_mutex();
    if(NULL == m_mutex) {
        return -1;
    }

    for(i = 0;i < RTSP_MANAGE_ENV_MAX_COUNT;i++) {
        m_envArray[i] = NULL;
        m_clCountArray[i] = 0;
    }
    m_LoopWatchVar = 0;
    for(i = 0;i < RTSP_MANAGE_ENV_MAX_COUNT;i++) {
        if( AS_ERROR_CODE_OK != as_create_thread((AS_THREAD_FUNC)rtsp_env_invoke,
                                                    this,&m_ThreadHandle[i],AS_DEFAULT_STACK_SIZE)) {
            return -1;
        }

    }

    return 0;
}
void    ASRtspClientManager::release()
{
    m_LoopWatchVar = 1;
    as_destroy_mutex(m_mutex);
    m_mutex = NULL;
}


void *ASRtspClientManager::rtsp_env_invoke(void *arg)
{
    ASRtspClientManager* manager = (ASRtspClientManager*)(void*)arg;
    manager->rtsp_env_thread();
    return NULL;
}
void ASRtspClientManager::rtsp_env_thread()
{
    u_int32_t index = thread_index();

    TaskScheduler* scheduler = NULL;
    UsageEnvironment* env = NULL;


    if(RTSP_MANAGE_ENV_MAX_COUNT <= index) {
        return;
    }
#if AS_APP_OS == AS_OS_LINUX
    scheduler = EpollTaskScheduler::createNew();
#elif AS_APP_OS == AS_OS_WIN32
    scheduler = BasicTaskScheduler::createNew();
#endif
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
    return;
}

u_int32_t ASRtspClientManager::find_beast_thread()
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

AS_HANDLE ASRtspClientManager::openURL(char const* rtspURL, ASRtspClientHandle* handle, bool bTcp) {

    as_mutex_lock(m_mutex);
    UsageEnvironment* env = NULL;
    u_int32_t index =  0;

    index = find_beast_thread();
    env = m_envArray[index];

    RTSPClient* rtspClient = ASRtspClient::createNew(index,*env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, RTSP_AGENT_NAME);
    if (rtspClient == NULL) {
        as_mutex_unlock(m_mutex);
        return NULL;
    }

    ASRtspClient* AsRtspClient = (ASRtspClient*)rtspClient;
    AsRtspClient->setMediaTcp(bTcp);
    if (AS_ERROR_CODE_OK == AsRtspClient->open(handle))
    {
        Medium::close(AsRtspClient);
        as_mutex_unlock(m_mutex);
        return NULL;
    }
    m_clCountArray[index]++;
    as_mutex_unlock(m_mutex);
    return (AS_HANDLE)AsRtspClient;
}


void      ASRtspClientManager::closeURL(AS_HANDLE handle)
{
    as_mutex_lock(m_mutex);
    TaskScheduler* scheduler = NULL;
    UsageEnvironment* env = NULL;

    ASRtspClient* pAsRtspClient = (ASRtspClient*)handle;
    u_int32_t index = pAsRtspClient->index();
    env = &pAsRtspClient->envir();
    scheduler = &env->taskScheduler();
    pAsRtspClient->close();
    m_clCountArray[index]--;
    as_mutex_unlock(m_mutex);
    return;
}
void ASRtspClientManager::setRecvBufSize(u_int32_t ulSize)
{
    m_ulRecvBufSize = ulSize;
}
u_int32_t ASRtspClientManager::getRecvBufSize()
{
    return m_ulRecvBufSize;
}







