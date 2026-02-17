#include "genome.hpp"

#include <algorithm>
#include <random>
#include <sstream>

// ---------------------------------------------------------------------------
// DefaultGenomeConfig Implementation
// ---------------------------------------------------------------------------
DefaultGenomeConfig::DefaultGenomeConfig(const GenomeParams &params)
    : num_inputs(params.num_inputs),
      num_outputs(params.num_outputs),
      num_hidden(params.num_hidden),
      feed_forward(params.feed_forward),
      compatibility_disjoint_coefficient(params.compatibility_disjoint_coefficient),
      compatibility_weight_coefficient(params.compatibility_weight_coefficient),
      conn_add_prob(params.conn_add_prob),
      conn_delete_prob(params.conn_delete_prob),
      node_add_prob(params.node_add_prob),
      node_delete_prob(params.node_delete_prob),
      single_structural_mutation(params.single_structural_mutation),
      structural_mutation_surer(params.structural_mutation_surer),
      initial_connection(params.initial_connection),
      connection_fraction(1.0),  // default value; may be overridden below
      next_node_key(0) {
    // Handle 'partial' connection type if specified.
    if (initial_connection.find("partial") != std::string::npos) {
        std::istringstream iss(initial_connection);
        std::string connType;
        iss >> connType >> connection_fraction;
        initial_connection = connType;
        if (connection_fraction < 0.0 || connection_fraction > 1.0)
            throw std::runtime_error("Partial connection fraction must be between 0.0 and 1.0.");
    }

    // Set up input keys (by convention, inputs are negative).
    for (int i = 0; i < num_inputs; i++) {
        input_keys.push_back(-i - 1);
    }
    // Set up output keys.
    for (int i = 0; i < num_outputs; i++) {
        output_keys.push_back(i);
    }
}

int DefaultGenomeConfig::get_new_node_key(const std::map<int, DefaultNodeGene> &node_dict) {
    if (!node_dict.empty()) {
        auto it = std::max_element(node_dict.begin(), node_dict.end(),
                                   [](const auto &a, const auto &b) { return a.first < b.first; });
        next_node_key = it->first + 1;
    }
    // Ensure the new key is unique.
    while (node_dict.find(next_node_key) != node_dict.end()) next_node_key++;
    return next_node_key++;
}

// ---------------------------------------------------------------------------
// DefaultGenome Implementation
// ---------------------------------------------------------------------------
DefaultGenome::DefaultGenome(int key_) : key(key_), fitness(0.0) {}

DefaultNodeGene DefaultGenome::create_node(const DefaultGenomeConfig &config, int node_key) {
    DefaultNodeGene node(node_key);
    node.init_attributes();  // Assume init_attributes() is implemented (even if empty)
    return node;
}

DefaultConnectionGene DefaultGenome::create_connection(const std::pair<int, int> &conn_key) {
    DefaultConnectionGene conn(conn_key);
    conn.init_attributes();  // Assume init_attributes() is implemented (even if empty)
    return conn;
}

void DefaultGenome::configure_new(DefaultGenomeConfig &config) {
    // Create output nodes.
    for (int node_key : config.output_keys) {
        nodes.insert({node_key, create_node(config, node_key)});
        nodes.emplace(node_key, create_node(config, node_key));
    }
    // Add hidden nodes if requested.
    for (int i = 0; i < config.num_hidden; i++) {
        int node_key = config.get_new_node_key(nodes);
        nodes.insert({node_key, create_node(config, node_key)});
    }
    // For demonstration, create a simple full connection from one random input.
    if (!config.input_keys.empty() && !config.output_keys.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, config.input_keys.size() - 1);
        int in_idx = dist(gen);
        int input_id = config.input_keys[in_idx];
        for (int output_id : config.output_keys) {
            std::pair<int, int> conn_key(input_id, output_id);
            connections.insert({conn_key, create_connection(conn_key)});
        }
    }
}

void DefaultGenome::mutate_add_node(DefaultGenomeConfig &config) {
    if (connections.empty()) return;

    // Select a random connection to split.
    std::vector<std::pair<int, int>> keys;
    for (const auto &kv : connections) keys.push_back(kv.first);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, keys.size() - 1);
    auto selected_key = keys[dist(gen)];

    // Disable the selected connection.
    auto conn_iter = connections.find(selected_key);
    if (conn_iter != connections.end()) {
        conn_iter->second.enabled = false;
    }

    // Create a new node.
    int new_node_key = config.get_new_node_key(nodes);
    nodes.insert({new_node_key, create_node(config, new_node_key)});

    // Create two new connections.
    std::pair<int, int> conn1_key(selected_key.first, new_node_key);
    DefaultConnectionGene conn1 = create_connection(conn1_key);
    conn1.weight = 1.0;
    connections.insert({conn1_key, conn1});

    std::pair<int, int> conn2_key(new_node_key, selected_key.second);
    DefaultConnectionGene conn2 = create_connection(conn2_key);
    // Use the weight from the original connection.
    conn2.weight = conn_iter->second.weight;
    connections.insert({conn2_key, conn2});
}

// ---------------------------------------------------------------------------
// Utility: Get a pruned copy of the genome.
// (This simplified version returns a copy without any pruning.)
// ---------------------------------------------------------------------------
DefaultGenome get_pruned_copy(const DefaultGenome &genome, const std::vector<int> &input_keys,
                              const std::vector<int> &output_keys) {
    DefaultGenome pruned(genome.key);
    pruned.nodes = genome.nodes;
    pruned.connections = genome.connections;
    pruned.fitness = genome.fitness;
    return pruned;
}
