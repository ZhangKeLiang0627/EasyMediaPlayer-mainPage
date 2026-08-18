#pragma once

namespace Net
{
    /**
     * @brief 通过公共 HTTP 接口对时，并更新系统时钟（settimeofday）
     * @return true-对时成功  false-失败（网络不可达 / 解析失败 / 无权限）
     * @note  使用 HTTP（非 HTTPS）是为了避免「时钟错误 → 证书 not yet valid」的
     *        鸡生蛋问题；主要用于无 RTC 的嵌入式板子（如 T113-S3 默认 1970）。
     */
    bool syncSystemTime(void);
}
