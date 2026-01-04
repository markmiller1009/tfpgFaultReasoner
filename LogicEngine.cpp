#include "LogicEngine.h"
#include <iostream> // For placeholder output
#include <algorithm>
#include <vector>
#include <cmath>
#include <map>
#include <set>
#include <functional>

LogicEngine::LogicEngine(const rTFPGModel& model, const SignalIngestor& ingestor)
    : m_model(model), m_ingestor(ingestor) {
    // Initialize node states for all nodes in the model
    for (const auto& node : m_model.getNodes()) {
        m_node_states[node.id] = NodeState{};
    }
}

/**
 * @brief REQ-ENG-03: Calculates the robustness for a single predicate.
 * @refinement Returns a positive value if the predicate is satisfied, negative if violated.
 */
double calculateRobustness(const Predicate& predicate, double signal_value, double range_min, double range_max) {
    double raw_val = 0.0;
    if (predicate.op == ">") {
        raw_val = signal_value - predicate.threshold;
    } else if (predicate.op == "<") {
        raw_val = predicate.threshold - signal_value;
    }
    // Extend with other operators like ==, >=, <= as needed.
    
    double range = range_max - range_min;
    if (range <= 1e-9) return raw_val; // Avoid division by zero, return raw
    
    return raw_val / range;
}

/**
 * @brief REQ-ENG-01: Evaluates all discrepancy node predicates against the full signal trace.
 */
