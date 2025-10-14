#include "Model.h"
#include <sys/wait.h>
#include <fstream>
#include "httplib.h"
#include "../utils/cJSON/cJSON.h"
#include "ResourcePool.h"

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
Model::Model(std::function<void(void)> exitCb, pthread_mutex_t &mutex)
{
    _threadExitFlag = false;
    _mutex = &mutex;

    // 设置UI回调函数
    Operations uiOpts = {0};

    uiOpts.exitCb = exitCb;
    uiOpts.runAppCb = std::bind(&Model::runApplication, this, std::placeholders::_1, std::placeholders::_2);

    _view.setOperations(uiOpts);

    /* Initialize resource pool */
    ResourcePool::Init();

    /* 创建UI */
    _view.create();

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
    httplib::Server svr;
    svr.Get("/hi", [](const httplib::Request &, httplib::Response &res)
            { res.set_content("Hello World!", "text/plain"); });
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) 
    { log_debug("%s %s -> %d", req.method.c_str(), req.path.c_str(), res.status); });
    
    usleep(50000);
    
    /* 读取数据 */
    // 读取配置文件
    if (model->readConfig() != true)
    {
        // 写入缺省信息到配置文件
        model->saveConfig();
    }

    /* Initialize Appalication */
    pthread_mutex_lock(model->_mutex);
    model->installApplications(model->_sysConfig.appVector);
    pthread_mutex_unlock(model->_mutex);

    while (!model->_threadExitFlag)
    {
        svr.listen("0.0.0.0", 6210);
        // pthread_mutex_lock(model->_mutex);
        // // ...
        // pthread_mutex_unlock(model->_mutex);

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

    wait(nullptr);           // 阻塞等待子进程返回
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

    char* lastSlash = std::strrchr(exePath, '/');
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
        printf("[Model] icon: %s\n", icon);

        argv = stringToArgv(exec, info.argv);

        // 添加应用程序到UI
        _view.addApplication((name), exec, argv, icon);

        delete[] icon;
        delete[] exec;
    }
}



#include "../libs/lvgl/src/extra/libs/png/lodepng.h"
#include "../utils/lv_100ask_screenshot/save_as_png.h"

