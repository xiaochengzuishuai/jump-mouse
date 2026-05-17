#include "mouse_controller.h"
#include "../common/adaptation_layer.h"
#include "../common/logger.h"
#include <cmath>
#include <format>
#include <fstream>
#include <filesystem>

WindowCenter MouseController::calculateCenter(HWND hwnd, bool clientArea) const {
    RECT r = Adaptation::getActualWindowRect(hwnd, clientArea);
    return { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
}
void MouseController::moveInstant(HWND hwnd, bool clientArea) {
    auto c = calculateCenter(hwnd, clientArea);
    if (c.x < -16000 || c.x > 32000 || c.y < -16000 || c.y > 32000) return;
    SetCursorPos(c.x, c.y);
    LOG_DEBUG(std::format("Moved cursor to ({}, {})", c.x, c.y));
}
double MouseController::easeOutCubic(double t) { double u = 1.0 - t; return 1.0 - u*u*u; }
void MouseController::startSmooth(HWND target, bool ca) {
    POINT cur; GetCursorPos(&cur); m_animFrom = cur;
    auto center = calculateCenter(target, ca);
    m_animTo = { center.x, center.y };
    m_animStart = std::chrono::steady_clock::now(); m_animActive = true;
}
bool MouseController::smoothTick() {
    if (!m_animActive) return true;
    auto now = std::chrono::steady_clock::now();
    double t = std::chrono::duration<double,std::milli>(now - m_animStart).count() / m_animDurationMs;
    if (t >= 1.0) { SetCursorPos(m_animTo.x, m_animTo.y); m_animActive = false; return true; }
    double e = easeOutCubic(t);
    SetCursorPos(m_animFrom.x + (LONG)((m_animTo.x - m_animFrom.x)*e),
                 m_animFrom.y + (LONG)((m_animTo.y - m_animFrom.y)*e));
    return false;
}

// ===================== Registry helpers =====================
static const wchar_t* REG_KEY = L"Control Panel\\Cursors";

std::wstring MouseController::registryCursorPath(const wchar_t* name) {
    HKEY hKey; std::wstring result;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH]; DWORD size = sizeof(buf), type;
        if (RegQueryValueExW(hKey, name, nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && type == REG_EXPAND_SZ)
            result = buf;
        else if (RegQueryValueExW(hKey, name, nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && type == REG_SZ)
            result = buf;
        RegCloseKey(hKey);
    }
    return result;
}

void MouseController::setRegistryCursorPath(const wchar_t* name, const std::wstring& path) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, name, 0, REG_EXPAND_SZ, (const BYTE*)path.c_str(),
                       (DWORD)((path.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

void MouseController::notifyCursorChange() {
    SystemParametersInfoW(0x0057, 0, nullptr, SPIF_SENDCHANGE); // SPI_SETCURSORS
    LOG_DEBUG("  SPI_SETCURSORS sent");
}

// ===================== Shape-to-registry mapping =====================
static const wchar_t* regNameForShape(const std::string& shape) {
    if (shape == "arrow")   return L"Arrow";
    if (shape == "hand")    return L"Hand";
    if (shape == "ibeam")   return L"IBeam";
    if (shape == "cross")   return L"Crosshair";
    if (shape == "wait")    return L"Wait";
    if (shape == "sizeall") return L"SizeAll";
    return nullptr; // custom colored
}

std::wstring MouseController::resolveCursorPath() const {
    // Priority: custom file > system cursor shape > custom colored (will generate .cur)
    if (!m_highlightCustomFile.empty()) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, m_highlightCustomFile.c_str(), -1, nullptr, 0);
        if (wlen > 1) {
            std::wstring wf(wlen-1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, m_highlightCustomFile.c_str(), -1, &wf[0], wlen);
            if (std::filesystem::exists(wf)) return wf;
        }
    }
    // System cursor: read corresponding registry value
    auto regName = regNameForShape(m_highlightShape);
    if (regName) {
        auto path = registryCursorPath(regName);
        if (!path.empty()) return path;
    }
    // Custom colored: will generate .cur file
    return L""; // caller generates temp file
}

// ===================== .cur file writer =====================
void MouseController::setBit(std::vector<BYTE>& plane, int stride, int x, int y, bool on) {
    int off = y * stride + x / 8; int bit = 7 - (x % 8);
    if (on) plane[off] |= (1U << bit); else plane[off] &= ~(1U << bit);
}
bool MouseController::shapeHit(const std::string& shape, int x, int y, int r) {
    int dx = x - r, dy = y - r;
    if (shape == "circle") return dx*dx + dy*dy <= r*r;
    if (shape == "square") return abs(dx) <= r && abs(dy) <= r;
    return false;
}

bool MouseController::writeCurFile(const std::wstring& path) {
    int sz = m_highlightSize, r = sz / 2;
    int andStride = ((sz + 31) / 32) * 4; // DWORD-aligned
    int xorSize = sz * sz * 4;
    int andSize = sz * andStride;
    int dataSize = sizeof(BITMAPINFOHEADER) + xorSize + andSize;
    int hotspot = r;

    BYTE rv = GetRValue((COLORREF)m_highlightColor);
    BYTE gv = GetGValue((COLORREF)m_highlightColor);
    BYTE bv = GetBValue((COLORREF)m_highlightColor);

    try {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;

        // ICONDIR (6 bytes)
        auto w16 = [&](uint16_t v) { f.put((char)v); f.put((char)(v>>8)); };
        auto w32 = [&](uint32_t v) { w16((uint16_t)v); w16((uint16_t)(v>>16)); };

        w16(0);       // reserved
        w16(2);       // type = CUR
        w16(1);       // count

        // ICONDIRENTRY (16 bytes)
        uint32_t imgOff = 22;
        f.put((char)sz);  // width (0 = 256 if >255)
        f.put((char)sz);  // height
        f.put((char)0);   // color count
        f.put((char)0);   // reserved
        w16((uint16_t)hotspot);
        w16((uint16_t)hotspot);
        w32(dataSize);
        w32(imgOff);

        // DIB: BITMAPINFOHEADER (40 bytes)
        w32(40); w32(sz); w32(sz * 2); w16(1); w16(32); w32(BI_RGB);
        w32(xorSize + andSize); w32(0); w32(0); w32(0); w32(0);

        // XOR pixels (top-down BGRA)
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                if (shapeHit(m_highlightShape, x, y, r)) {
                    f.put(bv); f.put(gv); f.put(rv); f.put((char)0);
                } else {
                    f.put((char)0); f.put((char)0); f.put((char)0); f.put((char)0);
                }
            }
        }

        // AND mask (monochrome, DWORD-aligned)
        std::vector<BYTE> andBits(andStride * sz, 0xFF);
        for (int y = 0; y < sz; ++y)
            for (int x = 0; x < sz; ++x)
                if (shapeHit(m_highlightShape, x, y, r))
                    setBit(andBits, andStride, x, y, false);
        f.write((const char*)andBits.data(), andBits.size());
        f.close();
        return true;
    } catch (...) { return false; }
}

