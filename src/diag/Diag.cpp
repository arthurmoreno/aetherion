#include "diag/Diag.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "GameDBHandler.hpp"
#include "diag/Sink.hpp"

namespace aetherion::diag {

namespace {

bool matchesGlob(std::string_view pattern, std::string_view name) {
  // Trailing-`*` glob, plus exact match. Sufficient for the
  // "water_sim.*" / "physics.move_solid_entity" cases the plan calls out;
  // intentionally simple to avoid pulling in <regex>.
  if (!pattern.empty() && pattern.back() == '*') {
    auto prefix = pattern.substr(0, pattern.size() - 1);
    return name.size() >= prefix.size() &&
           name.compare(0, prefix.size(), prefix) == 0;
  }
  return pattern == name;
}

std::string formatJsonBody(const nlohmann::json &payload) {
  // Render the payload as the *interior* of a JSON object so the spdlog
  // pattern `{"ts":"...","level":"...",%v}` produces a single valid JSON
  // line per record.
  std::string dumped = payload.dump();
  if (dumped.size() >= 2 && dumped.front() == '{' && dumped.back() == '}') {
    return dumped.substr(1, dumped.size() - 2);
  }
  // Non-object payloads (rare; the API takes a json by value) get wrapped
  // under a "payload" key so the line still parses.
  return std::string("\"payload\":") + dumped;
}

} // namespace

// ─── Concrete sinks ───────────────────────────────────────────────────

namespace {

class GameDBSinkImpl final : public Sink {
public:
  explicit GameDBSinkImpl(GameDBHandler *db) : db_(db) {}

  void write_metric(const MetricSample &s) override {
    if (!db_) {
      return;
    }
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                    s.ts.time_since_epoch())
                    .count();
    db_->putTimeSeries(s.name, static_cast<long long>(secs), s.value);
  }

  void write_event(const EventRecord &) override {
    // Events are spdlog-only; no-op here so a Tee'd Counter can write to
    // GameDB while events route to spdlog without complaint.
  }

  void flush() override {}

private:
  GameDBHandler *db_;
};

class SpdlogSinkImpl final : public Sink {
public:
  explicit SpdlogSinkImpl(SpdlogSink cfg) : cfg_(std::move(cfg)) {
    logger_ = spdlog::get(cfg_.logger_name);
  }

  void write_metric(const MetricSample &s) override {
    if (!ensureLogger()) {
      return;
    }
    // Render as a JSON-shaped body so it lines up with how events look.
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                    s.ts.time_since_epoch())
                    .count();
    logger_->log(cfg_.level,
                 R"("kind":"metric","name":"{}","value":{},"ts_s":{})", s.name,
                 s.value, static_cast<long long>(secs));
  }

  void write_event(const EventRecord &e) override {
    if (!ensureLogger()) {
      return;
    }
    logger_->log(e.level, R"("kind":"event","channel":"{}",{})", e.channel,
                 formatJsonBody(e.payload));
  }

  void flush() override {
    if (logger_) {
      logger_->flush();
    }
  }

private:
  bool ensureLogger() {
    if (!logger_) {
      logger_ = spdlog::get(cfg_.logger_name);
    }
    return static_cast<bool>(logger_);
  }

  SpdlogSink cfg_;
  std::shared_ptr<spdlog::logger> logger_;
};

} // namespace

std::unique_ptr<Sink> makeSink(const SinkVariant &cfg, GameDBHandler *db) {
  return std::visit(
      [db](const auto &v) -> std::unique_ptr<Sink> {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, GameDBSink>) {
          if (db == nullptr) {
            // Caller logged the warn at registration time; return null so
            // tick() skips this sink.
            return nullptr;
          }
          return std::make_unique<GameDBSinkImpl>(db);
        } else if constexpr (std::is_same_v<T, SpdlogSink>) {
          return std::make_unique<SpdlogSinkImpl>(v);
        }
        return nullptr;
      },
      cfg);
}

// ─── Handle implementations ───────────────────────────────────────────

struct Counter::Impl {
  std::string name;
  std::atomic<bool> active{true};
  std::atomic<std::uint64_t> value{0};

  std::uint64_t snapshot_and_reset() noexcept {
    return value.exchange(0, std::memory_order_acq_rel);
  }

  std::uint64_t peek() const noexcept {
    return value.load(std::memory_order_acquire);
  }
};

void Counter::inc(std::uint64_t delta) noexcept {
  if constexpr (!kEnabled) {
    return;
  }
  if (!impl_) {
    return;
  }
  if (!impl_->active.load(std::memory_order_relaxed)) {
    return;
  }
  impl_->value.fetch_add(delta, std::memory_order_relaxed);
}

