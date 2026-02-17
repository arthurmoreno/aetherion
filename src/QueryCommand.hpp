#ifndef QUERYCOMMAND_HPP
#define QUERYCOMMAND_HPP

#include <nanobind/nanobind.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "Logger.hpp"

namespace nb = nanobind;

struct QueryCommand {
    std::string type;
    // Store parameters as string -> string for simplicity;
    // adapt as necessary if you need typed values (ints, floats, etc.)
    std::unordered_map<std::string, std::string> params;

    // Add helper methods
    void setParam(const std::string& key, const std::string& value) { params[key] = value; }

    void setParam(const std::string& key, double value) { params[key] = std::to_string(value); }

    double getParamAsDouble(const std::string& key, double defaultValue = 0.0) const {
        auto it = params.find(key);
        if (it != params.end()) {
            try {
                return std::stod(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }
};

std::vector<QueryCommand> toCommandList(const nb::list& optionalQueries);

#endif  // QUERYCOMMAND_HPP