# 移动后临时变更鼠标样式 — 全面代码审查

> 日期：2026-05-17 | 审查范围：v1.3 全模块

---

## 一、Win10 适配性检查

| 检查项 | 状态 | 说明 |
|--------|:---:|------|
| `SetSystemCursor` | ✅ | Win2K+ 原生支持，asInvoker 权限足够 |
| `CreateDIBSection` / `CreateIconIndirect` | ✅ | 32-bit BGRA DIB，Win10 原生支持 |
| `LoadCursor` + `CopyCursor` | ✅ | 系统光标复制，全版本兼容 |
| `LowLevelHooksTimeout` 注册表限制 | ⚠️ | Win10 对 WH_KEYBOARD_LL 有超时限制，但仅影响按键追踪，不影响光标功能 |
| PerMonitorV2 DPI | ✅ | 通过 manifest 声明，物理坐标一致 |
| UIPI (User Interface Privilege Isolation) | ✅ | asInvoker 级别，与用户进程同权限，无跨级别阻挡 |
| `IDC_HAND` 光标 | ✅ | `WINVER >= 0x0500`，Win10 SDK 默认支持 |
| 中文路径光标文件 | ❌ | UTF-8 ↔ wstring 转换仅支持 ASCII，中文路径会损坏（**Bug #1**） |

---

## 二、可视化画布预览检查

### 2.1 画布架构

| 组件 | 实现 | 状态 |
|------|------|:---:|
| 画布类注册 | `RegisterClassExW("CursorCanvas")` 在 `WinMain` 中 | ✅ |
| 画布绘制 | `CanvasWndProc::WM_PAINT` 获取 `GetPropW(L"CURSOR")` | ⚠️ |
| 预览刷新 | `InvalidateRect` + `UpdateWindow` | ⚠️ |


### 2.2 发现的问题

**Bug #2：画布属性设置的竞态窗口**
- 在 `updatePreviewCursor()` 中，先 `DestroyCursor(m_hCurPreview)` 再 `SetPropW(hCanvas, L"CURSOR", newCursor)`
- 两步之间存在极短的时间窗口：若其间触发 `WM_PAINT`，`GetPropW` 将返回已销毁的句柄
- **概率**：极低（同一函数内同步操作），但理论存在
- **修复**：交换顺序，先 SetProp 再 DestroyCursor 旧句柄

**Bug #3：画布实际未渲染**
- `InvalidateRect` 后缺少 `UpdateWindow` 调用（在 `onInit` 流程中虽可依赖系统后续 `WM_PAINT`，但 `updatePreviewCursor` 内缺少）
- **修复**：在 `updatePreviewCursor` 末尾添加 `UpdateWindow(hCanvas)`

---

## 三、实际临时变动与设置匹配检查

### 3.1 daemon 应用流程

```
executeMove → highlightEnabled?
  ├─ yes → setHighlightShape/Color/Size/CustomFile → applyHighlight()
  │          ├─ highlightCustomFile 非空？→ LoadImageW 加载文件
  │          └─ 否则 → createColorCursor(shape, color, size)
  │                  ├─ 系统光标 → LoadCursor + CopyCursor（忽略颜色/大小）
  │                  └─ 自定义彩色 → CreateDIBSection + CreateIconIndirect
  └─ no  → 跳过
```

### 3.2 发现的问题

**Bug #4（严重）：导入文件模式切换后旧路径未清除**
- 在 `collectValues()` 中，用户从"导入文件"切回"个性化配置"时，`highlightCustomFile` 未清空
- **后果**：daemon 看到非空 `highlightCustomFile`，优先使用旧文件路径，忽略新的形状/颜色配置
- **修复**：`collectValues` 中，若 `IDC_RADIO_CONFIG` 被选中，将 `highlightCustomFile` 置为空字符串

**Bug #5：系统光标形状下大小控件仍然启用**
- `updateHighlightControls` 中仅禁用了颜色按钮（`si >= 6`），但大小 spin 始终启用
- 对系统光标（arrow/hand/ibeam 等），大小设置无效（`LoadCursor` 返回系统默认尺寸）
- **影响**：用户调整大小但光标无变化，体验困惑
- **修复**：系统光标形状时禁用大小控件，或仅在自定义彩色形状时启用

