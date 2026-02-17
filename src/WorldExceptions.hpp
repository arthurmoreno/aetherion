#pragma once

#include <stdexcept>
#include <string>

namespace aetherion {

/**
 * @brief Base exception class for all ecosystem engine errors
 *
 * This serves as the parent class for ecosystem-related exceptions,
 * including water simulation, vapor processing, and ecosystem dynamics.
 */
class EcosystemEngineException : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

}  // namespace aetherion
