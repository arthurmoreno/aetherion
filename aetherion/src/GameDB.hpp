#pragma once

#include <sqlite3.h>

#include <entt/entt.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
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
    bool putTimeSeries(const std::string &seriesName, uint64_t timestamp, double value);

    /**
     * @brief Query time series data within a time range
     *
     * @param seriesName Name of the time series
     * @param start_time Start timestamp for the query range
     * @param end_time End timestamp for the query range
     * @return std::vector<std::pair<uint64_t, double>> Vector of timestamp-value pairs
     */
    std::vector<std::pair<uint64_t, double>> queryTimeSeries(const std::string &seriesName,
                                                             uint64_t start_time,
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

   private:
    // SQLite database
    std::string sqlitePath;
    sqlite3 *sqliteDb;

    // EnTT registry for in-memory components
    entt::registry registry;

    // Flag to track if in-memory data needs to be synced
    bool needsSync;

    // Entity used for time series storage
    entt::entity timeSeriesEntity;
};