bool Counter::enabled() const noexcept {
  return impl_ && impl_->active.load(std::memory_order_relaxed);
}

struct Gauge::Impl {
  std::string name;
  AggFn aggregation = AggFn::Last;
  std::atomic<bool> active{true};

  // Single mutex guards the accumulator. Gauge is occasional (entity
  // counts, queue depths, etc.) — not on the same hot path as Counter.
  mutable std::mutex mu;
  bool has_value = false;
  double last = 0.0;
  double sum = 0.0;
  double min = 0.0;
  double max = 0.0;
  std::uint64_t count = 0;

  void set(double v) {
    std::lock_guard<std::mutex> lk(mu);
    if (!has_value) {
      min = v;
      max = v;
      has_value = true;
    } else {
      if (v < min)
        min = v;
      if (v > max)
        max = v;
    }
    last = v;
    sum += v;
    ++count;
  }

  // Returns (value, has_value). Resets accumulator.
  std::pair<double, bool> snapshot_and_reset() {
    std::lock_guard<std::mutex> lk(mu);
    if (!has_value) {
      return {0.0, false};
    }
    double out = 0.0;
    switch (aggregation) {
    case AggFn::Last:
      out = last;
      break;
    case AggFn::Sum:
      out = sum;
      break;
    case AggFn::Mean:
      out = (count > 0) ? (sum / static_cast<double>(count)) : 0.0;
      break;
    case AggFn::Min:
      out = min;
      break;
    case AggFn::Max:
      out = max;
      break;
    }
    has_value = false;
    last = 0.0;
    sum = 0.0;
    min = 0.0;
    max = 0.0;
    count = 0;
    return {out, true};
  }
};

void Gauge::set(double value) noexcept {
  if constexpr (!kEnabled) {
    return;
  }
  if (!impl_) {
    return;
  }
  if (!impl_->active.load(std::memory_order_relaxed)) {
    return;
  }
  try {
    impl_->set(value);
  } catch (...) {
    // mutex lock failure shouldn't reach the hot path; swallow to keep
    // the API noexcept.
  }
}

bool Gauge::enabled() const noexcept {
  return impl_ && impl_->active.load(std::memory_order_relaxed);
}

struct EventLogger::Impl {
  std::string channel;
  std::vector<std::string> required_keys;
  spdlog::level::level_enum default_level = spdlog::level::info;
  std::vector<std::unique_ptr<Sink>> sinks;
  std::atomic<bool> active{true};
  std::atomic<bool> warned_missing_keys{false};

  void emit(const nlohmann::json &payload, spdlog::level::level_enum level) {
    if (!active.load(std::memory_order_relaxed)) {
      return;
    }
    if (!required_keys.empty() && payload.is_object()) {
      for (const auto &key : required_keys) {
        if (!payload.contains(key)) {
          if (!warned_missing_keys.exchange(true)) {
            spdlog::warn("diag::event '{}' missing required key '{}'", channel,
                         key);
          }
          break;
        }
      }
    }
    EventRecord rec{channel, std::chrono::system_clock::now(), level, payload};
    for (auto &s : sinks) {
      if (s) {
        s->write_event(rec);
      }
    }
  }
};

void EventLogger::log(const nlohmann::json &payload) {
  if constexpr (!kEnabled) {
    return;
  }
  if (!impl_) {
    return;
  }
  impl_->emit(payload, impl_->default_level);
}

void EventLogger::log(const nlohmann::json &payload,
                      spdlog::level::level_enum level) {
  if constexpr (!kEnabled) {
    return;
  }
  if (!impl_) {
    return;
  }
  impl_->emit(payload, level);
}

bool EventLogger::enabled() const noexcept {
  return impl_ && impl_->active.load(std::memory_order_relaxed);
}

// ─── Registry ─────────────────────────────────────────────────────────

namespace {

struct CounterEntry {
  std::shared_ptr<Counter::Impl> impl;
  std::vector<std::unique_ptr<Sink>> sinks;
  std::chrono::milliseconds flush_every{1000};
  std::chrono::steady_clock::time_point next_flush;
};

struct GaugeEntry {
  std::shared_ptr<Gauge::Impl> impl;
  std::vector<std::unique_ptr<Sink>> sinks;
  std::chrono::milliseconds flush_every{1000};
  std::chrono::steady_clock::time_point next_flush;
};

struct EventEntry {
  std::shared_ptr<EventLogger::Impl> impl;
};

} // namespace

struct Registry::State {
  std::mutex mu; // Guards registration, tick, enable/disable.
  Registry::InitOptions opts;
  bool initialized = false;
  bool warned_missing_db = false;

