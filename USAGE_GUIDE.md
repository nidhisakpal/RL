# GeoSteiner with Battery-Aware Budget Constraints - Usage Guide

## Overview

This implementation extends GeoSteiner with battery-aware multi-objective optimization using CPLEX. It includes MST (Minimum Spanning Tree) correction to avoid double-counting when 2-terminal FSTs share terminals.

---

## Environment Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `GEOSTEINER_BUDGET` | Enable budget-constrained mode with specified limit | `GEOSTEINER_BUDGET=2.0` |
| `ENABLE_MST_CORRECTION` | Enable MST double-counting correction | `ENABLE_MST_CORRECTION=1` |
| `USE_CONSTRAINT_MST` | Use constraint-based MST correction (causes CPLEX crash) | `USE_CONSTRAINT_MST=1` |

---

## Quick Start

### Basic Pipeline
```bash
# Generate FST file and run optimization
./rand_points 20 | ./efst > test_20.fst
GEOSTEINER_BUDGET=2.0 ./bb test_20.fst

# With MST correction (pre-computation approach - stable)
ENABLE_MST_CORRECTION=1 GEOSTEINER_BUDGET=2.0 ./bb test_20.fst

# With MST correction (constraint-based approach - crashes during branching)
ENABLE_MST_CORRECTION=1 USE_CONSTRAINT_MST=1 GEOSTEINER_BUDGET=2.0 ./bb test_4.fst
```

### Full Iterative Optimization
```bash
./run_optimization.sh
```

---

## MST Correction Implementation

### Problem: Double-Counting in 2-Terminal FSTs

When two 2-terminal FSTs share a terminal, selecting both creates redundant coverage. The MST correction penalizes this by subtracting D_ij (the cost of the shared edge).

**Example:**
- FST_i connects terminals {A, B} with cost c_i
- FST_j connects terminals {B, C} with cost c_j
- If both selected, terminal B is covered twice
- D_ij = distance from B to nearest Steiner point (or edge midpoint)

### Approach 1: Pre-Computation (Default - Stable)

**Location:** `constrnt.c` lines 1432-1456

Adjusts FST objective costs directly before optimization:
```c
/* For each MST pair sharing a terminal */
objx[fst_i] -= D_ij / 2.0;
objx[fst_j] -= D_ij / 2.0;
```

**Effect:**
- When both FSTs selected: -D_ij/2 - D_ij/2 = -D_ij (full correction)
- When one FST selected: -D_ij/2 (partial correction, acceptable approximation)

**Advantages:**
- No additional LP variables
- No additional constraints
- No CPLEX crashes

### Approach 2: Constraint-Based (Causes CPLEX Crash)

**Location:** `constrnt.c` lines 860-926, 1507-1550

Uses auxiliary variables y_ij with linearization constraints:

**Variables:**
- y_ij in [0, 1] for each MST pair

**Constraints (3 per pair):**
```
y_ij <= x_i          (if FST i not selected, y_ij = 0)
y_ij <= x_j          (if FST j not selected, y_ij = 0)
y_ij >= x_i + x_j - 1  (if both selected, y_ij = 1)
```

**Objective modification:**
```
Add: -D_ij * y_ij
```

**Issue:** CPLEX crashes with `free(): invalid pointer` during branching when LP solution is fractional.

---

## How to Switch Between Approaches

### Pre-Computation (Default)
```bash
ENABLE_MST_CORRECTION=1 GEOSTEINER_BUDGET=2.0 ./bb < test.fst
```

### Constraint-Based (For Demonstrating Crash)
```bash
ENABLE_MST_CORRECTION=1 USE_CONSTRAINT_MST=1 GEOSTEINER_BUDGET=2.0 ./bb < test_4.fst
```

---

## Code Locations

### MST Correction Toggle
**File:** `constrnt.c` lines 636-649
```c
if (getenv("USE_CONSTRAINT_MST") != NULL) {
    /* Constraint-based approach - allocate y_ij variables */
    num_y_vars = mst_info -> num_pairs;
    fprintf(stderr, "DEBUG MST_CORRECTION: Using CONSTRAINT-BASED approach\n");
    fprintf(stderr, "WARNING: May cause CPLEX crashes during branching!\n");
} else {
    /* Pre-computation approach - no y_ij variables */
    num_y_vars = 0;
}
```

