#include <fstream>
#include <iostream>

#include "httpServer.h"
#include "../../utils/FileOperations/FileOperations.h"
#include "../../utils/log/log.h"

HttpServer::HttpServer()
{
    log_info("[HttpServer] HttpServer create!");
}

HttpServer::~HttpServer()
{
    log_info("[HttpServer] ~HttpServer exit!");
}

void HttpServer::init()
{
    std::string temp;
    loadHTML(temp);

    svr.set_logger([](const httplib::Request &req, const httplib::Response &res)
                   { log_debug("%s %s -> %d", req.method.c_str(), req.path.c_str(), res.status); });

    svr.set_error_handler([](const httplib::Request & /*req*/, httplib::Response &res)
                          {
    const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
    char buf[512];
    snprintf(buf, sizeof(buf), fmt, res.status);
    res.set_content(buf, "text/html"); });

    // Mount /public to ./www directory
    svr.set_mount_point("/public/", "./www/");
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
            { res.set_content("Hello World!", "text/plain"); });

    svr.Get("/upload", [this](const httplib::Request &, httplib::Response &res)
            { res.set_content(this->htmlUpload, "text/html"); });

    svr.Post("/post", [](const httplib::Request &req, httplib::Response &res)
             {
    const auto &image_file = req.form.get_file("image_file");
    const auto &text_file = req.form.get_file("text_file");

    log_info("[Svr] image file: name=%s, length=%zu; text file: name=%s, length=%zu",
             image_file.filename.c_str(), image_file.content.length(),
             text_file.filename.c_str(), text_file.content.length());

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
    // 解析请求路径，统一转换为带/的目录格式
    std::string req_path = req.path;
    // 移除/public前缀，得到相对路径
    if (req_path.find("/public") == 0) {
        req_path = req_path.substr(7); // 7是"/public"的长度
    }
    // 统一清理路径（移除首尾/）
    req_path.erase(0, req_path.find_first_not_of('/'));
    req_path.erase(req_path.find_last_not_of('/') + 1);

    // 拼接本地目录路径
    std::string current_actual_dir = "./www/";
    if (!req_path.empty()) {
        current_actual_dir += req_path + "/";
    }
    struct stat path_stat;

    // 情况1：路径不存在 → 返回404
    if (stat(current_actual_dir.c_str(), &path_stat) != 0) {
        res.status = 404;
        res.set_content("路径不存在：/public/" + req_path, "text/plain");
        return;
    }

    // 情况2：是目录 → 生成目录列表网页（强制用带/的路径）
    if (S_ISDIR(path_stat.st_mode)) {
        std::string dir_html = generateDirListHtml(req_path);
        res.set_content(dir_html, "text/html");
        return;
    }

    // 情况3：是文件 → 让挂载规则接管
    res.status = 404; });
}

void HttpServer::start(const std::string &host, int port)
{
    svr.listen(host, port);
}

