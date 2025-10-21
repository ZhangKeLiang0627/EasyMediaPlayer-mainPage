#include "udiskMonitor.h"
#include "../../utils/xepoll/xepoll.h"
#include "../../utils/xepoll/xinotify.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <dirent.h>
#include <algorithm>

UDiskMonitor::~UDiskMonitor()
{
    stop();
}

int UDiskMonitor::start(UDiskEventCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已启动
    if (udevContext_ != nullptr)
    {
        std::cerr << "UDiskMonitor already started" << std::endl;
        return -1;
    }

    // 检查回调有效性
    if (!callback)
    {
        std::cerr << "UDiskEventCallback cannot be null" << std::endl;
        return -2;
    }
    eventCallback_ = std::move(callback);

    // 创建udev上下文
    udevContext_ = udev_new();
    if (!udevContext_)
    {
        std::cerr << "Failed to create udev context" << std::endl;
        return -3;
    }

    // 监控/sys/class/block目录（块设备符号链接目录）
    const std::string blockDir = "/sys/class/block";
    bool ret = inotify_.AddFileWatch(blockDir,
                                     std::bind(&UDiskMonitor::handleBlockDirChangedEvent, this));
    if (!ret)
    {
        std::cerr << "Failed to watch " << blockDir << std::endl;
        udev_unref(udevContext_);
        udevContext_ = nullptr;
        return -3;
    }

    // 扫描已存在的U盘设备（初始化状态）
    scanExistingDevices();

    std::cout << "UDiskMonitor started successfully" << std::endl;
    return 0;
}

void UDiskMonitor::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 释放udev资源
    if (udevContext_)
    {
        udev_unref(udevContext_);
        udevContext_ = nullptr;
    }

    // 清空回调和监控列表
    eventCallback_ = nullptr;
    watchedDevs_.clear();

    // 结束inotify监听事件
    const std::string blockDir = "/sys/class/block";
    inotify_.DelFileWatch(blockDir);

    std::cout << "UDiskMonitor stopped" << std::endl;
}

// 核心：处理/sys/class/block目录变化（设备增减）
void UDiskMonitor::handleBlockDirChangedEvent()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Block directory changed, checking devices..." << std::endl;

    // 1. 读取当前目录下的所有设备（如sda、sda1）
    const std::string blockDir = "/sys/class/block";
    DIR *dir = opendir(blockDir.c_str());
    if (!dir)
    {
        std::cerr << "Failed to open " << blockDir << std::endl;
        return;
    }

    std::vector<std::string> currentDevs;
    dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // 过滤.和..，只保留sd开头的设备（U盘通常以sd开头）
        if (entry->d_name[0] != '.' && strncmp(entry->d_name, "sd", 2) == 0)
        {
            currentDevs.push_back(entry->d_name);
        }
    }
    closedir(dir);

    // 2. 对比已监控列表，找出新增和删除的设备
    // 新增设备（存在于currentDevs但不在watchedDevs_）
    for (const auto &dev : currentDevs)
    {
        if (std::find(watchedDevs_.begin(), watchedDevs_.end(), dev) == watchedDevs_.end())
        {
            // 检查是否为USB设备
            if (isUsbDevice(dev))
            {
                std::string devNode = "/dev/" + dev;
                std::string mountPoint = getMountPoint(devNode);
                if (eventCallback_)
                {
                    eventCallback_("add", dev, mountPoint);
                }
                watchedDevs_.push_back(dev); // 加入已监控列表
            }
        }
    }

    // 删除设备（存在于watchedDevs_但不在currentDevs）
    std::vector<std::string> newWatched;
    for (const auto &dev : watchedDevs_)
    {
        if (std::find(currentDevs.begin(), currentDevs.end(), dev) != currentDevs.end())
        {
            newWatched.push_back(dev); // 保留仍存在的设备
        }
        else
        {
            // 触发拔出事件
            if (eventCallback_)
            {
                eventCallback_("remove", dev, "");
            }
        }
    }
    watchedDevs_.swap(newWatched); // 更新已监控列表
}

void UDiskMonitor::scanExistingDevices()
{
    if (!udevContext_)
    {
        return;
    }

    // 创建udev枚举器（用于遍历设备）
    udev_enumerate *enumerator = udev_enumerate_new(udevContext_);
    if (!enumerator)
    {
        std::cerr << "Failed to create udev enumerator" << std::endl;
        return;
    }

    // 筛选block子系统的设备
    udev_enumerate_add_match_subsystem(enumerator, "block");
    udev_enumerate_scan_devices(enumerator);

    // 遍历所有block设备
    udev_list_entry *devices = udev_enumerate_get_list_entry(enumerator);
    udev_list_entry *entry = nullptr;
    udev_list_entry_foreach(entry, devices)
    {
        const char *sysPath = udev_list_entry_get_name(entry);
        udev_device *dev = udev_device_new_from_syspath(udevContext_, sysPath);
        if (!dev)
        {
            continue;
        }

        // 仅处理USB设备
        if (isUsbDevice(dev))
        {
            const char *devNode = udev_device_get_devnode(dev);
            const char *sysName = udev_device_get_sysname(dev);
            if (devNode && sysName)
            {
                // 获取挂载点并触发add事件
                std::string mountPoint = getMountPoint(devNode);
                if (eventCallback_)
                {
                    eventCallback_("add", sysName, mountPoint);
                }
            }
        }

        // 释放设备资源
        udev_device_unref(dev);
    }

    // 释放枚举器
    udev_enumerate_unref(enumerator);
    std::cout << "Scanned existing USB devices" << std::endl;
}

bool UDiskMonitor::isUsbDevice(udev_device *dev)
{
    const char *busType = udev_device_get_property_value(dev, "ID_BUS");
    return (busType && std::strcmp(busType, "usb") == 0);
}

bool UDiskMonitor::isUsbDevice(const std::string &devName)
{
    if (!udevContext_)
        return false;

    // 获取设备的sysfs路径
    std::string sysPath = "/sys/class/block/" + devName;
    udev_device *dev = udev_device_new_from_syspath(udevContext_, sysPath.c_str());
    if (!dev)
        return false;

    // 检查sysfs路径是否包含"usb"（适配嵌入式系统）
    const char *syspath = udev_device_get_syspath(dev);
    bool isUsb = (syspath && strstr(syspath, "usb") != nullptr);

    udev_device_unref(dev);
    return isUsb;
}

std::string UDiskMonitor::getMountPoint(const std::string &devNode)
{
    std::ifstream mountsFile("/proc/mounts");
    if (!mountsFile.is_open())
    {
        std::cerr << "Failed to open /proc/mounts" << std::endl;
        return "";
    }

    std::string line;
    while (std::getline(mountsFile, line))
    {
        std::istringstream lineStream(line);
        std::string dev, mountPoint;
        lineStream >> dev >> mountPoint;

        // 匹配设备节点与挂载点
        if (dev == devNode)
        {
            return mountPoint;
        }
    }

    return ""; // 未挂载
}