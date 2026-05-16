#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct AppConfig {
    int version = 1;
    std::string triggerMode = "alt_tab_only";
    std::string mouseMode = "instant";
    int smoothDurationMs = 150;
    int moveDelayMs = 0;
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
