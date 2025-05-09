#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Exception for unknown configuration items.
class UnknownConfigItemError : public std::runtime_error {
   public:
    explicit UnknownConfigItemError(const std::string &msg) : std::runtime_error(msg) {}
};

// A simple INI-style configuration parser.
class ConfigParser {
   public:
    using Section = std::unordered_map<std::string, std::string>;
    std::unordered_map<std::string, Section> sections;

    ConfigParser() = default;

    // Reads an INI file.
    void read_file(const std::string &filename) {
        std::ifstream file(filename);
        if (!file) throw std::runtime_error("Could not open config file: " + filename);
        std::string line;
        std::string current_section;
        while (std::getline(file, line)) {
            // Remove comments (lines starting with '#' or ';')
            auto comment_pos = line.find_first_of("#;");
            if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
            line = trim(line);
            if (line.empty()) continue;
            if (line.front() == '[' && line.back() == ']') {
                current_section = trim(line.substr(1, line.size() - 2));
                sections[current_section] = Section();
            } else {
                auto pos = line.find('=');
                if (pos == std::string::npos) continue;  // skip malformed lines
                std::string key = trim(line.substr(0, pos));
                std::string value = trim(line.substr(pos + 1));
                if (!current_section.empty()) {
                    sections[current_section][key] = value;
                }
            }
        }
    }

    bool has_section(const std::string &section) const {
        return sections.find(section) != sections.end();
    }

    std::string get(const std::string &section, const std::string &key) const {
        auto sec_it = sections.find(section);
        if (sec_it == sections.end()) throw std::runtime_error("Section not found: " + section);
        const auto &sec = sec_it->second;
        auto key_it = sec.find(key);
        if (key_it == sec.end())
            throw std::runtime_error("Key not found: " + key + " in section " + section);
        return key_it->second;
    }

    int getint(const std::string &section, const std::string &key) const {
        return std::stoi(get(section, key));
    }

    double getfloat(const std::string &section, const std::string &key) const {
        return std::stod(get(section, key));
    }

    bool getboolean(const std::string &section, const std::string &key) const {
        std::string val = get(section, key);
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        if (val == "true") return true;
        if (val == "false") return false;
        throw std::runtime_error(key + " must be True or False in section " + section);
    }

    // Returns all key-value pairs in a section.
    std::unordered_map<std::string, std::string> items(const std::string &section) const {
        auto it = sections.find(section);
        if (it == sections.end()) throw std::runtime_error("Section not found: " + section);
        return it->second;
    }

   private:
    static std::string trim(const std::string &s) {
        const char *whitespace = " \t\r\n";
        size_t start = s.find_first_not_of(whitespace);
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(whitespace);
        return s.substr(start, end - start + 1);
    }
};

// Helper function to split a string by a delimiter.
inline std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

// Helper function to join a vector of strings.
inline std::string join(const std::vector<std::string> &vec, const std::string &delimiter) {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); i++) {
        oss << vec[i];
        if (i != vec.size() - 1) oss << delimiter;
    }
    return oss.str();
}

// A templated configuration parameter.
template <typename T>
class ConfigParameter {
   public:
    std::string name;
    std::optional<T> default_value;

    // Constructor without a default value.
    explicit ConfigParameter(const std::string &name) : name(name), default_value(std::nullopt) {}

    // Constructor with a default value.
    ConfigParameter(const std::string &name, const T &def_val)
        : name(name), default_value(def_val) {}

    // Parse a parameter from the given section using a ConfigParser.
    T parse(const std::string &section, const ConfigParser &parser) const {
        if constexpr (std::is_same_v<T, int>) {
            return parser.getint(section, name);
        } else if constexpr (std::is_same_v<T, bool>) {
            return parser.getboolean(section, name);
        } else if constexpr (std::is_same_v<T, double>) {
            return parser.getfloat(section, name);
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            std::string v = parser.get(section, name);
            return split(v, ' ');
        } else if constexpr (std::is_same_v<T, std::string>) {
            return parser.get(section, name);
        } else {
            throw std::runtime_error("Unexpected configuration type for key: " + name);
        }
    }

