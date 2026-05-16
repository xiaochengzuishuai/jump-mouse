#include "config_dialog.h"
#include "daemon_controller.h"
#include "../common/logger.h"
#include "../daemon/mouse_controller.h"
#include <CommCtrl.h>
#include <commdlg.h>
#include <map>

static INT_PTR CALLBACK inputDlgProc(HWND, UINT, WPARAM, LPARAM);

// Canvas subclass: draws an HCURSOR stored as window property "CURSOR"
static LRESULT CALLBACK CanvasProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        // Background
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));

        HCURSOR hCur = (HCURSOR)GetPropW(hwnd, L"CURSOR");
        if (!hCur) hCur = LoadCursor(nullptr, IDC_ARROW);

        // Draw centered
        ICONINFO ii = {};
        if (GetIconInfo(hCur, &ii)) {
            int cx = GetSystemMetrics(SM_CXCURSOR) * 2;
            int cy = GetSystemMetrics(SM_CYCURSOR) * 2;
            if (cx < 48) cx = 48;
            int x = (rc.right - rc.left - cx) / 2;
            int y = (rc.bottom - rc.top - cy) / 2;
            DrawIconEx(hdc, x, y, hCur, cx, cy, 0, nullptr, DI_NORMAL);
            if (ii.hbmMask)  DeleteObject(ii.hbmMask);
            if (ii.hbmColor) DeleteObject(ii.hbmColor);
        }

        // Frame
        FrameRect(hdc, &rc, GetSysColorBrush(COLOR_GRAYTEXT));
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1; // Handled in WM_PAINT
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ===================== Constructor / Destructor =====================

ConfigDialog::ConfigDialog(HINSTANCE hInstance, const std::wstring& configPath)
    : m_hInstance(hInstance), m_configPath(configPath) {
    m_config.load(configPath);
    m_working = m_config.get();
}
ConfigDialog::~ConfigDialog() {
    if (m_hCurCurrent) DestroyCursor(m_hCurCurrent);
    if (m_hCurPreview) DestroyCursor(m_hCurPreview);
}

HWND ConfigDialog::create(HWND parent) {
    m_hwnd = CreateDialogParamW(m_hInstance, MAKEINTRESOURCEW(IDD_MAIN_DIALOG),
        parent, dlgProc, reinterpret_cast<LPARAM>(this));
    return m_hwnd;
}
void ConfigDialog::showWindow() { if (m_hwnd) { ShowWindow(m_hwnd, SW_SHOW); SetForegroundWindow(m_hwnd); } }
void ConfigDialog::hideWindow() { if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }

// ===================== Tray =====================

