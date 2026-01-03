#ifndef LOGIC_ENGINE_H
#define LOGIC_ENGINE_H

/**
 * @class LogicEngine
 * @brief Determines which nodes are active based on signal history.
 *
 * @requirement REQ-ENG-01: The engine shall implement a Mapping Function (ΠG) that converts
 *                         the buffered SignalTrace into a discrete state vector π[t].
 * @requirement REQ-ENG-02: The engine shall implement Signal Temporal Logic (STL) operators,
 *                         specifically the "Until" operator U[t_min, t_max].
 * @requirement REQ-ENG-03: The engine shall calculate the Robustness Degree ρ(ϕ,x).
 * @requirement REQ-ENG-04: The engine shall implement Hypothesis Tracking to identify the
 *                         "Activation Graph" (AG).
 */

#include "rTFPGModel.h"
#include "SignalIngestor.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <set>

// Represents the activation state of a node at a specific time.
struct NodeState {
    bool is_active = false;
    double robustness = 0.0; // REQ-ENG-03
    uint64_t activation_time_ms = 0;
    double trigger_value = 0.0;
};

/// @brief Holds the result of a diagnosis, including the failure node and scoring metrics.
struct DiagnosisResult {
    Node node;
    double plausibility;
    double robustness;
    std::set<std::string> expected_symptoms;
    std::vector<std::string> consistent_symptoms;
    std::map<std::string, double> symptom_values;
};

class LogicEngine {
public:
    LogicEngine(const rTFPGModel& model, const SignalIngestor& ingestor);

    // REQ-ENG-04: Main function to run the reasoning process.
    std::vector<DiagnosisResult> findActiveHypotheses();

    const std::unordered_map<std::string, NodeState>& getNodeStates() const { return m_node_states; }

private:
    const rTFPGModel& m_model;
    const SignalIngestor& m_ingestor;

    // Maps node ID to its current state (active, robustness, time)
    std::unordered_map<std::string, NodeState> m_node_states;
};

#endif // LOGIC_ENGINE_H