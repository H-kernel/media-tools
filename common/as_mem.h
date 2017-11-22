#ifndef SVS_COMMON_H_INCLUDE
#define SVS_COMMON_H_INCLUDE

#include <stdlib.h>

template<class T>
T* AS_NEW(T* &m, unsigned long ulMuili = 0)
{
    try
    {
        if (ulMuili == 0)
        {
            m = new T;
        }
        else
        {
            m = new T[ulMuili];
        }
        return m;
    }
    catch(...)
    {
        m = NULL;
        return NULL;
    }
};

enum DELETE_MULTI
{
    SINGLE = 0,
    MULTI = 1
};
    
template<class T>
void AS_DELETE(T* &m, unsigned long ulMuili = 0)
{
    if(NULL == m)
    {
        return;
    }
    
    try
    {
        if (0 == ulMuili)
        {
            delete m;
        }
        else
        {
            delete[] m;
        }
    }
    catch(...)
    {
    }
    
    m = NULL;
};

//安全动态分配单实例内存，生成的是子类，但返回的是父类指针
//类型仅限于继承类
template<class TBASE, class TREAL>
TBASE* AS_NEW_REAL(TBASE* &m)
{
    try
    {
        m = new TREAL;
    }
    catch(...)
    {
        m = NULL;
    }

    return m;
};

//安全释放单实例内存，参数是基类对象指针，但实际删除的是子类对象
//类型仅限于继承类
//若基类的析构函数是虚函数，则可直接使用SVS_DELETE
template<class TBASE, class TREAL>
void AS_DELETE_REAL(TBASE* &m)
{
    try
    {
        TREAL* p = (TREAL*)m; /*lint !e1774*///模板,安全转化
        delete p;
    }
    catch(...)
    {
        m = NULL;
    }

    m = NULL;
};

//将空指针安全转化为真实指针
template<class T>
T* AS_CAST(void* pVoid)
{
    T* pReal = NULL;

    try
    {
        pReal = dynamic_cast<T*>((T*)pVoid);
    }
    catch(...)
    {
        pReal = NULL;
    }

    return pReal;
};

#endif //SVS_COMMON_H_INCLUDE
