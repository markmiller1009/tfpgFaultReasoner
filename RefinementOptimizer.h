#ifndef REFINEMENT_OPTIMIZER_H
#define REFINEMENT_OPTIMIZER_H

/**
 * @class RefinementOptimizer
 * @brief Uses historical data to improve the graph (Offline or "Training" Mode).
 *
 * @requirement REQ-REF-01: Implement Diagnosis Error (DE) metric.
 * @requirement REQ-REF-02: Implement Cut-Set Analysis (Minimal Cut-Sets).
 * @requirement REQ-REF-03: Implement recursive Refine method.
 */

#include "rTFPGModel.h"
#include "SignalIngestor.h"
#include "LogicEngine.h"
#include <vector>
#include <string>
#include <memory>
#include <set>

struct LabeledTrace {
    std::shared_ptr<SignalIngestor> ingestor;
    bool expected_activation; // True if the target node should be active (S+), False otherwise (S-)
};

class RefinementOptimizer {
public:
    explicit RefinementOptimizer(rTFPGModel& model);

    /**
     * @brief REQ-REF-01: Calculates Diagnosis Error (DE).
     *        DE = (False Positives + False Negatives) / Total Samples
     */
    double calculateDiagnosisError(const std::string& targetNodeId, const std::vector<LabeledTrace>& dataset);

    /**
     * @brief REQ-REF-02: Identifies Minimal Cut-Sets (ancestors) of a node.
     *        Returns a set of Node IDs that are upstream of the target.
     */
    std::set<std::string> getMinimalCutSet(const std::string& nodeId);

    /**
     * @brief REQ-REF-03: Recursively modifies graph to minimize DE.
     * @param targetNodeId The node p to optimize.
     * @param candidateSetH A set of disjoint discrepancy nodes available for expansion.
     * @param dataset The labeled training data.
     */
    void refine(const std::string& targetNodeId, 
                const std::vector<Node>& candidateSetH, 
                const std::vector<LabeledTrace>& dataset);

private:
    rTFPGModel& m_model;

    bool tryInternalEdgeAddition(const std::string& p_id, double currentDE, const std::vector<LabeledTrace>& dataset);
};

#endif // REFINEMENT_OPTIMIZER_H