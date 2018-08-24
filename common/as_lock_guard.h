
/******************************************************************************
   版权所有 (C), 2008-2011, M.Kernel

 ******************************************************************************
  文件名          : as_lock_guard.h
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


#ifndef CLOCKGUARD_H_INCLUDE
#define CLOCKGUARD_H_INCLUDE    

extern "C"{
#include  "as_mutex.h"
}
class as_lock_guard
{
  public:
    as_lock_guard(as_mutex_t *pMutex);
    virtual ~as_lock_guard();
    
  public:
    static void lock(as_mutex_t *pMutex);
    static void unlock(as_mutex_t *pMutex);
    
 private:
    as_mutex_t *m_pMutex;
};

#endif // CLOCKGUARD_H_INCLUDE