    // Interpret a parameter from a key/value map (e.g. from a section).
    T interpret(const std::unordered_map<std::string, std::string> &config_dict) const {
        auto it = config_dict.find(name);
        if (it == config_dict.end()) {
            if (!default_value.has_value()) {
                throw std::runtime_error("Missing configuration item: " + name);
            } else {
                std::cerr << "Warning: Using default " << format(*default_value) << " for '" << name
                          << "'\n";
                return *default_value;
            }
        }
        const std::string &value = it->second;
        try {
            if constexpr (std::is_same_v<T, std::string>) {
                return value;
            } else if constexpr (std::is_same_v<T, int>) {
                return std::stoi(value);
            } else if constexpr (std::is_same_v<T, bool>) {
                std::string lower = value;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower == "true")
                    return true;
                else if (lower == "false")
                    return false;
                else
                    throw std::runtime_error(name + " must be True or False");
            } else if constexpr (std::is_same_v<T, double>) {
                return std::stod(value);
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                return split(value, ' ');
            }
        } catch (...) {
            throw std::runtime_error("Error interpreting config item '" + name + "' with value '" +
                                     value + "'");
        }
        throw std::runtime_error("Unexpected configuration type for key: " + name);
    }

    // Format a value as a string.
    std::string format(const T &value) const {
        if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            return join(value, " ");
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else {
            return std::to_string(value);
        }
    }
};

