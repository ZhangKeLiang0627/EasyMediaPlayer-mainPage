#pragma once

#include <string>
#include <vector>

/**
 * @brief Launch 模块：外部应用启动器（fork + execv）
 *
 * 参考 M5Stack launcher/APPLaunch 的 cp0_external_app_runner，
 * 修复了原 mainPage runApplication 的多个缺陷：
 *   - wait(nullptr) -> waitpid(pid) + EINTR 重试（避免误收其他子进程）
 *   - exec 失败 _exit(127) 而非 exit(0)
 *   - 子进程独立进程组 + subreaper，防孤儿/僵尸
 *   - 正确记录退出码（正常退出 / 信号终止）
 *   - 可选 flock 实例锁，防止同一应用重复启动
 *   - LVGL 协调：启动/返回时暂停/恢复 lv_timer，避免 UI 覆盖子进程画面
 */
namespace Launch
{

    /**
     * @brief 阻塞式启动外部应用，直到其退出
     * @param execPath 可执行文件路径（建议绝对路径）
     * @param args     附加参数（不含 argv[0]）
     * @param lockName 非空则启用 flock 实例锁（如 /tmp/eMP_xxx.lock）
     * @return 正常退出返回退出码；已被实例锁拦截返回 -2；
     *         启动失败/异常终止返回 -1
     */
    int runApplication(const std::string &execPath,
                       const std::vector<std::string> &args,
                       const std::string &lockName = "");

} // namespace Launch
