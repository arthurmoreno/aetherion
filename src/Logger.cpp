#include "Logger.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

// Initialize the static logger pointer
std::shared_ptr<spdlog::logger> Logger::logger = nullptr;

// Shared ringbuffer sink for the diag_file logger so the GUI can read recent
// records without tailing the on-disk file. Capacity sized to keep the last
// few seconds of dense diagnostic output.
static std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> g_diag_ringbuffer;

std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> diagRingbufferSink() {
  return g_diag_ringbuffer;
}

void Logger::initialize() {
  if (!logger) {
    try {
      // Initialize thread pool for asynchronous logging (optional)
      size_t queue_size = 8192; // Queue size must be greater than zero
      size_t thread_count = 1;  // Number of backing threads
      spdlog::init_thread_pool(queue_size, thread_count);

      // Create a multi-sink logger (console and file)
      std::vector<spdlog::sink_ptr> sinks;

      // Colored console sink
      auto console_sink =
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_pattern(
          "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
      sinks.push_back(console_sink);

      // Rotating file sink (optional)
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          "logs/my_project.log", 1048576 * 5,
          3); // 5MB per file, 3 rotated files
      file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");
      sinks.push_back(file_sink);

      spdlog::set_level(spdlog::level::info);
      // spdlog::set_level(spdlog::level::debug);

      auto console = spdlog::stdout_color_mt("console");
      auto err_logger = spdlog::stderr_color_mt("stderr");

      // ─── Diagnostic loggers (aetherion::diag) ────────────────────────
      // diag_console — terse human-readable summaries on stdout
      auto diag_console_sink =
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      diag_console_sink->set_pattern(
          "[%Y-%m-%d %H:%M:%S.%e] [diag] [%^%l%$] %v");
      auto diag_console =
          std::make_shared<spdlog::logger>("diag_console", diag_console_sink);
      diag_console->set_level(spdlog::level::info);
      spdlog::register_logger(diag_console);

      // diag_file — JSONL-shaped records in logs/diag.log + ringbuffer for GUI
      auto diag_file_sink =
          std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
              "logs/diag.log", 1048576 * 5, 3);
      // %v is the producer-formatted body; we let the EventLogger emit a
      // single JSON object per line so consumers can parse with `jq`.
      diag_file_sink->set_pattern(
          R"({"ts":"%Y-%m-%dT%H:%M:%S.%e","level":"%l","thread":%t,%v})");

      g_diag_ringbuffer =
          std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(2048);
      g_diag_ringbuffer->set_pattern(
          R"({"ts":"%Y-%m-%dT%H:%M:%S.%e","level":"%l","thread":%t,%v})");

      std::vector<spdlog::sink_ptr> diag_sinks{diag_file_sink,
                                               g_diag_ringbuffer};
      auto diag_file = std::make_shared<spdlog::logger>(
          "diag_file", diag_sinks.begin(), diag_sinks.end());
      diag_file->set_level(spdlog::level::trace);
      diag_file->flush_on(spdlog::level::info);
      spdlog::register_logger(diag_file);

      // Create asynchronous logger with the sinks
      logger = std::make_shared<spdlog::logger>("global_logger", begin(sinks),
                                                end(sinks));
      spdlog::register_logger(logger);
      logger->set_level(spdlog::level::debug); // Set global log level
      logger->flush_on(spdlog::level::info);   // Flush on info and above

      logger->info("Logger initialized successfully.");
    } catch (const spdlog::spdlog_ex &ex) {
      std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
    }
  }
}

std::shared_ptr<spdlog::logger> &Logger::getLogger() {
  if (!logger) {
    initialize();
  }
  return logger;
}

// Logging method implementations
void Logger::info(const std::string &message) {
  if (logger) {
    logger->info(message);
  }
}

void Logger::warn(const std::string &message) {
  if (logger) {
    logger->warn(message);
  }
}

void Logger::error(const std::string &message) {
  if (logger) {
    logger->error(message);
  }
}

void Logger::critical(const std::string &message) {
  if (logger) {
    logger->critical(message);
  }
}

void Logger::debug(const std::string &message) {
  if (logger) {
    logger->debug(message);
  }
}

void Logger::trace(const std::string &message) {
  if (logger) {
    logger->trace(message);
  }
}