#include "udiskMonitor.h"
#include "../../utils/xepoll/xepoll.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

// 静态成员初始化
std::mutex UDiskMonitor::instanceMutex_;

UDiskMonitor::~UDiskMonitor()
{
    stop();
}

int UDiskMonitor::start(UDiskEventCallback callback)
{
    std::lock_guard<std::mutex> lock(instanceMutex_);

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

    // 创建udev监控器（监听udev netlink）
    udevMonitor_ = udev_monitor_new_from_netlink(udevContext_, "udev");
    if (!udevMonitor_)
    {
        std::cerr << "Failed to create udev monitor" << std::endl;
        udev_unref(udevContext_);
        udevContext_ = nullptr;
        return -4;
    }

    // 设置监控过滤规则（仅关注block子系统）
    if (udev_monitor_filter_add_match_subsystem_devtype(udevMonitor_, "block", nullptr) < 0)
    {
        std::cerr << "Failed to set udev filter" << std::endl;
        udev_monitor_unref(udevMonitor_);
        udev_unref(udevContext_);
        udevMonitor_ = nullptr;
        udevContext_ = nullptr;
        return -5;
    }

    // 启动监控器
    if (udev_monitor_enable_receiving(udevMonitor_) < 0)
    {
        std::cerr << "Failed to enable udev monitor" << std::endl;
        udev_monitor_unref(udevMonitor_);
        udev_unref(udevContext_);
        udevMonitor_ = nullptr;
        udevContext_ = nullptr;
        return -6;
    }

    // 获取监控器文件描述符
    udevFd_ = udev_monitor_get_fd(udevMonitor_);
    if (udevFd_ < 0)
    {
        std::cerr << "Failed to get udev monitor fd" << std::endl;
        udev_monitor_unref(udevMonitor_);
        udev_unref(udevContext_);
        udevMonitor_ = nullptr;
        udevContext_ = nullptr;
        return -7;
    }

    // 扫描已存在的U盘设备（初始化状态）
    scanExistingDevices();

    // 将udev fd注册到epoll，监听读事件
    if (MY_EPOLL.EpollAddRead(udevFd_, &UDiskMonitor::handleUdevEvent) < 0)
    {
        std::cerr << "Failed to register udev fd to epoll" << std::endl;
        stop();
        return -8;
    }

    std::cout << "UDiskMonitor started successfully" << std::endl;
    return 0;
}

void UDiskMonitor::stop()
{
    std::lock_guard<std::mutex> lock(instanceMutex_);

    // 从epoll中移除监控
    if (udevFd_ != -1)
    {
        MY_EPOLL.EpollDel(udevFd_);
        udevFd_ = -1;
    }

    // 释放udev资源
    if (udevMonitor_)
    {
        udev_monitor_unref(udevMonitor_);
        udevMonitor_ = nullptr;
    }
    if (udevContext_)
    {
        udev_unref(udevContext_);
        udevContext_ = nullptr;
    }

    // 清空实例和回调
    eventCallback_ = nullptr;

    std::cout << "UDiskMonitor stopped" << std::endl;
}

void UDiskMonitor::handleUdevEvent(int fd)
{
    std::lock_guard<std::mutex> lock(instanceMutex_);

    std::cout << "UDiskMonitor handleUdevEvent begin" << std::endl;

    UDiskMonitor *instance_ = &UDiskMonitor::getInstance();

    // 检查实例有效性
    if (!instance_ || instance_->udevFd_ != fd || !instance_->udevMonitor_)
    {
        if (!instance_)
        {
            std::cerr << "UDiskMonitor instance is null, cannot handle udev event" << std::endl;
        }
        else if (instance_->udevFd_ != fd)
        {
            std::cerr << "Udev fd mismatch (expected: " << instance_->udevFd_
                      << ", actual: " << fd << "), skip event" << std::endl;
        }
        else if (!instance_->udevMonitor_)
        {
            std::cerr << "Udev monitor is null, cannot receive device event" << std::endl;
        }
        return;
    }

    // 读取udev事件
    udev_device *dev = udev_monitor_receive_device(instance_->udevMonitor_);
    if (!dev)
    {
        std::cerr << "Failed to receive udev device event (maybe no new event)" << std::endl;
        return;
    }

    // 提取事件信息
    const char *action = udev_device_get_action(dev);
    const char *devNode = udev_device_get_devnode(dev); // 设备节点（如/dev/sdb1）
    const char *sysName = udev_device_get_sysname(dev); // 设备名（如sdb1）

    // 仅处理USB设备的有效事件
    if (action && devNode && sysName && instance_->isUsbDevice(dev))
    {
        std::string eventType(action);
        std::string devName(sysName);
        std::string mountPoint;

        // 仅add事件需要获取挂载点
        if (eventType == "add")
        {
            mountPoint = instance_->getMountPoint(devNode);
        }

        // 触发回调函数
        if (instance_->eventCallback_)
        {
            std::cout << "UDiskMonitor eventCallback_ begin" << std::endl;
            instance_->eventCallback_(eventType, devName, mountPoint);
        }
    }

    // 释放设备资源
    udev_device_unref(dev);
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