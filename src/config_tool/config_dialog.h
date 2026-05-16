#pragma once

#include <Windows.h>
#include <string>
#include "../common/config_manager.h"
#include "../resource.h"

// Custom tray notification message
#define WM_TRAYICON    (WM_APP + 1)
#define UID_TRAY       1

class ConfigDialog {
public:
    ConfigDialog(HINSTANCE hInstance, const std::wstring& configPath);

    // Create modeless dialog; returns HWND or nullptr.
    HWND create(HWND parent);

    // Show/hide the settings window.
    void showWindow();
    void hideWindow();

    void refreshStatus();

private:
    HINSTANCE m_hInstance;
    std::wstring m_configPath;
    ConfigManager m_config;
    AppConfig m_working;
    HWND m_hwnd = nullptr;

    // Tray
    NOTIFYICONDATAW m_nid = {};
    HMENU m_trayMenu = nullptr;

    void createTrayIcon();
    void updateTrayMenu();
    void removeTrayIcon();
    void showTrayMenu();

    static INT_PTR CALLBACK dlgProc(HWND, UINT, WPARAM, LPARAM);
    void onInit(HWND hwnd);
    void onSave(HWND hwnd);
    void onCommand(HWND hwnd, WORD id, WORD code, HWND ctl);
    void onTrayMenu(WORD id);
    void onToggleDaemon();
    void onAddExclude(HWND hwnd);
    void onRemoveExclude(HWND hwnd);
    void collectValues(HWND hwnd);
    void refreshDaemonStatus(HWND hwnd);
};
