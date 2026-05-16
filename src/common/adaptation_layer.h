#pragma once

#include <Windows.h>
#include <string>
#include <string_view>

namespace Adaptation {

bool isWindows11();
bool isWindows10OrLater();

// System window detection
bool isSystemWindow(HWND hwnd);
bool isAltTabSwitcher(HWND hwnd);
bool isTaskbar(HWND hwnd);
bool isDesktop(HWND hwnd);
bool isStartMenu(HWND hwnd);
bool isFullscreenWindow(HWND hwnd);
bool isUWPWindow(HWND hwnd);
bool isCloakedWindow(HWND hwnd);

// Get window's executable name (lowercase)
std::string getProcessName(HWND hwnd);
std::wstring getWindowClassName(HWND hwnd);

// Get actual window rect (handles UWP ApplicationFrameWindow wrapper)
RECT getActualWindowRect(HWND hwnd, bool clientArea);

} // namespace Adaptation
