
/******************************************************************************
   版权所有 (C), 2008-2011, M.Kernel

 ******************************************************************************
  文件名          : ASLog.h
  版本号          : 1.0
  作者            : 
  生成日期        : 2008-8-17
  最近修改        : 
  功能描述        : AS日志模块用户接口
  函数列表        : 
  修改历史        : 
  1 日期          : 
    作者          : 
    修改内容      : 
*******************************************************************************/


#ifndef _AS_LOG_H_
#define _AS_LOG_H_

#include <stdarg.h>

#define ASLOG_API

//日志等级
typedef enum _ASLogLevel
{
    AS_LOG_EMERGENCY   = 0,    //系统不可用
    AS_LOG_ALERT       = 1,    //必须立刻采取行动，否则系统不可用
    AS_LOG_CRITICAL    = 2,    //严重错误
    AS_LOG_ERROR       = 3,    //一般错误
    AS_LOG_WARNING     = 4,    //警告
    AS_LOG_NOTICE      = 5,    //重要的正常信息
    AS_LOG_INFO        = 6,    //一般的正常信息
    AS_LOG_DEBUG       = 7,    //调试信息
}ASLogLevel;

//设置日志级别，默认是:LOG_INFO
ASLOG_API void ASSetLogLevel(long lLevel);

//设置当前写的日志文件路径名(完整路径或相对路径)
//默认是当前路径下:exename.log
ASLOG_API bool ASSetLogFilePathName(const char* szPathName);

//设置日志文件长度限制，超过此长度时生成新文件，单位KB(100K-100M,默认是10M)
ASLOG_API void ASSetLogFileLengthLimit(unsigned long ulLimitLengthKB);

//上面三项设置完成后，可以启动AS日志模块
ASLOG_API void ASStartLog(void);

//写一条日志(用下面定义的ASWriteLog宏来写日志)
ASLOG_API void __ASWriteLog(const char* szFileName, long lLine,
                             long lLevel, const char* format, va_list argp);
//停止AS日志模块
ASLOG_API void ASStopLog(void);

//vc6以及vc7.1都不支持C99(但g++支持)
//所以这里不能使用可变参数宏定义，利用对()操作符的重载实现
class CWriter   
{
    public:
        CWriter(const char* file, long line)
        {
            m_file_ = file;
            m_line_ = line;
        }
        void operator()(long level, const char* format, ...)
        {
            va_list argp;
            va_start(argp, format);
            __ASWriteLog(m_file_,m_line_,level,format,argp);
            va_end(argp);
        }
    private:
        CWriter()   //过PC-LINT
        {
            m_file_ = NULL;
            m_line_ = 0;
        }
        const char* m_file_;
        long m_line_;
};

//程序中使用如下宏写日志
#define AS_LOG (CWriter(__FILE__, __LINE__))
//例如：AS_LOG(LOG_INFO, "Recv=%d,Send=%d", nRecv,nSend);


#endif//_AS_LOG_H_

