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

RTMPStream::RTMPStream(void)
{
    m_pRtmp = NULL;
    InitSockets();
    m_pRtmp = RTMP_Alloc();
    RTMP_Init(m_pRtmp);
}
RTMPStream::~RTMPStream(void)
{

}

int RTMPStream::InitSockets()
{
#ifdef WIN32
    WORD version;
    WSADATA wsaData;
    version = MAKEWORD(1, 1);
    return (WSAStartup(version, &wsaData) == 0);
#else
    return TRUE;
#endif
}


void RTMPStream::CleanupSockets()
{
#ifdef WIN32
    WSACleanup();
#endif
}

char * RTMPStream::put_byte( char *output, uint8_t nVal )
{
    output[0] = nVal;
    return output+1;
}
char * RTMPStream::put_be16(char *output, uint16_t nVal )
{
    output[1] = nVal & 0xff;
    output[0] = nVal >> 8;
    return output+2;
}
char * RTMPStream::put_be24(char *output,uint32_t nVal )
{
    output[2] = nVal & 0xff;
    output[1] = nVal >> 8;
    output[0] = nVal >> 16;
    return output+3;
}
char * RTMPStream::put_be32(char *output, uint32_t nVal )
{
    output[3] = nVal & 0xff;
    output[2] = nVal >> 8;
    output[1] = nVal >> 16;
    output[0] = nVal >> 24;
    return output+4;
}
char *  RTMPStream::put_be64( char *output, uint64_t nVal )
{
    output=put_be32( output, nVal >> 32 );
    output=put_be32( output, nVal );
    return output;
}
char * RTMPStream::put_amf_string( char *c, const char *str )
{
    uint16_t len = strlen( str );
    c=put_be16( c, len );
    memcpy(c,str,len);
    return c+len;
}
char * RTMPStream::put_amf_double( char *c, double d )
{
    *c++ = AMF_NUMBER;  /* type: Number */
    {
        unsigned char *ci, *co;
        ci = (unsigned char *)&d;
        co = (unsigned char *)c;
        co[0] = ci[7];
        co[1] = ci[6];
        co[2] = ci[5];
        co[3] = ci[4];
        co[4] = ci[3];
        co[5] = ci[2];
        co[6] = ci[1];
        co[7] = ci[0];
    }
    return c+8;
}