// Helper function to write a set of parameters in a pretty (aligned) format.
inline void write_pretty_params(std::ostream &os,
                                const std::vector<std::pair<std::string, std::string>> &params) {
    size_t longest = 0;
    for (const auto &p : params) {
        if (p.first.size() > longest) longest = p.first.size();
    }
    auto sorted_params = params;
    std::sort(sorted_params.begin(), sorted_params.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    for (const auto &p : sorted_params) {
        os << std::setw(longest) << std::left << p.first << " = " << p.second << "\n";
    }
}

/*
  The main Config class.
  Template parameters:
    - GenomeType, ReproductionType, SpeciesSetType, StagnationType:
      These types must provide:
         • A type alias `ConfigType` for their configuration data.
         • A static method:
               static ConfigType parse_config(const std::unordered_map<std::string, std::string>&
  dict); • A static method: static void write_config(std::ostream& os, const ConfigType& config); •
  A static method or constant: section_name() that returns the section name.
*/
template <typename GenomeType, typename ReproductionType, typename SpeciesSetType,
          typename StagnationType>
class Config {
   public:
    // NEAT configuration parameters.
    int pop_size;
    std::string fitness_criterion;
    double fitness_threshold;
    bool reset_on_extinction;
    bool no_fitness_termination;

    // Configuration for the various types.
    typename GenomeType::ConfigType genome_config;
    typename SpeciesSetType::ConfigType species_set_config;
    typename StagnationType::ConfigType stagnation_config;
    typename ReproductionType::ConfigType reproduction_config;

    // The constructor reads and parses the configuration file.
    Config(const std::string &filename) {
        if (!std::filesystem::exists(filename)) {
            throw std::runtime_error("No such config file: " +
                                     std::filesystem::absolute(filename).string());
        }

        ConfigParser parser;
        parser.read_file(filename);

        if (!parser.has_section("NEAT"))
            throw std::runtime_error("'NEAT' section not found in NEAT configuration file.");

        // Define the expected NEAT parameters.
        ConfigParameter<int> p_pop_size("pop_size");
        ConfigParameter<std::string> p_fitness_criterion("fitness_criterion");
        ConfigParameter<double> p_fitness_threshold("fitness_threshold");
        ConfigParameter<bool> p_reset_on_extinction("reset_on_extinction");
        ConfigParameter<bool> p_no_fitness_termination("no_fitness_termination", false);

        // Parse required parameters. For those without defaults, errors will be thrown if missing.
        pop_size = p_pop_size.parse("NEAT", parser);
        fitness_criterion = p_fitness_criterion.parse("NEAT", parser);
        fitness_threshold = p_fitness_threshold.parse("NEAT", parser);
        reset_on_extinction = p_reset_on_extinction.parse("NEAT", parser);
        try {
            no_fitness_termination = p_no_fitness_termination.parse("NEAT", parser);
        } catch (...) {
            no_fitness_termination = false;
            std::cerr << "Warning: Using default false for 'no_fitness_termination'\n";
        }

        // Check for unknown keys in the NEAT section.
        std::vector<std::string> known = {"pop_size", "fitness_criterion", "fitness_threshold",
                                          "reset_on_extinction", "no_fitness_termination"};
        auto neat_items = parser.items("NEAT");
        std::vector<std::string> unknown;
        for (const auto &pair : neat_items) {
            if (std::find(known.begin(), known.end(), pair.first) == known.end())
                unknown.push_back(pair.first);
        }
        if (!unknown.empty()) {
            std::string err = "Unknown (section 'NEAT') configuration item";
            if (unknown.size() > 1) {
                err += "s:\n";
                for (const auto &u : unknown) err += "\t" + u + "\n";
            } else {
                err += " " + unknown[0];
            }
            throw UnknownConfigItemError(err);
        }

        // Parse additional type sections using the provided types' static parse_config methods.
        auto genome_dict = parser.items(GenomeType::section_name());
        genome_config = GenomeType::parse_config(genome_dict);

        auto species_set_dict = parser.items(SpeciesSetType::section_name());
        species_set_config = SpeciesSetType::parse_config(species_set_dict);

        auto stagnation_dict = parser.items(StagnationType::section_name());
        stagnation_config = StagnationType::parse_config(stagnation_dict);

        auto reproduction_dict = parser.items(ReproductionType::section_name());
        reproduction_config = ReproductionType::parse_config(reproduction_dict);
    }

    // Save writes the current configuration to a file.
    void save(const std::string &filename) const {
        std::ofstream os(filename);
        if (!os) throw std::runtime_error("Cannot open file for writing: " + filename);
        os << "# The `NEAT` section specifies parameters particular to the NEAT algorithm\n";
        os << "# or the experiment itself.  This is the only required section.\n";
        os << "[NEAT]\n";
        std::vector<std::pair<std::string, std::string>> neat_params;
        neat_params.emplace_back("pop_size", std::to_string(pop_size));
        neat_params.emplace_back("fitness_criterion", fitness_criterion);
        neat_params.emplace_back("fitness_threshold", std::to_string(fitness_threshold));
        neat_params.emplace_back("reset_on_extinction", reset_on_extinction ? "true" : "false");
        neat_params.emplace_back("no_fitness_termination",
                                 no_fitness_termination ? "true" : "false");
        write_pretty_params(os, neat_params);

        os << "\n[" << GenomeType::section_name() << "]\n";
        GenomeType::write_config(os, genome_config);

        os << "\n[" << SpeciesSetType::section_name() << "]\n";
        SpeciesSetType::write_config(os, species_set_config);

        os << "\n[" << StagnationType::section_name() << "]\n";
        StagnationType::write_config(os, stagnation_config);

        os << "\n[" << ReproductionType::section_name() << "]\n";
        ReproductionType::write_config(os, reproduction_config);
    }
};

// ------------------------------------------------------------------------
// Example usage:
//
// The following commented-out code shows how one might instantiate the
// Config class provided that appropriate types (GenomeType, etc.) are defined.
// Each type must provide the static methods and type alias as described above.
//
// struct Genome {
//     using ConfigType = std::unordered_map<std::string, std::string>;
//     static std::string section_name() { return "Genome"; }
//     static ConfigType parse_config(const std::unordered_map<std::string, std::string>& dict) {
//         return dict; // user-defined parsing here
//     }
//     static void write_config(std::ostream& os, const ConfigType& config) {
//         for (const auto& kv : config)
//             os << kv.first << " = " << kv.second << "\n";
//     }
// };
//
// // Similar definitions must be provided for Reproduction, SpeciesSet, Stagnation.
//
// int main() {
//     try {
//         Config<Genome, Genome, Genome, Genome> config("path/to/config.ini");
//         // ... use config ...
//         config.save("path/to/new_config.ini");
//     } catch (const std::exception &ex) {
//         std::cerr << "Error: " << ex.what() << "\n";
//         return 1;
//     }
//     return 0;
// }
// ------------------------------------------------------------------------