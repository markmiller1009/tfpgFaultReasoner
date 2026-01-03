#ifndef PROGNOSIS_MANAGER_H
#define PROGNOSIS_MANAGER_H

/**
 * @class PrognosisManager
 * @brief Estimates system reliability and Remaining Useful Life (RUL).
 *
 * @requirement REQ-PROG-01: Calculate Hypothesis Plausibility (ratio of consistent/expected alarms).
 * @requirement REQ-PROG-02: Implement Time-To-Criticality (TTC) algorithm (min propagation time).
 * @requirement REQ-PROG-03: Output TTC value as a proxy for RUL.
 */

#include "rTFPGModel.h"
#include "LogicEngine.h" // For NodeState
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

struct PrognosisResult {
    double ttc;
    std::string critical_node_id;
};

class PrognosisManager {
public:
    explicit PrognosisManager(const rTFPGModel& model);

    /**
     * @brief REQ-PROG-01: Calculates Hypothesis Plausibility.
     * @return Ratio of (currently active discrepancy nodes) / (total reachable discrepancy nodes)
     *         starting from the hypothesis node.
     */
    double calculatePlausibility(const std::string& hypothesisId, 
                                 const std::unordered_map<std::string, NodeState>& nodeStates);

    /**
     * @brief REQ-PROG-02 & REQ-PROG-03: Calculates Time-To-Criticality (TTC).
     * @return The minimum time (ms) from any currently active node to any node with 
     *         criticality_level >= criticalityThreshold. Returns -1.0 if unreachable.
     */
    PrognosisResult calculateTTC(const std::unordered_map<std::string, NodeState>& nodeStates, 
                        int criticalityThreshold, double current_time);

private:
    const rTFPGModel& m_model;
    std::unordered_map<std::string, Node> m_node_map;
    std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> m_adj; // ID -> {neighbor, time}

    void buildGraph();
};

#endif // PROGNOSIS_MANAGER_H