void ConfigDialog::createTrayIcon() {
    std::wstring iconPath = m_configPath;
    auto pos = iconPath.find_last_of(L"\\/");
    iconPath = (pos != std::wstring::npos) ? iconPath.substr(0, pos + 1) + L"1.ico" : L"1.ico";
    HICON hIcon = nullptr;
    if (GetFileAttributesW(iconPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        int cx = GetSystemMetrics(SM_CXSMICON), cy = GetSystemMetrics(SM_CYSMICON);
        hIcon = (HICON)LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, cx, cy, LR_LOADFROMFILE);
    }
    if (!hIcon) hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    m_nid = { sizeof(NOTIFYICONDATAW) }; m_nid.hWnd = m_hwnd; m_nid.uID = UID_TRAY;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON; m_nid.hIcon = hIcon;
    wcscpy_s(m_nid.szTip, L"Mouse Focus");
    Shell_NotifyIconW(NIM_ADD, &m_nid);
    if (hIcon) DestroyIcon(hIcon);
}
void ConfigDialog::updateTrayMenu() {
    if (m_trayMenu) DestroyMenu(m_trayMenu);
    m_trayMenu = CreatePopupMenu();
    AppendMenuW(m_trayMenu, MF_STRING, IDM_TRAY_OPEN, L"打开主界面");
    AppendMenuW(m_trayMenu, MF_SEPARATOR, 0, nullptr);
    DaemonController dc; bool running = dc.isRunning();
    AppendMenuW(m_trayMenu, MF_STRING, IDM_TRAY_TOGGLE,
        running ? L"服务状态：开（点击关闭）" : L"服务状态：关（点击开启）");
    AppendMenuW(m_trayMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_trayMenu, MF_STRING, IDM_TRAY_EXIT, L"退出软件");
}
void ConfigDialog::showTrayMenu() {
    updateTrayMenu(); POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(m_trayMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
}
void ConfigDialog::removeTrayIcon() { Shell_NotifyIconW(NIM_DELETE, &m_nid); if (m_trayMenu) DestroyMenu(m_trayMenu); }
void ConfigDialog::onToggleDaemon() { DaemonController dc; if (dc.isRunning()) { dc.stop(); Sleep(300); } else dc.start(); refreshStatus(); }
void ConfigDialog::onTrayMenu(WORD id) {
    switch (id) {
    case IDM_TRAY_OPEN:   showWindow(); break;
    case IDM_TRAY_TOGGLE: onToggleDaemon(); break;
    case IDM_TRAY_EXIT:   removeTrayIcon(); DestroyWindow(m_hwnd); PostQuitMessage(0); break;
    }
}

// ===================== Preview =====================

void ConfigDialog::updatePreviewCursor() {
    if (m_hCurPreview) { DestroyCursor(m_hCurPreview); m_hCurPreview = nullptr; }

    HWND hCombo = GetDlgItem(m_hwnd, IDC_COMBO_SHAPE);
    int si = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);

    const char* shapeKeys[] = { "circle", "square", "diamond", "arrow", "cross", "custom" };
    std::string shape = (si >= 0 && si < 6) ? shapeKeys[si] : "circle";

    BOOL translated; int size = (int)GetDlgItemInt(m_hwnd, IDC_EDIT_HIGHLIGHT_SIZE, &translated, FALSE);
    if (!translated || size < 24) size = 48;

    HWND hColorCombo = GetDlgItem(m_hwnd, IDC_COMBO_COLOR);
    int ci = (int)SendMessageW(hColorCombo, CB_GETCURSEL, 0, 0);
    COLORREF color = RGB(255, 255, 0); // yellow default
    if (ci >= 0) color = (COLORREF)(INT_PTR)SendMessageW(hColorCombo, CB_GETITEMDATA, ci, 0);
    // Ensure color is valid
    if (color == 0 || color == CLR_INVALID || color > 0x00FFFFFF) color = RGB(255, 255, 0);

    bool enabled = IsDlgButtonChecked(m_hwnd, IDC_CHECK_HIGHLIGHT) == BST_CHECKED;
    if (!enabled) {
        m_hCurPreview = CopyCursor(LoadCursor(nullptr, IDC_ARROW));
    } else if (shape == "custom") {
        if (!m_working.highlightCustomFile.empty()) {
            std::wstring wf(m_working.highlightCustomFile.begin(), m_working.highlightCustomFile.end());
            m_hCurPreview = (HCURSOR)LoadImageW(nullptr, wf.c_str(), IMAGE_CURSOR, size, size, LR_LOADFROMFILE);
        }
        if (!m_hCurPreview) m_hCurPreview = CopyCursor(LoadCursor(nullptr, IDC_ARROW));
    } else {
        m_hCurPreview = MouseController::createColorCursor(size, shape, color);
    }
    if (!m_hCurPreview) m_hCurPreview = CopyCursor(LoadCursor(nullptr, IDC_ARROW));

    // Update preview canvas property and repaint
    HWND hCanvas = GetDlgItem(m_hwnd, IDC_CANVAS_PREVIEW);
    if (hCanvas) {
        SetPropW(hCanvas, L"CURSOR", (HANDLE)m_hCurPreview);
        InvalidateRect(hCanvas, nullptr, TRUE);
        UpdateWindow(hCanvas);
    }
}

