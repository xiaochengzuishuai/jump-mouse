# 窗口聚焦鼠标自动移动 — 设计文档

> 版本：v2.0 | 日期：2026-05-16 | 状态：待评审 | 变更：双可执行文件架构 + Win32 GUI 配置工具

---

## 1. 需求概述

**目标**：在 Windows 10/11 多显示器环境下，用户通过 Alt+Tab 切换前台窗口时，自动将鼠标光标移动到新聚焦窗口的中心位置。

**架构方案**：双可执行文件
- `mouse_focus_daemon.exe` — 无窗口后台守护进程，负责实际的窗口监听与鼠标控制
- `mouse_focus_config.exe` — Win32 原生 GUI 配置工具，所见即所得编辑配置，保存为 `config.json`

**核心约束**：
- 守护进程无窗口后台运行，配置工具为独立 GUI
- 所有行为可通过 GUI 可视化配置
- 自适应 Win10/Win11 差异
- 代码可扩展、结构优良
- 零额外运行时依赖（仅 Windows SDK + 标准库）

---

## 2. 架构总览

### 2.1 整体架构（双可执行文件）

```
┌──────────────────────────────────────────────────┐
│                config.json                        │
│          （JSON 配置文件，双方共享）                │
└────────┬──────────────────────┬──────────────────┘
         │ 读取                  │ 读写
         ▼                       ▼
┌────────────────────┐  ┌──────────────────────────┐
│ mouse_focus_daemon │  │ mouse_focus_config.exe    │
│ （后台守护进程）     │  │ （Win32 GUI 配置工具）     │
│                    │  │                          │
│ 读取config.json    │  │ 可视化编辑所有配置项       │
│ 监听窗口切换        │  │ 保存到config.json         │
│ 移动鼠标           │  │ 启动/停止守护进程          │
│ 热重载配置          │  │ 查看运行状态              │
└────────────────────┘  └──────────────────────────┘
```

### 2.2 守护进程内部架构

```
┌─────────────────────────────────────────────────────┐
│                  daemon_main.cpp                     │
│   入口：解析命令行 → 初始化 → 消息泵 → 清理退出      │
└──────────┬──────────────────────────────────────────┘
           │ 创建/持有
    ┌──────┴──────┐
    │ Application │  ← 生命周期管理、各模块协调
    └──┬──┬──┬──┬─┘
       │  │  │  │
  ┌────┘  │  │  └────┐
  │       │  │       │
  ▼       ▼  ▼       ▼
┌────┐ ┌────┐ ┌────┐ ┌──────────┐
│配置│ │焦点│ │按键│ │鼠标控制器 │
│管理│ │监控│ │追踪│ │          │
└────┘ └──┬─┘ └──┬─┘ └──────────┘
          │      │         ▲
          └──┬───┘         │
             ▼             │
        ┌──────────┐       │
        │触发判定器 │───────┘
        └──────────┘
             ▲
        ┌────┴────┐
        │适配层    │ ← Win10/Win11 差异封装
        │日志系统  │
        └─────────┘
```

### 2.3 GUI 配置工具架构

```
┌────────────────────────────────────────┐
│           config_main.cpp               │
│   WinMain → 初始化 → 对话框 → 消息循环  │
└──────────────┬─────────────────────────┘
               │
    ┌──────────┴──────────┐
    │                     │
    ▼                     ▼
┌──────────────┐  ┌──────────────────┐
│ ConfigDialog │  │ DaemonController │
│ （主设置窗口） │  │ （守护进程管理）   │
│              │  │                  │
│ Win32 对话框  │  │ 启动守护进程      │
│ 布局控件绑定  │  │ 停止守护进程      │
│ 读/写配置    │  │ 查询运行状态      │
└──────┬───────┘  └──────────────────┘
       │
       ▼
┌──────────────┐
│ ConfigManager │  ← 与守护进程共用同一代码
└──────────────┘
```

### 2.4 模块职责（共用模块）

