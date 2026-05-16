#ifndef AETHERION_DIAG_THROTTLED_LOG_HPP
#define AETHERION_DIAG_THROTTLED_LOG_HPP

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <string>

namespace aetherion::diag {

// Wraps the static-clock throttle boilerplate that recurs across diagnostic
// sites. Keeps a steady_clock timestamp and fires the callable at most once
// per `interval`. The callable receives a `spdlog::logger &` so callers don't
// repeat the spdlog::get / stdout_color_mt dance.
//
// Intended as a `static` local at the callsite:
//   static ThrottledLog tlog{std::chrono::seconds(1)};
//   tlog.fire([&](spdlog::logger &log) { log.warn("..."); });
class ThrottledLog {
public:
  explicit ThrottledLog(std::chrono::steady_clock::duration interval,
                        std::string logger_name = "console")
      : interval_(interval), logger_name_(std::move(logger_name)) {}

  template <typename Fn> void fire(Fn &&fn) {
    auto now = std::chrono::steady_clock::now();
    if (now - last_ < interval_) {
      return;
    }
    last_ = now;
    auto logger = spdlog::get(logger_name_);
    if (!logger) {
      logger = spdlog::stdout_color_mt(logger_name_);
    }
    fn(*logger);
  }

private:
  std::chrono::steady_clock::duration interval_;
  std::string logger_name_;
  std::chrono::steady_clock::time_point last_{};
};

} // namespace aetherion::diag

#endif // AETHERION_DIAG_THROTTLED_LOG_HPP