std::string HttpServer::generateDirListHtml(const std::string &current_url_path)
{
    std::string clean_path = current_url_path;
    clean_path.erase(0, clean_path.find_first_not_of('/'));
    if (!clean_path.empty())
    {
        clean_path.erase(clean_path.find_last_not_of('/') + 1);
    }

    std::string html = R"(
    <!DOCTYPE html>
    <html lang="zh-CN">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>目录: /public/)" +
                       clean_path + R"(</title>
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
                       clean_path + R"(</h1>
        <div class="breadcrumb">
            <a href="/public/">public</a>
    )";

    // 面包屑导航 - 基于统一的clean_path构建，所有目录链接带/
    if (!clean_path.empty())
    {
        std::string temp_path;
        std::string remaining_path = clean_path;
        size_t pos = 0;

        while ((pos = remaining_path.find('/')) != std::string::npos)
        {
            std::string dir_part = remaining_path.substr(0, pos);
            if (dir_part.empty())
            {
                remaining_path = remaining_path.substr(pos + 1);
                continue;
            }
            temp_path += dir_part + "/";
            // 面包屑链接强制带/
            html += " / <a href=\"/public/" + temp_path + "\">" + dir_part + "</a>";
            remaining_path = remaining_path.substr(pos + 1);
        }

        if (!remaining_path.empty())
        {
            // 最后一级目录链接也强制带/
            html += " / <a href=\"/public/" + temp_path + remaining_path + "/\">" + remaining_path + "</a>";
        }
    }

    html += R"(
    </div>
    <ul class="file-list">
    )";

    if (!clean_path.empty())
    {
        std::string parent_url_path;
        size_t last_slash_pos = clean_path.find_last_of('/');

        if (last_slash_pos == std::string::npos)
        {
            // 情况1：当前是一级目录（如 "album"）→ 上级是public根目录
            parent_url_path = "";
        }
        else
        {
            // 情况2：当前是多级目录（如 "album/photo"）→ 上级是 "album"
            parent_url_path = clean_path.substr(0, last_slash_pos);
        }

        // 关键：上级目录链接强制带/，确保一次跳转到位
        std::string parent_url = "/public/";
        if (!parent_url_path.empty())
        {
            parent_url += parent_url_path + "/";
        }

        html += R"(
        <li class="item parent-item">
            <span class="icon dir-icon">📁</span>
            <a href=")" +
                parent_url + R"(">.. (返回上级目录)</a>
        </li>
        )";
    }

    // 拼接本地目录路径
    std::string current_actual_dir = "./www/";
    if (!clean_path.empty())
    {
        current_actual_dir += clean_path;
    }
    // 确保本地目录路径末尾有/，避免拼接错误
    if (!current_actual_dir.empty() && current_actual_dir.back() != '/')
    {
        current_actual_dir += "/";
    }

    try
    {
        std::error_code ec;
        // 1. 检查路径是否存在（通过 getFileType 判断是否为 NON_EXIST）
        FileOperations::FileType path_type = FileOperations::getFileType(current_actual_dir, ec);
        if (ec)
        {
            html += R"(
            <li class="item"><span style="color: #dc2626;">❌ 访问路径失败：)" +
                    FileOperations::getErrorMessage(ec) + R"(</span></li>
            )";
            ec.clear();
        }
        // 2. 路径不存在
        else if (path_type == FileOperations::FileType::NON_EXIST)
        {
            html += R"(
            <li class="item"><span style="color: #dc2626;">❌ 路径不存在：)" +
                    current_actual_dir + R"(</span></li>
            )";
        }
        // 3. 路径不是目录
        else if (path_type != FileOperations::FileType::DIRECTORY)
        {
            html += R"(
            <li class="item"><span style="color: #dc2626;">❌ 不是有效的目录：)" +
                    current_actual_dir + R"(</span></li>
            )";
        }
        // 4. 遍历目录（使用 FileOperations::listDir）
        else
        {
            std::vector<FileOperations::FileInfo> file_list;
            // 遍历目录，不包含隐藏文件（隐藏文件以.开头）
            bool list_success = FileOperations::listDir(current_actual_dir, file_list, false, ec);

            if (!list_success || ec)
            {
                html += R"(
                <li class="item"><span style="color: #dc2626;">❌ 遍历目录失败：)" +
                        FileOperations::getErrorMessage(ec) + R"(</span></li>
                )";
                ec.clear();
            }
            else
            {
                // 遍历获取到的文件/目录列表
                for (const auto &file_info : file_list)
                {
                    // 跳过 . 和 ..（listDir 已过滤隐藏文件，这里双重保险）
                    if (file_info.name == "." || file_info.name == "..")
                    {
                        continue;
                    }

                    // 拼接条目URL路径（基于clean_path，确保格式统一）
                    std::string entry_url_path = clean_path.empty() ? file_info.name : (clean_path + "/" + file_info.name);

                    if (file_info.type == FileOperations::FileType::DIRECTORY)
                    {
                        // 目录：URL强制带/，避免跳转后少/的问题
                        std::string entry_url = "/public/" + entry_url_path + "/";
                        html += R"(
                        <li class="item">
                            <span class="icon dir-icon">📁</span>
                            <a href=")" +
                                entry_url + R"(/">)" + file_info.name + R"(/</a>
                        </li>
                         )";
                    }
                    else if (file_info.type == FileOperations::FileType::FILE)
                    {
                        // 文件：URL不带/（正常逻辑）
                        std::string entry_url = "/public/" + entry_url_path;
                        html += R"(
                        <li class="item">
                            <span class="icon file-icon">📄</span>
                            <a href=")" +
                                entry_url + R"(">)" + file_info.name + R"(</a>
                        </li>
                        )";
                    }
                    else
                    {
                        // 符号链接/其他类型：按文件处理，标注类型
                        std::string entry_url = "/public/" + entry_url_path;
                        std::string type_label = (file_info.type == FileOperations::FileType::SYMLINK) ? "🔗 " : "📦 ";
                        html += R"(
                        <li class="item">
                            <span class="icon file-icon">)" +
                                type_label + R"(</span>
                            <a href=")" +
                                entry_url + R"(">)" + file_info.name + R"(</a>
                        </li>
                        )";
                    }
                }

                // 若目录为空，提示空目录
                if (file_list.empty())
                {
                    html += R"(
                    <li class="item"><span style="color: #718096;">📂 该目录为空</span></li>
                    )";
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        // 捕获未知异常，避免程序崩溃
        html += R"(
        <li class="item"><span style="color: #dc2626;">❌ 未知错误：)" +
                std::string(e.what()) + R"(</span></li>
        )";
    }

    html += R"(
        </ul>
    </body>
    </html>
    )";
    return html;
}

void HttpServer::loadHTML(std::string &target)
{
    if (!target.empty())
    {
        this->htmlUpload.clear();
        this->htmlUpload = target;
    }
    else
    {
        this->htmlUpload = R"(
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
    }
}