#ifndef __AS_TIME_H_INCLUDE
#define __AS_TIME_H_INCLUDE

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#include "as_config.h"
#include "as_basetype.h"
#include "as_common.h"


void  as_sleep(uint32_t ulMs );
void  as_start_ticks();
uint32_t as_get_ticks ();
uint32_t as_get_cur_msecond();
void  as_delay (uint32_t ulDelayTimeMs);
void  as_strftime(char * pszTimeStr, unsigned long ulLens, char* pszFormat, time_t ulTime);
time_t as_str2time(const char *pStr);
struct tm* as_Localtime(time_t* ulTime);


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */


#endif // __AS_TIME_H_INCLUDE

