#include "PrognosisManager.h"
#include <queue>
#include <set>
#include <algorithm>
#include <iostream>
#include <functional>
#include <limits>

// Constructor for the PrognosisManager.
PrognosisManager::PrognosisManager(const rTFPGModel& model) : m_model(model) {
    // Pre-builds the graph structure for efficient traversal during prognosis calculations.
    buildGraph();
}

// Pre-processes the model to build efficient data structures for graph traversal.
void PrognosisManager::buildGraph() {
    // Cache nodes in a map for O(1) lookup by ID.
    for (const auto& node : m_model.getNodes()) {
        m_node_map[node.id] = node;
    }

    // Build an adjacency list representation of the graph.
    // Each entry maps a node ID to a vector of its children and the minimum propagation time.
    for (const auto& edge : m_model.getEdges()) {
        m_adj[edge.from].push_back({edge.to, edge.time_min_ms});
    }
}

// REQ-PROG-01: Hypothesis Plausibility
// Calculates the plausibility of a given failure hypothesis.
// Plausibility is the ratio of observed symptoms to expected symptoms for that failure.
double PrognosisManager::calculatePlausibility(const std::string& hypothesisId, 
                                               const std::unordered_map<std::string, NodeState>& nodeStates) {
    std::queue<std::string> q;
    q.push(hypothesisId);
    std::set<std::string> visited;
    visited.insert(hypothesisId);

    int totalExpected = 0; // Counter for all symptoms expected to be caused by the hypothesis.
    int consistent = 0;    // Counter for expected symptoms that are actually active.

    while (!q.empty()) {
        std::string currId = q.front();
        q.pop();

        // Check if the current node is a Discrepancy (i.e., a symptom).
        if (m_node_map.count(currId)) {
            const auto& node = m_node_map.at(currId);
            if (node.type == NodeType::Discrepancy) {
                totalExpected++;
                // If the symptom is active in the current state, it is consistent with the hypothesis.
                if (nodeStates.count(currId) && nodeStates.at(currId).is_active) {
                    consistent++;
                }
            }
        }

        // Traverse to all children of the current node.
        if (m_adj.count(currId)) {
            for (const auto& edge : m_adj.at(currId)) {
                if (visited.find(edge.first) == visited.end()) {
                    visited.insert(edge.first);
                    q.push(edge.first);
                }
            }
        }
    }

    if (totalExpected == 0) return 0.0;
    // The plausibility score is the ratio of consistent symptoms to total expected symptoms.
    return static_cast<double>(consistent) / totalExpected;
}

// REQ-PROG-02 & REQ-PROG-03: Time-To-Criticality (TTC)
// TTC is the shortest time from the current state to the activation of a node
// that meets or exceeds the specified criticality threshold.
// This is implemented using Dijkstra's algorithm.
PrognosisResult PrognosisManager::calculateTTC(const std::unordered_map<std::string, NodeState>& nodeStates, 
                                      int criticalityThreshold, double current_time) {
    // Min-priority queue for Dijkstra's algorithm. Stores pairs of {accumulated_time, node_id}.
    using P = std::pair<double, std::string>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    // Map to store the minimum time to reach each node.
    std::unordered_map<std::string, double> min_dist;

    // Initialize the algorithm with the "State Front", which consists of all currently active nodes.
    // The starting time for each is its recorded activation time.
    for (const auto& pair : nodeStates) {
        if (pair.second.is_active) {
            double start_time = pair.second.activation_time_ms;
            pq.push({start_time, pair.first});
            min_dist[pair.first] = start_time;
        }
    }

    // Run Dijkstra's algorithm.
    while (!pq.empty()) {
        double d = pq.top().first;
        std::string u = pq.top().second;
        pq.pop();

        // Check if we have reached a node on the "Criticality Front".
        if (m_node_map.count(u) && m_node_map.at(u).criticality_level >= criticalityThreshold) {
            // If so, we have found the shortest path to a critical failure.
            double ttc = d - current_time;
            // std::cout << "[Prognosis] Critical node " << u << " (Level " << m_node_map.at(u).criticality_level 
            //           << ") predicted at " << d << "ms. TTC: " << ttc << "ms";
            // if (ttc < 0) std::cout << " (Overdue)";
            // std::cout << ".\n";
            // Return the time difference from now.
            return {ttc, u};
        }

        // Optimization: if we found a shorter path to `u` already, skip.
        if (min_dist.count(u) && d > min_dist.at(u)) continue;

        // Explore neighbors (children in the graph).
        if (m_adj.count(u)) {
            for (const auto& edge : m_adj.at(u)) {
                std::string v = edge.first;

                // If the downstream node is already active, we must respect its observed
                // activation time and not overwrite it with a theoretical prediction.
                if (nodeStates.count(v) && nodeStates.at(v).is_active) {
                    continue;
                }

                // The weight of the edge is the minimum propagation time.
                int weight = edge.second; // t_propagation

                // If we found a new shorter path to `v`, update its distance and add it to the queue.
                if (!min_dist.count(v) || min_dist[v] > d + weight) {
                    min_dist[v] = d + weight;
                    pq.push({min_dist[v], v});
                }
            }
        }
    }

    // If the loop completes without finding a path to a critical node, return -1.
    return {std::numeric_limits<double>::infinity(), ""}; // No critical node reachable
}