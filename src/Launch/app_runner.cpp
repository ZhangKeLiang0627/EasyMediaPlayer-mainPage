#include "Launch/app_runner.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../libs/lvgl/lvgl.h"
#include "../utils/log/log.h"

#if defined(__linux__)
#include <sys/file.h>
#endif

namespace Launch
{

namespace
{

/* flock 实例锁：返回 0 未被占用，1 已被占用，-1 出错 */
int checkInstanceLock(const std::string &lockPath)
{
#if defined(__linux__)
    int fd = open(lockPath.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0)
    {
        log_warn("[Launch] open lock %s failed: %s", lockPath.c_str(), strerror(errno));
        return -1;
    }

    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_GETLK, &fl) == -1)
    {
        log_warn("[Launch] F_GETLK %s failed: %s", lockPath.c_str(), strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return (fl.l_type == F_UNLCK) ? 0 : 1;
#else
    (void)lockPath;
    return 0;
#endif
}

} // namespace

int runApplication(const std::string &execPath,
                   const std::vector<std::string> &args,
                   const std::string &lockName)
{
    if (execPath.empty())
        return -1;

    /* 1. flock 实例锁：同应用已在运行则拒绝 */
    if (!lockName.empty())
    {
        int lockState = checkInstanceLock(lockName);
        if (lockState == 1)
        {
            log_warn("[Launch] %s already running (lock %s)", execPath.c_str(), lockName.c_str());
            return -2;
        }
    }

    /* 2. 构造 argv（vector<char*> 生命周期随函数结束自动释放） */
    std::vector<char *> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char *>(execPath.c_str()));
    for (const auto &arg : args)
        argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);

    /* 3. LVGL 协调：暂停 timer，防止 UI 绘制覆盖子进程画面 */
    lv_timer_enable(false);

    /* 4. fork + 独立进程组 + subreaper */
#if defined(__linux__)
    prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0); // 收割孙进程，防孤儿
#endif

    pid_t pid = fork();
    if (pid == 0)
    {
        /* 子进程：独立进程组；exec 失败用 _exit（异步信号安全，不跑 atexit/析构） */
        setpgid(0, 0);
        execv(execPath.c_str(), argv.data());
        log_error("[Launch] exec %s failed: %s", execPath.c_str(), strerror(errno));
        _exit(127);
    }

    if (pid < 0)
    {
        log_error("[Launch] fork failed: %s", strerror(errno));
        lv_timer_enable(true);
        return -1;
    }

    setpgid(pid, pid);

    /* 5. 等待 leader 退出（EINTR 重试），记录退出码 */
    int status = 0;
    while (waitpid(pid, &status, 0) == -1)
    {
        if (errno != EINTR)
        {
            log_error("[Launch] waitpid %d failed: %s", pid, strerror(errno));
            break;
        }
    }

    int result = -1;
    if (WIFEXITED(status))
    {
        result = WEXITSTATUS(status);
        log_info("[Launch] %s exited with code %d", execPath.c_str(), result);
    }
    else if (WIFSIGNALED(status))
    {
        result = -1;
        log_warn("[Launch] %s killed by signal %d", execPath.c_str(), WTERMSIG(status));
    }

    lv_timer_enable(true);
    return result;
}

} // namespace Launch
