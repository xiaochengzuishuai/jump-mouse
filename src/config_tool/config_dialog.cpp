#include "config_dialog.h"
#include "daemon_controller.h"
#include "../common/logger.h"
#include "../daemon/mouse_controller.h"
#include <CommCtrl.h>
#include <commdlg.h>

static INT_PTR CALLBACK inputDlgProc(HWND, UINT, WPARAM, LPARAM);

// Canvas subclass: paints HCURSOR stored via SetPropW(L"CURSOR")
static LRESULT CALLBACK CanvasSubclassProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        HCURSOR hCur = (HCURSOR)GetPropW(hwnd, L"CURSOR");
        if (hCur) {
            ICONINFO ii = {};
            if (GetIconInfo(hCur, &ii)) {
                BITMAP bm = {}; int cw = 32, ch = 32;
                if (ii.hbmColor && GetObjectW(ii.hbmColor, sizeof(bm), &bm)) { cw = bm.bmWidth; ch = bm.bmHeight; }
                else if (ii.hbmMask && GetObjectW(ii.hbmMask, sizeof(bm), &bm)) { cw = bm.bmWidth; ch = bm.bmHeight / 2; }
                int pad = 6, aw = rc.right - rc.left - pad*2, ah = rc.bottom - rc.top - pad*2;
                float s = 3.0f; if (cw > 0) s = min(s, (float)aw/cw); if (ch > 0) s = min(s, (float)ah/ch);
                int dw = (int)(cw*s), dh = (int)(ch*s);
                int x = (rc.right - rc.left - dw)/2, y = (rc.bottom - rc.top - dh)/2;
                DrawIconEx(hdc, x, y, hCur, dw, dh, 0, nullptr, DI_NORMAL);
                if (ii.hbmMask)  DeleteObject(ii.hbmMask);
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
            }
        }
        FrameRect(hdc, &rc, (HBRUSH)GetStockObject(GRAY_BRUSH));
        EndPaint(hwnd, &ps); return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, w, l);
}

// ===================== Constructor =====================

ConfigDialog::ConfigDialog(HINSTANCE hInstance, const std::wstring& configPath)
    : m_hInstance(hInstance), m_configPath(configPath) {
    m_config.load(configPath); m_working = m_config.get();
}
ConfigDialog::~ConfigDialog() {
    if (m_hCurCurrent) DestroyCursor(m_hCurCurrent);
    if (m_hCurPreview) DestroyCursor(m_hCurPreview);
}
HWND ConfigDialog::create(HWND parent) {
    m_hwnd = CreateDialogParamW(m_hInstance, MAKEINTRESOURCEW(IDD_MAIN_DIALOG),
        parent, dlgProc, (LPARAM)this);
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

// ===================== Preview =====================

void ConfigDialog::updatePreviewCursor() {
    bool enabled = IsDlgButtonChecked(m_hwnd, IDC_CHECK_HIGHLIGHT) == BST_CHECKED;
    bool useFile = IsDlgButtonChecked(m_hwnd, IDC_RADIO_FILE) == BST_CHECKED;

    BOOL tr; int size = (int)GetDlgItemInt(m_hwnd, IDC_EDIT_HIGHLIGHT_SIZE, &tr, FALSE);
    if (!tr || size < 24) size = 48;

    HCURSOR hNew = nullptr;
    if (!enabled) {
        hNew = CopyCursor(LoadCursor(nullptr, IDC_ARROW));
    } else if (useFile) {
        if (!m_working.highlightCustomFile.empty()) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, m_working.highlightCustomFile.c_str(), -1, nullptr, 0);
            if (wlen > 1) {
                std::wstring wf(wlen - 1, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, m_working.highlightCustomFile.c_str(), -1, &wf[0], wlen);
                hNew = (HCURSOR)LoadImageW(nullptr, wf.c_str(), IMAGE_CURSOR, size, size, LR_LOADFROMFILE);
            }
        }
        if (!hNew) hNew = CopyCursor(LoadCursor(nullptr, IDC_ARROW));
    } else {
        HWND hCombo = GetDlgItem(m_hwnd, IDC_COMBO_SHAPE);
        int si = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
        const char* keys[] = { "arrow","hand","ibeam","cross","sizeall","wait","circle","square" };
        std::string shape = (si >= 0 && si < 8) ? keys[si] : "arrow";
        hNew = MouseController::createColorCursor(size, shape, m_working.highlightColor);
    }
    if (!hNew) hNew = CopyCursor(LoadCursor(nullptr, IDC_ARROW));

    // Set property BEFORE destroying old cursor (avoids race window)
    HWND hCanvas = GetDlgItem(m_hwnd, IDC_CANVAS_PREVIEW);
    if (hCanvas) {
        SetPropW(hCanvas, L"CURSOR", (HANDLE)hNew);
        InvalidateRect(hCanvas, nullptr, TRUE);
        UpdateWindow(hCanvas);
    }

    // Now safe to destroy old preview cursor
    if (m_hCurPreview) DestroyCursor(m_hCurPreview);
    m_hCurPreview = hNew;
}

