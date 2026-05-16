#include "config_dialog.h"
#include "daemon_controller.h"
#include "../common/logger.h"
#include <CommCtrl.h>

static INT_PTR CALLBACK inputDlgProc(HWND, UINT, WPARAM, LPARAM);

ConfigDialog::ConfigDialog(HINSTANCE hInstance, const std::wstring& configPath)
    : m_hInstance(hInstance), m_configPath(configPath)
{
    m_config.load(configPath);
    m_working = m_config.get();
}

HWND ConfigDialog::create(HWND parent) {
    m_hwnd = CreateDialogParamW(m_hInstance, MAKEINTRESOURCEW(IDD_MAIN_DIALOG),
        parent, dlgProc, reinterpret_cast<LPARAM>(this));
    return m_hwnd;
}

void ConfigDialog::showWindow() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
    }
}

void ConfigDialog::hideWindow() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE);
}

// ===================== Tray Icon =====================

void ConfigDialog::createTrayIcon() {
    std::wstring iconPath = m_configPath;
    auto pos = iconPath.find_last_of(L"\\/");
    iconPath = (pos != std::wstring::npos)
        ? iconPath.substr(0, pos + 1) + L"1.ico"
        : L"1.ico";

    HICON hIcon = nullptr;
    if (GetFileAttributesW(iconPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        int cx = GetSystemMetrics(SM_CXSMICON);
        int cy = GetSystemMetrics(SM_CYSMICON);
        hIcon = (HICON)LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, cx, cy, LR_LOADFROMFILE);
    }
    if (!hIcon) {
        hIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr),
            MAKEINTRESOURCEW(32512), IMAGE_ICON, 0, 0, LR_SHARED);
    }

    m_nid = { sizeof(NOTIFYICONDATAW) };
    m_nid.hWnd = m_hwnd;
    m_nid.uID = UID_TRAY;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = hIcon;
    wcscpy_s(m_nid.szTip, L"Mouse Focus");

    Shell_NotifyIconW(NIM_ADD, &m_nid);
    if (hIcon) DestroyIcon(hIcon);
}

void ConfigDialog::updateTrayMenu() {
    if (m_trayMenu) DestroyMenu(m_trayMenu);
    m_trayMenu = CreatePopupMenu();

    AppendMenuW(m_trayMenu, MF_STRING, IDM_TRAY_OPEN, L"打开主界面");
    AppendMenuW(m_trayMenu, MF_SEPARATOR, 0, nullptr);

    DaemonController dc;
    bool running = dc.isRunning();
    if (running) {
        AppendMenuW(m_trayMenu, MF_STRING, IDM_TRAY_TOGGLE, L"服务状态：开（点击关闭）");
    } else {
        AppendMenuW(m_trayMenu, MF_STRING, IDM_TRAY_TOGGLE, L"服务状态：关（点击开启）");
    }

    AppendMenuW(m_trayMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_trayMenu, MF_STRING, IDM_TRAY_EXIT, L"退出软件");
}

void ConfigDialog::showTrayMenu() {
    updateTrayMenu();
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd); // required for TrackPopupMenu to work correctly
    TrackPopupMenu(m_trayMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
        pt.x, pt.y, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0); // fix menu not dismissing
}

void ConfigDialog::removeTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &m_nid);
    if (m_trayMenu) DestroyMenu(m_trayMenu);
}

void ConfigDialog::onToggleDaemon() {
    DaemonController dc;
    if (dc.isRunning()) {
        dc.stop();
        Sleep(300);
    } else {
        dc.start();
    }
    refreshStatus();
}

void ConfigDialog::onTrayMenu(WORD id) {
    switch (id) {
    case IDM_TRAY_OPEN:
        showWindow();
        break;
    case IDM_TRAY_TOGGLE:
        onToggleDaemon();
        break;
    case IDM_TRAY_EXIT:
        removeTrayIcon();
        DestroyWindow(m_hwnd);
        PostQuitMessage(0);
        break;
    }
}

// ===================== Dialog Procedure =====================

