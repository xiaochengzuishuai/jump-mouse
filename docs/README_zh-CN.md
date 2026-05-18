# Jump Mouse — 仅跳转（中文完整文档）

**Alt+Tab 窗口切换时，鼠标光标自动跳转到新聚焦窗口的中心位置。**

---

## 目录

- [简介](#简介)
- [功能特性](#功能特性)
- [快速开始](#快速开始)
- [配置详解](#配置详解)
- [构建指南](#构建指南)
- [架构说明](#架构说明)
- [常见问题](#常见问题)
- [更新日志](#更新日志)

---

## 简介

在 Windows 10/11 多显示器环境下，使用 Alt+Tab 切换窗口后，手动去移动鼠标到新窗口中心是一件繁琐的事情。Jump Mouse 自动化了这个过程：

1. 你按下 Alt+Tab 切换窗口
2. 鼠标自动移动到新窗口的中心
3. 你立刻就能操作，无需手动找光标

**零运行时依赖**，下载即用，无需安装任何运行库。

---

## 功能特性

### 核心能力

| 特性 | 说明 |
|------|------|
| **Alt+Tab 检测** | 三阶段检测（Alt按下→记录候选窗口→Alt释放执行移动），准确率极高 |
| **任意聚焦变化** | 可选：任何窗口聚焦变化都触发移动 |
| **瞬间移动** | `SetCursorPos` 直接跳转，零延迟 |
| **平滑动画** | cubic ease-out 缓动曲线，先快后慢，视觉自然 |
| **延迟等待** | 最小化窗口恢复动画耗时不一，可配置延迟确保获取正确窗口位置 |
| **系统窗口过滤** | 自动忽略任务栏、桌面、Alt+Tab 切换器等非用户窗口 |
| **进程排除** | 指定进程名（如 `devenv.exe`），其窗口聚焦时不触发移动 |

### GUI 配置工具

- Win32 原生对话框，启动快速、内存占用小
- 所有配置项可视化编辑，无需手动修改 JSON
- 系统托盘常驻，右键菜单控制服务启停
- 配置保存后守护进程自动热重载

### 跨版本兼容

- Windows 10 1809+ 与 Windows 11 均支持
- UWP 应用窗口正确处理
- PerMonitorV2 DPI 适配，多显示器不同缩放比正常工作
- Win10 Alt+Tab 切换器（`TaskSwitcherWnd`）与 Win11（`TopLevelWindowForOverflowXamlIsland`）均正确识别

---

## 快速开始

### 下载运行

1. 从 [GitHub Releases](https://github.com) 下载最新 `Jump Mouse - v1.0.zip`
2. 解压到任意目录
3. 双击运行 `jump_mouse_config.exe`
4. 点击界面上的「**启动**」按钮
5. 试试 Alt+Tab 切换窗口，鼠标自动跟随

### 开机自启

在 GUI 配置工具中勾选「**开机自启**」→ 点击「**保存设置**」即可。

### 命令行参数（守护进程）

```bash
# 使用默认 config.json（exe 同目录）
jump_mouse_daemon.exe

# 指定配置文件路径
jump_mouse_daemon.exe --config "D:\my_config.json"

# 显示帮助
jump_mouse_daemon.exe --help
```

---

## 配置详解

配置文件 `config.json`，支持 GUI 编辑或直接修改。

```jsonc
{
  // 触发模式: "alt_tab_only" | "any_focus_change"
  "triggerMode": "alt_tab_only",

  // 鼠标移动模式: "instant" | "smooth"
  "mouseMode": "smooth",

  // 平滑动画持续时间（毫秒），范围 50-600
  "smoothDurationMs": 600,

  // 最小化窗口恢复等待时间（毫秒），范围 0-2000
  "moveDelayMs": 100,

  // 目标区域: "window_rect"（含标题栏）| "client_rect"（仅客户区）
  "targetArea": "window_rect",

  // 总开关
  "enabled": true,

  // 排除的进程名列表（不区分大小写）
  "excludedProcesses": [],

  // 排除的窗口类名片段
  "excludedClasses": [],

  // 日志级别: "none" | "error" | "warn" | "info" | "debug"
  "logLevel": "none",

  // 日志输出文件路径（空字符串 = 不写文件）
  "logFile": ""
}
```

### 配置热重载

在 GUI 中保存配置或直接修改 `config.json` 后，守护进程会**在 2 秒内**自动检测并加载新配置，无需手动重启。

---

## 构建指南

### 环境要求

- Visual Studio 2022（Community 免费版即可）+ "使用 C++ 的桌面开发" 工作负荷
- Windows 10 SDK 10.0.19041+（随 VS 安装）
- CMake 3.20+（随 VS 安装）

### 编译步骤

```bash
# 打开 "x64 Native Tools Command Prompt for VS 2022"

cmake -B build -S . -A x64
cmake --build build --config Release
```

产物位于 `build/release/`：
```
build/release/
├── jump_mouse_daemon.exe
├── jump_mouse_config.exe
└── config.json
```

### GitHub Actions 自动构建

推送 `v*` 标签到 GitHub 后，Actions 自动编译并创建 Release。

---

## 架构说明

### 双可执行文件

```
jump_mouse_daemon.exe    后台守护进程（无窗口）
    ├── FocusMonitor      SetWinEventHook 监听前台窗口变化
    ├── KeyTracker        WH_KEYBOARD_LL 追踪 Alt 键状态
    ├── TriggerDetector   综合判定是否为有效 Alt+Tab 触发
    └── MouseController   计算窗口中心 + 执行移动 / 平滑动画

jump_mouse_config.exe    GUI 配置工具
    ├── ConfigDialog      Win32 对话框 UI（资源脚本驱动）
    ├── ConfigManager      JSON 配置读写（与守护进程共用）
    └── DaemonController  守护进程的启动/停止/状态查询
```

### 核心模块

| 模块 | 文件 | 职责 |
|------|------|------|
| 配置管理 | `config_manager.h/cpp` | JSON 读写、验证、热加载 |
| 适配层 | `adaptation_layer.h/cpp` | Win10/11 差异、DPI、UWP 窗口处理 |
| 焦点监控 | `focus_monitor.h/cpp` | `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` 封装 |
| 按键追踪 | `key_tracker.h/cpp` | `WH_KEYBOARD_LL` 低级键盘钩子 |
| 触发判定 | `trigger_detector.h/cpp` | Alt+Tab 三阶段检测逻辑核心 |
| 鼠标控制 | `mouse_controller.h/cpp` | 窗口中心计算 + instant/smooth 移动 |
| GUI 对话框 | `config_dialog.h/cpp` | Win32 对话框 UI，系统托盘 |
| 守护管理 | `daemon_controller.h/cpp` | CreateProcess 启停守护进程 |

### 技术栈

- **语言**: C++20（`<filesystem>`, `<chrono>`, `<format>`）
- **UI**: Win32 Dialog（`.rc` 资源脚本）
- **构建**: CMake + MSVC
- **依赖**: 零第三方库，仅 Windows SDK

---

## 常见问题

### Q: 启动守护进程后，Alt+Tab 无效？

1. 检查配置文件中 `"enabled": true`
2. 排除列表中的进程不会触发移动，将 `excludedProcesses` 设为空数组再试
3. 将 `logLevel` 设为 `"debug"` 并指定 `logFile` 路径，查看日志排查

### Q: 最小化窗口恢复后鼠标位置不正确？

增加 `moveDelayMs` 的值（如 300-500ms），等待窗口恢复动画完成后再移动。

### Q: 鼠标移动卡顿/不平滑？

- 瞬间模式无卡顿，平滑模式使用 10ms 定时器分帧
- 可增加 `smoothDurationMs` 让动画更慢
- 将 `mouseMode` 设为 `"instant"` 完全跳过动画

### Q: 无法启动守护进程？

- 检查是否已有实例运行（任务管理器查找 `jump_mouse_daemon.exe`）
- 确认 exe 文件与 `config.json` 在同一目录
- 日志级别设为 `"debug"` 查看错误信息

### Q: 开机自启不生效？

- 在 GUI 中取消勾选 → 保存 → 重新勾选 → 保存
- 或手动检查注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\JumpMouse`

---

## 项目结构

```
jump-mouse/
├── src/
│   ├── common/
│   │   ├── config_manager.h/cpp     # JSON 配置读写/验证
│   │   ├── adaptation_layer.h/cpp   # Win 版本检测、DPI、窗口过滤
│   │   ├── json_util.h              # 自研最小 JSON 解析器
│   │   └── logger.h                 # 轻量日志宏
│   ├── daemon/
│   │   ├── daemon_main.cpp          # 守护进程入口
│   │   ├── application.h/cpp        # 生命周期管理、模块组装
│   │   ├── focus_monitor.h/cpp      # SetWinEventHook 封装
│   │   ├── key_tracker.h/cpp        # WH_KEYBOARD_LL 钩子
│   │   ├── trigger_detector.h/cpp   # 触发逻辑核心
│   │   └── mouse_controller.h/cpp   # 光标移动（instant + smooth）
│   └── config_tool/
│       ├── config_main.cpp          # WinMain 入口
│       ├── config_dialog.h/cpp      # 主设置对话框
│       └── daemon_controller.h/cpp  # 守护进程管理
├── CMakeLists.txt
├── config.json
├── README.md
└── docs/
    ├── README_zh-CN.md              # 本文件
    ├── README_en.md                 # 英文文档
    ├── DESIGN_DOC.md
    ├── BUILD_GUIDE.md
    ├── CODE_REVIEW.md
    └── ISSUES.md
```

---

## 更新日志

### 仅跳转V1.0（2026-05-18）

- 稳定版第一版
- 核心功能：Alt+Tab 检测 + 鼠标移动（instant/smooth）
- Win32 GUI 配置工具 + 系统托盘 + 配置热重载
- 修复资源 ID 冲突（UI 控件混乱问题的根因）
- 移除旧版鼠标个性化功能及相关代码
- 静态链接 CRT，无需安装运行库

---

## 许可

MIT License