// ===================== Highlight application =====================

bool MouseController::applyHighlight() {
    if (!m_highlight || m_highlightActive) return false;

    // 1. Backup current Arrow value (user's actual cursor, not system default)
    m_originalArrowPath = registryCursorPath(L"Arrow");
    int len = WideCharToMultiByte(CP_UTF8, 0, m_originalArrowPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string arrowPathUtf8(len > 1 ? len-1 : 0, '\0');
    if (len > 1) WideCharToMultiByte(CP_UTF8, 0, m_originalArrowPath.c_str(), -1, &arrowPathUtf8[0], len, nullptr, nullptr);
    LOG_DEBUG(std::format("  Original Arrow = {}", arrowPathUtf8));

    // 2. Determine new cursor path
    std::wstring newPath;
    auto regName = regNameForShape(m_highlightShape);

    if (!m_highlightCustomFile.empty()) {
        // User-provided custom .cur file
        int wlen = MultiByteToWideChar(CP_UTF8, 0, m_highlightCustomFile.c_str(), -1, nullptr, 0);
        if (wlen > 1) {
            std::wstring wf(wlen-1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, m_highlightCustomFile.c_str(), -1, &wf[0], wlen);
            if (std::filesystem::exists(wf)) newPath = wf;
        }
    }

    if (newPath.empty() && regName) {
        // System cursor shape: use the path from the corresponding registry key
        newPath = registryCursorPath(regName);
    }

    if (newPath.empty()) {
        // Custom colored shape: generate temp .cur file
        wchar_t tmpDir[MAX_PATH];
        GetTempPathW(MAX_PATH, tmpDir);
        std::filesystem::create_directories(std::wstring(tmpDir) + L"MouseFocus");
        wchar_t tmpFile[MAX_PATH];
        GetTempFileNameW((std::wstring(tmpDir) + L"MouseFocus\\").c_str(), L"cur", 0, tmpFile);
        m_tempCurFile = tmpFile;
        // Change extension to .cur
        m_tempCurFile = m_tempCurFile.substr(0, m_tempCurFile.size() - 4) + L".cur";
        DeleteFileW(tmpFile); // remove the .tmp
        if (writeCurFile(m_tempCurFile))
            newPath = m_tempCurFile;
    }

    if (newPath.empty()) {
        LOG_DEBUG("  Could not resolve cursor path");
        return false;
    }

    // 3. Write new path to Arrow and notify system
    setRegistryCursorPath(L"Arrow", newPath);
    notifyCursorChange();

    m_highlightActive = true;
    GetCursorPos(&m_highlightPos);
    LOG_DEBUG(std::format("  Cursor changed: {} at ({},{})",
        std::string(newPath.begin(), newPath.end()), m_highlightPos.x, m_highlightPos.y));
    return true;
}

bool MouseController::pollHighlightRestore() {
    if (!m_highlightActive) return true;
    POINT cur; GetCursorPos(&cur);
    int dx = cur.x - m_highlightPos.x, dy = cur.y - m_highlightPos.y;
    if (dx*dx + dy*dy > 4) {
        LOG_DEBUG(std::format("  Mouse moved — restoring (d={},{})", dx, dy));
        forceRestoreCursor();
        return true;
    }
    return false;
}

void MouseController::forceRestoreCursor() {
    if (!m_highlightActive) return;
    m_highlightActive = false;

    // Restore original Arrow path
    if (!m_originalArrowPath.empty()) {
        setRegistryCursorPath(L"Arrow", m_originalArrowPath);
        notifyCursorChange();
        LOG_DEBUG("  Cursor restored to original Arrow");
        m_originalArrowPath.clear();
    }

    // Clean up temp .cur file
    if (!m_tempCurFile.empty()) {
        DeleteFileW(m_tempCurFile.c_str());
        m_tempCurFile.clear();
    }
}

// ===================== Preview cursor (for GUI) =====================
HCURSOR MouseController::createColorCursor(int size, const std::string& shape, COLORREF rgb) {
    // System cursor shapes: return a copy
    auto regName = regNameForShape(shape);
    if (regName) {
        HCURSOR hSys = LoadCursor(nullptr, MAKEINTRESOURCEW(
            shape=="arrow"?32512: shape=="hand"?32649: shape=="ibeam"?32513:
            shape=="cross"?32515: shape=="wait"?32514: shape=="sizeall"?32646: 32512));
        return hSys ? CopyCursor(hSys) : nullptr;
    }

    // Custom colored: use DIB + CreateIconIndirect
    int r = size / 2;
    int andStride = ((size + 15) / 16) * 2;
    std::vector<BYTE> andBits(andStride * size, 0xFF);

    BITMAPINFO bmi = {}; bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size; bmi.bmiHeader.biHeight = -size;
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr); BYTE* pColor = nullptr;
    HBITMAP hbmColor = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&pColor, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!hbmColor || !pColor) { if (hbmColor) DeleteObject(hbmColor); return nullptr; }

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (shapeHit(shape, x, y, r)) {
                setBit(andBits, andStride, x, y, false);
                int off = (y * size + x) * 4;
                pColor[off+0]=GetBValue(rgb); pColor[off+1]=GetGValue(rgb);
                pColor[off+2]=GetRValue(rgb); pColor[off+3]=0;
            }
        }
    }
    HBITMAP hbmMask = CreateBitmap(size, size, 1, 1, andBits.data());
    ICONINFO ii = {}; ii.fIcon = FALSE; ii.xHotspot=r; ii.yHotspot=r;
    ii.hbmMask = hbmMask; ii.hbmColor = hbmColor;
    HCURSOR h = CreateIconIndirect(&ii);
    DeleteObject(hbmMask); DeleteObject(hbmColor);
    return h;
}

HCURSOR createPreviewCursor(int size, const std::string& shape, COLORREF color) {
    return MouseController::createColorCursor(size, shape, color);
}
