#include "GameDB.hpp"

#include <chrono>
#include <filesystem>

GameDB::GameDB(const std::string& sqlite_path)
    : sqlitePath(sqlite_path), sqliteDb(nullptr), needsSync(false) {
    // Create directory for the SQLite database if it doesn't exist
    std::filesystem::create_directories(std::filesystem::path(sqlite_path).parent_path());

    // Initialize SQLite
    if (sqlite3_open(sqlite_path.c_str(), &sqliteDb) != SQLITE_OK) {
        std::string errorMsg = sqlite3_errmsg(sqliteDb);
        Logger::getLogger()->error("Error opening SQLite DB: {}", errorMsg);
        sqliteDb = nullptr;
        throw std::runtime_error("Failed to open SQLite database: " + errorMsg);
    } else {
        Logger::getLogger()->info("SQLite DB opened at: {}", sqlite_path);
    }

    // Create tables if they don't exist
    createTables();

    // Load existing time series data from database
    loadFromDatabase();
}

GameDB::~GameDB() {
    // Sync any unsaved data
    if (needsSync) {
        syncToDatabase();
    }

    // Close SQLite database
    if (sqliteDb) {
        sqlite3_close(sqliteDb);
    }
}

bool GameDB::putTimeSeries(const std::string& seriesName, uint64_t timestamp, double value) {
    try {
        entt::entity targetEntity = entt::null;

        // 1) Search for an existing TimeSeriesComponent with that name
        auto view = registry.view<TimeSeriesComponent>();
        for (auto entity : view) {
            auto& comp = view.get<TimeSeriesComponent>(entity);
            if (comp.timeSeriesName == seriesName) {
                targetEntity = entity;
                break;
            }
        }

        // 2) If found, just add the new datapoint
        if (targetEntity != entt::null) {
            auto& existingComp = registry.get<TimeSeriesComponent>(targetEntity);
            existingComp.addDataPoint(timestamp, value);
        }
        // 3) Otherwise create a new entity + component
        else {
            targetEntity = registry.create();
            auto& newComp = registry.emplace<TimeSeriesComponent>(targetEntity);
            newComp.timeSeriesName = seriesName;
            newComp.addDataPoint(timestamp, value);
        }

        // Mark as needing sync - Setting as false just to debug.
        needsSync = false;

        // Sync to database immediately if appropriate
        return syncToDatabase();
    } catch (const std::exception& e) {
        Logger::getLogger()->error("Error storing time series data: {}", e.what());
        return false;
    }
}

std::vector<std::pair<uint64_t, double>> GameDB::queryTimeSeries(const std::string& seriesName,
                                                                 uint64_t start_time,
                                                                 uint64_t end_time) {
    std::vector<std::pair<uint64_t, double>> results;

    try {
        // First check in-memory cache
        for (auto entity : registry.view<TimeSeriesComponent>()) {
            auto& timeSeriesComp = registry.get<TimeSeriesComponent>(entity);
            if (timeSeriesComp.timeSeriesName == seriesName) {
                results = timeSeriesComp.getDataPoints(start_time, end_time);

                // If we have results, return them
                if (!results.empty()) {
                    // Logger::getLogger()->info("Found {} results in memory cache",
                    // results.size());
                    return results;
                }
            }
        }

        // If no results in memory, check the database
        const char* query =
            "SELECT timestamp, value FROM time_series WHERE series_name = ? AND timestamp >= ? AND "
            "timestamp <= ? "
            "ORDER BY timestamp";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(sqliteDb, query, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getLogger()->error("Failed to prepare SQLite query: {}",
                                       sqlite3_errmsg(sqliteDb));
            return results;
        }

        // Bind parameters
        sqlite3_bind_text(stmt, 1, seriesName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(start_time));
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(end_time));

        // Execute query and fetch results
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            uint64_t ts = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
            double val = sqlite3_column_double(stmt, 2);
            results.emplace_back(ts, val);

            // Also update memory cache
            auto entity = registry.create();
            auto& timeSeriesComp = registry.emplace<TimeSeriesComponent>(entity);
            timeSeriesComp.timeSeriesName = seriesName;
            timeSeriesComp.addDataPoint(ts, val);
        }

        sqlite3_finalize(stmt);
        Logger::getLogger()->info("Found {} results in database", results.size());

    } catch (const std::exception& e) {
        Logger::getLogger()->error("Error querying time series data: {}", e.what());
    }

    return results;
}

