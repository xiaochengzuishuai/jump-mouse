# 窗口聚焦鼠标自动移动 — 设计文档

> 版本：仅跳转V1.0（稳定版） | 日期：2026-05-18 | 状态：✅ 稳定版第一版

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
| `MouseController` | 计算窗口中心 + 移动光标（instant/smooth） | daemon only |
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
2. 忽略 Alt+Tab 切换器自身窗口（类名 `TaskSwitcherWnd`、`TopLevelWindowForOverflowXamlIsland`）
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
- 支持命令行指定：`mouse_focus_daemon.exe --config "D:\path\config.json"`
- 支持运行时热重载（轮询文件修改时间）

### 4.2 Schema

```jsonc
{
  "version": 1,

  // 触发模式
  // "alt_tab_only"       — 仅 Alt+Tab 切换（含 Shift 反向）
  // "any_focus_change"   — 任何窗口聚焦变化均触发
  "triggerMode": "alt_tab_only",

  // 鼠标移动模式
  // "instant" — 瞬间移动
  // "smooth"  — 平滑动画
  "mouseMode": "smooth",

  // 平滑动画持续时间（毫秒），仅 mouseMode=smooth 时生效（50-600）
  "smoothDurationMs": 600,

  // Alt+Tab 后等待恢复中的最小化窗口的延迟（毫秒，0-2000）
  "moveDelayMs": 100,

  // 是否启用
  "enabled": true,

  // 目标区域
  // "window_rect" — 窗口完整矩形（含标题栏）的中心
  // "client_rect" — 客户区中心（不含标题栏/边框）
  "targetArea": "window_rect",

  // 排除的进程名（不含路径），不区分大小写
  "excludedProcesses": [],

  // 排除的窗口类名片段（部分匹配）
  "excludedClasses": [],

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

---

## 6. 文件结构

```
mouse_focus/
├── CMakeLists.txt
├── config.json
├── DESIGN_DOC.md
│
├── src/
│   ├── common/
│   │   ├── config_manager.h/cpp     # JSON 配置读写/验证
│   │   ├── adaptation_layer.h/cpp   # Win 版本检测、DPI、窗口过滤
│   │   ├── json_util.h              # 自研最小 JSON 解析器
│   │   └── logger.h                 # 轻量日志宏
│   │
│   ├── daemon/
│   │   ├── daemon_main.cpp          # 入口、命令行解析
│   │   ├── application.h/cpp        # 生命周期管理，模块组装
│   │   ├── focus_monitor.h/cpp      # SetWinEventHook 封装
│   │   ├── key_tracker.h/cpp        # 低级键盘钩子 (WH_KEYBOARD_LL)
│   │   ├── trigger_detector.h/cpp   # 触发逻辑核心
│   │   └── mouse_controller.h/cpp   # 光标移动（instant + smooth）
│   │
│   └── config_tool/
│       ├── config_main.cpp          # WinMain 入口
│       ├── config_dialog.h/cpp      # 主设置对话框（Win32 Dialog）
│       └── daemon_controller.h/cpp  # 守护进程管理（启/停/状态）
│
├── ico图标/
│   ├── 1.ico                        # 配置工具托盘图标
│   └── exe.ico                      # 守护进程图标
│
└── src/
    ├── mouse_focus.rc               # 配置工具资源脚本
    └── daemon.rc                    # 守护进程资源脚本
