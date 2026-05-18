# Jump Mouse — 完整编译与环境配置指南

> 适用平台：Windows 10 / Windows 11  
> 编译器：MSVC (Visual Studio 2022)  
> C++ 标准：C++20  

---

## 目录

1. [前置知识](#1-前置知识)
2. [环境准备（一次性）](#2-环境准备一次性)
3. [方案 A：Visual Studio 2022 IDE（最简单）](#3-方案-avisual-studio-2022-ide)
4. [方案 B：VS Build Tools + CMake 命令行](#4-方案-bvs-build-tools--cmake-命令行)
5. [方案 C：VS Code + CMake 插件（推荐日常开发）](#5-方案-cvs-code--cmake-插件推荐日常开发)
6. [方案 D：VS Code + MSBuild（无 CMake）](#6-方案-dvs-code--msbuild无-cmake)
7. [方案 E：Windows Terminal + 开发者 PowerShell](#7-方案-ewindows-terminal--开发者-powershell)
8. [编译后产物](#8-编译后产物)
9. [常见问题排查](#9-常见问题排查)

---

## 1. 前置知识

本项目特点：

| 项目属性 | 说明 |
|---------|------|
| **语言标准** | C++20 |
| **编译器** | 仅支持 MSVC（使用了 Win32 API + .rc 资源文件） |
| **依赖** | 零外部库，仅需 Windows SDK |
| **产物** | 两个 .exe 文件（守护进程 + GUI 配置工具） |
| **构建系统** | CMake（跨 IDE），也支持直接用 MSBuild |

**不支持 MinGW / Clang on Windows**，因为资源文件 (.rc) 和部分 Windows API 需要 MSVC 工具链。

---

## 2. 环境准备（一次性）

无论选择哪种方案，以下组件**必须安装**：

### 2.1 安装 Visual Studio 2022 或 Build Tools

前往 [Visual Studio 下载页](https://visualstudio.microsoft.com/zh-hans/downloads/) 下载以下之一：

| 安装包 | 包含内容 | 适用场景 |
|--------|---------|---------|
| **Visual Studio 2022 Community**（免费） | IDE + 编译器 + CMake | 方案 A、C、D |
| **Visual Studio 2022 Build Tools**（免费，仅 3GB） | 编译器 + CMake（无 IDE） | 方案 B、E |

**安装时必须勾选的工作负荷**：

```
☑ 使用 C++ 的桌面开发
  └─ MSVC v143 - VS 2022 C++ x64/x86 生成工具
  └─ Windows 11 SDK (10.0.22621.0 或更高)
  └─ 用于 Windows 的 C++ CMake 工具         ← 方案 B/C/E 需要
```

### 2.2 验证安装

打开 **命令提示符** 或 **PowerShell**，运行：

```cmd
where cl.exe
:: 预期输出: C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.xx.xxxxx\bin\Hostx64\x64\cl.exe

where cmake.exe
:: 预期输出: C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

如果没有 `where` 输出，说明环境变量未配置，参考 [方案 E 第 2 步](#7-方案-ewindows-terminal--开发者-powershell)。

---

## 3. 方案 A：Visual Studio 2022 IDE（最简单）

> 适合：不熟悉命令行的用户，希望一键编译调试。

### 步骤 1：用 VS 打开 CMake 项目

1. 启动 **Visual Studio 2022**
2. 菜单栏 → **文件** → **打开** → **CMake...**
3. 选择项目根目录的 `CMakeLists.txt`
4. 点击 **打开**

VS 会自动执行 CMake 配置，等待输出窗口显示：

```
CMake generation finished.
```

### 步骤 2：选择配置

顶部工具栏：

```
解决方案配置: [Release]  ▼    架构: [x64]  ▼
```

下拉选择 **Release** + **x64**。

### 步骤 3：生成

- 菜单栏 → **生成** → **全部生成**
- 或快捷键 `Ctrl+Shift+B`

### 步骤 4：产物位置

```
项目根目录/out/build/x64-Release/
├── jump_mouse_daemon.exe
├── jump_mouse_config.exe
└── config.json
```

### 步骤 5：运行

在 VS 中直接运行：
- 顶部工具栏启动项下拉 → 选择 `jump_mouse_config.exe`
- 点击绿色 ▶ 按钮

---

## 4. 方案 B：VS Build Tools + CMake 命令行

> 适合：CI/CD、写脚本自动化、极简环境（无 VS IDE）。

### 步骤 1：打开 VS 开发者命令行

在开始菜单搜索并打开：

```
x64 Native Tools Command Prompt for VS 2022
```

或

```
Developer PowerShell for VS 2022
```

这一步会设置好 `cl.exe`、`cmake.exe`、`MSBuild.exe` 等环境变量。

### 步骤 2：cd 到项目目录

```cmd
cd /d E:\玩转ai\cc_project\鼠标聚焦脚本
```

### 步骤 3：CMake 配置

```cmd
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
```

参数说明：

| 参数 | 含义 |
|------|------|
| `-B build` | 生成文件输出到 `build/` 目录 |
| `-S .` | 源码目录为当前目录 |
| `-G "Visual Studio 17 2022"` | 使用 VS 2022 生成器 |
| `-A x64` | 目标架构 64 位 |

### 步骤 4：编译

```cmd
cmake --build build --config Release
```

或使用并行编译加速：

```cmd
cmake --build build --config Release --parallel
```

### 步骤 5：产物位置

```
build/Release/
├── jump_mouse_daemon.exe
├── jump_mouse_config.exe
└── config.json
```

### 一键脚本

创建 `build.bat` 放在项目根目录：

```bat
@echo off
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
echo.
echo Build complete. Output: build\Release\
echo   jump_mouse_daemon.exe
echo   jump_mouse_config.exe
pause
```

双击运行即可。

---

## 5. 方案 C：VS Code + CMake 插件（推荐日常开发）

> 适合：日常写代码、调试、修改，体验最好的方案。

### 步骤 1：安装 VS Code 扩展

在 VS Code 中按 `Ctrl+Shift+X`，搜索并安装：

| 扩展名 | 发布者 | 必须 |
|--------|--------|:---:|
| **C/C++** | Microsoft | ✅ |
| **CMake Tools** | Microsoft | ✅ |
| **CMake** | twxs | 可选（语法高亮） |

### 步骤 2：用 VS Code 打开项目

```cmd
code "E:\玩转ai\cc_project\鼠标聚焦脚本"
```

或者在 VS Code 中：**文件** → **打开文件夹** → 选择项目根目录。

### 步骤 3：选择 Kit（工具链）

1. 按 `Ctrl+Shift+P`，输入 `CMake: Select a Kit`
2. 选择：

```
Visual Studio Community 2022 Release - amd64
```

如果没有出现 VS 2022 选项，说明 VS 2022 未安装或 CMake Tools 未检测到。确认安装了 **"使用 C++ 的桌面开发"** 工作负荷。

### 步骤 4：选择变体

点击底部状态栏的：

```
[CMake] [Debug] [x64]
```

将 Debug 切换为 **Release**（或保持 Debug 以方便调试）。

### 步骤 5：配置

按 `Ctrl+Shift+P` → `CMake: Configure`，或直接保存任意 CMakeLists.txt 触发自动配置。

输出面板应显示：

```
[cmake] Configuring done
[cmake] Generating done
```

### 步骤 6：编译

- 按 `F7` 或
- 点击底部状态栏的 **🔨 Build** 按钮

### 步骤 7：运行/调试

- 按 `Ctrl+Shift+P` → `CMake: Set Launch Target` → 选择 `jump_mouse_config.exe`
- 按 `Shift+F5` 运行（不含调试）
- 按 `Ctrl+F5` 调试运行

### VS Code 完整配置（可选优化）

在项目根目录创建 `.vscode/settings.json`：

```json
{
  "cmake.configureOnOpen": true,
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "cmake.generator": "Visual Studio 17 2022",
  "cmake.preferredGenerators": ["Visual Studio 17 2022"],
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

创建 `.vscode/launch.json` 以便 F5 调试 GUI 配置工具：

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Config Tool (Debug)",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/Release/jump_mouse_config.exe",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "console": "internalConsole"
    },
    {
      "name": "Daemon (Debug)",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/Release/jump_mouse_daemon.exe",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "console": "internalConsole"
    }
  ]
}
```

---

## 6. 方案 D：VS Code + MSBuild（无 CMake）

> 适合：不想装 CMake 插件，直接用 VS 底层工具链。

### 步骤 1：手动生成 .vcxproj 文件

在 **x64 Native Tools Command Prompt for VS 2022** 中：

```cmd
cd /d "E:\玩转ai\cc_project\鼠标聚焦脚本"

cmake -B build_vs -S . -G "Visual Studio 17 2022" -A x64
```

成功后会在 `build_vs/` 下生成 `.sln` 和 `.vcxproj` 文件。这个步骤只需执行一次。

### 步骤 2：在 VS Code 中配置 tasks.json

在项目根目录创建 `.vscode/tasks.json`：

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "MSBuild (Release)",
      "type": "shell",
      "command": "MSBuild",
      "args": [
        "${workspaceFolder}\\build_vs\\jump_mouse.sln",
        "/p:Configuration=Release",
        "/p:Platform=x64",
        "/m",
        "/v:minimal"
      ],
      "group": {
        "kind": "build",
        "isDefault": true
      },
      "problemMatcher": "$msCompile",
      "detail": "编译全部目标 (Release x64)"
    },
    {
      "label": "MSBuild (Debug)",
      "type": "shell",
      "command": "MSBuild",
      "args": [
        "${workspaceFolder}\\build_vs\\jump_mouse.sln",
        "/p:Configuration=Debug",
        "/p:Platform=x64",
        "/m"
      ],
      "problemMatcher": "$msCompile"
    }
  ]
}
```

### 步骤 3：编译

- 按 `Ctrl+Shift+B` 运行默认构建任务
- 或 `Ctrl+Shift+P` → `Tasks: Run Task` → 选择任务

### 注意事项

此方案必须在 **VS Code 内通过 Developer PowerShell 启动**，否则 `MSBuild` 命令不可用：

```cmd
code "E:\玩转ai\cc_project\鼠标聚焦脚本"
```

VS Code 会继承终端的 PATH 环境变量。

---

## 7. 方案 E：Windows Terminal + 开发者 PowerShell

> 适合：喜欢纯命令行、写自动化脚本的用户。

### 步骤 1：安装 Windows Terminal（推荐）

从 Microsoft Store 搜索安装 **Windows Terminal**（免费）。

### 步骤 2：配置开发者 PowerShell Profile

打开 Windows Terminal → 设置 → 添加新配置文件，命令行为：

```powershell
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1"
```

参数 `-Arch amd64 -HostArch amd64`。

或者每次手动初始化：

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
```

> ⚠️ 路径中 `Community` 部分取决于你安装的 VS 版本：
> - Community → `Community`
> - Professional → `Professional`
> - Enterprise → `Enterprise`
> - Build Tools → `BuildTools`

### 步骤 3：一键编译脚本

创建 `build.ps1`：

```powershell
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "=== Configuring CMake ===" -ForegroundColor Cyan
cmake -B "$ProjectDir/build" -S $ProjectDir -G "Visual Studio 17 2022" -A x64

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "=== Building ($Configuration) ===" -ForegroundColor Cyan
cmake --build "$ProjectDir/build" --config $Configuration --parallel

if ($LASTEXITCODE -eq 0) {
    Write-Host "=== Build Succeeded ===" -ForegroundColor Green
    Write-Host "Output: $ProjectDir\build\$Configuration\" -ForegroundColor Yellow
    Get-ChildItem "$ProjectDir\build\$Configuration\*.exe" | ForEach-Object {
        Write-Host "  $_" -ForegroundColor White
    }
} else {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}
```

运行：

```powershell
# 在开发者 PowerShell 中
cd "E:\玩转ai\cc_project\鼠标聚焦脚本"
.\build.ps1

# 或指定 Debug
.\build.ps1 -Configuration Debug
```

---

## 8. 编译后产物

编译成功后，产物结构：

```
build/Release/
├── jump_mouse_daemon.exe     # 后台守护进程（无窗口）
├── jump_mouse_config.exe     # GUI 配置工具
└── config.json               # 默认配置文件（从项目根复制）
```

### 运行方式

```cmd
# 1. 打开 GUI 配置界面
jump_mouse_config.exe

# 2. 在界面上点"启动"启动守护进程

# 或者直接启动守护进程（使用默认配置）
jump_mouse_daemon.exe
```

### 发布打包

将以下文件复制到同一目录即为完整发布包：

```
AnyFolder/
├── jump_mouse_daemon.exe
├── jump_mouse_config.exe
└── config.json
```

无需任何运行时 DLL（MSVC 静态链接 Windows SDK）。

---

## 9. 常见问题排查

### Q1：cmake 配置时报 "No CMAKE_CXX_COMPILER could be found"

**原因**：未安装 VS 2022 或未安装 C++ 工作负荷。

**解决**：
1. 打开 Visual Studio Installer
2. 确认勾选了 **"使用 C++ 的桌面开发"**
3. 在"单个组件"中确认有 `MSVC v143` 和 `Windows 11 SDK`

### Q2：VS Code 中 CMake Tools 找不到 Kit

**原因**：CMake Tools 未检测到 VS 2022。

**解决**：
1. 确保 VS 2022 安装了 **"用于 Windows 的 C++ CMake 工具"** 组件
2. 重启 VS Code
3. 如果仍不行，在 VS Code settings.json 中手动指定：

```json
{
  "cmake.configureSettings": {
    "CMAKE_GENERATOR": "Visual Studio 17 2022"
  }
}
```

### Q3：编译时报 "fatal error C1083: 无法打开包括文件: 'Windows.h'"

**原因**：Windows SDK 未安装或版本不匹配。

**解决**：
1. 打开 Visual Studio Installer → 修改
2. 确保勾选了 `Windows 11 SDK`（或 `Windows 10 SDK 10.0.19041+`）
3. 在 CMake 配置时指定 SDK 版本：

```cmd
cmake -B build -S . -DCMAKE_SYSTEM_VERSION=10.0.22621.0
```

### Q4：编译时报 resource.h 或 .rc 相关错误

**原因**：资源编译器 (rc.exe) 未正确配置。

**解决**：确保使用的是 **x64 Native Tools Command Prompt** 或 **Visual Studio 17 2022** 生成器。

### Q5：运行守护进程时报 "Another instance is already running"

**原因**：已经有一个守护进程在运行。

**解决**：
```cmd
# 方法1：通过 GUI 工具点"停止"
jump_mouse_config.exe

# 方法2：任务管理器手动结束
taskkill /f /im jump_mouse_daemon.exe
```

### Q6：鼠标移动行为不符合预期

**排查步骤**：
1. 将 `config.json` 中的 `logLevel` 改为 `"debug"`
2. 指定 `logFile` 路径（如 `"C:\\jump_mouse.log"`）
3. 重启守护进程
4. Alt+Tab 切换几次后查看日志文件

### Q7：开机自启不生效

**原因**：守护进程路径变更或注册表权限问题。

**解决**：
1. 确保守护进程 exe 没有被移动
2. 在 GUI 工具中取消勾选"开机自启"→ 保存 → 重新勾选 → 保存
3. 或手动检查注册表：

```cmd
reg query HKCU\Software\Microsoft\Windows\CurrentVersion\Run /v MouseFocusDaemon
```

### Q8：DPI 多显示器场景鼠标位置偏移

**原因**：DPI Awareness 未被正确声明。

**解决**：
1. 确保 `jump_mouse.manifest` 文件与 exe 在同一目录
2. 或使用 PropertySheet 嵌入清单（已通过 .rc 文件处理，确保编译时 .rc 被包含）
3. 验证：右键 .exe → 属性 → 兼容性 → 更改高 DPI 设置 → 查看是否显示"由应用程序控制"

---

## 快速参考卡片

```
┌─────────────────────────────────────────────────────────┐
│  最小化编译流程（只需 3 步）                              │
│                                                         │
│  1. 安装 VS 2022 Community + "使用C++的桌面开发"        │
│                                                         │
│  2. 打开 "x64 Native Tools Command Prompt for VS 2022"  │
│                                                         │
│  3. cd 到项目目录，执行:                                 │
│     cmake -B build -S .                                 │
│     cmake --build build --config Release                │
│                                                         │
│  产物在 build\Release\                                  │
└─────────────────────────────────────────────────────────┘
```

---

> 文档版本：v1.0 | 最后更新：2026-05-16