void ConfigDialog::onBrowseCustomCursor(HWND hwnd) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Cursor Files (*.cur)\0*.cur\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        int len = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
        if (len > 1) { std::string s(len-1, '\0'); WideCharToMultiByte(CP_UTF8, 0, file, -1, s.data(), len, nullptr, nullptr); m_working.highlightCustomFile = s; }
        updatePreviewCursor();
    }
}

// ===================== Dialog Proc =====================

INT_PTR CALLBACK ConfigDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ConfigDialog* self = (ConfigDialog*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_INITDIALOG) {
        self = (ConfigDialog*)lParam;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = hwnd;
        self->onInit(hwnd);
        self->createTrayIcon();
        return TRUE;
    }
    if (!self) return FALSE;
    switch (msg) {
    case WM_COMMAND:
        self->onCommand(hwnd, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
        return TRUE;
    case WM_TIMER:
        if (wParam == 999) { KillTimer(hwnd, 999); self->refreshDaemonStatus(hwnd); }
        return TRUE;
    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_LBUTTONUP) IsWindowVisible(hwnd) ? self->hideWindow() : self->showWindow();
        else if (LOWORD(lParam) == WM_RBUTTONUP) self->showTrayMenu();
        return TRUE;
    case WM_CLOSE: self->hideWindow(); return TRUE;
    case WM_DESTROY: self->removeTrayIcon(); PostQuitMessage(0); return TRUE;
    }
    return FALSE;
}

void ConfigDialog::refreshStatus() { refreshDaemonStatus(m_hwnd); }

// ===================== Init =====================

