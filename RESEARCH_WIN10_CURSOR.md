# Win10 鼠标个性化设置——技术调研

> 日期：2026-05-17

---

## 一、核心发现

### `SetSystemCursor` 在 Win10 上需要管理员权限

项目当前使用 `SetSystemCursor(hNew, OCR_NORMAL)` 来替换系统光标。但经调研确认：

> **`SetSystemCursor` 在 Windows 10/11 上需要进程以管理员身份运行（`requireAdministrator`），否则始终返回失败。**

当前 daemon 的 manifest 使用 `asInvoker`（普通用户权限），因此 `SetSystemCursor` **从未成功执行过**。这就是所有光标个性化功能完全不生效的根因。

### 来源
- [SetSystemCursor MSDN](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setsystemcursor)：系统级 API，修改全局光标资源
- 多个 Q&A 确认 UAC 下需要提权，否则返回 `ERROR_ACCESS_DENIED`

---

## 二、Win10 正确的光标自定义方案

### 方案对比

| 方案 | 权限要求 | 优点 | 缺点 |
|------|:---:|------|------|
| `SetSystemCursor` + 管理员权限 | 管理员 | 代码简单 | 每次启动弹 UAC，安全风险 |
| 注册表 `HKCU\Control Panel\Cursors` + `SPI_SETCURSORS` | 普通用户 ✅ | 无需提权，标准做法 | 需要写 .cur 文件到磁盘 |
| `SetCursor` (per-thread) | 普通用户 | 最简单 | 仅对自身窗口有效，daemon 无窗口不适用 |

### 推荐方案：注册表 + `SystemParametersInfo`

Windows 的光标方案是通过注册表管理的：

```
HKCU\Control Panel\Cursors
  ├─ Arrow    = "C:\Windows\Cursors\aero_arrow.cur"    ← 标准箭头
  ├─ Hand     = "C:\Windows\Cursors\aero_link.cur"      ← 手型
  ├─ IBeam    = "C:\Windows\Cursors\beam_r.cur"         ← 文本选择
  ├─ Crosshair = "C:\Windows\Cursors\cross_r.cur"       ← 精确选择
  ├─ Wait     = "C:\Windows\Cursors\aero_busy.cur"      ← 忙碌
  └─ Move     = "C:\Windows\Cursors\move_r.cur"         ← 移动
```

修改后调用 API 刷新：
```cpp
SystemParametersInfoW(SPI_SETCURSORS, 0, 0, SPIF_SENDCHANGE);
```

---

## 三、实现方案

### 流程

```
触发移动后：
  1. 保存 HKCU\...\Cursors\Arrow 原始值
  2a. 系统光标形状 → 读取目标形状的注册表路径 → 写入 Arrow
  2b. 自定义彩色形状 → 生成临时 .cur 文件 → 路径写入 Arrow
  3. SystemParametersInfoW(SPI_SETCURSORS, 0, 0, SPIF_SENDCHANGE)

鼠标移动后恢复：
  4. 将 Arrow 恢复为原始值
  5. SystemParametersInfoW(SPI_SETCURSORS, 0, 0, SPIF_SENDCHANGE)
```

### 需要实现的功能

| 组件 | 功能 |
|------|------|
| 注册表读写 | 保存/恢复 `HKCU\Control Panel\Cursors\Arrow` 值 |
| 系统光标映射 | `arrow→Arrow, hand→Hand, ibeam→IBeam, cross→Crosshair, wait→Wait, sizeall→Move` |
| `.cur` 文件生成 | 将自定义 DIB 光标数据写入有效的 .cur 格式文件 |
| `SPI_SETCURSORS` 调用 | 通知系统刷新光标方案 |
| 临时文件管理 | 在 `%TEMP%` 下创建 / 删除临时 .cur 文件 |

### 注册表键映射

| 形状 key | 注册表值名 | 说明 |
|----------|-----------|------|
| `arrow` | `Arrow` | 标准选择 |
| `hand` | `Hand` | 链接选择 |
| `ibeam` | `IBeam` | 文本选择 |
| `cross` | `Crosshair` | 精确选择 |
| `sizeall` | `Move` | 移动 |
| `wait` | `Wait` | 忙碌 |
| `circle` | `Arrow` (临时文件路径) | 自定义圆形 |
| `square` | `Arrow` (临时文件路径) | 自定义方形 |

---

## 四、需要确认的问题

1. **是否接受临时 .cur 文件方案？** 会在 `%TEMP%\MouseFocus\` 下生成临时光标文件（几KB），关闭 daemon 时自动清理
2. **系统光标大小问题**：注册表方式使用系统光标文件，其大小由光标文件自身决定，用户设置的"大小"参数无法影响系统光标（仅影响自定义彩色形状）
3. **是否需要保留原有 `SetSystemCursor` 作为备选？** 可在管理员权限下使用更简单的方式

---

> **结论**：弃用 `SetSystemCursor`，改用注册表 + `SystemParametersInfo(SPI_SETCURSORS)` 方案。无需管理员权限，Win10 原生兼容。
