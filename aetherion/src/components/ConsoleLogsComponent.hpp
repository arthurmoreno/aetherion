#ifndef CONSOLE_LOGS_COMPONENT_H
#define CONSOLE_LOGS_COMPONENT_H

#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <ylt/struct_pack.hpp>

struct ConsoleLogsComponent {
    std::map<std::string, std::string> log_buffer;
    uint32_t max_size;

    // Helper function to get current time as string
    std::string current_time_string() const {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &now_time_t);
#else
        localtime_r(&now_time_t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    // Add a new log entry
    void add_log(const std::string& log);

    // Retrieve the current buffer of logs
    std::map<std::string, std::string> get_logs() const;
};

#endif  // CONSOLE_LOGS_COMPONENT_H