void ConfigDialog::onInit(HWND hwnd) {
    const auto& cfg = m_working;

    // Subclass canvas controls for reliable painting
    HWND hCanvas1 = GetDlgItem(hwnd, IDC_CANVAS_CURRENT);
    HWND hCanvas2 = GetDlgItem(hwnd, IDC_CANVAS_PREVIEW);
    SetWindowLongPtrW(hCanvas1, GWLP_WNDPROC, (LONG_PTR)CanvasProc);
    SetWindowLongPtrW(hCanvas2, GWLP_WNDPROC, (LONG_PTR)CanvasProc);

    CheckRadioButton(hwnd, IDC_RADIO_ALT_TAB, IDC_RADIO_ANY,
        cfg.triggerMode == "any_focus_change" ? IDC_RADIO_ANY : IDC_RADIO_ALT_TAB);
    CheckRadioButton(hwnd, IDC_RADIO_INSTANT, IDC_RADIO_SMOOTH,
        cfg.mouseMode == "smooth" ? IDC_RADIO_SMOOTH : IDC_RADIO_INSTANT);
    SetDlgItemInt(hwnd, IDC_EDIT_DURATION, cfg.smoothDurationMs, FALSE);
    SendDlgItemMessageW(hwnd, IDC_SPIN_DURATION, UDM_SETRANGE32, 50, 600);
    EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DURATION), cfg.mouseMode == "smooth");
    EnableWindow(GetDlgItem(hwnd, IDC_SPIN_DURATION), cfg.mouseMode == "smooth");
    SetDlgItemInt(hwnd, IDC_EDIT_DELAY, cfg.moveDelayMs, FALSE);
    SendDlgItemMessageW(hwnd, IDC_SPIN_DELAY, UDM_SETRANGE32, 0, 2000);
    CheckRadioButton(hwnd, IDC_RADIO_WINDOW, IDC_RADIO_CLIENT,
        cfg.targetArea == "client_rect" ? IDC_RADIO_CLIENT : IDC_RADIO_WINDOW);
    CheckDlgButton(hwnd, IDC_CHECK_ENABLED, cfg.enabled ? BST_CHECKED : BST_UNCHECKED);

    // Highlight
    CheckDlgButton(hwnd, IDC_CHECK_HIGHLIGHT, cfg.highlightEnabled ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hwnd, IDC_EDIT_HIGHLIGHT_SIZE, cfg.highlightSize, FALSE);
    SendDlgItemMessageW(hwnd, IDC_SPIN_HIGHLIGHT_SIZE, UDM_SETRANGE32, 24, 128);

    // Shape combo
    HWND shapeCombo = GetDlgItem(hwnd, IDC_COMBO_SHAPE);
    const wchar_t* shapeNames[] = { L"圆形", L"方形", L"菱形", L"箭头", L"十字", L"自定义文件" };
    const char*    shapeKeys[]  = { "circle", "square", "diamond", "arrow", "cross", "custom" };
    for (int i = 0; i < 6; ++i) {
        int idx = (int)SendMessageW(shapeCombo, CB_ADDSTRING, 0, (LPARAM)shapeNames[i]);
        SendMessageW(shapeCombo, CB_SETITEMDATA, idx, i);
    }
    int shapeSel = 0;
    for (int i = 0; i < 6; ++i) { if (cfg.highlightShape == shapeKeys[i]) { shapeSel = i; break; } }
    SendMessageW(shapeCombo, CB_SETCURSEL, shapeSel, 0);

    // Color combo
    HWND colorCombo = GetDlgItem(hwnd, IDC_COMBO_COLOR);
    const wchar_t* colorNames[] = { L"黄色", L"红色", L"绿色", L"蓝色", L"洋红", L"青色", L"白色" };
    COLORREF colorVals[] = { RGB(255,255,0), RGB(255,0,0), RGB(0,255,0),
        RGB(0,128,255), RGB(255,0,255), RGB(0,255,255), RGB(255,255,255) };
    for (int i = 0; i < 7; ++i) {
        int idx = (int)SendMessageW(colorCombo, CB_ADDSTRING, 0, (LPARAM)colorNames[i]);
        SendMessageW(colorCombo, CB_SETITEMDATA, idx, (LPARAM)colorVals[i]);
    }
    int colorSel = 0;
    for (int i = 0; i < 7; ++i) { if (cfg.highlightColor == (int)colorVals[i]) { colorSel = i; break; } }
    SendMessageW(colorCombo, CB_SETCURSEL, colorSel, 0);

    // Excluded processes
    HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
    for (auto& p : cfg.excludedProcesses) {
        std::wstring w(p.begin(), p.end());
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str()));
    }

    // Log
    HWND combo = GetDlgItem(hwnd, IDC_COMBO_LOG);
    const wchar_t* logNames[] = { L"无", L"错误", L"警告", L"信息", L"调试" };
    for (int i = 0; i < 5; ++i) SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)logNames[i]);
    int sel = 0;
    if (cfg.logLevel == "error") sel = 1; else if (cfg.logLevel == "warn") sel = 2;
    else if (cfg.logLevel == "info") sel = 3; else if (cfg.logLevel == "debug") sel = 4;
    SendMessageW(combo, CB_SETCURSEL, sel, 0);

    DaemonController dc;
    CheckDlgButton(hwnd, IDC_CHECK_AUTOSTART, dc.isAutoStart() ? BST_CHECKED : BST_UNCHECKED);
    refreshDaemonStatus(hwnd);

    // Canvas: capture current cursor and initial preview
    if (m_hCurCurrent) DestroyCursor(m_hCurCurrent);
    m_hCurCurrent = CopyCursor(LoadCursor(nullptr, IDC_ARROW));
    SetPropW(hCanvas1, L"CURSOR", (HANDLE)m_hCurCurrent);

    updatePreviewCursor();
    // Ensure preview canvas property set
    HWND hPrev = GetDlgItem(hwnd, IDC_CANVAS_PREVIEW);
    if (hPrev && m_hCurPreview) SetPropW(hPrev, L"CURSOR", (HANDLE)m_hCurPreview);
}

