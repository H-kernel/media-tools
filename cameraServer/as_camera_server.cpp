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

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}


// Implementation of "ASRtsp2SipClient":

ASRtspCheckChannel* ASRtspCheckChannel::createNew(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                    int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
    AS_LOG(AS_LOG_INFO,"ASRtspCheckChannel::createNew,url:[%s].",rtspURL);
    return new ASRtspCheckChannel(ulEnvIndex,env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ASRtspCheckChannel::ASRtspCheckChannel(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                 int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
  m_ulEnvIndex     = ulEnvIndex;
  m_bSupportsGetParameter = False;
  m_enStatus       = AS_RTSP_STATUS_INIT;
  m_bObervser      = NULL;
  m_bStop          = False;
  m_ulDuration     = 0;
  m_ulStartTime    = time(NULL);
  m_enCheckResult  = AS_RTSP_CHECK_RESULT_SUCCESS;
}

ASRtspCheckChannel::~ASRtspCheckChannel() {
}

int32_t ASRtspCheckChannel::open(uint32_t ulDuration,ASRtspStatusObervser* observer)
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::open,duration:[%d].",ulDuration);
    m_bObervser = observer;
    m_ulDuration = ulDuration;
    m_ulStartTime= time(NULL);
    return sendOptionsCommand(&ASRtspCheckChannel::continueAfterOPTIONS);
}
void    ASRtspCheckChannel::close()
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::close,begin.");
    m_bStop = True;
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::close,end.");
}


void    ASRtspCheckChannel::handle_after_options(int resultCode, char* resultString)
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_options begin.");

    do {
        if (resultCode != 0) {
            AS_LOG(AS_LOG_WARNING,"ASRtspCheckChannel::handle_after_options,"
                                  "this result:[%d] is not right.",resultCode);
            m_enCheckResult  = AS_RTSP_CHECK_RESULT_OPEN_URL;
            delete[] resultString;
            break;
        }

        Boolean serverSupportsGetParameter = RTSPOptionIsSupported("GET_PARAMETER", resultString);
        delete[] resultString;
        SupportsGetParameter(serverSupportsGetParameter);
        sendDescribeCommand(continueAfterDESCRIBE);
        AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_options end.");
        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream();
    AS_LOG(AS_LOG_WARNING,"ASRtspCheckChannel::handle_after_options exit.");
    return;
}
void    ASRtspCheckChannel::handle_after_describe(int resultCode, char* resultString)
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_describe begin.");
    do {
        if (resultCode != 0) {
            AS_LOG(AS_LOG_WARNING,"ASRtspCheckChannel::handle_after_describe,"
                                  "this result:[%d] is not right.",resultCode);
            m_enCheckResult  = AS_RTSP_CHECK_RESULT_OPEN_URL;
            delete[] resultString;
            break;
        }

        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(envir(), resultString);
        delete[] resultString; // because we don't need it anymore
        if (scs.session == NULL) {
            AS_LOG(AS_LOG_WARNING,"ASRtspCheckChannel::handle_after_describe,"
                                  "create the session fail.");
            break;
        } else if (!scs.session->hasSubsessions()) {
            AS_LOG(AS_LOG_WARNING,"ASRtspCheckChannel::handle_after_describe,"
                                  "this is no sub session.");
            break;
        }

        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession();
        AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_describe end.");
        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream();
    AS_LOG(AS_LOG_WARNING,"ASRtspCheckChannel::handle_after_describe exit.");
    return;
}


