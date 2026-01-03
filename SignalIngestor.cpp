#include "SignalIngestor.h"
#include <stdexcept>

// Constructor for SignalIngestor.
SignalIngestor::SignalIngestor(const nlohmann::json& fault_model) {
    // REQ-IN-03: Populate the mapping from the fault model's "signals" array.
    // This pre-populates the ID mapping to ensure known signals are registered from the start.
    if (fault_model.contains("signals") && fault_model["signals"].is_array()) {
        for (const auto& signal : fault_model["signals"]) {
            std::string source_name = signal["source_name"];
            // Check if the signal name is already mapped to avoid duplicates.
            if (m_parameter_to_internal_id.find(source_name) == m_parameter_to_internal_id.end()) {
                m_parameter_to_internal_id[source_name] = m_next_internal_id;
                m_internal_id_to_parameter.push_back(source_name);
                m_next_internal_id++;
            }
        }
    }
}

// Gets the internal integer ID for a given string parameter ID.
int SignalIngestor::getInternalId(const std::string& parameterID) const {
    auto it = m_parameter_to_internal_id.find(parameterID);
    if (it != m_parameter_to_internal_id.end()) {
        return it->second;
    }
    return -1; // Return -1 for unknown parameters.
}

// Gets the string parameter ID for a given internal integer ID.
const std::string& SignalIngestor::getParameterId(int internalId) const {
    if (internalId < 0 || static_cast<size_t>(internalId) >= m_internal_id_to_parameter.size()) {
        throw std::out_of_range("Internal ID out of range.");
    }
    return m_internal_id_to_parameter[static_cast<size_t>(internalId)];
}

// Adds a new data sample to the internal buffer.
void SignalIngestor::ingest(const DataSample& sample) {
    // REQ-IN-02: This buffer currently stores all samples as they arrive.
    // In a more complex system, this could implement time-grid alignment or other normalization steps.
    m_samples.push_back(sample);
}

// Retrieves the complete history of ingested samples.
const std::vector<DataSample>& SignalIngestor::getSamples() const {
    return m_samples;
}