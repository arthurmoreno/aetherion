#include "genes.hpp"

// ---------------------------------------------------------------------------
// DefaultNodeGene Implementation
// ---------------------------------------------------------------------------
DefaultNodeGene::DefaultNodeGene(int key)
    : key(key), bias(0.0f), response(1.0f), activation(""), aggregation("") {}

std::string DefaultNodeGene::to_string() const {
    std::ostringstream oss;
    oss << "DefaultNodeGene(key=" << key << ", bias=" << bias << ", response=" << response
        << ", activation=" << activation << ", aggregation=" << aggregation << ")";
    return oss.str();
}

DefaultNodeGene* DefaultNodeGene::copy() const {
    DefaultNodeGene* new_gene = new DefaultNodeGene(key);
    new_gene->bias = bias;
    new_gene->response = response;
    new_gene->activation = activation;
    new_gene->aggregation = aggregation;
    return new_gene;
}

double DefaultNodeGene::distance(const DefaultNodeGene& other, const GenomeConfig& config) const {
    double d = std::abs(bias - other.bias) + std::abs(response - other.response);
    if (activation != other.activation) d += 1.0;
    if (aggregation != other.aggregation) d += 1.0;
    return d * config.compatibility_weight_coefficient;
}

DefaultNodeGene* DefaultNodeGene::crossover(const DefaultNodeGene& other) const {
    DefaultNodeGene* new_gene = new DefaultNodeGene(key);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, 1);
    new_gene->bias = (dis(gen) > 0.5) ? bias : other.bias;
    new_gene->response = (dis(gen) > 0.5) ? response : other.response;
    new_gene->activation = (dis(gen) > 0.5) ? activation : other.activation;
    new_gene->aggregation = (dis(gen) > 0.5) ? aggregation : other.aggregation;
    return new_gene;
}

BaseGene* DefaultNodeGene::crossover(const BaseGene& other) const {
    // Downcast to DefaultNodeGene. Caller must ensure same type.
    const DefaultNodeGene& otherNode = dynamic_cast<const DefaultNodeGene&>(other);
    return static_cast<BaseGene*>(crossover(otherNode));
}

// The init_attributes method sets default values for the node gene.
// In a full implementation, these might be based on a configuration object.
void DefaultNodeGene::init_attributes() {
    bias = 0.0f;       // Default bias.
    response = 1.0f;   // Default response.
    activation = "";   // Default activation function (empty string).
    aggregation = "";  // Default aggregation function (empty string).
}

void DefaultNodeGene::mutate() {
    // Implement mutation logic, even if empty for now.
}

// ---------------------------------------------------------------------------
// DefaultConnectionGene Implementation
// ---------------------------------------------------------------------------
DefaultConnectionGene::DefaultConnectionGene(const std::pair<int, int>& key)
    : key(key), weight(0.0f), enabled(true) {}

std::string DefaultConnectionGene::to_string() const {
    std::ostringstream oss;
    oss << "DefaultConnectionGene(key=(" << key.first << ", " << key.second << ")"
        << ", weight=" << weight << ", enabled=" << (enabled ? "true" : "false") << ")";
    return oss.str();
}

DefaultConnectionGene* DefaultConnectionGene::copy() const {
    DefaultConnectionGene* new_gene = new DefaultConnectionGene(key);
    new_gene->weight = weight;
    new_gene->enabled = enabled;
    return new_gene;
}

double DefaultConnectionGene::distance(const DefaultConnectionGene& other,
                                       const GenomeConfig& config) const {
    double d = std::abs(weight - other.weight);
    if (enabled != other.enabled) d += 1.0;
    return d * config.compatibility_weight_coefficient;
}

DefaultConnectionGene* DefaultConnectionGene::crossover(const DefaultConnectionGene& other) const {
    DefaultConnectionGene* child = new DefaultConnectionGene(key);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, 1);
    child->weight = (dis(gen) > 0.5) ? weight : other.weight;
    child->enabled = enabled && other.enabled;
    return child;
}

BaseGene* DefaultConnectionGene::crossover(const BaseGene& other) const {
    // Downcast to DefaultConnectionGene. Caller must ensure same type.
    const DefaultConnectionGene& otherConn = dynamic_cast<const DefaultConnectionGene&>(other);
    return static_cast<BaseGene*>(crossover(otherConn));
}

// The init_attributes method sets default values for the connection gene.
// In a full implementation, these values might come from a configuration.
void DefaultConnectionGene::init_attributes() {
    weight = 0.0f;   // Default weight.
    enabled = true;  // Default to enabled.
}
