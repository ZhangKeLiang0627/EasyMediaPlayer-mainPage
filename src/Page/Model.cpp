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

const char *html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>文件上传表单</title>
    <style>
        /* 基础样式重置 */
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }

        /* 渐变背景动画 */
        body {
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            background: linear-gradient(45deg, #4158D0, #C850C0, #FFCC70);
            background-size: 400% 400%;
            animation: gradientAnimation 15s ease infinite;
            padding: 20px;
        }

        @keyframes gradientAnimation {
            0% { background-position: 0% 50%; }
            50% { background-position: 100% 50%; }
            100% { background-position: 0% 50%; }
        }

        /* 表单容器样式 */
        .form-container {
            background-color: rgba(255, 255, 255, 0.9);
            padding: 2.5rem;
            border-radius: 12px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.15);
            width: 100%;
            max-width: 500px;
        }

        /* 表单标题 */
        .form-title {
            color: #333;
            margin-bottom: 1.5rem;
            text-align: center;
            font-size: 1.8rem;
        }

        /* 文件输入容器 */
        .file-input-group {
            margin-bottom: 1.5rem;
        }

        /* 输入框标签 */
        .input-label {
            display: block;
            margin-bottom: 0.5rem;
            color: #555;
            font-weight: 500;
            font-size: 1.1rem;
        }

        /* 文件选择框样式 */
        .file-input {
            width: 100%;
            padding: 0.8rem;
            border: 2px solid #ddd;
            border-radius: 6px;
            background-color: #f9f9f9;
            transition: border-color 0.3s ease;
            font-size: 1rem;
        }

        .file-input:focus {
            outline: none;
            border-color: #6a5acd;
        }

        /* 提交按钮样式 */
        .submit-btn {
            width: 100%;
            padding: 1rem;
            background-color: #6a5acd;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 1.1rem;
            font-weight: 600;
            cursor: pointer;
            transition: background-color 0.3s ease;
            margin-top: 1rem;
        }

        .submit-btn:hover {
            background-color: #554a99;
        }

        /* 响应式调整 */
        @media (max-width: 600px) {
            .form-container {
                padding: 1.5rem;
            }
            
            .form-title {
                font-size: 1.5rem;
            }
        }
    </style>
</head>
<body>
    <div class="form-container">
        <h2 class="form-title">文件上传</h2>
        <form id="formElem">
            <div class="file-input-group">
                <label class="input-label" for="image_file">图片文件 (仅支持图片格式)</label>
                <input type="file" id="image_file" name="image_file" accept="image/*" class="file-input">
            </div>
            
            <div class="file-input-group">
                <label class="input-label" for="text_file">文本文件 (仅支持文本格式)</label>
                <input type="file" id="text_file" name="text_file" accept="text/*" class="file-input">
            </div>
            
            <input type="submit" value="上传文件" class="submit-btn">
        </form>
    </div>

    <script>
        formElem.onsubmit = async (e) => {
            e.preventDefault();
            let res = await fetch('/post', {
                method: 'POST',
                body: new FormData(formElem)
            });
            console.log(await res.text());
            // 可以添加上传成功的提示
            alert('文件上传成功！请查看控制台输出');
        };
    </script>
</body>
</html>
)";

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