  std::unordered_map<std::string, CounterEntry> counters;
  std::unordered_map<std::string, GaugeEntry> gauges;
  std::unordered_map<std::string, EventEntry> events;

  // Names disabled before registration so a late-registered handle still
  // honours the gate.
  std::unordered_set<std::string> disable_globs;

  bool isDisabledByGlob(const std::string &name) const {
    for (const auto &g : disable_globs) {
      if (matchesGlob(g, name)) {
        return true;
      }
    }
    return false;
  }

  std::vector<std::unique_ptr<Sink>>
  buildSinks(const std::vector<SinkVariant> &cfgs, const std::string &name) {
    std::vector<std::unique_ptr<Sink>> out;
    out.reserve(cfgs.size());
    for (const auto &cfg : cfgs) {
      auto sink = makeSink(cfg, opts.gamedb_handler);
      if (sink == nullptr && std::holds_alternative<GameDBSink>(cfg) &&
          opts.gamedb_handler == nullptr && !warned_missing_db) {
        spdlog::warn("diag: GameDBSink registered for '{}' but Registry "
                     "initialised without a GameDBHandler — dropping samples",
                     name);
        warned_missing_db = true;
      }
      out.push_back(std::move(sink));
    }
    return out;
  }
};

Registry::Registry() : state_(std::make_unique<State>()) {}
Registry::~Registry() = default;

Registry &Registry::instance() {
  static Registry r;
  return r;
}

void Registry::initialize(const InitOptions &opts) {
  std::lock_guard<std::mutex> lk(state_->mu);
  state_->opts = opts;
  state_->initialized = true;
  state_->warned_missing_db = false;
}

void Registry::shutdown() {
  flush_all();
  std::lock_guard<std::mutex> lk(state_->mu);
  state_->initialized = false;
}

void Registry::reset_for_testing() {
  std::lock_guard<std::mutex> lk(state_->mu);
  state_->counters.clear();
  state_->gauges.clear();
  state_->events.clear();
  state_->disable_globs.clear();
  state_->opts = InitOptions{};
  state_->initialized = false;
  state_->warned_missing_db = false;
}

Counter Registry::counter(const CounterConfig &cfg) {
  std::lock_guard<std::mutex> lk(state_->mu);
  if (state_->counters.count(cfg.name) || state_->gauges.count(cfg.name)) {
    throw std::runtime_error("diag::Registry::counter: duplicate name '" +
                             cfg.name + "'");
  }
  auto impl = std::make_shared<Counter::Impl>();
  impl->name = cfg.name;
  if (state_->isDisabledByGlob(cfg.name)) {
    impl->active.store(false);
  }
  CounterEntry entry;
  entry.impl = impl;
  entry.sinks = state_->buildSinks(cfg.sinks, cfg.name);
  entry.flush_every = cfg.flush_every;
  entry.next_flush = std::chrono::steady_clock::now() + cfg.flush_every;
  state_->counters.emplace(cfg.name, std::move(entry));
  Counter c;
  c.impl_ = std::move(impl);
  return c;
}

Gauge Registry::gauge(const GaugeConfig &cfg) {
  std::lock_guard<std::mutex> lk(state_->mu);
  if (state_->counters.count(cfg.name) || state_->gauges.count(cfg.name)) {
    throw std::runtime_error("diag::Registry::gauge: duplicate name '" +
                             cfg.name + "'");
  }
  auto impl = std::make_shared<Gauge::Impl>();
  impl->name = cfg.name;
  impl->aggregation = cfg.aggregation;
  if (state_->isDisabledByGlob(cfg.name)) {
    impl->active.store(false);
  }
  GaugeEntry entry;
  entry.impl = impl;
  entry.sinks = state_->buildSinks(cfg.sinks, cfg.name);
  entry.flush_every = cfg.flush_every;
  entry.next_flush = std::chrono::steady_clock::now() + cfg.flush_every;
  state_->gauges.emplace(cfg.name, std::move(entry));
  Gauge g;
  g.impl_ = std::move(impl);
  return g;
}

EventLogger Registry::event(const EventConfig &cfg) {
  std::lock_guard<std::mutex> lk(state_->mu);
  if (state_->events.count(cfg.channel)) {
    throw std::runtime_error("diag::Registry::event: duplicate channel '" +
                             cfg.channel + "'");
  }
  auto impl = std::make_shared<EventLogger::Impl>();
  impl->channel = cfg.channel;
  impl->required_keys = cfg.required_keys;
  impl->default_level = cfg.default_level;
  impl->sinks = state_->buildSinks(cfg.sinks, cfg.channel);
  if (state_->isDisabledByGlob(cfg.channel)) {
    impl->active.store(false);
  }
  EventEntry entry;
  entry.impl = impl;
  state_->events.emplace(cfg.channel, std::move(entry));
  EventLogger e;
  e.impl_ = std::move(impl);
  return e;
}