**Bug #6：color 选择器对系统形状不可见但仍可点击**
- 颜色按钮仅在 `isCustomColored = (si >= 6)` 时启用，但按钮始终可见
- 用户可能不理解为何颜色按钮灰色不可用
- **建议**：系统光标形状时显示提示文本或隐藏颜色按钮

---

## 四、导入光标文件实际应用检查

### 4.1 流程

**UI 端**：
```
onBrowseCustomCursor → GetOpenFileNameW → WideCharToMultiByte(CP_UTF8)
→ 存储到 m_working.highlightCustomFile（UTF-8）
```

**daemon 端**：
```
applyHighlight → m_highlightCustomFile 非空
→ std::wstring wfile(m_highlightCustomFile.begin(), m_highlightCustomFile.end())
→ LoadImageW(wfile, IMAGE_CURSOR, size, size, LR_LOADFROMFILE)
```

### 4.2 发现的问题

**Bug #1（严重）：UTF-8 → wstring 转换断裂**
- UI 端正确使用 `WideCharToMultiByte(CP_UTF8)` 将路径编码为 UTF-8
- daemon 端使用 `std::wstring(charIter, charIter)` **逐字节转换**，仅对 0-127 ASCII 有效
- **中文路径**（如 `C:\用户\桌面\cursor.cur`）会被损坏，导致 `LoadImageW` 加载失败
- **修复**：daemon 端使用 `MultiByteToWideChar(CP_UTF8)` 还原

**Bug #7：LoadImageW 可能忽略请求的尺寸**
- `LR_LOADFROMFILE` 配合指定尺寸时，Windows 可能选择 .cur 文件内最优匹配的尺寸，忽略请求
- 这是 Windows API 的固有行为，非代码 bug
- **影响**：自定义光标文件可能以原始尺寸显示，而非用户指定的大小
- **缓解**：在 UI 标注"导入文件时大小可能由文件本身决定"

---

## 五、其他潜在兼容性问题

| 问题 | 级别 | 说明 |
|------|:---:|------|
| `SetSystemCursor` 后进程崩溃无法恢复原始光标 | 中 | 已通过析构函数 `forceRestoreCursor` 缓解，但异常退出无法保证 |
| 系统光标 `IDC_SIZEALL` 在 Win10 早期版本行为差异 | 低 | 1703 后一致性良好 |
| 多显示器下 `SetCursorPos` + 物理坐标一致性 | 低 | PerMonitorV2 声明下已验证 |

---

## 六、需要立即修复的 Bug 清单

| ID | 严重度 | 描述 | 文件 |
|----|:---:|------|------|
| #1 | 🔴 严重 | UTF-8 → wstring 中文路径损坏 | `mouse_controller.cpp:150` |
| #4 | 🔴 严重 | 切回配置模式后旧文件路径残留 | `config_dialog.cpp:collectValues` |
| #2 | 🟡 中等 | 画布属性设置竞态窗口 | `config_dialog.cpp:updatePreviewCursor` |
| #3 | 🟡 中等 | 预览缺少 UpdateWindow | `config_dialog.cpp:updatePreviewCursor` |
| #5 | 🟡 中等 | 系统光标形状下大小控件误导启用 | `config_dialog.cpp:updateHighlightControls` |
| #6 | 🟢 低 | 颜色按钮对系统形状不可用但可见 | 体验优化 |
| #7 | 🟢 低 | LoadImageW 可能忽略请求尺寸 | Windows API 限制 |

---

---

## 七、v1.3.1 追加修复

| ID | 修复 | 说明 |
|----|------|------|
| #8 | 形状下拉初始化加固 | 增加 `CB_RESETCONTENT` + `CB_INITSTORAGE(8,32)` + 错误回退（ASCII key 名） |
| #9 | 新增「刷新」按钮 | 重新从磁盘加载 `config.json` → 更新所有高亮控件 → 刷新双画布预览 → 状态栏显示"已刷新" |
| #10 | 刷新时恢复当前光标画布 | `InvalidateRect` + `UpdateWindow` 确保即时重绘 |

> **结论**：核心功能流程完整，Win10 适配正确。严重 bug #1/#4 已修复。形状下拉增加回退机制。新增刷新按钮用于诊断和强制重新加载。
