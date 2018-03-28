#ifndef __CMEDIADATAQUEUE_H__
#define __CMEDIADATAQUEUE_H__

#include "as_atomic.h"

class CMediaDataBlock
{
public:
    CMediaDataBlock();
    CMediaDataBlock(size_t ulSize);
    virtual ~CMediaDataBlock();
    char *base (void) const;
    char *end (void) const;
    char *rd_ptr (void) const;
    void rd_ptr (char *ptr);
    void rd_ptr (size_t n);
    char *wr_ptr (void) const;
    void wr_ptr (char *ptr);
    void wr_ptr (size_t n);
    /*
    * Message length is (wr_ptr - rd_ptr).
    */
    size_t length (void) const;
    void length (size_t n);
    size_t size (void) const;
    /**
    * Set the number of bytes in the top-level Message_Block,
    * reallocating space if necessary.  However, the @c rd_ptr_ and
    * @c wr_ptr_ remain at the original offsets into the buffer, even if
    * it is reallocated.  Returns 0 if successful, else -1.
    */
    int size (size_t length);
private:
    char         *m_pData;
    size_t        m_ulSize;
    char         *m_rd_ptr;
    char         *m_wr_ptr;
};

class CMediaDataQueue
{
public:
    CMediaDataQueue();

    virtual ~CMediaDataQueue();

    int32_t init(uint32_t unQueueSize);

    void close();

    uint32_t message_count() const;

    bool empty() const;

    bool full() const;

    int32_t enqueue_tail(CMediaDataBlock* mb, const ACE_Time_Value *timeout = NULL);

    int32_t dequeue_head(CMediaDataBlock*& mb, const ACE_Time_Value *timeout = NULL);
private:
    volatile uint32_t      m_WriteIndex;
    volatile uint32_t      m_WriteTag;
    volatile uint32_t      m_ReadIndex;
    volatile uint32_t      m_ReadTag;

    volatile int32_t       m_ActiveFlag;
    uint32_t               m_unMaxQueueSize;
    uint32_t               m_unArraySize;

    volatile uint32_t      m_QueueSize;
    CMediaDataBlock**      m_pDataArray;
};

#endif // __CMEDIADATAQUEUE_H__