void ConfigDialog::updateHighlightControls(HWND hwnd) {
    bool enabled = IsDlgButtonChecked(hwnd, IDC_CHECK_HIGHLIGHT) == BST_CHECKED;
    bool useFile = IsDlgButtonChecked(hwnd, IDC_RADIO_FILE) == BST_CHECKED;

    // Check if shape is custom colored (circle/square) vs system cursor
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_SHAPE);
    int si = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    bool isCustomColored = (si >= 6); // indices 6=circle, 7=square

    EnableWindow(GetDlgItem(hwnd, IDC_COMBO_SHAPE), enabled && !useFile);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_COLOR), enabled && !useFile && isCustomColored);
    // Size: enabled for custom colored shapes (circle/square) or file mode
    bool sizeOn = enabled && (useFile || isCustomColored);
    EnableWindow(GetDlgItem(hwnd, IDC_EDIT_HIGHLIGHT_SIZE), sizeOn);
    EnableWindow(GetDlgItem(hwnd, IDC_SPIN_HIGHLIGHT_SIZE), sizeOn);
    EnableWindow(GetDlgItem(hwnd, IDC_RADIO_CONFIG), enabled);
    EnableWindow(GetDlgItem(hwnd, IDC_RADIO_FILE), enabled);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_BROWSE_CUR), enabled && useFile);
}

void ConfigDialog::onBrowseCustomCursor(HWND hwnd) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Cursor Files (*.cur;*.ani)\0*.cur;*.ani\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        int len = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
        if (len > 1) { std::string s(len-1,'\0'); WideCharToMultiByte(CP_UTF8, 0, file, -1, s.data(), len, nullptr, nullptr); m_working.highlightCustomFile = s; }
        updatePreviewCursor();
    }
}

void ConfigDialog::onRefreshHighlight(HWND hwnd) {
    // Reload config from disk and update UI + preview
    m_config.load(m_configPath);
    m_working = m_config.get();

    // Update controls from reloaded config
    CheckDlgButton(hwnd, IDC_CHECK_HIGHLIGHT, m_working.highlightEnabled ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hwnd, IDC_EDIT_HIGHLIGHT_SIZE, m_working.highlightSize, FALSE);

    HWND shapeCombo = GetDlgItem(hwnd, IDC_COMBO_SHAPE);
    if (shapeCombo) {
        if (SendMessageW(shapeCombo, CB_GETCOUNT, 0, 0) == 0) {
            // Combo was empty — repopulate
            const wchar_t* names[] = {
                L"标准选择(箭头)", L"链接选择(手型)", L"文本选择(I型)",
                L"精确选择(十字)",   L"移动(四向)",     L"忙碌(等待)",
                L"圆形彩色",         L"方形彩色"
            };
            const char* keys[] = { "arrow","hand","ibeam","cross","sizeall","wait","circle","square" };
            for (int i = 0; i < 8; ++i)
                SendMessageW(shapeCombo, CB_ADDSTRING, 0, (LPARAM)names[i]);
            int sel = 0;
            for (int i = 0; i < 8; ++i) { if (m_working.highlightShape == keys[i]) { sel = i; break; } }
            SendMessageW(shapeCombo, CB_SETCURSEL, sel, 0);
        } else {
            // Just update selection
            const char* keys[] = { "arrow","hand","ibeam","cross","sizeall","wait","circle","square" };
            int sel = 0;
            for (int i = 0; i < 8; ++i) { if (m_working.highlightShape == keys[i]) { sel = i; break; } }
            SendMessageW(shapeCombo, CB_SETCURSEL, sel, 0);
        }
    }

    bool hasFile = !m_working.highlightCustomFile.empty();
    CheckRadioButton(hwnd, IDC_RADIO_CONFIG, IDC_RADIO_FILE,
        hasFile ? IDC_RADIO_FILE : IDC_RADIO_CONFIG);

    updateHighlightControls(hwnd);
    updatePreviewCursor();

    // Refresh current cursor canvas
    HWND hCurCanvas = GetDlgItem(hwnd, IDC_CANVAS_CURRENT);
    if (hCurCanvas) {
        if (m_hCurCurrent) DestroyCursor(m_hCurCurrent);
        m_hCurCurrent = CopyCursor(LoadCursor(nullptr, IDC_ARROW));
        SetPropW(hCurCanvas, L"CURSOR", (HANDLE)m_hCurCurrent);
        InvalidateRect(hCurCanvas, nullptr, TRUE);
        UpdateWindow(hCurCanvas);
    }

    SetDlgItemTextW(hwnd, IDC_STATUS, L"已刷新");
    SetTimer(hwnd, 999, 2000, nullptr);
}