#include <stdio.h>      // 错误打印（perror）、标准输入输出
#include <stdlib.h>     // 内存分配/释放（malloc/free）
#include <fcntl.h>      // 文件打开模式（O_RDONLY）
#include <sys/mman.h>   // 内存映射（mmap/munmap）
#include <sys/ioctl.h>  // IO控制（ioctl）
#include <linux/fb.h>   // 帧缓冲结构体（struct fb_var_screeninfo）
#include <unistd.h>     // 延时（usleep）、关闭文件（close）
#include <stdint.h>     // 固定宽度整数类型（uint8_t/uint32_t）
// @brief 对当前LVGL屏幕进行截图，并将截图保存到指定文件
// @param filename 保存截图的文件路径
// @return 操作成功返回0，失败返回-1
int Model::screenshot(const char *filename) 
{
	int fb_fd = open("/dev/fb0", O_RDONLY);
	if (fb_fd < 0) { 
		perror("打开帧缓冲失败"); 
		return -1; 
	}
    struct fb_var_screeninfo vinfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    int width = vinfo.xres;         // 屏幕宽度
    int height = vinfo.yres;        // 屏幕高度
    int bpp = vinfo.bits_per_pixel; // 每像素位数（如16/24/32）
    int line_length = vinfo.xres_virtual * (bpp / 8); // 行字节数
    size_t buffer_size = line_length * height;       // 总缓冲区大小
	
    // 映射帧缓冲到用户空间
    uint8_t *fb_mem = (uint8_t *)mmap(NULL, buffer_size, PROT_READ, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        perror("帧缓冲映射失败");
        close(fb_fd);
        return -1;
    }

    unsigned int screensize = buffer_size; // 声明screensize变量

    // 增强型垂直同步与帧稳定机制
    int vsync = 0;
    int retries = 3;
    bool vsync_supported = (ioctl(fb_fd, FBIO_WAITFORVSYNC, &vsync) >= 0);

    uint8_t *frame_snapshot = NULL; // 声明帧快照指针
    if (vsync_supported) {
        // 成功获取VSYNC，确保在帧开始时读取
        ioctl(fb_fd, FBIO_WAITFORVSYNC, &vsync);
        
        // 捕获完整帧缓冲快照
        frame_snapshot = (uint8_t *)malloc(screensize);
        if (!frame_snapshot) {
            perror("快照内存分配失败");
            munmap(fb_mem, screensize);
            close(fb_fd);
            return -1;
        }
        memcpy(frame_snapshot, fb_mem, screensize);
    } else {
        // VSYNC不受支持，增强型多帧验证机制
        const size_t CHECK_SIZE = 4096; // 扩大比较区域至4KB
        const int STABLE_FRAMES_REQUIRED = 2; // 需要连续2帧稳定
        int stable_count = 0;
        uint8_t *prev_frame = (uint8_t *)malloc(CHECK_SIZE);
        uint8_t *curr_frame = (uint8_t *)malloc(CHECK_SIZE);

        // 初始读取基准帧
        memcpy(prev_frame, fb_mem, CHECK_SIZE);

        for (int i = 0; i < retries * 2; i++) {
            usleep(8000); // 缩短基础延迟，增加采样密度
            memcpy(curr_frame, fb_mem, CHECK_SIZE);

            // 比较当前帧与前一帧
            if (memcmp(prev_frame, curr_frame, CHECK_SIZE) == 0) {
                stable_count++;
                if (stable_count >= STABLE_FRAMES_REQUIRED) break;
            } else {
                stable_count = 0; // 出现变化则重置稳定计数器
            }
            memcpy(prev_frame, curr_frame, CHECK_SIZE); // 更新基准帧
        }

        free(prev_frame);
        free(curr_frame);

        // 最终稳定性保障
        if (stable_count < STABLE_FRAMES_REQUIRED) {
            usleep(40000); // 最后尝试延长等待40ms
        }

        // 捕获完整帧缓冲快照，确保数据一致性
        frame_snapshot = (uint8_t *)malloc(screensize);
        if (!frame_snapshot) {
            perror("快照内存分配失败");
            munmap(fb_mem, screensize);
            close(fb_fd);
            return -1;
        }
        memcpy(frame_snapshot, fb_mem, screensize);
    }
    // 分配目标RGB24缓冲区
    uint8_t *rgb24_buf = (uint8_t *)malloc(width * height * 3);
    if (!rgb24_buf) { perror("内存分配失败"); munmap(fb_mem, buffer_size); close(fb_fd); return -1; }
	
    // 使用静态快照进行像素格式转换（RGB565→RGB888）
    uint32_t *src_ptr = (uint32_t *)frame_snapshot;
    uint8_t *dst_ptr = rgb24_buf;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t pixel = src_ptr[y * (line_length / 4) + x]; // 使用实际宽度计算索引，修复越界访问
            // RGB565转RGB888
            *dst_ptr++ = (pixel >> 16) & 0xFF; // R (从RGBA8888中提取红色通道)
            *dst_ptr++ = (pixel >> 8) & 0xFF;  // G (从RGBA8888中提取绿色通道)
            *dst_ptr++ = pixel & 0xFF;         // B (从RGBA8888中提取蓝色通道)
        }
    }
    // 释放静态快照内存
    free(frame_snapshot);
    frame_snapshot = NULL;

    // 保存为PNG文件
    save_as_png_file(rgb24_buf, width, height, 32, filename);
	// lodepng_encode24_file(filename, (uint8_t*)rgb24_buf, width, height); // 使用24位编码匹配RGB24格式
    // 释放资源
    free(rgb24_buf);
    munmap(fb_mem, buffer_size);
    close(fb_fd);
    return 0;
}
