#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <memory>

class Logger {
public:
  // Retrieves the singleton logger instance
  static std::shared_ptr<spdlog::logger> &getLogger();

  // Initializes the logger with desired settings
  static void initialize();

  // Logging methods
  void info(const std::string &message);
  void warn(const std::string &message);
  void error(const std::string &message);
  void critical(const std::string &message);
  void debug(const std::string &message);
  void trace(const std::string &message);

private:
  Logger() = default; // Private constructor to prevent instantiation
  static std::shared_ptr<spdlog::logger> logger;
};

// Accessor for the ringbuffer sink wired into `diag_file`. The diagnostic GUI
// program reads recent records from here; nullptr until Logger::initialize().
std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> diagRingbufferSink();

#endif // LOGGER_HPP