#include "ConsoleLogsComponent.hpp"

// Add a new log entry with the current timestamp
void ConsoleLogsComponent::add_log(const std::string& log) {
    std::string timestamp = current_time_string();
    log_buffer[timestamp] = log;

    // Ensure the buffer does not exceed max_size
    if (log_buffer.size() > max_size) {
        log_buffer.erase(log_buffer.begin());
    }
}

// Retrieve the current buffer of logs
std::map<std::string, std::string> ConsoleLogsComponent::get_logs() const { return log_buffer; }