INT_PTR CALLBACK ConfigDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ConfigDialog* self = reinterpret_cast<ConfigDialog*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_INITDIALOG) {
        self = reinterpret_cast<ConfigDialog*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
        self->onInit(hwnd);
        self->createTrayIcon();
        return TRUE;
    }

    if (!self) return FALSE;

    switch (msg) {
    case WM_COMMAND:
        self->onCommand(hwnd, LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HWND>(lParam));
        return TRUE;

    case WM_TIMER:
        if (wParam == 999) {
            KillTimer(hwnd, 999);
            self->refreshDaemonStatus(hwnd);
        }
        return TRUE;

    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_LBUTTONUP) {
            if (IsWindowVisible(hwnd))
                self->hideWindow();
            else
                self->showWindow();
        } else if (LOWORD(lParam) == WM_RBUTTONUP) {
            self->showTrayMenu();
        }
        return TRUE;

    case WM_CLOSE:
        // Minimize to tray instead of closing
        self->hideWindow();
        return TRUE;

    case WM_DESTROY:
        self->removeTrayIcon();
        PostQuitMessage(0);
        return TRUE;
    }
    return FALSE;
}

void ConfigDialog::refreshStatus() {
    refreshDaemonStatus(m_hwnd);
}

// ===================== Init =====================

void ConfigDialog::onInit(HWND hwnd) {
    const auto& cfg = m_working;

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

    HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
    for (auto& p : cfg.excludedProcesses) {
        std::wstring w(p.begin(), p.end());
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str()));
    }

    HWND combo = GetDlgItem(hwnd, IDC_COMBO_LOG);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"无"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"错误"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"警告"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"信息"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"调试"));
    int sel = 0;
    if (cfg.logLevel == "error") sel = 1;
    else if (cfg.logLevel == "warn") sel = 2;
    else if (cfg.logLevel == "info") sel = 3;
    else if (cfg.logLevel == "debug") sel = 4;
    SendMessageW(combo, CB_SETCURSEL, sel, 0);

    DaemonController dc;
    CheckDlgButton(hwnd, IDC_CHECK_AUTOSTART, dc.isAutoStart() ? BST_CHECKED : BST_UNCHECKED);
    refreshDaemonStatus(hwnd);
}

void ConfigDialog::refreshDaemonStatus(HWND hwnd) {
    DaemonController dc;
    bool running = dc.isRunning();
    SetDlgItemTextW(hwnd, IDC_STATUS,
        running ? L"● 运行中" : L"○ 已停止");
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_START), !running);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_STOP), running);
}

// ===================== Save & Collect =====================

void ConfigDialog::collectValues(HWND hwnd) {
    m_working.triggerMode = IsDlgButtonChecked(hwnd, IDC_RADIO_ANY) == BST_CHECKED
        ? "any_focus_change" : "alt_tab_only";
    m_working.mouseMode = IsDlgButtonChecked(hwnd, IDC_RADIO_SMOOTH) == BST_CHECKED
        ? "smooth" : "instant";

    BOOL translated;
    int dur = GetDlgItemInt(hwnd, IDC_EDIT_DURATION, &translated, FALSE);
    if (translated) {
        if (dur < 50) dur = 50;
        if (dur > 600) dur = 600;
        m_working.smoothDurationMs = dur;
    }

    int delay = GetDlgItemInt(hwnd, IDC_EDIT_DELAY, &translated, FALSE);
    if (translated) {
        if (delay < 0) delay = 0;
        if (delay > 2000) delay = 2000;
        m_working.moveDelayMs = delay;
    }

    m_working.targetArea = IsDlgButtonChecked(hwnd, IDC_RADIO_CLIENT) == BST_CHECKED
        ? "client_rect" : "window_rect";
    m_working.enabled = IsDlgButtonChecked(hwnd, IDC_CHECK_ENABLED) == BST_CHECKED;

    m_working.excludedProcesses.clear();
    HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
    int count = static_cast<int>(SendMessageW(list, LB_GETCOUNT, 0, 0));
    for (int i = 0; i < count; ++i) {
        int len = static_cast<int>(SendMessageW(list, LB_GETTEXTLEN, i, 0));
        if (len <= 0) continue;
        std::wstring w(len + 1, L'\0');
        SendMessageW(list, LB_GETTEXT, i, reinterpret_cast<LPARAM>(w.data()));
        w.resize(len);
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Len > 1) {
            std::string utf8(utf8Len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, utf8.data(), utf8Len, nullptr, nullptr);
            m_working.excludedProcesses.push_back(utf8);
        }
    }

    HWND combo = GetDlgItem(hwnd, IDC_COMBO_LOG);
    int sel = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    const char* levels[] = { "none", "error", "warn", "info", "debug" };
    m_working.logLevel = (sel >= 0 && sel < 5) ? levels[sel] : "none";
}

