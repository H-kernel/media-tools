
/******************************************************************************
   版权所有 (C), 2008-2011, M.Kernel

 ******************************************************************************
  文件名          : as_ring_cache.cpp
  版本号          : 1.0
  作者            : hexin
  生成日期        : 2008-08-07
  最近修改        :
  功能描述        : 环形缓冲区
  函数列表        :
  修改历史        :
  1 日期          :
    作者          :
    修改内容      :
*******************************************************************************/


#include "as_ring_cache.h"

as_ring_cache::as_ring_cache()
{
    m_pBuffer = NULL;
    m_ulBufferSize = 0;
    m_ulReader = 0;
    m_ulWriter = 0;
    m_ulDataSize = 0;

    m_pMutex = as_create_mutex();
}

as_ring_cache::~as_ring_cache()
{
    if(NULL != m_pBuffer)
    {
        try
        {
            delete[] m_pBuffer;
            m_pBuffer = NULL;
        }
        catch(...)
        {
            m_pBuffer = NULL;
        }
        m_pBuffer = NULL;
    }
    m_ulBufferSize = 0;
    m_ulReader = 0;
    m_ulWriter = 0;
    m_ulDataSize = 0;

    as_destroy_mutex(m_pMutex);
}

//设置缓冲区大小，返回设置完成后缓冲的大小
unsigned long as_ring_cache::SetCacheSize(unsigned long ulCacheSize)
{
    as_mutex_lock(m_pMutex);
    //清空数据
    m_ulReader = 0;
    m_ulWriter = 0;
    m_ulDataSize = 0;

    //缓冲区大小未发生变化，不需要重新申请内存
    if(ulCacheSize == m_ulBufferSize)
    {
        as_mutex_unlock(m_pMutex);
        return m_ulBufferSize;
    }

    //缓冲区大小发生变化，需要重新申请内存
    //释放当前缓冲内存
    if(NULL != m_pBuffer)
    {
        try
        {
            delete[] m_pBuffer;
            m_pBuffer = NULL;
        }
        catch(...)
        {
            m_pBuffer = NULL;
        }
        m_pBuffer = NULL;
    }

    //申请新缓冲内存
    m_ulBufferSize = ulCacheSize;
    if(m_ulBufferSize > 0)
    {
        try
        {
            m_pBuffer = new char[m_ulBufferSize];
        }
        catch(...)
        {
        }

        if(NULL == m_pBuffer)
        {//申请失败
            m_ulBufferSize = 0;
        }
    }

    as_mutex_unlock(m_pMutex);
    return m_ulBufferSize;
}

//获得当前缓冲区大小
unsigned long as_ring_cache::GetCacheSize() const
{
    return m_ulBufferSize;
}

//查看指定长度数据，但缓冲中仍然保存这些数据，返回实际读取数据长度
//PCLINT注释说明：该函数在本类内部不使用
unsigned long as_ring_cache::Peek(char* pBuf, unsigned long ulPeekLen)/*lint -e1714*/
{
    unsigned long ulResult = 0;

    as_mutex_lock(m_pMutex);

    //计算实际可读取的长度
    ulResult = m_ulDataSize>ulPeekLen?ulPeekLen:m_ulDataSize;
    if(0 == ulResult)
    {
        as_mutex_unlock(m_pMutex);
        return ulResult;
    }

    //数据呈单段分布
    //PCLINT注释说明：本函数主要考虑调用效率，故不做过多的参数检查，由调用者保证
    if(m_ulReader < m_ulWriter)/*lint -e613*/
    {//ooo********ooooo
        //PCLINT注释说明：主要考虑调用效率，不做过多检查
        ::memcpy(pBuf, m_pBuffer+m_ulReader, ulResult);/*lint -e670*/
        as_mutex_unlock(m_pMutex);
        return ulResult;
    }

    //数据呈两段分布，m_ulReader等于m_ulWriter时数据满，也是两段
    //*B*oooooooo**A**
    unsigned long ulASectionLen = m_ulBufferSize - m_ulReader;//A段数据长度
    if(ulResult <= ulASectionLen)//A段数据长度足够
    {
        ::memcpy(pBuf, m_pBuffer+m_ulReader, ulResult);
    }
    else//A段数据长度不够，还需要从B段读取
    {/*lint -e668*/
        //PCLINT注释说明：主要考虑调用效率，不做过多检查
        //先读A段，再从B段补读
        ::memcpy(pBuf, m_pBuffer+m_ulReader, ulASectionLen);
        ::memcpy(pBuf+ulASectionLen, m_pBuffer, ulResult-ulASectionLen);
    }

    as_mutex_unlock(m_pMutex);
    return ulResult;
}