| 模块 | 职责 | 被谁使用 |
|------|------|---------|
| `ConfigManager` | JSON 配置读写、验证、默认值 | daemon + config tool |
| `FocusMonitor` | 监听前台窗口变化事件 | daemon only |
| `KeyTracker` | 追踪 Alt 键按下/释放状态 | daemon only |
| `TriggerDetector` | 判定窗口切换是否为 Alt+Tab 触发 | daemon only |
| `MouseController` | 计算窗口中心 + 移动光标 | daemon only |
| `AdaptationLayer` | Win10/Win11 差异、DPI、UWP 检测 | daemon + config tool |
| `Logger` | 可选调试日志 | daemon + config tool |
| `DaemonController` | 启动/停止/查询守护进程 | config tool only |
| `ConfigDialog` | Win32 对话框 UI 布局与逻辑 | config tool only |

---

## 3. 核心流程

### 3.1 Alt+Tab 检测逻辑（默认模式）

```
       时间轴 ─────────────────────────────────────►

   Alt 按下     前台窗口变化       Alt 释放
      │              │                │
      ▼              ▼                ▼
  ┌──────┐    ┌──────────────┐   ┌──────────┐
  │标记   │    │记录新窗口句柄 │   │触发移动  │
  │Alt=true│   │候选 = HWND   │   │候选窗口  │
  └──────┘    └──────────────┘   │Alt=false │
                                 └──────────┘
```

**关键判定条件**：
1. 前台窗口变化事件到达时 `Alt` 处于按下状态 → 标记为"Alt+Tab 候选"
2. 忽略 Alt+Tab 切换器自身窗口（类名 `TaskSwitcherWnd`、`MultitaskingViewFrame`）
3. Alt 释放时，若存在候选窗口 → 执行鼠标移动
4. 排除非用户窗口（TaskBar、Progman、Shell_TrayWnd 等）
5. Win+Tab 可以通过检测 `VK_LWIN` / `VK_RWIN` 作为补充支持

### 3.2 任意聚焦变化模式

```
   前台窗口变化 → 过滤系统窗口 → 直接执行鼠标移动
```

### 3.3 鼠标移动流程

```
 移动请求(HWND)
      │
      ▼
┌──────────────┐
│窗口有效性检查 │ ← IsWindow(), IsIconic(), IsWindowVisible()
└──────┬───────┘
       ▼
┌──────────────┐
│获取窗口矩形  │ ← GetWindowRect()  物理坐标
└──────┬───────┘
       ▼
┌──────────────┐
│含标题栏？    │ ← 可选：仅客户区
└──────┬───────┘
       ▼
┌──────────────┐
│ DPI 坐标修正 │ ← Win10 多显示器不同缩放比例
└──────┬───────┘
       ▼
┌──────────────┐
│执行移动      │
│instant: SetCursorPos()
│smooth: 分段插值动画
└──────────────┘
```

---

## 4. 配置文件设计

### 4.1 格式与位置

- 格式：JSON
- 默认路径：可执行文件同目录 `config.json`
- 支持命令行指定：`mouse_focus.exe --config "D:\path\config.json"`
- 支持运行时热重载（监听文件变更或定期检查）

### 4.2 Schema

```jsonc
{
  // === 核心行为 ===
  "version": 1,

  // 触发模式
  // "alt_tab_only"       — 仅 Alt+Tab 切换（含 Shift 反向）
  // "any_focus_change"   — 任何窗口聚焦变化均触发
  "triggerMode": "alt_tab_only",

  // 鼠标移动模式
  // "instant" — 瞬间移动
  // "smooth"  — 平滑动画
  "mouseMode": "instant",

  // 平滑动画持续时间（毫秒），仅 mouseMode=smooth 时生效
  "smoothDurationMs": 150,

  // 是否启用
  "enabled": true,

  // === 排除规则 ===

  // 排除的进程名（不含路径），不区分大小写
  "excludedProcesses": [
    // 示例:
    // "devenv.exe",
    // "spotify.exe"
  ],

  // 排除的窗口类名片段（部分匹配）
  "excludedClasses": [
    // 默认始终排除系统窗口，此处为用户自定义追加
  ],

  // === 高级选项 ===

  // 目标区域
  // "window_rect"    — 窗口完整矩形（含标题栏）的中心
  // "client_rect"    — 客户区中心（不含标题栏/边框）
  "targetArea": "window_rect",

  // 日志级别: "none" | "error" | "warn" | "info" | "debug"
  "logLevel": "none",

  // 日志文件路径（空字符串 = 不写文件）
  "logFile": ""
}
```

---

## 5. Windows 版本适配

