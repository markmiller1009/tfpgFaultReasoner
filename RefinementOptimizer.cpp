#include "RefinementOptimizer.h"
#include <iostream>
#include <algorithm>
#include <queue>

// Constructor for the RefinementOptimizer.
RefinementOptimizer::RefinementOptimizer(rTFPGModel& model) : m_model(model) {}

// REQ-REF-01: Diagnosis Error (DE)
// Calculates the Diagnosis Error (DE) for a target node.
// DE is the ratio of misclassifications over a dataset of labeled traces.
double RefinementOptimizer::calculateDiagnosisError(const std::string& targetNodeId, const std::vector<LabeledTrace>& dataset) {
    if (dataset.empty()) return 0.0;

    int misclassifications = 0;

    // Iterate through each labeled trace in the dataset.
    for (const auto& trace : dataset) {
        // Instantiate a temporary engine to run a diagnosis on this specific trace.
        LogicEngine engine(m_model, *trace.ingestor);
        // Run the diagnosis to get the active hypotheses and node states.
        auto active_nodes = engine.findActiveHypotheses(); // This runs the logic
        const auto& node_states = engine.getNodeStates();

        // Check if the target node was activated in the simulation.
        bool is_active = false;
        if (node_states.count(targetNodeId)) {
            is_active = node_states.at(targetNodeId).is_active;
        }

        // Compare the simulation result with the ground truth from the labeled trace.
        if (is_active != trace.expected_activation) {
            misclassifications++;
        }
    }

    // Return the error rate.
    return static_cast<double>(misclassifications) / dataset.size();
}

// REQ-REF-02: Cut-Set Analysis (Simplified as Ancestor Traversal for TFPG)
// Finds all ancestor nodes of a given node, which form the basis for refinement.
std::set<std::string> RefinementOptimizer::getMinimalCutSet(const std::string& nodeId) {
    std::set<std::string> mcs;
    std::queue<std::string> q;
    q.push(nodeId);
    std::set<std::string> visited;
    visited.insert(nodeId);

    // Perform a backward Breadth-First Search (BFS) to find all ancestors.
    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();

        for (const auto& edge : m_model.getEdges()) {
            if (edge.to == curr) {
                mcs.insert(edge.from);
                if (visited.find(edge.from) == visited.end()) {
                    visited.insert(edge.from);
                    q.push(edge.from);
                }
            }
        }
    }
    return mcs;
}

// REQ-REF-03: Recursive Refine Method
// This is the core of the model refinement algorithm. It attempts to improve the model
// by adding or modifying nodes and edges to reduce the Diagnosis Error.
void RefinementOptimizer::refine(const std::string& p_id, 
                                 const std::vector<Node>& candidateSetH, 
                                 const std::vector<LabeledTrace>& dataset) {
    
    // Calculate the current diagnosis error for the node to be refined.
    double currentDE = calculateDiagnosisError(p_id, dataset);
    // If there is no error, no refinement is needed.
    if (currentDE == 0.0) return;

    std::cout << "Refining Node: " << p_id << " (Current DE: " << currentDE << ")" << std::endl;

    // 1. Successor Selection: Try to find a successor node with a lower or equal DE.
    // If found, recurse on that successor. This prioritizes refining downstream nodes first.
    for (const auto& edge : m_model.getEdges()) {
        if (edge.from == p_id) {
            std::string d_prime_id = edge.to;
            double successorDE = calculateDiagnosisError(d_prime_id, dataset);
            
            if (successorDE <= currentDE) {
                std::cout << "  -> Traversing to successor: " << d_prime_id << std::endl;
                refine(d_prime_id, candidateSetH, dataset);
                return; // Recurse on d'
            }
        }
    }

    // 2. Edge Addition (Internal): Try adding an edge from an existing node to the current node `p`.
    // This explores if a missing causal link can explain the error.
    std::set<std::string> mcs = getMinimalCutSet(p_id);
    for (const auto& node : m_model.getNodes()) {
        // Consider adding an edge from another discrepancy node that is not already an ancestor.
        if (node.type == NodeType::Discrepancy && node.id != p_id && mcs.find(node.id) == mcs.end()) {
            // Tentatively add the new edge.
            Edge newEdge = {node.id, p_id, 0, 1000}; // Default interval
            m_model.addEdge(newEdge);

            // Check if this change reduces the diagnosis error.
            double newDE = calculateDiagnosisError(p_id, dataset);
            if (newDE < currentDE) {
                std::cout << "  -> Added internal edge: " << node.id << " -> " << p_id << std::endl;
                // If it helps, keep the edge and recurse on `p`.
                refine(p_id, candidateSetH, dataset); // Recurse on p
                return;
            } else {
                // Otherwise, revert the change.
                m_model.removeEdge(node.id, p_id); // Revert
            }
        }
    }

    // 3. Node Expansion (External): Try adding a new candidate node from `candidateSetH` to the model.
    for (const auto& d_prime : candidateSetH) {
        // Check if d_prime is already in the graph to avoid duplicates in this simplified logic
        bool exists = false;
        for(const auto& n : m_model.getNodes()) if(n.id == d_prime.id) exists = true;
        if(exists) continue;

        // Add the new candidate node to the model.
        m_model.addNode(d_prime);
        // Case A: Create an edge from `p` to the new node `d_prime`.
        Edge edgeA = {p_id, d_prime.id, 0, 1000};
        m_model.addEdge(edgeA);

        // Check if this new structure reduces the DE of the new node `d_prime`.
        double de_d_prime = calculateDiagnosisError(d_prime.id, dataset);
        
        if (de_d_prime < currentDE) {
             std::cout << "  -> Expanded (Case A): " << p_id << " -> " << d_prime.id << std::endl;
             // If successful, keep the change and recurse on the new node `d_prime`.
             refine(d_prime.id, candidateSetH, dataset); // Recurse on d'
             return;
        } else {
            // Revert Case A.
            m_model.removeEdge(p_id, d_prime.id);
            // Don't remove node yet, might be used in Case B
        }

        // Case B: Create an edge from a predecessor of `p` to the new node `d_prime`.
        bool improvementFound = false;
        std::vector<std::string> predecessors;
        for(const auto& e : m_model.getEdges()) {
            if(e.to == p_id) predecessors.push_back(e.from);
        }

        for(const auto& v_id : predecessors) {
            Edge edgeB = {v_id, d_prime.id, 0, 1000};
            m_model.addEdge(edgeB);
            
            // Requirement: "If this reduces the DE of p"
            // Note: Adding an edge to d' doesn't inherently change p's logic unless p depends on d'.
            // However, if d' becomes a new intermediate node, we might need to restructure p to depend on d'.
            // The requirement as written implies simply adding the branch might help if p's logic was dynamic.
            // For this implementation, we follow the instruction strictly: add edge, check DE(p).
            double newDE = calculateDiagnosisError(p_id, dataset);
            
            if (newDE < currentDE) {
                std::cout << "  -> Expanded (Case B): " << v_id << " -> " << d_prime.id << std::endl;
                improvementFound = true;
                break; 
            } else {
                m_model.removeEdge(v_id, d_prime.id);
            }
        }

        if (improvementFound) {
            // If successful, keep the change and recurse on `p`.
            refine(p_id, candidateSetH, dataset); // Recurse on p
            return;
        }
        
        // If neither case worked, remove the candidate node from the model.
        m_model.removeNode(d_prime.id);
    }
}