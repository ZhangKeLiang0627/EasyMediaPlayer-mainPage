#include "Launch/desktop_entry.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Launch
{

namespace
{

void trim(std::string &value)
{
    const size_t first = value.find_first_not_of(" \t");
    if (first == std::string::npos)
    {
        value.clear();
        return;
    }
    const size_t last = value.find_last_not_of(" \t");
    value = value.substr(first, last - first + 1);
}

bool parseBool(const std::string &value)
{
    return value == "true" || value == "True" || value == "1";
}

bool containsControl(const std::string &value)
{
    for (unsigned char c : value)
    {
        if (c < 0x20 || c == 0x7F)
            return true;
    }
    return false;
}

bool containsShellMeta(const std::string &text)
{
    static const char *kShellMetacharacters = "|&;<>`$\\\n\r";
    return text.find_first_of(kShellMetacharacters) != std::string::npos;
}

std::string firstToken(const std::string &exec)
{
    std::istringstream stream(exec);
    std::string token;
    stream >> token;
    return token;
}

bool fileExecutable(const std::string &path)
{
#if defined(_WIN32)
    (void)path;
    return false;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
           access(path.c_str(), X_OK) == 0;
#endif
}

} // namespace

bool desktopEntryFilenameValid(const std::string &name)
{
    static const std::string suffix = ".desktop";
    if (name.size() <= suffix.size() ||
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
        return false;
    for (unsigned char c : name)
    {
        if (c < 0x20 || c == 0x7F || c == '/' || c == '\\')
            return false;
    }
    return true;
}

DesktopEntry parseDesktopEntry(const std::string &contents)
{
    DesktopEntry entry;
    bool inDesktopEntry = false;
    bool inConfigSection = false;
    bool hidden = false;
    bool validType = true;

    std::istringstream stream(contents);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        if (line[0] == '[')
        {
            inDesktopEntry = (line == "[Desktop Entry]");
            inConfigSection = (line == "[config]");
            continue;
        }

        const size_t separator = line.find('=');
        if (separator == std::string::npos)
            continue;
        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        trim(key);
        trim(value);

        if (inDesktopEntry)
        {
            if (key == "Name")
                entry.name = value;
            else if (key == "Icon")
                entry.icon = value;
            else if (key == "Exec")
                entry.exec = value;
            else if (key == "Terminal")
                entry.terminal = parseBool(value);
            else if (key == "Hidden" || key == "NoDisplay")
                hidden = hidden || parseBool(value);
            else if (key == "Type")
                validType = (value == "Application");
        }
        else if (inConfigSection)
        {
            // 自定义扩展段：[config] 下的任意 key=value 都收进 config map
            entry.config[key] = value;
        }
    }

    if (hidden || !validType || entry.name.empty() || entry.exec.empty() ||
        entry.name.size() > 256 || entry.exec.size() > 512 ||
        containsControl(entry.name) || containsControl(entry.icon))
        return entry; // valid 保持 false

    entry.valid = true;
    return entry;
}

bool desktopExecIsSafe(const std::string &exec, std::string &reason)
{
    if (exec.empty())
    {
        reason = "empty Exec";
        return false;
    }
    if (exec.size() > 512)
    {
        reason = "Exec too long";
        return false;
    }
    if (containsShellMeta(exec))
    {
        reason = "Exec contains shell metacharacters";
        return false;
    }

    const std::string token = firstToken(exec);
    if (token.empty())
    {
        reason = "missing executable";
        return false;
    }

    // 带路径的：必须是真实可执行文件
    if (token.find('/') != std::string::npos)
    {
        if (!fileExecutable(token))
        {
            reason = "executable path is not executable";
            return false;
        }
        return true;
    }

    // 纯命令名：白名单
    static const char *kAllowedNames[] = {"bash", "python3", "vim", "vi", "nano", "sh"};
    const bool allowed = std::find(std::begin(kAllowedNames), std::end(kAllowedNames), token) !=
                         std::end(kAllowedNames);
    if (!allowed)
    {
        reason = "executable name is not allowlisted";
        return false;
    }
    return true;
}

} // namespace Launch
