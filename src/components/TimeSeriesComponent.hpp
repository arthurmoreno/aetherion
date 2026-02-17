#ifndef TIMESERIES_COMPONENT_HPP
#define TIMESERIES_COMPONENT_HPP

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Component to store time series data in memory using EnTT
 *
 * This component stores time series data in memory for efficient access.
 * Data is stored in a std::map for quick lookups by timestamp.
 */
struct TimeSeriesComponent {
    // Map of timestamps to values
    // Using std::map for ordered storage and binary search lookups
    std::string timeSeriesName;
    std::map<uint64_t, double> timeSeriesData;

    // Add a data point
    void addDataPoint(uint64_t timestamp, double value) { timeSeriesData[timestamp] = value; }

    // Get data points within a time range
    std::vector<std::pair<uint64_t, double>> getDataPoints(uint64_t startTime,
                                                           uint64_t endTime) const {
        std::vector<std::pair<uint64_t, double>> results;

        // Find the first timestamp greater than or equal to startTime
        auto it = timeSeriesData.lower_bound(startTime);

        // Iterate through all timestamps in the range [startTime, endTime]
        while (it != timeSeriesData.end() && it->first <= endTime) {
            results.emplace_back(it->first, it->second);
            ++it;
        }

        return results;
    }

    // Clear all data points
    void clear() { timeSeriesData.clear(); }

    // Get number of data points
    size_t size() const { return timeSeriesData.size(); }
};

#endif  // TIMESERIES_COMPONENT_HPP