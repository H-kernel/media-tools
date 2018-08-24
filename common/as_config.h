#ifndef __AS_MEDIA_KENERL_CONFIG_H__
#define __AS_MEDIA_KENERL_CONFIG_H__


#define   AS_OS_LINUX                       1
#define   AS_OS_WIN32                       2

#define   AS_BIG_ENDIAN                     0
#define   AS_LITTLE_ENDIAN                  1


#ifdef ENV_LINUX
#define   AS_APP_OS                     AS_OS_LINUX
#define   AS_BYTE_ORDER                 AS_LITTLE_ENDIAN
#endif

#ifdef WIN32
#define   AS_APP_OS                     AS_OS_WIN32
#define   AS_BYTE_ORDER                 AS_LITTLE_ENDIAN

#ifndef snprintf
#define snprintf _snprintf
#endif
#ifndef strcasecmp
#define strcasecmp stricmp
#endif
#ifndef strncasecmp
#define strncasecmp strnicmp
#endif
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif

#endif

#endif /*__AS_MEDIA_KENERL_CONFIG_H__*/