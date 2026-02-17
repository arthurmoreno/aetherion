#ifndef QUERYRESPONSE_HPP
#define QUERYRESPONSE_HPP

#include <nanobind/nanobind.h>

#include <bitset>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <ylt/struct_pack.hpp>

namespace nb = nanobind;

// Define response flags
enum ResponseFlag {
    LIST_STRING = 0,
    MAP_OF_MAPS,
    LIST_DOUBLE,
    MAP_OF_LISTS_DOUBLE,  // New flag for map of lists of double
    MAP_OF_MAPS_DOUBLE,   // New flag for map of maps of double
    RESPONSE_COUNT        // Keeps count of total response types
};

// Header structure to include response flags
struct QueryResponseHeader {
    unsigned long responseMask;
};

class QueryResponse {
   public:
    unsigned long responseMask = 0;

    virtual ~QueryResponse() = default;

    // Serialize the response into a byte buffer
    virtual std::vector<char> serialize() const = 0;

    // Deserialize the response from a byte buffer
    static QueryResponse* deserialize(const char* data, size_t size);

    // Python serialization wrapper functions
    virtual nb::bytes py_serialize() const = 0;

   protected:
    // Serialize the header
    void serializeHeader(std::vector<char>& buffer) const {
        QueryResponseHeader header{responseMask};
        struct_pack::serialize_to(buffer, header);
    }

    // Deserialize the header
    static QueryResponseHeader deserializeHeader(const char* data, size_t size, size_t& offset) {
        size_t consume_len = 0;
        auto header_result = struct_pack::deserialize<QueryResponseHeader>(
            data + offset, size - offset, consume_len);
        if (!header_result) {
            throw std::runtime_error("Failed to deserialize QueryResponseHeader");
        }
        offset += consume_len;
        return header_result.value();
    }
};

class ListStringResponse : public QueryResponse {
   public:
    std::vector<std::string> strings;

    ListStringResponse() { responseMask |= (1UL << LIST_STRING); }

    ListStringResponse(const std::vector<std::string>& strs) : strings(strs) {
        responseMask |= (1UL << LIST_STRING);
    }

    // Serialize the response
    std::vector<char> serialize() const override {
        std::vector<char> buffer;
        serializeHeader(buffer);
        struct_pack::serialize_to(buffer, strings);
        return buffer;
    }

    // Deserialize to ListStringResponse
    static ListStringResponse* deserialize(const char* data, size_t size, size_t& offset) {
        size_t consume_len = 0;
        auto strings_result = struct_pack::deserialize<std::vector<std::string>>(
            data + offset, size - offset, consume_len);
        if (!strings_result) {
            throw std::runtime_error("Failed to deserialize ListStringResponse");
        }
        offset += consume_len;
        return new ListStringResponse(strings_result.value());
    }

    // Python serialization
    nb::bytes py_serialize() const override {
        std::vector<char> serialized_data = serialize();
        return nb::bytes(serialized_data.data(), serialized_data.size());
    }
};

class ListDoubleResponse : public QueryResponse {
   public:
    std::vector<double> values;

    ListDoubleResponse() { responseMask |= (1UL << LIST_DOUBLE); }

    ListDoubleResponse(const std::vector<double>& vals) : values(vals) {
        responseMask |= (1UL << LIST_DOUBLE);
    }

    // Serialize the response
    std::vector<char> serialize() const override {
        std::vector<char> buffer;
        serializeHeader(buffer);
        struct_pack::serialize_to(buffer, values);
        return buffer;
    }

    // Deserialize to ListDoubleResponse
    static ListDoubleResponse* deserialize(const char* data, size_t size, size_t& offset) {
        size_t consume_len = 0;
        auto values_result = struct_pack::deserialize<std::vector<double>>(
            data + offset, size - offset, consume_len);
        if (!values_result) {
            throw std::runtime_error("Failed to deserialize ListDoubleResponse");
        }
        offset += consume_len;
        return new ListDoubleResponse(values_result.value());
    }

    // Python serialization
    nb::bytes py_serialize() const override {
        std::vector<char> serialized_data = serialize();
        return nb::bytes(serialized_data.data(), serialized_data.size());
    }
};

class MapOfMapsResponse : public QueryResponse {
   public:
    std::map<std::string, std::map<std::string, std::string>> mapOfMaps;

