#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "as_rtsp2sip_client.h"
#include "as_config.h"
#if AS_APP_OS == AS_OS_LINUX
#include "as_daemon.h"
#endif
#include "as_common.h"
#include "as_time.h"


long creat_daemon(void);
void workFunc();
void server_exit();


int main(int argc, char *argv[])
{
#if AS_APP_OS == AS_OS_LINUX
    RUNNING_MOD runType = enBackGround;
    if (argc > 1 &&
        (0 == strncmp(argv[1], "-t", strlen("-t"))))
    {
        runType = enForeGround;
    }

    as_run_service(workFunc, runType, server_exit,RTSP2SIP_LOG_FILE, 99);
#else
    workFunc();
#endif
    as_sleep(10000);
    while (true)
    {
        as_sleep(100);
    }
    return 0;
}

void startexit()
{
#if AS_APP_OS == AS_OS_LINUX
    send_sigquit_to_deamon();
#endif
}


void server_exit()
{
    ASRtsp2SiptManager::instance().close();
    ASRtsp2SiptManager::instance().release();
    return;
}

void workFunc()
{
    int32_t ret = ASRtsp2SiptManager::instance().init();
    if (AS_ERROR_CODE_OK != ret)
    {
        return ;
    }
    ret = ASRtsp2SiptManager::instance().open();
    if (AS_ERROR_CODE_OK != ret)
    {
        return ;
    }
    
    return ;
}
