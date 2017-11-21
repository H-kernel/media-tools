/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2017, Live Networks, Inc.  All rights reserved
// A demo application, showing how to create and run a RTSP client (that can potentially receive multiple streams concurrently).
//
// NOTE: This code - although it builds a running application - is intended only to illustrate how to develop your own RTSP
// client application.  For a full-featured RTSP client application - with much more functionality, and many options - see
// "openRTSP": http://www.live555.com/openRTSP/
#ifdef WIN32
#include "stdafx.h"
#endif
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "as_rtsp_client.h"
#include "RTSPCommon.hh"


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
}

ASRtspClient::~ASRtspClient() {
}

int32_t ASRtspClient::open(as_rtsp_callback_t* cb)
{
    m_cb = cb;
    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
    // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
    // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
    scs.Start();
    return sendOptionsCommand(&ASRtspClientManager::continueAfterOPTIONS);
}
void    ASRtspClient::close()
{
    scs.Stop();
    if (scs.session != NULL) {
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession* subsession;

        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                someSubsessionsWereActive = True;
            }
        }

        if (someSubsessionsWereActive) {
          // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
          // Don't bother handling the response to the "TEARDOWN".
          sendTeardownCommand(*scs.session,&ASRtspClientManager::continueAfterTeardown);
        }
    }

}
double ASRtspClient::getDuration()
{
    return scs.duration;
}

void ASRtspClient::seek(double start)
{
    if(NULL == scs.session)
    {
        return;
    }
    m_dStarttime = scs.session->playStartTime();
    m_dEndTime = scs.session->playEndTime();
    if(start < m_dStarttime)
    {
        return;
    }

    if(start > m_dEndTime)
    {
        return;
    }

    // send the pause first
    sendPauseCommand(*scs.session,&ASRtspClientManager::continueAfterPause);

    // send the play with new start time
    sendPlayCommand(*scs.session, &ASRtspClientManager::continueAfterSeek, start, m_dEndTime);
}
void ASRtspClient::pause()
{
    if(NULL == scs.session)
    {
        return;
    }
    // send the pause first
    sendPauseCommand(*scs.session,&ASRtspClientManager::continueAfterPause);
}
void ASRtspClient::play()
{
    if (AS_RTSP_STATUS_PAUSE != m_curStatus)
    {
        return;
    }
    if(NULL == scs.session)
    {
        return;
    }
    m_dStarttime = scs.session->playStartTime();
    m_dEndTime = scs.session->playEndTime();
    double curTime = 0;
    // send the play with new start time
    sendPlayCommand(*scs.session, &ASRtspClientManager::continueAfterSeek, curTime, m_dEndTime);
}

void ASRtspClient::report_status(int status)
{
    m_curStatus = status;
    if(NULL == m_cb) {
        return;
    }
    if(NULL == m_cb->f_status_cb) {
        return;
    }

    m_cb->f_status_cb(this,status,m_cb->ctx);
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
                subsession->sink = NULL;
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
                subsession->sink = NULL;
                if (sink != NULL) {
                    sink->Stop();
                }
            }
        }
    }
}



ASStreamSink* ASStreamSink::createNew(UsageEnvironment& env, MediaSubsession& subsession,
                                      char const* streamId,as_rtsp_callback_t* cb) {
  return new ASStreamSink(env, subsession, streamId,cb);
}

ASStreamSink::ASStreamSink(UsageEnvironment& env, MediaSubsession& subsession,
                           char const* streamId,as_rtsp_callback_t* cb)
  : MediaSink(env),fSubsession(subsession),m_cb(cb) {
    fStreamId = strDup(streamId);
    fReceiveBuffer = (u_int8_t*)&fMediaBuffer[0];
    prefixSize = 0;

    memset(&m_MediaInfo,0,sizeof(MediaFrameInfo));

    m_MediaInfo.type = AS_RTSP_DATA_TYPE_OTHER;

    if(!strcmp(fSubsession.mediumName(), "video")) {
        fReceiveBuffer = (u_int8_t*)&fMediaBuffer[DUMMY_SINK_H264_STARTCODE_SIZE];
        fMediaBuffer[0] = 0x00;
        fMediaBuffer[1] = 0x00;
        fMediaBuffer[2] = 0x00;
        fMediaBuffer[3] = 0x01;
        prefixSize = DUMMY_SINK_H264_STARTCODE_SIZE;
        m_MediaInfo.type = AS_RTSP_DATA_TYPE_VIDEO;
    }
    else if(!strcmp(fSubsession.mediumName(), "audio")){
        m_MediaInfo.type = AS_RTSP_DATA_TYPE_AUDIO;
    }

    m_MediaInfo.rtpPayloadFormat = fSubsession.rtpPayloadFormat();
    m_MediaInfo.rtpTimestampFrequency = fSubsession.rtpTimestampFrequency();
    m_MediaInfo.codecName = (char*)fSubsession.codecName();
    m_MediaInfo.protocolName = (char*)fSubsession.protocolName();
    m_MediaInfo.videoWidth =fSubsession.videoWidth();
    m_MediaInfo.videoHeight =fSubsession.videoHeight();
    m_MediaInfo.videoFPS =fSubsession.videoFPS();
    m_MediaInfo.numChannels =fSubsession.numChannels();

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

    if(NULL != m_cb) {
        if(NULL != m_cb->f_data_cb) {
            unsigned int size = frameSize + prefixSize;
            m_cb->f_data_cb(&m_MediaInfo,(char*)&fMediaBuffer[0],size,m_cb->ctx);
        }
    }
    // Then continue, to request the next frame of data:
    continuePlaying();
}

