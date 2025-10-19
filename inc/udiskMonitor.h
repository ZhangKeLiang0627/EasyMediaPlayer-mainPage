#ifndef UDISK_MONITOR_H
#define UDISK_MONITOR_H

#include <string>
#include <mutex>
#include <functional>
#include <libudev.h>

/**
 * @brief U盘事件回调函数类型
 * @param eventType 事件类型（"add"：插入，"remove"：拔出）
 * @param devName 设备名（如"sdb1"）
 * @param mountPoint 挂载点（仅add事件有效，空表示未挂载）
 */
using UDiskEventCallback = std::function<void(const std::string &, const std::string &, const std::string &)>;

/**
 * @brief U盘热插拔监控类
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
     * @brief 处理epoll触发的udev事件（静态回调）
     * @param fd 触发事件的文件描述符
     */
    static void handleUdevEvent(int fd);

    /**
     * @brief 扫描系统中已存在的U盘设备（进程启动时调用）
     */
    void scanExistingDevices();

    /**
     * @brief 判断设备是否为USB设备
     * @param dev udev设备对象
     * @return true：是USB设备，false：不是
     */
    bool isUsbDevice(udev_device *dev);

    /**
     * @brief 获取设备的挂载点（从/proc/mounts读取）
     * @param devNode 设备节点（如"/dev/sdb1"）
     * @return 挂载点路径（空字符串表示未挂载）
     */
    std::string getMountPoint(const std::string &devNode);

    udev *udevContext_ = nullptr;         // udev上下文
    udev_monitor *udevMonitor_ = nullptr; // udev监控器
    int udevFd_ = -1;                     // 监控器文件描述符
    UDiskEventCallback eventCallback_;    // 事件回调函数

    static std::mutex instanceMutex_; // 线程安全锁
};

#endif // UDISK_MONITOR_H