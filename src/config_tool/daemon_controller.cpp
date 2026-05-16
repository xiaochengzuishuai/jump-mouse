#include "daemon_controller.h"
#include <filesystem>
#include <TlHelp32.h>

std::wstring DaemonController::exeDir() const {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::filesystem::path p(path);
    return p.parent_path().wstring();
}

std::wstring DaemonController::daemonPath() const {
    return exeDir() + L"\\" + DAEMON_EXE;
}

bool DaemonController::isRunning() const {
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, MUTEX_NAME);
    if (h) {
        CloseHandle(h);
        return true;
    }
    return false;
}

bool DaemonController::start() {
    if (isRunning()) return true;

    auto path = daemonPath();
    if (!std::filesystem::exists(path)) return false;

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(path.c_str(), nullptr,
            nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW,
            nullptr, nullptr, &si, &pi)) {
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool DaemonController::stop() {
    // Find the daemon's hidden window and send WM_QUIT
    HWND hwnd = findDaemonWindow();
    if (hwnd) {
        PostMessageW(hwnd, WM_QUIT, 0, 0);
        return true;
    }

    // Fallback: find process and terminate
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (std::wstring(pe.szExeFile) == DAEMON_EXE) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc) {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                }
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return true;
}

HWND DaemonController::findDaemonWindow() const {
    return FindWindowW(L"MouseFocusDaemon_Hidden", L"");
}

bool DaemonController::setAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    bool ok = false;
    if (enable) {
        auto path = daemonPath();
        ok = (RegSetValueExW(hKey, L"MouseFocusDaemon", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(path.c_str()),
                static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS);
    } else {
        ok = (RegDeleteValueW(hKey, L"MouseFocusDaemon") == ERROR_SUCCESS
              || RegDeleteValueW(hKey, L"MouseFocusDaemon") == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(hKey);
    return ok;
}

bool DaemonController::isAutoStart() const {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type;
    wchar_t buf[512] = {};
    DWORD size = sizeof(buf);
    bool exists = (RegQueryValueExW(hKey, L"MouseFocusDaemon", nullptr,
                       &type, reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return exists;
}
