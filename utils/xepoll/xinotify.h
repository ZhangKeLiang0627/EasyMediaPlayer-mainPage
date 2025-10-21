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
    // 监控文件（原函数，保留）
    bool AddFileWatch(const std::string &path, std::function<void()> handler);
    bool DelFileWatch(const std::string &path);
    // 新增：监控目录
    bool AddDirWatch(const std::string &path, std::function<void()> handler);
    bool DelDirWatch(const std::string &path);

private:
    int inotify_fd_;
    // 区分文件和目录的监控描述符映射（避免路径相同导致的冲突）
    std::unordered_map<std::string, int> file_watch_fd_map_;   // 文件监控：路径 -> watch_fd
    std::unordered_map<std::string, int> dir_watch_fd_map_;    // 目录监控：路径 -> watch_fd
    std::unordered_map<int, std::function<void()>> listeners_; // 统一的事件回调：watch_fd -> handler
};

#endif