### MST Pair Identification
**File:** `constrnt.c` lines 170-287

Finds all pairs of 2-terminal FSTs sharing a terminal:
```c
/* For each terminal, find all 2-terminal FSTs containing it */
for (terminal = 0; terminal < num_verts; terminal++) {
    /* Find all 2-terminal FSTs sharing this terminal */
    /* Create pairs from all combinations */
    pairs[pair_count++] = {fst_i, fst_j, shared_terminal, D_ij};
}
```

### Pre-Computation Cost Adjustment
**File:** `constrnt.c` lines 1432-1456
```c
/* Pre-compute MST corrections by adjusting FST objective costs */
for (p = 0; p < mst_info->num_pairs; p++) {
    int fst_i = mst_info->pairs[p].fst_i;
    int fst_j = mst_info->pairs[p].fst_j;
    double D_ij = mst_info->pairs[p].penalty;

    objx[fst_i] -= D_ij / 2.0;
    objx[fst_j] -= D_ij / 2.0;
}
```

### Constraint-Based y_ij Variables
**File:** `constrnt.c` lines 1507-1550
```c
/* Add y_ij variables and constraints */
for (p = 0; p < num_y_vars_lp; p++) {
    /* y_ij <= x_i */
    /* y_ij <= x_j */
    /* y_ij >= x_i + x_j - 1 */
    /* Objective: -D_ij * y_ij */
}
```

---

## Mathematical Formulation

### Objective Function
```
Minimize: Σ (tree_cost[i] + α × battery_cost[i]) × x[i]
        + β × Σ not_covered[j]
        - Σ D_ij × y_ij  (constraint-based only)
```

Where:
- α = weight for battery cost (negative = reward for low battery)
- β = penalty for uncovered terminals
- D_ij = MST correction penalty for shared terminal

### Battery Cost Formula
```
battery_cost[i] = α × (-1 + avg_battery / 100)
```
- Range: [-α, 0] where α = 10.0 default
- Low battery (0%) → -α (strong incentive to cover)
- High battery (100%) → 0 (no incentive)

### D_ij Calculation
```
D_ij = 10.0 × (-1 + battery[shared_terminal] / 100)
```
Same formula as battery cost, applied to the shared terminal.

---

## Demonstrating the CPLEX Crash

### Test Files That Trigger Crash
- `test_4.fst` - 4 terminals, fractional LP solution triggers branching
- `test_8.fst` - 8 terminals, also causes crash

### Test Files That Work
- `test_6.fst` - 6 terminals, integer LP solution (no branching needed)

### Commands
```bash
# CRASHES - constraint-based with branching
ENABLE_MST_CORRECTION=1 USE_CONSTRAINT_MST=1 GEOSTEINER_BUDGET=2.0 ./bb < test_4.fst

# WORKS - pre-computation approach
ENABLE_MST_CORRECTION=1 GEOSTEINER_BUDGET=2.0 ./bb < test_4.fst

# WORKS - constraint-based without branching (integer LP)
ENABLE_MST_CORRECTION=1 USE_CONSTRAINT_MST=1 GEOSTEINER_BUDGET=2.0 ./bb < test_6.fst
```

### Expected Crash Output
```
DEBUG MST_CORRECTION: Using CONSTRAINT-BASED approach
WARNING: May cause CPLEX crashes during branching!
...
free(): invalid pointer
Aborted (core dumped)
```

---

## Compilation

```bash
make clean
make
```

Requires CPLEX installation with proper library paths configured.

---

## File Summary

| File | Purpose |
|------|---------|
| `constrnt.c` | Main constraint generation with MST correction |
| `battery_wrapper.c` | Updates terminal battery levels based on coverage |
| `run_optimization.sh` | Iterative optimization script |
| `bb.c` | Branch and bound solver |
| `bbsubs.c` | Branch and bound subroutines |

---

## Troubleshooting

### CPLEX Crash During Branching
- **Cause:** Constraint-based MST correction with fractional LP solutions
- **Solution:** Use pre-computation approach (default) or increase budget to avoid branching

### Low Budget Utilization
- **Cause:** Tree cost scaling was dominating battery reward
- **Fix:** Removed `* nedges` scaling factor from tree cost

### Terminals Losing Battery When Connected
- **Cause:** FST selection parsing failed to match `x[i] = 1.000000` format
- **Fix:** Parse double value and check `>= 0.5` threshold