void ConfigDialog::onSave(HWND hwnd) {
    collectValues(hwnd);
    m_config.apply(m_working);
    if (m_config.save(m_configPath)) {
        DaemonController dc;
        dc.setAutoStart(IsDlgButtonChecked(hwnd, IDC_CHECK_AUTOSTART) == BST_CHECKED);
        refreshDaemonStatus(hwnd);
        SetDlgItemTextW(hwnd, IDC_STATUS, L"配置已保存");
        SetTimer(hwnd, 999, 2000, nullptr);
    } else {
        MessageBoxW(hwnd, L"保存配置文件失败，请检查文件权限。",
            L"错误", MB_OK | MB_ICONERROR);
    }
}

// ===================== Commands =====================

void ConfigDialog::onCommand(HWND hwnd, WORD id, WORD /*code*/, HWND /*ctl*/) {
    switch (id) {
    case IDOK:  onSave(hwnd); break;
    case IDCANCEL: hideWindow(); break;
    case IDC_RADIO_SMOOTH:
        EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DURATION), TRUE);
        EnableWindow(GetDlgItem(hwnd, IDC_SPIN_DURATION), TRUE);
        break;
    case IDC_RADIO_INSTANT:
        EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DURATION), FALSE);
        EnableWindow(GetDlgItem(hwnd, IDC_SPIN_DURATION), FALSE);
        break;
    case IDC_BTN_ADD:    onAddExclude(hwnd);    break;
    case IDC_BTN_REMOVE: onRemoveExclude(hwnd); break;
    case IDM_TRAY_OPEN:
    case IDM_TRAY_TOGGLE:
    case IDM_TRAY_EXIT:   onTrayMenu(id);        break;
    case IDC_BTN_START: {
        DaemonController dc;
        if (dc.start()) refreshDaemonStatus(hwnd);
        else MessageBoxW(hwnd,
            L"无法启动守护进程。\n\n请确保 mouse_focus_daemon.exe 与本程序在同一目录。",
            L"启动失败", MB_OK | MB_ICONWARNING);
        break;
    }
    case IDC_BTN_STOP: {
        DaemonController dc;
        dc.stop();
        Sleep(300);
        refreshDaemonStatus(hwnd);
        break;
    }
    }
}

void ConfigDialog::onAddExclude(HWND hwnd) {
    wchar_t buf[256] = {};
    if (DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_INPUT_DIALOG),
            hwnd, inputDlgProc, reinterpret_cast<LPARAM>(buf)) == IDOK) {
        if (buf[0]) {
            HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
            int count = static_cast<int>(SendMessageW(list, LB_GETCOUNT, 0, 0));
            bool dup = false;
            for (int i = 0; i < count; ++i) {
                wchar_t existing[256] = {};
                SendMessageW(list, LB_GETTEXT, i, reinterpret_cast<LPARAM>(existing));
                if (_wcsicmp(existing, buf) == 0) { dup = true; break; }
            }
            if (!dup)
                SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buf));
        }
    }
}

void ConfigDialog::onRemoveExclude(HWND hwnd) {
    HWND list = GetDlgItem(hwnd, IDC_LIST_EXCLUDE);
    int sel = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    if (sel != LB_ERR)
        SendMessageW(list, LB_DELETESTRING, sel, 0);
}

// ===================== Input Sub-Dialog =====================

static INT_PTR CALLBACK inputDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* outBuf = nullptr;
    if (msg == WM_INITDIALOG) {
        outBuf = reinterpret_cast<wchar_t*>(lParam);
        SetFocus(GetDlgItem(hwnd, IDC_INPUT_TEXT));
        return FALSE;
    }
    if (msg == WM_COMMAND) {
        if (LOWORD(wParam) == IDOK) {
            if (outBuf) GetDlgItemTextW(hwnd, IDC_INPUT_TEXT, outBuf, 255);
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}
