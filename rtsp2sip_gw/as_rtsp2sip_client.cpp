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
#include "stdafx.h"
#include "liveMedia.hh"
#include "RTSPCommon.hh"
#include "GroupsockHelper.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "as_rtsp2sip_client.h"
#include "RTSPCommon.hh"
#include "as_log.h"
#include "as_lock_guard.h"
#include "as_ini_config.h"
#include "as_timer.h"
#include "as_mem.h"



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
#define SEND_BY_ORTP

ASRtsp2RtpChannel* ASRtsp2RtpChannel::createNew(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                    int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new ASRtsp2RtpChannel(ulEnvIndex,env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ASRtsp2RtpChannel::ASRtsp2RtpChannel(u_int32_t ulEnvIndex,UsageEnvironment& env, char const* rtspURL,
                 int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
  m_ulEnvIndex = ulEnvIndex;
  m_bSupportsGetParameter = False;
  m_pObserver = NULL;
  m_nCallId   = 0;
  m_nTransID  = 0;
  m_LocalPorts = NULL;
}

ASRtsp2RtpChannel::~ASRtsp2RtpChannel() {
}

int32_t ASRtsp2RtpChannel::open(int nCallId,int nTransID,CRtpPortPair* local_ports,IRtspChannelObserver* pObserver)
{
    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
    // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
    // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
    m_pObserver = pObserver;
    m_LocalPorts = local_ports;
    m_nCallId = nCallId;
    m_nTransID = nTransID;

    return sendOptionsCommand(&ASRtsp2RtpChannel::continueAfterOPTIONS);
}
void    ASRtsp2RtpChannel::close()
{
    shutdownStream(0);
}

void ASRtsp2RtpChannel::play()
{
    Groupsock* rtpGroupsock = NULL;
    if((NULL != m_LocalPorts)&&(m_DestinInfo.bSet())&&(scs.session != NULL)) {
        /* open the send rtp sik */
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession* subsession;

        while ((subsession = iter.next()) != NULL) {
#ifdef SEND_BY_ORTP
           if(!strcmp(scs.subsession->mediumName(), "video")) {
               subsession->sink = ASRtsp2SipVideoSink::createNew(envir(), *subsession,m_LocalPorts,&m_DestinInfo);
           }
           else if(!strcmp(scs.subsession->mediumName(), "audio")){
               subsession->sink = ASRtsp2SipAudioSink::createNew(envir(), *subsession,m_LocalPorts,&m_DestinInfo);
           }
           else {
               subsession->sink = NULL;
               continue;
           }

#else
            // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
            // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
            // after we've sent a RTSP "PLAY" command.)
            if(!strcmp(subsession->mediumName(), "video")) {
                rtpGroupsock = scs.m_VideoGp;
            }
            else if(!strcmp(subsession->mediumName(), "audio")) {
                rtpGroupsock = scs.m_AudioGp;
            }
            else {
                continue;
            }
            subsession->sink = createNewRTPSink(*subsession,rtpGroupsock);
#endif
            // perhaps use your own custom "MediaSink" subclass instead
            if (subsession->sink == NULL) {
                continue;
            }

            subsession->miscPtr = this; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
            subsession->sink->startPlaying(*(subsession->readSource()),
                             subsessionAfterPlaying, subsession);
            // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
            if (subsession->rtcpInstance() != NULL) {
              subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, subsession);
            }
        }

    }

    /* send play command */
    // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != NULL) {
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
    } else {
        scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
        sendPlayCommand(*scs.session, continueAfterPLAY);
    }
}
void    ASRtsp2RtpChannel::SetDestination(CRtpDestinations& des)
{
    m_DestinInfo.init(des.ServerVideoAddr(),des.ServerVideoPort(),
                      des.ServerAudioAddr(),des.ServerAudioPort());
}

void    ASRtsp2RtpChannel::handle_after_options(int resultCode, char* resultString)
{
    if(0 != resultCode) {
        shutdownStream();
        return;
    }

    do {
        if (resultCode != 0) {
          delete[] resultString;
          break;
        }

        Boolean serverSupportsGetParameter = RTSPOptionIsSupported("GET_PARAMETER", resultString);
        delete[] resultString;
        SupportsGetParameter(serverSupportsGetParameter);
        if(NULL != m_pObserver)
        {
            m_pObserver->OnOptions(m_nCallId);
        }
        sendDescribeCommand(continueAfterDESCRIBE);
        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream();
    return;
}
void    ASRtsp2RtpChannel::handle_after_describe(int resultCode, char* resultString)
{
    if(0 != resultCode) {
        shutdownStream();
        return;
    }
    do {
        if (resultCode != 0) {
          delete[] resultString;
          break;
        }

        m_strRtspSdp = resultString;
        if(NULL != m_pObserver)
        {
            m_pObserver->OnDescribe(m_nCallId,m_nTransID,m_strRtspSdp);
        }
        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(envir(), m_strRtspSdp.c_str());
        delete[] resultString; // because we don't need it anymore
        if (scs.session == NULL) {
          break;
        } else if (!scs.session->hasSubsessions()) {
          break;
        }

        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession();

        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream();

    return;
}


void    ASRtsp2RtpChannel::handle_after_setup(int resultCode, char* resultString)
{
    if(0 != resultCode) {
        shutdownStream();
        return;
    }
    do {

        if (resultCode != 0) {
            break;
        }
    } while (0);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession();
    return;
}
void    ASRtsp2RtpChannel::handle_after_play(int resultCode, char* resultString)
{

    Boolean success = False;
    if(0 != resultCode) {
        shutdownStream();
        return;
    }
    do {

        if (resultCode != 0) {
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
            scs.streamTimerTask = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, this);
        }

        success = True;
        if(NULL != m_pObserver)
        {
            m_pObserver->OnPlay(m_nCallId);
        }
        if(SupportsGetParameter()) {
            sendGetParameterCommand(*scs.session,continueAfterGET_PARAMETE, "", NULL);
        }

    } while (0);
    delete[] resultString;

    if (!success) {
        // An unrecoverable error occurred with this stream.
        shutdownStream();
    }
    return;
}
void    ASRtsp2RtpChannel::handle_after_teardown(int resultCode, char* resultString)
{
    if(NULL != m_pObserver)
    {
        m_pObserver->OnTearDown(m_nCallId);
    }
    return;
}
void    ASRtsp2RtpChannel::handle_subsession_after_playing(MediaSubsession* subsession)
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
    shutdownStream();
}

RTPSink* ASRtsp2RtpChannel
::createNewRTPSink(MediaSubsession& subsession,Groupsock* rtpGroupsock) {

  char const* fCodecName = subsession.codecName();
  unsigned char rtpPayloadTypeIfDynamic = subsession.rtpPayloadFormat();
  FramedSource* inputSource = subsession.readSource();

  // Create (and return) the appropriate "RTPSink" object for our codec:
  // (Note: The configuration string might not be correct if a transcoder is used. FIX!) #####
  RTPSink* newSink;
  if (strcmp(fCodecName, "AC3") == 0 || strcmp(fCodecName, "EAC3") == 0) {
    newSink = AC3AudioRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                     subsession.rtpTimestampFrequency());
#if 0 // This code does not work; do *not* enable it:
  } else if (strcmp(fCodecName, "AMR") == 0 || strcmp(fCodecName, "AMR-WB") == 0) {
    Boolean isWideband = strcmp(fCodecName, "AMR-WB") == 0;
    newSink = AMRAudioRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                     isWideband, fClientMediaSubsession.numChannels());
