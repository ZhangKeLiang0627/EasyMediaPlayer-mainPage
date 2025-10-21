#ifndef UDISK_MONITOR_H
#define UDISK_MONITOR_H

#include <string>
#include <mutex>
#include <vector>
#include <functional>
#include <libudev.h>
#include "../utils/xepoll/xinotify.h"

/**
 * @brief U盘事件回调函数类型
 * @param eventType 事件类型（"add"：插入，"remove"：拔出）
 * @param devName 设备名（如"sdb1"）
 * @param mountPoint 挂载点（仅add事件有效，空表示未挂载）
 */
using UDiskEventCallback = std::function<void(const std::string &, const std::string &, const std::string &)>;

/**
 * @brief U盘热插拔监控类（inotify + epoll + udev）
 * 功能：实时监控U盘插入/拔出事件，进程启动时扫描已存在的U盘
 */
class UDiskMonitor
{
public:
    // 禁止拷贝构造和赋值（单例模式）
    UDiskMonitor(const UDiskMonitor &) = delete;
    UDiskMonitor &operator=(const UDiskMonitor &) = delete;

    /**
     * @brief 获取单例实例
     */
    static UDiskMonitor &getInstance()
    {
        static UDiskMonitor instance;
        return instance;
    }

    /**
     * @brief 启动U盘监控
     * @param callback 事件回调函数（接收事件类型、设备名、挂载点）
     * @return 0：成功，非0：失败（错误码见实现）
     */
    int start(UDiskEventCallback callback);

    /**
     * @brief 停止U盘监控，释放资源
     */
    void stop();

private:
    // 私有构造函数（单例模式）
    UDiskMonitor() = default;
    ~UDiskMonitor();

    /**
     * @brief 处理inotify事件（目录变化回调）
     */
    void handleBlockDirChangedEvent();

    /**
     * @brief 扫描系统中已存在的U盘设备（进程启动时调用）
     */
    void scanExistingDevices();

    /**
     * @brief 判断设备是否为USB设备
     * @return true / false
     */
    bool isUsbDevice(udev_device *dev);
    bool isUsbDevice(const std::string &devName);

    /**
     * @brief 获取设备的挂载点（从/proc/mounts读取）
     * @param devNode 设备节点（如"/dev/sdb1"）
     * @return 挂载点路径（空字符串表示未挂载）
     */
    std::string getMountPoint(const std::string &devNode);

    Xinotify inotify_;                     // inotify监控器
    udev *udevContext_ = nullptr;          // udev上下文
    UDiskEventCallback eventCallback_;     // 事件回调函数
    std::vector<std::string> watchedDevs_; // 监控设备列表
    std::mutex mutex_;                     // 线程安全锁
};

#endif // UDISK_MONITOR_H