| 特性点 | Win10 行为 | Win11 行为 | 适配策略 |
|--------|-----------|-----------|---------|
| Alt+Tab 窗口类名 | `TaskSwitcherWnd` | `TopLevelWindowForOverflowXamlIsland` | 类名模糊匹配，不硬编码 |
| 圆角窗口 | 无 | Win11 圆角影响 `GetWindowRect` 边界（极小，可忽略） | 无需特殊处理 |
| Snap 布局 | 无 | 窗口被 Snap 后 rect 变小 | `GetWindowRect` 返回实际 rect，天然适配 |
| 虚拟桌面 | SetWinEventHook 跨桌面 | 同左 | 默认跨桌面监听，用户可配置 `WINEVENT_SKIPOWNPROCESS` |
| DPI 缩放 | 支持 per-monitor v2 | 同左 | 声明 `PerMonitorV2` DPI Awareness |
| UWP 应用 | 有 `ApplicationFrameWindow` 包裹 | 同左 | 获取实际子窗口 rect |

### DPI 适配要点

```cpp
// 程序清单声明 DPI Awareness
// PerMonitorV2: 每个显示器独立缩放，窗口移动时自动通知

// 计算窗口中心时：
// GetWindowRect 返回物理坐标，SetCursorPos 接受物理坐标
// 不需要额外转换，但需要确保程序 DPI Aware
```

---

## 6. 文件结构

```
mouse_focus/
├── CMakeLists.txt              # CMake 构建（MSVC）
├── config.json                 # 默认配置
├── DESIGN_DOC.md               # 本文件
│
├── src/
│   ├── common/                 # === 共用模块 ===
│   │   ├── config_manager.h/cpp     # JSON 配置读写/验证
│   │   ├── adaptation_layer.h/cpp   # Win 版本检测、DPI、窗口过滤
│   │   └── logger.h                 # 轻量日志宏
│   │
│   ├── daemon/                 # === 守护进程 ===
│   │   ├── daemon_main.cpp          # 入口、命令行解析
│   │   ├── application.h/cpp        # 生命周期管理，模块组装
│   │   ├── focus_monitor.h/cpp      # SetWinEventHook 封装
│   │   ├── key_tracker.h/cpp        # 低级键盘钩子 (WH_KEYBOARD_LL)
│   │   ├── trigger_detector.h/cpp   # 触发逻辑核心
│   │   └── mouse_controller.h/cpp   # 光标移动（instant + smooth）
│   │
│   └── config_tool/            # === GUI 配置工具 ===
│       ├── config_main.cpp          # WinMain 入口
│       ├── config_dialog.h/cpp      # 主设置对话框（Win32 Dialog）
│       └── daemon_controller.h/cpp  # 守护进程管理（启/停/状态）
│
├── third_party/
│   └── nlohmann/               # nlohmann/json (header-only)
│
└── README.md
```

**依赖**：
- 标准库：C++20（`<filesystem>`、`<chrono>`、`<optional>`、`<format>`）
- 第三方：`nlohmann/json`（header-only，JSON 解析）
- 系统：Windows SDK（`user32`、`kernel32`、`shell32`、`comctl32`）
- 无其他运行时依赖

---

## 7. GUI 配置工具设计

### 7.1 窗口布局