// 封装目录列表生成函数
std::string generateDirListHtml(const std::string &current_url_path)
{
    std::string html = R"(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>目录: /public/)" +
                       current_url_path + R"(</title>
    <style>
        body { font-family: 'Segoe UI', sans-serif; max-width: 800px; margin: 2rem auto; padding: 0 1rem; }
        h1 { color: #2d3748; border-bottom: 2px solid #e2e8f0; padding-bottom: 0.5rem; }
        .breadcrumb { margin: 1rem 0; color: #6b7280; font-size: 0.95rem; }
        .breadcrumb a { color: #2b6cb0; text-decoration: none; }
        .breadcrumb a:hover { text-decoration: underline; }
        .file-list { list-style: none; padding: 0; margin-top: 1.5rem; }
        .item { 
            padding: 0.8rem 1rem; 
            margin: 0.5rem 0; 
            border-radius: 6px; 
            transition: background 0.3s;
            display: flex;
            align-items: center;
        }
        .item:hover { background-color: #f7fafc; }
        .item a { 
            text-decoration: none; 
            color: #2b6cb0; 
            font-size: 1.05rem;
            flex: 1;
        }
        .item a:hover { text-decoration: underline; }
        .icon { margin-right: 0.8rem; font-size: 1.2rem; }
        .dir-icon { color: #ecc94b; }
        .file-icon { color: #4a5568; }
        .parent-item { color: #718096; }
    </style>
</head>
<body>
    <h1>目录: /public/)" +
                       current_url_path + R"(</h1>
    <div class="breadcrumb">
        <a href="/public">public</a>
)";

    // 修复1：面包屑导航路径拼接（避免多斜杠）
    if (!current_url_path.empty())
    {
        std::string temp_path;
        std::string remaining_path = current_url_path;
        size_t pos = 0;

        while ((pos = remaining_path.find('/')) != std::string::npos)
        {
            std::string dir_part = remaining_path.substr(0, pos);
            if (dir_part.empty())
            { // 跳过空路径（避免多斜杠导致的错误）
                remaining_path = remaining_path.substr(pos + 1);
                continue;
            }
            temp_path += dir_part + "/"; // 拼接上级目录路径（如 "sub1/"）
            html += " / <a href=\"/public/" + temp_path + "\">" + dir_part + "</a>";
            remaining_path = remaining_path.substr(pos + 1);
        }

        // 处理最后一级目录（如 "sub2"）
        if (!remaining_path.empty())
        {
            html += " / <a href=\"/public/" + temp_path + remaining_path + "\">" + remaining_path + "</a>";
        }
    }

    html += R"(
    </div>
    <ul class="file-list">
)";

    // 修复2：上级目录路径计算（核心！解决跳转失效问题）
    if (!current_url_path.empty())
    {
        std::string parent_url_path;
        size_t last_slash_pos = current_url_path.find_last_of('/');

        if (last_slash_pos == std::string::npos)
        {
            // 情况1：当前是一级目录（如 "sub1"）→ 上级是根目录（空字符串）
            parent_url_path = "";
        }
        else
        {
            // 情况2：当前是多级目录（如 "sub1/sub2"）→ 上级是 "sub1"
            parent_url_path = current_url_path.substr(0, last_slash_pos);
            // 避免上级路径以斜杠结尾（如 "sub1/" → 改为 "sub1"）
            if (!parent_url_path.empty() && parent_url_path.back() == '/')
            {
                parent_url_path.pop_back();
            }
        }

        // 生成上级目录链接（空路径对应 /public，非空路径对应 /public/parent）
        std::string parent_url = parent_url_path.empty() ? "/public" : ("/public/" + parent_url_path);
        html += R"(
        <li class="item parent-item">
            <span class="icon dir-icon">📁</span>
            <a href=")" +
                parent_url + R"(">.. (返回上级目录)</a>
        </li>
        )";
    }

    // 遍历当前目录文件（逻辑不变，确保路径正确）
    std::string current_actual_dir = "./www/" + current_url_path;
    DIR *dir = opendir(current_actual_dir.c_str());
    if (dir)
    {
        dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string name = entry->d_name;
            if (name == "." || name == "..")
                continue;

            // 拼接文件/目录的 URL（避免多斜杠）
            std::string entry_url_path;
            if (current_url_path.empty())
            {
                entry_url_path = name; // 根目录下：直接用文件名（如 "pic.jpg"）
            }
            else
            {
                entry_url_path = current_url_path + "/" + name; // 子目录下：如 "sub1/pic.jpg"
            }
            std::string entry_url = "/public/" + entry_url_path;

            // 判断目录/文件，生成对应链接
            std::string entry_actual_path = current_actual_dir + "/" + name;
            struct stat entry_stat;
            if (stat(entry_actual_path.c_str(), &entry_stat) == 0)
            {
                if (S_ISDIR(entry_stat.st_mode))
                {
                    // 目录：URL 末尾加 /（确保跳转后识别为目录）
                    html += R"(
        <li class="item">
            <span class="icon dir-icon">📁</span>
            <a href=")" + entry_url +
                            R"(/">)" + name + R"(/</a>
        </li>
                    )";
                }
                else
                {
                    // 文件：直接跳转（挂载规则处理）
                    html += R"(
        <li class="item">
            <span class="icon file-icon">📄</span>
            <a href=")" + entry_url +
                            R"(">)" + name + R"(</a>
        </li>
                    )";
                }
            }
        }
        closedir(dir);
    }
    else
    {
        html += R"(
        <li class="item"><span style="color: #dc2626;">❌ 无法访问目录（权限不足或目录不存在）</span></li>
        )";
    }

    html += R"(
    </ul>
</body>
</html>
)";
    return html;
}

/**
 * @brief 线程处理函数
 *
 * @return void*
 */
