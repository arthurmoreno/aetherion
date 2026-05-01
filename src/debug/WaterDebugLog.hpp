#pragma once

#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

// Watch corridor matching the known crash coordinates.
inline bool waterDebugInWatchRegion(int x, int y, int z) {
  return x >= 88 && x <= 99 && y >= 49 && y <= 51 && z >= 0 && z <= 2;
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