#endif
  } else if (strcmp(fCodecName, "DV") == 0) {
    newSink = DVVideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
  } else if (strcmp(fCodecName, "GSM") == 0) {
    newSink = GSMAudioRTPSink::createNew(envir(), rtpGroupsock);
  } else if (strcmp(fCodecName, "H263-1998") == 0 || strcmp(fCodecName, "H263-2000") == 0) {
    newSink = H263plusVideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                          subsession.rtpTimestampFrequency());
  } else if (strcmp(fCodecName, "H264") == 0) {
    newSink = H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                      subsession.fmtp_spropparametersets());
  } else if (strcmp(fCodecName, "H265") == 0) {
    newSink = H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                      subsession.fmtp_spropvps(),
                      subsession.fmtp_spropsps(),
                      subsession.fmtp_sproppps());
  } else if (strcmp(fCodecName, "JPEG") == 0) {
    newSink = SimpleRTPSink::createNew(envir(), rtpGroupsock, 26, 90000, "video", "JPEG",
                       1/*numChannels*/, False/*allowMultipleFramesPerPacket*/, False/*doNormalMBitRule*/);
  } else if (strcmp(fCodecName, "MP4A-LATM") == 0) {
    newSink = MPEG4LATMAudioRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                           subsession.rtpTimestampFrequency(),
                           subsession.fmtp_config(),
                           subsession.numChannels());
  } else if (strcmp(fCodecName, "MP4V-ES") == 0) {
    newSink = MPEG4ESVideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                         subsession.rtpTimestampFrequency(),
                         subsession.attrVal_unsigned("profile-level-id"),
                         subsession.fmtp_config());
  } else if (strcmp(fCodecName, "MPA") == 0) {
    newSink = MPEG1or2AudioRTPSink::createNew(envir(), rtpGroupsock);
  } else if (strcmp(fCodecName, "MPA-ROBUST") == 0) {
    newSink = MP3ADURTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
  } else if (strcmp(fCodecName, "MPEG4-GENERIC") == 0) {
    newSink = MPEG4GenericRTPSink::createNew(envir(), rtpGroupsock,
                         rtpPayloadTypeIfDynamic, subsession.rtpTimestampFrequency(),
                         subsession.mediumName(),
                         subsession.attrVal_str("mode"),
                         subsession.fmtp_config(), subsession.numChannels());
  } else if (strcmp(fCodecName, "MPV") == 0) {
    newSink = MPEG1or2VideoRTPSink::createNew(envir(), rtpGroupsock);
  } else if (strcmp(fCodecName, "OPUS") == 0) {
    newSink = SimpleRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                       48000, "audio", "OPUS", 2, False/*only 1 Opus 'packet' in each RTP packet*/);
  } else if (strcmp(fCodecName, "T140") == 0) {
    newSink = T140TextRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
  } else if (strcmp(fCodecName, "THEORA") == 0) {
    newSink = TheoraVideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
        subsession.fmtp_config());
  } else if (strcmp(fCodecName, "VORBIS") == 0) {
    newSink = VorbisAudioRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                        subsession.rtpTimestampFrequency(), subsession.numChannels(),
                        subsession.fmtp_config());
  } else if (strcmp(fCodecName, "VP8") == 0) {
    newSink = VP8VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
  } else if (strcmp(fCodecName, "VP9") == 0) {
    newSink = VP9VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
  } else if (strcmp(fCodecName, "AMR") == 0 || strcmp(fCodecName, "AMR-WB") == 0) {
    // Proxying of these codecs is currently *not* supported, because the data received by the "RTPSource" object is not in a
    // form that can be fed directly into a corresponding "RTPSink" object.
    return NULL;
  } else if (strcmp(fCodecName, "QCELP") == 0 ||
         strcmp(fCodecName, "H261") == 0 ||
         strcmp(fCodecName, "H263-1998") == 0 || strcmp(fCodecName, "H263-2000") == 0 ||
         strcmp(fCodecName, "X-QT") == 0 || strcmp(fCodecName, "X-QUICKTIME") == 0) {
    return NULL;
  } else {
    // This codec is assumed to have a simple RTP payload format that can be implemented just with a "SimpleRTPSink":
    Boolean allowMultipleFramesPerPacket = True; // by default
    Boolean doNormalMBitRule = True; // by default
    // Some codecs change the above default parameters:
    if (strcmp(fCodecName, "MP2T") == 0) {
      doNormalMBitRule = False; // no RTP 'M' bit
    }
    newSink = SimpleRTPSink::createNew(envir(), rtpGroupsock,
                       rtpPayloadTypeIfDynamic, subsession.rtpTimestampFrequency(),
                       subsession.mediumName(), fCodecName,
                       subsession.numChannels(), allowMultipleFramesPerPacket, doNormalMBitRule);
  }

  // Because our relayed frames' presentation times are inaccurate until the input frames have been RTCP-synchronized,
  // we temporarily disable RTCP "SR" reports for this "RTPSink" object:
  newSink->enableRTCPReports() = False;

  // Also tell our "PresentationTimeSubsessionNormalizer" object about the "RTPSink", so it can enable RTCP "SR" reports later:
  PresentationTimeSubsessionNormalizer* ssNormalizer;
  if (strcmp(fCodecName, "H264") == 0 ||
      strcmp(fCodecName, "H265") == 0 ||
      strcmp(fCodecName, "MP4V-ES") == 0 ||
      strcmp(fCodecName, "MPV") == 0 ||
      strcmp(fCodecName, "DV") == 0) {
    // There was a separate 'framer' object in front of the "PresentationTimeSubsessionNormalizer", so go back one object to get it:
    ssNormalizer = (PresentationTimeSubsessionNormalizer*)(((FramedFilter*)inputSource)->inputSource());
  } else {
    ssNormalizer = (PresentationTimeSubsessionNormalizer*)inputSource;
  }
  ssNormalizer->setRTPSink(newSink);

  return newSink;
}

void ASRtsp2RtpChannel::setupNextSubsession() {

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
            unsigned ulRecvBufSize = ASRtsp2SiptManager::instance().getRecvBufSize();
            if (ulRecvBufSize > curBufferSize) {
                (void)setReceiveBufferTo(envir(), socketNum, ulRecvBufSize);
              }
            }

            // Continue setting up this subsession, by sending a RTSP "SETUP" command:
            sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
        }
        return;
    }

    if(NULL != m_pObserver)
    {
        m_pObserver->OnSetUp(m_nCallId,m_nTransID,m_LocalPorts);
    }

    /* send the play by the control */

    return;
}

void ASRtsp2RtpChannel::shutdownStream(int exitCode) {

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
          sendTeardownCommand(*scs.session, NULL);
        }
    }

    /* report the status */
    if(exitCode) {
        handle_after_teardown(0,NULL);
    }

    /* not close here ,it will be closed by the close URL */
    //Medium::close(rtspClient);
    // Note that this will also cause this stream's "ASRtsp2SipStreamState" structure to get reclaimed.

}



// Implementation of the RTSP 'response handlers':
void ASRtsp2RtpChannel::continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtsp2RtpChannel* pAsRtspClient = (ASRtsp2RtpChannel*)rtspClient;
    pAsRtspClient->handle_after_options(resultCode,resultString);
}

void ASRtsp2RtpChannel::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtsp2RtpChannel* pAsRtspClient = (ASRtsp2RtpChannel*)rtspClient;
    pAsRtspClient->handle_after_describe(resultCode,resultString);

}


void ASRtsp2RtpChannel::continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {

    ASRtsp2RtpChannel* pAsRtspClient = (ASRtsp2RtpChannel*)rtspClient;
    pAsRtspClient->handle_after_setup(resultCode,resultString);
}

void ASRtsp2RtpChannel::continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
    ASRtsp2RtpChannel* pAsRtspClient = (ASRtsp2RtpChannel*)rtspClient;
    pAsRtspClient->handle_after_play(resultCode,resultString);
}

void ASRtsp2RtpChannel::continueAfterGET_PARAMETE(RTSPClient* rtspClient, int resultCode, char* resultString) {
    delete[] resultString;
}


// Implementation of the other event handlers:

void ASRtsp2RtpChannel::subsessionAfterPlaying(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);
    ASRtsp2RtpChannel* pAsRtspClient = (ASRtsp2RtpChannel*)rtspClient;

    pAsRtspClient->handle_subsession_after_playing(subsession);
}

void ASRtsp2RtpChannel::subsessionByeHandler(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    // Now act as if the subsession had closed:
    subsessionAfterPlaying(subsession);
}

void ASRtsp2RtpChannel::streamTimerHandler(void* clientData) {
    ASRtsp2RtpChannel* rtspClient = (ASRtsp2RtpChannel*)clientData;
    ASRtsp2SipStreamState& scs = rtspClient->scs; // alias

    scs.streamTimerTask = NULL;

    // Shut down the stream:
    rtspClient->shutdownStream();
}




// Implementation of "ASRtsp2SipStreamState":

ASRtsp2SipStreamState::ASRtsp2SipStreamState()
  : iter(NULL), session(NULL), subsession(NULL),
    streamTimerTask(NULL), duration(0.0),
    m_VideoRtpGp(NULL),m_VideoRtcpGp(NULL),
    m_AudioRtpGp(NULL),m_AudioRtcpGp(NULL),
    m_VideoRTCPInstance(NULL),m_AudioRTCPInstance(NULL){
}

ASRtsp2SipStreamState::~ASRtsp2SipStreamState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    UsageEnvironment& env = session->envir(); // alias

    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    Medium::close(session);
  }
}
void ASRtsp2SipStreamState::open(CRtpPortPair* local_ports)
{
    if(NULL == local_ports) {
        return;
    }
    /*
    Groupsock*               m_VideoRtpGp;
    Groupsock*               m_VideoRtcpGp;
    Groupsock*               m_AudioRtpGp;
    Groupsock*               m_AudioRtcpGp;
    RTCPInstance*            m_VideoRTCPInstance;
    RTCPInstance*            m_AudioRTCPInstance;
    

    NoReuse dummy(envir()); // ensures that we skip over ports that are already in use
    for (portNumBits serverPortNum = fInitialPortNum; ; ++serverPortNum) {
      struct in_addr dummyAddr; dummyAddr.s_addr = 0;

      serverRTPPort = serverPortNum;
      rtpGroupsock = createGroupsock(dummyAddr, serverRTPPort);
      if (rtpGroupsock->socketNum() < 0) {
        delete rtpGroupsock;
        continue; // try again
      }

      // Create a separate 'groupsock' object (with the next (odd) port number) for RTCP:
      serverRTCPPort = ++serverPortNum;
      rtcpGroupsock = createGroupsock(dummyAddr, serverRTCPPort);
      if (rtcpGroupsock->socketNum() < 0) {
      delete rtpGroupsock;
      delete rtcpGroupsock;
      continue; // try again
    }

      break; // success
    }
    */
}