void *Model::threadProcHandler(void *arg)
{
    Model *model = static_cast<Model *>(arg); // 将arg转换为Model指针

    httplib::Server svr; // 初始化http服务器

    svr.set_logger([](const httplib::Request &req, const httplib::Response &res)
                   { log_debug("%s %s -> %d", req.method.c_str(), req.path.c_str(), res.status); });

    svr.set_error_handler([](const httplib::Request & /*req*/, httplib::Response &res)
                          {
    const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
    char buf[BUFSIZ];
    snprintf(buf, sizeof(buf), fmt, res.status);
    res.set_content(buf, "text/html"); });

    // Mount /public to ./www directory
    svr.set_mount_point("/public", "./www");
    svr.set_file_extension_and_mimetype_mapping("cc", "text/x-c");
    svr.set_file_extension_and_mimetype_mapping("cpp", "text/x-c");
    svr.set_file_extension_and_mimetype_mapping("hh", "text/x-h");
    svr.set_file_extension_and_mimetype_mapping("html", "text/html");
    svr.set_file_extension_and_mimetype_mapping("htm", "text/html");
    svr.set_file_extension_and_mimetype_mapping("css", "text/css");
    svr.set_file_extension_and_mimetype_mapping("js", "text/javascript");
    svr.set_file_extension_and_mimetype_mapping("json", "application/json");
    svr.set_file_extension_and_mimetype_mapping("xml", "application/xml");
    svr.set_file_extension_and_mimetype_mapping("png", "image/png");
    svr.set_file_extension_and_mimetype_mapping("jpg", "image/jpeg");
    svr.set_file_extension_and_mimetype_mapping("jpeg", "image/jpeg");
    svr.set_file_extension_and_mimetype_mapping("gif", "image/gif");
    svr.set_file_extension_and_mimetype_mapping("svg", "image/svg+xml");
    svr.set_file_extension_and_mimetype_mapping("ico", "image/x-icon");
    svr.set_file_extension_and_mimetype_mapping("pdf", "application/pdf");
    svr.set_file_extension_and_mimetype_mapping("zip", "application/zip");
    svr.set_file_extension_and_mimetype_mapping("txt", "text/plain");

    svr.Get("/", [](const httplib::Request &, httplib::Response &res)
            { res.set_content(html, "text/html"); });

    svr.Get(R"(/numbers/(\d+))", [&](const httplib::Request &req, httplib::Response &res)
            {
    auto numbers = req.matches[1];
    res.set_content(numbers, "text/plain"); });

    svr.Get("/hi", [](const httplib::Request &, httplib::Response &res)
            { res.set_content("Hello World!", "text/plain"); });

    svr.Post("/post", [](const httplib::Request &req, httplib::Response &res)
             {
    const auto &image_file = req.form.get_file("image_file");
    const auto &text_file = req.form.get_file("text_file");

    std::cout << "image file length: " << image_file.content.length() << std::endl
         << "image file name: " << image_file.filename << std::endl
         << "text file length: " << text_file.content.length() << std::endl
         << "text file name: " << text_file.filename << std::endl;

    {
      std::ofstream ofs(image_file.filename, std::ios::binary);
      ofs << image_file.content;
    }

    {
      std::ofstream ofs(text_file.filename);
      ofs << text_file.content;
      log_debug("[Svr] content: %s", text_file.content.c_str());
    }

    res.set_content("done", "text/plain"); });

    svr.Get(R"(/public(/.*)?)", [&](const httplib::Request &req, httplib::Response &res)
            {
    // 解析当前 URL 路径（/public 后面的部分）
    std::string current_url_path = req.matches[1];
    if (!current_url_path.empty()) {
        current_url_path = current_url_path.substr(1); // 去掉开头的 /，如 "/subdir" → "subdir"
    }

    // 拼接本地目录路径
    std::string current_actual_dir = "./www/" + current_url_path;
    struct stat path_stat;

    // 情况1：路径不存在 → 返回 404
    if (stat(current_actual_dir.c_str(), &path_stat) != 0) {
        res.status = 404;
        res.set_content("路径不存在：/public/" + current_url_path, "text/plain");
        return;
    }

    // 情况2：是目录 → 生成目录列表网页
    if (S_ISDIR(path_stat.st_mode)) {
        std::string dir_html = generateDirListHtml(current_url_path);
        res.set_content(dir_html, "text/html");
        return;
    }

    // 情况3：是文件 → 直接返回 404 让挂载规则接管（关键！）
    // 因为 /public 已挂载到 ./www，框架会自动处理 /public/xxx 的文件请求
    // 这里不需要手动处理，直接让路由“不匹配”即可（返回 404 触发框架默认处理）
    res.status = 404; });

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