    MapOfMapsResponse() { responseMask |= (1UL << MAP_OF_MAPS); }

    MapOfMapsResponse(const std::map<std::string, std::map<std::string, std::string>>& m)
        : mapOfMaps(m) {
        responseMask |= (1UL << MAP_OF_MAPS);
    }

    // Serialize the response
    std::vector<char> serialize() const override {
        std::vector<char> buffer;
        serializeHeader(buffer);
        struct_pack::serialize_to(buffer, mapOfMaps);
        return buffer;
    }

    // Deserialize to MapOfMapsResponse
    static MapOfMapsResponse* deserialize(const char* data, size_t size, size_t& offset) {
        size_t consume_len = 0;
        auto map_result =
            struct_pack::deserialize<std::map<std::string, std::map<std::string, std::string>>>(
                data + offset, size - offset, consume_len);
        if (!map_result) {
            throw std::runtime_error("Failed to deserialize MapOfMapsResponse");
        }
        offset += consume_len;
        return new MapOfMapsResponse(map_result.value());
    }

    // Python serialization
    nb::bytes py_serialize() const override {
        std::vector<char> serialized_data = serialize();
        return nb::bytes(serialized_data.data(), serialized_data.size());
    }
};

class MapOfListsOfDoubleResponse : public QueryResponse {
   public:
    std::map<std::string, std::vector<double>> mapOfLists;

    MapOfListsOfDoubleResponse() { responseMask |= (1UL << MAP_OF_LISTS_DOUBLE); }

    MapOfListsOfDoubleResponse(const std::map<std::string, std::vector<double>>& m)
        : mapOfLists(m) {
        responseMask |= (1UL << MAP_OF_LISTS_DOUBLE);
    }

    // Serialize the response
    std::vector<char> serialize() const override {
        std::vector<char> buffer;
        serializeHeader(buffer);
        struct_pack::serialize_to(buffer, mapOfLists);
        return buffer;
    }

    // Deserialize to MapOfListsOfDoubleResponse
    static MapOfListsOfDoubleResponse* deserialize(const char* data, size_t size, size_t& offset) {
        size_t consume_len = 0;
        auto map_result = struct_pack::deserialize<std::map<std::string, std::vector<double>>>(
            data + offset, size - offset, consume_len);
        if (!map_result) {
            throw std::runtime_error("Failed to deserialize MapOfListsOfDoubleResponse");
        }
        offset += consume_len;
        return new MapOfListsOfDoubleResponse(map_result.value());
    }

    // Python serialization
    nb::bytes py_serialize() const override {
        std::vector<char> serialized_data = serialize();
        return nb::bytes(serialized_data.data(), serialized_data.size());
    }
};

class MapOfMapsOfDoubleResponse : public QueryResponse {
   public:
    std::map<std::string, std::map<std::string, double>> mapOfMaps;

    MapOfMapsOfDoubleResponse() { responseMask |= (1UL << MAP_OF_MAPS_DOUBLE); }

    MapOfMapsOfDoubleResponse(const std::map<std::string, std::map<std::string, double>>& m)
        : mapOfMaps(m) {
        responseMask |= (1UL << MAP_OF_MAPS_DOUBLE);
    }

    // Serialize the response
    std::vector<char> serialize() const override {
        std::vector<char> buffer;
        serializeHeader(buffer);
        struct_pack::serialize_to(buffer, mapOfMaps);
        return buffer;
    }

    // Deserialize to MapOfMapsOfDoubleResponse
    static MapOfMapsOfDoubleResponse* deserialize(const char* data, size_t size, size_t& offset) {
        size_t consume_len = 0;
        auto map_result =
            struct_pack::deserialize<std::map<std::string, std::map<std::string, double>>>(
                data + offset, size - offset, consume_len);
        if (!map_result) {
            throw std::runtime_error("Failed to deserialize MapOfMapsOfDoubleResponse");
        }
        offset += consume_len;
        return new MapOfMapsOfDoubleResponse(map_result.value());
    }

    // Python serialization
    nb::bytes py_serialize() const override {
        std::vector<char> serialized_data = serialize();
        return nb::bytes(serialized_data.data(), serialized_data.size());
    }
};

#endif  // QUERYRESPONSE_HPP