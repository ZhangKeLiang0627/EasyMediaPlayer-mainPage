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
    bool DelFileWatch(const std::string &path);

private:
    int inotify_fd_;
    std::unordered_map<std::string, int> file_watch_fd_map_;
    std::unordered_map<int, std::function<void()>> listeners_file_change_;
};

#endif
