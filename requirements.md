# TFPG Fault Reasoner - Software Requirements Specification

## 1. Introduction
The TFPG Fault Reasoner is a stand-alone diagnostic and prognostic simulation engine. It models physical systems using Timed Failure Propagation Graphs (TFPG) to detect anomalies, isolate root causes, and predict cascading failures based on sensor telemetry.

## 2. Functional Requirements

### 2.1 Input Processing
*   **FR-01 Model Ingestion**: The system shall accept a **Fault Model** in JSON format defining the system architecture.
    *   **FR-01.1**: The system shall validate the model against the defined JSON schema (Signals, Nodes, Edges).
    *   **FR-01.2**: The system shall support two node types: `FailureMode` (Root Cause) and `Discrepancy` (Symptom).
    *   **FR-01.3**: The system shall parse signal definitions including units, ranges, and data types (Continuous/Discrete).
*   **FR-02 Scenario Ingestion**: The system shall accept a **Fault Scenario** in JSON format defining the simulation timeline.
    *   **FR-02.1**: The system shall parse a time-ordered stream of sensor data updates.
    *   **FR-02.2**: The system shall support "Ground Truth" fault injections for validation purposes.

### 2.2 Simulation Engine
*   **FR-03 Time-Step Execution**: The system shall process the scenario data sequentially, respecting the `timestamp_ms` of each event.
*   **FR-04 State Monitoring**: The system shall maintain the current state of all defined signals and nodes throughout the simulation duration.

### 2.3 Fault Detection (Symptom Monitoring)
*   **FR-05 Predicate Evaluation**: The system shall evaluate `Discrepancy` predicates against current signal values at every time step.
    *   **FR-05.1**: The system shall support standard comparison operators (`<`, `>`, `<=`, `>=`, `==`, `!=`).
    *   **FR-05.2**: The system shall trigger a Discrepancy as "Active" when its predicate is satisfied.
*   **FR-06 Logic Gate Processing**: The system shall respect logic gates defined on Discrepancy nodes.
    *   **FR-06.1 OR Gate**: The node shall activate if *any* incoming propagation path is valid.
    *   **FR-06.2 AND Gate**: The node shall activate only if *all* required incoming propagation paths are valid.

### 2.4 Diagnostic Reasoning
*   **FR-07 Root Cause Isolation**: Upon symptom activation, the system shall traverse the graph backwards to identify candidate `FailureMode` nodes.
*   **FR-08 Temporal Validation**: The system shall validate causal links based on defined propagation delays.
    *   **FR-08.1**: A link is valid only if the time difference between the cause and effect falls within `[time_min_ms, time_max_ms]`.
    *   **FR-08.2**: The system shall reject hypotheses where observed timing contradicts the model constraints.
*   **FR-09 Multi-Fault Diagnosis**: The system shall be capable of identifying multiple independent root causes occurring simultaneously (e.g., Double Failure scenarios).
*   **FR-10 Hypothesis Ranking**: The system shall categorize diagnoses into tiers (e.g., Verified, Potential, Unexplained).

### 2.5 Prognostic Reasoning
*   **FR-11 Cascading Failure Prediction**: The system shall predict downstream nodes that are likely to activate based on currently active nodes and graph topology.
*   **FR-12 Latent Risk Identification**: The system shall identify "Latent Risks" where an AND-gate is partially satisfied (waiting for a second failure).
*   **FR-13 Time-To-Criticality (TTC)**: The system shall estimate the time remaining before a critical failure occurs, based on edge delay definitions.

### 2.6 Reporting
*   **FR-14 Diagnostic Output**: The system shall generate a readable log or report at specified time intervals or upon state changes.
    *   **FR-14.1**: The report shall list currently active Failure Modes and Discrepancies.
    *   **FR-14.2**: The report shall include prognostic warnings for impending failures.

## 3. Design Requirements (Non-Functional)

### 3.1 Architecture
*   **DR-01 Data-Driven**: The application logic must be decoupled from the system model. The engine must function for *any* valid JSON model (e.g., Hydraulic, Avionics, Electrical) without code recompilation.
*   **DR-02 Stand-Alone**: The application shall operate as a self-contained executable or script, requiring only the input JSON files to run.

### 3.2 Reliability & Accuracy
*   **DR-03 Determinism**: Given the same Model and Scenario input, the system must produce the exact same diagnostic output every time.
*   **DR-04 Physics-Based Constraints**: The reasoning engine must strictly adhere to the physics constraints (time delays) defined in the model; it cannot infer causality if the timing is physically impossible.

### 3.3 Interface
*   **DR-05 JSON Compliance**: All inputs and structured outputs must conform to standard JSON formatting.
*   **DR-06 Extensibility**: The schema shall allow for future addition of node types or signal types without breaking the core reasoning engine.

```