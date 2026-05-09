#pragma once

#include <sqlite3.h>

#include <cstddef>
#include <entt/entt.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Logger.hpp"
#include "components/TimeSeriesComponent.hpp"

/**
 * @brief Database handler for game data
 *
 * Provides persistent storage using SQLite and in-memory cache using EnTT
 */
class GameDB {
public:
  /**
   * @brief Constructor for GameDB
   *
   * @param sqlite_path Path to the SQLite database file
   */
  GameDB(const std::string &sqlite_path);

  /**
   * @brief Destructor for GameDB
   */
  ~GameDB();

  /**
   * @brief Store a time series data point
   *
   * @param seriesName Name of the time series
   * @param timestamp Time point for the data point
   * @param value Value to store
   * @return bool Success status
   */
  bool putTimeSeries(const std::string &seriesName, uint64_t timestamp,
                     double value);

  /**
   * @brief Query time series data within a time range
   *
   * @param seriesName Name of the time series
   * @param start_time Start timestamp for the query range
   * @param end_time End timestamp for the query range
   * @return std::vector<std::pair<uint64_t, double>> Vector of timestamp-value
   * pairs
   */
  std::vector<std::pair<uint64_t, double>>
  queryTimeSeries(const std::string &seriesName, uint64_t start_time,
                  uint64_t end_time);

  /**
   * @brief Execute an arbitrary SQL query
   *
   * @param query SQL query string
   * @return bool Success status
   */
  bool executeSQL(const std::string &query);

  /**
   * @brief Create database tables
   *
   * @return bool Success status
   */
  bool createTables();

  /**
   * @brief Reset the database
   *
   * @return bool Success status
   */
  bool resetDB();

  /**
   * @brief Sync in-memory data to SQLite
   *
   * @return bool Success status
   */
  bool syncToDatabase();

  /**
   * @brief Load data from SQLite to in-memory cache
   *
   * @return bool Success status
   */
  bool loadFromDatabase();

  /**
   * @brief Find the in-memory TimeSeriesComponent for a series name.
   *
   * Test-facing accessor used to peek at the cache without going
   * through the query path. Returns nullptr if no component exists for
   * the given name.
   */
  const TimeSeriesComponent *
  findTimeSeriesComponent(const std::string &seriesName) const;

  /**
   * @brief Count rows on disk for a series. Bypasses the in-memory
   * cache by running a raw SQLite COUNT(*). Returns -1 on SQL error.
   * Test-facing accessor used to verify the trim trigger.
   */
  long long countOnDiskRows(const std::string &seriesName) const;

private:
  /**
   * @brief Validate and fix time_series table schema
   */
  void validateTimeSeriesSchema();

  /**
   * @brief Insert a single (series, ts, value) row directly. Used by
   * the hot putTimeSeries path so we don't replay the entire cache on
   * every call (which was the original O(N) per-put behaviour that
   * spammed the log and froze the game).
   */
  bool insertSinglePoint(const std::string &seriesName, uint64_t timestamp,
                         double value);

  /**
   * @brief Trim a single series on disk to at most kMaxOnDiskRowsPerSeries
   * rows by deleting the oldest. Cheaper than the per-INSERT trigger
   * because it runs at most once per `kInsertsPerTrim` puts to a series.
   */
  void trimSeriesOnDisk(const std::string &seriesName);

  // SQLite database
  std::string sqlitePath;
  sqlite3 *sqliteDb;

  // EnTT registry for in-memory components
  entt::registry registry;

  // Flag to track if in-memory data needs to be synced
  bool needsSync;

  // Per-series put counter — used to amortise the disk-trim work over
  // many inserts instead of paying it per row.
  std::unordered_map<std::string, std::size_t> insertsSinceTrim_;

  // Entity used for time series storage
  entt::entity timeSeriesEntity;
};