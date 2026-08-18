#include "../inc/common_inc.h"
#include "Model.h"

#if defined(__arm__) || defined(__aarch64__)
#include "TimeSync.h"
#endif

static Page::Model *model;

static void exitCallback(void);

int main(int argc, char *argv[])
{
    // log init
    logger_init("/mnt/UDISK/logs/", "eMP.log", false, 1024 * 1024 * 1, 10);

    log_info("[Sys] eMP_mainPage begin!");

#if defined(__arm__) || defined(__aarch64__)
    // 板子无 RTC，时钟默认 1970 会导致 HTTPS 证书 "not yet valid"；
    // 在 HAL::Init（LVGL tick 启动）之前先对时一次，避免时钟跳变影响 tick
    Net::syncSystemTime();
#endif

    // Init HAL
    HAL::Init();

    // model初始化
    model = new Page::Model(exitCallback);

    for (;;)
    {
        // ...
        usleep(5 * 1000 * 1000);
    }

    return 0;
}

/**
 * @brief 退出回调函数
 */
static void exitCallback(void)
{

    delete model;

    exit(0);
}