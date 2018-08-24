
/******************************************************************************
   版权所有 (C), 2008-2011, M.Kernel

 ******************************************************************************
  文件名          : as_lock_guard.cpp
  版本号          : 1.0
  作者            :
  生成日期        : 2008-8-17
  最近修改        :
  功能描述        : 实现智能锁功能
  函数列表        :
  修改历史        :
  1 日期          :
    作者          :
    修改内容      :
*******************************************************************************/



#include "as_lock_guard.h"

as_lock_guard::as_lock_guard(as_mutex_t *pMutex)
{
    m_pMutex = NULL;

    if(NULL == pMutex)
    {
        return;
    }

    m_pMutex = pMutex;

    (void)as_mutex_lock(m_pMutex);
}

as_lock_guard::~as_lock_guard()
{
    if(NULL == m_pMutex)
    {
        return;
    }
    (void)as_mutex_unlock(m_pMutex);

    m_pMutex = NULL;
}

/*******************************************************************************
Function:       // as_lock_guard::lock
Description:    // 加锁
Calls:          //
Data Accessed:  //
Data Updated:   //
Input:          // as_mutex_t *pMutex
Output:         // 无
Return:         // 无
Others:         // 无
*******************************************************************************/
void as_lock_guard::lock(as_mutex_t *pMutex)
{
    if(NULL == pMutex)
    {
        return;
    }
    (void)as_mutex_lock(pMutex);
}

/*******************************************************************************
Function:       // as_lock_guard::unlock
Description:    // 释放锁
Calls:          //
Data Accessed:  //
Data Updated:   //
Input:          // AS_Mutex *pMutex
Output:         // 无
Return:         // 无
Others:         // 无
*******************************************************************************/
void as_lock_guard::unlock(as_mutex_t *pMutex)
{
    if(NULL == pMutex)
    {
        return;
    }
    (void)as_mutex_unlock(pMutex);
}