ASRtsp2SipVideoSink* ASRtsp2SipVideoSink::createNew(UsageEnvironment& env, MediaSubsession& subsession,
                                  CRtpPortPair* local_ports,CRtpDestinations* des) {
    return new ASRtsp2SipVideoSink(env, subsession,local_ports,des);
}

ASRtsp2SipVideoSink::ASRtsp2SipVideoSink(UsageEnvironment& env, MediaSubsession& subsession,
                                  CRtpPortPair* local_ports,CRtpDestinations* des)
  : MediaSink(env),fSubsession(subsession) {
    fReceiveBuffer = (u_int8_t*)&fMediaBuffer[0];
    prefixSize = 0;
    m_pVideoSession = NULL;

    fReceiveBuffer = (u_int8_t*)&fMediaBuffer[DUMMY_SINK_H264_STARTCODE_SIZE];
    fMediaBuffer[0] = 0x00;
    fMediaBuffer[1] = 0x00;
    fMediaBuffer[2] = 0x00;
    fMediaBuffer[3] = 0x01;
    prefixSize = DUMMY_SINK_H264_STARTCODE_SIZE;


    m_pVideoSession = rtp_session_new(RTP_SESSION_SENDONLY);

    rtp_session_set_scheduling_mode(m_pVideoSession,1);
    rtp_session_set_blocking_mode(m_pVideoSession,0);
    rtp_session_set_local_addr(m_pVideoSession, 0, local_ports->getVRtpPort(), local_ports->getVRtcpPort());
    rtp_session_set_remote_addr_full (m_pVideoSession,des->ServerVideoAddr().c_str(), des->ServerVideoPort(), des->ServerVideoAddr().c_str(), des->ServerVideoPort()+1);
    rtp_session_enable_adaptive_jitter_compensation(m_pVideoSession,1);
    rtp_session_set_jitter_compensation(m_pVideoSession,40);
    rtp_session_set_payload_type(m_pVideoSession,fSubsession.rtpPayloadFormat());

}

ASRtsp2SipVideoSink::~ASRtsp2SipVideoSink() {
    fReceiveBuffer = NULL;
     if(NULL != m_pVideoSession)
    {
        rtp_session_destroy(m_pVideoSession);
        m_pVideoSession = NULL;
    }

}

