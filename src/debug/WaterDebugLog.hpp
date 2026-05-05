#pragma once

#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

// ────────────────────────────────────────────────────────────────────────────
// Master switch — single compile-time toggle for the entire water debugger.
//
// When false (default), every helper below short-circuits to a no-op AND every
// per-category flag is forced false at compile time, so all gated call sites
// in the codebase compile to nothing. Zero runtime overhead, zero JSONL file
// creation, zero file I/O.
//
// Flip to true to re-arm the debugger; use the per-category flags below to
// scope the active probe families. Toggle deliberately and remember to flip
// back to false before shipping a build.
// ────────────────────────────────────────────────────────────────────────────
inline constexpr bool kWaterDebugEnabled = false;

// Watch corridor — narrow region around the active investigation. Adjust as
// needed; only voxels inside this box emit JSONL events to keep the trace
// readable. Currently scoped to the evaporation/condensation column for the
// single-threaded segfault investigation (x > 80, 30 < y < 70, full z).
inline bool waterDebugInWatchRegion(int x, int y, int z) {
  (void)z; // keep z unrestricted for now
  return x >= 81 && y >= 31 && y <= 69;
}

// Per-category gates — composed with the master so disabling kWaterDebugEnabled
// forces every category false at compile time. Each category is the AND of
// the master and the per-category intent. To narrow what gets logged when the
// debugger is on, flip individual categories without touching the master.
//
// No active investigation. Flip the master and one or more categories below
// to re-arm. Categories are AND-ed with the master so disabling
// `kWaterDebugEnabled` forces every category false at compile time.
inline constexpr bool kWaterDebugTrackTickPhases =
    kWaterDebugEnabled && false; // World::update phase boundaries
inline constexpr bool kWaterDebugTrackEvapCondense =
    kWaterDebugEnabled && false;
inline constexpr bool kWaterDebugTrackHandlers =
    kWaterDebugEnabled && false; // PhysicsEngine::on*Event entry/exit
inline constexpr bool kWaterDebugTrackWaterSpreadSteps =
    kWaterDebugEnabled && false;
inline constexpr bool kWaterDebugTrackVaporMovement =
    kWaterDebugEnabled && false; // V3/V4 stall
inline constexpr bool kWaterDebugTrackVaporMergeSideways =
    kWaterDebugEnabled && false;
inline constexpr bool kWaterDebugTrackVelocityMove =
    kWaterDebugEnabled && false; // water flow

// Thread-safe append of a single JSON line to water_debug.jsonl.
//
// Master-flag gate at the top: with kWaterDebugEnabled == false, the function
// returns before ever touching the static ofstream / mutex, so the file is
// never opened and no I/O happens. Compilers fully elide the body via DCE.
inline void waterDebugLog(const std::string &json_line) {
  if (!kWaterDebugEnabled)
    return;
  static std::mutex mtx;
  static std::ofstream ofs("water_debug.jsonl", std::ios::app);
  std::lock_guard<std::mutex> guard(mtx);
  ofs << json_line << "\n";
  ofs.flush();
}

inline std::string waterDebugThreadId() {
  std::ostringstream oss;
  oss << std::this_thread::get_id();
  return oss.str();
}

// Tick-phase marker — no coord filter; emits one JSONL line per phase boundary.
// The trailing tick counter is appended by the caller for ordering across
// ticks.
inline void waterDebugLogPhase(const char *phase, const char *boundary,
                               long long tick) {
  if (!kWaterDebugTrackTickPhases)
    return;
  std::ostringstream jss;
  jss << "{\"event\":\"phase\"" << ",\"phase\":\"" << phase << "\""
      << ",\"boundary\":\"" << boundary << "\",\"tick\":" << tick
      << ",\"thread\":\"" << waterDebugThreadId() << "\"}";
  waterDebugLog(jss.str());
}

// Per-handler ENTRY/EXIT marker — no coord filter; one JSONL line per call.
// `handler` is the event type short name (e.g. "WaterFallEntity"); `boundary`
// is "begin" or "end". After a crash, the last `handler ... begin` without a
// matching `end` identifies the firing event type.
inline void waterDebugLogHandler(const char *handler, const char *boundary) {
  if (!kWaterDebugTrackHandlers)
    return;
  std::ostringstream jss;
  jss << "{\"event\":\"handler\"" << ",\"handler\":\"" << handler << "\""
      << ",\"boundary\":\"" << boundary << "\"" << ",\"thread\":\""
      << waterDebugThreadId() << "\"}";
  waterDebugLog(jss.str());
}

// RAII scope guard — drop one of these at the top of an event handler and the
// destructor logs the `end` marker on every return path, throw, or normal
// fall-through. A SIGSEGV mid-handler skips the destructor → the trailing
// `end` is missing → that's the smoking gun in the trace.
//
// Both ctor and dtor short-circuit when kWaterDebugTrackHandlers is false (and
// therefore also when kWaterDebugEnabled is false), so the class becomes a
// pair of no-op stack frames the compiler can elide entirely.
class WaterDebugHandlerScope {
public:
  explicit WaterDebugHandlerScope(const char *name) : name_(name) {
    if (!kWaterDebugTrackHandlers)
      return;
    waterDebugLogHandler(name_, "begin");
  }
  ~WaterDebugHandlerScope() {
    if (!kWaterDebugTrackHandlers)
      return;
    waterDebugLogHandler(name_, "end");
  }
  WaterDebugHandlerScope(const WaterDebugHandlerScope &) = delete;
  WaterDebugHandlerScope &operator=(const WaterDebugHandlerScope &) = delete;

private:
  const char *name_;
};
