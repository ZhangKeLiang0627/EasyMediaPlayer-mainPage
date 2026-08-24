# Launch 系统设计（eMP_mainPage）

本文档说明 eMP_mainPage 应用管理（发现 / 启动）的架构设计，供维护与二次开发参考。
实现参考了 [launcher/APPLaunch](https://github.com/ZhangKeLiang0627/launcher/tree/master/projects/APPLaunch)
（M5Stack cp0）的 desktop-entry / directory-watcher / external-app-runner 机制，
并结合本项目的 LVGL 8.3 + sunxifb + 触摸屏环境做了适配与简化。

## 1. 总体结构

```
main.cpp
  └─ Model（Page 命名空间）
       ├─ threadDataProcHandler ── 启动扫描 + 60s 对时 + 200ms inotify 轮询
       ├─ threadLvglHandler    ── lv_task_handler（持 _mutex）
       ├─ threadHttpSvrHandler ── HTTP 管理服务（6210）
       └─ Xinotify _inotify    ── inotify 封装（多目录监听）
            ├─ /mnt/exUDISK            （U 盘挂载点 → 图标显隐）
            └─ /mnt/UDISK/applications （.desktop → 自动发现）
                    │ 事件回调
                    ▼
              Model::appsNotifyDirHandler → reloadApplications()
                    │
                    ▼
              Launch::scanApplications() ── desktop_entry 解析 + 安全校验
                    │
                    ▼
              View::addApplication()（增量：按 exec 去重）
```

点击应用按钮：

```
View::applicationEventHandler → Model::runApplication
      → Launch::runApplication（fork + execv）
           ├─ flock 实例锁检查
           ├─ lv_timer_enable(false)   ← 暂停 LVGL 渲染
           ├─ fork + setpgid + subreaper
           ├─ waitpid(pid) + EINTR 重试
           ├─ 退出码日志（WIFEXITED/WIFSIGNALED）
           └─ lv_timer_enable(true) + 回主界面动画
```

## 2. 应用描述格式（.desktop）

目录：`/mnt/UDISK/applications/*.desktop`，Linux desktop-entry 规范的轻量子集。

```ini
[Desktop Entry]
Name=Token Monitor
Exec=/mnt/UDISK/eMP_tokenMonitor [args...]
Icon=/path/to/icon.png
Terminal=false
Hidden=false
Type=Application
```

### 解析规则（desktop_entry.cpp）

| 规则 | 说明 |
|---|---|
| 段头 | `[Desktop Entry]` 标准段；`[config]` 自定义段（任意 key=value 收入 `config` map） |
| 注释 | `#` / `;` 开头的行忽略；`\r` 结尾兼容 |
| 必填 | `Name`、`Exec` 非空；`Type` 必须 `Application` |
| 过滤 | `Hidden=true` / `NoDisplay=true` 忽略 |
| 长度 | Name ≤ 256B，Exec ≤ 512B |
| 控制字符 | Name/Icon 含控制字符拒绝 |

### Exec 安全校验（desktopExecIsSafe）

```cpp
1. 空 / 超 512B → 拒绝
2. 含 shell 元字符 `|&;<>`$\\\n\r` → 拒绝（防注入）
3. 第一个 token 含 '/' → stat + access(X_OK) 必须真实可执行
4. 纯命令名 → 白名单 {bash, python3, vim, vi, nano, sh}
```

## 3. 自动发现（app_scanner + inotify）

- **扫描**：`scanApplications(dir)` 用 `opendir/readdir` 遍历，过滤 `.desktop` 后缀、
  读文件 → `parseDesktopEntry` → `desktopExecIsSafe`，上限 64 个（防失控）。
- **监听**：`Xinotify::AddDirWatch("/mnt/UDISK/applications", cb)`，
  事件（IN_CREATE/IN_DELETE/IN_MOVED_*）→ `appsNotifyDirHandler` → 重扫 → 增量刷新。
- **增量**：`Model::_installedExec`（std::set）按可执行路径去重，新的才 `addApplication`，
  避免整屏重建。
- **向后兼容**：`sysconfig.json` 的 `applications` 数组作为兜底合并（同路径跳过）。

### 设计：inotify fd 刻意设为阻塞

inotify fd 在 `Xinotify` 构造函数里经 `MY_EPOLL.EpollAddRead` 注册进 epoll 轮询线程，
而 `EpollAddRead`（`utils/xepoll/xepoll.cpp`）内部用
`fcntl(fd, F_GETFD, 0) | O_NONBLOCK` 把 fd 强制设为非阻塞——注意这里用 `F_GETFD`
虽写法不严谨，但 `O_NONBLOCK` 位仍会被 `F_SETFL` 写入，fd 实际已是非阻塞。

我们**刻意**在 `EpollAddRead` 之后 `fcntl(F_SETFL, flags & ~O_NONBLOCK)` 把 inotify fd
改回阻塞，理由：

- 消费 inotify 的线程（epoll 轮询线程，以及 `Model` 的轮询线程）只做 inotify 一件事，
  没有需要即时响应的其它回调；
- epoll 是 **LT（水平触发）** 模式且 `HandleEvent` 单次 `read`，`epoll_wait` 已保证可读时
  `read` 不会卡死；无事件时线程自然在 `epoll_wait` 中休眠；
- 阻塞比非阻塞忙等（`EAGAIN` 空转）更省 CPU，相当于把"等待"交给内核。

`HandleEvent` 仍保留对 `EAGAIN` 的防御性返回（无事件属正常）。

> 历史备注：原来"必须用 `F_GETFL` 否则 fd 实际阻塞"的说法不准确——原写法用 `F_GETFD`
> 只是没保留其它状态标志（对 inotify 无影响），`O_NONBLOCK` 位本就会被置位，并非 fd
> 阻塞的根因；要让它阻塞，真正需要做的是显式清除 `O_NONBLOCK`（在 `EpollAddRead` 之后）。

## 4. 启动器（app_runner）

`Launch::runApplication(execPath, args, lockName)` 阻塞式启动外部应用：

| 步骤 | 说明 |
|---|---|
| 1. 实例锁 | `lockName` 非空时 `flock` F_GETLK 检查，已被持有返回 -2（已在运行） |
| 2. LVGL 协调 | `lv_timer_enable(false)`，防止主界面 UI 覆盖子进程画面 |
| 3. fork | 子进程 `setpgid(0,0)`（独立进程组）+ `execv`，失败 `_exit(127)` |
| 4. subreaper | `prctl(PR_SET_CHILD_SUBREAPER)`，能收割应用再 fork 的孙进程 |
| 5. 等待 | `waitpid(pid)` + EINTR 重试；`WIFEXITED`→退出码，`WIFSIGNALED`→信号号 |
| 6. 恢复 | `lv_timer_enable(true)`，返回退出码 |

对比原实现的修复点：

| 原实现 | 问题 | 现实现 |
|---|---|---|
| `wait(nullptr)` | 可能误收其他子进程 | `waitpid(pid)` |
| `exit(0)`（exec 失败） | 失败报成功 + 跑 atexit/析构有锁风险 | `_exit(127)` |
| 无进程组 | 无法整组管理 | `setpgid` + subreaper |
| 退出码丢弃 | 无法判断崩溃/启动失败 | WIFEXITED/WIFSIGNALED 记录日志 |
| 相对路径 `./xxx` | 依赖 cwd | `.desktop` 用绝对路径 |
| `stringToArgv` 泄漏 | 每次启动泄漏 | vector<char*> 生命周期管理 + `freeArgv` |
| 无实例锁 | 重复启动 | flock（`/tmp/<name>.lock`） |

> 触摸屏逃生通道（ESC 长按强制退出）**未实现**：本项目输入只有触摸，
> 且无键盘事件源；外部应用卡死由应用自身负责，mainPage 等待 leader 退出。

## 5. 网络对时（TimeSync）

- 来源（HTTP 而非 HTTPS，避免"时钟错→证书 not yet valid"死结）：
  1. `http://worldtimeapi.org/api/ip` → JSON `unixtime`
  2. 回退 `http://www.baidu.com` → `Date` 响应头（RFC 1123）
- musl 无 `timegm`，自实现（Howard Hinnant days_from_civil）；glibc 需 `_GNU_SOURCE`。
- 触发：`main()` 在 `HAL::Init` 前一次；data 线程每 60s 一次（仅 ARM 编译启用）。

## 6. 已知边界 / 后续

- **删除应用**：inotify 触发重扫但只做"新增"（增量），已删除的 .desktop 对应按钮
  不会自动移除（需整页重建或 View 增加 removeApplication，留待后续）。
- **图标**：`Icon` 空时按钮无图标（`lv_img_set_src` 传空串会因指针解引用崩溃，
  已改为传 `nullptr` 并跳过）。
- **心跳/watchdog**：应用卡死时 mainPage 会一直等待；如需强制退出需接入触摸长按
  手势 + killpg，列入后续。
- **C++ 标准**：项目保持 C++11（GCC 6.4.1 交叉工具链约束），Launch 模块未使用
  C++17 特性（用 `valid` 标志代替 `std::optional`）。