```
┌─────────────────────────────────────────────────────────────┐
│  Mouse Focus — 设置                              [—] [□] [×]│
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─ 核心行为 ────────────────────────────────────────────┐ │
│  │                                                        │ │
│  │  触发模式：                                            │ │
│  │    ◉ 仅 Alt+Tab 切换    ○ 任意窗口聚焦变化            │ │
│  │                                                        │ │
│  │  鼠标移动：                                            │ │
│  │    ◉ 瞬间移动          ○ 平滑动画                     │ │
│  │                                                        │ │
│  │  动画时长：[ 150 ] ms  (50-300)                        │ │
│  │                                                        │ │
│  │  目标区域：                                            │ │
│  │    ◉ 窗口中心（含标题栏）  ○ 客户区中心                │ │
│  │                                                        │ │
│  │  ☑ 启用服务                                            │ │
│  │                                                        │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
│  ┌─ 排除规则 ────────────────────────────────────────────┐ │
│  │                                                        │ │
│  │  排除进程（按进程名，如 "devenv.exe"）：               │ │
│  │  ┌──────────────────────────────────┐  [添加]          │ │
│  │  │ devenv.exe                       │  [移除]          │ │
│  │  │ spotify.exe                      │                  │ │
│  │  │                                  │                  │ │
│  │  └──────────────────────────────────┘                  │ │
│  │                                                        │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
│  ┌─ 守护进程状态 ────────────────────────────────────────┐ │
│  │  状态：● 运行中         [启动] [停止] [开机自启]       │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
│  ┌─ 日志 ────────────────────────────────────────────────┐ │
│  │  日志级别：[无 ▼]    日志文件：[                ] [浏览]│ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
│                    [保存设置]   [取消]                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 控件映射

| 配置项 | 控件类型 | 控件 ID | 说明 |
|--------|---------|---------|------|
| `triggerMode` | Radio Button 组 | `IDC_RADIO_ALT_TAB` / `IDC_RADIO_ANY` | 二选一 |
| `mouseMode` | Radio Button 组 | `IDC_RADIO_INSTANT` / `IDC_RADIO_SMOOTH` | 二选一 |
| `smoothDurationMs` | Edit + Spin | `IDC_EDIT_DURATION` + `IDC_SPIN_DURATION` | 范围 50-300 |
| `targetArea` | Radio Button 组 | `IDC_RADIO_WINDOW` / `IDC_RADIO_CLIENT` | 二选一 |
| `enabled` | CheckBox | `IDC_CHECK_ENABLED` | 勾选/取消 |
| `excludedProcesses` | ListBox + Add + Remove | `IDC_LIST_EXCLUDE` + `IDC_BTN_ADD` + `IDC_BTN_REMOVE` | 进程名列表 |
| `logLevel` | ComboBox | `IDC_COMBO_LOG` | 无/错误/警告/信息/调试 |
| `logFile` | Edit + Browse | `IDC_EDIT_LOGFILE` + `IDC_BTN_BROWSE` | 文件路径 |
| 守护状态 | Static Text + Buttons | `IDC_STATUS` + `IDC_BTN_START` + `IDC_BTN_STOP` | 状态显示 |
| 保存 | Button | `IDOK` | 写 config.json |
| 取消 | Button | `IDCANCEL` | 放弃修改并关闭 |

### 7.3 对话框处理流程

```
WM_INITDIALOG
  │
  ├── ConfigManager::load()      ← 从 config.json 读取当前配置
  ├── DaemonController::status() ← 查询守护进程是否在运行
  ├── 各控件 SetCheck/SetText/... ← 填充 UI
  └── 返回 TRUE

WM_COMMAND:
  ├── IDC_RADIO_SMOOTH → 启用/禁用动画时长控件
  ├── IDC_BTN_ADD      → 弹出输入框，添加进程名到列表
  ├── IDC_BTN_REMOVE   → 删除列表选中项
  ├── IDC_BTN_START    → DaemonController::start()
  ├── IDC_BTN_STOP     → DaemonController::stop()
  ├── IDC_BTN_BROWSE   → 打开文件选择对话框
  ├── IDOK             → 收集控件值 → ConfigManager::save() → EndDialog
  └── IDCANCEL         → EndDialog（不保存）

WM_CLOSE → EndDialog
```

### 7.4 DaemonController 接口

```cpp
class DaemonController {
public:
    // 检查守护进程是否在运行
    bool isRunning() const;

    // 启动守护进程（CreateProcess，无窗口）
    // 返回 true 表示成功启动
    bool start();

    // 停止守护进程（发送 WM_QUIT 到守护进程的消息窗口）
    bool stop();

    // 切换开机自启（写注册表 HKCU\...\Run）
    bool setAutoStart(bool enable);
    bool isAutoStart() const;

private:
    // 通过命名互斥体检测进程
    static constexpr const wchar_t* MUTEX_NAME =
        L"Global\\MouseFocusScript_Instance";

    // 守护进程可执行文件路径（与本工具同目录）
    std::wstring daemonPath() const;
};
```

### 7.5 配置文件热重载（守护进程侧）

守护进程通过 `ReadDirectoryChangesW` 监视 `config.json` 所在目录：

```
config.json 被配置工具修改
       │
       ▼
ReadDirectoryChangesW 通知
       │
       ▼
等待 100ms 防抖（文件可能还在写入）
       │
       ▼
ConfigManager::load() 重新读取
       │
       ▼