bool RTMPStream::Connect(const char* url)
{
    if(RTMP_SetupURL(m_pRtmp, (char*)url)<0)
    {
        return FALSE;
    }
    RTMP_EnableWrite(m_pRtmp);
    if(RTMP_Connect(m_pRtmp, NULL)<0)
    {
        return FALSE;
    }
    if(RTMP_ConnectStream(m_pRtmp,0)<0)
    {
        return FALSE;
    }
    return TRUE;
}
// ????
void RTMPStream::Close()
{
    if(m_pRtmp)
    {

       if(RTMP_IsConnected(m_pRtmp)) {
            RTMP_Close(m_pRtmp);
       }
       RTMP_Free(m_pRtmp);

       m_pRtmp = NULL;
    }
}
// ??MetaData
bool RTMPStream::SendMetadata(LPRTMPMetadata lpMetaData)
{
    if(lpMetaData == NULL)
    {
        return false;
    }

    char body[1024] = {0};

    char * p = (char *)body;
    p = put_byte(p, AMF_STRING );
    p = put_amf_string(p , "@setDataFrame" );

    p = put_byte( p, AMF_STRING );
    p = put_amf_string( p, "onMetaData" );

    p = put_byte(p, AMF_OBJECT );
    p = put_amf_string( p, "copyright" );
    p = put_byte(p, AMF_STRING );
    p = put_amf_string( p, "firehood" );

    p =put_amf_string( p, "width");
    p =put_amf_double( p, lpMetaData->nWidth);

    p =put_amf_string( p, "height");
    p =put_amf_double( p, lpMetaData->nHeight);

    p =put_amf_string( p, "framerate" );
    p =put_amf_double( p, lpMetaData->nFrameRate);

    p =put_amf_string( p, "videodatarate" );
    p =put_amf_double( p, lpMetaData->nVideoDataRate);

    p =put_amf_string( p, "videocodecid" );
    p =put_amf_double( p, lpMetaData->nvideocodecid );

    if(lpMetaData->bHasAudio) {

        p =put_amf_string( p, "audiodatarate");
        p =put_amf_double( p, lpMetaData->nAudioDatarate);

        p =put_amf_string( p, "audiosamplerate");
        p =put_amf_double( p, lpMetaData->nAudioSampleRate);

        p =put_amf_string( p, "audiosamplesize" );
        p =put_amf_double( p, lpMetaData->nAudioSampleSize);

        p =put_amf_string( p, "stereo" );
        p =put_amf_double( p, lpMetaData->nAudioChannels);

        p =put_amf_string( p, "audiocodecid" );
        p =put_amf_double( p, lpMetaData->nAudioFmt);
    }

    p =put_amf_string( p, "" );
    p =put_byte( p, AMF_OBJECT_END  );

    int index = p-body;

    return SendPacket(RTMP_PACKET_TYPE_INFO,(unsigned char*)body,p-body,0);

    int i = 0;
    body[i++] = 0x17; // 1:keyframe  7:AVC
    body[i++] = 0x00; // AVC sequence header

    body[i++] = 0x00;
    body[i++] = 0x00;
    body[i++] = 0x00; // fill in 0;

    // AVCDecoderConfigurationRecord.
    body[i++] = 0x01; // configurationVersion
    body[i++] = lpMetaData->Sps[1]; // AVCProfileIndication
    body[i++] = lpMetaData->Sps[2]; // profile_compatibility
    body[i++] = lpMetaData->Sps[3]; // AVCLevelIndication
    body[i++] = 0xff; // lengthSizeMinusOne

    // sps nums
    body[i++] = 0xE1; //&0x1f
    // sps data length
    body[i++] = lpMetaData->nSpsLen>>8;
    body[i++] = lpMetaData->nSpsLen&0xff;
    // sps data
    memcpy(&body[i],lpMetaData->Sps,lpMetaData->nSpsLen);
    i= i+lpMetaData->nSpsLen;

    // pps nums
    body[i++] = 0x01; //&0x1f
    // pps data length
    body[i++] = lpMetaData->nPpsLen>>8;
    body[i++] = lpMetaData->nPpsLen&0xff;
    // sps data
    memcpy(&body[i],lpMetaData->Pps,lpMetaData->nPpsLen);
    i= i+lpMetaData->nPpsLen;

    return SendPacket(RTMP_PACKET_TYPE_VIDEO,(unsigned char*)body,i,0);

}
// ??H264???
bool RTMPStream::SendH264Packet(unsigned char *data,unsigned int size,bool bIsKeyFrame,unsigned int nTimeStamp)
{
    if(data == NULL && size<11)
    {
        return false;
    }

    unsigned char *body = new unsigned char[size+9];

    int i = 0;

    if(bIsKeyFrame)
        body[i++] = 0x17;// 1:Iframe  7:AVC
    else
        body[i++] = 0x27;// 2:Pframe  7:AVC

    body[i++] = 0x01;// AVC NALU
    body[i++] = 0x00;
    body[i++] = 0x00;
    body[i++] = 0x00;

    // NALU size
    body[i++] = size>>24;
    body[i++] = size>>16;
    body[i++] = size>>8;
    body[i++] = size&0xff;

    // NALU data
    memcpy(&body[i],data,size);
    bool bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO,body,i+size,nTimeStamp);
    delete[] body;
    return bRet;
}
//??AAC??
bool RTMPStream::SendAACPacket(unsigned char* data,unsigned int size,unsigned int nTimeStamp )
{
    if(m_pRtmp == NULL)
        return FALSE;

    if (size > 0)
    {
        RTMPPacket * packet;
        unsigned char * body;

        packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE+size+2);
        memset(packet,0,RTMP_HEAD_SIZE);

        packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
        body = (unsigned char *)packet->m_body;

        /*AF 01 + AAC RAW data*/
        body[0] = 0xAF;
        body[1] = 0x01;
        memcpy(&body[2],data,size);

        packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
        packet->m_nBodySize = size+2;
        packet->m_nChannel = 0x04;
        packet->m_nTimeStamp = nTimeStamp;
        packet->m_hasAbsTimestamp = 0;
        packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
        packet->m_nInfoField2 = m_pRtmp->m_stream_id;

        /*??????*/
        RTMP_SendPacket(m_pRtmp,packet,TRUE);
        free(packet);
    }
    return true;
}

