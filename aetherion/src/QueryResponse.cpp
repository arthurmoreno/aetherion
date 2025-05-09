#include "QueryResponse.hpp"

QueryResponse* QueryResponse::deserialize(const char* data, size_t size) {
    size_t offset = 0;
    QueryResponseHeader header = deserializeHeader(data, size, offset);

    // Determine which response type(s) are present based on the bitmap
    // Here, we assume only one type per response for simplicity
    if (header.responseMask & (1UL << LIST_STRING)) {
        return ListStringResponse::deserialize(data, size, offset);
    } else if (header.responseMask & (1UL << MAP_OF_MAPS)) {
        return MapOfMapsResponse::deserialize(data, size, offset);
    } else if (header.responseMask & (1UL << LIST_DOUBLE)) {
        return ListDoubleResponse::deserialize(data, size, offset);
    } else if (header.responseMask & (1UL << MAP_OF_LISTS_DOUBLE)) {
        return MapOfListsOfDoubleResponse::deserialize(data, size, offset);
    } else if (header.responseMask & (1UL << MAP_OF_MAPS_DOUBLE)) {
        return MapOfMapsOfDoubleResponse::deserialize(data, size, offset);
    } else {
        throw std::runtime_error("Unknown QueryResponse type");
    }
}