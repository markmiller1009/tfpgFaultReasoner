#ifndef RTFPG_MODEL_H
#define RTFPG_MODEL_H

/**
 * @class rTFPGModel
 * @brief Holds the static graph definition G=<F,D,E,ET,DC,DP> plus prognosis attributes.
 *
 * @requirement REQ-MOD-01: The model shall store Edges (E) with time intervals.
 * @requirement REQ-MOD-02: The model shall store Discrepancy Nodes (D).
 * @requirement REQ-MOD-03: The model shall store Failure Mode Nodes (F).
 * @requirement REQ-MOD-04: The class shall provide a method GetCriticalityFront(int n).
 */

#include <string>
#include <vector>
#include <optional>
#include "json.hpp"

// Represents a single signal source from the model definition
struct Signal {
    std::string id;
    std::string source_name;
    std::string type;
    std::string units;
    double range_min = 0.0;
    double range_max = 1.0;
};

// REQ-MOD-02: Discrepancy Predicate (DP)
struct Predicate {
    std::string signal_ref;
    std::string op;
    double threshold;
};

// REQ-MOD-02: Discrepancy Type (DC)
enum class GateType {
    OR,
    AND
};

enum class NodeType {
    FailureMode,
    Discrepancy
};

// REQ-MOD-02 & REQ-MOD-03: Node structure for Failure Modes (F) and Discrepancies (D)
struct Node {
    std::string id;
    std::string name;
    NodeType type;
    std::optional<GateType> gate_type; // Only for Discrepancies
    std::optional<Predicate> predicate; // Only for Discrepancies
    int criticality_level = 0; // REQ-MOD-02: Criticality Level (CL)
};

// REQ-MOD-01: Edge structure (E) with time intervals (ET)
struct Edge {
    std::string from;
    std::string to;
    int time_min_ms;
    int time_max_ms;
    // std::string mode; // Optional operational mode (EM) - not in current JSON
};

class rTFPGModel {
public:
    explicit rTFPGModel(const nlohmann::json& model_data);

    // REQ-MOD-04: Returns the set of all nodes with Criticality Level >= n
    std::vector<Node> GetCriticalityFront(int n) const;

    const std::vector<Signal>& getSignals() const { return m_signals; }
    const std::vector<Node>& getNodes() const { return m_nodes; }
    const std::vector<Edge>& getEdges() const { return m_edges; }

    // Methods for Graph Refinement
    void addNode(const Node& node);
    void removeNode(const std::string& id);
    void addEdge(const Edge& edge);
    void removeEdge(const std::string& from, const std::string& to);

private:
    std::vector<Signal> m_signals;
    std::vector<Node> m_nodes;
    std::vector<Edge> m_edges;
};

#endif // RTFPG_MODEL_H