Boolean ASStreamSink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
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
    return;
}

u_int32_t ASRtspClientManager::find_beast_thread()
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



AS_HANDLE ASRtspClientManager::openURL(char const* rtspURL,as_rtsp_callback_t* cb) {

    u_int32_t index = find_beast_thread();
    UsageEnvironment* env = m_envArray[index];

    RTSPClient* rtspClient = ASRtspClient::createNew(index,*env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, RTSP_AGENT_NAME);
    if (rtspClient == NULL) {
        return NULL;
    }
    m_clCountArray[index]++;

    ASRtspClient* AsRtspClient = (ASRtspClient*)rtspClient;

    AsRtspClient->open(cb);
    return (AS_HANDLE)AsRtspClient;
}

void      ASRtspClientManager::closeURL(AS_HANDLE handle)
{
    ASRtspClient* pAsRtspClient = (ASRtspClient*)handle;
    u_int32_t index = pAsRtspClient->index();
    pAsRtspClient->close();
    m_clCountArray[index]--;

    return;
}

double ASRtspClientManager::getDuration(AS_HANDLE handle)
{
    ASRtspClient* pAsRtspClient = (ASRtspClient*)handle;
    return pAsRtspClient->getDuration();
}
void ASRtspClientManager::seek(AS_HANDLE handle,double start)
{
    ASRtspClient* pAsRtspClient = (ASRtspClient*)handle;
    pAsRtspClient->seek(start);
    return;
}
void ASRtspClientManager::pause(AS_HANDLE handle)
{
    ASRtspClient* pAsRtspClient = (ASRtspClient*)handle;
    pAsRtspClient->pause();
}
void ASRtspClientManager::play(AS_HANDLE handle)
{
    ASRtspClient* pAsRtspClient = (ASRtspClient*)handle;
    pAsRtspClient->play();
}
void ASRtspClientManager::setRecvBufSize(u_int32_t ulSize)
{
    m_ulRecvBufSize = ulSize;
}
u_int32_t ASRtspClientManager::getRecvBufSize()
{
    return m_ulRecvBufSize;
}



// Implementation of the RTSP 'response handlers':
void ASRtspClientManager::continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString) {

    if(0 != resultCode) {
        shutdownStream(rtspClient);
        return;
    }

    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
        if (resultCode != 0) {
          env << *rtspClient << "Failed to deal options: " << resultString << "\n";
          delete[] resultString;
          break;
        }

        Boolean serverSupportsGetParameter = RTSPOptionIsSupported("GET_PARAMETER", resultString);
        delete[] resultString;
        pAsRtspClient->SupportsGetParameter(serverSupportsGetParameter);

        rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);

}

void ASRtspClientManager::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {


    if(0 != resultCode) {
        shutdownStream(rtspClient);
        return;
    }
    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        ASRtspStreamState& scs = ((ASRtspClient*)rtspClient)->scs; // alias
        ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;
        if (resultCode != 0) {
          env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
          delete[] resultString;
          break;
        }

        char* const sdpDescription = resultString;
        env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        delete[] sdpDescription; // because we don't need it anymore
        if (scs.session == NULL) {
          env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
          break;
        } else if (!scs.session->hasSubsessions()) {
          env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
          break;
        }

        /* report the status */
        pAsRtspClient->report_status(AS_RTSP_STATUS_INIT);

        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession(rtspClient);

        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
}


void ASRtspClientManager::setupNextSubsession(RTSPClient* rtspClient) {
    UsageEnvironment& env = rtspClient->envir(); // alias
    ASRtspStreamState& scs = ((ASRtspClient*)rtspClient)->scs; // alias
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;

    scs.subsession = scs.iter->next();
    if (scs.subsession != NULL) {
        if (!scs.subsession->initiate()) {
            env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
            setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
        } else {
            env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
            if (scs.subsession->rtcpIsMuxed()) {
                env << "client port " << scs.subsession->clientPortNum();
            } else {
                env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
            }
            env << ")\n";

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
            rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
        }
        return;
    }
    /* report the status */
    pAsRtspClient->report_status(AS_RTSP_STATUS_SETUP);
    // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != NULL) {
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
    } else {
        scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
    }

    return;
}

