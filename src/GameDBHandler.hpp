#pragma once

#include <memory>  // Required for std::unique_ptr
#include <string>
#include <vector>

#include "GameDB.hpp"
#include "Logger.hpp"

class GameDBHandler {
   public:
    GameDBHandler(const std::string& sqliteFile);
    ~GameDBHandler();

    // Delete copy constructor & copy assignment to prevent copying unique_ptr
    GameDBHandler(const GameDBHandler&) = delete;
    GameDBHandler& operator=(const GameDBHandler&) = delete;

    // Allow move operations (transferring ownership)
    GameDBHandler(GameDBHandler&&) noexcept = default;
    GameDBHandler& operator=(GameDBHandler&&) noexcept = default;

    void createTables();
    void putTimeSeries(const std::string& seriesName, long long timestamp, double value);
    std::vector<std::pair<uint64_t, double>> queryTimeSeries(const std::string& seriesName,
                                                             long long start, long long end);
    void executeSQL(const std::string& sql);

    // Force database reset if necessary
    bool resetDB();

   private:
    std::string sqliteFile_;
    std::unique_ptr<GameDB> gameDB;
};