void ASRtsp2SipVideoSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {
    ASRtsp2SipVideoSink* sink = (ASRtsp2SipVideoSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ASRtsp2SipVideoSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {

    unsigned int size = frameSize + prefixSize;
    rtp_session_send_with_ts(m_pVideoSession,(uint8_t *)&fMediaBuffer[0],size,10);
    continuePlaying();
}

Boolean ASRtsp2SipVideoSink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}

ASRtsp2SipAudioSink* ASRtsp2SipAudioSink::createNew(UsageEnvironment& env, MediaSubsession& subsession,
                                  CRtpPortPair* local_ports,CRtpDestinations* des) {
  return new ASRtsp2SipAudioSink(env, subsession,local_ports,des);
}

ASRtsp2SipAudioSink::ASRtsp2SipAudioSink(UsageEnvironment& env, MediaSubsession& subsession,
                                  CRtpPortPair* local_ports,CRtpDestinations* des)
  : MediaSink(env),fSubsession(subsession) {

    m_pAudioSession = NULL;

    m_pAudioSession = rtp_session_new(RTP_SESSION_SENDONLY);

    rtp_session_set_scheduling_mode(m_pAudioSession,1);
    rtp_session_set_blocking_mode(m_pAudioSession,0);
    rtp_session_set_local_addr(m_pAudioSession, 0, local_ports->getARtpPort(), local_ports->getARtcpPort());
    rtp_session_set_remote_addr_full (m_pAudioSession,des->ServerAudioAddr().c_str(), des->ServerAudioPort(), des->ServerAudioAddr().c_str(), des->ServerAudioPort()+1);
    rtp_session_enable_adaptive_jitter_compensation(m_pAudioSession,1);
    rtp_session_set_jitter_compensation(m_pAudioSession,20);
    rtp_session_set_payload_type(m_pAudioSession,fSubsession.rtpPayloadFormat());
}

ASRtsp2SipAudioSink::~ASRtsp2SipAudioSink() {
    if(NULL != m_pAudioSession)
    {
        rtp_session_destroy(m_pAudioSession);
        m_pAudioSession = NULL;
    }
}

void ASRtsp2SipAudioSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {
    ASRtsp2SipAudioSink* sink = (ASRtsp2SipAudioSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ASRtsp2SipAudioSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {

    /*
    m_MediaInfo.rtpPayloadFormat = fSubsession.rtpPayloadFormat();
    m_MediaInfo.rtpTimestampFrequency = fSubsession.rtpTimestampFrequency();
    m_MediaInfo.presentationTime = presentationTime;
    m_MediaInfo.codecName = (char*)fSubsession.codecName();
    m_MediaInfo.protocolName = (char*)fSubsession.protocolName();
    m_MediaInfo.videoWidth = fSubsession.videoWidth();
    m_MediaInfo.videoHeight = fSubsession.videoHeight();
    m_MediaInfo.videoFPS = fSubsession.videoFPS();
    m_MediaInfo.numChannels = fSubsession.numChannels();
    */
    // Then continue, to request the next frame of data:
    rtp_session_send_with_ts(m_pAudioSession,(uint8_t *)&fMediaBuffer[0],frameSize,10);
    continuePlaying();
}

Boolean ASRtsp2SipAudioSink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame((u_int8_t*)&fMediaBuffer[0], DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}




CSipSession::CSipSession()
{
    m_mutex         = NULL;
    m_enStatus      = SIP_SESSION_STATUS_ADD;
    m_bRegister     = false;
    m_nRegID        = -1;
    m_strUsername   = "";
    m_strPasswd     = "";
    m_strDomain     = "";
    m_strRealM      = "";
    m_strCameraID   = "";
    m_strStreamType = "";
    m_ulRepInterval = GW_REPORT_DEFAULT;
    m_strReportUrl  = "";
}


CSipSession::~CSipSession()
{
}

void CSipSession::Init()
{
    m_mutex = as_create_mutex();
    return;
}

void CSipSession::SetSipRegInfo(bool bRegister,std::string &strUsername,std::string &strPasswd,
                               std::string &strDomain,std::string &strRealM)
{
    m_bRegister = bRegister;
    m_strUsername = strUsername;
    m_strPasswd   = strPasswd;
    m_strDomain   = strDomain;
    m_strRealM    = strRealM;
    return;
}

void CSipSession::SetCameraInfo(std::string &strCameraID,std::string &strStreamType)
{
    m_strCameraID = strCameraID;
    m_strStreamType = strStreamType;
    return;
}

SIP_SESSION_STATUS CSipSession::SessionStatus()
{
    as_lock_guard locker(m_mutex);
    return m_enStatus;
}
void CSipSession::SessionStatus(SIP_SESSION_STATUS enStatus)
{
    as_lock_guard locker(m_mutex);
    m_enStatus = enStatus;
}
int32_t CSipSession::handle_invite(int nCallId,int nTransID,CRtpPortPair* local_ports,sdp_message_t *remote_sdp/* = NULL */)
{
    sdp_connection_t *audio_con  = NULL;
    sdp_media_t      *md_audio   = NULL;
    sdp_connection_t *video_con  = NULL;
    sdp_media_t      *md_video   = NULL;
    CRtpDestinations  dest;

    std::string strRtspUrl = "";

    as_lock_guard locker(m_mutex);
    /* get the rtsp play url first */

    /* create the rtsp live session */
    u_int32_t index = ASRtsp2SiptManager::instance().find_beast_thread();
    UsageEnvironment* env = ASRtsp2SiptManager::instance().get_env(index);
    RTSPClient* rtspClient = ASRtsp2RtpChannel::createNew(index,*env, strRtspUrl.c_str(),
                                     RTSP_CLIENT_VERBOSITY_LEVEL, RTSP_AGENT_NAME);
    if (rtspClient == NULL) {
        ASRtsp2SiptManager::instance().releas_env(index);
        return NULL;
    }

    ASRtsp2RtpChannel* AsRtspChannel = (ASRtsp2RtpChannel*)rtspClient;

    if(NULL != remote_sdp) {
        //set the remote info
        audio_con = eXosip_get_audio_connection(remote_sdp);
        md_audio = eXosip_get_audio_media(remote_sdp);
        video_con = eXosip_get_video_connection(remote_sdp);
        md_video = eXosip_get_video_media(remote_sdp);
        std::string strVideoAddr = "";
        std::string strAudioAddr = "";
        unsigned short usVideoPort = 0;
        unsigned short usAudioPort = 0;
        if(video_con) {
            strVideoAddr = video_con->c_addr;
            usVideoPort = atoi(md_video->m_port);
        }
        if(audio_con) {
            strAudioAddr = audio_con->c_addr;
            usAudioPort = atoi(md_audio->m_port);
        }
        dest.init(strVideoAddr,usVideoPort, strAudioAddr,usAudioPort);
        AsRtspChannel->SetDestination(dest);
    }

    AsRtspChannel->open(nCallId, nTransID,local_ports, this);

    /* bind the rtsp channel */
    m_callRtspMap.insert(CALLRTSPCHANNELMAP::value_type(nCallId,AsRtspChannel));

    return AS_ERROR_CODE_OK;
}

void    CSipSession::handle_bye(int nCallId)
{
    as_lock_guard locker(m_mutex);
    CALLRTSPCHANNELMAP::iterator iter = m_callRtspMap.find(nCallId);
    if(iter == m_callRtspMap.end())
    {
        return;
    }

    ASRtsp2RtpChannel* AsRtspChannel = iter->second;

    AsRtspChannel->close();
    Medium::close(AsRtspChannel);
    m_callRtspMap.erase(iter);
    return;
}
void    CSipSession::handle_ack(int nCallId,sdp_message_t *remote_sdp/* = NULL */)
{
    sdp_connection_t *audio_con  = NULL;
    sdp_media_t      *md_audio   = NULL;
    sdp_connection_t *video_con  = NULL;
    sdp_media_t      *md_video   = NULL;
    CRtpDestinations  dest;

    as_lock_guard locker(m_mutex);
    CALLRTSPCHANNELMAP::iterator iter = m_callRtspMap.find(nCallId);
    if(iter == m_callRtspMap.end())
    {
        return;
    }

    ASRtsp2RtpChannel* AsRtspChannel = iter->second;
    if(NULL != remote_sdp) {
        //set the remote info
        audio_con = eXosip_get_audio_connection(remote_sdp);
        md_audio = eXosip_get_audio_media(remote_sdp);
        video_con = eXosip_get_video_connection(remote_sdp);
        md_video = eXosip_get_video_media(remote_sdp);
        std::string strVideoAddr = "";
        std::string strAudioAddr = "";
        unsigned short usVideoPort = 0;
        unsigned short usAudioPort = 0;
        if(video_con) {
            strVideoAddr = video_con->c_addr;
            usVideoPort = atoi(md_video->m_port);
        }
        if(audio_con) {
            strAudioAddr = audio_con->c_addr;
            usAudioPort = atoi(md_audio->m_port);
        }
        dest.init(strVideoAddr,usVideoPort, strAudioAddr,usAudioPort);
        AsRtspChannel->SetDestination(dest);
    }

    AsRtspChannel->play();
}

void    CSipSession::close_all()
{
    as_lock_guard locker(m_mutex);
    ASRtsp2RtpChannel* AsRtspChannel =  NULL;
    CALLRTSPCHANNELMAP::iterator iter = m_callRtspMap.begin();
    for(;iter != m_callRtspMap.end();++iter)
    {
        AsRtspChannel = iter->second;
        AsRtspChannel->close();
        Medium::close(AsRtspChannel);
    }
    m_callRtspMap.clear();
    return;
}
void CSipSession::OnOptions(int nCallId)
{
    // nothing to do
    return;
}
void CSipSession::OnDescribe(int nCallId,int nTransID,std::string& sdp)
{
    // save the sdp info
    m_callRtspSdpMap.insert(TRANSSDPMAP::value_type(nTransID,sdp));
    return;
}
void CSipSession::OnSetUp(int nCallId,int nTransID,CRtpPortPair* local_ports)
{
    // send the sip 200 OK
    std::string strsdp = "";
    TRANSSDPMAP::iterator iter = m_callRtspSdpMap.find(nTransID);
    if(iter != m_callRtspSdpMap.end())
    {
        strsdp = iter->second;
    }
    ASRtsp2SiptManager::instance().send_invit_200_ok(nTransID, local_ports,strsdp);
    return;
}
void CSipSession::OnPlay(int nCallId)
{
    // nothing to do
    return;
}
void CSipSession::OnTearDown(int nCallId)
{
    // send the sip bye
    return;
}

void CSipSessionTimer::onTrigger(void *pArg, ULONGLONG ullScales, TriggerStyle enStyle)
{
    ASRtsp2SiptManager::instance().check_all_sip_session();
    return;
}


ASRtsp2SiptManager::ASRtsp2SiptManager()
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
    m_ulRtpStartPort   = GW_RTP_PORT_START;
    m_ulRtpEndPort     = GW_RTP_PORT_END;
    memset(m_ThreadHandle,0,sizeof(as_thread_t*)*RTSP_MANAGE_ENV_MAX_COUNT);
    memset(m_envArray,0,sizeof(UsageEnvironment*)*RTSP_MANAGE_ENV_MAX_COUNT);
    memset(m_clCountArray,0,sizeof(u_int32_t)*RTSP_MANAGE_ENV_MAX_COUNT);
    m_ulLogLM          = AS_LOG_WARNING;
    m_pEXosipCtx       = NULL;
    m_strLocalIP       = "";
    m_usPort           = GW_SIP_PORT_DEFAULT;
    m_strFireWallIP    = "";
    m_strTransPort     = "";
    m_strProxyAddr     = "";
    m_usProxyPort      = GW_SIP_PORT_DEFAULT;
    m_strAppID         = "";
    m_strAppSecret     = "";
    m_strAppKey        = "";
    m_strAppKey        = "";
}

ASRtsp2SiptManager::~ASRtsp2SiptManager()
{
}

int32_t ASRtsp2SiptManager::init()
{

    /* read the system config file */
    if (AS_ERROR_CODE_OK != read_system_conf()) {
        return AS_ERROR_CODE_FAIL;
    }

    /* start the log module */
    ASSetLogLevel(m_ulLogLM);
    ASSetLogFilePathName(RTSP2SIP_LOG_FILE);
    ASStartLog();

    /* init the port pair */
    if(AS_ERROR_CODE_OK != init_port_pairs()) {
        return AS_ERROR_CODE_FAIL;
    }

    m_mutex = as_create_mutex();
    if(NULL == m_mutex) {
        return AS_ERROR_CODE_FAIL;
    }


    /* init the sip context */
    m_pEXosipCtx = eXosip_malloc();
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
        AS_LOG(AS_LOG_ERROR, "ASRtsp2SiptManager::Init,sip listens fail.");
        return AS_ERROR_CODE_FAIL;
    }

    if (0 < m_strLocalIP.length()) {
        eXosip_masquerade_contact (m_pEXosipCtx, m_strLocalIP.c_str(), m_usPort);
    }

    if (0 < m_strFireWallIP.length()) {
        eXosip_masquerade_contact (m_pEXosipCtx, m_strFireWallIP.c_str(), m_usPort);
    }

    eXosip_set_user_agent(m_pEXosipCtx, RTSP_AGENT_NAME);

    eXosip_set_proxy_addr(m_pEXosipCtx,(char*)m_strProxyAddr.c_str(), m_usProxyPort);

    /* init the timer manage */

    if(AS_ERROR_CODE_OK != as_timer::instance().init(GW_TIMER_SCALE))
    {
        AS_LOG(AS_LOG_ERROR, "ASRtsp2SiptManager::Init,init the timer fail.");
        return AS_ERROR_CODE_FAIL;
    }


    return AS_ERROR_CODE_OK;
}
void    ASRtsp2SiptManager::release()
{

    m_LoopWatchVar = 1;
    if(NULL != m_pEXosipCtx)
    {
        eXosip_quit (m_pEXosipCtx);
        osip_free (m_pEXosipCtx);
        m_pEXosipCtx = NULL;
    }
    as_destroy_mutex(m_mutex);
    m_mutex = NULL;
    ASStopLog();
}

int32_t ASRtsp2SiptManager::open()
{

    // Begin by setting up our usage environment:
    u_int32_t i = 0;

    /* run the timer */
    if (AS_ERROR_CODE_OK != as_timer::instance().run()) {
        return AS_ERROR_CODE_FAIL;
    }

    /* registe the sip session check timer */
    if (AS_ERROR_CODE_OK != as_timer::instance().registerTimer(&m_SipSessionTimer,
                                     this, GW_TIMER_SIPSESSION,enRepeated)) {
        return AS_ERROR_CODE_FAIL;
    }

    m_LoopWatchVar = 0;
    /* start the http server deal thread */
    if (AS_ERROR_CODE_OK != as_create_thread((AS_THREAD_FUNC)http_env_invoke,
        this, &m_HttpThreadHandle, AS_DEFAULT_STACK_SIZE)) {
        return AS_ERROR_CODE_FAIL;
    }
    /* start the sip client register thread */
    if (AS_ERROR_CODE_OK != as_create_thread((AS_THREAD_FUNC)sip_env_invoke,
        this, &m_SipThreadHandle, AS_DEFAULT_STACK_SIZE)) {
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
            return AS_ERROR_CODE_FAIL;
        }

    }

    return 0;
}

