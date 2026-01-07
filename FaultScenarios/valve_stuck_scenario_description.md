# TEST_002_VALVE_STUCK Scenario Description

Here is a detailed, step-by-step description of the TEST_002_VALVE_STUCK scenario.
This scenario is designed to validate the system's ability to distinguish between a "Pump Failure" (which causes low pressure) and a "Blockage" (which causes high pressure), verifying the propagation chain: Valve Stuck (FM2) → High Pressure (D3) → No Flow (D4).

## Scenario Overview

*   **Scenario ID:** TEST_002_VALVE_STUCK
*   **Total Duration:** 2600+ ms
*   **Objective:** Verify that the Logic Engine correctly identifies a blockage based on a pressure spike (deadhead condition) rather than a pressure drop, adhering to specific propagation delays.

## Chronological Breakdown

### Phase 1: Nominal Operation (t = 0 ms to 1000 ms)
The simulation begins with the hydraulic system in a healthy state.
*   **Current:** 2.5 Amps (Nominal load).
*   **Pressure:** 50.0–52.0 PSI (Nominal operating pressure).
*   **Flow Rate:** 5.0 GPM (Nominal flow).

### Phase 2: The Fault Injection (t = 2000 ms)
*   **Event:** Valve_Stuck_Closed (FM2) occurs.
*   **Action:** The simulation injects the fault Valve_Stuck_Closed.
*   **Physics:** A downstream valve jams shut while the pump is still running. This immediately creates a "deadhead" condition where fluid has nowhere to go.

### Phase 3: Primary Symptom - Pressure Spike (t = 2150 ms)
*   **Event:** D3 (High Pressure) Activates.
*   **Sensor Data:** `sensor_outlet_pressure_psi` spikes from 52.0 PSI to 120.0 PSI.
*   **Logic Verification:**
    *   **Threshold:** The value exceeds the high-pressure threshold (> 100.0 PSI).
    *   **Time Elapsed:** 150 ms since the fault (2000 ms).
    *   **Constraint Check:** The model defines the propagation FM2 -> D3 with a window of 50–300 ms. The observed 150 ms delay is valid, confirming the pump is fighting the blockage.

### Phase 4: The Differentiator - Motor Load (t = 2200 ms)
*   **Event:** No Electrical Alarm (D1 remains Inactive).
*   **Sensor Data:** `sensor_motor_current_amps` rises slightly to 3.0 A.
*   **Significance:**
    *   In the "Pump Burnout" scenario, current dropped to 0.0 A.
    *   Here, the current rises (or remains non-zero) because the pump motor is working harder to push against the closed valve.
    *   **Logic Result:** The system explicitly notes that D1 (Low Current) is not triggered. This allows the Logic Engine to rule out "Pump Burnout" as a root cause.

### Phase 5: Secondary Symptom - Flow Cessation (t = 2600 ms)
*   **Event:** D4 (No Flow) Activates.
*   **Sensor Data:** `sensor_flow_rate_gpm` drops to 0.0 GPM.
*   **Physics:** Because the valve is stuck closed, fluid movement stops completely, despite the high pressure upstream.
*   **Logic Verification:**
    *   **Threshold:** Flow drops below the threshold (< 1.0 GPM).
    *   **Time Elapsed:** 450 ms after the Pressure Spike (D3 at 2150 ms).
    *   **Constraint Check:** The model defines the propagation D3 -> D4 with a window of 200–1000 ms. The observed 450 ms delay falls perfectly within this range.

## Summary of Causal Logic

This scenario confirms the diagnoser can handle "physics-based" reasoning where the order and direction of sensor changes matter:

1. t=2000ms: Fault (Valve Stuck).
2. t=2150ms: Pressure Spikes (distinguishing it from a leak or pump failure).
3. t=2200ms: Current remains High (confirming the motor is still alive).
4. t=2600ms: Flow Stops (confirming the blockage).

The Logic Engine successfully maps this to the path: FM2 → D3 → D4.