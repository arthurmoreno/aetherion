#ifndef AETHERION_DIAG_SINK_HPP
#define AETHERION_DIAG_SINK_HPP

#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/common.h>
#include <string>

#include "diag/Diag.hpp"

class GameDBHandler;

namespace aetherion::diag {

struct MetricSample {
  std::string name;
  std::chrono::system_clock::time_point ts;
  double value;
  AggFn applied_aggregation = AggFn::Sum;
};

struct EventRecord {
  std::string channel;
  std::chrono::system_clock::time_point ts;
  spdlog::level::level_enum level;
  nlohmann::json payload;
};

class Sink {
public:
  virtual ~Sink() = default;
  virtual void write_metric(const MetricSample &) = 0;
  virtual void write_event(const EventRecord &) = 0;
  virtual void flush() = 0;
};

// Build a concrete Sink for a SinkVariant. Returns nullptr if the variant
// resolves to a sink that needs a resource which wasn't provided
// (e.g. GameDBSink without a handler) — caller logs the warning and the
// metric quietly drops samples for that sink.
std::unique_ptr<Sink> makeSink(const SinkVariant &cfg, GameDBHandler *db);

} // namespace aetherion::diag

#endif // AETHERION_DIAG_SINK_HPP