void ConfigDialog::refreshDaemonStatus(HWND hwnd) {
    DaemonController dc; bool running = dc.isRunning();
    SetDlgItemTextW(hwnd, IDC_STATUS, running ? L"● 运行中" : L"○ 已停止");
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_START), !running);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_STOP), running);
}

// ===================== Collect & Save =====================

void ConfigDialog::collectValues(HWND hwnd) {
    BOOL translated;
    m_working.triggerMode = IsDlgButtonChecked(hwnd, IDC_RADIO_ANY) == BST_CHECKED ? "any_focus_change" : "alt_tab_only";
    m_working.mouseMode = IsDlgButtonChecked(hwnd, IDC_RADIO_SMOOTH) == BST_CHECKED ? "smooth" : "instant";
    int dur = GetDlgItemInt(hwnd, IDC_EDIT_DURATION, &translated, FALSE);
    if (translated) { if (dur < 50) dur = 50; if (dur > 600) dur = 600; m_working.smoothDurationMs = dur; }
    int delay = GetDlgItemInt(hwnd, IDC_EDIT_DELAY, &translated, FALSE);
    if (translated) { if (delay < 0) delay = 0; if (delay > 2000) delay = 2000; m_working.moveDelayMs = delay; }
    m_working.targetArea = IsDlgButtonChecked(hwnd, IDC_RADIO_CLIENT) == BST_CHECKED ? "client_rect" : "window_rect";
    m_working.enabled = IsDlgButtonChecked(hwnd, IDC_CHECK_ENABLED) == BST_CHECKED;
    m_working.highlightEnabled = IsDlgButtonChecked(hwnd, IDC_CHECK_HIGHLIGHT) == BST_CHECKED;
    int hlSize = GetDlgItemInt(hwnd, IDC_EDIT_HIGHLIGHT_SIZE, &translated, FALSE);
    if (translated) { if (hlSize < 24) hlSize = 24; if (hlSize > 128) hlSize = 128; m_working.highlightSize = hlSize; }

    HWND shapeCombo = GetDlgItem(hwnd, IDC_COMBO_SHAPE);
    int si = (int)SendMessageW(shapeCombo, CB_GETCURSEL, 0, 0);
    const char* shapeKeys[] = { "circle", "square", "diamond", "arrow", "cross", "custom" };
    m_working.highlightShape = (si >= 0 && si < 6) ? shapeKeys[si] : "circle";

    HWND colorCombo = GetDlgItem(hwnd, IDC_COMBO_COLOR);
    int ci = (int)SendMessageW(colorCombo, CB_GETCURSEL, 0, 0);
    m_working.highlightColor = (ci >= 0) ? (int)(COLORREF)(INT_PTR)SendMessageW(colorCombo, CB_GETITEMDATA, ci, 0) : 0x0000FFFF;

    m_working.excludedProcesses.clear();
    HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
    int count = (int)SendMessageW(list, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; ++i) {
        int len = (int)SendMessageW(list, LB_GETTEXTLEN, i, 0);
        if (len <= 0) continue;
        std::wstring w(len + 1, L'\0');
        SendMessageW(list, LB_GETTEXT, i, reinterpret_cast<LPARAM>(w.data())); w.resize(len);
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Len > 1) { std::string utf8(utf8Len-1, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, utf8.data(), utf8Len, nullptr, nullptr); m_working.excludedProcesses.push_back(utf8); }
    }
    HWND combo = GetDlgItem(hwnd, IDC_COMBO_LOG);
    int ls = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    const char* levels[] = { "none", "error", "warn", "info", "debug" };
    m_working.logLevel = (ls >= 0 && ls < 5) ? levels[ls] : "none";
}

