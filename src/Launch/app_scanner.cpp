#include "Launch/app_scanner.h"

#include <dirent.h>
#include <fstream>
#include <sstream>

#include "../utils/log/log.h"

namespace Launch
{

std::vector<DesktopEntry> scanApplications(const std::string &dir, size_t maxApps)
{
    std::vector<DesktopEntry> entries;

    DIR *dp = opendir(dir.c_str());
    if (dp == nullptr)
    {
        log_warn("[Launch] scan: cannot open %s", dir.c_str());
        return entries;
    }

    struct dirent *dent;
    while ((dent = readdir(dp)) != nullptr)
    {
        if (entries.size() >= maxApps)
            break;

        const std::string name = dent->d_name;
        if (dent->d_type != DT_REG && dent->d_type != DT_UNKNOWN)
            continue;
        if (!desktopEntryFilenameValid(name))
            continue;

        const std::string path = dir + "/" + name;
        std::ifstream file(path);
        if (!file.is_open())
        {
            log_warn("[Launch] scan: cannot read %s", path.c_str());
            continue;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        file.close();

        Launch::DesktopEntry entry = Launch::parseDesktopEntry(ss.str());
        if (!entry.valid)
        {
            log_warn("[Launch] scan: skip %s (invalid/missing Name or Exec)", path.c_str());
            continue;
        }

        std::string reason;
        if (!desktopExecIsSafe(entry.exec, reason))
        {
            log_warn("[Launch] scan: skip %s (unsafe Exec: %s)", path.c_str(), reason.c_str());
            continue;
        }

        log_info("[Launch] found app: %s -> %s", entry.name.c_str(), entry.exec.c_str());
        entries.push_back(entry);
    }

    closedir(dp);
    return entries;
}

} // namespace Launch
