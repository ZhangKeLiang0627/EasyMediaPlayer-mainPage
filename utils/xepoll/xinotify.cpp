#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <sys/inotify.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>

#include "xepoll.h"
#include "xinotify.h"

Xinotify::Xinotify()
{
#if defined(__linux__)
    inotify_fd_ = inotify_init();
    if (inotify_fd_ < 0)
    {
        fprintf(stderr, "inotify_init failed\n");
    }
    assert(inotify_fd_ > 0);
    MY_EPOLL.EpollAddRead(inotify_fd_, std::bind(&Xinotify::HandleEvent, this));
#endif
    std::cout << "Inotify init" << std::endl;
}

Xinotify::~Xinotify()
{
#if defined(__linux__)
    if (inotify_fd_ > 0)
    {
        MY_EPOLL.EpollDel(inotify_fd_);
        // 移除所有文件监控
        for (auto &wds : file_watch_fd_map_)
        {
            inotify_rm_watch(inotify_fd_, wds.second);
        }
        // 移除所有目录监控
        for (auto &wds : dir_watch_fd_map_)
        {
            inotify_rm_watch(inotify_fd_, wds.second);
        }
        close(inotify_fd_);
    }
#endif
}

// 监控文件（原函数，保留）
bool Xinotify::AddFileWatch(const std::string &path, std::function<void()> handler)
{
#if defined(__linux__)
    // 检查文件是否存在
    if (-1 == access(path.c_str(), F_OK))
    {
        fprintf(stderr, "file %s not exist, create it\n", path.c_str());
        FILE *pFile = fopen(path.c_str(), "w");
        if (pFile != nullptr)
        {
            fclose(pFile);
        }
    }

    // 监控文件事件（如修改、关闭写入等）
    int watch_fd = inotify_add_watch(inotify_fd_, path.c_str(), IN_MODIFY | IN_CLOSE_WRITE);
    if (watch_fd < 0)
    {
        fprintf(stderr, "inotify_add_watch (file) %s failed: %s\n", path.c_str(), strerror(errno));
        return false;
    }
    file_watch_fd_map_[path] = watch_fd;
    listeners_[watch_fd] = handler; // 注册回调
#endif
    std::cout << "Inotify add file watch: " << path << std::endl;
    return true;
}

// 监控目录（新增函数）
bool Xinotify::AddDirWatch(const std::string &path, std::function<void()> handler)
{
#if defined(__linux__)
    // 检查目录是否存在
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "dir %s not exist or not a directory, create it\n", path.c_str());
        // 尝试创建目录（递归创建多级目录）
        std::string cmd = "mkdir -p " + path;
        if (system(cmd.c_str()) != 0)
        {
            fprintf(stderr, "create dir %s failed\n", path.c_str());
            return false;
        }
    }

    // 监控目录事件（创建、删除、移动文件/子目录等）
    int watch_fd = inotify_add_watch(
        inotify_fd_,
        path.c_str(),
        IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE_SELF // 目录相关事件
    );
    if (watch_fd < 0)
    {
        fprintf(stderr, "inotify_add_watch (dir) %s failed: %s\n", path.c_str(), strerror(errno));
        return false;
    }
    dir_watch_fd_map_[path] = watch_fd;
    listeners_[watch_fd] = handler; // 注册回调
#endif
    std::cout << "Inotify add dir watch: " << path << std::endl;
    return true;
}

// 移除文件监控（原函数，保留）
bool Xinotify::DelFileWatch(const std::string &path)
{
#if defined(__linux__)
    auto it = file_watch_fd_map_.find(path);
    if (it != file_watch_fd_map_.end())
    {
        inotify_rm_watch(inotify_fd_, it->second);
        listeners_.erase(it->second);
        file_watch_fd_map_.erase(it);
        std::cout << "Inotify remove file watch: " << path << std::endl;
        return true;
    }
#endif
    return false;
}

// 移除目录监控（新增函数）
bool Xinotify::DelDirWatch(const std::string &path)
{
#if defined(__linux__)
    auto it = dir_watch_fd_map_.find(path);
    if (it != dir_watch_fd_map_.end())
    {
        inotify_rm_watch(inotify_fd_, it->second);
        listeners_.erase(it->second);
        dir_watch_fd_map_.erase(it);
        std::cout << "Inotify remove dir watch: " << path << std::endl;
        return true;
    }
#endif
    return false;
}

// 处理事件（修改为支持文件和目录事件）
int Xinotify::HandleEvent()
{
#if defined(__linux__)
    char buf[512];
    struct inotify_event *event;
    int event_size = sizeof(struct inotify_event);

    int read_len = read(inotify_fd_, buf, sizeof(buf));
    if (read_len < event_size)
    {
        printf("could not get event!\n");
        return -1;
    }

    int pos = 0;
    while (read_len >= event_size)
    {
        event = (struct inotify_event *)(buf + pos);
        // 检查事件是否有效（包含文件名且有监控回调）
        if (event->len > 0)
        {
            auto handle_it = listeners_.find(event->wd);
            if (handle_it != listeners_.end())
            {
                // 触发回调（无论文件还是目录事件，统一由注册的handler处理）
                handle_it->second();
                // 调试：打印事件详情
                if (event->mask & IN_CREATE)
                    std::cout << "Dir event: create " << event->name << std::endl;
                else if (event->mask & IN_DELETE)
                    std::cout << "Dir event: delete " << event->name << std::endl;
                else if (event->mask & IN_MOVED_TO)
                    std::cout << "Dir event: move to " << event->name << std::endl;
                else if (event->mask & IN_MOVED_FROM)
                    std::cout << "Dir event: move from " << event->name << std::endl;
                else if (event->mask & IN_MODIFY)
                    std::cout << "File event: modify " << event->name << std::endl;
            }
        }
        // 移动到下一个事件
        int temp_size = event_size + event->len;
        read_len -= temp_size;
        pos += temp_size;
    }
#endif
    return 0;
}