void    ASRtspCheckChannel::handle_after_setup(int resultCode, char* resultString)
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_setup begin.");
    if(0 != resultCode) {
        m_enCheckResult  = AS_RTSP_CHECK_RESULT_OPEN_URL;
        shutdownStream();
        return;
    }

    if(scs.session != NULL) {
        /* open the send rtp sik */
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession* subsession;

        while ((subsession = iter.next()) != NULL) {
           if (!strcmp(subsession->mediumName(), "video")) {
                AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_setup,create a video sink.");
                subsession->sink = ASRtspCheckVideoSink::createNew(envir(), *subsession);
           }
            else if (!strcmp(subsession->mediumName(), "audio")){
                AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_setup,create a audio sink.");
                subsession->sink = ASRtspCheckAudioSink::createNew(envir(), *subsession);
           }
           else {
               AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_setup,this is not video and audio sink.");
               subsession->sink = NULL;
               continue;
           }

            // perhaps use your own custom "MediaSink" subclass instead
            if (subsession->sink == NULL) {
                continue;
            }

            subsession->miscPtr = this; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
            AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_setup,the sink start playing.");
            subsession->sink->startPlaying(*(subsession->readSource()),
                             subsessionAfterPlaying, subsession);
            // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
            if (subsession->rtcpInstance() != NULL) {
              subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, subsession);
            }
        }

    }
    if(NULL != resultString) {
        delete[] resultString;
    }
    // Set up the next subsession, if any:
    setupNextSubsession();
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_setup end.");
    return;
}
void    ASRtspCheckChannel::handle_after_play(int resultCode, char* resultString)
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_play begin.");
    Boolean success = False;

    do {

        if (resultCode != 0) {
            m_enCheckResult  = AS_RTSP_CHECK_RESULT_OPEN_URL;
            break;
        }

        // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
        // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
        // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
        // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
        //if (scs.duration > 0) {
            //unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
            //scs.duration += delaySlop;
            //unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
            unsigned uSecsToDelay = (unsigned)(GW_TIMER_CHECK_TASK*1000);
            scs.streamTimerTask = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, this);
        //}

        success = True;
        if(SupportsGetParameter()) {
            sendGetParameterCommand(*scs.session,continueAfterGET_PARAMETE, "", NULL);
        }

    } while (0);
    delete[] resultString;


    if (!success) {
        // An unrecoverable error occurred with this stream.
        AS_LOG(AS_LOG_WARNING,"ASRtspCheckChannel::handle_after_play,play not success.");
        shutdownStream();
    }
    else {
        m_enStatus = AS_RTSP_STATUS_PLAY;
        if( NULL != m_bObervser) {
            m_bObervser->NotifyStatus(AS_RTSP_STATUS_PLAY);
        }
    }
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_play end.");
    return;
}
void    ASRtspCheckChannel::handle_after_teardown(int resultCode, char* resultString)
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_teardown begin.");
    shutdownStream();
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_after_teardown end.");
    return;
}
void    ASRtspCheckChannel::handle_subsession_after_playing(MediaSubsession* subsession)
{
    // Begin by closing this subsession's stream:
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_subsession_after_playing begin.");
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession& session = subsession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subsession = iter.next()) != NULL) {
        if (subsession->sink != NULL) {
            AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_subsession_after_playing end.");
            return; // this subsession is still active
        }
    }

    // All subsessions' streams have now been closed, so shutdown the client:
    m_enCheckResult  = AS_RTSP_CHECK_RESULT_OPEN_URL;
    shutdownStream();
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::handle_subsession_after_playing exit.");
}

void    ASRtspCheckChannel::handle_after_timeout()
{

    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::streamTimerHandler.");
    scs.streamTimerTask = NULL;

    time_t now = time(NULL);

    uint32_t ulPass = (uint32_t)(now - m_ulStartTime);

    // Shut down the stream:
    if((m_bStop)||(ulPass > m_ulDuration)) {
        shutdownStream();
        return;
    }


    // Check the Stream recv Status
    if (scs.session != NULL) {
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession*        subsession;

        while ((subsession = iter.next()) != NULL) {
            if (strcmp(subsession->mediumName(), "video")) {
                continue;
            }
            ASRtspCheckVideoSink* pVideoSink = (ASRtspCheckVideoSink*)subsession->sink;
            if(NULL == pVideoSink) {
                continue;
            }
            if(0 == pVideoSink->getRecvVideoSize())
            {
                AS_LOG(AS_LOG_WARNING,"ASRtspCheckChannel::streamTimerHandler,there is not recv video data.");
                m_enCheckResult  = AS_RTSP_CHECK_RESULT_RECV_DATA;
                shutdownStream();
                return;
            }
        }
    }


    unsigned uSecsToDelay = (unsigned)(GW_TIMER_CHECK_TASK*1000);
    scs.streamTimerTask
       = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                                 (TaskFunc*)streamTimerHandler, this);
    return;
}


void ASRtspCheckChannel::setupNextSubsession() {

    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::setupNextSubsession begin.");
    scs.subsession = scs.iter->next();
    if (scs.subsession != NULL) {
        if (!scs.subsession->initiate()) {
            setupNextSubsession(); // give up on this subsession; go to the next one
        } else {

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
            unsigned curBufferSize = getReceiveBufferSize(envir(), socketNum);
            unsigned ulRecvBufSize = ASCameraSvrManager::instance().getRecvBufSize();
            if (ulRecvBufSize > curBufferSize) {
                (void)setReceiveBufferTo(envir(), socketNum, ulRecvBufSize);
              }
            }

            // Continue setting up this subsession, by sending a RTSP "SETUP" command:
            sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
        }
        AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::setupNextSubsession end.");
        return;
    }

    /* send the play by the control */
    m_enStatus = AS_RTSP_STATUS_SETUP;
    if( NULL != m_bObervser) {
        m_bObervser->NotifyStatus(AS_RTSP_STATUS_SETUP);
    }
    // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != NULL) {
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::setupNextSubsession,send play message,startime:[%s] endtime:[%s].",
                                               scs.session->absStartTime(),scs.session->absEndTime());
        sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
    } else {
        scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
        AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::setupNextSubsession,send play message,duration:[%f].",scs.duration);
        sendPlayCommand(*scs.session, continueAfterPLAY);
    }
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::setupNextSubsession exit.");
    return;
}