void ASRtsp2SiptManager::close()
{
    as_timer::instance().exit();
    m_LoopWatchVar = 1;

    return;
}


int32_t ASRtsp2SiptManager::read_system_conf()
{
    as_ini_config config;
    std::string   strValue="";
    if(INI_SUCCESS != config.ReadIniFile(RTSP2SIP_CONF_FILE))
    {
        return AS_ERROR_CODE_FAIL;
    }

    /* log level */
    if(INI_SUCCESS == config.GetValue("LOG_CFG","LogLM",strValue))
    {
        m_ulLogLM = atoi(strValue.c_str());
    }

    /* http listen port */
    if(INI_SUCCESS == config.GetValue("LISTEN_PORT","ListenPort",strValue))
    {
        m_httpListenPort = atoi(strValue.c_str());
    }

    /* rtp port range */
    if(INI_SUCCESS == config.GetValue("VIDEO_RTP_PORT_RANGE","PortStart",strValue))
    {
        m_ulRtpStartPort = atoi(strValue.c_str());
    }

    if(INI_SUCCESS == config.GetValue("VIDEO_RTP_PORT_RANGE","PortEnd",strValue))
    {
        m_ulRtpEndPort = atoi(strValue.c_str());
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
    /* ACS AppID */
    if(INI_SUCCESS == config.GetValue("ACS_CFG","AppID",strValue))
    {
        m_strAppID = strValue;
    }
    /* ACS AppSecret */
    if(INI_SUCCESS == config.GetValue("ACS_CFG","AppSecret",strValue))
    {
        m_strAppSecret = strValue;
    }
    /* ACS AppKey */
    if(INI_SUCCESS == config.GetValue("ACS_CFG","AppKey",strValue))
    {
        m_strAppKey = strValue;
    }
    /* ACS CallUrl */
    if(INI_SUCCESS == config.GetValue("ACS_CFG","CallUrl",strValue))
    {
        m_strLiveUrl = strValue;
    }
    return AS_ERROR_CODE_OK;
}

void  ASRtsp2SiptManager::http_callback(struct evhttp_request *req, void *arg)
{
    ASRtsp2SiptManager* pManage = (ASRtsp2SiptManager*)arg;
    pManage->handle_http_req(req);
}

void *ASRtsp2SiptManager::http_env_invoke(void *arg)
{
    ASRtsp2SiptManager* manager = (ASRtsp2SiptManager*)(void*)arg;
    manager->http_env_thread();
    return NULL;
}
void *ASRtsp2SiptManager::sip_env_invoke(void *arg)
{
    ASRtsp2SiptManager* manager = (ASRtsp2SiptManager*)(void*)arg;
    manager->sip_env_thread();
    return NULL;
}
void *ASRtsp2SiptManager::rtsp_env_invoke(void *arg)
{
    ASRtsp2SiptManager* manager = (ASRtsp2SiptManager*)(void*)arg;
    manager->rtsp_env_thread();
    return NULL;
}
void ASRtsp2SiptManager::http_env_thread()
{
    AS_LOG(AS_LOG_INFO,"ASRtsp2SiptManager::http_env_thread begin.");
    m_httpBase = event_base_new();
    if (NULL == m_httpBase)
    {
        AS_LOG(AS_LOG_CRITICAL,"ASRtsp2SiptManager::http_env_thread,create the event base fail.");
        return;
    }
    m_httpServer = evhttp_new(m_httpBase);
    if (NULL == m_httpServer)
    {
        AS_LOG(AS_LOG_CRITICAL,"ASRtsp2SiptManager::http_env_thread,create the http base fail.");
        return;
    }

    int ret = evhttp_bind_socket(m_httpServer, GW_SERVER_ADDR, m_httpListenPort);
    if (0 != ret)
    {
        AS_LOG(AS_LOG_CRITICAL,"ASRtsp2SiptManager::http_env_thread,bind the http socket fail.");
        return;
    }

    evhttp_set_timeout(m_httpServer, HTTP_OPTION_TIMEOUT);
    evhttp_set_gencb(m_httpServer, http_callback, this);
    event_base_dispatch(m_httpBase);

    AS_LOG(AS_LOG_INFO,"ASRtsp2SiptManager::http_env_thread end.");
    return;
}
void ASRtsp2SiptManager::sip_env_thread()
{
    int counter = 0;
    eXosip_event_t *event = NULL;
    struct eXosip_stats stats;

    AS_LOG(AS_LOG_INFO,"ASRtsp2SiptManager::sip_env_thread begin.");
    while (!m_LoopWatchVar)
    {
        counter++;
        if (counter % SIP_STATIC_INTER == 0)
        {
          memset (&stats, 0, sizeof (struct eXosip_stats));
          eXosip_lock (m_pEXosipCtx);
          eXosip_set_option (m_pEXosipCtx, EXOSIP_OPT_GET_STATISTICS, &stats);
          eXosip_unlock (m_pEXosipCtx);
          AS_LOG(AS_LOG_INFO, "eXosip stats: inmemory=(tr:%i//reg:%i) average=(tr:%f//reg:%f)", stats.allocated_transactions, stats.allocated_registrations, stats.average_transactions, stats.average_registrations);
        }
        if (!(event = eXosip_event_wait (m_pEXosipCtx, 0, 1)))
        {
#ifdef OSIP_MONOTHREAD
          eXosip_execute(m_pEXosipCtx);
#endif
          eXosip_automatic_action (m_pEXosipCtx);
          osip_usleep (10000);
          continue;
        }

#ifdef OSIP_MONOTHREAD
        eXosip_execute (m_pEXosipCtx);
#endif

        eXosip_lock (m_pEXosipCtx);
        eXosip_automatic_action (m_pEXosipCtx);

        deal_sip_event(event);

        eXosip_unlock (m_pEXosipCtx);
        eXosip_event_free (event);
    }

    AS_LOG(AS_LOG_INFO,"ASRtsp2SiptManager::sip_env_thread end.");
    return;
}
void ASRtsp2SiptManager::rtsp_env_thread()
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

u_int32_t ASRtsp2SiptManager::find_beast_thread()
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
UsageEnvironment* ASRtsp2SiptManager::get_env(u_int32_t index)
{
    UsageEnvironment* env = m_envArray[index];
    m_clCountArray[index]++;
    return env;
}
void ASRtsp2SiptManager::releas_env(u_int32_t index)
{
    m_clCountArray[index]--;
}


void ASRtsp2SiptManager::handle_http_req(struct evhttp_request *req)
{
    AS_LOG(AS_LOG_DEBUG, "ASRtsp2SiptManager::handle_http_req begin");

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
    AS_LOG(AS_LOG_DEBUG, "ASRtsp2SiptManager::handle_http_req request path[%s].", uri_str.c_str());

    evbuffer *pbuffer = req->input_buffer;
    string post_str;
    int n = 0;
    char  szBuf[HTTP_REQUEST_MAX + 1] = { 0 };
    while ((n = evbuffer_remove(pbuffer, &szBuf, HTTP_REQUEST_MAX - 1)) > 0)
    {
        szBuf[n] = '\0';
        post_str.append(szBuf, n);
    }

    AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::handle_http_req, msg[%s]", post_str.c_str());

    std::string strResp = "";
    if(AS_ERROR_CODE_OK != handle_session(post_str,strResp))
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
    AS_LOG(AS_LOG_DEBUG, "ASRtsp2SiptManager::handle_http_req end");
}
void ASRtsp2SiptManager::check_all_sip_session()
{
    as_lock_guard locker(m_mutex);

    CSipSession* pSession = NULL;
    SIP_SESSION_STATUS enStatus = SIP_SESSION_STATUS_ADD;

    SIPSESSIONMAP::iterator iter = m_SipSessionMap.begin();

    for(;iter != m_SipSessionMap.end();)
    {
        pSession = iter->second;
        if(NULL == pSession)
        {
            ++iter;
            continue;
        }
        enStatus = pSession->SessionStatus();
        if(SIP_SESSION_STATUS_ADD == enStatus)
        {
            /* send the register */
            send_sip_regsiter(pSession);
        }
        else if(SIP_SESSION_STATUS_REG == enStatus)
        {
            /* check timeout */
            send_sip_check_timeout(pSession);
        }
        else if(SIP_SESSION_STATUS_RUNING == enStatus)
        {
            /* send heart beat option*/
            send_sip_option(pSession);
        }
        else if(SIP_SESSION_STATUS_REMOVE == enStatus)
        {
            /* delete the session*/
            send_sip_unregsiter(pSession);
            iter = m_SipSessionMap.erase(iter);
            AS_DELETE(pSession);
            continue;
        }
        ++iter;
    }
    return;
}


int32_t ASRtsp2SiptManager::handle_session(std::string &strReqMsg,std::string &strRespMsg)
{
    std::string strSessionID  = "";

    int32_t ret = AS_ERROR_CODE_OK;

    AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::handle_session,msg:[%s].",strReqMsg.c_str());



    XMLDocument doc;
    XMLError xmlerr = doc.Parse(strReqMsg.c_str(),strReqMsg.length());
    if(XML_SUCCESS != xmlerr)
    {
        AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::handle_session,parse xml msg:[%s] fail.",strReqMsg.c_str());
        return AS_ERROR_CODE_FAIL;
    }

    XMLElement *req = doc.RootElement();
    if(NULL == req)
    {
        AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::handle_session,get xml req node fail.");
        return AS_ERROR_CODE_FAIL;
    }

    XMLElement *session = req->FirstChildElement("session");
    if(NULL == session)
    {
        AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::handle_session,get xml session node fail.");
        return AS_ERROR_CODE_FAIL;
    }




    const char* sessionid = session->Attribute("sessionid");
    if(NULL == sessionid)
    {
        AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::handle_session,get xml session id fail.");
        return AS_ERROR_CODE_FAIL;
    }
    strSessionID = sessionid;
    const char* command = session->Attribute("command");
    if(NULL == command)
    {
        AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::handle_session,get xml session command fail.");
        return AS_ERROR_CODE_FAIL;
    }

    if(0 == strncmp(command,XML_MSG_COMMAND_ADD,strlen(XML_MSG_COMMAND_ADD)))
    {
        ret = handle_add_session(session);
    }
    else if(0 == strncmp(command,XML_MSG_COMMAND_REMOVE,strlen(XML_MSG_COMMAND_REMOVE)))
    {
        handle_remove_session(strSessionID);
    }

    XMLDocument resp;
    XMLPrinter printer;
    XMLDeclaration *declare = resp.NewDeclaration();
    XMLElement *respEle = resp.NewElement("resp");
    resp.InsertEndChild(respEle);
    respEle->SetAttribute("version", "1.0");
    XMLElement *SesEle = resp.NewElement("session");
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

    SesEle->SetAttribute("sessionid",sessionid);
    SesEle->SetAttribute("command",command);

    resp.Accept(&printer);
    strRespMsg = printer.CStr();

    AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::handle_session,end");
    return AS_ERROR_CODE_OK;
}
int32_t ASRtsp2SiptManager::handle_add_session(const XMLElement *session)
{
    bool bRegister            = false;
    std::string strReportURL  = "";
    std::string strSessionID  = "";
    std::string strUsername   = "";
    std::string strPasswd     = "";
    std::string strDomain     = "";
    std::string strRealM      = "";
    std::string strCameraID   = "";
    std::string strStreamType = "";
    std::string strRespMsg    = "";
    u_int32_t   ulInterval    = GW_REPORT_DEFAULT;


    const char* sessionid = session->Attribute("sessionid");
    if(NULL != sessionid)
    {
        strSessionID = sessionid;
    }

   const XMLElement *report = session->FirstChildElement("report");
    if(NULL == report)
    {
        AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::add_session,get xml report node fail.");
        return AS_ERROR_CODE_FAIL;
    }
    const char* interval = report->Attribute("interval");
    if(NULL != interval)
    {
        ulInterval = atoi(interval);
    }
    const char* url      = report->Attribute("url");
    if(NULL != url)
    {
        strReportURL = url;
    }

    const XMLElement *Register = report->FirstChildElement("register");
    if(NULL == Register)
    {
        AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::add_session,get xml register node fail.");
        return AS_ERROR_CODE_FAIL;
    }


    const XMLElement *params = report->FirstChildElement("params");
    if(NULL == params)
    {
        AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::add_session,get xml register node fail.");
        return AS_ERROR_CODE_FAIL;
    }

    const XMLElement *param = params->FirstChildElement("param");
    const char* name  = NULL;
    const char* value = NULL;
    while(param)
    {
        name = param->Attribute("name");
        value = param->Attribute("value");

        if(NULL == name)
        {
            param = param->NextSiblingElement();
            continue;
        }

        if(0 == strncmp(name,XML_MSG_NODE_USERNAME,strlen(XML_MSG_NODE_USERNAME)))
        {
            if(NULL == value)
            {
                param = param->NextSiblingElement();
                continue;
            }
            strUsername = value;
        }
        else if(0 == strncmp(name,XML_MSG_NODE_PASSWORD,strlen(XML_MSG_NODE_PASSWORD)))
        {
            if(NULL == value)
            {
                param = param->NextSiblingElement();
                continue;
            }
            strPasswd = value;
        }
        else if(0 == strncmp(name,XML_MSG_NODE_DOMAIN,strlen(XML_MSG_NODE_DOMAIN)))
        {
            if(NULL == value)
            {
                param = param->NextSiblingElement();
                continue;
            }
            strDomain = value;
        }
        else if(0 == strncmp(name,XML_MSG_NODE_REALM,strlen(XML_MSG_NODE_REALM)))
        {
            if(NULL == value)
            {
                param = param->NextSiblingElement();
                continue;
            }
            strRealM = value;
        }
        else if(0 == strncmp(name,XML_MSG_NODE_CAMERAID,strlen(XML_MSG_NODE_CAMERAID)))
        {
            if(NULL == value)
            {
                param = param->NextSiblingElement();
                continue;
            }
            strCameraID = value;
        }
        else if(0 == strncmp(name,XML_MSG_NODE_STREAMTYPE,strlen(XML_MSG_NODE_STREAMTYPE)))
        {
            if(NULL == value)
            {
                param = param->NextSiblingElement();
                continue;
            }
            strStreamType = value;
        }

        param = param->NextSiblingElement();
    }


    AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::add_session,userName:[%s],register:[%d],"
                          "Passwd:[%s],Domain:[%s],realM:[%s]"
                           "cameraid:[%s],streamType:[%s]",
                          strUsername.c_str(),bRegister,
                          strPasswd.c_str(),strDomain.c_str(),strRealM.c_str(),
                          strCameraID.c_str(),strStreamType.c_str());
    CSipSession* pSession = NULL;
    as_lock_guard locker(m_mutex);
    SIPSESSIONMAP::iterator iter = m_SipSessionMap.find(strSessionID);
    if(iter == m_SipSessionMap.end())
    {
        pSession = AS_NEW(pSession);
        if(NULL == pSession)
        {
            return AS_ERROR_CODE_FAIL;
        }
        pSession->Init();
        m_SipSessionMap.insert(SIPSESSIONMAP::value_type(strSessionID,pSession));
    }
    else
    {
        pSession = iter->second;
    }
    pSession->RepInterval(ulInterval);
    pSession->ReportUrl(strReportURL);
    pSession->SetSipRegInfo(bRegister,strUsername, strPasswd, strDomain,strRealM);
    pSession->SetCameraInfo(strCameraID,strStreamType);

    AS_LOG(AS_LOG_INFO, "ASRtsp2SiptManager::add_session,end");
    return AS_ERROR_CODE_OK;
}
void    ASRtsp2SiptManager::handle_remove_session(std::string &strSessionID)
{
    CSipSession* pSession = NULL;
    as_lock_guard locker(m_mutex);
    SIPSESSIONMAP::iterator iter = m_SipSessionMap.find(strSessionID);
    if(iter == m_SipSessionMap.end())
    {
        return ;
    }
    pSession = iter->second;

    pSession->SessionStatus(SIP_SESSION_STATUS_REMOVE);

    return;
}

