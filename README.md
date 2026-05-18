# Jump Mouse — 仅跳转

**Alt+Tab 切换窗口时，鼠标自动跳转到新窗口中心。**

[English](./docs/English_Manual.md)

---

## 功能

- 检测 Alt+Tab 窗口切换，自动将鼠标移动到新聚焦窗口的中心
- 支持**瞬间移动**与**平滑动画**两种模式
- 可配置的**延迟等待**（适配最小化窗口恢复动画）
- GUI 配置工具 + 系统托盘，无需手动编辑配置文件
- 配置热重载，修改即生效
- 零外部依赖，Windows 10/11 原生运行

## 快速开始

1. 从 [Releases](../../releases) 下载最新版本
2. 解压到任意目录，所有文件放在一起
3. 运行 `jump_mouse_config.exe`
4. 点击「**启动**」开启服务
5. 使用 Alt+Tab 切换窗口，鼠标自动跟随

## 包含文件

| 文件 | 说明 |
|------|------|
| `jump_mouse_daemon.exe` | 后台守护进程，监听窗口切换并移动鼠标 |
| `jump_mouse_config.exe` | GUI 配置工具，可视化编辑所有设置 |
| `config.json` | 默认配置文件 |

## 配置项

| 配置 | 说明 | 默认值 |
|------|------|--------|
| 触发模式 | 仅 Alt+Tab / 任意聚焦变化 | 仅 Alt+Tab |
| 移动模式 | 瞬间 / 平滑动画 | 平滑 |
| 动画时长 | 平滑动画持续时间 | 600ms |
| 移动延迟 | 最小化窗口恢复等待时间 | 100ms |
| 目标区域 | 窗口中心 / 客户区中心 | 窗口中心 |
| 排除进程 | 不触发移动的进程列表 | 空 |
| 日志级别 | 无 / 错误 / 警告 / 信息 / 调试 | 无 |

## 系统要求

- Windows 10 1809+ / Windows 11
- 64 位 (x64)
- 无需安装任何运行时（静态链接 MSVC CRT）

## 从源码构建

```bash
cmake -B build -S . -A x64
cmake --build build --config Release
```

产物在 `build/release/` 目录。

详细构建指南见：[编译指南](./docs/编译指南.md)

## 文档

- [设计文档](docs/设计文档.md)
- [编译指南](docs/编译指南.md)
- [代码审查](docs/代码审查.md)
- [已知问题](docs/已知问题.md)
- [中文完整文档](docs/中文完整文档.md)
- [English Documentation](docs/English_Manual.md)

## 技术栈

- C++20
- Win32 API + MSVC
- 零第三方库，仅依赖 Windows SDK

## 许可

MIT License
