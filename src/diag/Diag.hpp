#ifndef AETHERION_DIAG_DIAG_HPP
#define AETHERION_DIAG_DIAG_HPP

// Aetherion diagnostic module — a thin wrapper that gives producers four
// typed handles (Counter, Gauge, EventLogger; Histogram in v2) and a central
// Registry that owns cadence/sink configuration. See
// `.claude/docs/epics-plans/2026-05-08-unified-diagnostic-logging-cpp.md`.

#include <spdlog/common.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class GameDBHandler;

namespace aetherion::diag {

// Compile-time master switch. With `kEnabled = false`, every emit body is
// short-circuited and DCE removes it — same discipline as
// `kWaterDebugEnabled` in `src/debug/WaterDebugLog.hpp`.
inline constexpr bool kEnabled = true;

enum class AggFn { Sum, Last, Mean, Min, Max };

// ─── Sink configuration values ─────────────────────────────────────────
// Producers pass these by value at registration time; the Registry
// materialises the actual sink objects internally.

struct GameDBSink {}; // → handler->putTimeSeries

struct SpdlogSink {
  std::string logger_name = "diag_file";
  spdlog::level::level_enum level = spdlog::level::info;
};

using SinkVariant = std::variant<GameDBSink, SpdlogSink>;

// ─── Configuration types ──────────────────────────────────────────────

struct CounterConfig {
  std::string name;
  std::string description;
  std::string unit = "events";
  std::chrono::milliseconds flush_every{1000};
  std::vector<SinkVariant> sinks;
};

struct GaugeConfig {
  std::string name;
  std::string description;
  std::string unit;
  AggFn aggregation = AggFn::Last;
  std::chrono::milliseconds flush_every{1000};
  std::vector<SinkVariant> sinks;
};

struct EventConfig {
  std::string channel;
  std::vector<std::string> required_keys;
  spdlog::level::level_enum default_level = spdlog::level::info;
  std::vector<SinkVariant> sinks;
};

// ─── Handle types ─────────────────────────────────────────────────────
// All cheap shared_ptr to PIMPL state. Default-constructed handles are
// inert (enabled() == false) so engines can hold them as members without
// requiring two-phase init.

// The Impl structs are forward-declared here and defined only in
// Diag.cpp, so the layout stays hidden. They're nested inside the public
// section so internal Registry bookkeeping (CounterEntry / GaugeEntry /
// EventEntry in Diag.cpp) can hold `std::shared_ptr<Counter::Impl>`
// without needing extra friend declarations.

class Counter {
public:
  struct Impl;

  Counter() = default;
  void inc(std::uint64_t delta = 1) noexcept;
  bool enabled() const noexcept;

private:
  friend class Registry;
  std::shared_ptr<Impl> impl_;
};

class Gauge {
public:
  struct Impl;

  Gauge() = default;
  void set(double value) noexcept;
  bool enabled() const noexcept;

private:
  friend class Registry;
  std::shared_ptr<Impl> impl_;
};

class EventLogger {
public:
  struct Impl;

  EventLogger() = default;
  void log(const nlohmann::json &payload);
  void log(const nlohmann::json &payload, spdlog::level::level_enum level);
  bool enabled() const noexcept;

private:
  friend class Registry;
  std::shared_ptr<Impl> impl_;
};

// ─── Registry ─────────────────────────────────────────────────────────
class Registry {
public:
  struct InitOptions {
    GameDBHandler *gamedb_handler = nullptr;
    std::string session_id;
  };

  static Registry &instance();

  void initialize(const InitOptions &opts);
  void shutdown();

  // Drop all registrations + clear init state. Test helper; not for
  // production use (would invalidate every outstanding handle).
  void reset_for_testing();

  Counter counter(const CounterConfig &cfg);
  Gauge gauge(const GaugeConfig &cfg);
  EventLogger event(const EventConfig &cfg);

  // Driven from the engine's main update loop. Walks every registered
  // metric, flushes any whose `flush_every` window has elapsed since the
  // previous flush.
  void tick();

  // Forced flush of every registered metric — call on graceful shutdown.
  void flush_all();

  // Runtime gating. Names support a trailing `*` glob (e.g. "water_sim.*").
  void enable(std::string_view name_or_glob);
  void disable(std::string_view name_or_glob);
  bool is_enabled(std::string_view name) const;

  // Test-only inspection: read the live accumulator without flushing.
  // Returns 0 if the counter doesn't exist or is disabled.
  std::uint64_t peek_counter(std::string_view name) const;

private:
  Registry();
  ~Registry();
  Registry(const Registry &) = delete;
  Registry &operator=(const Registry &) = delete;

  struct State;
  std::unique_ptr<State> state_;
};

} // namespace aetherion::diag

#endif // AETHERION_DIAG_DIAG_HPP
