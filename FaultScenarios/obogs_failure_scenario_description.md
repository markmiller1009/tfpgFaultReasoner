# TEST_004_DOUBLE_FAILURE_ASPHYXIA Scenario Description

Here is a detailed step-by-step description of the "TEST_004_DOUBLE_FAILURE_ASPHYXIA" scenario.
This scenario is designed to stress-test the AND-gate logic and Prognosis Manager by simulating a "Double Fault" condition where a primary system failure is compounded by a latent failure in the backup system.

## Scenario Overview

*   **Scenario ID:** TEST_004_DOUBLE_FAILURE_ASPHYXIA.
*   **Objective:** Validate that the system correctly identifies a catastrophic "Total Asphyxia" risk (D6) which requires two independent root causes (FM3 and FM4) to occur.
*   **Total Duration:** 7500+ ms.

## Chronological Breakdown

### Phase 1: Nominal Flight (t = 0 ms)
The simulation begins with the aircraft in a healthy state. All sensors read nominal values well within their safety thresholds:
*   **O2 Concentration:** 94.5% (Threshold > 93.0%).
*   **Plenum Pressure:** 45.0 psi (Threshold > 10.0 psi).
*   **BOS (Backup) Pressure:** 1800.0 psi (Threshold > 200.0 psi).

### Phase 2: The Primary Fault (t = 2000 ms)
*   **Event:** Fault Injection of FM3 (Regulator_Stuck_Closed).
*   **Physics:** The breathing regulator jams shut, restricting airflow to the pilot's mask.
*   **System Status:** The system has failed, but sensors have not yet registered the drop due to physical propagation delays.

### Phase 3: Primary Symptom Manifestation (t = 2200 ms)
*   **Event:** D3 (Low_Plenum_Pressure) Activates.
*   **Sensor Data:** `sensor_plenum_pressure_psi` drops to 4.0 psi (well below the 10.0 psi threshold).
*   **Logic Engine State:**
    *   The Logic Engine recognizes FM3 as a candidate diagnosis.
    *   **Latent Risk:** The engine evaluates the edge D3 -> D6 (Total Asphyxia). However, D6 is an AND gate requiring two parents. Since the second parent (D5) is healthy, D6 remains UNREACHABLE.
    *   **Prognosis:** The system flags a "Latent Risk," meaning the aircraft is one failure away from catastrophe.

### Phase 4: Cascading Effects (t = 4000 ms)
*   **Event:** D1 (Low_O2_Concentration) Activates.
*   **Sensor Data:** `sensor_product_o2_conc_pct` drops to 90.0%.
*   **Significance:** This phase confirms the structural update you made to the model (D3 -> D1). Because the plenum pressure dropped (Phase 3), the sieve beds lost efficiency, causing O2 concentration to drop.
*   **Diagnosis Update:** FM3 is now strongly verified because it explains both D3 (Pressure) and D1 (Concentration).

### Phase 5: The Secondary Fault (t = 6000 ms)
*   **Event:** Fault Injection of FM4 (BOS_Leak).
*   **Context:** In a real-world scenario, the pilot, realizing the main regulator has failed (Phase 3), attempts to pull the emergency oxygen ring. However, the Backup Oxygen System (BOS) has a latent leak and is empty.

### Phase 6: Secondary Symptom Manifestation (t = 6500 ms)
*   **Event:** D5 (BOS_Depleted) Activates.
*   **Sensor Data:** `sensor_bos_pressure_psi` drops to 50.0 psi (Critical threshold is 200.0 psi).
*   **Logic Engine State:**
    *   FM4 is identified as a second active fault.
    *   The condition for the AND Gate (D6) is now partially met: Parent 1 (D3) is active, and Parent 2 (D5) is active.

### Phase 7: Catastrophic Logic Satisfaction (t = 7500 ms)
*   **Event:** D6 (Total_Asphyxia_Risk) Activates.
*   **Sensor Data:** Main pressure is 2.0 psi; BOS pressure is 0.0 psi.
*   **Logic Resolution:**
    *   The edge delay for D3 -> D6 is 1000–5000 ms. Since D3 activated at 2200 ms, this path is valid.
    *   The edge delay for D5 -> D6 is 1000–5000 ms. Since D5 activated at 6500 ms, the system waits for the timer.
    *   At t=7500ms, the minimum propagation time (1000ms) from D5 is satisfied.
*   **Final State:** The system declares Criticality Level 10 (Catastrophic). The pilot has no Main Oxygen and no Backup Oxygen.

## Summary of Causal Logic

This scenario validates that your reasoner correctly handles convergent failure paths:

1. Path A: FM3 → D3 (Active)
2. Path B: FM4 → D5 (Active)
3. Convergence: (D3 AND D5) → D6 (Active).

If either FM3 or FM4 had not occurred, D6 would have remained UNREACHABLE, and the pilot would have survived on the remaining functional system (either Main or Backup).