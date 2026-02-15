#pragma once

#include <dirent.h>
#include <vector>

#include "View.h"
#include "common_inc.h"
#include "../libs/lvgl/lvgl.h"
#include "../utils/xepoll/xinotify.h"

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
        std::mutex _mutex;                        // 互斥量
        std::thread _threadLvgl;                  // lvgl线程
        std::thread _threadDataProc;              // 数据处理线程
        std::thread _threadHttpSvr;              
        std::condition_variable _cv;              // 条件变量
        std::atomic<bool> _threadExitFlag{false}; // 线程退出标志位
        SysConfig _sysConfig;                     // 配置信息
        int _legalConfigAppNum;                   // 配置文件有效的app个数

        View _view; // View的实例
        Xinotify _inotify;
        lv_timer_t *_timer;

    private:
        /**
         * @brief LVGL处理线程
         */
        void
        threadLvglHandler(void);
        /**
         * @brief data处理线程
         */
        void threadDataProcHandler(void);
        void threadHttpSvrHandler(void);

        void runApplication(const char *exec, char *const argv[]);
        void installApplications(std::vector<AppInfo> &appVector);
        static char **stringToArgv(const char *exec, std::string &str);
        bool readConfig(void);
        void saveConfig(void);
        void udiskNotifyDirHandler(const std::string &path);
        
        void update(void);
        static void onTimerUpdate(lv_timer_t *timer);

    public:
        Model(std::function<void(void)> exitCb);
        ~Model();

        // 获取当前可执行文件所在路径
        static std::string getExeDirectory(void);
        static bool isMounted(const char *mount_point);
    };
}