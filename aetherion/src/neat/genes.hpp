#ifndef GENES_HPP
#define GENES_HPP

#include <cmath>
#include <random>
#include <sstream>
#include <string>
#include <utility>

// A simple configuration structure for gene distance calculations.
struct GenomeConfig {
    double compatibility_weight_coefficient;
};

// ---------------------------------------------------------------------------
// BaseGene: Abstract base class for gene types.
// ---------------------------------------------------------------------------
class BaseGene {
   public:
    // The base class no longer forces all children to have an int key.
    BaseGene() = default;
    virtual ~BaseGene() = default;

    virtual std::string to_string() const = 0;
    virtual BaseGene* copy() const = 0;
    virtual BaseGene* crossover(const BaseGene& other) const = 0;
    virtual void init_attributes() = 0;
    virtual void mutate() = 0;
};

// ---------------------------------------------------------------------------
// DefaultNodeGene: Represents a node gene with attributes similar to bias,
// response, activation, and aggregation.
// ---------------------------------------------------------------------------
class DefaultNodeGene : public BaseGene {
   public:
    int key;
    float bias;
    float response;
    std::string activation;
    std::string aggregation;

    // Constructor; initializes key and sets default attribute values.
    DefaultNodeGene(int key);

    // Return a string representation.
    virtual std::string to_string() const override;

    // Deep copy.
    virtual DefaultNodeGene* copy() const override;

    // Compute the distance between this node gene and another using config.
    double distance(const DefaultNodeGene& other, const GenomeConfig& config) const;

    // Crossover: randomly inherit attributes from this gene or the other.
    DefaultNodeGene* crossover(const DefaultNodeGene& other) const;

    // BaseGene override for crossover.
    virtual BaseGene* crossover(const BaseGene& other) const override;

    // Initialize attributes to default values.
    virtual void init_attributes() override;

    void mutate() override;
};

// ---------------------------------------------------------------------------
// DefaultConnectionGene: Represents a connection gene with a weight and an
// enabled flag.
// ---------------------------------------------------------------------------
class DefaultConnectionGene : public BaseGene {
   public:
    std::pair<int, int> key;  // (input, output)
    float weight;
    bool enabled;

    // Constructor; initializes key and sets default weight and enabled.
    DefaultConnectionGene(const std::pair<int, int>& key);

    // Return a string representation.
    virtual std::string to_string() const override;

    // Deep copy.
    virtual DefaultConnectionGene* copy() const override;

    // Compute the distance between this connection gene and another using config.
    double distance(const DefaultConnectionGene& other, const GenomeConfig& config) const;

    // Crossover: create a new connection gene inheriting attributes.
    DefaultConnectionGene* crossover(const DefaultConnectionGene& other) const;

    // BaseGene override for crossover.
    virtual BaseGene* crossover(const BaseGene& other) const override;

    // Initialize attributes to default values.
    virtual void init_attributes() override;

    void mutate() override {};
};

#endif  // GENES_HPP
