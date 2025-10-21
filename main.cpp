#include "../inc/common_inc.h"
#include "Model.h"
#include "udiskMonitor.h"

#include "utils/xepoll/xinotify.h"

static Page::Model *model;

Xinotify inotify_;

void NotifyFileHandler()
{
    log_debug("[xinotify] /mnt/UDISK/test.json file is changed");
}

void NotifyDirHandler()
{
    log_info("[xinotify] /mnt/UDISK dir is changed");
}

static void exitCallback(void);

int main(int argc, char *argv[])
{
    // log init
    logger_init("/mnt/UDISK/logs/", "eMP.log", false, 1024 * 1024 * 1, 10);

    log_info("[Sys] eMP_mainPage begin!");

    std::string testPath = "/mnt/UDISK/test.json";
    int ret = inotify_.AddFileWatch(testPath, std::bind(&NotifyFileHandler));
    if (ret != true)
    {
        log_error("[xinotify] error, can not create inotify!");
    }

    std::string testDirPath = "/mnt/UDISK";
    ret = inotify_.AddDirWatch(testDirPath, std::bind(&NotifyDirHandler));
    if (ret != true)
    {
        log_error("[xinotify] error, can not create inotify!");
    }

    // Init HAL
    HAL::Init();

    // model初始化
    model = new Page::Model(exitCallback);

    // // udisk设备监控启动
    // int ret = UDiskMonitor::getInstance().start(HAL::onUDiskEvent);
    // if (ret != 0)
    // {
    //     std::cerr << "Failed to start UDiskMonitor, error code: " << ret << std::endl;
    // }

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