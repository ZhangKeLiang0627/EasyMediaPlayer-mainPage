#pragma once

#include <string>
#include <vector>

#include "Launch/desktop_entry.h"

/**
 * @brief Launch 模块：扫描 applications 目录，发现 .desktop 应用
 */
namespace Launch
{

    /**
     * @brief 扫描指定目录下的 *.desktop 文件并解析
     * @param dir 应用描述目录（如 /mnt/UDISK/applications）
     * @param maxApps 最大应用数（防御性上限）
     * @return 解析成功且通过 Exec 安全校验的应用列表
     */
    std::vector<DesktopEntry> scanApplications(const std::string &dir, size_t maxApps = 64);

} // namespace Launch
