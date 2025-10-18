#include "../inc/common_inc.h"
#include "Model.h"
#include "udiskMonitor.h"

static Page::Model *model;
static UDiskMonitor monitor;

static void exitCallback(void);

int main(int argc, char *argv[])
{
    // log init
    logger_init("/mnt/UDISK/logs/", "eMP.log", false, 1024 * 1024 * 1, 10);

    log_info("[Sys] eMP_mainPage begin!");

    // Init HAL
    HAL::Init();

    // model初始化
    model = new Page::Model(exitCallback);

    // udisk设备监控启动
    int ret = monitor.start(HAL::onUDiskEvent);
    if (ret != 0)
    {
        std::cerr << "Failed to start UDiskMonitor, error code: " << ret << std::endl;
    }

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