void ASRtsp2SiptManager::send_sip_regsiter(CSipSession* pSession)
{
    if(NULL != pSession)
    {
        return;
    }

    if(!pSession->bRegister())
    {
        pSession->SessionStatus(SIP_SESSION_STATUS_RUNING);
    }

    std::string strUsername   = pSession->UserName();
    std::string strPasswd     = pSession->Password();
    std::string strDomain     = pSession->Domain();
    std::string strRealM      = pSession->RealM();

    if (strUsername.length() && strPasswd.length()) {
        std::string strRegID = strUsername + std::string("@") + strDomain;
        if (eXosip_add_authentication_info(m_pEXosipCtx, strUsername.c_str(), strRegID.c_str(), strPasswd.c_str(), NULL, strRealM.c_str())) {
            AS_LOG (AS_LOG_ERROR, "eXosip_add_authentication_info failed");
            return ;
        }
    }

    std::string proxy = std::string("sip:") + strDomain;
    std::string fromuser = std::string("sip:+") + strUsername + std::string("@") + strDomain;
    std::string contact = std::string("sip:+") + strUsername + std::string("@") + m_strLocalIP;

    //std::string contact = std::string("sip:") + strUsername + std::string("@") + strDomain;

    osip_message_t *reg = NULL;

   // osip_nict_set_destination()

    int regID = eXosip_register_build_initial_register(m_pEXosipCtx, fromuser.c_str(), proxy.c_str(), contact.c_str(),3600, &reg);
    if (regID < 1) {
        AS_LOG (AS_LOG_ERROR, "eXosip_register_build_initial_register failed");
        return ;
    }
    int i = eXosip_register_send_register(m_pEXosipCtx,regID, reg);
    if (i != 0) {
        AS_LOG (AS_LOG_ERROR, "eXosip_register_send_register failed");
        return ;
    }
    pSession->RegID(regID);
    pSession->SessionStatus(SIP_SESSION_STATUS_REG);

    return ;
}

