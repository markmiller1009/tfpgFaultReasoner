--------------------------------------------------------------------------------
# TFPG Model Schema and Construction Guide

## 1. JSON Schema File (tfpg_schema.json)

This schema defines the structure for a Real-Timed Failure Propagation Graph (rTFPG). It validates the model's signals, nodes, propagation edges, and failure predicates.

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "rTFPG Fault Model Schema",
  "description": "Schema for F-22 OBOGS Timed Failure Propagation Graph models, incorporating rTFPG predicates.",
  "type": "object",
  "required": [
    "model_name",
    "version",
    "signals",
    "nodes",
    "edges"
  ],
  "properties": {
    "model_name": {
      "type": "string",
      "description": "Unique identifier for the system model [3]."
    },
    "version": {
      "type": "string",
      "description": "Versioning string (e.g., '1.1') [3]."
    },
    "description": {
      "type": "string",
      "description": "Human-readable summary of the model scope."
    },
    "signals": {
      "type": "array",
      "description": "List of sensor inputs or telemetry signals used to drive the logic engine [3].",
      "items": {
        "type": "object",
        "required": [
          "id",
          "source_name",
          "type"
        ],
        "properties": {
          "id": {
            "type": "string",
            "description": "Internal ID used in node predicates (e.g., 'S1')."
          },
          "source_name": {
            "type": "string",
            "description": "External data stream name mapping to the sensor."
          },
          "type": {
            "type": "string",
            "enum": [
              "Continuous",
              "Discrete"
            ],
            "description": "Data type of the signal [4]."
          },
          "units": {
            "type": "string"
          },
          "range_min": {
            "type": "number"
          },
          "range_max": {
            "type": "number"
          }
        }
      }
    },
    "nodes": {
      "type": "array",
      "description": "The graph vertices representing Root Causes (FailureMode) and Symptoms (Discrepancy) [5].",
      "items": {
        "type": "object",
        "required": [
          "id",
          "name",
          "type"
        ],
        "properties": {
          "id": {
            "type": "string",
            "description": "Unique Node ID (e.g., 'FM1', 'D1')."
          },
          "name": {
            "type": "string"
          },
          "description": {
            "type": "string"
          },
          "type": {
            "type": "string",
            "enum": [
              "FailureMode",
              "Discrepancy"
            ],
            "description": "FailureMode = Root Cause; Discrepancy = Symptom [5]."
          },
          "gate_type": {
            "type": "string",
            "enum": [
              "OR",
              "AND"
            ],
            "description": "Logic gate for symptom activation [2]."
          },
          "criticality_level": {
            "type": "integer",
            "minimum": 0,
            "maximum": 10,
            "description": "Severity level used for prognosis ranking [6]."
          },
          "predicate": {
            "type": "object",
            "required": [
              "signal_ref",
              "operator",
              "threshold"
            ],
            "description": "Logic to determine if this node is ACTIVE based on signal data [1].",
            "properties": {
              "signal_ref": {
                "type": "string",
                "description": "Must match a signal 'id'."
              },
              "operator": {
                "type": "string",
                "enum": [
                  "<",
                  ">",
                  "<=",
                  ">=",
                  "==",
                  "!="
                ]
              },
              "threshold": {
                "type": "number"
              }
            }
          }
        },
        "if": {
          "properties": {
            "type": {
              "const": "Discrepancy"
            }
          }
        },
        "then": {
          "required": [
            "gate_type",
            "predicate",
            "criticality_level"
          ]
        }
      }
    },
    "edges": {
      "type": "array",
      "description": "Directed causal links defining propagation paths [5].",
      "items": {
        "type": "object",
        "required": [
          "from",
          "to",
          "time_min_ms",
          "time_max_ms"
        ],
        "properties": {
          "from": {
            "type": "string",
            "description": "Source Node ID."
          },
          "to": {
            "type": "string",
            "description": "Target Node ID."
          },
          "time_min_ms": {
            "type": "integer",
            "minimum": 0,
            "description": "Minimum propagation delay in milliseconds [2]."
          },
          "time_max_ms": {
            "type": "integer",
            "minimum": 0,
            "description": "Maximum propagation delay in milliseconds [2]."
          },
          "description": {
            "type": "string"
          }
        }
      }
    }
  }
}
```

## 2. Construction Guide

To construct a valid .json model compatible with this schema, follow these structural rules derived from the TFPG and rTFPG literature.

### A. Signal Definition
* Purpose: Maps external telemetry to internal variables.
* Constraint: The id (e.g., "S1") must be unique and is referenced by Discrepancy predicates.
* Example:

### B. Node Definition
Nodes are the vertices of the graph. There are two strict types:

#### 1. FailureMode (Root Cause):
* Represents the origin of a fault.
* Rule: Must not have incoming edges (it is a source node).
* Rule: Does not have a predicate (it is inferred, not measured).

#### 2. Discrepancy (Symptom):
* Represents an off-nominal condition.
* Rule: Must have a predicate (e.g., S1 < 93.0).
* Rule: Must have a gate_type:
    * OR: Activated if any parent propagates a failure.
    * AND: Activated only if all parents propagate a failure.
* Rule: Must have a criticality_level (0-10) to support prognosis and Time-To-Criticality (TTC) calculations.

### C. Edge Definition (Propagation)
Edges represent the causal flow of failure effects over time.

* Timing:
    * time_min_ms: The physical minimum time for a failure at node A to manifest at node B.
    * time_max_ms: The maximum time before the effect must appear. If the timer exceeds this without the child activating, the link is considered "Broken" or "Contradicted".
* Topology Rules:
    * Completeness: Ensure intermediate steps are modeled. If A causes B, and B causes C, you must model A -> B and B -> C.
    * No Self-Loops: A node cannot connect to itself.

### D. Validation Checklist
Before using the model, verify:
1. Does every Discrepancy have at least one incoming edge?
2. Do all AND gates have at least two parents? (An AND gate with one parent is functionally an OR gate).
3. Are all signal_ref values defined in the signals array?