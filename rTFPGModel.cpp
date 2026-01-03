#include "rTFPGModel.h"
#include <algorithm> // For std::copy_if

rTFPGModel::rTFPGModel(const nlohmann::json& model_data) {
    // Parse the "signals" array from the JSON model.
    if (model_data.contains("signals") && model_data["signals"].is_array()) {
        for (const auto& j_signal : model_data["signals"]) {
            Signal signal;
            signal.id = j_signal.at("id").get<std::string>();
            signal.source_name = j_signal.at("source_name").get<std::string>();
            signal.type = j_signal.at("type").get<std::string>();
            signal.units = j_signal.at("units").get<std::string>();
            signal.range_min = j_signal.value("range_min", 0.0);
            signal.range_max = j_signal.value("range_max", 1.0);
            m_signals.push_back(signal);
        }
    }

    // REQ-MOD-03 & REQ-MOD-02: Parse the "nodes" array (Failure Modes and Discrepancies).
    if (model_data.contains("nodes") && model_data["nodes"].is_array()) {
        for (const auto& j_node : model_data["nodes"]) {
            Node node;
            node.id = j_node.at("id").get<std::string>();
            node.name = j_node.at("name").get<std::string>();

            std::string type_str = j_node.at("type").get<std::string>();
            if (type_str == "FailureMode") {
                node.type = NodeType::FailureMode;
            } else { // "Discrepancy"
                node.type = NodeType::Discrepancy;

                // Parse Discrepancy-specific fields.
                std::string gate_type_str = j_node.at("gate_type").get<std::string>();
                if (gate_type_str == "OR") {
                    node.gate_type = GateType::OR;
                } else { // "AND"
                    node.gate_type = GateType::AND;
                }
                node.criticality_level = j_node.at("criticality_level").get<int>();


                // Parse the predicate which defines the condition for the discrepancy to be active.
                const auto& j_predicate = j_node.at("predicate");
                Predicate p;
                p.signal_ref = j_predicate.at("signal_ref").get<std::string>();
                p.op = j_predicate.at("operator").get<std::string>();
                p.threshold = j_predicate.at("threshold").get<double>();
                node.predicate = p;
            }

            // The comment below is outdated as criticality_level is now parsed.
            // If it were present, it would be parsed here, e.g.:
            // node.criticality_level = j_node.value("criticality_level", 0);

            m_nodes.push_back(node);
        }
    }

    // REQ-MOD-01: Parse the "edges" array, which defines the causal and temporal relationships between nodes.
    if (model_data.contains("edges") && model_data["edges"].is_array()) {
        for (const auto& j_edge : model_data["edges"]) {
            Edge edge;
            edge.from = j_edge.at("from").get<std::string>();
            edge.to = j_edge.at("to").get<std::string>();
            edge.time_min_ms = j_edge.at("time_min_ms").get<int>();
            edge.time_max_ms = j_edge.at("time_max_ms").get<int>();

            m_edges.push_back(edge);
        }
    }
}

// REQ-MOD-04: Implementation of GetCriticalityFront
std::vector<Node> rTFPGModel::GetCriticalityFront(int n) const {
    std::vector<Node> front;
    std::copy_if(m_nodes.begin(), m_nodes.end(), std::back_inserter(front),
                 [n](const Node& node) {
                     return node.criticality_level >= n;
                 });
    return front;
}

void rTFPGModel::addNode(const Node& node) {
    // Check if node already exists to avoid duplicates
    for (const auto& n : m_nodes) {
        if (n.id == node.id) return;
    }
    m_nodes.push_back(node);
}

void rTFPGModel::removeNode(const std::string& id) {
    m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                                 [&id](const Node& n) { return n.id == id; }),
                  m_nodes.end());
    // Also remove connected edges
    m_edges.erase(std::remove_if(m_edges.begin(), m_edges.end(),
                                 [&id](const Edge& e) { return e.from == id || e.to == id; }),
                  m_edges.end());
}

void rTFPGModel::addEdge(const Edge& edge) {
    m_edges.push_back(edge);
}

void rTFPGModel::removeEdge(const std::string& from, const std::string& to) {
    m_edges.erase(std::remove_if(m_edges.begin(), m_edges.end(),
                                 [&from, &to](const Edge& e) {
                                     return e.from == from && e.to == to;
                                 }),
                  m_edges.end());
}