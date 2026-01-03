#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <map>

// This code assumes you have the nlohmann/json library available.
// If using a package manager like vcpkg: vcpkg install nlohmann-json
// Or include the single header file directly.

#include "json.hpp"
#include "LogicEngine.h"
#include "SignalIngestor.h"
#include "PrognosisManager.h"


using json = nlohmann::json;

int main(int argc, char* argv[]) {

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <fault_model.json> <test_data.json>" << std::endl;
        return 1;
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
    const int CRITICALITY_THRESHOLD = 5; 

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

        // D. Output Diagnosis Results
        // If any failure modes are identified as active hypotheses, output the results.
        if (!diagnoses.empty()) {
            std::cout << "\n==============================================================================\n";
            std::cout << "[Time: " << sample.timestamp_ms << "ms] DIAGNOSTIC REPORT\n";
            std::cout << "==============================================================================\n";

            // Retrieve the full state map (active/inactive nodes, robustness, etc.) for prognosis.
            const auto& nodeStates = engine.getNodeStates();

            // Iterate through each identified fault and run prognosis.
            for (const auto& diag : diagnoses) {
                std::cout << "\nHypothesis: " << diag.node.id << " (" << diag.node.name << ")\n";
                std::cout << "------------------------------------------------------------------------------\n";
                std::cout << " * Plausibility: " << (diag.plausibility * 100.0) << "% | Aggregate Robustness: " << diag.robustness << "\n";

                std::cout << " * Expected Discrepancies: " << diag.expected_symptoms.size() << " (";
                bool first = true;
                for (const auto& id : diag.expected_symptoms) {
                    if (!first) { std::cout << ", "; }
                    std::cout << id;
                    first = false;
                }
                std::cout << ")\n";

                std::cout << " * Observed Discrepancies: " << diag.consistent_symptoms.size() << "\n";
                for (const auto& id : diag.consistent_symptoms) {
                    const auto& state = nodeStates.at(id);
                    std::cout << "   - " << id << ": Activated at t=" << state.activation_time_ms << "ms";

                    if (node_lookup.count(id) && node_lookup.at(id).predicate) {
                        const auto& pred = *node_lookup.at(id).predicate;
                        std::string signal_name = pred.signal_ref;
                        if (signal_lookup.count(pred.signal_ref)) {
                            signal_name = signal_lookup.at(pred.signal_ref).source_name;
                        }
                        std::cout << " (" << signal_name << ": " << state.trigger_value << pred.op << pred.threshold << ")";
                    }
                    std::cout << ".\n";
                }

                // E. Run Prognosis (REQ-PROG-02/03)
                // Calculate the Time-To-Criticality (TTC), which is the estimated time remaining
                // until a critical failure occurs.
                const double ttc = prognosis.calculateTTC(nodeStates, CRITICALITY_THRESHOLD, sample.timestamp_ms);
                
                std::cout << " * Prognosis:\n";
                if (ttc == std::numeric_limits<double>::infinity()) {
                     std::cout << "   - System appears stable; no critical failure path detected from this state.\n";
                } else if (ttc > 0) {
                     std::cout << "   - WARNING: Time-To-Criticality (TTC) is " << ttc << " ms.\n";
                } else if (ttc == 0) {
                     std::cout << "   - CRITICAL: A critical failure condition has been reached.\n";
                } else {
                     std::cout << "   - STATUS: Critical propagation stalled. Prediction overdue by " 
                               << std::abs(ttc) << " ms (Latent Risk).\n";
                }
            }
            std::cout << "==============================================================================\n" << std::endl;
        }
    }

    std::cout << "\nSimulation Complete." << std::endl;
    return 0;
}