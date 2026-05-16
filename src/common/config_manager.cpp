#include "config_manager.h"
#include "json_util.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

void ConfigManager::setDefaults() {
    m_cfg = AppConfig{};
}

static std::string readFile(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

static std::string lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

bool ConfigManager::load(const std::wstring& path) {
    try {
        auto text = readFile(path);
        if (text.empty()) {
            setDefaults();
            m_loadedPath = path;
            return save(path);
        }
        auto root = JsonValue::parse(text);

        AppConfig cfg;
        cfg.version          = root.has("version") ? root["version"].asInt(1) : 1;
        cfg.triggerMode      = root.has("triggerMode") ? root["triggerMode"].asString("alt_tab_only") : "alt_tab_only";
        cfg.mouseMode        = root.has("mouseMode") ? root["mouseMode"].asString("smooth") : "smooth";
        cfg.smoothDurationMs = root.has("smoothDurationMs") ? root["smoothDurationMs"].asInt(600) : 600;
        cfg.moveDelayMs      = root.has("moveDelayMs") ? root["moveDelayMs"].asInt(100) : 100;
        cfg.highlightEnabled    = root.has("highlightEnabled") ? root["highlightEnabled"].asBool(false) : false;
        cfg.highlightShape     = root.has("highlightShape") ? root["highlightShape"].asString("circle") : "circle";
        cfg.highlightColor     = root.has("highlightColor") ? root["highlightColor"].asInt(0x0000FFFF) : 0x0000FFFF;
        cfg.highlightSize      = root.has("highlightSize") ? root["highlightSize"].asInt(48) : 48;
        cfg.highlightCustomFile = root.has("highlightCustomFile") ? root["highlightCustomFile"].asString("") : "";
        cfg.enabled            = root.has("enabled") ? root["enabled"].asBool(true) : true;
        cfg.targetArea       = root.has("targetArea") ? root["targetArea"].asString("window_rect") : "window_rect";
        cfg.logLevel         = root.has("logLevel") ? root["logLevel"].asString("none") : "none";
        cfg.logFile          = root.has("logFile") ? root["logFile"].asString("") : "";

        if (root.has("excludedProcesses")) {
            for (auto& v : root["excludedProcesses"].asArray())
                cfg.excludedProcesses.push_back(v.asString());
        }
        if (root.has("excludedClasses")) {
            for (auto& v : root["excludedClasses"].asArray())
                cfg.excludedClasses.push_back(v.asString());
        }

        // Validate
        if (cfg.triggerMode != "alt_tab_only" && cfg.triggerMode != "any_focus_change")
            cfg.triggerMode = "alt_tab_only";
        if (cfg.mouseMode != "instant" && cfg.mouseMode != "smooth")
            cfg.mouseMode = "instant";
        if (cfg.smoothDurationMs < 50) cfg.smoothDurationMs = 50;
        if (cfg.smoothDurationMs > 600) cfg.smoothDurationMs = 600;
        if (cfg.moveDelayMs < 0) cfg.moveDelayMs = 0;
        if (cfg.moveDelayMs > 2000) cfg.moveDelayMs = 2000;
        if (cfg.highlightSize < 24) cfg.highlightSize = 24;
        if (cfg.highlightSize > 128) cfg.highlightSize = 128;
        if (cfg.highlightShape != "circle" && cfg.highlightShape != "square"
         && cfg.highlightShape != "diamond" && cfg.highlightShape != "arrow"
         && cfg.highlightShape != "cross" && cfg.highlightShape != "custom")
            cfg.highlightShape = "circle";
        if (cfg.targetArea != "window_rect" && cfg.targetArea != "client_rect")
            cfg.targetArea = "window_rect";

        m_cfg = cfg;
        m_loadedPath = path;
        return true;
    } catch (const std::exception& e) {
        setDefaults();
        m_loadedPath = path;
        return false;
    }
}

bool ConfigManager::save(const std::wstring& path) const {
    try {
        auto root = JsonValue::makeObject();
        root["version"]          = JsonValue::makeInt(m_cfg.version);
        root["triggerMode"]      = JsonValue::makeString(m_cfg.triggerMode);
        root["mouseMode"]        = JsonValue::makeString(m_cfg.mouseMode);
        root["smoothDurationMs"] = JsonValue::makeInt(m_cfg.smoothDurationMs);
        root["moveDelayMs"]      = JsonValue::makeInt(m_cfg.moveDelayMs);
        root["highlightEnabled"]    = JsonValue::makeBool(m_cfg.highlightEnabled);
        root["highlightShape"]     = JsonValue::makeString(m_cfg.highlightShape);
        root["highlightColor"]     = JsonValue::makeInt(m_cfg.highlightColor);
        root["highlightSize"]      = JsonValue::makeInt(m_cfg.highlightSize);
        root["highlightCustomFile"] = JsonValue::makeString(m_cfg.highlightCustomFile);
        root["enabled"]            = JsonValue::makeBool(m_cfg.enabled);
        root["targetArea"]       = JsonValue::makeString(m_cfg.targetArea);
        root["logLevel"]         = JsonValue::makeString(m_cfg.logLevel);
        root["logFile"]          = JsonValue::makeString(m_cfg.logFile);

        auto exeProcs = JsonValue::makeArray();
        for (auto& p : m_cfg.excludedProcesses)
            exeProcs.asArray().push_back(JsonValue::makeString(p));
        root["excludedProcesses"] = exeProcs;

        auto exeClasses = JsonValue::makeArray();
        for (auto& c : m_cfg.excludedClasses)
            exeClasses.asArray().push_back(JsonValue::makeString(c));
        root["excludedClasses"] = exeClasses;

        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f << root.serialize(2);
        return true;
    } catch (...) {
        return false;
    }
}
