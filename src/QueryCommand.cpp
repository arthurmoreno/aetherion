#include "QueryCommand.hpp"

namespace nb = nanobind;

std::vector<QueryCommand> toCommandList(const nb::list& optionalQueries) {
    std::vector<QueryCommand> commands;
    commands.reserve(optionalQueries.size());

    for (auto item : optionalQueries) {
        // Skip non-dictionary items
        if (!nb::isinstance<nb::dict>(item)) {
            // Optionally log an error here if desired
            continue;
        }

        nb::dict commandDict = nb::cast<nb::dict>(item);

        // Check for "type"
        if (!commandDict.contains("type")) {
            // Optionally log an error here if desired
            continue;
        }

        // Convert the "type"
        std::string type = nb::cast<std::string>(commandDict["type"]);

        // Check for "params"
        if (!commandDict.contains("params")) {
            // Optionally log an error here if desired
            Logger::getLogger()->debug("[toCommandList] No 'params' found for command type '{}'",
                                       type);
            continue;
        }

        // Convert "params" to a string->string map
        QueryCommand cmd;
        cmd.type = type;

        // If 'params' is a dict of various Python objects, we can convert everything to string.
        nb::dict paramsDict = nb::cast<nb::dict>(commandDict["params"]);
        for (const auto& kv : paramsDict) {
            std::string key = nb::cast<std::string>(kv.first);
            std::string val;

            // Handle different Python types
            if (nb::isinstance<nb::str>(kv.second)) {
                val = nb::cast<std::string>(kv.second);
            } else if (nb::isinstance<nb::int_>(kv.second)) {
                val = std::to_string(nb::cast<int64_t>(kv.second));
            } else if (nb::isinstance<nb::float_>(kv.second)) {
                val = std::to_string(nb::cast<double>(kv.second));
            } else if (nb::isinstance<nb::bool_>(kv.second)) {
                val = nb::cast<bool>(kv.second) ? "true" : "false";
            } else {
                // For other types, use Python's str() function
                nb::object str_obj = nb::module_::import_("builtins").attr("str")(kv.second);
                val = nb::cast<std::string>(str_obj);
            }

            // Logger::getLogger()->debug("[toCommandList] Adding param: {} = {}", key, val);
            cmd.params[key] = val;
        }

        commands.push_back(std::move(cmd));
    }

    return commands;
}