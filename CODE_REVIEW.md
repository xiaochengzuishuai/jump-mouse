# 代码审查 — v2.1

> 日期：2026-05-17 | 审查范围：核心功能

---

## 检查项

| 检查项 | 状态 | 说明 |
|--------|:---:|------|
| Alt+Tab 检测 | ✅ | 三阶段检测（Alt按下→记录候选→Alt释放移动） |
| 最小化窗口延迟恢复 | ✅ | `moveDelayMs` 延迟后重新获取窗口位置 |
| 平滑动画 | ✅ | cubic ease-out，10ms定时器驱动 |
| 瞬间移动 | ✅ | `SetCursorPos` 直接跳转 |
| 系统窗口过滤 | ✅ | TaskSwitcherWnd、Shell_TrayWnd、Progman 等 |
| UWP 窗口支持 | ✅ | `ApplicationFrameWindow` 包裹检测 |
| 配置热重载 | ✅ | 轮询文件修改时间，2秒间隔 |
| 单实例 | ✅ | 命名互斥体 `Global\MouseFocusScript_Instance` |
| 系统托盘 | ✅ | 右键菜单：打开/切换/退出 |
| DPI 适配 | ✅ | 嵌入 manifest 声明 PerMonitorV2 |
| Win10/Win11 兼容 | ✅ | 类名模糊匹配 |
| 零第三方依赖 | ✅ | 自研 json_util.h |

---

## 已知限制

| 问题 | 级别 | 说明 |
|------|:---:|------|
| `WH_KEYBOARD_LL` 超时限制 | 低 | Win10 对低级别钩子有超时限制，但仅影响按键追踪 |
| 全屏游戏 | 低 | 未自动检测全屏进程，需用户手动添加到排除列表 |

> **结论**：核心功能完整稳定，Win10/Win11 兼容良好。