TriggerDetector 等模块读取新配置
```

---

## 8. 关键类接口（守护进程）

### 8.1 Application

```cpp
class Application {
public:
    int run(HINSTANCE hInstance);

private:
    ConfigManager      m_config;
    std::unique_ptr<FocusMonitor>     m_focusMonitor;
    std::unique_ptr<KeyTracker>       m_keyTracker;
    std::unique_ptr<TriggerDetector>  m_triggerDetector;
    std::unique_ptr<MouseController>  m_mouseController;

    HWND m_hwnd = nullptr;  // 隐藏消息窗口
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void setupModules();
    void teardownModules();
};
```

### 8.2 FocusMonitor

```cpp
using FocusChangeCallback = std::function<void(HWND newForeground)>;

class FocusMonitor {
public:
    void start(FocusChangeCallback cb);
    void stop();
    HWND currentForeground() const;

private:
    HWINEVENTHOOK m_hook = nullptr;
    FocusChangeCallback m_callback;

    static void CALLBACK eventProc(
        HWINEVENTHOOK, DWORD event,
        HWND hwnd, LONG idObject, LONG idChild,
        DWORD, DWORD);
};
```

### 8.3 KeyTracker

```cpp
class KeyTracker {
public:
    void start();
    void stop();
    bool isAltDown() const;
    bool wasAltRecentlyDown(std::chrono::milliseconds window) const;
    // 注册 Alt 释放回调
    using AltReleaseCallback = std::function<void()>;
    void onAltReleased(AltReleaseCallback cb);

private:
    HHOOK m_hook = nullptr;
    std::atomic<bool> m_altDown{false};
    std::chrono::steady_clock::time_point m_altUpTime;
    AltReleaseCallback m_altReleaseCb;

    static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
};
```

### 8.4 TriggerDetector

```cpp
class TriggerDetector {
public:
    TriggerDetector(ConfigManager& cfg, MouseController& mouse);

    void onFocusChange(HWND hwnd);
    void onAltPressed();
    void onAltReleased();

    // 注册实际执行移动的回调（供平滑动画异步使用）
    using MoveCallback = std::function<void(HWND)>;
    void onMoveRequested(MoveCallback cb);

private:
    ConfigManager&     m_config;
    MouseController&   m_mouse;
    MoveCallback       m_moveCb;

    bool  m_altDown        = false;
    HWND  m_candidateHwnd  = nullptr;

    bool isSystemWindow(HWND hwnd);
    bool isExcludedProcess(HWND hwnd);
    bool isExcludedClass(HWND hwnd);
};
```

### 8.5 MouseController

```cpp
enum class MouseMode { Instant, Smooth };

struct WindowCenter {
    LONG x, y;
};

