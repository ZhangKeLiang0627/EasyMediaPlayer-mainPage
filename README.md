# eMP_mainPage

Allwinner T113-S3 板子的主界面（Launcher），基于 LVGL 8.3 + sunxifb，负责展示 eMP 系列应用入口并跳转。

## 功能

- **应用自动发现**：监听 `/mnt/UDISK/applications/*.desktop`，插 U 盘 / 拷入新应用自动加载按钮（inotify）
- **应用启动器**：fork + execv 重构（独立进程组 / subreaper / 退出码记录 / 实例锁），跳转更健壮
- **网络对时**（ARM 编译启用）：板子无 RTC 时自动同步系统时间（WorldTimeAPI → Baidu 回退）
- **现代 JSON**：`nlohmann/json` 替代旧版 cJSON
- 网络状态 / 时间日期展示、U 盘图标、HTTP 管理服务（6210 端口）

## Snapshots

![](./pictures/image-1.jpg)
![](./pictures/image-2.png)

## 环境

```shell
# 交叉编译需要（按本机实际路径修改）
export STAGING_DIR=/home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target
```

## 编译

- Makefile
```shell
# 交叉编译
./build.sh

# 本地编译
./build.sh 0
```

- CMake
```shell
# 交叉编译
export STAGING_DIR=/home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_t113s3.cmake ..
make -j32

# 本地编译
mkdir build && cd build
cmake ..
make -j32
```

## 运行

可执行文件为：`eMP_mainPage`

> **注意：本地编译，这需要一些文件依赖和库依赖才可以编译/运行起来...**

### 本地编译的库依赖
```bash
sudo apt update

# 包含 gcc、g++、make 等基础工具
sudo apt install build-essential  

# SDL2库、Freetype
sudo apt install libsdl2-dev libfreetype6-dev libncurses5-dev libstdc++6 

# Plus: 如果cmake时遇到Wayland依赖找不到的情况时，请输入以下指令安装
sudo apt install -y wayland-protocols libxkbcommon-dev
```

### 本地编译的文件依赖

```bash
# 请创建对应文件夹
sudo mkdir /mnt/UDISK /mnt/UDISK/applications
```

并将[此处的内容](https://github.com/ZhangKeLiang0627/EasyMediaPlayer/tree/main/firmware)全部移入该文件夹中即可！

## 应用自动发现（.desktop）

每个应用在 `/mnt/UDISK/applications/` 放一个 `.desktop` 描述文件，mainPage 启动时扫描、
运行中通过 inotify 监听目录变化自动加载/刷新按钮。

示例 `/mnt/UDISK/applications/eMP_tokenMonitor.desktop`：

```ini
[Desktop Entry]
Name=Token Monitor
Exec=/mnt/UDISK/eMP_tokenMonitor
Icon=
Type=Application
```

| 字段 | 说明 |
|---|---|
| `Name` | 按钮显示名称（必填） |
| `Exec` | 可执行文件绝对路径 + 可选参数（必填，须通过安全检查） |
| `Icon` | 图标路径（绝对路径，或相对 `eMP_mainPage` 可执行目录；留空则按钮无图标） |
| `Type` | 必须为 `Application` |

安全规则（`src/Launch/desktop_entry.cpp`）：
- 含 shell 元字符 `|&;<>\`$\\\n\r` 的 Exec 会被拒绝（防注入）
- 带路径的 Exec 必须是真实存在的可执行文件
- 纯命令名走白名单（bash/python3/vim/vi/nano/sh）
- `Hidden`/`NoDisplay=true` 的文件会被忽略

> 向后兼容：`./config/sysconfig.json` 的 `applications` 数组仍生效，作为兜底合并
> （与 .desktop 按可执行路径去重）。新应用请优先使用 .desktop。

## 应用启动器（Launch 模块）

`src/Launch/` 三个文件构成应用管理核心：

| 文件 | 职责 |
|---|---|
| `desktop_entry.h/.cpp` | .desktop 解析 + Exec 安全校验 |
| `app_scanner.h/.cpp` | 扫描 applications/ 目录 |
| `app_runner.h/.cpp` | fork/execv 启动器（进程组 / subreaper / 退出码 / flock 实例锁 / LVGL 协调） |

启动流程修复了原实现的多个缺陷（详见 `docs/launch-system.md`）：
- `wait(nullptr)` → `waitpid(pid)` + EINTR 重试
- exec 失败 `_exit(127)`（原来 `exit(0)` 语义错误）
- 子进程独立进程组 + subreaper，防孤儿/僵尸
- 记录退出码（正常退出 / 信号终止）
- flock 实例锁：同一应用重复点击不再重复启动
- `lv_timer_enable(false)` 暂停 LVGL 渲染，避免主界面覆盖子进程画面

## 网络对时（ARM）

T113-S3 无 RTC（默认 1970），HTTPS 证书会报 "not yet valid"。mainPage 在 ARM 编译下：
- `main()` 启动时（`HAL::Init` 前）对时一次
- data 线程每 60s 周期对时
- 数据源：`http://worldtimeapi.org/api/ip` 的 `unixtime`，失败回退 `http://www.baidu.com` 的 `Date` 头

## 文件

- `./config/sysconfig.json`：亮度/音量等系统参数 + 旧版 applications 兜底
- `./inc/Launch/` + `./src/Launch/`：应用描述 / 扫描 / 启动器
- `./inc/TimeSync.h` + `./src/Net/TimeSync.cpp`：网络对时
- `./libs/nlohmann/json.hpp`：JSON 解析（替代 cJSON）

## 详细设计

见 [`docs/launch-system.md`](docs/launch-system.md)
