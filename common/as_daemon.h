#ifndef __AS_DAEMON_H__
#define __AS_DAEMON_H__

#define DAEMO_SUCCESS    0
#define DAEMO_FAIL       -1

typedef enum
{
    enForeGround          = 0, //前台运行
    enBackGround          = 1 //后台运行
}RUNNING_MOD;


/*iRunningMod : 1  守护进程模式，0 前台模式*/
void as_run_service(void(*pWorkFunc)(),
                         int32_t iRunningMod,
                         void (*pExitFunc)(),
                         const char* service_conf_path,
            int32_t service_id );
//退出
void send_sigquit_to_deamon();

#endif // __AS_DAEMON_H__


