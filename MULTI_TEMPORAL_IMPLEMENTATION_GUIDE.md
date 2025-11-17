# Multi-Temporal Battery-Aware Network Optimization
## Complete Implementation Guide - All Phases

**Date:** October 18, 2025
**Project:** Battery-Aware GeoSteiner with Multi-Temporal Optimization
**Author:** Implementation completed in phases

---

## Table of Contents
1. [Overview](#overview)
2. [Mathematical Formulation](#mathematical-formulation)
3. [Phase 1: Edge Enumeration Infrastructure](#phase-1-edge-enumeration-infrastructure)
4. [Phase 2: Single-Period Z Variables](#phase-2-single-period-z-variables)
5. [Phase 3: Multi-Period Variables](#phase-3-multi-period-variables)
6. [Phase 4: Battery Dynamics (Planned)](#phase-4-battery-dynamics-planned)
7. [Phase 5: Graph Distance Integration (Planned)](#phase-5-graph-distance-integration-planned)
8. [Battery Cost Evolution](#battery-cost-evolution)
9. [Usage Guide](#usage-guide)
10. [Testing Examples](#testing-examples)

---

## Overview

This project extends the GeoSteiner library to solve battery-aware network optimization problems across multiple time periods. The goal is to optimize network topology over time while considering:

1. **Tree cost**: Physical cost of the Steiner tree
2. **Battery levels**: Nodes with low battery get higher priority
3. **Topology stability**: Minimize changes between time periods (graph distance)
4. **Budget constraints**: Total tree cost must stay within budget

### Problem Context

In a wireless sensor network:
- **Terminals (nodes)** have battery levels that change over time
- Being **covered by the selected FST** recharges the battery
- Being **uncovered** depletes the battery
- We want to **maximize network lifetime** by balancing coverage across time

---

## Mathematical Formulation

### Variables (Multi-Temporal)

For each time period **t = 0, 1, ..., T-1**:

1. **x[i,t]**: Binary variable, 1 if Full Steiner Tree (FST) i is selected at time t
2. **not_covered[j,t]**: Continuous variable [0,1], amount terminal j is uncovered at time t
3. **Z[e,t]**: Binary variable, 1 if edge e is used at time t (Phase 2+)
4. **b[j,t]**: Continuous variable [0,100], battery level of terminal j at time t (Phase 4+)

### Objective Function (Evolution Across Phases)

**Phase 1-2 (Single Period):**
```
Minimize: Σᵢ [tree_cost[i] + α × battery_cost[i]] × x[i] + β × Σⱼ not_covered[j]
```

**Phase 3 (Multi-Period, Independent):**
```
Minimize: Σₜ Σᵢ [tree_cost[i] + α × battery_cost[i]] × x[i,t] + β × Σₜ Σⱼ not_covered[j,t]
```

**Phase 4 (With Battery Dynamics):**
```
Minimize: Σₜ Σᵢ [tree_cost[i] + α × (1 - b[j(i),t]/100)] × x[i,t]
          + β × Σₜ Σⱼ not_covered[j,t]
```
where **j(i)** is a representative terminal covered by FST i (e.g., terminal with lowest battery)

**Phase 5 (With Graph Distance):**
```
Minimize: Σₜ Σᵢ [tree_cost[i] + α × (1 - b[j(i),t]/100)] × x[i,t]
          + β × Σₜ Σⱼ not_covered[j,t]
          + γ × Σₜ GD(t, t+1)
```
where **GD(t, t+1)** = graph distance between topologies at time t and t+1

### Constraints (Multi-Temporal)

For each time period **t**:

1. **Spanning constraint**:
   ```
   Σᵢ (|FST[i]| - 1) × x[i,t] + Σⱼ not_covered[j,t] = n - 1
   ```

2. **Soft cutset constraints** (for each terminal j):
   ```
   Type 1: x[i,t] + not_covered[j,t] ≤ 1  for each FST i covering j
   Type 2: Σᵢ x[i,t] + n × not_covered[j,t] ≤ n  (sum over FSTs covering j)
   ```

3. **Budget constraint**:
   ```
   Σᵢ tree_cost[i] × x[i,t] ≤ budget_limit
   ```

4. **Z[e,t] linking** (Phase 2+):
   ```
   Z[e,t] ≤ Σᵢ x[i,t]  for all FSTs i containing edge e
   ```

5. **Battery dynamics** (Phase 4+):
   ```
   b[j,t+1] = b[j,t] + charge_rate × (1 - not_covered[j,t]) - demand_rate
   0 ≤ b[j,t] ≤ 100
   b[j,0] = initial_battery[j]
   ```

6. **Graph distance** (Phase 5+):
   ```
   GD(t, t+1) = Σₑ |Z[e,t] - Z[e,t+1]|
   ```

---

## Phase 1: Edge Enumeration Infrastructure

### Status: ✅ COMPLETE

### Purpose
Build a global edge map to enable efficient edge-based operations needed for graph distance calculations.

### Implementation

#### Data Structure: Edge Map
```c
struct edge_map {
    int num_edges;              // Total unique edges
    struct edge_info *edges;    // Array of edge information
    struct hash_table *hash;    // Hash table for O(1) lookup
};

struct edge_info {
    int p1, p2;                 // Endpoints
    double length;              // Edge length
    int num_fsts;              // Number of FSTs containing this edge
    int *fst_indices;          // List of FST indices
};
```

#### Key Functions
- `create_edge_map()`: Build edge map from FST data
- `get_edge_fsts()`: Get list of FSTs containing an edge
- `hash_edge()`: Hash function for edge lookup

#### Files Modified
- **edge_map.c**: Complete edge map implementation (new file)
- **edge_map.h**: Header file (new file)
- **cra.c**: Added edge map creation call
- **Makefile**: Added edge_map.o to build

#### Testing
```bash
./rand_points 4 | ./efst | GEOSTEINER_BUDGET=2.0 ./bb
```

**Results:**
- Edge map created successfully
- Hash table with 1021 buckets
- O(1) edge lookup verified
- FST-to-edge mapping working

### Variable Count
No change from baseline (single period):
- **Variables**: x[i], not_covered[j]
- **Count**: nedges + nterms

---

## Phase 2: Single-Period Z Variables

### Status: ✅ COMPLETE

### Purpose
Add binary edge coverage variables Z[e] to enable graph distance calculations in future phases.

### Implementation

#### New Variables
- **Z[e]**: Binary variable, 1 if edge e is used in the selected FSTs
- **Count**: num_edges (typically equals nedges, but represents unique edges)

#### New Constraints
For each edge e:
```
Z[e] - Σᵢ x[i] ≤ 0    for all FSTs i containing edge e

Equivalent to: Z[e] ≤ Σᵢ x[i]
```

This ensures Z[e] = 1 only if at least one FST containing edge e is selected.

#### Files Modified
- **constrnt.c**:
  - Updated `macsz` calculation to include Z variables
  - Added Z[e] linking constraint generation (lines 863-907)
  - Set Z[e] objective coefficients to 0 (no cost in Phase 2)

- **bb.c**:
  - Updated variable allocation to include Z variables
  - Added debug output for Z[e] values

- **bbsubs.c**, **ckpt.c**: Updated for Z variable handling

#### Variable Indexing
```
x[i]           → indices 0 to nedges-1
not_covered[j] → indices nedges to nedges+nterms-1
Z[e]           → indices nedges+nterms to nedges+nterms+num_edges-1
```

#### Testing
```bash
GEOSTEINER_BUDGET=2.0 ./bb < test_4.fst
```

**Results:**
- 12 cols: 4 FSTs + 4 terminals + 4 Z variables ✓
- Z[e] variables all 0.0 (expected - no cost, so solver sets them to 0)
- Linking constraints added correctly

### Variable Count (Single Period)
- **Variables**: x[i], not_covered[j], Z[e]
- **Count**: nedges + nterms + num_edges
- **Example**: 4 + 4 + 4 = **12 variables**

---

## Phase 3: Multi-Period Variables

### Status: ✅ COMPLETE

### Purpose
Extend single-period formulation to T time periods, creating infrastructure for battery dynamics.

### Implementation

#### Variable Structure
Transform all single-period variables into multi-period:
```
x[i]           → x[i,t]           for t = 0, 1, ..., T-1
not_covered[j] → not_covered[j,t]
Z[e]           → Z[e,t]
```

#### Variable Indexing Formula
Linear indexing across time:
```c
var_idx = t × vars_per_period + offset

where:
  vars_per_period = nedges + nterms + num_edges
  offset = position within period
```

**Examples:**
```
x[2,5]           = 5 × 12 + 2 = 62
not_covered[3,7] = 7 × 12 + 4 + 3 = 91
Z[1,10]          = 10 × 12 + 8 + 1 = 129
```

#### Constraint Replication
Added main time loop in `_gst_initialize_constraint_pool()`:

```c
for (int time_period = 0; time_period < num_time_periods; time_period++) {
    int period_offset = time_period * vars_per_period;

    // Generate all constraints for this period:
    // - Spanning constraint
    // - Hard cutset constraints
    // - Soft cutset constraints (type 1 & 2)
    // - Source terminal constraint
    // - Incompatibility constraints
    // - 2-terminal SEC constraints
    // - Budget constraint
    // - "At least one FST" constraint
    // - Z[e,t] linking constraints
}
```

All variable indices updated to include `period_offset`.

#### Files Modified
- **constrnt.c**:
  - Added time period configuration (lines 221-230)
  - Added main time loop (lines 510-533)
  - Updated ALL constraint generation to use `period_offset`
  - Updated objective function for all T periods (lines 1228-1278)
  - Closed time loop (lines 909-911)

- **run_optimization.sh**: Updated status to Phase 3 COMPLETE

#### Environment Variables
- **GEOSTEINER_TIME_PERIODS**: Set number of time periods (default: 1, max: 100)

#### Testing

**Test 1: T=3 periods**
```bash
GEOSTEINER_BUDGET=1.8 GEOSTEINER_TIME_PERIODS=3 ./bb < test_4.fst
```
- **Variables**: 36 (3 × 12)
- **Constraints**: Generated for t=0, t=1, t=2
- **CPLEX**: 132 rows, 36 cols ✓

**Test 2: T=15 periods**
```bash
GEOSTEINER_BUDGET=1.8 GEOSTEINER_TIME_PERIODS=15 ./bb < test_4.fst
```
- **Variables**: 180 (15 × 12)
- **Constraints**: Generated for t=0 to t=14
- **CPLEX**: 660 rows, 180 cols ✓

### Variable Count (Multi-Period)
- **Variables**: x[i,t], not_covered[j,t], Z[e,t] for all t
- **Count**: T × (nedges + nterms + num_edges)
- **Example (T=15)**: 15 × 12 = **180 variables**

---

## Phase 4: Battery Dynamics (PLANNED)

### Status: ⏳ PLANNED

### Purpose
Add battery state variables and inter-period linking to model battery evolution over time.

### Planned Variables

#### New: Battery State Variables
```
b[j,t]: Continuous variable [0, 100], battery level of terminal j at time t
```

### Planned Constraints

#### 1. Battery Evolution
For each terminal j, each time period t:
```
b[j,t+1] = b[j,t] + charge_rate × (1 - not_covered[j,t]) - demand_rate

where:
  charge_rate = amount of charge gained when covered
  demand_rate = baseline energy consumption
  coverage[j,t] = 1 - not_covered[j,t]
```

**Linearized form:**
```
b[j,t+1] - b[j,t] + charge_rate × not_covered[j,t] = charge_rate - demand_rate
```

#### 2. Battery Bounds
```
0 ≤ b[j,t] ≤ 100  for all j, t
```

#### 3. Initial Conditions
```
b[j,0] = initial_battery[j]  (from input file)
```

### Planned Objective Function Update

**Current (Phase 3):**
```
Minimize: Σₜ Σᵢ [tree_cost[i] + α × battery_cost[i]] × x[i,t] + β × Σₜ Σⱼ not_covered[j,t]
```

**Phase 4 Target:**
```
Minimize: Σₜ Σᵢ [tree_cost[i] + α × (1 - b[j(i),t]/100)] × x[i,t] + β × Σₜ Σⱼ not_covered[j,t]

where j(i) = representative terminal for FST i (e.g., min battery terminal)
```

**Key change:** Battery cost is now **dynamic**, based on current battery level at time t.

### Planned Variable Count
- **Variables**: x[i,t], not_covered[j,t], Z[e,t], **b[j,t]**
- **Count**: T × (nedges + nterms + num_edges + nterms)
- **Example (T=15, 4 terminals)**: 15 × (4 + 4 + 4 + 4) = **240 variables**

### Implementation Steps

1. Add battery state variables to LP formulation
2. Update variable allocation in all files
3. Add battery evolution constraints
4. Update objective function to use b[j,t]
5. Read initial_battery from input file
6. Test with simple scenarios

---

## Phase 5: Graph Distance Integration (PLANNED)

### Status: ⏳ PLANNED

### Purpose
Add graph distance objective term to minimize topology changes between consecutive time periods.

### Mathematical Formulation

#### Graph Distance Definition
The graph distance between topologies at time t and t+1:
```
GD(t, t+1) = Σₑ |Z[e,t] - Z[e,t+1]|
```

This counts the number of edges that change state (added or removed) between periods.

#### Linearization
Since Z variables are binary, we can linearize using auxiliary variables:

```
Introduce: D[e,t] ≥ 0  (continuous, represents |Z[e,t] - Z[e,t+1]|)

Constraints:
  D[e,t] ≥ Z[e,t] - Z[e,t+1]
  D[e,t] ≥ Z[e,t+1] - Z[e,t]

Then: GD(t, t+1) = Σₑ D[e,t]
```

### Updated Objective Function

```
Minimize: Σₜ Σᵢ [tree_cost[i] + α × (1 - b[j(i),t]/100)] × x[i,t]
          + β × Σₜ Σⱼ not_covered[j,t]
          + γ × Σₜ Σₑ D[e,t]

where:
  α = battery cost weight (0.1)
  β = penalty for uncovered terminals (1000000)
  γ = graph distance weight (TBD, ~1.0-10.0)
```

### Planned Variable Count
- **Variables**: x[i,t], not_covered[j,t], Z[e,t], b[j,t], **D[e,t]**
- **Count**: T × (nedges + nterms + num_edges + nterms) + (T-1) × num_edges
- **Example (T=15)**: 15×16 + 14×4 = **296 variables**

### Implementation Steps

1. Add D[e,t] auxiliary variables for graph distance
2. Add linearization constraints
3. Update objective function with graph distance term
4. Tune γ parameter for desired topology stability
5. Test and compare with/without graph distance

---

## Battery Cost Evolution

### Overview

The battery cost component in the objective function evolves across phases to become increasingly sophisticated and responsive to dynamic battery levels.

---

### Phase 1-2: Static Battery Cost

**Formula:**
```
battery_cost[i] = Σⱼ battery[j]  (for all terminals j covered by FST i)
```

**Objective contribution:**
```
α × battery_cost[i] × x[i]
```

**Characteristics:**
- Battery levels are **static** (read from input, never change)
- Same battery cost throughout entire optimization
- α = 0.1 (weight parameter)
- Higher battery cost → lower priority FST

**Example:**
```
Terminal batteries: [28.6, 6.0, 24.3, 69.7]
FST 0 covers terminals [0,1,2,3]: battery_cost[0] = 28.6 + 6.0 + 24.3 + 69.7 = 128.6
```

**In objective:**
```
0.1 × 128.6 × x[0] = 12.86 × x[0]
```

**Interpretation:** FSTs covering high-battery terminals are penalized (de-prioritized).

---

### Phase 3: Multi-Period Static Battery Cost

**Formula:** (Same as Phase 1-2, but replicated for each period)
```
battery_cost[i] = Σⱼ battery[j]  (static, same for all t)
```

**Objective contribution:**
```
Σₜ α × battery_cost[i] × x[i,t]
```

**Characteristics:**
- Still **static** battery levels
- Each period uses same battery costs
- Periods are **independent** (no temporal coupling)
- Not realistic - batteries don't change despite coverage changes

**Limitation:**
- Battery levels should evolve based on coverage history
- No incentive to balance coverage across time

---

### Phase 4: Dynamic Battery Cost (PLANNED)

**Formula:**
```
battery_cost[i,t] = α × (1 - b[j(i),t]/100)

where:
  b[j(i),t] = battery level of representative terminal j for FST i at time t
  j(i) = terminal with minimum battery among those covered by FST i
```

**Objective contribution:**
```
Σₜ Σᵢ [tree_cost[i] + α × (1 - b[j(i),t]/100)] × x[i,t]
```

**Characteristics:**
- Battery cost is now **dynamic** - changes with b[j,t]
- **Low battery (b[j,t] → 0)**: battery_cost → α × 1 = **high cost** → **high priority**
- **High battery (b[j,t] → 100)**: battery_cost → α × 0 = **low cost** → **low priority**
- Coupled with battery evolution constraints

**Battery Evolution:**
```
b[j,t+1] = b[j,t] + charge_rate × (1 - not_covered[j,t]) - demand_rate
```

**Coupling:**
- If terminal j is **covered** at time t: not_covered[j,t] ≈ 0 → b[j,t+1] increases
- If terminal j is **uncovered** at time t: not_covered[j,t] = 1 → b[j,t+1] decreases
- Next period, battery cost reflects new battery level

**Example Timeline:**

| Time | Terminal j | b[j,t] | covered? | b[j,t+1] | battery_cost at t+1 |
|------|-----------|--------|----------|----------|---------------------|
| t=0  | j=1       | 80.0   | Yes      | 90.0     | 0.1 × (1 - 90/100) = 0.01 |
| t=1  | j=1       | 90.0   | No       | 80.0     | 0.1 × (1 - 80/100) = 0.02 |
| t=2  | j=1       | 80.0   | No       | 70.0     | 0.1 × (1 - 70/100) = 0.03 |
| t=3  | j=1       | 70.0   | Yes      | 80.0     | 0.1 × (1 - 80/100) = 0.02 |

**Key Insight:**
As battery drains, the terminal becomes higher priority for coverage in future periods.

---

### Battery Cost Comparison Table

| Aspect | Phase 1-2 | Phase 3 | Phase 4 (Planned) |
|--------|-----------|---------|-------------------|
| **Battery levels** | Static | Static | Dynamic |
| **Formula** | Σⱼ battery[j] | Σⱼ battery[j] | α × (1 - b[j,t]/100) |
| **Time dependency** | No | No | Yes |
| **Inter-period coupling** | N/A | No | Yes |
| **Realistic** | No | No | Yes |
| **Balances coverage** | No | No | Yes |
| **Max cost (b=0)** | High battery sum | High battery sum | α × 1.0 = 0.1 |
| **Min cost (b=100)** | Low battery sum | Low battery sum | α × 0.0 = 0.0 |
| **Priority** | High battery = low priority | Same | **Low battery = high priority** |

---

### Why (1 - b[j,t]/100)?

The term `(1 - b[j,t]/100)` is designed to be **inversely proportional** to battery level:

```
b[j,t] = 0   → (1 - 0/100)   = 1.0  → Maximum penalty (HIGH priority for coverage)
b[j,t] = 50  → (1 - 50/100)  = 0.5  → Medium penalty
b[j,t] = 100 → (1 - 100/100) = 0.0  → Zero penalty (no need for coverage)
```

**Objective interpretation:**
- Selecting an FST that covers a **low-battery terminal** adds **high cost** to objective
- But this is exactly what we want! It **forces the solver** to cover low-battery terminals
- The high cost ensures the terminal gets coverage (to minimize total objective)

**Combined with battery evolution:**
- Covering low-battery terminals → batteries recharge → cost decreases next period
- Ignoring terminals → batteries drain → cost increases next period → forced to cover
- Result: **Balanced coverage** across all terminals over time

---

### Parameter Values

| Parameter | Value | Meaning |
|-----------|-------|---------|
| **α** | 0.1 | Battery cost weight |
| **β** | 1,000,000 | Penalty for uncovered terminals (soft constraint) |
| **γ** | TBD (~1-10) | Graph distance weight (Phase 5) |
| **charge_rate** | TBD (~10-20) | Energy gained per period when covered |
| **demand_rate** | TBD (~5-10) | Energy consumed per period |

---

## Usage Guide

### Environment Variables

| Variable | Purpose | Default | Range |
|----------|---------|---------|-------|
| `GEOSTEINER_BUDGET` | Budget constraint (normalized) | None (required) | 0.0 - 10.0 |
| `GEOSTEINER_TIME_PERIODS` | Number of time periods T | 1 | 1 - 100 |

### Compilation

```bash
make clean
make bb
```

### Basic Pipeline

```bash
# Generate random terminals with batteries
./rand_points 20 > terminals.txt

# Generate Full Steiner Trees
./efst < terminals.txt > trees.fst

# Run optimization (single period)
GEOSTEINER_BUDGET=2.0 ./bb < trees.fst

# Run optimization (multi-period, T=15)
GEOSTEINER_BUDGET=2.0 GEOSTEINER_TIME_PERIODS=15 ./bb < trees.fst
```

### Input File Format

The `rand_points` program generates terminals with battery levels:

```
4            # Number of terminals
0 0.4588 0.2373 28.60    # Terminal 0: x, y, battery
1 0.1271 0.3510 6.00     # Terminal 1
2 0.1545 0.4808 24.30    # Terminal 2
3 0.9474 0.1441 69.70    # Terminal 3
```

### Output Interpretation

**CPLEX allocation:**
```
% cpx allocation: 660 rows, 180 cols, 3600 nz
```
- **660 rows**: Number of constraints
- **180 cols**: Number of variables (T × vars_per_period)
- **3600 nz**: Number of non-zeros in constraint matrix

**Variable count check:**
```
DEBUG PHASE3: Total variables = 15 periods × 12 vars/period = 180 total
```

---

## Testing Examples

### Example 1: Single Period (Phase 2)

```bash
# Generate 4 terminals
./rand_points 4 > test_4.txt

# Create FSTs
./efst < test_4.txt > test_4.fst

# Run single-period optimization
GEOSTEINER_BUDGET=2.0 ./bb < test_4.fst
```

**Expected output:**
```
DEBUG PHASE3: Total variables = 1 periods × 12 vars/period = 12 total
% cpx allocation: 22 rows, 12 cols, 120 nz
```

**Variable breakdown:**
- 4 FST variables: x[0], x[1], x[2], x[3]
- 4 terminal variables: not_covered[0..3]
- 4 edge variables: Z[0..3]

---

### Example 2: Multi-Period T=3 (Phase 3)

```bash
GEOSTEINER_BUDGET=1.8 GEOSTEINER_TIME_PERIODS=3 ./bb < test_4.fst
```

**Expected output:**
```
DEBUG PHASE3 CONSTR: Multi-temporal constraints for T=3 periods
DEBUG PHASE3 CONSTR: Generating constraints for period t=0 (offset=0)
DEBUG PHASE3 CONSTR: Generating constraints for period t=1 (offset=12)
DEBUG PHASE3 CONSTR: Generating constraints for period t=2 (offset=24)
DEBUG PHASE3 CONSTR: Completed constraint generation for all 3 time periods
DEBUG PHASE3: Total variables = 3 periods × 12 vars/period = 36 total
% cpx allocation: 132 rows, 36 cols, 720 nz
```

**Variable breakdown (T=3):**
- 12 FST variables: x[0..3, 0..2]
- 12 terminal variables: not_covered[0..3, 0..2]
- 12 edge variables: Z[0..3, 0..2]

---

### Example 3: Full Scale T=15 (Phase 3)

```bash
GEOSTEINER_BUDGET=1.8 GEOSTEINER_TIME_PERIODS=15 ./bb < test_4.fst
```

**Expected output:**
```
DEBUG PHASE3 CONSTR: Completed constraint generation for all 15 time periods
DEBUG PHASE3: Total variables = 15 periods × 12 vars/period = 180 total
% cpx allocation: 660 rows, 180 cols, 3600 nz
```

**Performance:**
- Compilation: ~30 seconds
- Phase 1 (FST generation): < 1 second
- Constraint generation: ~1 second
- LP solving: Varies (seconds to minutes)

---

### Example 4: With Battery Dynamics (Phase 4 - Future)

```bash
# This will work after Phase 4 implementation
GEOSTEINER_BUDGET=2.0 GEOSTEINER_TIME_PERIODS=15 \
CHARGE_RATE=15 DEMAND_RATE=5 \
./bb < test_20.fst
```

**Expected changes:**
- 240 variables (adds b[j,t] for each terminal)
- Battery evolution constraints active
- Objective uses dynamic battery costs
- Coverage balances across periods

---

## Summary

### Current Implementation Status

| Phase | Status | Variables | Key Features |
|-------|--------|-----------|--------------|
| **Phase 1** | ✅ COMPLETE | No change | Edge enumeration, hash map |
| **Phase 2** | ✅ COMPLETE | +num_edges | Z[e] variables, linking constraints |
| **Phase 3** | ✅ COMPLETE | ×T periods | Multi-temporal framework |
| **Phase 4** | ⏳ PLANNED | +T×nterms | Battery dynamics, inter-period coupling |
| **Phase 5** | ⏳ PLANNED | +(T-1)×num_edges | Graph distance, topology stability |

### Variable Count Progression

| Phase | Variables per Period | Total (T=1) | Total (T=15) |
|-------|---------------------|-------------|--------------|
| Baseline | nedges + nterms | 8 | 8 |
| Phase 2 | +num_edges | 12 | 12 |
| Phase 3 | ×T | 12 | 180 |
| Phase 4 (planned) | +nterms | 16 | 240 |
| Phase 5 (planned) | +num_edges (T-1) | 16 | 296 |

### Battery Cost Progression

| Phase | Formula | Dynamic? | Realistic? |
|-------|---------|----------|------------|
| 1-2 | Σⱼ battery[j] | No | No |
| 3 | Same | No | No |
| 4 | α × (1 - b[j,t]/100) | **Yes** | **Yes** |
| 5 | Same + graph distance | **Yes** | **Yes** |

---

## Next Steps

### Immediate: Phase 4 Implementation

1. Add battery state variables b[j,t]
2. Implement battery evolution constraints
3. Update objective function to use dynamic battery costs
4. Test with simple scenarios
5. Validate battery evolution behavior

### Future: Phase 5 Implementation

1. Add D[e,t] auxiliary variables
2. Implement graph distance linearization
3. Add graph distance to objective
4. Tune γ parameter
5. Analyze topology stability vs coverage trade-off

---

## References

- **GeoSteiner Library**: Original implementation by Warme, Winter, Zachariasen
- **CPLEX Solver**: IBM ILOG CPLEX Optimization Studio
- **Project Repository**: battery-aware-network--main

---

**Document Version:** 1.0
**Last Updated:** October 18, 2025
**Implementation Complete Through:** Phase 3
