#pragma once

#include <string>
# include "../libs/cpp-httplib/httplib.h"

class HttpServer
{

public:
    HttpServer();
    ~HttpServer();

private:
    httplib::Server svr;
    std::string htmlUpload;

public:
    void init();
    void start(const std::string &host, int port);
    void stop();
    std::string generateDirListHtml(const std::string &current_url_path);
    void loadHTML(std::string &target);
    httplib::Server & getHttpServer() { return this->svr; }
};
