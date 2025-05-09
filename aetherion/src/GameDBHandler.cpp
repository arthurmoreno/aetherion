#include "GameDBHandler.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

GameDBHandler::GameDBHandler(const std::string& sqliteFile)
    : sqliteFile_(sqliteFile), gameDB(std::make_unique<GameDB>(sqliteFile)) {
    // Ensure tables exist
    Logger::getLogger()->debug("Creating tables in GameDBHandler constructor");
    createTables();
}

GameDBHandler::~GameDBHandler() = default;

void GameDBHandler::createTables() {
    std::string createPlayersSQL = R"(
        CREATE TABLE IF NOT EXISTS players (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            password_hash TEXT NOT NULL
        );
    )";

    std::string createTimeSeriesSQL = R"(
        CREATE TABLE IF NOT EXISTS time_series (
            series_name TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            value REAL NOT NULL,
            PRIMARY KEY(series_name, timestamp)
        );
    )";

    gameDB->executeSQL(createPlayersSQL);
    gameDB->executeSQL(createTimeSeriesSQL);
}

void GameDBHandler::putTimeSeries(const std::string& seriesName, long long timestamp,
                                  double value) {
    Logger::getLogger()->info(
        "[GameDBHandler::putTimeSeries] Called with seriesName={}, timestamp={}, value={}",
        seriesName, timestamp, value);

    // Convert timestamp to uint64_t for storage
    uint64_t ts = static_cast<uint64_t>(timestamp);

    // Store directly in our improved GameDB
    if (!gameDB->putTimeSeries(seriesName, ts, value)) {
        Logger::getLogger()->error("Failed to store time series data");
    }
}

std::vector<std::pair<uint64_t, double>> GameDBHandler::queryTimeSeries(
    const std::string& seriesName, long long start, long long end) {
    Logger::getLogger()->info(
        "[GameDBHandler::queryTimeSeries] Called with seriesName={}, start={}, end={}", seriesName,
        start, end);

    // Convert to uint64_t
    uint64_t startTime = static_cast<uint64_t>(start);
    uint64_t endTime = static_cast<uint64_t>(end);

    // Query from our improved GameDB
    auto results = gameDB->queryTimeSeries(seriesName, startTime, endTime);

    Logger::getLogger()->info("Found {} results in time series query for {}", results.size(),
                              seriesName);
    return results;
}

void GameDBHandler::executeSQL(const std::string& sql) { gameDB->executeSQL(sql); }

bool GameDBHandler::resetDB() {
    Logger::getLogger()->warn("[GameDBHandler::resetDB] Resetting database");
    return gameDB->resetDB();
}