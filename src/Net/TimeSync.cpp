// strptime 在 glibc 下需要 _GNU_SOURCE / _XOPEN_SOURCE（必须在所有头文件之前定义）
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "TimeSync.h"
#include "httplib.h"
#include <nlohmann/json.hpp>

#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "../utils/log/log.h"

using nlohmann::json;

namespace Net
{

/* 通过 settimeofday 更新系统时钟（需要 root 权限） */
static bool setTimeByUnix(long long unixTime)
{
    struct timeval tv;
    tv.tv_sec = (time_t)unixTime;
    tv.tv_usec = 0;

    if (settimeofday(&tv, nullptr) != 0)
    {
        log_warn("[TimeSync] settimeofday(%lld) failed: %s", unixTime, strerror(errno));
        return false;
    }

    log_info("[TimeSync] time synced -> unix=%lld", unixTime);
    return true;
}

/* Howard Hinnant 算法：儒略历日转"自 1970-01-01 起的天数"，用于实现 timegm */
static long long daysFromCivil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

/* 等价于 timegm（musl 无此函数），把 tm 当作 UTC 换算成 epoch */
static long long timegmUtc(const struct tm *tm)
{
    long long days = daysFromCivil(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    return days * 86400LL + tm->tm_hour * 3600LL + tm->tm_min * 60LL + tm->tm_sec;
}

/* 解析 RFC 1123 Date 头（如 "Tue, 18 Aug 2026 05:09:00 GMT"），失败返回 false */
static bool parseRfc1123Date(const std::string &s, long long &out)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    if (strptime(s.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm) == nullptr)
        return false;

    out = timegmUtc(&tm);
    return true;
}

bool syncSystemTime(void)
{
    /* 方案 1：WorldTimeAPI，JSON 里直接带 unixtime（最省事） */
    {
        httplib::Client cli("http://worldtimeapi.org");
        cli.set_connection_timeout(5, 0);
        cli.set_read_timeout(5, 0);
        cli.set_follow_location(true);

        auto res = cli.Get("/api/ip");
        if (res != nullptr && res->status == 200)
        {
            try
            {
                json j = json::parse(res->body);
                if (j.contains("unixtime") && j["unixtime"].is_number())
                {
                    return setTimeByUnix(j["unixtime"].get<long long>());
                }
            }
            catch (...)
            {
                log_warn("[TimeSync] worldtimeapi parse failed");
            }
        }
    }

    /* 方案 2（回退）：解析任意 HTTP 服务器的 Date 响应头（百度，国内可达性好） */
    {
        httplib::Client cli("http://www.baidu.com");
        cli.set_connection_timeout(5, 0);
        cli.set_read_timeout(5, 0);
        cli.set_follow_location(true);

        auto res = cli.Get("/");
        if (res != nullptr)
        {
            std::string date = res->get_header_value("Date");
            long long unixTime = 0;
            if (!date.empty() && parseRfc1123Date(date, unixTime))
            {
                return setTimeByUnix(unixTime);
            }
        }
    }

    log_warn("[TimeSync] all time sources failed");
    return false;
}

} // namespace Net