void ASRtspCheckChannel::shutdownStream() {
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::shutdownStream begin.");
    // First, check whether any subsessions have still to be closed:
    uint32_t ulDuration  = time(NULL) - m_ulStartTime;
    uint64_t ulVideoRecv = 0;
    uint64_t ulAudioRecv = 0;
    if (scs.session != NULL) {
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession* subsession;

        while ((subsession = iter.next()) != NULL) {
            if (!strcmp(subsession->mediumName(), "video")) {
                ASRtspCheckVideoSink* pVideoSink = (ASRtspCheckVideoSink*)subsession->sink;
                if(NULL != pVideoSink) {
                    ulVideoRecv = pVideoSink->getRecvVideoSize();
                }
            }
            else if (!strcmp(subsession->mediumName(), "audio")){
                ASRtspCheckAudioSink* pAudioSink = (ASRtspCheckAudioSink*)subsession->sink;
                if(NULL != pAudioSink) {
                    ulAudioRecv = pAudioSink->getRecvAudioSize();
                }
            }
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
          AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::shutdownStream,send teardown command.");
          sendTeardownCommand(*scs.session, NULL);
        }
    }

    /* report the status */
    if (NULL != m_bObervser)
    {
        m_enStatus = AS_RTSP_STATUS_TEARDOWN;
        m_bObervser->NotifyRecvData(m_enCheckResult,ulDuration, ulVideoRecv, ulAudioRecv);
        m_bObervser->NotifyStatus(AS_RTSP_STATUS_TEARDOWN);
    }
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::shutdownStream end.");

    /* not close here ,it will be closed by the close URL */
    //Medium::close(rtspClient);
    // Note that this will also cause this stream's "ASRtsp2SipStreamState" structure to get reclaimed.

}



// Implementation of the RTSP 'response handlers':
void ASRtspCheckChannel::continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtspCheckChannel* pAsRtspClient = (ASRtspCheckChannel*)rtspClient;
    pAsRtspClient->handle_after_options(resultCode,resultString);
}

void ASRtspCheckChannel::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtspCheckChannel* pAsRtspClient = (ASRtspCheckChannel*)rtspClient;
    pAsRtspClient->handle_after_describe(resultCode,resultString);

}


void ASRtspCheckChannel::continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtspCheckChannel* pAsRtspClient = (ASRtspCheckChannel*)rtspClient;
    pAsRtspClient->handle_after_setup(resultCode,resultString);
}

void ASRtspCheckChannel::continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
    ASRtspCheckChannel* pAsRtspClient = (ASRtspCheckChannel*)rtspClient;
    pAsRtspClient->handle_after_play(resultCode,resultString);
}

void ASRtspCheckChannel::continueAfterGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString) {
    delete[] resultString;
}

void ASRtspCheckChannel::continueAfterTeardown(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtspCheckChannel* pAsRtspClient = (ASRtspCheckChannel*)rtspClient;

    pAsRtspClient->handle_after_teardown(resultCode, resultString);
    delete[] resultString;
}
// Implementation of the other event handlers:

void ASRtspCheckChannel::subsessionAfterPlaying(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);
    ASRtspCheckChannel* pAsRtspClient = (ASRtspCheckChannel*)rtspClient;

    pAsRtspClient->handle_subsession_after_playing(subsession);
}

void ASRtspCheckChannel::subsessionByeHandler(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    // Now act as if the subsession had closed:
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckChannel::subsessionByeHandler.");
    subsessionAfterPlaying(subsession);
}

void ASRtspCheckChannel::streamTimerHandler(void* clientData) {
    ASRtspCheckChannel* rtspClient = (ASRtspCheckChannel*)clientData;
    rtspClient->handle_after_timeout();
}




// Implementation of "ASRtsp2SipStreamState":

ASRtspCheckStreamState::ASRtspCheckStreamState()
  : iter(NULL), session(NULL), subsession(NULL),
    streamTimerTask(NULL), duration(0.0){
}

ASRtspCheckStreamState::~ASRtspCheckStreamState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    UsageEnvironment& env = session->envir(); // alias

    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    Medium::close(session);
  }
}


ASRtspCheckVideoSink* ASRtspCheckVideoSink::createNew(UsageEnvironment& env, MediaSubsession& subsession) {
    return new ASRtspCheckVideoSink(env, subsession);
}

ASRtspCheckVideoSink::ASRtspCheckVideoSink(UsageEnvironment& env, MediaSubsession& subsession)
  : MediaSink(env),fSubsession(subsession) {
    m_ulRecvSize = 0;
}

ASRtspCheckVideoSink::~ASRtspCheckVideoSink() {
}

void ASRtspCheckVideoSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {
    ASRtspCheckVideoSink* sink = (ASRtspCheckVideoSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ASRtspCheckVideoSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {

    m_ulRecvSize += frameSize;
    continuePlaying();
}

Boolean ASRtspCheckVideoSink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame((u_int8_t*)&fMediaBuffer[0], DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}

ASRtspCheckAudioSink* ASRtspCheckAudioSink::createNew(UsageEnvironment& env, MediaSubsession& subsession) {
  return new ASRtspCheckAudioSink(env, subsession);
}

ASRtspCheckAudioSink::ASRtspCheckAudioSink(UsageEnvironment& env, MediaSubsession& subsession)
  : MediaSink(env),fSubsession(subsession) {
    m_ulRecvSize = 0;
}

ASRtspCheckAudioSink::~ASRtspCheckAudioSink() {
}

void ASRtspCheckAudioSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {
    ASRtspCheckAudioSink* sink = (ASRtspCheckAudioSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ASRtspCheckAudioSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {
    m_ulRecvSize += frameSize;

    continuePlaying();
}

Boolean ASRtspCheckAudioSink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame((u_int8_t*)&fMediaBuffer[0], DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}



ASLensInfo::ASLensInfo()
{
    m_Status = AS_RTSP_CHECK_STATUS_WAIT;
    m_strCameraID = "";
    m_strStreamType = "";
    m_handle = NULL;
    m_time = 0;
    m_ulDuration  = 0;
    m_ulVideoRecv = 0;
    m_ulAudioRecv = 0;
    m_enCheckResult = AS_RTSP_CHECK_RESULT_SUCCESS;
}
ASLensInfo::~ASLensInfo()
{
}
void ASLensInfo::setLensInfo(std::string& strCameraID,std::string& strStreamType)
{
    AS_LOG(AS_LOG_DEBUG,"ASLensInfo::setLensInfo,cameraID:[%s],streamType:[%s].",
                                                strCameraID.c_str(),strStreamType.c_str());
    m_strCameraID = strCameraID;
    m_strStreamType = strStreamType;
}

void ASLensInfo::check()
{
    AS_LOG(AS_LOG_DEBUG,"ASLensInfo::check,status:[%d].",m_Status);
    if(AS_RTSP_CHECK_STATUS_END == m_Status )
    {
        return;
    }
    if(AS_RTSP_CHECK_STATUS_RUN == m_Status)
    {
        /* check the run time and break */
        time_t cur = time(NULL);
        if(cur > (m_time + RTSP_CLINET_RUN_DURATION))
        {
            AS_LOG(AS_LOG_DEBUG,"ASLensInfo::check,the run timeout,so stop the task.");
            stopRtspCheck();
        }
        return;
    }

    /* start the lens check */
    AS_LOG(AS_LOG_DEBUG,"ASLensInfo::check,start the new rtsp .");

    if(AS_ERROR_CODE_OK != StartRtspCheck())
    {
        /* start fail */
        AS_LOG(AS_LOG_DEBUG,"ASLensInfo::check,start the new rtsp fail .");
        m_Status = AS_RTSP_CHECK_STATUS_END;
        return ;
    }
    m_time = time(NULL);

    m_Status = AS_RTSP_CHECK_STATUS_RUN;
}

CHECK_STATUS ASLensInfo::Status()
{
    return m_Status;
}
void ASLensInfo::NotifyStatus(AS_RTSP_STATUS status)
{
    AS_LOG(AS_LOG_DEBUG,"ASLensInfo::NotifyStatus,rtsp status:[%d].",status);
    if(AS_RTSP_STATUS_TEARDOWN == status)
    {
         m_Status = AS_RTSP_CHECK_STATUS_END;
         m_handle = NULL;
    }
}
void ASLensInfo::NotifyRecvData(AS_RTSP_CHECK_RESULT enResult,uint32_t ulDuration,uint64_t ulVideoRecv,uint64_t ulAudioRecv)
{
    m_enCheckResult = enResult;
    m_ulDuration    = ulDuration;
    m_ulVideoRecv   = ulVideoRecv;
    m_ulAudioRecv   = ulAudioRecv;
}


int32_t ASLensInfo::StartRtspCheck()
{
    AS_LOG(AS_LOG_DEBUG,"ASLensInfo::StartRtspCheck begin.");
    ASEvLiveHttpClient httpHandle;
    std::string strRtspUrl;
    int32_t nRet = httpHandle.send_live_url_request(m_strCameraID,m_strStreamType,strRtspUrl);
    if(nRet != AS_ERROR_CODE_OK)
    {
        AS_LOG(AS_LOG_WARNING,"ASLensInfo::StartRtspCheck,get the rtsp url fail.");
        m_enCheckResult = AS_RTSP_CHECK_RESULT_URL_FAIL;
        return AS_ERROR_CODE_FAIL;
    }
    AS_LOG(AS_LOG_INFO,"ASLensInfo::StartRtspCheck,get the rtsp url:[%s].",strRtspUrl.c_str());

    AS_LOG(AS_LOG_DEBUG,"ASLensInfo::StartRtspCheck end.");
    return AS_ERROR_CODE_OK;
}
void    ASLensInfo::stopRtspCheck()
{
    AS_LOG(AS_LOG_DEBUG,"ASLensInfo::stopRtspCheck begin.");

    AS_LOG(AS_LOG_DEBUG,"ASLensInfo::stopRtspCheck end.");
    return;
}


ASRtspCheckTask::ASRtspCheckTask()
{
    m_Status = AS_RTSP_CHECK_STATUS_WAIT;
}
ASRtspCheckTask::~ASRtspCheckTask()
{
    ASLensInfo* pLenInfo = NULL;

    LENSINFOLISTITRT iter = m_LensList.begin();

    for(; iter != m_LensList.end();++iter)
    {
        pLenInfo = *iter;
        if(NULL !=  pLenInfo)
        {
            AS_DELETE(pLenInfo);
        }
    }
    m_LensList.clear();
}
void ASRtspCheckTask::setTaskInfo(std::string& strCheckID,std::string& strReportUrl)
{
    m_strCheckID   = strCheckID;
    m_strReportUrl = strReportUrl;
    AS_LOG(AS_LOG_INFO,"ASRtspCheckTask::setTaskInfo,checkId:[%s],report url:[%s].",
                                          m_strCheckID.c_str(),m_strReportUrl.c_str());
}
void ASRtspCheckTask::addCamera(std::string& strCameraID,std::string& strStreamTye)
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckTask::addCamera begin.");
    ASLensInfo* pLenInfo = NULL;
    pLenInfo = AS_NEW(pLenInfo);
    if(NULL == pLenInfo)
    {
        return;
    }
    pLenInfo->setLensInfo(strCameraID, strStreamTye);
    m_LensList.push_back(pLenInfo);
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckTask::addCamera end.");
}
void ASRtspCheckTask::checkTask()
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckTask::checkTask begin.");
    if(AS_RTSP_CHECK_STATUS_END == m_Status )
    {
        AS_LOG(AS_LOG_DEBUG,"ASRtspCheckTask::checkTask,task is end.");
        return;
    }

    ASLensInfo* pLenInfo = NULL;
    bool bRunning = false;

    LENSINFOLISTITRT iter = m_LensList.begin();

    for(; iter != m_LensList.end();++iter)
    {
        pLenInfo = *iter;
        pLenInfo->check();
        if(AS_RTSP_CHECK_STATUS_END != pLenInfo->Status() )
        {
            bRunning = true;
        }
    }

    if(bRunning)
    {
        AS_LOG(AS_LOG_DEBUG,"ASRtspCheckTask::checkTask,task is go running now.");
        return;
    }

    //report the task info to the server,and end eth task
    ReportTaskStatus();

    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckTask::checkTask,task is stop ,so report to server.");
    m_Status = AS_RTSP_CHECK_STATUS_END;
}
CHECK_STATUS ASRtspCheckTask::TaskStatus()
{
    return m_Status;
}
void ASRtspCheckTask::ReportTaskStatus()
{
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckTask::ReportTaskStatus begin.");
    ASEvLiveHttpClient httpHandle;

    /* 1.build the request xml message */
    XMLDocument msg;
    XMLPrinter printer;
    XMLDeclaration *declare = msg.NewDeclaration();
    XMLElement *report = msg.NewElement("report");
    msg.InsertEndChild(declare);
    msg.InsertEndChild(report);
    report->SetAttribute("version", "1.0");
    XMLElement *check = msg.NewElement("check");
    report->InsertEndChild(check);

    check->SetAttribute("checkid", m_strCheckID.c_str());
    XMLElement *CameraList = msg.NewElement("cameralist");
    check->InsertEndChild(CameraList);

    LENSINFOLISTITRT iter = m_LensList.begin();
    ASLensInfo* pLenInfo = NULL;
    std::string    strCameraID;
    AS_RTSP_CHECK_RESULT       ulCheckResult;
    uint32_t       ulDuration;
    uint64_t       ulVideoRecv;
    uint64_t       ulAudioRecv;
    char           szbuf[RTSP_CHECK_TMP_BUF_SIZE] = {0};

    for(; iter != m_LensList.end();++iter)
    {
        pLenInfo = *iter;
        if(NULL == pLenInfo)
        {
            continue;
        }
        strCameraID   = pLenInfo->getCameraID();
        ulCheckResult = pLenInfo->getResult();
        ulDuration    = pLenInfo->getDuration();
        ulVideoRecv   = pLenInfo->getVideoRecv();
        ulAudioRecv   = pLenInfo->getAudioRecv();
        XMLElement *Camera = msg.NewElement("camera");
        CameraList->InsertEndChild(Camera);

        /*
        Camera->SetAttribute("ID", strCameraID.c_str());
        snprintf(szbuf,RTSP_CHECK_TMP_BUF_SIZE,"%d",ulCheckResult);
        Camera->SetAttribute("result", szbuf);
        snprintf(szbuf,RTSP_CHECK_TMP_BUF_SIZE,"%d",ulDuration);
        Camera->SetAttribute("duration", szbuf);
        snprintf(szbuf,RTSP_CHECK_TMP_BUF_SIZE,"%lld",ulVideoRecv);
        Camera->SetAttribute("video_recv", szbuf);
        snprintf(szbuf,RTSP_CHECK_TMP_BUF_SIZE,"%lld",ulAudioRecv);
        Camera->SetAttribute("audio_recv", szbuf);
        */
    }


    msg.Accept(&printer);
    std::string strRespMsg = printer.CStr();
    /* sent the http request */
    httpHandle.report_check_msg(m_strReportUrl,strRespMsg);
    AS_LOG(AS_LOG_DEBUG,"ASRtspCheckTask::ReportTaskStatus end.");
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


int32_t CManscdp::parse(const char* pszXML)
{
    XMLDocument doc;
    XMLError xmlerr = doc.Parse(pszXML,strlen(pszXML));
    if(XML_SUCCESS != xmlerr)
    {
        AS_LOG(AS_LOG_WARNING, "CManscdp::parse,parse xml msg:[%s] fail.",pszXML);
        return AS_ERROR_CODE_FAIL;
    }


    XMLElement *req = doc.RootElement();
    if (NULL == req)
    {
        AS_LOG(AS_LOG_ERROR, "Parse XML failed, content is %s.", pszXML);
        return AS_ERROR_CODE_FAIL;
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
        AS_LOG(AS_LOG_ERROR, "Parse XML failed, content is %s.", pszXML);
        return AS_ERROR_CODE_FAIL;
    }

    return AS_ERROR_CODE_OK;
}

int32_t CManscdp::parseNotify(const XMLElement &rRoot)
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

int32_t CManscdp::parseResponse(const XMLElement &rRoot)
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

int32_t CManscdp::parseQueryCatalog(const XMLElement &rRoot)
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

int32_t CManscdp::parseDeviceItem(const XMLElement &rItem)
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
    /*
    SVS_DEVICE_INFO stDeviceInfo;
    stDeviceInfo.eDeviceType = SVS_DEV_TYPE_GB28181;
    stDeviceInfo.eDeviceStatus = (0 == strcmp(pStatus->GetText(), "ON"))
                                ? SVS_DEV_STATUS_ONLINE
                                : SVS_DEV_STATUS_OFFLINE;
    strncpy(stDeviceInfo.szDeviceID, pDeviceID->GetText(), sizeof(stDeviceInfo.szDeviceID) - 1);
    strncpy(stDeviceInfo.szDeviceName, pName->GetText(), sizeof(stDeviceInfo.szDeviceName) - 1);

    int32_t nResult = IAccessControlManager::instance().notifyDeviceInfo(stDeviceInfo);
    if (0 != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "Notfiy device info failed.");
        return -1;
    }*/

    return AS_ERROR_CODE_OK;
}

int32_t CManscdp::createQueryCatalog()
{
    try
    {
        XMLDeclaration *declare = m_objXmlDoc.NewDeclaration();
        m_objXmlDoc.LinkEndChild(declare);

        XMLElement *xmlQuery = m_objXmlDoc.NewElement("Query");
        m_objXmlDoc.LinkEndChild(xmlQuery);

        XMLElement *xmlCmdType = m_objXmlDoc.NewElement("CmdType");
        xmlCmdType->SetText("Catalog");
        xmlQuery->LinkEndChild(xmlCmdType);

        XMLElement *xmlSN = m_objXmlDoc.NewElement("SN");
        xmlSN->SetText("17430");
        xmlQuery->LinkEndChild(xmlSN);

        XMLElement *xmlDeviceID = m_objXmlDoc.NewElement("DeviceID");
        xmlSN->SetText("34020000001320000001");
        xmlQuery->LinkEndChild(xmlDeviceID);
    }
    catch(...)
    {
        AS_LOG(AS_LOG_ERROR, "Create query catalog xml failed.");
        return -1;
    }

    return 0;
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
    if(AS_ERROR_CODE_OK != handle_check(post_str,strResp))
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


int32_t ASCameraSvrManager::handle_check(std::string &strReqMsg,std::string &strRespMsg)
{
    std::string strCheckID  = "";

    int32_t ret = AS_ERROR_CODE_OK;

    AS_LOG(AS_LOG_INFO, "ASCameraSvrManager::handle_check,msg:[%s].",strReqMsg.c_str());



    XMLDocument doc;
    XMLError xmlerr = doc.Parse(strReqMsg.c_str(),strReqMsg.length());
    if(XML_SUCCESS != xmlerr)
    {
        AS_LOG(AS_LOG_WARNING, "ASCameraSvrManager::handle_check,parse xml msg:[%s] fail.",strReqMsg.c_str());
        return AS_ERROR_CODE_FAIL;
    }

    XMLElement *req = doc.RootElement();
    if(NULL == req)
    {
        AS_LOG(AS_LOG_WARNING, "ASCameraSvrManager::handle_check,get xml req node fail.");
        return AS_ERROR_CODE_FAIL;
    }

    XMLElement *check = req->FirstChildElement("check");
    if(NULL == check)
    {
        AS_LOG(AS_LOG_WARNING, "ASCameraSvrManager::handle_check,get xml session node fail.");
        return AS_ERROR_CODE_FAIL;
    }




    const char* checkid = check->Attribute("checkid");
    if(NULL == checkid)
    {
        AS_LOG(AS_LOG_WARNING, "ASCameraSvrManager::handle_check,get xml check id fail.");
        return AS_ERROR_CODE_FAIL;
    }
    strCheckID = checkid;

    ret = handle_check_task(check);

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

    SesEle->SetAttribute("checkid",checkid);

    resp.Accept(&printer);
    strRespMsg = printer.CStr();

    AS_LOG(AS_LOG_DEBUG, "ASCameraSvrManager::handle_session,end");
    return AS_ERROR_CODE_OK;
}
int32_t ASCameraSvrManager::handle_check_task(const XMLElement *check)
{
    std::string strReportURL  = "";
    std::string strCheckID    = "";
    std::string strCameraID   = "";
    std::string strStreamType = "";
    //uint32_t    ulInterval    = 0;
    ASRtspCheckTask* task     = NULL;


    const char* checkid = check->Attribute("checkid");
    if(NULL != checkid)
    {
        strCheckID = checkid;
    }

   const XMLElement *report = check->FirstChildElement("report");
    if(NULL == report)
    {
        AS_LOG(AS_LOG_INFO, "ASCameraSvrManager::handle_check_task,get xml report node fail.");
        return AS_ERROR_CODE_FAIL;
    }
    //const char* interval = report->Attribute("interval");
    //if(NULL != interval)
    //{
    //    ulInterval = atoi(interval);
    //}
    const char* url      = report->Attribute("url");
    if(NULL != url)
    {
        strReportURL = url;
    }

    const XMLElement *CameraList = check->FirstChildElement("cameralist");
    if(NULL == CameraList)
    {
        AS_LOG(AS_LOG_WARNING, "ASCameraSvrManager::handle_check_task,get xml cameralist node fail.");
        return AS_ERROR_CODE_FAIL;
    }

    task = AS_NEW(task);
    if(NULL == task)
    {
        AS_LOG(AS_LOG_WARNING, "ASCameraSvrManager::handle_check_task,create task fail.");
        return AS_ERROR_CODE_FAIL;
    }

    task->setTaskInfo(strCheckID,strReportURL);


    const XMLElement *camera = CameraList->FirstChildElement("camera");

    const char* cameraId  = NULL;
    const char* streamType = NULL;
    uint32_t count = 0;
    while(camera)
    {
        cameraId = camera->Attribute("ID");
        streamType = camera->Attribute("streamType");

        if(NULL == cameraId)
        {
            camera = camera->NextSiblingElement();
            continue;
        }
        strCameraID   = cameraId;
        strStreamType = streamType;

        task->addCamera(strCameraID,strStreamType);
        count++;

        camera = camera->NextSiblingElement();
    }


    AS_LOG(AS_LOG_INFO, "ASCameraSvrManager::handle_check_task,CheckID:[%s],camera count:[%d].",
        strCheckID.c_str(), count);

    as_lock_guard locker(m_mutex);

    AS_LOG(AS_LOG_DEBUG, "ASCameraSvrManager::handle_check_task,end");
    return AS_ERROR_CODE_OK;
}

void  ASCameraSvrManager::check_task_status()
{
    ASRtspCheckTask* task     = NULL;

    as_lock_guard locker(m_mutex);
    AS_LOG(AS_LOG_INFO, "ASCameraSvrManager::check_task_status begin.");

    AS_LOG(AS_LOG_DEBUG, "ASCameraSvrManager::check_task_status,end");
}


void ASCameraSvrManager::setRecvBufSize(u_int32_t ulSize)
{
    m_ulRecvBufSize = ulSize;
}
u_int32_t ASCameraSvrManager::getRecvBufSize()
{
    return m_ulRecvBufSize;
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
    /*����pAnswer�����⴦����eXosip_message_send_answer�л��Զ�����eXosip_message_build_answer
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
    bool bRegister = false;

    do
    {
        //expires�ֶ�
        osip_header_t* pExpires = NULL;
        osip_message_get_expires(rEvent.request, 0, &pExpires);
        if (NULL == pExpires)
        {
            AS_LOG(AS_LOG_ERROR, "Parse %s expires failed.", rEvent.request->sip_method);
            nRespCode = SIP_BAD_REQUEST;
            break;
        }

        if (pExpires->hvalue[0] != '0') //ע��
        {
            nResult = handleDeviceRegister(rEvent);
            bRegister = true;
        }
        else    //ע��
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
    //contact�ֶ�
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

    DEVICE_INFO szDeviceInfo;
    if (NULL != pReceived)
    {
        strncpy(szDeviceInfo.szHost, pReceived->gvalue, sizeof(szDeviceInfo.szHost) - 1);
        strncpy(szDeviceInfo.szPort, pRport->gvalue, sizeof(szDeviceInfo.szPort) - 1);
    }
    else
    {
        strncpy(szDeviceInfo.szHost, pContact->url->host, sizeof(szDeviceInfo.szHost) - 1);
        strncpy(szDeviceInfo.szPort, pContact->url->port, sizeof(szDeviceInfo.szPort) - 1);
    }
    strncpy(szDeviceInfo.szUserName, pContact->url->username, sizeof(szDeviceInfo.szUserName) - 1);
    szDeviceInfo.strTo = szDeviceInfo.strTo + szDeviceInfo.szUserName + "@" + szDeviceInfo.szHost + ":" + szDeviceInfo.szPort;

    {
        m_devMap[szDeviceInfo.szUserName] = szDeviceInfo;
    }
    send_sip_response(rEvent,SIP_OK);
    AS_LOG(AS_LOG_INFO, "Register new user \"%s\", host is %s, port is %s.",
            szDeviceInfo.szUserName, szDeviceInfo.szHost, szDeviceInfo.szPort);

    /* send the catalog req */
    return send_catalog_Req(szDeviceInfo);
}

int32_t ASCameraSvrManager::handleDeviceUnRegister(eXosip_event_t& rEvent)
{
    //contact�ֶ�
    osip_contact_t* pContact = NULL;
    osip_message_get_contact(rEvent.request, 0, &pContact);
    if (NULL == pContact)
    {
        AS_LOG(AS_LOG_ERROR, "Parse %s contact failed.", rEvent.request->sip_method);
        return -1;
    }

    {
        m_devMap.erase(pContact->url->username);
    }

    AS_LOG(AS_LOG_INFO, "Unregister user \"%s\".", pContact->url->username);
    return 0;
}

int32_t ASCameraSvrManager::handleMessageReq(eXosip_event_t& rEvent)
{

    int32_t nResult = 0;
    int32_t nRespCode = SIP_OK;

    do
    {
        osip_from_t *pFrom = osip_message_get_from(rEvent.request);
        if (NULL == pFrom)
        {
            AS_LOG(AS_LOG_ERROR, "Parse %s from failed.", rEvent.request->sip_method);
            nRespCode = SIP_BAD_REQUEST;
            break;
        }

        {
            if (m_devMap.find(pFrom->url->username) == m_devMap.end())
            {
                AS_LOG(AS_LOG_INFO, "User \"%s\" hasn't register.", pFrom->url->username);
                nRespCode = SIP_NOT_FOUND;
                break;
            }
        }

        osip_body_t *pBody = NULL;
        osip_message_get_body(rEvent.request, 0, &pBody);
        if (NULL == pBody)
        {
            AS_LOG(AS_LOG_ERROR, "Parse %s body failed.", rEvent.request->sip_method);
            nRespCode = SIP_BAD_REQUEST;
            break;
        }

        CManscdp objManscdp;
        nResult = objManscdp.parse(pBody->body);
        if (0 != nResult)
        {
            AS_LOG(AS_LOG_INFO, "Parse %s body failed, content is %s.", rEvent.request->sip_method, pBody->body);
            nRespCode = SIP_BAD_REQUEST;
            break;
        }

        AS_LOG(AS_LOG_INFO, "Parse %s body OK, content is %s.", rEvent.request->sip_method, pBody->body);
    }while(0);


    nResult = send_sip_response(rEvent, nRespCode);
    if (0 != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "Response %s failed.", rEvent.request->sip_method);
        return -1;
    }

    return 0;
}

int32_t ASCameraSvrManager::send_catalog_Req(DEVICE_INFO& devInfo)
{
    osip_message_t *pRequest = NULL;
    std::string strFrom = "sip:" + m_strServerID+ "@" + m_strLocalIP;
    if(0 <  m_strFireWallIP.length())
    {
        strFrom = "sip:" + m_strServerID+ "@" + m_strFireWallIP;
    }
    int32_t nResult = eXosip_message_build_request(m_pEXosipCtx, &pRequest,
                                                  "MESSAGE", devInfo.strTo.c_str(),
                                                  strFrom.c_str(),NULL);
    if (OSIP_SUCCESS != nResult)
    {
        AS_LOG(AS_LOG_ERROR, "eXosip_message_build_request Message failed, error code is %d.", nResult);
        return -1;
    }

    char szBody[1024] = {0};
    snprintf(szBody, sizeof(szBody), "<?xml version=\"1.0\"?>\
                                     <Query>\
                                       <CmdType>Catalog</CmdType>\
                                       <SN>17430</SN>\
                                       <DeviceID>%s</DeviceID>\
                                     </Query>",devInfo.szUserName);

    nResult = osip_message_set_body(pRequest, szBody, strlen(szBody));
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
    if (OSIP_SUCCESS >= nResult)    //����0��ʾ�ɹ�
    {
        AS_LOG(AS_LOG_ERROR, "eXosip_message_send_request Message failed, error code is %d.", nResult);
        return -1;
    }

    AS_LOG(AS_LOG_INFO, "eXosip_message_send_request Message success, tid is %d.", nResult);
    return 0;
}