void ASRtspClientManager::continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
    if(0 != resultCode) {
        shutdownStream(rtspClient);
        return;
    }
    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        ASRtspStreamState& scs = ((ASRtspClient*)rtspClient)->scs; // alias
        ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;

        if (resultCode != 0) {
            env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << resultString << "\n";
            break;
        }

        env << *rtspClient << "Set up the \"" << *scs.subsession << "\" subsession (";
        if (scs.subsession->rtcpIsMuxed()) {
            env << "client port " << scs.subsession->clientPortNum();
        } else {
            env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
        }
        env << ")\n";

        // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
        // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
        // after we've sent a RTSP "PLAY" command.)

        scs.subsession->sink = ASStreamSink::createNew(env, *scs.subsession, rtspClient->url(),pAsRtspClient->get_cb());
          // perhaps use your own custom "MediaSink" subclass instead
        if (scs.subsession->sink == NULL) {
            env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
            << "\" subsession: " << env.getResultMsg() << "\n";
            break;
        }

        env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
        scs.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
        scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
                           subsessionAfterPlaying, scs.subsession);
        // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
        if (scs.subsession->rtcpInstance() != NULL) {
            scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
        }
    } while (0);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession(rtspClient);
}

void ASRtspClientManager::continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
    Boolean success = False;
    if(0 != resultCode) {
        shutdownStream(rtspClient);
        return;
    }
    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        ASRtspStreamState& scs = ((ASRtspClient*)rtspClient)->scs; // alias
        ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;

        if (resultCode != 0) {
            env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
            break;
        }

        // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
        // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
        // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
        // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
        if (scs.duration > 0) {
            unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
            scs.duration += delaySlop;
            unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
            scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
        }

        env << *rtspClient << "Started playing session";
        if (scs.duration > 0) {
            env << " (for up to " << scs.duration << " seconds)";
        }
        env << "...\n";

        success = True;
        /* report the status */
        pAsRtspClient->report_status(AS_RTSP_STATUS_PLAY);
        if(pAsRtspClient->SupportsGetParameter()) {
            rtspClient->sendGetParameterCommand(*scs.session,continueAfterGET_PARAMETE, "", NULL);
        }

    } while (0);
    delete[] resultString;

    if (!success) {
        // An unrecoverable error occurred with this stream.
        shutdownStream(rtspClient);
    }
}

void ASRtspClientManager::continueAfterGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString) {
    delete[] resultString;
}

void ASRtspClientManager::continueAfterPause(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    if(0 != resultCode) {
        return;
    }
    ASRtspStreamState& scs = ((ASRtspClient*)rtspClient)->scs; // alias
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;

    pAsRtspClient->report_status(AS_RTSP_STATUS_PAUSE);

    delete[] resultString;
}
void ASRtspClientManager::continueAfterSeek(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    if(0 != resultCode) {
        return;
    }
    ASRtspStreamState& scs = ((ASRtspClient*)rtspClient)->scs; // alias
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;

    pAsRtspClient->report_status(AS_RTSP_STATUS_PLAY);
    delete[] resultString;
}

void ASRtspClientManager::continueAfterTeardown(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;

    shutdownStream(pAsRtspClient,0);
    delete[] resultString;
}



// Implementation of the other event handlers:

void ASRtspClientManager::subsessionAfterPlaying(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

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
    shutdownStream(rtspClient);
}

void ASRtspClientManager::subsessionByeHandler(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
    UsageEnvironment& env = rtspClient->envir(); // alias

    env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";

    // Now act as if the subsession had closed:
    subsessionAfterPlaying(subsession);
}

void ASRtspClientManager::streamTimerHandler(void* clientData) {
    ASRtspClient* rtspClient = (ASRtspClient*)clientData;
    ASRtspStreamState& scs = rtspClient->scs; // alias

    scs.streamTimerTask = NULL;

    // Shut down the stream:
    shutdownStream(rtspClient);
}

void ASRtspClientManager::shutdownStream(RTSPClient* rtspClient, int exitCode) {
    UsageEnvironment& env = rtspClient->envir(); // alias
    ASRtspStreamState& scs = ((ASRtspClient*)rtspClient)->scs; // alias
    ASRtspClient* pAsRtspClient = (ASRtspClient*)rtspClient;

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
          rtspClient->sendTeardownCommand(*scs.session, NULL);
        }
    }

    env << *rtspClient << "Closing the stream.\n";
    /* report the status */
    if(exitCode) {
        pAsRtspClient->report_status(AS_RTSP_STATUS_TEARDOWN);
    }

    /* not close here ,it will be closed by the close URL */
    Medium::close(rtspClient);
    // Note that this will also cause this stream's "ASRtspStreamState" structure to get reclaimed.

}