void Registry::tick() {
  if constexpr (!kEnabled) {
    return;
  }
  std::lock_guard<std::mutex> lk(state_->mu);
  auto now = std::chrono::steady_clock::now();
  auto wall_now = std::chrono::system_clock::now();

  for (auto &[name, entry] : state_->counters) {
    if (now < entry.next_flush) {
      continue;
    }
    auto value = entry.impl->snapshot_and_reset();
    MetricSample sample{name, wall_now, static_cast<double>(value), AggFn::Sum};
    for (auto &s : entry.sinks) {
      if (s) {
        s->write_metric(sample);
      }
    }
    entry.next_flush = now + entry.flush_every;
  }

  for (auto &[name, entry] : state_->gauges) {
    if (now < entry.next_flush) {
      continue;
    }
    auto [value, present] = entry.impl->snapshot_and_reset();
    if (present) {
      MetricSample sample{name, wall_now, value, entry.impl->aggregation};
      for (auto &s : entry.sinks) {
        if (s) {
          s->write_metric(sample);
        }
      }
    }
    entry.next_flush = now + entry.flush_every;
  }
}

void Registry::flush_all() {
  std::lock_guard<std::mutex> lk(state_->mu);
  auto now = std::chrono::system_clock::now();
  auto steady_now = std::chrono::steady_clock::now();

  for (auto &[name, entry] : state_->counters) {
    auto value = entry.impl->snapshot_and_reset();
    MetricSample sample{name, now, static_cast<double>(value), AggFn::Sum};
    for (auto &s : entry.sinks) {
      if (s) {
        s->write_metric(sample);
        s->flush();
      }
    }
    entry.next_flush = steady_now + entry.flush_every;
  }
  for (auto &[name, entry] : state_->gauges) {
    auto [value, present] = entry.impl->snapshot_and_reset();
    if (present) {
      MetricSample sample{name, now, value, entry.impl->aggregation};
      for (auto &s : entry.sinks) {
        if (s) {
          s->write_metric(sample);
          s->flush();
        }
      }
    }
    entry.next_flush = steady_now + entry.flush_every;
  }
}

void Registry::enable(std::string_view name_or_glob) {
  std::lock_guard<std::mutex> lk(state_->mu);
  std::string key(name_or_glob);
  state_->disable_globs.erase(key);
  for (auto &[name, entry] : state_->counters) {
    if (matchesGlob(name_or_glob, name)) {
      entry.impl->active.store(true);
    }
  }
  for (auto &[name, entry] : state_->gauges) {
    if (matchesGlob(name_or_glob, name)) {
      entry.impl->active.store(true);
    }
  }
  for (auto &[name, entry] : state_->events) {
    if (matchesGlob(name_or_glob, name)) {
      entry.impl->active.store(true);
    }
  }
}

void Registry::disable(std::string_view name_or_glob) {
  std::lock_guard<std::mutex> lk(state_->mu);
  state_->disable_globs.emplace(name_or_glob);
  for (auto &[name, entry] : state_->counters) {
    if (matchesGlob(name_or_glob, name)) {
      entry.impl->active.store(false);
    }
  }
  for (auto &[name, entry] : state_->gauges) {
    if (matchesGlob(name_or_glob, name)) {
      entry.impl->active.store(false);
    }
  }
  for (auto &[name, entry] : state_->events) {
    if (matchesGlob(name_or_glob, name)) {
      entry.impl->active.store(false);
    }
  }
}

bool Registry::is_enabled(std::string_view name) const {
  std::lock_guard<std::mutex> lk(state_->mu);
  std::string key(name);
  if (auto it = state_->counters.find(key); it != state_->counters.end()) {
    return it->second.impl->active.load();
  }
  if (auto it = state_->gauges.find(key); it != state_->gauges.end()) {
    return it->second.impl->active.load();
  }
  if (auto it = state_->events.find(key); it != state_->events.end()) {
    return it->second.impl->active.load();
  }
  return false;
}

std::uint64_t Registry::peek_counter(std::string_view name) const {
  std::lock_guard<std::mutex> lk(state_->mu);
  std::string key(name);
  if (auto it = state_->counters.find(key); it != state_->counters.end()) {
    return it->second.impl->peek();
  }
  return 0;
}

} // namespace aetherion::diag