void ConfigDialog::onChooseColor(HWND hwnd) {
    static COLORREF s_custom[16] = {};
    CHOOSECOLORW cc = { sizeof(cc) };
    cc.hwndOwner = hwnd;
    cc.rgbResult = m_working.highlightColor;
    cc.lpCustColors = s_custom;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (ChooseColorW(&cc)) {
        m_working.highlightColor = (int)cc.rgbResult;
        updatePreviewCursor();
    }
}

// ===================== Init =====================

void ConfigDialog::onInit(HWND hwnd) {
    const auto& cfg = m_working;

    // Core behavior
    CheckRadioButton(hwnd, IDC_RADIO_ALT_TAB, IDC_RADIO_ANY,
        cfg.triggerMode == "any_focus_change" ? IDC_RADIO_ANY : IDC_RADIO_ALT_TAB);
    CheckRadioButton(hwnd, IDC_RADIO_INSTANT, IDC_RADIO_SMOOTH,
        cfg.mouseMode == "smooth" ? IDC_RADIO_SMOOTH : IDC_RADIO_INSTANT);
    SetDlgItemInt(hwnd, IDC_EDIT_DURATION, cfg.smoothDurationMs, FALSE);
    SendDlgItemMessageW(hwnd, IDC_SPIN_DURATION, UDM_SETRANGE32, 50, 600);
    BOOL smooth = cfg.mouseMode == "smooth";
    EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DURATION), smooth);
    EnableWindow(GetDlgItem(hwnd, IDC_SPIN_DURATION), smooth);
    SetDlgItemInt(hwnd, IDC_EDIT_DELAY, cfg.moveDelayMs, FALSE);
    SendDlgItemMessageW(hwnd, IDC_SPIN_DELAY, UDM_SETRANGE32, 0, 2000);
    CheckRadioButton(hwnd, IDC_RADIO_WINDOW, IDC_RADIO_CLIENT,
        cfg.targetArea == "client_rect" ? IDC_RADIO_CLIENT : IDC_RADIO_WINDOW);
    CheckDlgButton(hwnd, IDC_CHECK_ENABLED, cfg.enabled ? BST_CHECKED : BST_UNCHECKED);

    // Highlight
    CheckDlgButton(hwnd, IDC_CHECK_HIGHLIGHT, cfg.highlightEnabled ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hwnd, IDC_EDIT_HIGHLIGHT_SIZE, cfg.highlightSize, FALSE);
    SendDlgItemMessageW(hwnd, IDC_SPIN_HIGHLIGHT_SIZE, UDM_SETRANGE32, 24, 128);

    // Subclass canvas static controls for cursor painting
    SetWindowLongPtrW(GetDlgItem(hwnd, IDC_CANVAS_CURRENT), GWLP_WNDPROC, (LONG_PTR)CanvasSubclassProc);
    SetWindowLongPtrW(GetDlgItem(hwnd, IDC_CANVAS_PREVIEW), GWLP_WNDPROC, (LONG_PTR)CanvasSubclassProc);

    // Dynamically create shape combo and color button (bypass .rc template issues)
    // Convert DLU to pixels using dialog base units
    LONG base = GetDialogBaseUnits();
    int baseX = LOWORD(base), baseY = HIWORD(base);
    auto dlu2px = [=](int x, int y) { return MulDiv(x, baseX, 4); };
    auto dlu2py = [=](int x, int y) { return MulDiv(y, baseY, 8); };

    HWND hShapeCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
        dlu2px(210,0), dlu2py(0,164), dlu2px(95,0), dlu2py(0,200),
        hwnd, (HMENU)(UINT_PTR)IDC_COMBO_SHAPE, m_hInstance, nullptr);
    if (hShapeCombo) {
        SendMessageW(hShapeCombo, WM_SETFONT, (WPARAM)SendMessageW(hwnd, WM_GETFONT, 0, 0), TRUE);
        const wchar_t* names[] = {
            L"标准选择(箭头)", L"链接选择(手型)", L"文本选择(I型)",
            L"精确选择(十字)",   L"移动(四向)",     L"忙碌(等待)",
            L"圆形彩色",         L"方形彩色"
        };
        const char* keys[] = { "arrow","hand","ibeam","cross","sizeall","wait","circle","square" };
        for (int i = 0; i < 8; ++i) SendMessageW(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)names[i]);
        int sel = 0;
        for (int i = 0; i < 8; ++i) { if (cfg.highlightShape == keys[i]) { sel = i; break; } }
        SendMessageW(hShapeCombo, CB_SETCURSEL, sel, 0);
        SendMessageW(hShapeCombo, CB_SETMINVISIBLE, 5, 0);
    }

    HWND hColorBtn = CreateWindowExW(0, L"BUTTON", L"颜色...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        dlu2px(210,0), dlu2py(0,190), dlu2px(60,0), dlu2py(0,14),
        hwnd, (HMENU)(UINT_PTR)IDC_BTN_COLOR, m_hInstance, nullptr);
    if (hColorBtn) {
        SendMessageW(hColorBtn, WM_SETFONT, (WPARAM)SendMessageW(hwnd, WM_GETFONT, 0, 0), TRUE);
    }

    // Radio toggle: config vs file (determined by presence of custom file)
    bool hasCustomFile = !cfg.highlightCustomFile.empty();
    CheckRadioButton(hwnd, IDC_RADIO_CONFIG, IDC_RADIO_FILE,
        hasCustomFile ? IDC_RADIO_FILE : IDC_RADIO_CONFIG);

    updateHighlightControls(hwnd);

    // Excluded processes
    HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
    for (auto& p : cfg.excludedProcesses) {
        std::wstring w(p.begin(), p.end());
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)w.c_str());
    }

    // Log combo
    HWND combo = GetDlgItem(hwnd, IDC_COMBO_LOG);
    const wchar_t* logNames[] = { L"无", L"错误", L"警告", L"信息", L"调试" };
    for (int i = 0; i < 5; ++i) SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)logNames[i]);
    int lsel = 0;
    if (cfg.logLevel == "error") lsel = 1; else if (cfg.logLevel == "warn") lsel = 2;
    else if (cfg.logLevel == "info") lsel = 3; else if (cfg.logLevel == "debug") lsel = 4;
    SendMessageW(combo, CB_SETCURSEL, lsel, 0);

    DaemonController dc;
    CheckDlgButton(hwnd, IDC_CHECK_AUTOSTART, dc.isAutoStart() ? BST_CHECKED : BST_UNCHECKED);
    refreshDaemonStatus(hwnd);

    // Current cursor canvas
    if (m_hCurCurrent) DestroyCursor(m_hCurCurrent);
    m_hCurCurrent = CopyCursor(LoadCursor(nullptr, IDC_ARROW));
    HWND hCurCanvas = GetDlgItem(hwnd, IDC_CANVAS_CURRENT);
    if (hCurCanvas) SetPropW(hCurCanvas, L"CURSOR", (HANDLE)m_hCurCurrent);

    updatePreviewCursor();
}