//读取指定长度数据，返回实际读取数据长度
unsigned long as_ring_cache::Read(char* pBuf, unsigned long ulReadLen)
{
    unsigned long ulResult = 0;

    as_mutex_lock(m_pMutex);

    //计算实际可读取的长度
    ulResult = m_ulDataSize>ulReadLen?ulReadLen:m_ulDataSize;
    if(0 == ulResult)
    {
        as_mutex_unlock(m_pMutex);
        return ulResult;
    }

    //数据呈单段分布
    if(m_ulReader < m_ulWriter)
    {//ooo********ooooo
        ::memcpy(pBuf, m_pBuffer+m_ulReader, ulResult);

        //数据被读取，更新读取位置
        m_ulReader += ulResult;/*lint -e414*/
        //PCLINT注释说明：主要考虑调用效率，不做过多检查
        m_ulReader %= m_ulBufferSize;
        //数据已被读取，更新缓冲区数据长度
        m_ulDataSize -= ulResult;

        as_mutex_unlock(m_pMutex);
        return ulResult;
    }

    //数据呈两段分布，m_ulReader等于m_ulWriter时数据满，也是两段
    //*B*oooooooo**A**
    unsigned long ulASectionLen = m_ulBufferSize - m_ulReader;//A段数据长度
    if(ulResult <= ulASectionLen)//A段数据长度足够
    {
        //PCLINT注释说明：主要考虑调用效率，不作过多检查，指针使用无问题
        ::memcpy(pBuf, m_pBuffer+m_ulReader, ulResult);/*lint -e826*/

        //数据被读取，更新读取位置
        m_ulReader += ulResult;
        m_ulReader %= m_ulBufferSize;
    }
    else//A段数据长度不够，还需要从B段读取
    {
        //先读A段，再从B段补读
        ::memcpy(pBuf, m_pBuffer+m_ulReader, ulASectionLen);
        //PCLINT注释说明：主要考虑调用效率，不做过多检查，以确保调用效率
        ::memcpy(pBuf+ulASectionLen, m_pBuffer, ulResult-ulASectionLen);/*lint -e429*/
        m_ulReader = ulResult - ulASectionLen;//数据被读取，更新读取位置
    }
    //数据已被读取，更新缓冲区数据长度
    m_ulDataSize -= ulResult;

    as_mutex_unlock(m_pMutex);
    return ulResult;
}

//写指定长度数据，返回实际写数据长度，若缓冲区空间不够，禁止写入
unsigned long as_ring_cache::Write(const char* pBuf, unsigned long ulWriteLen)
{
    unsigned long ulResult = 0;

    as_mutex_lock(m_pMutex);

    //计算实际可写入长度，若空余缓冲区不够，则不写入数据
    ulResult = (m_ulBufferSize-m_ulDataSize)<ulWriteLen?0:ulWriteLen;
    if(0 == ulResult)
    {
        as_mutex_unlock(m_pMutex);
        return ulResult;
    }

    //空余空间呈单段分布
    if(m_ulReader > m_ulWriter)
    {//***oooooooo*****
        ::memcpy(m_pBuffer+m_ulWriter, pBuf, ulResult);

        //数据已写入，更新写入位置
        m_ulWriter += ulResult;
        m_ulWriter %= m_ulBufferSize;
        //数据已写入，更新缓冲区数据长度
        m_ulDataSize += ulResult;

        as_mutex_unlock(m_pMutex);
        return ulResult;
    }

    //空余空间呈两段分布，m_ulReader等于m_ulWriter时无数据，也是两段分布
    //oBo********ooAoo
    unsigned long ulASectionLen = m_ulBufferSize - m_ulWriter;//A段空余缓冲长度
    if(ulResult <= ulASectionLen)//A段空余缓冲长度足够
    {/*lint -e669*/
        //PCLINT注释说明：主要考虑调用效率，不做过多检查
        ::memcpy(m_pBuffer+m_ulWriter, pBuf, ulResult);

        //数据已写入，更新写入位置
        m_ulWriter += ulResult;
        m_ulWriter %= m_ulBufferSize;
    }
    else//A段空余缓冲长度不够，还要向B段写入
    {
        ::memcpy(m_pBuffer+m_ulWriter, pBuf, ulASectionLen);
        //PCLINT注释说明：主要考虑调用效率，不做过多检查
        ::memcpy(m_pBuffer, pBuf+ulASectionLen, ulResult-ulASectionLen);/*lint !e662*/
        m_ulWriter = ulResult - ulASectionLen;//数据已写入，更新写入位置
    }

    //数据已写入，更新缓冲区数据长度
    m_ulDataSize += ulResult;

    as_mutex_unlock(m_pMutex);
    return ulResult;
}

//获得当前缓冲中数据大小
unsigned long as_ring_cache::GetDataSize() const
{
    return m_ulDataSize;
}

//获得当前空余缓冲大小
unsigned long as_ring_cache::GetEmptySize() const
{
    return (m_ulBufferSize - m_ulDataSize);
}

//清空数据
void as_ring_cache::Clear()
{
    as_mutex_lock(m_pMutex);
    m_ulReader = 0;
    m_ulWriter = 0;
    m_ulDataSize = 0;
    as_mutex_unlock(m_pMutex);
}

//获得当前缓冲区中数据长度和缓冲区长度的比例的百分数
unsigned long as_ring_cache::GetUsingPercent() const
{
    //防止除数为0，异常保护
    if (0 == m_ulBufferSize)
    {
        return 0;
    }

    unsigned long ulCurrentUsingPercent = (m_ulDataSize*100)/m_ulBufferSize;

    return ulCurrentUsingPercent;
}

