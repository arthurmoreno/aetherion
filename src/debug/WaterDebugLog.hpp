#pragma once

#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

// Watch corridor — narrow region around the active investigation. Adjust as
// needed; only voxels inside this box emit JSONL events to keep the trace
// readable. Currently scoped to the spring's vapor column for the gas-stall
// investigation.
inline bool waterDebugInWatchRegion(int x, int y, int z) {
  return x >= 48 && x <= 52 && y >= 86 && y <= 96 && z >= 0 && z <= 9;
}

// Thread-safe append of a single JSON line to water_debug.jsonl.
inline void waterDebugLog(const std::string &json_line) {
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