```

**依赖**：
- 标准库：C++20（`<filesystem>`、`<chrono>`、`<format>`）
- 系统：Windows SDK（`user32`、`kernel32`、`shell32`、`comctl32`）
- 零第三方依赖

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
│  │  时长/延迟 ms：[ 600 ] [ 100 ]                        │ │
│  │                                                        │ │
│  │  目标区域：                                            │ │
│  │    ◉ 窗口中心（含标题栏）  ○ 客户区中心                │ │
│  │                                                        │ │
│  │  ☑ 启用服务                                            │ │
│  │                                                        │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
│  ┌─ 排除规则 ────────────────────────────────────────────┐ │
│  │  进程名列表（如 devenv.exe）：                          │ │
│  │  ┌───────────────────────┐  [添加]                     │ │
│  │  │                       │  [移除]                     │ │
│  │  └───────────────────────┘                             │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
│  ┌─ 守护进程状态 ────────────────────────────────────────┐ │
│  │  状态：● 运行中         [启动] [停止] [开机自启]       │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
│  ┌─ 日志 ────────────────────────────────────────────────┐ │
│  │  日志级别：[无 ▼]                                      │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
│                    [保存设置]   [关闭]                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 控件映射

| 配置项 | 控件类型 | 控件 ID | 说明 |
|--------|---------|---------|------|
| `triggerMode` | Radio Button 组 | `IDC_RADIO_ALT_TAB` / `IDC_RADIO_ANY` | 二选一 |
| `mouseMode` | Radio Button 组 | `IDC_RADIO_INSTANT` / `IDC_RADIO_SMOOTH` | 二选一 |
| `smoothDurationMs` | Edit + Spin | `IDC_EDIT_DURATION` + `IDC_SPIN_DURATION` | 范围 50-600 |
| `moveDelayMs` | Edit + Spin | `IDC_EDIT_DELAY` + `IDC_SPIN_DELAY` | 范围 0-2000 |
| `targetArea` | Radio Button 组 | `IDC_RADIO_WINDOW` / `IDC_RADIO_CLIENT` | 二选一 |
| `enabled` | CheckBox | `IDC_CHECK_ENABLED` | 勾选/取消 |
| `excludedProcesses` | ListBox + Add + Remove | `IDC_LIST_EXCLUDE` + `IDC_BTN_ADD` + `IDC_BTN_REMOVE` | 进程名列表 |
| `logLevel` | ComboBox | `IDC_COMBO_LOG` | 无/错误/警告/信息/调试 |
| 守护状态 | Static Text + Buttons | `IDC_STATUS` + `IDC_BTN_START` + `IDC_BTN_STOP` | 状态显示 |
| 保存 | Button | `IDOK` | 写 config.json |
| 关闭 | Button | `IDCANCEL` | 隐藏到托盘 |

---

## 8. 关键类接口

### 8.1 Application

```cpp
class Application {
public:
    int run(HINSTANCE hInstance, const std::wstring& configPath);

private:
    ConfigManager m_config;
    MouseController m_mouse;
    std::unique_ptr<FocusMonitor> m_focusMonitor;
    std::unique_ptr<KeyTracker> m_keyTracker;
    std::unique_ptr<TriggerDetector> m_triggerDetector;

    HWND m_hwnd = nullptr;
    void setupModules();
    void checkConfigReload();
    void executeMove(HWND target);
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};
```

### 8.2 MouseController

```cpp
struct WindowCenter { LONG x, y; };

class MouseController {
public:
    WindowCenter calculateCenter(HWND hwnd, bool clientArea) const;
    void moveInstant(HWND hwnd, bool clientArea);

    void setSmoothDuration(int ms);
    void startSmooth(HWND targetHwnd, bool clientArea);
    bool smoothTick();  // returns true when done

private:
    POINT m_animFrom, m_animTo;
    std::chrono::steady_clock::time_point m_animStart;
    int m_animDurationMs = 150;
    bool m_animActive = false;
    static double easeOutCubic(double t);
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
- 定时器（10ms）驱动每帧
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

---

## 11. 扩展点

未来可扩展的方向（代码结构已预留）：

1. **Win+Tab 支持** — 复用 Win 键追踪逻辑
2. **特定显示器绑定** — "仅当前显示器上的窗口才触发"
3. **窗口排除规则可视化编辑** — 运行时通过热键"排除当前窗口"
4. **动画风格自定义** — 支持多种缓动曲线
5. **多语言国际化** — GUI 界面中英切换

---

## 12. 安全与边界

| 场景 | 处理 |
|------|------|
| 安全桌面（UAC） | 低权限钩子不跨会话，自动失效 |
| 全屏游戏 | 可配置排除全屏进程 |
| 远程桌面 | 正常工作的显示器虚拟化下行为一致 |
| 多用户同时登录 | 每会话独立实例，句柄不跨会话 |
| 意外退出 | 钩子在进程退出时由 OS 清理 |
| 高 CPU 动画 | 平滑动画限制最长时间 600ms，超时强制结束 |

---

## 13. 构建与运行

### 构建

```bash
cmake -B build -S .
cmake --build build --config Release
```

生成文件：
- `build\Release\mouse_focus_daemon.exe`
- `build\Release\mouse_focus_config.exe`

### 使用方式

```
# 守护进程（后台运行，无窗口）
mouse_focus_daemon.exe
mouse_focus_daemon.exe --config "D:\my_config.json"

# GUI 配置工具
mouse_focus_config.exe
```

**两程序协作流程**：

```
用户打开 mouse_focus_config.exe
  → 修改设置 → 点 [保存] → 写入 config.json
  → 守护进程检测到文件变更 → 自动热重载新配置

用户点 [启动]
  → CreateProcess("mouse_focus_daemon.exe")
  → 守护进程启动，创建互斥体
  → 配置工具检测到互斥体，显示"运行中"
```

**单实例控制**：守护进程通过命名互斥体 `Global\MouseFocusScript_Instance` 保证只有一个实例在运行。

---

> **评审检查清单**
> - [x] 双可执行文件架构
> - [x] GUI 布局覆盖所有配置项
> - [x] ALT+Tab 检测逻辑覆盖边界情况
> - [x] 多显示器 DPI 混合场景
> - [x] 守护进程热重载配置
> - [x] GUI 启停守护进程
> - [x] 扩展点设计
