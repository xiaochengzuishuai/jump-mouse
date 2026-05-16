#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <format>
#include <Windows.h>

enum class LogLevel { None = 0, Error = 1, Warn = 2, Info = 3, Debug = 4 };

class Logger {
public:
    static Logger& instance() {
        static Logger s;
        return s;
    }

    void setLevel(LogLevel level) { m_level = level; }
    void setFile(const std::wstring& path) {
        std::lock_guard lock(m_mutex);
        if (m_file.is_open()) m_file.close();
        if (!path.empty()) m_file.open(path, std::ios::app);
    }

    void log(LogLevel level, const std::string& msg) {
        if (level > m_level) return;
        std::lock_guard lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::zoned_time(std::chrono::current_zone(), now);
        auto line = std::format("{:%T} [{}] {}\n", time, levelName(level), msg);
        OutputDebugStringA(line.c_str());
        if (m_file.is_open()) m_file << line << std::flush;
    }

private:
    Logger() = default;
    LogLevel m_level = LogLevel::None;
    std::mutex m_mutex;
    std::ofstream m_file;

    static const char* levelName(LogLevel lv) {
        switch (lv) {
            case LogLevel::Error: return "ERROR";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Info:  return "INFO";
            case LogLevel::Debug: return "DEBUG";
            default: return "";
        }
    }
};

#define LOG_ERROR(msg)   Logger::instance().log(LogLevel::Error, msg)
#define LOG_WARN(msg)    Logger::instance().log(LogLevel::Warn, msg)
#define LOG_INFO(msg)    Logger::instance().log(LogLevel::Info, msg)
#define LOG_DEBUG(msg)   Logger::instance().log(LogLevel::Debug, msg)

inline LogLevel levelFromString(const std::string& s) {
    if (s == "debug") return LogLevel::Debug;
    if (s == "info")  return LogLevel::Info;
    if (s == "warn")  return LogLevel::Warn;
    if (s == "error") return LogLevel::Error;
    return LogLevel::None;
}

inline std::string levelToString(LogLevel lv) {
    switch (lv) {
        case LogLevel::Debug: return "debug";
        case LogLevel::Info:  return "info";
        case LogLevel::Warn:  return "warn";
        case LogLevel::Error: return "error";
        default: return "none";
    }
}
