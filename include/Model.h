#pragma once

#include <string>
#include <functional>
#include <dirent.h>
#include <vector>

#include "../libs/lvgl/lvgl.h"
#include "common_inc.h"
#include "View.h"

namespace Page
{
    class Model
    {
    public:
        struct AppInfo
        {
            std::string name;   // 应用程序名称
            std::string exec;   // 应用程序执行文件
            std::string argv;   // 应用程序参数
            std::string icon;   // 应用程序icon(bin)
            std::string config; // 应用程序配置文件(json)
        };

        struct SysConfig
        {
            int brightness;                 // 保存亮度
            int volume;                     // 保存音量
            std::vector<AppInfo> appVector; // 欲安装的应用程序信息
        };

    private:
        pthread_t _pthread;      // 数据处理线程
        bool _threadExitFlag;    // 线程退出标志位
        pthread_mutex_t *_mutex; // 互斥量
        SysConfig _sysConfig;    // 配置信息
        int _legalConfigAppNum;  // 配置文件有效的app个数

        View _view; // View的实例

    private:
        static void *threadProcHandler(void *);
        void runApplication(const char *exec, char *const argv[]);
        void installApplications(std::vector<AppInfo> &appVector);
        static char **stringToArgv(const char *exec, std::string &str);
        bool readConfig(void);
        void saveConfig(void);

    public:
        Model(std::function<void(void)> exitCb, pthread_mutex_t &mutex);
        ~Model();
    };
}