void ConfigDialog::onSave(HWND hwnd) {
    collectValues(hwnd); m_config.apply(m_working);
    if (m_config.save(m_configPath)) {
        DaemonController dc; dc.setAutoStart(IsDlgButtonChecked(hwnd, IDC_CHECK_AUTOSTART) == BST_CHECKED);
        refreshDaemonStatus(hwnd); SetDlgItemTextW(hwnd, IDC_STATUS, L"配置已保存"); SetTimer(hwnd, 999, 2000, nullptr);
    } else {
        MessageBoxW(hwnd, L"保存配置文件失败，请检查文件权限。", L"错误", MB_OK | MB_ICONERROR);
    }
}

// ===================== Commands =====================

void ConfigDialog::onCommand(HWND hwnd, WORD id, WORD code, HWND /*ctl*/) {
    switch (id) {
    case IDOK: onSave(hwnd); break;
    case IDCANCEL: hideWindow(); break;
    case IDC_RADIO_SMOOTH:
        EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DURATION), TRUE);
        EnableWindow(GetDlgItem(hwnd, IDC_SPIN_DURATION), TRUE); break;
    case IDC_RADIO_INSTANT:
        EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DURATION), FALSE);
        EnableWindow(GetDlgItem(hwnd, IDC_SPIN_DURATION), FALSE); break;
    case IDC_BTN_ADD:    onAddExclude(hwnd);    break;
    case IDC_BTN_REMOVE: onRemoveExclude(hwnd); break;
    case IDM_TRAY_OPEN: case IDM_TRAY_TOGGLE: case IDM_TRAY_EXIT: onTrayMenu(id); break;
    case IDC_BTN_START: { DaemonController dc; if (dc.start()) refreshDaemonStatus(hwnd);
        else MessageBoxW(hwnd, L"无法启动守护进程。\n\n请确保 mouse_focus_daemon.exe 与本程序在同一目录。", L"启动失败", MB_OK | MB_ICONWARNING); break; }
    case IDC_BTN_STOP: { DaemonController dc; dc.stop(); Sleep(300); refreshDaemonStatus(hwnd); break; }
    case IDC_CHECK_HIGHLIGHT:
    case IDC_EDIT_HIGHLIGHT_SIZE:
        if (code == EN_CHANGE || id == IDC_CHECK_HIGHLIGHT) updatePreviewCursor(); break;
    case IDC_COMBO_SHAPE:
        if (code == CBN_SELCHANGE) updatePreviewCursor(); break;
    case IDC_COMBO_COLOR:
        if (code == CBN_SELCHANGE) updatePreviewCursor(); break;
    case IDC_BTN_BROWSE_CUR: onBrowseCustomCursor(hwnd); break;
    }
}

void ConfigDialog::onAddExclude(HWND hwnd) {
    wchar_t buf[256] = {};
    if (DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_INPUT_DIALOG), hwnd, inputDlgProc, (LPARAM)buf) == IDOK) {
        if (buf[0]) {
            HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
            int count = (int)SendMessageW(list, LB_GETCOUNT, 0, 0); bool dup = false;
            for (int i = 0; i < count; ++i) {
                wchar_t existing[256] = {}; SendMessageW(list, LB_GETTEXT, i, (LPARAM)existing);
                if (_wcsicmp(existing, buf) == 0) { dup = true; break; }
            }
            if (!dup) SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)buf);
        }
    }
}
void ConfigDialog::onRemoveExclude(HWND hwnd) {
    HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
    int sel = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) SendMessageW(list, LB_DELETESTRING, sel, 0);
}

static INT_PTR CALLBACK inputDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* outBuf = nullptr;
    if (msg == WM_INITDIALOG) { outBuf = (wchar_t*)lParam; SetFocus(GetDlgItem(hwnd, IDC_INPUT_TEXT)); return FALSE; }
    if (msg == WM_COMMAND) {
        if (LOWORD(wParam) == IDOK) { if (outBuf) GetDlgItemTextW(hwnd, IDC_INPUT_TEXT, outBuf, 255); EndDialog(hwnd, IDOK); return TRUE; }
        if (LOWORD(wParam) == IDCANCEL) { EndDialog(hwnd, IDCANCEL); return TRUE; }
    }
    return FALSE;
}
