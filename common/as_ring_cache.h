
/******************************************************************************
   版权所有 (C), 2008-2011, M.Kernel

 *
 *                 环形缓冲区
 *
 *    描述：
 *      1、初步创建 2008-08-07
 *
 *    说明：
 *      1、读写位置说明
 *         reader为数据区头部，即读数据开始位置
 *         writer为空余区头部，即写数据开始位置
 *
 *      2、数据保存主要分两种情况，如下图：
 *
 *         A、   reader      writer
 *                 ↓          ↓
 *           □□□■■■■■■□□□□□□□
 *
 *         B、   writer              reader
 *                 ↓                  ↓
 *           ■■■□□□□□□□□□□■■■
 *
 */

#ifndef _RING_CACHE_H_
#define _RING_CACHE_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
extern "C"{
#include "as_mutex.h"
}
class as_ring_cache
{
    public:
        as_ring_cache();
        virtual ~as_ring_cache();
    public:

        //获得当前缓冲区中数据长度和缓冲区长度的比例的百分数
        unsigned long GetUsingPercent() const;

        //设置缓冲区大小，返回设置完成后缓冲的大小
        unsigned long SetCacheSize(unsigned long ulCacheSize);

        //获得当前缓冲区大小
        unsigned long GetCacheSize() const;

        //查看指定长度数据，但缓冲中仍然保存这些数据，返回实际读取数据长度
        unsigned long Peek(char* pBuf, unsigned long ulPeekLen);

        //读取指定长度数据，并将这些数据从缓冲中清理掉，返回实际读取数据长度
        unsigned long Read(char* pBuf, unsigned long ulReadLen);

        //写指定长度数据，返回实际写数据长度，若缓冲区空间不够，禁止写入
        unsigned long Write(const char* pBuf, unsigned long ulWriteLen);

        //获得当前缓冲中数据大小
        unsigned long GetDataSize() const;

        //获得当前空余缓冲大小
        unsigned long GetEmptySize() const;

        //清空数据
        void Clear();
    private:
        as_mutex_t*      m_pMutex;    //缓冲区访问保护

        char*    m_pBuffer;        //缓冲区
        unsigned long    m_ulBufferSize;    //缓冲区大小
        unsigned long    m_ulDataSize;    //数据长度

        //将缓冲区所有字节从0开始编号，到(m_lBufferSize-1)
        //以下两值就是此编号，即缓冲区偏移值
        unsigned long    m_ulReader;        //数据区头部(数据读取端)
        unsigned long    m_ulWriter;        //空余区头部(数据写入端)
};

#endif
