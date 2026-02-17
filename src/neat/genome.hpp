#ifndef GENOME_HPP
#define GENOME_HPP

#include <map>
#include <string>
#include <vector>

#include "genes.hpp"

// Structure to hold raw genome parameters.
struct GenomeParams {
    int num_inputs;
    int num_outputs;
    int num_hidden;
    bool feed_forward;
    double compatibility_disjoint_coefficient;
    double compatibility_weight_coefficient;
    double conn_add_prob;
    double conn_delete_prob;
    double node_add_prob;
    double node_delete_prob;
    bool single_structural_mutation;
    std::string structural_mutation_surer;
    std::string initial_connection;
};

// ---------------------------------------------------------------------------
// DefaultGenomeConfig: Holds configuration for a genome and provides
// helper functions (e.g. for generating new node keys).
// ---------------------------------------------------------------------------
class DefaultGenomeConfig {
   public:
    int num_inputs;
    int num_outputs;
    int num_hidden;
    bool feed_forward;
    double compatibility_disjoint_coefficient;
    double compatibility_weight_coefficient;
    double conn_add_prob;
    double conn_delete_prob;
    double node_add_prob;
    double node_delete_prob;
    bool single_structural_mutation;
    std::string structural_mutation_surer;
    std::string initial_connection;
    double connection_fraction;

    std::vector<int> input_keys;
    std::vector<int> output_keys;

    // For generating unique node keys.
    int next_node_key;

    // Constructor: initializes configuration from raw parameters.
    DefaultGenomeConfig(const GenomeParams &params);

    // Returns a new node key that is not already used in node_dict.
    int get_new_node_key(const std::map<int, DefaultNodeGene> &node_dict);
};

// ---------------------------------------------------------------------------
// DefaultGenome: Represents a genome containing node and connection genes.
// ---------------------------------------------------------------------------
class DefaultGenome {
   public:
    int key;  // Unique identifier for the genome.
    std::map<int, DefaultNodeGene> nodes;
    std::map<std::pair<int, int>, DefaultConnectionGene> connections;
    double fitness;

    // Constructor.
    DefaultGenome(int key_);

    // Create a new node gene. This method calls the node's init_attributes().
    DefaultNodeGene create_node(const DefaultGenomeConfig &config, int node_key);

    // Create a new connection gene. This method calls the connection's init_attributes().
    DefaultConnectionGene create_connection(const std::pair<int, int> &conn_key);

    // Configure a new genome: creates output nodes, hidden nodes (if any),
    // and adds initial connections.
    void configure_new(DefaultGenomeConfig &config);

    // A simple mutation: add a node by splitting a random connection.
    void mutate_add_node(DefaultGenomeConfig &config);
};

// Utility function to get a pruned copy of the genome.
// (A full implementation would remove nodes/connections not required for outputs.)
DefaultGenome get_pruned_copy(const DefaultGenome &genome, const std::vector<int> &input_keys,
                              const std::vector<int> &output_keys);

#endif  // GENOME_HPP