void    ASRtsp2SiptManager::send_sip_unregsiter(CSipSession* pSession)
{
    if(NULL != pSession)
    {
        return;
    }

    /* stop the media channel */
    pSession->close_all();

    /* send the unregister message */
    if(!pSession->bRegister())
    {
        return;
    }

    std::string strUsername   = pSession->UserName();
    std::string strPasswd     = pSession->Password();
    std::string strDomain     = pSession->Domain();
    std::string strRealM      = pSession->RealM();
    int regID                 = pSession->RegID();

    if(0 >= regID)
    {
        return;
    }

    std::string proxy = std::string("sip:") + strDomain;
    std::string fromuser = std::string("sip:+") + strUsername + std::string("@") + strDomain;
    std::string contact = std::string("sip:+") + strUsername + std::string("@") + m_strLocalIP;

    //std::string contact = std::string("sip:") + strUsername + std::string("@") + strDomain;

    osip_message_t *reg = NULL;

   // osip_nict_set_destination()
   if ( OSIP_SUCCESS != eXosip_register_build_register(m_pEXosipCtx,regID,0,&reg)) {
        AS_LOG (AS_LOG_ERROR, "eXosip_register_build_register failed");
        return;
    }
    int i = eXosip_register_send_register(m_pEXosipCtx,regID, reg);
    if (i != 0) {
        AS_LOG (AS_LOG_ERROR, "eXosip_register_send_register failed");
        return;
    }
    eXosip_remove_authentication_info(m_pEXosipCtx, strUsername.c_str(), strRealM.c_str());
    pSession->RegID(0);
    return;
}
void    ASRtsp2SiptManager::send_sip_check_timeout(CSipSession* pSession)
{
    return;
}
void    ASRtsp2SiptManager::send_sip_option(CSipSession* pSession)
{
    return;
}

void ASRtsp2SiptManager::deal_sip_event(eXosip_event_t *event)
{
    if(NULL == event)
    {
        return;
    }
    event->request;
    AS_LOG(AS_LOG_INFO, "deal the eXosip event (type, did, cid) = (%d, %d, %d)", event->type, event->did, event->cid);
    switch (event->type)
    {
        case EXOSIP_REGISTRATION_SUCCESS:
        {
            deal_regsiter_success(event);
            break;
        }
        case EXOSIP_REGISTRATION_FAILURE:
        {
            deal_regsiter_fail(event);
            break;
        }
        case EXOSIP_CALL_INVITE:
        {
            deal_call_invite_req(event);
            break;
        }
        case EXOSIP_CALL_ACK:
        {
            deal_call_ack_req(event);
            break;
        }
        case EXOSIP_CALL_CLOSED:
        {
            deal_call_close_req(event);
            break;
        }
        case EXOSIP_CALL_CANCELLED:
        {
            deal_call_cancelled_req(event);
            break;
        }
        case EXOSIP_MESSAGE_NEW:
        {
            deal_message_req(event);
            break;
        }
        default:
        {
          AS_LOG (AS_LOG_INFO, "recieved unknown eXosip event (type, did, cid) = (%d, %d, %d)", event->type, event->did, event->cid);
          break;
        }

    }
}
void ASRtsp2SiptManager::deal_regsiter_success(eXosip_event_t *event)
{
    int nRegID  = event->rid;
    as_lock_guard locker(m_mutex);
    REGSESSIONMAP::iterator  iter = m_RegSessionMap.find(nRegID);
    if (iter == m_RegSessionMap.end()) {
        AS_LOG (AS_LOG_WARNING, "registrered:[%d] successfully,but not find the session",nRegID);
        return;
    }

    CSipSession* pSession = iter->second;
    pSession->SessionStatus(SIP_SESSION_STATUS_RUNING);

    AS_LOG (AS_LOG_INFO, "registrered successfully");
}
void ASRtsp2SiptManager::deal_regsiter_fail(eXosip_event_t *event)
{
    int nRegID  = event->rid;
    as_lock_guard locker(m_mutex);
    REGSESSIONMAP::iterator  iter = m_RegSessionMap.find(nRegID);
    if (iter == m_RegSessionMap.end()) {
        AS_LOG (AS_LOG_WARNING, "registrer fail:[%d] successfully,but not find the session",nRegID);
        return;
    }

    CSipSession* pSession = iter->second;
    pSession->SessionStatus(SIP_SESSION_STATUS_ADD);
    m_RegSessionMap.erase(iter);

    AS_LOG (AS_LOG_INFO, "registrered fail.");
}
void ASRtsp2SiptManager::deal_call_invite_req(eXosip_event_t *event)
{
    AS_LOG (AS_LOG_INFO, "CSipManager::deal_call_invite_req,deal INVITE begin");
    osip_message_t   *invite;
    osip_message_t   *answer;
    sdp_message_t    *remote_sdp = NULL;
    CRtpPortPair     *local_ports = NULL;


    int i,call_id, dialog_id;

    CSipSession* pSession = NULL;

    invite = event->request;
    call_id = event->cid;
    dialog_id = event->did;

    std::string strUsername = invite->req_uri->username;

    AS_LOG(AS_LOG_INFO, "deal INVITE ,call the user:[%s].", strUsername.c_str());

    as_lock_guard locker(m_mutex);
    SIPSESSIONMAP::iterator iter = m_SipSessionMap.begin();
    for(;iter != m_SipSessionMap.end();++iter)
    {
        pSession = iter->second;
        if(strUsername == pSession->UserName())
        {
            break;
        }
        pSession = NULL;
    }
    if(NULL == pSession)
    {
        /* send the 405 reject invite*/
        i = eXosip_call_build_answer (m_pEXosipCtx, event->tid, 405, &answer);
        if (i != 0) {
          AS_LOG (AS_LOG_ERROR, "failed to create the reject message.");
          return;
        }
        answer->reason_phrase = osip_strdup("the camera is not found");
        i = eXosip_call_send_answer (m_pEXosipCtx, event->tid, 405, answer);
        if (i != 0) {
          AS_LOG (AS_LOG_ERROR, "failed to send the reject message.");
          return;
        }
        return;
    }

    local_ports = get_free_port_pair(call_id);
    if(NULL == local_ports)
    {
        /* send the 405 reject invite*/
        i = eXosip_call_build_answer (m_pEXosipCtx, event->tid, 405, &answer);
        if (i != 0) {
          AS_LOG (AS_LOG_ERROR, "failed to create the reject message.");
          return;
        }
        answer->reason_phrase = osip_strdup("there is no free ports for media.");
        i = eXosip_call_send_answer (m_pEXosipCtx, event->tid, 405, answer);
        if (i != 0) {
          AS_LOG (AS_LOG_ERROR, "failed to send the reject message.");
          return;
        }
        return;
    }


    remote_sdp = eXosip_get_remote_sdp(m_pEXosipCtx,event->did);

    long lResult = pSession->handle_invite(call_id, event->tid,local_ports, remote_sdp);
    if(AS_ERROR_CODE_OK != lResult) {

        free_port_pair(call_id);
        /* send the 405 reject invite*/
        i = eXosip_call_build_answer (m_pEXosipCtx, event->tid, 405, &answer);
        if (i != 0) {
          AS_LOG (AS_LOG_ERROR, "failed to create the reject message.");
          return;
        }
        osip_free(answer->reason_phrase);
        answer->reason_phrase = osip_strdup("create media channel fail");
        i = eXosip_call_send_answer (m_pEXosipCtx, event->tid, 405, answer);
        if (i != 0) {
          AS_LOG (AS_LOG_ERROR, "failed to send the reject message.");
          return;
        }
        return;
    }

    AS_LOG (AS_LOG_INFO, "CSipManager::deal_call_invite_req,deal INVITE end");

    return;
}

