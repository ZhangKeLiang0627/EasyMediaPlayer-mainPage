#include "Model.h"
#include <sys/wait.h>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <mntent.h>
#include <chrono>
#include <sys/stat.h>

#include "httplib.h"
#include "httpServer.h"
#include "ResourcePool.h"
#include "Launch/desktop_entry.h"
#include "Launch/app_scanner.h"
#include "Launch/app_runner.h"
#include <nlohmann/json.hpp>

#if defined(__arm__) || defined(__aarch64__)
#include "TimeSync.h"
#endif

using nlohmann::json;

static const char *configNumberItemName[] =
    {
        "brightness",
        "volume",
};

static const char *appInfoItemName[] =
    {
        "name",
        "exec",
        "argv",
        "icon",
        "config",
};

#define CONFIG_DIR "./config/"
#define CONFIG_FILE "sysconfig.json"
#define APPS_DIR "/mnt/UDISK/applications"  // .desktop 应用描述目录
#define TIME_SYNC_INTERVAL_SEC 60           // 板子周期对时间隔
#define INOTIFY_POLL_US 200000              // inotify 事件轮询间隔（200ms）

using namespace Page;

/**
 * @brief Model构造函数
 *
 * @param exitCb
 * @param mutex
 */
Model::Model(std::function<void(void)> exitCb)
{
    _threadExitFlag = false;

    // 设置UI回调函数
    Operations uiOpts = {0};

    uiOpts.exitCb = exitCb;
    uiOpts.runAppCb = std::bind(&Model::runApplication, this, std::placeholders::_1, std::placeholders::_2);

    _view.setOperations(uiOpts);

    // 监听 U 盘挂载点（图标显隐用）
    std::string udiskPath = "/mnt/exUDISK";
    int ret = _inotify.AddDirWatch(udiskPath, std::bind(&Page::Model::udiskNotifyDirHandler, this, std::placeholders::_1));
    if (ret != true)
    {
        log_error("[xinotify] error, can not create inotify for %s", udiskPath.c_str());
    }

    // 监听 .desktop 应用描述目录（插 U 盘 / 拷入新应用自动发现）
    _appsDir = APPS_DIR;
    mkdir(_appsDir.c_str(), 0777); // 目录不存在则创建（inotify 要求目录存在）
    ret = _inotify.AddDirWatch(_appsDir, std::bind(&Page::Model::appsNotifyDirHandler, this, std::placeholders::_1));
    if (ret != true)
    {
        log_error("[xinotify] error, can not create inotify for %s", _appsDir.c_str());
    }

    /* Initialize resource pool */
    ResourcePool::Init();

    /* 创建UI */
    _view.create();

    // 这里设置一个1000ms的定时器，软定时器，用于在onTimerUpdate里update
    _timer = lv_timer_create(onTimerUpdate, 1000, this);
    update();

    // 创建lvgl处理线程，传递this指针
    _threadLvgl = std::thread([](Model *pThis)
                              { pThis->threadLvglHandler(); }, this);
    _threadLvgl.detach();

    // 创建data处理线程，传递this指针
    _threadDataProc = std::thread([](Model *pThis)
                                  { pThis->threadDataProcHandler(); }, this);

    // 创建httpsvr线程，传递this指针
    _threadHttpSvr = std::thread([](Model *pThis)
                                 { pThis->threadHttpSvrHandler(); }, this);
    _threadHttpSvr.detach();

    // 若设置了 EMP_AUTOSHOT 环境变量，则延时自动截图（用于无显示环境验证 / 生成文档图）
    const char *autoShot = getenv("EMP_AUTOSHOT");
    if (autoShot != nullptr)
    {
        int sec = atoi(autoShot);
        if (sec <= 0)
            sec = 3;
        lv_timer_t *shotTimer = lv_timer_create([](lv_timer_t *timer)
                                                 {
            Model *pThis = (Model *)timer->user_data;
            View::takeScreenshot(); },
                                                 sec * 1000, this);
        lv_timer_set_repeat_count(shotTimer, 1);
        log_info("[Model] auto screenshot scheduled in %ds", sec);
    }

    // _cv.notify_all();
}

Model::~Model()
{
    _threadExitFlag = true;
    // _cv.notify_all(); // 唤醒休眠中的线程，使其立即检查退出标志

    // 等待线程退出，回收资源
    if (_threadDataProc.joinable())
    {
        log_info("[Model] joining _threadDataProc...");
        _threadDataProc.join();
        log_info("[Model] _threadDataProc joined");
    }

    _view.release();
    lv_timer_del(_timer);

    log_info("[Model] ~Model exit!");
}