// ????
int RTMPStream::SendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp)
{
    if(m_pRtmp == NULL)
        return FALSE;


    RTMPPacket packet;
    RTMPPacket_Reset(&packet);
    RTMPPacket_Alloc(&packet,size);

    packet.m_packetType = nPacketType;
    packet.m_nChannel = 0x04;
    packet.m_headerType = /*RTMP_PACKET_SIZE_MEDIUM*/RTMP_PACKET_SIZE_LARGE;
    packet.m_nTimeStamp = nTimestamp;
    packet.m_nInfoField2 = m_pRtmp->m_stream_id;
    packet.m_nBodySize = size;
    packet.m_hasAbsTimestamp = 0;
    memcpy(packet.m_body,data,size);

    //??????????
    int nRet = RTMP_SendPacket(m_pRtmp,&packet,TRUE);

    RTMPPacket_Free(&packet);

    return nRet;
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
  m_ulTryTime = 0;
  m_bTcp = false;
  m_lLastHeartBeat = time(NULL);

  m_rtmpUlr = NULL;
}

ASRtsp2RtmpClient::~ASRtsp2RtmpClient() {
    if (NULL != m_mutex) {
        as_destroy_mutex(m_mutex);
        m_mutex = NULL;
    }
    m_RtmpStream.Close();
}

int32_t ASRtsp2RtmpClient::open(char const* rtmpURL,as_rtsp_callback_t* cb)
{
    as_lock_guard locker(m_mutex);
    m_cb = cb;
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

void ASRtsp2RtmpClient::report_status(int status)
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
        if (2 > m_ulTryTime) {
            tryReqeust();
        }
        return;
    }
    do {
        UsageEnvironment& env = envir(); // alias
        if (resultCode != 0) {
            delete[] resultString;
            break;
        }

        char* const sdpDescription = resultString;

        size_t lens = strlen(sdpDescription);

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
        report_status(AS_RTSP_STATUS_INIT);


        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession();

        return;
    } while (0);

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

    /* connect to the rtmp server */
    if (!m_RtmpStream.Connect(m_rtmpUlr)) {
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
        return;
    }
    do {
        UsageEnvironment& env = envir(); // alias

        if (resultCode != 0) {
            break;
        }

        // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
        // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
        // after we've sent a RTSP "PLAY" command.)

        scs.subsession->sink = ASRtsp2RtmpStreamSink::createNew(env, *scs.subsession,&m_RtmpStream, url());
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
        return;
    }
    do {
        UsageEnvironment& env = envir(); // alias

        if (resultCode != 0) {
            break;
        }

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
        return;
    }

    report_status(AS_RTSP_STATUS_PAUSE);

    delete[] resultString;
}
void ASRtsp2RtmpClient::handleAfterSeek(int resultCode, char* resultString)
{
    if (0 != resultCode) {
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
    UsageEnvironment& env = envir(); // alias


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

    m_RtmpStream.Close();

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
    m_cb = NULL;
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
    return true;
}
void ASRtsp2RtmpClient::tryReqeust()
{
    /* shutshow first*/
    as_lock_guard locker(m_mutex);
    m_bRunning = 0;
    resetTCPSockets();
    shutdownStream();
    if (NULL != scs.streamTimerTask) {
        envir().taskScheduler().unscheduleDelayedTask(scs.streamTimerTask);
        scs.streamTimerTask = NULL;
    }
    sendDescribeCommand(continueAfterDESCRIBE);
    m_ulTryTime++;
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
        ASRtsp2RtmpStreamSink* sink = NULL;
        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                sink = (ASRtsp2RtmpStreamSink*)subsession->sink;
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
        ASRtsp2RtmpStreamSink* sink = NULL;
        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                sink = (ASRtsp2RtmpStreamSink*)subsession->sink;
                if (sink != NULL) {
                    sink->Stop();
                }
            }
        }
    }
}