void ASRtsp2SiptManager::send_invit_200_ok(int nTransID,CRtpPortPair*local_ports,std::string& strSdp)
{
    AS_LOG(AS_LOG_DEBUG, "deal the send invite 200 ok begin.");
    osip_message_t   *answer;
    char localip[SIP_LOCAL_IP_LENS] = {0};
    char localsdp[SIP_SDP_LENS_MAX] = {0};
    /*2.build the response message and sdp info*/
    int i = eXosip_call_build_answer(m_pEXosipCtx, nTransID, 200, &answer);
    if (i != 0) {
        AS_LOG (AS_LOG_ERROR, "failed to create the 200 OK message.");
        eXosip_call_send_answer(m_pEXosipCtx,nTransID, 400, NULL);
        return;
    }

    eXosip_guess_localip(m_pEXosipCtx,AF_INET, localip, SIP_LOCAL_IP_LENS);

    snprintf (localsdp, SIP_SDP_LENS_MAX,
                        "v=0\r\n"
                        "o=- 0 0 IN IP4 %s\r\n"
                        "s=all camera\r\n"
                        "c=IN IP4 %s\r\n"
                        "t=0 0\r\n"
                        "m=audio %d RTP/AVP 0\r\n"
                        "a=rtpmap:0 PCMU/8000\r\n"
                        "a=rtpmap:8 PCMA/8000\r\n"
                        "m=video %d RTP/AVP 96\r\n"
                        "a=rtpmap:96 H264/90000\r\n",
                        localip, localip,local_ports->getARtpPort(),local_ports->getVRtpPort());

    AS_LOG(AS_LOG_DEBUG, " local media channel Sdp:[%s]", localsdp);

    osip_message_set_body (answer, localsdp, strlen(localsdp));
    osip_message_set_content_type (answer, "application/sdp");
    AS_LOG(AS_LOG_DEBUG, " send the invite 200 OK.");
    eXosip_call_send_answer(m_pEXosipCtx,nTransID, 200, answer);
    AS_LOG(AS_LOG_DEBUG, "deal the send invite 200 ok end.");
}

void ASRtsp2SiptManager::deal_call_ack_req(eXosip_event_t *event)
{
    AS_LOG (AS_LOG_INFO, "CSipManager::deal_call_ack_req,deal ACK begin");
    osip_message_t *ack;
    sdp_message_t  *remote_sdp = NULL;
    sdp_connection_t *audio_con = NULL;
    sdp_media_t      *md_audio = NULL;
    sdp_connection_t *video_con = NULL;
    sdp_media_t      *md_video = NULL;



    int call_id, dialog_id;
    CSipSession* pSession = NULL;

    ack = event->request;

    std::string strUsername = ack->req_uri->username;

    as_lock_guard locker(m_mutex);
    SIPSESSIONMAP::iterator iter = m_SipSessionMap.find(strUsername);
    if(iter == m_SipSessionMap.end())
    {
        AS_LOG(AS_LOG_ERROR, "the ack not found the camerea.");
        return;
    }
    pSession = iter->second;

    call_id = event->cid;
    dialog_id = event->did;

    remote_sdp = eXosip_get_remote_sdp(m_pEXosipCtx,event->did);

    pSession->handle_ack(call_id,remote_sdp);
    AS_LOG (AS_LOG_INFO, "CSipManager::deal_call_ack_req,deal ACK end");
}
void ASRtsp2SiptManager::deal_call_close_req(eXosip_event_t *event)
{
    AS_LOG (AS_LOG_INFO, "CSipManager::deal_call_close_req,deal CALL CLOSE begin");
    osip_message_t   *close;
    osip_message_t   *answer;
    sdp_message_t    *remote_sdp = NULL;
    sdp_connection_t *audio_con = NULL;
    sdp_media_t      *md_audio = NULL;
    sdp_connection_t *video_con = NULL;
    sdp_media_t      *md_video = NULL;



    int i,call_id, dialog_id;
    CSipSession* pSession = NULL;

    close = event->request;

    std::string strUsername = close->req_uri->username;

    as_lock_guard locker(m_mutex);
    SIPSESSIONMAP::iterator iter = m_SipSessionMap.find(strUsername);
    if(iter == m_SipSessionMap.end())
    {
        /* send the 405 reject invite*/
        i = eXosip_call_build_answer (m_pEXosipCtx, event->tid, 405, &answer);
        if (i != 0) {
          AS_LOG (AS_LOG_ERROR, "failed to create the reject message.");
          return;
        }
        osip_free(answer->reason_phrase);
        answer->reason_phrase = osip_strdup("the camera is not found");
        i = eXosip_call_send_answer (m_pEXosipCtx, event->tid, 405, answer);
        if (i != 0) {
          AS_LOG (AS_LOG_ERROR, "failed to send the reject message.");
          return;
        }
        return;
    }
    pSession = iter->second;

    call_id = event->cid;
    dialog_id = event->did;

    pSession->handle_bye(call_id);
    free_port_pair(call_id);

    AS_LOG (AS_LOG_INFO, "CSipManager::deal_call_close_req,deal CALL CLOSE end");
}
void ASRtsp2SiptManager::deal_call_cancelled_req(eXosip_event_t *event)
{
    AS_LOG (AS_LOG_INFO, "CSipManager::deal_call_cancelled_req,deal CANCELLED begin");
    osip_message_t *cancelled;
    sdp_message_t  *remote_sdp = NULL;
    sdp_connection_t *audio_con = NULL;
    sdp_media_t      *md_audio = NULL;
    sdp_connection_t *video_con = NULL;
    sdp_media_t      *md_video = NULL;



    int call_id, dialog_id;
    CSipSession* pSession = NULL;

    cancelled = event->request;

    std::string strUsername = cancelled->req_uri->username;

    as_lock_guard locker(m_mutex);
    SIPSESSIONMAP::iterator iter = m_SipSessionMap.find(strUsername);
    if(iter == m_SipSessionMap.end())
    {
        return;
    }
    pSession = iter->second;

    call_id = event->cid;
    dialog_id = event->did;

    pSession->handle_bye(call_id);
    free_port_pair(call_id);

    AS_LOG (AS_LOG_INFO, "CSipManager::deal_call_cancelled_req,deal CANCELLED end");
}

void ASRtsp2SiptManager::deal_message_req(eXosip_event_t *event)
{
    AS_LOG (AS_LOG_INFO, "CSipManager::deal_message_req,deal MESSAGE begin");
    osip_message_t *answer;
    int i;

    i = eXosip_message_build_answer (m_pEXosipCtx, event->tid, 200, &answer);
    if (i != 0) {
      AS_LOG (AS_LOG_ERROR, "failed to create %s", event->request->sip_method);
      return;
    }
    i = eXosip_message_send_answer (m_pEXosipCtx, event->tid, 200, answer);
    if (i != 0) {
      AS_LOG (AS_LOG_INFO, "failed to send %s", event->request->sip_method);
      return;
    }
    AS_LOG (AS_LOG_INFO, "%s answer with 200", event->request->sip_method);
    AS_LOG (AS_LOG_INFO, "CSipManager::deal_message_req,deal MESSAGE end");
}
int32_t       ASRtsp2SiptManager::init_port_pairs()
{
    unsigned short usCount = m_ulRtpEndPort - m_ulRtpStartPort + 1;
    unsigned short usPairCount = usCount / GW_PORT_PAIR_SIZE;
    unsigned short usPort = m_ulRtpEndPort;
    unsigned short usVRtpPort  = 0;
    unsigned short usVRtcpPort = 0;
    unsigned short usARtpPort  = 0;
    unsigned short usARtcpPort = 0;

    CRtpPortPair* pair = NULL;

    for(unsigned short i = 0;i < usPairCount;i++)
    {
        usVRtpPort = usPort++;
        usVRtcpPort = usPort++;
        usARtpPort = usPort++;
        usARtcpPort = usPort++;

        pair = AS_NEW(pair);
        if(NULL == pair)
        {
            return AS_ERROR_CODE_FAIL;
        }
        pair->init(usVRtpPort, usVRtcpPort, usARtpPort, usARtcpPort);
        m_freePortList.push_back(pair);
    }

    m_callPortMap.clear();
    return AS_ERROR_CODE_OK;
}

CRtpPortPair* ASRtsp2SiptManager::get_free_port_pair(int nCallId)
{
    CRtpPortPair* pair = NULL;
    if(0 == m_freePortList.size())
    {
        return NULL;
    }
    pair = m_freePortList.front();
    m_freePortList.pop_front();

    m_callPortMap.insert(CALLPORTPAIREMAP::value_type(nCallId,pair));
    return pair;
}
void          ASRtsp2SiptManager::free_port_pair(int nCallId)
{
    CRtpPortPair* pair = NULL;
    CALLPORTPAIREMAP::iterator iter = m_callPortMap.find(nCallId);
    if(iter == m_callPortMap.end())
    {
        return;
    }
    pair = iter->second;
    m_callPortMap.erase(iter);
    m_freePortList.push_back(pair);
    return;
}
void ASRtsp2SiptManager::setRecvBufSize(u_int32_t ulSize)
{
    m_ulRecvBufSize = ulSize;
}
u_int32_t ASRtsp2SiptManager::getRecvBufSize()
{
    return m_ulRecvBufSize;
}





