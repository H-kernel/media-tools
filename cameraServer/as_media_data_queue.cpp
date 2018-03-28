#include "as_media_data_queue.h"

CMediaDataBlock::CMediaDataBlock()
{
    m_pData   = NULL;
    m_ulSize  = 0;
    m_rd_ptr  = NULL;
    m_wr_ptr  = NULL;
}
CMediaDataBlock::CMediaDataBlock(size_t ulSize)
{
}
CMediaDataBlock::~CMediaDataBlock()
{
}
char *CMediaDataBlock::base (void) const
{
    return m_pData;
}
char *CMediaDataBlock::end (void) const
{
    if(NULL == m_pData)
    {
        return NULL;
    }
    return (m_pData + m_ulSize);
}
char *CMediaDataBlock::rd_ptr (void) const
{
    return m_rd_ptr;
}
void CMediaDataBlock::rd_ptr (char *ptr)
{
    m_rd_ptr = ptr;
}
void CMediaDataBlock::rd_ptr (size_t n)
{
    m_rd_ptr += n;
}
char *CMediaDataBlock::wr_ptr (void) const
{
    return m_wr_ptr;
}
void CMediaDataBlock::wr_ptr (char *ptr)
{
    m_wr_ptr = ptr;
}
void CMediaDataBlock::wr_ptr (size_t n)
{
    m_wr_ptr += n;
}

size_t CMediaDataBlock::length (void) const
{
    return
}
void CMediaDataBlock::length (size_t n)
{
}
size_t CMediaDataBlock::size (void) const
{
}
int CMediaDataBlock::size (size_t length)
{
}


CMediaDataQueue::CMediaDataQueue()
{
    atomic_set(m_ActiveFlag, 1);
    m_unArraySize    = 0;
    m_unMaxQueueSize = 0;

    atomic_set(m_QueueSize, 0);
    m_pDataArray  = NULL;

    atomic_set(m_WriteIndex, 0);
    atomic_set(m_WriteTag, 0);
    atomic_set(m_ReadIndex, 0);
    atomic_set(m_ReadTag, 0);
}

CMediaDataQueue::~CMediaDataQueue()
{
    if (NULL != m_pDataArray)
    {
        delete[] m_pDataArray;
        m_pDataArray = NULL;
    }
}


int32_t CMediaDataQueue::init(uint32_t unQueueSize)
{
    atomic_set(m_ActiveFlag, 1);
    m_unMaxQueueSize = unQueueSize;
    m_unArraySize    = unQueueSize + 1;

    atomic_set(m_QueueSize, 0);
    m_pDataArray  = NULL;

    atomic_set(m_WriteIndex, 0);
    atomic_set(m_WriteTag, 0);
    atomic_set(m_ReadIndex, 0);
    atomic_set(m_ReadTag, 0);

    m_unMaxQueueSize = unQueueSize;
    m_unArraySize    = unQueueSize + 1;

    try
    {
        m_pDataArray = new CMediaDataBlock*[m_unArraySize];
    }
    catch (...)
    {
        return RET_ERR_SYS_NEW;
    }

    memset(m_pDataArray, 0x0, sizeof(CMediaDataBlock*) * m_unArraySize);
    return RET_OK;
}


void CMediaDataQueue::close()
{
    atomic_set(m_ActiveFlag, 0);
}


uint32_t CMediaDataQueue::message_count() const
{
    return (uint32_t) atomic_read(m_QueueSize);
}


bool CMediaDataQueue::empty() const
{
    return (atomic_read(m_QueueSize) == 0);
}


bool CMediaDataQueue::full() const
{
    return ((uint32_t) atomic_read(m_QueueSize) >= m_unMaxQueueSize);
}


int32_t CMediaDataQueue::enqueue_tail(CMediaDataBlock* mb, const ACE_Time_Value *timeout)
{
    if (NULL == mb)
    {
        return RET_FAIL;
    }

    if ((NULL == m_pDataArray) || (0 == m_unArraySize))
    {
        return RET_FAIL;
    }
    bool bFlag               = false;
    bool bTimeOut            = false;
    uint32_t unHeadIndex = 0;

    ACE_Time_Value tvTimeOut;
    ACE_Time_Value startTime = ACE_OS::gettimeofday();
    if (NULL != timeout)
    {
        bTimeOut  = true;
        tvTimeOut = *timeout;
    }

    while (0 != atomic_read(m_ActiveFlag))
    {
        if (bTimeOut && (ACE_OS::gettimeofday() - startTime > tvTimeOut))
        {
            return RET_ERR_TIMEOUT;
        }

        unHeadIndex = (uint32_t)atomic_read(m_WriteIndex);
        uint32_t unTag       = (uint32_t)atomic_read(m_WriteTag);
        if ((unHeadIndex + 1) % m_unArraySize == (uint32_t)atomic_read(m_ReadIndex))
        {
            (void)usleep(1000);
            continue;
        }

        if (NULL != m_pDataArray[unHeadIndex])
        {
            continue;
        }

        if (compare_and_swap2(&m_WriteIndex, unHeadIndex, unTag, (unHeadIndex + 1) % m_unArraySize, unTag + 1))
        {
            bFlag = true;
            break;
        }

    }

    if (bFlag)
    {
        m_pDataArray[unHeadIndex] = mb;
        atomic_inc(&m_QueueSize);

        return RET_OK;
    }

    return RET_FAIL;
}


int32_t CMediaDataQueue::dequeue_head(CMediaDataBlock* &mb, const ACE_Time_Value *timeout)
{
    if ((NULL == m_pDataArray) || (0 == m_unArraySize))
    {
        return RET_FAIL;
    }

    bool bFlag               = false;
    bool bTimeOut            = false;
    uint32_t unTailIndex = 0;
    ACE_Time_Value tvTimeOut;
    ACE_Time_Value startTime = ACE_OS::gettimeofday();
    if (NULL != timeout)
    {
        bTimeOut  = true;
        tvTimeOut = *timeout;
    }

    while (0 != atomic_read(m_ActiveFlag))
    {
        if (bTimeOut && (ACE_OS::gettimeofday() - startTime > tvTimeOut))
        {
            return RET_ERR_TIMEOUT;
        }

        unTailIndex = (uint32_t)atomic_read(m_ReadIndex);
        uint32_t unTag = (uint32_t)atomic_read(m_ReadTag);

        if (unTailIndex == (uint32_t)atomic_read(m_WriteIndex))
        {
            (void)usleep(1000);
            continue;
        }


        if (NULL == m_pDataArray[unTailIndex])
        {
            continue;
        }

        if (compare_and_swap2(&m_ReadIndex, unTailIndex, unTag, (unTailIndex + 1) % m_unArraySize, unTag + 1))
        {
            bFlag = true;
            break;
        }

    }

    if (bFlag)
    {
        mb = m_pDataArray[unTailIndex];
        m_pDataArray[unTailIndex] = 0;
        atomic_dec(&m_QueueSize);

        return RET_OK;
    }

    return RET_FAIL;
}

