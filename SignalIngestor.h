#ifndef SIGNAL_INGESTOR_H
#define SIGNAL_INGESTOR_H

/**
 * @class SignalIngestor
 * @brief (Input Handling) Bridges a specific boolean/timestamp input format with the 
 *        mathematical requirements of rTFPGs.
 *
 * @requirement REQ-IN-01: The application shall define a DataSample structure.
 * @requirement REQ-IN-02: The class shall implement a Signal Normalization buffer.
 * @requirement REQ-IN-03: The class shall map parameterID strings to unique internal integer IDs for O(1) lookup speed.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "json.hpp" // For using nlohmann::json

/**
 * @brief REQ-IN-01: Defines the structure for a single data point from a test stream. 
 * This struct represents one event or measurement from the system.
 */
struct DataSample {
    /// The time of the event, in milliseconds.
    uint64_t timestamp_ms;
    /// A string identifier for the signal or fault.
    std::string parameterID;
    /// The numerical value of the signal.
    double value;
    /// A flag to indicate if this sample represents a fault injection rather than a sensor reading.
    bool is_failure_mode;
};

class SignalIngestor {
public:
    /**
     * @brief Constructs a SignalIngestor.
     * @param fault_model The parsed JSON fault model, used to pre-populate signal ID mappings.
     */
    explicit SignalIngestor(const nlohmann::json& fault_model);

    // REQ-IN-03: Map parameterID strings to unique internal integer IDs for O(1) lookup.
    /**
     * @brief Gets the internal integer ID for a given string parameter ID.
     * @param parameterID The string identifier of the parameter.
     * @return The internal integer ID, or -1 if not found.
     */
    int getInternalId(const std::string& parameterID) const;
    /**
     * @brief Gets the string parameter ID for a given internal integer ID.
     * @param internalId The internal integer ID.
     * @return The string identifier of the parameter.
     * @throws std::out_of_range if the internalId is invalid.
     */
    const std::string& getParameterId(int internalId) const;

    // REQ-IN-02: Ingests a sample into the normalization buffer.
    /**
     * @brief Adds a new data sample to the internal buffer.
     * @param sample The DataSample to add.
     */
    void ingest(const DataSample& sample);
    /**
     * @brief Retrieves the complete history of ingested samples.
     * @return A constant reference to the vector of samples.
     */
    const std::vector<DataSample>& getSamples() const;

private:
    /// Map from string parameter IDs to internal integer IDs for fast lookups.
    std::unordered_map<std::string, int> m_parameter_to_internal_id; 
    /// Vector to map internal integer IDs back to string parameter IDs.
    std::vector<std::string> m_internal_id_to_parameter; 
    /// The next available internal ID to be assigned.
    int m_next_internal_id = 0; 
    /// The buffer storing all ingested data samples in order of arrival.
    std::vector<DataSample> m_samples; 
};

#endif // SIGNAL_INGESTOR_H