ASRtsp2RtmpStreamSink* ASRtsp2RtmpStreamSink::createNew(UsageEnvironment& env, MediaSubsession& subsession,
    RTMPStream* pRtmpStream,char const* streamId) {
    ASRtsp2RtmpStreamSink* pSink = NULL;
    try{
        pSink = new ASRtsp2RtmpStreamSink(env, subsession,pRtmpStream, streamId);
    }
    catch (...){
        pSink = NULL;
    }
    return pSink;
}

ASRtsp2RtmpStreamSink::ASRtsp2RtmpStreamSink(UsageEnvironment& env, MediaSubsession& subsession,
    RTMPStream* pRtmpStream,char const* streamId)
    : MediaSink(env), fSubsession(subsession),m_pRtmpStream(pRtmpStream) {
    fStreamId = strDup(streamId);
    m_bWaitFirstKeyFrame = false;
    fReceiveBuffer = (u_int8_t*)&fMediaBuffer[RTMP_HEAD_SIZE];
    prefixSize     = RTMP_HEAD_SIZE;

    memset(&m_stMetadata,0,sizeof(RTMPMetadata));

    if (!strcmp(fSubsession.mediumName(), "video")) {
        m_bWaitFirstKeyFrame = true;
        if (!strcmp(fSubsession.codecName(), "H264")) {
            fMediaBuffer[prefixSize] = 0x00;
            fMediaBuffer[prefixSize + 1] = 0x00;
            fMediaBuffer[prefixSize + 2] = 0x00;
            fMediaBuffer[prefixSize + 3] = 0x01;

            prefixSize += DUMMY_SINK_H264_STARTCODE_SIZE;
            fReceiveBuffer = (u_int8_t*)&fMediaBuffer[prefixSize];
            m_stMetadata.nvideocodecid = FLV_CODECID_H264;
        }

        // TODO H265
    }
    else if (!strcmp(fSubsession.mediumName(), "audio")){
        fReceiveBuffer = (u_int8_t*)&fMediaBuffer[prefixSize];
    }
    m_bRunning = true;

}

ASRtsp2RtmpStreamSink::~ASRtsp2RtmpStreamSink() {
    fReceiveBuffer = NULL;
    if(NULL == fStreamId) {
        delete[] fStreamId;
        fStreamId = NULL;
    }
}