bool GameDB::executeSQL(const std::string& query) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(sqliteDb, query.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Logger::getLogger()->error("SQLite Error: {}", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool GameDB::createTables() {
    std::string query = R"(
        CREATE TABLE IF NOT EXISTS players (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            password_hash TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS time_series (
            series_name TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            value REAL NOT NULL,
            PRIMARY KEY(series_name, timestamp)
        );
        CREATE TABLE IF NOT EXISTS game_state (
            player_id INTEGER PRIMARY KEY,
            level INTEGER NOT NULL,
            score INTEGER NOT NULL,
            FOREIGN KEY(player_id) REFERENCES players(id)
        );
    )";
    return executeSQL(query);
}

bool GameDB::resetDB() {
    Logger::getLogger()->warn("Resetting database");

    // Close the database connection
    if (sqliteDb) {
        sqlite3_close(sqliteDb);
        sqliteDb = nullptr;
    }

    try {
        // Delete the database file
        if (std::filesystem::exists(sqlitePath)) {
            std::filesystem::remove(sqlitePath);
            Logger::getLogger()->info("Successfully removed database file");
        }

        // Reopen the database
        if (sqlite3_open(sqlitePath.c_str(), &sqliteDb) != SQLITE_OK) {
            std::string errorMsg = sqlite3_errmsg(sqliteDb);
            Logger::getLogger()->error("Error reopening SQLite DB after reset: {}", errorMsg);
            return false;
        }

        // Recreate the tables
        createTables();

        // Clear in-memory data
        needsSync = false;

        Logger::getLogger()->info("Database successfully reset");
        return true;
    } catch (const std::exception& e) {
        Logger::getLogger()->error("Error resetting database: {}", e.what());
        return false;
    }
}

bool GameDB::syncToDatabase() {
    if (!needsSync) {
        return true;  // Nothing to sync
    }

    try {
        // Begin transaction for better performance
        executeSQL("BEGIN TRANSACTION");

        // Prepare statement for inserting time series data
        const char* query =
            "INSERT OR REPLACE INTO time_series (series_name, timestamp, value) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(sqliteDb, query, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getLogger()->error("Failed to prepare SQLite insert: {}",
                                       sqlite3_errmsg(sqliteDb));
            executeSQL("ROLLBACK");
            return false;
        }

        // Insert each data point
        for (auto entity : registry.view<TimeSeriesComponent>()) {
            auto& timeSeriesComp = registry.get<TimeSeriesComponent>(entity);
            for (const auto& [timestamp, value] : timeSeriesComp.timeSeriesData) {
                // Bind parameters
                sqlite3_bind_text(stmt, 1, timeSeriesComp.timeSeriesName.c_str(), -1,
                                  SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(timestamp));
                sqlite3_bind_double(stmt, 3, value);

                // Execute
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    Logger::getLogger()->error("Error inserting time series data: {}",
                                               sqlite3_errmsg(sqliteDb));
                    sqlite3_finalize(stmt);
                    executeSQL("ROLLBACK");
                    return false;
                }

                // Reset statement for next use
                sqlite3_reset(stmt);
                sqlite3_clear_bindings(stmt);
            }
        }

        sqlite3_finalize(stmt);

        // Commit transaction
        executeSQL("COMMIT");

        // Mark as synced
        needsSync = false;
        Logger::getLogger()->info("Successfully synced time series data points to database");

        return true;
    } catch (const std::exception& e) {
        Logger::getLogger()->error("Error syncing to database: {}", e.what());
        executeSQL("ROLLBACK");
        return false;
    }
}

bool GameDB::loadFromDatabase() {
    try {
        // Clear any existing data

        // Query all time series data
        const char* query =
            "SELECT series_name, timestamp, value FROM time_series ORDER BY series_name, timestamp";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(sqliteDb, query, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getLogger()->error("Failed to prepare SQLite query: {}",
                                       sqlite3_errmsg(sqliteDb));
            return false;
        }

        // Execute query and fetch results
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string seriesName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            uint64_t ts = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
            double val = sqlite3_column_double(stmt, 2);

            // Add to memory cache
            auto entity = registry.create();
            auto& timeSeriesComp = registry.emplace<TimeSeriesComponent>(entity);
            timeSeriesComp.timeSeriesName = seriesName;
            timeSeriesComp.addDataPoint(ts, val);
            count++;
        }

        sqlite3_finalize(stmt);
        Logger::getLogger()->info("Loaded {} time series data points from database", count);

        // Mark as synced since we just loaded from the database
        needsSync = false;

        return true;
    } catch (const std::exception& e) {
        Logger::getLogger()->error("Error loading from database: {}", e.what());
        return false;
    }
}