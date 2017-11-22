#ifndef AS_BASETYPE_H_INCLUDE
#define AS_BASETYPE_H_INCLUDE
#if (AS_APP_OS == AS_OS_LINUX)
#ifndef SOCKET
typedef int SOCKET; 
#endif
#endif

#ifndef  ULONGLONG
#if (AS_APP_OS == AS_OS_LINUX)
typedef  unsigned long long     ULONGLONG ;
#endif
#if (AS_APP_OS == AS_OS_WIN32)
typedef  unsigned __int64       ULONGLONG; /*lint !e761*///非重复定义,针对不同的操作系统
#endif
#endif

#ifndef  LONGLONG
#if (AS_APP_OS == AS_OS_LINUX)
typedef  long long              LONGLONG;
#endif
#if (AS_APP_OS == AS_OS_WIN32)
typedef  __int64                LONGLONG; /*lint !e761*///非重复定义,针对不同的操作系统
#endif
#endif

#ifndef  ULONG
typedef  unsigned long          ULONG ; /*lint !e761*///非重复定义,针对不同的操作系统
#endif

#ifndef  LONG
typedef  long                   LONG; /*lint !e761*///非重复定义,针对不同的操作系统
#endif

#ifndef  USHORT
typedef  unsigned short         USHORT ; /*lint !e761*///非重复定义,针对不同的操作系统
#endif

#ifndef  SHORT
typedef  short                  SHORT ; /*lint !e761*///非重复定义,针对不同的操作系统
#endif

#ifndef  UCHAR
typedef  unsigned char          UCHAR ; /*lint !e761*///非重复定义,针对不同的操作系统
#endif

#ifndef  CHAR
typedef  char                   CHAR ; /*lint !e761*///非重复定义,针对不同的操作系统
#endif

#ifndef  VOID
typedef  void                   VOID ; /*lint !e761*///非重复定义,针对不同的操作系统
#endif

#ifndef  UINT
typedef  unsigned int          UINT ; /*lint !e761*///非重复定义,针对不同的操作系统
#endif

#ifndef  AS_BOOLEAN
typedef enum _AS_BOOLEAN
{
    AS_TRUE = 1,
    AS_FALSE = 0
}AS_BOOLEAN;
#endif


#ifndef  AS_NULL
#define  AS_NULL               NULL
#endif

#ifndef  AS_SUCCESS
#define  AS_SUCCESS             0
#endif

#ifndef  AS_FAIL
#define  AS_FAIL               -1
#endif

#if AS_BYTE_ORDER == AS_LITTLE_ENDIAN
#define AS_ntohl(x)            ((((x) & 0x000000ff)<<24)|(((x) & 0x0000ff00) << 8) |(((x) & 0x00ff0000)>>8) |(((x) & 0xff000000) >>24))
#define AS_htonl(x)            (AS_ntohl(x))
#else
#define AS_ntohl(x)            (x)
#define AS_htonl(x)            (AS_ntohl(x))
#endif

#if AS_BYTE_ORDER == AS_LITTLE_ENDIAN
#define AS_ntohs(x)            ((((x) & 0x00ff) << 8) |(((x) & 0xff00) >> 8))
#define AS_htons(x)            (AS_ntohs(x))
#else
#define AS_ntohs(x)          (x)
#define AS_htons(x)          (AS_ntohs(x))
#endif

#if AS_BYTE_ORDER == AS_LITTLE_ENDIAN
#define  AS_ntohll( x )        (((AS_ntohl( ((x) & 0xFFFFFFFF)))<< 32) | (AS_ntohl(((x)>>32)&0xFFFFFFFF)))
#define  AS_htonll( x )        (AS_ntohll(x))
#else
#define  AS_ntohll( x )        (x)
#define  AS_htonll( x )        (AS_ntohll(x))
#endif

#define QUEUE_MODE_NOWAIT  0
#define QUEUE_MODE_WAIT    1


#endif //AS_BASETYPE_H_INCLUDE
