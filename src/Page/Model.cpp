#include "Model.h"
#include <sys/wait.h>
#include "cJSON.h"
#include <fstream>

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

#define CONFIG_DIR "./configs/"
#define CONFIG_FILE "sysconfig.json"

using namespace Page;

/* 支持的视频文件格式 */
static const char *fileType[] = {".avi", ".mkv", ".flv", ".ts", ".mp4", ".webm", "asf", "mpg", ".mpeg", ".mov", ".vob", ".3gp", ".wmv", ".pmp"};

/**
 * @brief Model构造函数
 *
 * @param exitCb
 * @param mutex
 */
Model::Model(std::function<void(void)> exitCb, pthread_mutex_t &mutex)
{
    _threadExitFlag = false;
    _mutex = &mutex;

    // 设置UI回调函数
    Operations uiOpts = {0};

    uiOpts.exitCb = exitCb;
    uiOpts.runAppCb = std::bind(&Model::runApplication, this, std::placeholders::_1, std::placeholders::_2);

    _view.setOperations(uiOpts);

    pthread_create(&_pthread, NULL, threadProcHandler, this); // 创建执行线程，传递this指针
}

Model::~Model()
{
    _threadExitFlag = true;

    _view.release();
}

/**
 * @brief 线程处理函数
 *
 * @return void*
 */
void *Model::threadProcHandler(void *arg)
{
    Model *model = static_cast<Model *>(arg); // 将arg转换为Model指针
    usleep(50000);

    /* 读取数据 */
    // 读取配置文件
    if (model->readConfig() != true)
    {
        // 写入缺省信息到配置文件
        model->saveConfig();
    }

    /* 创建UI */
    pthread_mutex_lock(model->_mutex);
    model->_view.create();
    model->installApplications(model->_sysConfig.appVector);
    pthread_mutex_unlock(model->_mutex);

    while (!model->_threadExitFlag)
    {

        pthread_mutex_lock(model->_mutex);
        // ...
        pthread_mutex_unlock(model->_mutex);

        usleep(50000);
    }
}

/**
 * @brief 读取设置信息
 * @return true - 读取到了配置信息  false - 缺省值配置信息
 */
bool Model::readConfig(void)
{
    std::ifstream file;
    _legalConfigAppNum = 0;

    // 打开 "./configs/sysconfig.json"
    file.open(CONFIG_DIR CONFIG_FILE, std::ios::in);

    if (file.is_open() != true)
    {
        _sysConfig.brightness = 50; // 缺省值
        _sysConfig.volume = 50;

        AppInfo info = {.name = "eMP_mainPage", .exec = "eMP_mainPage", .argv = "<null>", .icon = "eMP_mainPage.bin", .config = ""};
        _sysConfig.appVector.push_back(info);

        printf("[Sys] Open \"./configs/sysconfig.json\" failed! Please check!\n");

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
        char *icon = new char[iconLen + 14];
        const char *name = info.name.c_str();
        char **argv;

        sprintf(exec, "./%s", info.exec.c_str());
        sprintf(icon, "S:./res/icon/%s", info.icon.c_str());
        argv = stringToArgv(exec, info.argv);

        _view.addApplication((name), exec, argv, icon); // 添加应用程序到UI

        delete[] icon;
        delete[] exec;
    }
}