void evaluateSignalTrace(const rTFPGModel& model, const SignalIngestor& ingestor, std::unordered_map<std::string, NodeState>& node_states) {
    // std::cout << "--- Evaluating Signal Trace ---" << std::endl;
    // This is a simplified approach that iterates through all ingested samples.
    // A real-time system would process samples as they arrive.
    for (const auto& sample : ingestor.getSamples()) {
        // Check if the sample corresponds to a fault injection or a sensor reading
        bool is_sensor_reading = false;
        for (const auto& signal : model.getSignals()) {
            if (signal.source_name == sample.parameterID) {
                is_sensor_reading = true;
                break;
            }
        }

        if (is_sensor_reading) {
            for (const auto& node : model.getNodes()) {
                if (node.type == NodeType::Discrepancy && node.predicate) {
                    // Inefficient lookup, a map would be better.
                    std::string source_name_for_predicate;
                    double range_min = 0.0;
                    double range_max = 1.0;
                    for (const auto& sig : model.getSignals()) {
                        if (sig.id == node.predicate->signal_ref) {
                            source_name_for_predicate = sig.source_name;
                            range_min = sig.range_min;
                            range_max = sig.range_max;
                            break;
                        }
                    }

                    if (source_name_for_predicate == sample.parameterID) {
                        double robustness = calculateRobustness(*node.predicate, sample.value, range_min, range_max);
                        
                        // Update robustness for inactive nodes to reflect current state (e.g. negative or positive-but-blocked)
                        if (!node_states[node.id].is_active) {
                            node_states[node.id].robustness = robustness;
                        }

                        if (robustness > 0 && !node_states[node.id].is_active) {
                            bool condition_met = true;
                            if (node.gate_type == GateType::AND) {
                                for (const auto& edge : model.getEdges()) {
                                    if (edge.to == node.id) {
                                        if (!node_states[edge.from].is_active || node_states[edge.from].activation_time_ms > sample.timestamp_ms) {
                                            condition_met = false;
                                            break;
                                        }
                                    }
                                }
                            }

                            if (condition_met) {
                                node_states[node.id].is_active = true;
                                node_states[node.id].robustness = robustness;
                                node_states[node.id].activation_time_ms = sample.timestamp_ms;
                                std::cout << "Node " << node.id << " (" << node.name << ") activated at time " << sample.timestamp_ms << "ms";
                                std::cout << " (" << source_name_for_predicate << ": " << sample.value << node.predicate->op << node.predicate->threshold << ").\n";
                                node_states[node.id].trigger_value = sample.value;
                            }
                        }
                    }
                }
            }
        } else {
            // This is a fault injection (e.g., "Pump_Motor_Burnout")
            std::string target_node_id = sample.parameterID;
            bool found = false;

            if (node_states.find(target_node_id) != node_states.end()) {
                found = true;
            } else {
                for (const auto& node : model.getNodes()) {
                    if (node.name == sample.parameterID) {
                        target_node_id = node.id;
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                auto& state = node_states[target_node_id];
                if (!state.is_active && sample.value > 0) {
                    state.is_active = true;
                    state.activation_time_ms = sample.timestamp_ms;
                    std::cout << "FAULT INJECTED: " << sample.parameterID << " activated at time " << sample.timestamp_ms << "ms.\n";
                    state.trigger_value = sample.value;
                    // std::cout << "FAULT INJECTED: " << sample.parameterID << " activated at time " << sample.timestamp_ms << "ms.\n";
                }
            }
        }
    }
}

// REQ-ENG-04: Main function to run the reasoning process.
std::vector<DiagnosisResult> LogicEngine::findActiveHypotheses() {
    // std::cout << "--- Starting Logic Engine ---" << std::endl;

    // 1. Evaluate predicates based on signal data (REQ-ENG-01, REQ-ENG-03) to detect Discrepancies
    evaluateSignalTrace(m_model, m_ingestor, m_node_states);

    // Build a lookup map for nodes
    std::unordered_map<std::string, Node> node_map;
    for (const auto& node : m_model.getNodes()) {
        node_map[node.id] = node;
    }

    // Identify newly active discrepancies (Symptoms)
    std::vector<std::string> active_symptoms;
    for (const auto& [id, state] : m_node_states) {
        if (state.is_active && node_map.count(id) && node_map.at(id).type == NodeType::Discrepancy) {
            active_symptoms.push_back(id);
        }
    }

    std::set<std::string> candidate_failures;

    // 2. Backward Propagation (BProp) - Trace back to find potential root causes
    std::function<void(std::string)> backwardPropagate = [&](std::string current_id) {
        for (const auto& edge : m_model.getEdges()) {
            if (edge.to == current_id) {
                std::string parent_id = edge.from;
                
                if (node_map.at(parent_id).type == NodeType::FailureMode) {
                    candidate_failures.insert(parent_id);
                } else if (node_map.at(parent_id).type == NodeType::Discrepancy) {
                    // Check consistency: Parent must be active and within time window
                    if (m_node_states.count(parent_id) && m_node_states.at(parent_id).is_active) {
                        double t_child = m_node_states.at(current_id).activation_time_ms;
                        double t_parent = m_node_states.at(parent_id).activation_time_ms;
                        double delta = t_child - t_parent;

                        if (delta >= edge.time_min_ms && delta <= edge.time_max_ms) {
                            backwardPropagate(parent_id);
                        }
                    }
                }
            }
        }
    };

    for (const auto& symptom : active_symptoms) {
        backwardPropagate(symptom);
    }

    // 3. Forward Propagation (FProp) & Consistency Check
    std::vector<DiagnosisResult> ranked_diagnoses;

    for (const auto& fm_id : candidate_failures) {
        // Calculate Plausibility: (Consistent Symptoms / Expected Symptoms)
        std::set<std::string> expected_symptoms;
        std::vector<std::string> queue = {fm_id};
        std::set<std::string> visited = {fm_id};
        
        while (!queue.empty()) {
            std::string u = queue.front();
            queue.erase(queue.begin());
            for (const auto& edge : m_model.getEdges()) {
                if (edge.from == u && visited.find(edge.to) == visited.end()) {
                    visited.insert(edge.to);
                    queue.push_back(edge.to);
                    if (node_map.at(edge.to).type == NodeType::Discrepancy) {
                        expected_symptoms.insert(edge.to);
                    }
                }
            }
        }

        int consistent_count = 0;
        double sum_all_robustness = 0.0;
        std::vector<std::string> consistent_symptoms;
        std::map<std::string, double> symptom_values;
        for (const auto& s_id : expected_symptoms) {
            if (m_node_states.count(s_id)) {
                sum_all_robustness += m_node_states.at(s_id).robustness;
                if (m_node_states.at(s_id).is_active) {
                    consistent_count++;
                    consistent_symptoms.push_back(s_id);
                    symptom_values[s_id] = m_node_states.at(s_id).trigger_value;
                }
            }
        }

        double plausibility = expected_symptoms.empty() ? 0.0 : (double)consistent_count / expected_symptoms.size();
        
        // Calculate Aggregate Robustness normalized between -1.0 and 1.0
        double aggregate_robustness = 0.0;
        if (!expected_symptoms.empty()) {
            aggregate_robustness = sum_all_robustness / expected_symptoms.size();
            // Clamp to -1.0 to 1.0
            aggregate_robustness = std::max(-1.0, std::min(1.0, aggregate_robustness));
        }

        if (plausibility > 0.0) {
            ranked_diagnoses.push_back({node_map.at(fm_id), plausibility, aggregate_robustness, expected_symptoms, consistent_symptoms, symptom_values});
        }
    }

    // 4. Rank by Plausibility/Robustness
    std::sort(ranked_diagnoses.begin(), ranked_diagnoses.end(), [](const DiagnosisResult& a, const DiagnosisResult& b) {
        if (std::abs(a.plausibility - b.plausibility) > 1e-6) {
            return a.plausibility > b.plausibility;
        }
        return a.robustness > b.robustness;
    });

    // std::cout << "--- Logic Engine Finished ---\n" << std::endl;
    return ranked_diagnoses;
}