void ASRtsp2RtmpStreamSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {
    ASRtsp2RtmpStreamSink* sink = (ASRtsp2RtmpStreamSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ASRtsp2RtmpStreamSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {

    if(!m_bRunning) {
        continuePlaying();
        return;
    }

    if(FLV_CODECID_H264 == m_stMetadata.nvideocodecid) {
        sendH264Frame(frameSize,numTruncatedBytes,presentationTime,durationInMicroseconds);
    }
    else if(FLV_CODECID_H265 == m_stMetadata.nvideocodecid ) {
        sendH265Frame();
    }
    // Then continue, to request the next frame of data:
    continuePlaying();
}

Boolean ASRtsp2RtmpStreamSink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}

// send the H264 frame
void ASRtsp2RtmpStreamSink::sendH264Frame(unsigned frameSize, unsigned numTruncatedBytes,
             struct timeval presentationTime, unsigned durationInMicroseconds)
{
    unsigned int nTimeStamp = presentationTime.tv_sec * 1000 + presentationTime.tv_usec / 1000;
    H264_FU_INDICATOR* fu = (H264_FU_INDICATOR*)(void*)fReceiveBuffer;
    if(H264_NALU_TYPE_SPS == fu->TYPE) {
        if(H264_PPS_SPS_FRAME_LEN_MAX < frameSize) {
            return;
        }
        memcpy((char*)&m_stMetadata.Sps[0],fReceiveBuffer,frameSize);
        m_stMetadata.nSpsLen = frameSize;
    }
    else if(H264_NALU_TYPE_PPS == fu->TYPE) {
        if(H264_PPS_SPS_FRAME_LEN_MAX < frameSize) {
            return;
        }
        memcpy((char*)&m_stMetadata.Pps[0],fReceiveBuffer,frameSize);
        m_stMetadata.nPpsLen = frameSize;
    }
    else if(H264_NALU_TYPE_IDR == fu->TYPE) {
        /* key frame */
        sendH264KeyFrame(frameSize,nTimeStamp);
    }
    else {
        if(m_bWaitFirstKeyFrame) {
            return;
        }
        m_pRtmpStream->SendH264Packet(fReceiveBuffer,frameSize,false,nTimeStamp);
    }
    return;
}
void ASRtsp2RtmpStreamSink::sendH264KeyFrame(unsigned frameSize, unsigned int nTimeStamp)
{
    if(m_bWaitFirstKeyFrame) {
        if((0 == m_stMetadata.nPpsLen)
            ||(0 == m_stMetadata.nSpsLen)) {
            /* wait the pps sps and idr frame */
            return;;
        }

        /* first key frame and with pps sps frame */
        m_pRtmpStream->SendMetadata(&m_stMetadata);

        m_bWaitFirstKeyFrame = false;
    }

    m_pRtmpStream->SendH264Packet(fReceiveBuffer,frameSize,true,nTimeStamp);

}

// send the H265 frame
void ASRtsp2RtmpStreamSink::sendH265Frame()
{
    //todo:
}

void ASRtsp2RtmpStreamSink::Start()
{
    m_bRunning = true;
}
void ASRtsp2RtmpStreamSink::Stop()
{
    m_bRunning = false;
}


ASRtsp2RtmpClientManager::ASRtsp2RtmpClientManager()
{
    m_ulTdIndex     = 0;
    m_LoopWatchVar  = 0;
    m_ulRecvBufSize = RTSP_SOCKET_RECV_BUFFER_SIZE_DEFAULT;
}

ASRtsp2RtmpClientManager::~ASRtsp2RtmpClientManager()
{
}

int32_t ASRtsp2RtmpClientManager::init()
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
void    ASRtsp2RtmpClientManager::release()
{
    m_LoopWatchVar = 1;
    as_destroy_mutex(m_mutex);
    m_mutex = NULL;
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



AS_HANDLE ASRtsp2RtmpClientManager::openURL(char const* rtspURL,char const* rtmpURL, as_rtsp_callback_t* cb, bool bTcp) {

    as_mutex_lock(m_mutex);
    TaskScheduler* scheduler = NULL;
    UsageEnvironment* env = NULL;
    u_int32_t index =  0;

    index = find_beast_thread();
    env = m_envArray[index];

    //unsigned long ulLen = strlen(rtspURL) - 14;
    //char* pszUrl = new char[ulLen];
    //memset(pszUrl, 0, ulLen);
    //strncpy(pszUrl, rtspURL, ulLen - 1);
    //char*pszUrl = "rtsp://47.97.197.105:557/pag://172.16.0.11:7302:33000000001310000580:0:MAIN:TCP?cnid=2&pnid=2&auth=50&streamform=rtp";
    //char*pszUrl = "rtsp://47.97.197.105:557/pag://172.16.0.11:7302:33000000001310000580:0:MAIN:TCP?cnid=2&pnid=2&auth=50";
    //RTSPClient* rtspClient = ASRtsp2RtmpClient::createNew(index, *env, pszUrl, RTSP_CLIENT_VERBOSITY_LEVEL, RTSP_AGENT_NAME);

    RTSPClient* rtspClient = ASRtsp2RtmpClient::createNew(index,*env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, RTSP_AGENT_NAME);
    if (rtspClient == NULL) {
        as_mutex_unlock(m_mutex);
        return NULL;
    }
    m_clCountArray[index]++;

    ASRtsp2RtmpClient* pRtsp2RtmpClient = (ASRtsp2RtmpClient*)rtspClient;
    pRtsp2RtmpClient->setMediaTcp(bTcp);
    if (AS_ERROR_CODE_OK == pRtsp2RtmpClient->open(rtmpURL,cb))
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

void ASRtsp2RtmpClientManager::setRecvBufSize(u_int32_t ulSize)
{
    m_ulRecvBufSize = ulSize;
}
u_int32_t ASRtsp2RtmpClientManager::getRecvBufSize()
{
    return m_ulRecvBufSize;
}







