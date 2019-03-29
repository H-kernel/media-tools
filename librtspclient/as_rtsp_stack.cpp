#include "as_rtsp_client.h"
#include "as_rtsp_stack.h"

ASRtspClientHandle::ASRtspClientHandle()
{
    m_hRtspClient = NULL;
}

ASRtspClientHandle::~ASRtspClientHandle()
{

}

int32_t ASRtspClientHandle::open(char* pszUrl)
{
    m_hRtspClient = ASRtspClientManager::instance().openURL(pszUrl,this,true);
    if(NULL == m_hRtspClient) {
        return AS_RTSP_ERROR_FAIL;
    }
    return AS_RTSP_ERROR_OK;
}

void ASRtspClientHandle::close()
{
    if(NULL == m_hRtspClient) {
        return;
    }
    ASRtspClientManager::instance().closeURL(m_hRtspClient);
    m_hRtspClient = NULL;
}


ASRtspClientStack::ASRtspClientStack()
{

}

ASRtspClientStack::~ASRtspClientStack()
{

}

int32_t  ASRtspClientStack::init()
{
    return ASRtspClientManager::instance().init();
}
void     ASRtspClientStack::release()
{
    ASRtspClientManager::instance().release();
}
void     ASRtspClientStack::setRecvBufSize(u_int32_t ulSize)
{
    ASRtspClientManager::instance().setRecvBufSize(ulSize);
}
uint32_t ASRtspClientStack::getRecvBufSize()
{
    return ASRtspClientManager::instance().getRecvBufSize();
}
