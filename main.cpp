#include <iostream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <map>
#include <set>

// This code assumes you have the nlohmann/json library available.
// If using a package manager like vcpkg: vcpkg install nlohmann-json
// Or include the single header file directly.

#include "json.hpp"
#include "LogicEngine.h"
#include "SignalIngestor.h"
#include "PrognosisManager.h"


using json = nlohmann::json;

int main(int argc, char* argv[]) {

    if (argc < 3 || argc > 5) {
        std::cerr << "Usage: " << argv[0] << " <fault_model.json> <test_data.json> [criticality_threshold] [output_log_file]" << std::endl;
        return 1;
    }

    int criticality_threshold = 5; // Default value
    std::string output_log_file = "";

    // Parse optional arguments
    if (argc >= 4) {
        try {
            size_t pos;
            criticality_threshold = std::stoi(argv[3], &pos);
            if (pos != std::string(argv[3]).length()) throw std::invalid_argument("Not an integer");
            
            if (argc == 5) {
                output_log_file = argv[4];
            }
        } catch (...) {
            // argv[3] is not an integer, assume it's the output file
            if (argc == 5) {
                std::cerr << "Error: Invalid criticality threshold '" << argv[3] << "'. Must be an integer if output file is also provided." << std::endl;
                return 1;
            }
            output_log_file = argv[3];
        }
    }

    std::ofstream logFile;
    std::streambuf* cout_backup = nullptr;
    if (!output_log_file.empty()) {
        logFile.open(output_log_file);
        if (!logFile.is_open()) {
            std::cerr << "Error: Could not open log file: " << output_log_file << std::endl;
            return 1;
        }
        logFile << "Fault Model: " << argv[1] << "\nTest Data: " << argv[2] << "\n";
        logFile << "--------------------------------------------------\n";
        cout_backup = std::cout.rdbuf();
        std::cout.rdbuf(logFile.rdbuf());
    }

    // ---------------------------------------------------------
    // 1. Load and Parse Static Fault Model
    // ---------------------------------------------------------
    // Define the path to the fault model file.
    std::string filePath = argv[1];
    std::ifstream inputFile(filePath);

    // Error handling for file opening.
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open model file: " << filePath << std::endl;
        return 1;
    }

    // Parse the JSON data from the file.
    json modelData;
    try {
        modelData = json::parse(inputFile);
    } catch (const json::parse_error& e) {
        std::cerr << "Model JSON Parse Error: " << e.what() << std::endl;
        return 1;
    }

    // Initialize System Objects
    // REQ-MOD-01 to 04: Load static graph definitions from the parsed JSON data.
    rTFPGModel rtfpg(modelData); 
    
    // REQ-IN-03: Initialize the signal ingestor, which maps signal names to internal IDs for efficient lookup.
    SignalIngestor ingestor(modelData); 

    // REQ-ENG-01: Initialize the Logic Engine, providing it with the model and a reference to the ingestor for signal history.
    LogicEngine engine(rtfpg, ingestor); 

    // REQ-PROG-02: Initialize the Prognosis Manager with the fault model to calculate future failure states.
    PrognosisManager prognosis(rtfpg); 

    std::cout << "System Initialized. Nodes: " << rtfpg.getNodes().size() << std::endl;

    // Create lookup maps for detailed reporting
    std::map<std::string, Node> node_lookup;
    for (const auto& node : rtfpg.getNodes()) {
        node_lookup[node.id] = node;
    }
    std::map<std::string, Signal> signal_lookup;
    for (const auto& sig : rtfpg.getSignals()) {
        signal_lookup[sig.id] = sig;
    }

    // ---------------------------------------------------------
    // 2. Load Test Data Stream
    // ---------------------------------------------------------
    // Define the path to the test data file.
    std::string testDataPath = argv[2];
    std::ifstream testDataFile(testDataPath);

    // Error handling for file opening.
    if (!testDataFile.is_open()) {
        std::cerr << "Error: Could not open test data file: " << testDataPath << std::endl;
        return 1;
    }

    // Parse the JSON test data from the file.
    json testData;
    try {
        testData = json::parse(testDataFile);
    } catch (const json::parse_error& e) {
        std::cerr << "Test Data JSON Parse Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Starting Simulation: " << testData["scenario_id"] << "\n" << std::endl;

    // ---------------------------------------------------------
    // 3. Real-Time Processing Loop
    // ---------------------------------------------------------
    // Define a criticality threshold for prognosis. Any node with a criticality level
    // greater than or equal to this value is considered a critical failure.

    std::cout << "\nUsing Criticality Threshold: " << criticality_threshold << std::endl;

    std::set<std::string> last_active_symptoms;
    std::map<std::string, double> last_robustness_scores;
    double last_ttc = std::numeric_limits<double>::infinity();

    // Iterate through each event in the "data_stream" array of the test data.
    for (const auto& event : testData["data_stream"]) {
        // Skip comment blocks in the JSON stream, which are used for documentation.
        if (event.contains("comment")) continue;

        // A. Construct DataSample (REQ-IN-01)
        // Create a DataSample struct for the current event.
        DataSample sample;
        sample.timestamp_ms = event["timestamp_ms"];
        sample.parameterID = event["parameter_id"];
        // The 'is_failure_mode' field distinguishes between sensor readings and fault injections.
        sample.is_failure_mode = event.value("is_failure_mode", false);
        
        // Handle boolean vs double inputs (Source 213: signals are continuous x: N -> R^n)
        // Convert boolean JSON values to 1.0 or 0.0 to treat all signals as continuous values.
        if (event["value"].is_boolean()) {
            sample.value = event["value"] ? 1.0 : 0.0;
        } else {
            sample.value = event["value"];
        }

        // B. Ingest Signal (REQ-IN-02)
        // Add the current sample to the ingestor's buffer. This history is used by the Logic Engine.
        ingestor.ingest(sample);

        // C. Run Diagnosis (REQ-ENG-04)
        // The Logic Engine processes the accumulated signal history to find any active failure hypotheses.
        auto diagnoses = engine.findActiveHypotheses();
        const auto& nodeStates = engine.getNodeStates();

        // 1. Check for changes in active symptoms
        std::set<std::string> current_active_symptoms;
        for (const auto& [id, state] : nodeStates) {
            if (state.is_active && node_lookup.count(id) && node_lookup.at(id).type == NodeType::Discrepancy) {
                current_active_symptoms.insert(id);
            }
        }
        bool symptoms_changed = (current_active_symptoms != last_active_symptoms);
        last_active_symptoms = current_active_symptoms;

        // 2. Check for changes in robustness scores
        bool robustness_changed = false;
        std::map<std::string, double> current_robustness_scores;
        for (const auto& diag : diagnoses) {
            current_robustness_scores[diag.node.id] = diag.robustness;
            if (last_robustness_scores.find(diag.node.id) == last_robustness_scores.end() ||
                std::abs(last_robustness_scores[diag.node.id] - diag.robustness) > 1e-6) {
                robustness_changed = true;
            }
        }
        if (last_robustness_scores.size() != current_robustness_scores.size()) {
            robustness_changed = true;
        }
        if (robustness_changed && !diagnoses.empty()) {
            std::cout << "Robustness metrics updated based on new evidence.\n";
        }
        last_robustness_scores = current_robustness_scores;

        // E. Run Prognosis (REQ-PROG-02/03)
        // Calculate the Time-To-Criticality (TTC) *before* deciding to print, as it's a trigger.
        const auto prognosis_result = prognosis.calculateTTC(nodeStates, criticality_threshold, sample.timestamp_ms);
        const double ttc = prognosis_result.ttc;
        const std::string& target_id = prognosis_result.critical_node_id;

        // 3. Check for TTC expiration (TTC reaches 0)
        // Note: With updated PrognosisManager, ttc will only be <= 0 if a FUTURE event is overdue.
        // Active events are skipped.
        bool ttc_expired = (ttc <= 0 && last_ttc > 0);
        if (ttc_expired && !diagnoses.empty()) {
            std::cout << "CRITICAL PROGNOSIS UPDATE: Prediction for " << target_id << " is now OVERDUE.\n";
        }
        last_ttc = ttc;

        // D. Output Diagnosis Results
        // If any failure modes are identified as active hypotheses, output the results.
        if (!diagnoses.empty() && (symptoms_changed || robustness_changed || ttc_expired)) {
            std::cout << "\n==============================================================================\n";
            std::cout << "[Time: " << sample.timestamp_ms << "ms] SYSTEM DIAGNOSTIC REPORT\n";
            std::cout << "==============================================================================\n";

            std::vector<DiagnosisResult> tier1;
            std::vector<DiagnosisResult> tier2;

            for (const auto& diag : diagnoses) {
                if (diag.plausibility >= 1.0) {
                    tier1.push_back(diag);
                } else {
                    tier2.push_back(diag);
                }
            }

            // --- TIER 1: PRIMARY DIAGNOSIS ---
            std::cout << "\n[TIER 1] PRIMARY DIAGNOSIS (Confidence: 100%)\n";
            std::cout << "------------------------------------------------------------------------------\n";
            
            std::cout << "SYSTEM PROGNOSIS:\n";
            
            // Check for CURRENTLY ACTIVE critical nodes
            std::string active_critical_id = "";
            int max_crit_level = -1;
            for (const auto& [id, state] : nodeStates) {
                if (state.is_active && node_lookup.count(id)) {
                    int cl = node_lookup.at(id).criticality_level;
                    if (cl >= criticality_threshold && cl > max_crit_level) {
                        max_crit_level = cl;
                        active_critical_id = id;
                    }
                }
            }

            if (!active_critical_id.empty()) {
                std::cout << "   - CRITICAL FAILURE ACTIVE (Target: " << active_critical_id << ").\n";
                bool target_is_active = nodeStates.count(target_id) && nodeStates.at(target_id).is_active;
                if (ttc > 0 && ttc != std::numeric_limits<double>::infinity() && target_id != active_critical_id && !target_is_active) {
                    std::cout << "   - WARNING: Cascading Failure expected in " << ttc << " ms (Target: " << target_id << ").\n";
                }
            } else if (ttc == std::numeric_limits<double>::infinity()) {
                 std::cout << "   - System stable.\n";
            } else if (ttc > 0) {
                std::cout << "   - WARNING: Failure expected in " << ttc << " ms (Target: " << target_id << ").\n";
            } else {
                std::cout << "   - Latent Risk (Target: " << target_id << ").\n";
            }
            std::cout << "\n";

            // Helper lambda to determine symptom status
            auto get_symptom_status = [&](const std::string& id, double current_time) -> std::pair<std::string, std::string> {
                if (nodeStates.count(id) && nodeStates.at(id).is_active) {
                    return {"CONFIRMED", ""};
                }
                
                // Check parents
                std::vector<const Edge*> incoming;
                for (const auto& edge : rtfpg.getEdges()) {
                    if (edge.to == id) incoming.push_back(&edge);
                }
                
                if (incoming.empty()) return {"MISSING", "No parents"};

                const auto& node_def = node_lookup.at(id);
                bool is_and = (node_def.gate_type == GateType::AND);
                
                if (is_and) {
                    // AND Gate: All parents must be active
                    for (const auto* edge : incoming) {
                        if (!nodeStates.count(edge->from) || !nodeStates.at(edge->from).is_active) {
                            return {"UNREACHABLE", "Parent " + edge->from + " is inactive"};
                        }
                    }
                    // All parents active. Check timing of the LATEST parent trigger.
                    double max_act_time = -1.0;
                    const Edge* triggering_edge = nullptr;
                    for (const auto* edge : incoming) {
                        if (nodeStates.at(edge->from).activation_time_ms > max_act_time) {
                            max_act_time = nodeStates.at(edge->from).activation_time_ms;
                            triggering_edge = edge;
                        }
                    }
                    
                    if (triggering_edge) {
                        double delta = current_time - max_act_time;
                        if (delta < triggering_edge->time_min_ms) return {"PENDING", "Propagation Delay"};
                        if (delta > triggering_edge->time_max_ms) return {"MISSING", "Overdue"};
                        return {"MISSING", "Should be active"};
                    }
                } else {
                    // OR Gate: At least one parent must be active
                    bool any_active = false;
                    bool any_overdue = false;
                    bool all_pending = true;
                    
                    for (const auto* edge : incoming) {
                        if (nodeStates.count(edge->from) && nodeStates.at(edge->from).is_active) {
                            any_active = true;
                            double delta = current_time - nodeStates.at(edge->from).activation_time_ms;
                            if (delta > edge->time_max_ms) any_overdue = true;
                            if (delta >= edge->time_min_ms) all_pending = false;
                        }
                    }
                    
                    if (!any_active) return {"UNREACHABLE", "Parent inactive"};
                    if (any_overdue) return {"MISSING", "Overdue"};
                    if (all_pending) return {"PENDING", "Propagation Delay"};
                    return {"MISSING", "Should be active"};
                }
                return {"UNKNOWN", ""};
            };

            if (tier1.empty()) {
                if (!tier2.empty()) {
                    std::cout << "[!] WARNING: Active symptoms explained only by low-confidence hypotheses.\n\n";
                }
                std::cout << "    - None\n";
            } else {
                std::cout << "FAULTS DETECTED:\n";

                int idx = 1;

                for (const auto& d : tier1) {
                    std::cout << "    " << idx++ << ". " << d.node.name << " (" << d.node.id << ")\n";
                    // Tier 1 is 100% confidence, so status is generally Verified.
                    // But we can check for pending items.
                    // Since Plausibility=1.0, there are no MISSING items (consistent=expected).
                    // So we assume Verified.
                    std::cout << "       > Status: VERIFIED\n";
                    
                    std::cout << "       > Active Symptoms:\n";
                    for (const auto& id : d.consistent_symptoms) {
                        std::string name = node_lookup.count(id) ? node_lookup.at(id).name : "Unknown";
                        std::string time_str = "Inactive";
                        if (nodeStates.count(id) && nodeStates.at(id).is_active) {
                            time_str = std::to_string(nodeStates.at(id).activation_time_ms) + "ms";
                        }
                        std::cout << "         - " << id << " (" << name << ") activated at " << time_str << "\n";
                    }

                }
            }

            // --- TIER 2: ALTERNATIVE HYPOTHESES ---
            if (!tier2.empty()) {
                std::cout << "\n------------------------------------------------------------------------------\n";
                std::cout << "[TIER 2] PARTIAL HYPOTHESES (Confidence: < 100%)\n";
                std::cout << "------------------------------------------------------------------------------\n";
                std::cout << "POTENTIAL FAULTS:\n";
                
                for (const auto& d : tier2) {
                    // Determine Hypothesis Status
                    std::string hyp_status = "CONFIRMED";
                    int pending_cnt = 0;
                    int missing_cnt = 0;
                    int unreachable_cnt = 0;
                    
                    for (const auto& id : d.expected_symptoms) {
                        auto status = get_symptom_status(id, sample.timestamp_ms);
                        if (status.first == "PENDING") pending_cnt++;
                        else if (status.first == "UNREACHABLE") unreachable_cnt++;
                        else if (status.first == "MISSING") missing_cnt++;
                    }

                    if (d.node.type == NodeType::FailureMode) {
                        if (missing_cnt == 0 && (pending_cnt > 0 || unreachable_cnt > 0)) hyp_status = "VERIFIED (Propagating)";
                        else if (d.plausibility > 0.8) hyp_status = "VERIFIED (Root Cause Active)";
                        else hyp_status = "POSSIBLE (Weak Evidence)";
                    } else {
                         if (missing_cnt > 0) hyp_status = "LOW CONFIDENCE (Precursors Missing)";
                         else if (pending_cnt > 0) hyp_status = "VERIFIED (Awaiting Propagation)";
                         else hyp_status = "CONFIRMED";
                    }


                    std::cout << "[?] " << d.node.name << " (" << d.node.id << ") [Confidence: " << (d.plausibility * 100.0) << "%]\n";
                    std::cout << "    > Status: " << hyp_status << "\n";
                    
                    std::cout << "    > Active Symptoms:\n";
                    for (const auto& id : d.consistent_symptoms) {
                        std::string name = node_lookup.count(id) ? node_lookup.at(id).name : "Unknown";
                        std::string time_str = "Inactive";
                        if (nodeStates.count(id) && nodeStates.at(id).is_active) {
                            time_str = std::to_string(nodeStates.at(id).activation_time_ms) + "ms";
                        }
                        std::cout << "      - " << id << " (" << name << ") activated at " << time_str << "\n";
                    }

                    std::cout << "    > Missing / Inactive Symptoms:\n";
                    for (const auto& id : d.expected_symptoms) {
                        bool active = false;
                        double rob = 0.0;
                        if (nodeStates.count(id)) {
                            active = nodeStates.at(id).is_active;
                            rob = nodeStates.at(id).robustness;
                        }
                        
                        if (!active) {
                            std::string name = node_lookup.count(id) ? node_lookup.at(id).name : "Unknown";
                            
                            auto status = get_symptom_status(id, sample.timestamp_ms);
                            if (status.first == "UNREACHABLE") {
                                std::cout << "      - " << id << " (" << name << ") is UNREACHABLE (" << status.second << ")\n";
                            } else if (status.first == "PENDING") {
                                std::cout << "      - " << id << " (" << name << ") is PENDING (" << status.second << ")\n";
                            } else {
                                std::cout << "      - " << id << " (" << name << ") is MISSING (" << status.second << ")\n";
                            }
                        }
                    }
                    std::cout << "\n";
                }
            }

            // --- UNEXPLAINED SYMPTOMS ---
            std::cout << "------------------------------------------------------------------------------\n";
            std::cout << "[TIER 3] UNEXPLAINED SYMPTOMS:\n";
            std::cout << "------------------------------------------------------------------------------\n";
            
            std::set<std::string> explained_symptoms;
            for (const auto& d : tier1) {
                for (const auto& s : d.consistent_symptoms) explained_symptoms.insert(s);
            }
            for (const auto& d : tier2) {
                for (const auto& s : d.consistent_symptoms) explained_symptoms.insert(s);
            }

            bool found_unexplained = false;
            for (const auto& [id, state] : nodeStates) {
                if (state.is_active && node_lookup.count(id) && node_lookup.at(id).type == NodeType::Discrepancy) {
                    if (explained_symptoms.find(id) == explained_symptoms.end()) {
                        std::string name = node_lookup.at(id).name;
                        std::cout << "    - " << id << " (" << name << ")\n";
                        std::cout << "      > Analysis: Active but not predicted by selected hypotheses.\n";
                        std::cout << "      > Potential Causes: Signal Noise, Unmodeled Fault, or Hypothesis Truncation.\n";
                        found_unexplained = true;
                    }
                }
            }
            if (!found_unexplained) {
                std::cout << "    - None\n";
            }
            std::cout << "\n";
        }
    }

    std::cout << "\nSimulation Complete." << std::endl;

    if (cout_backup) {
        std::cout.rdbuf(cout_backup);
    }
    return 0;
}