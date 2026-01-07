# TEST_001_PUMP_BURNOUT Scenario Description

Here is a detailed, step-by-step description of the TEST_001_PUMP_BURNOUT scenario.
This scenario is designed to simulate a sudden electrical failure of a pump motor and verify that the Logic Engine correctly tracks the physical propagation delays across current, pressure, and flow sensors.

## Scenario Overview

*   **Scenario ID:** TEST_001_PUMP_BURNOUT
*   **Total Duration:** 1800+ ms
*   **Objective:** Verify the causal chain: Pump Burnout (FM1) → Low Current (D1) → Low Pressure (D2) → No Flow (D4).

## Chronological Breakdown

### Phase 1: Nominal Operation (t = 0 ms to 900 ms)
The simulation begins with the system in a healthy, steady state. All sensor readings are within nominal operating ranges, establishing a baseline for the Logic Engine.
*   **Current:** 2.5–2.6 Amps (Nominal).
*   **Pressure:** ~50.0 PSI (Nominal).
*   **Flow Rate:** 5.0 GPM (Nominal).

### Phase 2: The Fault Injection (t = 1000 ms)
*   **Event:** Pump_Motor_Burnout (FM1) occurs.
*   **Action:** The simulation injects a "True" value for the failure mode Pump_Motor_Burnout.
*   **Context:** This represents the distinct moment the electrical motor windings burn out or open-circuit. This is the Root Cause.

### Phase 3: Immediate Electrical Symptom (t = 1010 ms)
*   **Event:** D1 (Low Current) Activates.
*   **Sensor Data:** `sensor_motor_current_amps` drops instantly from 2.6 A to 0.0 A.
*   **Logic Verification:**
    *   The value crosses the threshold (< 0.5 A).
    *   **Time Elapsed:** 10 ms since the fault.
    *   **Constraint Check:** The model expects this propagation (FM1 -> D1) to occur within 0–20 ms. The 10 ms event successfully satisfies this tight electrical timing constraint.

### Phase 4: Hydraulic Pressure Loss (t = 1250 ms)
*   **Event:** D2 (Low Pressure) Activates.
*   **Physics:** With the motor dead, the pump stops generating head pressure. However, pressure takes a moment to bleed off from the lines.
*   **Sensor Data:**
    *   At t=1100 ms, pressure drops to 30.0 PSI (dropping, but hasn't triggered the alarm yet).
    *   At t=1250 ms, pressure drops to 8.0 PSI.
*   **Logic Verification:**
    *   The value crosses the critical low-pressure threshold (< 10.0 PSI).
    *   **Time Elapsed:** 250 ms since the fault.
    *   **Constraint Check:** The model expects FM1 -> D2 to occur within 100–500 ms. The 250 ms event falls perfectly within this window, validating the hydraulic lag.

### Phase 5: Flow Cessation (t = 1800 ms)
*   **Event:** D4 (No Flow) Activates.
*   **Physics:** Following the loss of pressure, the fluid flow gradually slows down due to inertia and friction until it stops completely. This is a downstream effect of the pressure loss.
*   **Sensor Data:**
    *   At t=1500 ms, flow slows to 3.0 GPM.
    *   At t=1800 ms, flow drops to 0.0 GPM.
*   **Logic Verification:**
    *   The value crosses the no-flow threshold (< 1.0 GPM).
    *   **Time Elapsed:** 550 ms after the Pressure Warning (D2 at 1250 ms).
    *   **Constraint Check:** The model expects D2 -> D4 to occur within 500–2000 ms. The 550 ms delay validates that flow loss is a consequence of pressure loss, not an immediate effect.

## Summary of Causal Logic

The scenario successfully models a physical cascade where electrical failure is instantaneous, pressure loss is rapid but not instant, and flow loss is the lagging indicator:

1. t=1000ms: Fault Injection (FM1).
2. t=1010ms: Electrical signal lost (D1 confirmed).
3. t=1250ms: System pressure crashes (D2 confirmed).
4. t=1800ms: Fluid momentum ceases (D4 confirmed).