#include <assert.h>
#include <errno.h>
#include <fcntl.h>
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
    /* 故意将 inotify fd 设为阻塞：本 epoll 轮询线程只处理 inotify 事件，
       没有其他需要即时响应的回调；且 epoll 为 LT 模式、HandleEvent 单次
       read，epoll_wait 已保证可读时 read 不会卡死。无事件时线程自然在
       epoll_wait 中休眠，比非阻塞忙等更省 CPU。必须在 EpollAddRead 之后
       清除 O_NONBLOCK，因为 EpollAddRead 内部会把 fd 强制设为非阻塞。 */
    int flags = fcntl(inotify_fd_, F_GETFL, 0);
    if (flags >= 0)
        fcntl(inotify_fd_, F_SETFL, flags & ~O_NONBLOCK);
#endif
    std::cout << "Inotify init" << std::endl;
}

Xinotify::~Xinotify()
{
#if defined(__linux__)
    if (inotify_fd_ > 0)
    {
        MY_EPOLL.EpollDel(inotify_fd_);
        for (auto wds : watch_fd_map_)
        {
            inotify_rm_watch(inotify_fd_, wds.second);
        }
        close(inotify_fd_);
    }
#endif
}

bool Xinotify::AddFileWatch(const std::string &path, std::function<void()> handler)
{
#if defined(__linux__)
    // 保证文件存在
    if (-1 == access(path.c_str(), F_OK))
    {
        fprintf(stderr, "file %s not exist, create it\n", path.c_str());
        FILE *pFile = fopen(path.c_str(), "w");
        if (pFile != nullptr)
        {
            fclose(pFile);
        }
        // return false;
    }

    // IN_MODIFY 监控文件被修改
    int watch_fd = inotify_add_watch(inotify_fd_, path.c_str(), IN_ALL_EVENTS);
    if (watch_fd < 0)
    {
        fprintf(stderr, "inotify_add_watch %s failed\n", path.c_str());
        return false;
    }
    else
    {
        watch_fd_map_[path] = watch_fd;
        listeners_file_change_[watch_fd] = handler;
    }
#endif
    std::cout << "Inotify add watch " << path << std::endl;
    return true;
}

bool Xinotify::DelFileWatch(const std::string &path)
{
#if defined(__linux__)
    if (watch_fd_map_.count(path))
    {
        inotify_rm_watch(inotify_fd_, watch_fd_map_[path]);
        listeners_file_change_.erase(watch_fd_map_[path]);
        watch_fd_map_.erase(path);
        std::cout << "Remove file from watch " << path << std::endl;
        return true;
    }
#endif
    return false;
}

bool Xinotify::AddDirWatch(const std::string &path, std::function<void(const std::string &)> handler)
{
#if defined(__linux__)
    // 检查目录是否存在
    struct stat st;
    if (stat(path.c_str(), &st) == -1 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "dir %s not exist or not a directory\n", path.c_str());
        return false;
    }

    // 监控目录下的创建、删除、移动等事件
    int watch_fd = inotify_add_watch(inotify_fd_, path.c_str(), IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (watch_fd < 0)
    {
        fprintf(stderr, "inotify_add_watch dir %s failed\n", path.c_str());
        return false;
    }
    else
    {
        watch_fd_map_[path] = watch_fd;                    // 复用现有映射存储watch_fd
        listeners_dir_change_[watch_fd] = {path, handler}; // 存储目录路径和回调
    }
#endif
    std::cout << "Inotify add dir watch " << path << std::endl;
    return true;
}

bool Xinotify::DelDirWatch(const std::string &path)
{
#if defined(__linux__)
    if (watch_fd_map_.count(path))
    {
        int watch_fd = watch_fd_map_[path];
        inotify_rm_watch(inotify_fd_, watch_fd);
        listeners_dir_change_.erase(watch_fd); // 移除目录监听器
        watch_fd_map_.erase(path);             // 复用现有映射删除
        std::cout << "Remove dir from watch " << path << std::endl;
        return true;
    }
#endif
    return false;
}

int Xinotify::HandleEvent()
{
#if defined(__linux__)
    char buf[512];
    struct inotify_event *event;
    int event_size = sizeof(struct inotify_event);

    // 读取事件（inotify fd 为阻塞模式，但本函数只在 epoll_wait 报告可读后
    // 才被调用，且采用 LT 模式单次 read，故不会阻塞；保留 EAGAIN 防御）
    int read_len = read(inotify_fd_, buf, sizeof(buf));
    if (read_len < 0)
    {
        if (errno != EAGAIN)
            printf("read inotify event failed\n");
        return -1;
    }

    // 如果read的返回值，小于inotify_event大小出现错误
    if (read_len < event_size)
    {
        printf("could not get valid event\n");
        return -1;
    }

    // 因为read的返回值存在一个或者多个inotify_event对象，需要一个一个取出来处理
    int pos = 0;
    while (read_len >= event_size)
    {
        event = (struct inotify_event *)(buf + pos);

        if (event->mask & IN_CLOSE_WRITE)
        {
            auto handle_it = listeners_file_change_.find(event->wd);
            if (handle_it != listeners_file_change_.end())
            {
                handle_it->second();
            }
        }

        // 处理目录事件
        if (event->mask & (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO))
        {
            auto dir_it = listeners_dir_change_.find(event->wd);
            if (dir_it != listeners_dir_change_.end())
            {
                // 拼接完整路径（目录+文件名）
                std::string full_path = dir_it->second.first;
                if (!full_path.empty() && full_path.back() != '/')
                {
                    full_path += "/";
                }
                full_path += event->name;
                dir_it->second.second(full_path); // 调用目录事件回调
            }
        }
        // 一个事件的真正大小：inotify_event 结构体所占用的空间 + inotify_event->name 所占用的空间
        // 移动到下一个事件
        int temp_size = event_size + event->len;
        read_len -= temp_size;
        pos += temp_size;
    }
#endif
    return 0;
}