class MouseController {
public:
    WindowCenter calculateCenter(HWND hwnd, bool clientArea) const;
    void moveInstant(HWND hwnd, bool clientArea);
    void moveSmooth(HWND hwnd, bool clientArea,
                    std::chrono::milliseconds duration);

private:
    void doSmoothMove(POINT from, POINT to,
                      std::chrono::milliseconds duration);
};
```

---

## 9. 平滑动画算法

使用 **ease-out 缓动曲线**（先快后慢，更自然）：

```
t = elapsed / duration           // 归一化时间 [0, 1]
eased = 1 - (1-t)^3              // cubic ease-out
x(t) = from.x + (to.x - from.x) * eased
y(t) = from.y + (to.y - from.y) * eased
```

实现方式：
- 高频定时器（~1000Hz）驱动每帧
- 避免阻塞消息泵（使用 `SetTimer` + `WM_TIMER`）
- 中途有新目标窗口 → 放弃当前动画，立即跳转新目标

---

## 10. 系统窗口识别规则

以下窗口**始终不触发**移动：

| 窗口 | 识别方式 |
|------|---------|
| 桌面 (Progman/WorkerW) | 类名 `Progman` / `WorkerW` |
| 任务栏 | 类名 `Shell_TrayWnd` |
| Alt+Tab 切换器 | 类名包含 `TaskSwitcher` / `TopLevelWindowForOverflow` |
| 开始菜单 (Win10) | 类名 `Windows.UI.Core.CoreWindow` |
| 空 HWND | `hwnd == nullptr` 或 `!IsWindow(hwnd)` |
| 不可见窗口 | `!IsWindowVisible(hwnd)` |
| 无所有者的顶层消息窗口 | `GetWindow(hwnd, GW_OWNER) == nullptr && className == "..."` |

---

## 11. 扩展点

未来可扩展的方向（代码结构已预留）：

1. **Win+Tab 支持** — 复用 Win 键追踪逻辑
2. **特定显示器绑定** — "仅当前显示器上的窗口才触发"
3. **窗口排除规则可视化编辑** — 运行时通过热键"排除当前窗口"
4. **应用到特定场景** — 仅在特定工作区/桌面布局下触发
5. **动画风格自定义** — 支持多种缓动曲线（ease-in、ease-in-out、spring 等）
6. **鼠标附加操作** — 到达目标后自动点击、滚轮等
7. **多语言国际化** — GUI 界面中英切换

---

## 12. 安全与边界

| 场景 | 处理 |
|------|------|
| 安全桌面（UAC） | 低权限钩子不跨会话，自动失效 |
| 全屏游戏 | 需要评估：可配置排除全屏进程 |
| 远程桌面 | 正常工作的显示器虚拟化下行为一致 |
| 多用户同时登录 | 每会话独立实例，句柄不跨会话 |
| 意外退出 | 钩子在进程退出时由 OS 清理 |
| 高 CPU 动画 | 平滑动画限制最长时间 300ms，超时强制结束 |

---

## 13. 构建与运行

### CMakeLists.txt（概要）

```cmake
cmake_minimum_required(VERSION 3.20)
project(mouse_focus VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========== 共用库（ConfigManager, AdaptationLayer, Logger）==========
add_library(common STATIC
    src/common/config_manager.cpp
    src/common/adaptation_layer.cpp
)

target_include_directories(common PUBLIC
    src third_party/nlohmann/include
)

target_link_libraries(common PUBLIC user32)

# ========== 守护进程 ==========
add_executable(mouse_focus_daemon
    src/daemon/daemon_main.cpp
    src/daemon/application.cpp
    src/daemon/focus_monitor.cpp
    src/daemon/key_tracker.cpp
    src/daemon/trigger_detector.cpp
    src/daemon/mouse_controller.cpp
)

target_link_libraries(mouse_focus_daemon PRIVATE common user32 kernel32 shell32)

# ========== GUI 配置工具 ==========
add_executable(mouse_focus_config WIN32
    src/config_tool/config_main.cpp
    src/config_tool/config_dialog.cpp
    src/config_tool/daemon_controller.cpp
)

target_link_libraries(mouse_focus_config PRIVATE common user32 kernel32 shell32 comctl32)

# 嵌入 Windows XP+ 视觉样式清单
set_target_properties(mouse_focus_config PROPERTIES
    WIN32_EXECUTABLE TRUE
)
```

### 使用方式

```
# === 守护进程 ===
# 直接启动（读取同目录 config.json，无窗口后台运行）
mouse_focus_daemon.exe

# 指定配置文件
mouse_focus_daemon.exe --config "D:\my_config.json"

# === GUI 配置工具 ===
# 打开设置界面
mouse_focus_config.exe

# 界面操作：
#   1. 修改各项配置
#   2. 点击 [保存设置] 写入 config.json（守护进程自动热重载）
#   3. 点击 [启动] / [停止] 控制守护进程
```

**两程序协作流程**：

```
用户打开 mouse_focus_config.exe
  → 修改设置 → 点 [保存] → 写入 config.json
  → 守护进程通过 ReadDirectoryChangesW 检测到变更
  → 自动热重载新配置，无需重启

用户点 [启动]
  → CreateProcess("mouse_focus_daemon.exe")
  → 守护进程启动，创建互斥体
  → 配置工具检测到互斥体，显示"运行中"
```

**单实例控制**：守护进程通过命名互斥体 `Global\MouseFocusScript_Instance` 保证只有一个实例在运行。

---

> **评审检查清单**
> - [ ] 双可执行文件架构是否合理？
> - [ ] GUI 布局是否覆盖所有配置项？
> - [ ] ALT+Tab 检测逻辑是否覆盖边界情况？
> - [ ] 多显示器 DPI 混合场景是否正确？
> - [ ] 守护进程热重载配置是否可靠？
> - [ ] GUI 启停守护进程是否正确？
> - [ ] 扩展点设计是否充足？
> - [ ] 是否有遗漏的功能需求？
