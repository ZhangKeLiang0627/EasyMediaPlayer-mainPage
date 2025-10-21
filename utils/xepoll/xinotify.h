#ifndef __XINOTIFY_H__
#define __XINOTIFY_H__

#include <unordered_map>
#include <vector>
#include <functional>

class Xinotify
{
public:
    Xinotify();
    ~Xinotify();

    int HandleEvent();
    bool AddFileWatch(const std::string &path, std::function<void()> handler);
    bool AddDirWatch(const std::string &path, std::function<void(const std::string &)> handler);
    bool DelFileWatch(const std::string &path);
    bool DelDirWatch(const std::string &path);

private:
    int inotify_fd_;
    std::unordered_map<std::string, int> watch_fd_map_;
    std::unordered_map<int, std::function<void()>> listeners_file_change_;
    std::unordered_map<int, std::pair<std::string, std::function<void(const std::string &)>>> listeners_dir_change_;
};

#endif
