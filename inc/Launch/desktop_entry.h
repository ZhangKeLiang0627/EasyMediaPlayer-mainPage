#pragma once

#include <map>
#include <string>

/**
 * @brief Launch 模块：应用描述（.desktop）解析与安全校验
 *
 * 参考 M5Stack launcher/APPLaunch 的 desktop_entry 实现，
 * 支持标准 [Desktop Entry] 段 + 自定义 [config] 扩展段。
 * 注意：项目为 C++11，不使用 std::optional，用 valid 字段表达解析成败。
 */
namespace Launch
{

    struct DesktopEntry
    {
        std::string name;                // Name：显示名称
        std::string exec;                // Exec：可执行路径 + 参数（空格分隔）
        std::string icon;                // Icon：图标路径
        bool terminal = false;           // Terminal：保留字段（触摸屏忽略）
        std::map<std::string, std::string> config; // [config] 段：应用自定义配置

        bool valid = false;              // 是否通过校验
    };

    /**
     * @brief 校验 .desktop 文件名（必须 .desktop 后缀，无路径分隔符/控制字符）
     */
    bool desktopEntryFilenameValid(const std::string &name);

    /**
     * @brief 解析 .desktop 文件内容
     * @return 解析后的条目；失败时 valid=false（自动过滤 Hidden/NoDisplay、
     *         非 Application 类型、控制字符等）
     */
    DesktopEntry parseDesktopEntry(const std::string &contents);

    /**
     * @brief Exec 字段安全校验（防止恶意 .desktop 注入 shell 命令）
     * @param reason 拒绝原因（失败时填充）
     * @return true-安全
     */
    bool desktopExecIsSafe(const std::string &exec, std::string &reason);

} // namespace Launch
