#include "Model.h"
#include <sys/wait.h>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <mntent.h>
#include <chrono>

#include "httplib.h"
#include "ResourcePool.h"
#include "../utils/cJSON/cJSON.h"

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

    std::string udiskPath = "/mnt/exUDISK";
    int ret = _inotify.AddDirWatch(udiskPath, std::bind(&Page::Model::udiskNotifyDirHandler, this, std::placeholders::_1));
    if (ret != true)
    {
        log_error("[xinotify] error, can not create inotify for %s", udiskPath.c_str());
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
    std::tm local_tm{};
    localtime_r(&now_time_t, &local_tm);
    lv_label_set_text_fmt(_view.ui.timeLabel, "%.2d:%.2d:%.2d", local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
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
    httplib::Server svr;
    svr.Get("/hi", [](const httplib::Request &, httplib::Response &res)
            { res.set_content("Hello World!", "text/plain"); });
    svr.set_logger([](const httplib::Request &req, const httplib::Response &res)
                   { log_debug("%s %s -> %d", req.method.c_str(), req.path.c_str(), res.status); });

    usleep(50000);

    // 读取配置文件
    if (readConfig() != true)
    {
        // 写入缺省信息到配置文件
        saveConfig();
    }

    // Initialize Appalication
    std::unique_lock<std::mutex> lock(_mutex);
    installApplications(_sysConfig.appVector);
    lock.unlock();

    svr.listen("0.0.0.0", 6210);

    while (!_threadExitFlag)
    {
        usleep(50000);
    }

    log_info("[Model] threadDataProcHandler exit!");
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

    // 拷贝 sysconfig.json 的数据内容
    char *buf = new char[4096];
    memset(buf, 0, 4096);
    file.read(buf, 4096);
    file.close();

    // 解析cJSON数据格式
    cJSON *cjson = cJSON_Parse(buf);
    if (cjson != nullptr)
    {
        // 获取数值参数
        int *value[] = {&_sysConfig.brightness, &_sysConfig.volume};
        for (int i = 0; i < sizeof(value) / sizeof(value[0]); i++)
        {
            cJSON *item = cJSON_GetObjectItem(cjson, configNumberItemName[i]);
            if (item != nullptr)
                *(value[i]) = item->valueint;
        }

        // std::string *config_str[] = {&_sysConfig.mainbgFile, &_sysConfig.weatherKey}; // 字符串参数
        // for (int i = 0; i < sizeof(config_str) / sizeof(config_str[0]); i++)
        // {
        //     cJSON *item = cJSON_GetObjectItem(cjson, configStringItemName[i]);
        //     if (item != nullptr)
        //         *(config_str[i]) = std::string(item->valuestring);
        // }

        printf("[Sys] param sysConfig end.\n");

        // 获取应用程序
        cJSON *applications = cJSON_GetObjectItem(cjson, "applications");
        int array_size = cJSON_GetArraySize(applications);
        for (int i = 0; i < array_size; i++)
        {
            cJSON *app_info = cJSON_GetArrayItem(applications, i);

            AppInfo info;
            std::string *app_str[] = {&info.name, &info.exec, &info.argv, &info.icon, &info.config};
            for (int j = 0; j < sizeof(app_str) / sizeof(app_str[0]); j++)
            {
                cJSON *item = cJSON_GetObjectItem(app_info, appInfoItemName[j]);
                if (item != nullptr && item->type != cJSON_NULL)
                    *(app_str[j]) = std::string(item->valuestring);
            }

            if (info.config != "")
                ++_legalConfigAppNum;

            // 向容器插入一个元素
            _sysConfig.appVector.push_back(info);
        }

        printf("[Sys] applications sysConfig end.\n");

        cJSON_Delete(cjson);
    }
    delete[] buf;

    return true;
}

/**
 * @brief 保存设置信息
 * @param sysConfig 保存的设置信息
 */
void Model::saveConfig(void)
{
    cJSON *cjson = cJSON_CreateObject();

    const int *value[] = {&_sysConfig.brightness, &_sysConfig.volume};
    for (int i = 0; i < sizeof(value) / sizeof(value[0]); i++)
        cJSON_AddNumberToObject(cjson, configNumberItemName[i], *value[i]);

    // const std::string *configString[] = {&_sysConfig.mainbgFile, &_sysConfig.weatherKey}; // 字符串参数
    // for (int i = 0; i < sizeof(configString) / sizeof(configString[0]); i++)
    //     cJSON_AddStringToObject(cjson, configStringItemName[i], configString[i]->c_str());

    cJSON *applications = cJSON_CreateArray();

    for (AppInfo &info : _sysConfig.appVector)
    {
        cJSON *appInfo = cJSON_CreateObject();

        cJSON_AddStringToObject(appInfo, appInfoItemName[0], info.name.c_str());
        cJSON_AddStringToObject(appInfo, appInfoItemName[1], info.exec.c_str());
        cJSON_AddStringToObject(appInfo, appInfoItemName[2], info.argv.c_str());
        cJSON_AddStringToObject(appInfo, appInfoItemName[3], info.icon.c_str());
        cJSON_AddStringToObject(appInfo, appInfoItemName[4], info.config.c_str());

        cJSON_AddItemToArray(applications, appInfo);
    }

    cJSON_AddItemToObject(cjson, "applications", applications);

    std::string jsonString(cJSON_Print(cjson));

    std::ofstream file;

    file.open(CONFIG_DIR CONFIG_FILE, std::ios::out); // 写方式打开文件

    file << jsonString << std::endl;

    file.close();

    cJSON_Delete(cjson);
}

/**
 * @brief 运行 app 回调函数
 * @brief exec app 执行文件
 * @param app的main函数参数
 * @note 由于回调函数被UI线程(主线程)执行，因此会阻塞UI线程
 */
void Model::runApplication(const char *exec, char *const argv[])
{
    if (exec == nullptr)
        return;

    pid_t pid = fork(); // 创建子进程

    if (pid == 0) // 子进程
    {
        int ret = execv(exec, argv);
        if (ret < 0)
        {
            printf("[Sys] create %s failed\n", exec);
            exit(0); // 子进程退出
        }
    }

    wait(nullptr); // 阻塞等待子进程返回
    printf("[View] return to mainPage!\n");

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

/**
 * @brief 安装应用程序
 * @param apps 应用程序表
 */
void Model::installApplications(std::vector<AppInfo> &appVector)
{
    for (AppInfo &info : appVector)
    {
        printf("[Model] install application.\n");
        int execLen = info.exec.length();
        int iconLen = info.icon.length();

        char *exec = new char[execLen + 3];
        char *icon = new char[iconLen + 256];

        const char *name = info.name.c_str();
        char **argv;

        sprintf(exec, "./%s", info.exec.c_str());
        sprintf(icon, "%spicture/icon/%s", getExeDirectory().c_str(), info.icon.c_str());
        printf("[Model] exec: %s, icon: %s\n", exec, icon);

        argv = stringToArgv(exec, info.argv);

        // 添加应用程序到UI
        _view.addApplication((name), exec, argv, icon);

        delete[] icon;
        delete[] exec;
    }
}

void Model::udiskNotifyDirHandler(const std::string &path)
{
    log_debug("[xinotify] exUDISK dir content is changed");
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