/**
 * @brief 定时器更新函数
 *
 */
void Model::onTimerUpdate(lv_timer_t *timer)
{
    Model *instance = (Model *)timer->user_data;

    instance->update();
}

/**
 * @brief 更新UI等事务
 *
 */
void Model::update(void)
{
    // 轮询U盘挂载情况
    _view.setUdisk(isMounted("/mnt/exUDISK"));

    // 更新时间
    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    static std::tm local_tm{};
    localtime_r(&now_time_t, &local_tm);
    lv_label_set_text_fmt(_view.ui.timeCont.timeLabel, "%.2d:%.2d:%.2d", local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
    lv_label_set_text_fmt(_view.ui.timeCont.dateLabel, "%d/%d/%d", local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday);
}

/**
 * @brief LVGL处理线程
 */
void Model::threadLvglHandler(void)
{
    while (!_threadExitFlag)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        uint32_t ms = lv_task_handler();
        lock.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    log_info("[Model] threadLvglHandler exit!");
}

/**
 * @brief 线程处理函数
 */
void Model::threadDataProcHandler(void)
{
    // 读取配置文件
    if (readConfig() != true)
    {
        // 写入缺省信息到配置文件
        saveConfig();
    }

    // Initialize Appalication（.desktop 优先 + sysconfig 兜底合并）
    std::unique_lock<std::mutex> lock(_mutex);
    installApplications();
    lock.unlock();

    uint32_t timeSyncCountdown = TIME_SYNC_INTERVAL_SEC;

    while (!_threadExitFlag)
    {
        usleep(INOTIFY_POLL_US);

        // inotify 事件由 epoll 循环线程（Xinotify 构造时注册到 MY_EPOLL）统一消费，
        // 本线程不再调用 HandleEvent()：双消费者会与 epoll 线程争抢同一 fd 的 read，
        // 且退出时 join 易卡死。此循环仅保留周期对时（板子无 RTC）。

#if defined(__arm__) || defined(__aarch64__)
        // 板子无 RTC，每 60s 周期对时（main() 里已启动对时一次）
        if (--timeSyncCountdown == 0)
        {
            timeSyncCountdown = TIME_SYNC_INTERVAL_SEC;
            Net::syncSystemTime();
        }
#endif
    }

    log_info("[Model] threadDataProcHandler exit!");
}

/**
 * @brief 线程处理函数
 */
void Model::threadHttpSvrHandler(void)
{
    HttpServer svr;

    svr.init();

    log_info("[Model] threadHttpSvrHandler start!");

    svr.getHttpServer().listen("0.0.0.0", 6210);

    log_info("[Model] threadHttpSvrHandler exit!");
}

/**
 * @brief 读取设置信息
 * @return true - 读取到了配置信息  false - 缺省值配置信息
 */
bool Model::readConfig(void)
{
    std::ifstream file;
    _legalConfigAppNum = 0;

    // 打开 "./config/sysconfig.json"
    file.open(CONFIG_DIR CONFIG_FILE, std::ios::in);

    if (file.is_open() != true)
    {
        _sysConfig.brightness = 50; // 缺省值
        _sysConfig.volume = 50;

        AppInfo info = {.name = "nullAPPHere", .exec = "<null>", .argv = "<null>", .icon = "null.bin", .config = ""};
        _sysConfig.appVector.push_back(info);

        printf("[Sys] Open \"./config/sysconfig.json\" failed! Please check!\n");

        return false;
    }

    try
    {
        json j = json::parse(file); // 流式解析，天然避免 4KB 截断问题
        file.close();

        _sysConfig.brightness = j.value("brightness", 50);
        _sysConfig.volume = j.value("volume", 50);

        _sysConfig.appVector.clear();
        if (j.contains("applications") && j["applications"].is_array())
        {
            for (const auto &app : j["applications"])
            {
                AppInfo info;
                info.name = app.value("name", "");
                info.exec = app.value("exec", "");
                info.argv = app.value("argv", "");
                info.icon = app.value("icon", "");
                info.config = app.value("config", "");

                if (info.config != "")
                    ++_legalConfigAppNum;

                _sysConfig.appVector.push_back(info);
            }
        }

        printf("[Sys] param sysConfig end.\n");
        printf("[Sys] applications sysConfig end.\n");
    }
    catch (const std::exception &e)
    {
        printf("[Sys] parse sysconfig.json failed: %s\n", e.what());
        file.close();
        return false;
    }

    return true;
}

/**
 * @brief 保存设置信息
 * @param sysConfig 保存的设置信息
 */
void Model::saveConfig(void)
{
    json j;

    j["brightness"] = _sysConfig.brightness;
    j["volume"] = _sysConfig.volume;

    json applications = json::array();
    for (const AppInfo &info : _sysConfig.appVector)
    {
        json appInfo;
        appInfo["name"] = info.name;
        appInfo["exec"] = info.exec;
        appInfo["argv"] = info.argv;
        appInfo["icon"] = info.icon;
        appInfo["config"] = info.config;
        applications.push_back(appInfo);
    }
    j["applications"] = applications;

    std::ofstream file;
    file.open(CONFIG_DIR CONFIG_FILE, std::ios::out); // 写方式打开文件
    file << j.dump(4) << std::endl;
    file.close();
}

/**
 * @brief 运行 app 回调函数
 * @brief exec app 执行文件
 * @param app的main函数参数
 * @note 由 UI 线程触发；内部经 Launch::runApplication 阻塞等待子进程退出
 */
void Model::runApplication(const char *exec, char *const argv[])
{
    if (exec == nullptr)
        return;

    /* 收集参数（argv[0] 是 exec，跳过） */
    std::vector<std::string> args;
    if (argv != nullptr)
    {
        for (int i = 1; argv[i] != nullptr && i < 5; i++)
            args.push_back(argv[i]);
    }

    /* 实例锁：/tmp/<可执行名>.lock，防止同一应用重复启动 */
    std::string lockName = "/tmp/";
    std::string execStr(exec);
    size_t slash = execStr.find_last_of('/');
    if (slash != std::string::npos)
        lockName += execStr.substr(slash + 1);
    else
        lockName += execStr;
    lockName += ".lock";

    int result = Launch::runApplication(exec, args, lockName);
    if (result == -2)
    {
        printf("[Model] %s already running, skip\n", exec);
        return;
    }

    printf("[View] return to mainPage! (exit=%d)\n", result);

    // lv_async_call([](void *data){
    //     Model *model = (Model *)data;
    //     model->_view.appearAnimStart(false);}, this);

    _view.appearAnimStart(false); // 触发UI动画
}

/**
 * @brief 将字符串参数转为 char**
 * @return 带有应用程序执行路径的完整argv
 * @最大支持5个参数
 */
char **Model::stringToArgv(const char *exec, std::string &str)
{
    int i = 0;
    size_t dataStart = 0;
    size_t dataEnd = 0;
    std::string dataStr = "";

    char **argv = new char *[5];
    memset(argv, 0, 5 * sizeof(char *)); // 全部置空，避免后续 delete[] 垃圾指针

    int len = strlen(exec) + 1;
    argv[0] = new char[len];
    sprintf(argv[i++], "%s", exec);

    do
    {
        dataStart = str.find('<', dataEnd); // 寻找 < 字符
        if (dataStart != std::string::npos)
        {
            dataStart += 1;
            dataEnd = str.find('>', dataStart); // 寻找 >
            if (dataEnd != std::string::npos)
            {
                dataStr = str.substr(dataStart, dataEnd - dataStart);
                if (dataStr != "null")
                {
                    int len = dataStr.length() + 1;
                    argv[i] = new char[len];
                    sprintf(argv[i], "%s", dataStr.c_str());
                }
                else
                {
                    argv[i] = nullptr;
                    break;
                }
            }
        }
    } while (++i < 5);

    return argv;
}

std::string Model::getExeDirectory(void)
{
    const size_t bufSize = 1024;
    char exePath[bufSize] = {0};

    const ssize_t len = readlink("/proc/self/exe", exePath, bufSize - 1);
    if (len == -1)
    {
        throw std::runtime_error("Failed to read executable path");
    }
    exePath[len] = '\0';

    char *lastSlash = std::strrchr(exePath, '/');
    if (!lastSlash)
    {
        throw std::runtime_error("Invalid executable path format");
    }
    *lastSlash = '\0';

    return std::string(exePath) + '/';
}

namespace
{
/* 释放 stringToArgv 产生的 char**（含每个元素） */
void freeArgv(char **argv)
{
    if (argv == nullptr)
        return;
    for (int i = 0; i < 5 && argv[i] != nullptr; i++)
        delete[] argv[i];
    delete[] argv;
}
} // namespace

/**
 * @brief 安装应用程序：.desktop 自动发现优先，sysconfig.json 的 applications 兜底合并
 * @note 由 data 线程调用（持 _mutex），增量添加：已存在的 exec 跳过
 */
void Model::installApplications(void)
{
    /* 1. 扫描 /mnt/UDISK/applications/*.desktop（自动发现） */
    std::vector<Launch::DesktopEntry> entries = Launch::scanApplications(_appsDir);

    for (const auto &entry : entries)
    {
        /* Exec 的第一个 token 是可执行路径，其余为参数 */
        std::string exec = entry.exec;
        std::string args;
        size_t space = exec.find(' ');
        if (space != std::string::npos)
        {
            args = exec.substr(space + 1);
            exec = exec.substr(0, space);
        }

        if (_installedExec.count(exec) > 0)
            continue; // 已添加

        printf("[Model] install(desktop): %s -> %s\n", entry.name.c_str(), exec.c_str());

        char *execDup = strdup(exec.c_str());
        char **argv = stringToArgv(execDup, args);

        /* 图标：desktop 的 Icon 相对路径时拼 exe 目录；空则给空串（LVGL 无害处理） */
        std::string iconPath = entry.icon;
        if (!iconPath.empty() && iconPath[0] != '/')
            iconPath = getExeDirectory() + iconPath;
        const char *iconSrc = iconPath.empty() ? nullptr : iconPath.c_str();

        _view.addApplication(entry.name.c_str(), execDup, argv, (void *)iconSrc);

        _installedExec.insert(exec);

        freeArgv(argv);
        free(execDup);
    }

    /* 2. sysconfig.json 的 applications 兜底（向后兼容） */
    for (const AppInfo &info : _sysConfig.appVector)
    {
        if (info.exec.empty() || info.exec == "<null>")
            continue;

        std::string execPath = info.exec;
        /* 相对路径统一转绝对路径（旧配置里是 ./eMP_xxx） */
        if (execPath.size() >= 2 && execPath[0] == '.' && execPath[1] == '/')
            execPath = getExeDirectory() + execPath.substr(2);

        if (_installedExec.count(execPath) > 0)
            continue; // 与 desktop 条目重复，跳过

        printf("[Model] install(config): %s -> %s\n", info.name.c_str(), execPath.c_str());

        char *exec = strdup(execPath.c_str());
        std::string argsStr = info.argv;
        char **argv = stringToArgv(exec, argsStr);

        /* 图标：相对路径拼 exe 目录 + picture/icon/（保持原约定） */
        std::string iconPath = info.icon;
        if (!iconPath.empty() && iconPath[0] != '/')
            iconPath = getExeDirectory() + "picture/icon/" + iconPath;
        const char *iconSrc = iconPath.empty() ? nullptr : iconPath.c_str();

        _view.addApplication(info.name.c_str(), exec, argv, (void *)iconSrc);

        _installedExec.insert(execPath);

        freeArgv(argv);
        free(exec);
    }
}

/**
 * @brief 重载应用列表（inotify 事件触发）：增量添加新发现的 .desktop 应用
 */
void Model::reloadApplications(void)
{
    std::unique_lock<std::mutex> lock(_mutex);
    installApplications();
    lock.unlock();
}

void Model::appsNotifyDirHandler(const std::string &path)
{
    log_info("[xinotify] applications dir changed: %s", path.c_str());
    reloadApplications();
}

void Model::udiskNotifyDirHandler(const std::string &path)
{
    log_debug("[xinotify] exUDISK dir content is changed");
    /* U 盘内容变化也可能带来新的 .desktop 应用，顺带重扫一次 */
    reloadApplications();
}

/**
 * @brief 判断指定路径是否已挂载
 * @param mount_point 待检查的挂载点（如/mnt/exUDISK）
 * @return true-已挂载，false-未挂载
 */
bool Model::isMounted(const char *mount_point)
{
    if (!mount_point || strlen(mount_point) == 0)
    {
        return false;
    }

    // 打开/proc/mounts（内核实时挂载表）
    FILE *mnt_fp = setmntent("/proc/mounts", "r");
    if (!mnt_fp)
    {
        perror("setmntent failed");
        return false;
    }

    struct mntent *mnt_entry = nullptr;
    bool mounted = false;

    // 遍历挂载表，匹配挂载点
    while ((mnt_entry = getmntent(mnt_fp)) != nullptr)
    {
        if (strcmp(mnt_entry->mnt_dir, mount_point) == 0)
        {
            mounted = true;
            break;
        }
    }

    endmntent(mnt_fp); // 关闭文件
    return mounted;
}