#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct AppConfig {
    int version = 1;
    std::string triggerMode = "alt_tab_only";
    std::string mouseMode = "smooth";
    int smoothDurationMs = 600;
    int moveDelayMs = 100;
    bool highlightEnabled = false;
    std::string highlightShape = "arrow";
    int highlightColor = 0x0000FFFF;  // COLORREF: RGB(255,255,0)=yellow
    int highlightSize = 48;
    std::string highlightCustomFile;
    bool enabled = true;
    std::string targetArea = "window_rect";
    std::vector<std::string> excludedProcesses;
    std::vector<std::string> excludedClasses;
    std::string logLevel = "none";
    std::string logFile;
};

class ConfigManager {
public:
    ConfigManager() { setDefaults(); }

    bool load(const std::wstring& path);
    bool save(const std::wstring& path) const;

    const AppConfig& get() const { return m_cfg; }

    // Thread-safe config swap for hot-reload
    void apply(const AppConfig& cfg) { m_cfg = cfg; }

    const std::wstring& loadedPath() const { return m_loadedPath; }

private:
    void setDefaults();
    AppConfig m_cfg;
    std::wstring m_loadedPath;
};