void ConfigDialog::refreshDaemonStatus(HWND hwnd) {
    DaemonController dc; bool running = dc.isRunning();
    SetDlgItemTextW(hwnd, IDC_STATUS, running ? L"● 运行中" : L"○ 已停止");
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_START), !running);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_STOP), running);
}

// ===================== Collect & Save =====================

void ConfigDialog::collectValues(HWND hwnd) {
    BOOL tr;
    m_working.triggerMode = IsDlgButtonChecked(hwnd, IDC_RADIO_ANY) == BST_CHECKED ? "any_focus_change" : "alt_tab_only";
    m_working.mouseMode = IsDlgButtonChecked(hwnd, IDC_RADIO_SMOOTH) == BST_CHECKED ? "smooth" : "instant";
    int dur = GetDlgItemInt(hwnd, IDC_EDIT_DURATION, &tr, FALSE);
    if (tr) { if (dur < 50) dur = 50; if (dur > 600) dur = 600; m_working.smoothDurationMs = dur; }
    int delay = GetDlgItemInt(hwnd, IDC_EDIT_DELAY, &tr, FALSE);
    if (tr) { if (delay < 0) delay = 0; if (delay > 2000) delay = 2000; m_working.moveDelayMs = delay; }
    m_working.targetArea = IsDlgButtonChecked(hwnd, IDC_RADIO_CLIENT) == BST_CHECKED ? "client_rect" : "window_rect";
    m_working.enabled = IsDlgButtonChecked(hwnd, IDC_CHECK_ENABLED) == BST_CHECKED;
    m_working.highlightEnabled = IsDlgButtonChecked(hwnd, IDC_CHECK_HIGHLIGHT) == BST_CHECKED;

    int hlSize = GetDlgItemInt(hwnd, IDC_EDIT_HIGHLIGHT_SIZE, &tr, FALSE);
    if (tr) { if (hlSize < 24) hlSize = 24; if (hlSize > 128) hlSize = 128; m_working.highlightSize = hlSize; }

    HWND shapeCombo = GetDlgItem(hwnd, IDC_COMBO_SHAPE);
    int si = (int)SendMessageW(shapeCombo, CB_GETCURSEL, 0, 0);
    const char* keys[] = { "arrow","hand","ibeam","cross","sizeall","wait","circle","square" };
    m_working.highlightShape = (si >= 0 && si < 8) ? keys[si] : "arrow";

    // Clear custom file if config mode is active
    if (IsDlgButtonChecked(hwnd, IDC_RADIO_CONFIG) == BST_CHECKED)
        m_working.highlightCustomFile.clear();

    m_working.excludedProcesses.clear();
    HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
    int count = (int)SendMessageW(list, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; ++i) {
        int len = (int)SendMessageW(list, LB_GETTEXTLEN, i, 0);
        if (len <= 0) continue;
        std::wstring w(len+1, L'\0'); SendMessageW(list, LB_GETTEXT, i, (LPARAM)w.data()); w.resize(len);
        int l = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (l > 1) { std::string s(l-1,'\0'); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), l, nullptr, nullptr); m_working.excludedProcesses.push_back(s); }
    }
    HWND combo = GetDlgItem(hwnd, IDC_COMBO_LOG);
    int ls = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    const char* lvls[] = { "none","error","warn","info","debug" };
    m_working.logLevel = (ls >= 0 && ls < 5) ? lvls[ls] : "none";
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
    case IDOK:  onSave(hwnd); break;
    case IDCANCEL: hideWindow(); break;
    case IDC_RADIO_SMOOTH:
        EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DURATION), TRUE); EnableWindow(GetDlgItem(hwnd, IDC_SPIN_DURATION), TRUE); break;
    case IDC_RADIO_INSTANT:
        EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DURATION), FALSE); EnableWindow(GetDlgItem(hwnd, IDC_SPIN_DURATION), FALSE); break;
    case IDC_BTN_ADD: onAddExclude(hwnd); break;
    case IDC_BTN_REMOVE: onRemoveExclude(hwnd); break;
    case IDM_TRAY_OPEN: case IDM_TRAY_TOGGLE: case IDM_TRAY_EXIT: onTrayMenu(id); break;
    case IDC_BTN_START: { DaemonController dc; if (dc.start()) refreshDaemonStatus(hwnd);
        else MessageBoxW(hwnd, L"无法启动守护进程。", L"错误", MB_OK | MB_ICONWARNING); break; }
    case IDC_BTN_STOP: { DaemonController dc; dc.stop(); Sleep(300); refreshDaemonStatus(hwnd); break; }
    case IDC_CHECK_HIGHLIGHT: updateHighlightControls(hwnd); updatePreviewCursor(); break;
    case IDC_RADIO_CONFIG: case IDC_RADIO_FILE: updateHighlightControls(hwnd); updatePreviewCursor(); break;
    case IDC_COMBO_SHAPE: if (code == CBN_SELCHANGE) updatePreviewCursor(); break;
    case IDC_EDIT_HIGHLIGHT_SIZE: if (code == EN_CHANGE) updatePreviewCursor(); break;
    case IDC_BTN_COLOR: onChooseColor(hwnd); break;
    case IDC_BTN_REFRESH: onRefreshHighlight(hwnd); break;
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
                wchar_t e[256] = {}; SendMessageW(list, LB_GETTEXT, i, (LPARAM)e);
                if (_wcsicmp(e, buf) == 0) { dup = true; break; }
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
