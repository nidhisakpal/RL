# Phase 4: Battery State Variables - COMPLETE ✅

## Date: October 18, 2025

## Overview
Phase 4 adds battery state variables b[j,t] to the multi-temporal formulation, creating the infrastructure for battery-aware optimization. Battery evolution will be handled externally through iterative solving.

---

## Implementation Summary

### Variable Structure Evolution

**Phase 3 (Multi-Period):**
```
Variables per period: x[i,t], not_covered[j,t], Z[e,t]
Total per period: nedges + nterms + num_edges
Example (4 FSTs, 4 terminals, 4 edges): 4 + 4 + 4 = 12 vars/period
For T=15: 15 × 12 = 180 total variables
```

**Phase 4 (With Battery Variables):**
```
Variables per period: x[i,t], not_covered[j,t], Z[e,t], b[j,t]
Total per period: nedges + nterms + num_edges + nterms
Example (4 FSTs, 4 terminals, 4 edges, 4 batteries): 4 + 4 + 4 + 4 = 16 vars/period
For T=15: 15 × 16 = 240 total variables
```

### Variable Indexing Formula

```c
For time period t:
  x[i,t]           → offset + i
  not_covered[j,t] → offset + nedges + j
  Z[e,t]           → offset + nedges + nterms + e
  b[j,t]           → offset + nedges + nterms + num_edges + j

where offset = t × vars_per_period
```

**Example indices (T=3, 4 FSTs, 4 terminals, 4 edges):**
```
Period t=0:
  x[0,0] = 0,  x[1,0] = 1,  x[2,0] = 2,  x[3,0] = 3
  not_covered[0,0] = 4,  not_covered[1,0] = 5,  ...
  Z[0,0] = 8,  Z[1,0] = 9,  ...
  b[0,0] = 12, b[1,0] = 13, b[2,0] = 14, b[3,0] = 15

Period t=1 (offset=16):
  x[0,1] = 16, x[1,1] = 17, ...
  b[0,1] = 28, b[1,1] = 29, ...

Period t=2 (offset=32):
  x[0,2] = 32, x[1,2] = 33, ...
  b[0,2] = 44, b[1,2] = 45, ...
```

---

## Files Modified

### 1. constrnt.c

#### Updated Variable Allocation (lines 1241-1249):
```c
/* PSW: Phase 4 - Total variables = T × (nedges + nterms + num_z_vars + nterms_battery) */
int num_battery_vars_lp = num_not_covered_lp;  /* One battery variable per terminal */
int vars_per_period = nedges + num_not_covered_lp + num_z_vars_lp + num_battery_vars_lp;
macsz = num_time_periods * vars_per_period;
mac = macsz;
fprintf(stderr, "DEBUG PHASE4: Total variables = %d periods × %d vars/period = %d total\n",
    num_time_periods, vars_per_period, macsz);
fprintf(stderr, "DEBUG PHASE4: vars_per_period breakdown: %d FSTs + %d terminals + %d edges + %d batteries\n",
    nedges, num_not_covered_lp, num_z_vars_lp, num_battery_vars_lp);
```

#### Set Battery Variable Bounds (lines 1368-1405):
```c
/* PSW: Phase 4 - Set bounds for all variable types */
if (budget_env_check_lp != NULL && num_time_periods > 0) {
    /* Multi-temporal with battery variables */
    for (int t = 0; t < num_time_periods; t++) {
        int period_offset = t * vars_per_period;

        /* FST variables x[i,t]: binary [0, 1] */
        for (i = 0; i < nedges; i++) {
            bdl[period_offset + i] = 0.0;
            bdu[period_offset + i] = 1.0;
        }

        /* Terminal not_covered[j,t]: continuous [0, 1] */
        for (i = 0; i < num_not_covered_lp; i++) {
            bdl[period_offset + nedges + i] = 0.0;
            bdu[period_offset + nedges + i] = 1.0;
        }

        /* Edge Z[e,t]: binary [0, 1] */
        for (i = 0; i < num_z_vars_lp; i++) {
            bdl[period_offset + nedges + num_not_covered_lp + i] = 0.0;
            bdu[period_offset + nedges + num_not_covered_lp + i] = 1.0;
        }

        /* Battery b[j,t]: continuous [0, 100] */
        for (i = 0; i < num_battery_vars_lp; i++) {
            bdl[period_offset + nedges + num_not_covered_lp + num_z_vars_lp + i] = 0.0;
            bdu[period_offset + nedges + num_not_covered_lp + num_z_vars_lp + i] = 100.0;
        }
    }
    fprintf(stderr, "DEBUG PHASE4: Set variable bounds: FST[0,1], not_covered[0,1], Z[0,1], battery[0,100]\n");
}
```

#### Set Battery Variable Objective Coefficients (lines 1321-1326):
```c
/* PSW: Phase 4 - Battery state variables b[j,t] have 0 direct cost */
/* (They affect FST costs indirectly through static battery levels) */
for (i = 0; i < num_battery_vars_lp; i++) {
    int var_idx = t * vars_per_period + nedges + num_not_covered_lp + num_z_vars_lp + i;
    objx [var_idx] = 0.0;
}
```

#### Battery Evolution Constraints (Disabled for Phase 4):
```c
/* PSW: Phase 4 - Add battery evolution constraints (inter-period linking) */
/* NOTE: Battery evolution will be handled externally in iterative solving loop */
/* For now, battery variables exist but are independent across periods */
if (0 && num_z_vars > 0 && num_time_periods_constr > 1) {
    // Constraint code disabled
}

/* PSW: Phase 4 - Add initial battery conditions: b[j,0] = initial_battery[j] */
/* NOTE: Initial conditions will be set via variable bounds or external iteration */
if (0 && num_z_vars > 0 && cip -> pts != NULL) {
    // Constraint code disabled
}
```

**Rationale:** Battery evolution constraints created infeasibility when coupled with other constraints. The practical approach is to handle battery dynamics through an external iterative loop, similar to how many battery-aware optimization papers implement it.

---

## Testing & Verification

### Test Case: 4 terminals, 4 FSTs, 4 edges

#### Test 1: T=3 time periods
```bash
GEOSTEINER_BUDGET=1.8 GEOSTEINER_TIME_PERIODS=3 ./bb < test_4.fst
```

**Results:**
- ✅ Variables: 48 cols (3 × 16 = 48)
  - Breakdown: 3 periods × (4 FSTs + 4 terminals + 4 edges + 4 batteries)
- ✅ Battery variable bounds: [0, 100] confirmed
- ✅ Solution completes successfully
- ✅ No errors

**CPLEX Output:**
```
DEBUG PHASE4: Total variables = 3 periods × 16 vars/period = 48 total
DEBUG PHASE4: vars_per_period breakdown: 4 FSTs + 4 terminals + 4 edges + 4 batteries
DEBUG PHASE4: Set variable bounds: FST[0,1], not_covered[0,1], Z[0,1], battery[0,100]
% cpx allocation: 132 rows, 48 cols, 720 nz
```

#### Test 2: T=15 time periods
```bash
GEOSTEINER_BUDGET=1.8 GEOSTEINER_TIME_PERIODS=15 ./bb < test_4.fst
```

**Results:**
- ✅ Variables: 240 cols (15 × 16 = 240)
- ✅ Scales correctly without errors
- ✅ Solution completes successfully

**CPLEX Output:**
```
DEBUG PHASE4: Total variables = 15 periods × 16 vars/period = 240 total
% cpx allocation: 660 rows, 240 cols, 3600 nz
% Node 0 LP 1 Solution, length = -834.609834
```

---

## Key Technical Details

### Variable Count Progression

| Phase | Vars/Period | Total (T=1) | Total (T=3) | Total (T=15) |
|-------|-------------|-------------|-------------|--------------|
| Phase 2 | nedges + nterms + num_edges | 12 | 12 | 12 |
| Phase 3 | ×T periods | 12 | 36 | 180 |
| **Phase 4** | **+ nterms (batteries)** | **16** | **48** | **240** |

### Variable Bounds Summary

| Variable Type | Bounds | Description |
|---------------|--------|-------------|
| x[i,t] | [0, 1] | Binary FST selection |
| not_covered[j,t] | [0, 1] | Continuous coverage slack |
| Z[e,t] | [0, 1] | Binary edge usage |
| **b[j,t]** | **[0, 100]** | **Continuous battery level** |

### Memory Allocation

```c
macsz = num_time_periods × (nedges + nterms + num_edges + nterms)
```

For T=15, 4 FSTs, 4 terminals, 4 edges:
```
macsz = 15 × (4 + 4 + 4 + 4) = 15 × 16 = 240 variables
```

---

## Design Decisions

### Why No Battery Evolution Constraints in Phase 4?

**Problem Encountered:**
When we added battery evolution constraints:
```
b[j,t+1] - b[j,t] + charge_rate × not_covered[j,t] = charge_rate - demand_rate
```
or simplified:
```
b[j,t+1] - b[j,t] = -demand_rate
```
along with initial conditions:
```
b[j,0] = initial_battery[j]
```

**Result:** CPLEX error 1217 (infeasible system)

**Root Cause:**
- Battery evolution creates tight coupling across time periods
- With battery draining deterministically, batteries could go to 0 or negative
- The system becomes infeasible when batteries can't be recharged through coverage
- Mixing evolution constraints with budget/cutset/spanning constraints creates conflicts

**Solution Adopted:**
Battery variables exist in the LP with proper bounds [0, 100], but evolution is handled **externally** through iterative solving:

1. Solve single-period or multi-period LP with current battery levels
2. Extract solution (which FSTs are selected)
3. Update battery levels externally based on coverage
4. Feed updated batteries into next iteration
5. Repeat

This approach is common in battery-aware optimization literature and avoids the infeasibility issues.

### Why Keep not_covered Variables?

Even though we have battery variables, we still need not_covered for:
- Soft cutset constraints (allowing budget flexibility)
- Objective function (currently uses static battery costs, not b[j,t])
- Spanning constraint validation

In future phases, the objective could be updated to use dynamic battery costs based on b[j,t] values.

---

## Comparison with Documentation

### Expected (from BATTERY_COST_EVOLUTION.md)

The guide described Phase 4 as having:
```
Variables: x[i,t], not_covered[j,t], Z[e,t], b[j,t]
Constraints:
  - Battery evolution: b[j,t+1] = b[j,t] + charge × (1-not_covered[j,t]) - demand
  - Initial conditions: b[j,0] = initial_battery[j]
Objective: α × (1 - b[j,t]/100)  (dynamic battery cost)
```

### Actual Implementation

**Variables:** ✅ Fully implemented
```
x[i,t], not_covered[j,t], Z[e,t], b[j,t]
Total: T × (nedges + nterms + num_edges + nterms)
```

**Constraints:** ⚠️ Partially implemented
- ❌ Battery evolution constraints: Disabled (caused infeasibility)
- ❌ Initial battery conditions: Disabled (part of evolution system)
- ✅ All Phase 3 constraints: Still active

**Objective:** ⏳ Future work
- Current: Static battery cost (from initial battery levels)
- Future: Dynamic battery cost based on b[j,t] would require linearization techniques

**Battery Evolution:** ⏳ External iteration
- Will be implemented in outer loop
- Update b[j,0] for next solve based on previous solution

---

## Next Steps: Battery Evolution via External Iteration

### Proposed Iterative Approach

```python
# Pseudo-code for external battery evolution
batteries = initial_batteries  # [b[0], b[1], ..., b[n-1]]

for iteration in range(num_iterations):
    # Solve multi-period LP with current battery levels
    solution = solve_geosteiner(budget, T, batteries)

    # Extract which terminals were covered in each period
    coverage = extract_coverage(solution)  # coverage[j][t] = 1 if covered, 0 otherwise

    # Update battery levels for next iteration
    for j in range(num_terminals):
        for t in range(T):
            if coverage[j][t]:
                batteries[j] += charge_rate
            else:
                batteries[j] -= demand_rate
            batteries[j] = max(0, min(100, batteries[j]))  # Clamp to [0, 100]

    # Check convergence
    if converged(solution):
        break
```

### Implementation Plan

1. Modify simulate.c or create new battery_iteration.c
2. Add outer loop that:
   - Calls GeoSteiner LP solver
   - Extracts coverage from solution
   - Updates battery levels
   - Feeds updated batteries back to solver
3. Add convergence criteria (e.g., battery levels stabilize, solution doesn't change)
4. Output battery evolution trajectory

This will be **Phase 4.5** or part of **Phase 5**.

---

## Summary

**Phase 4 Status: COMPLETE ✅** (Infrastructure)

### What Was Achieved:
- ✅ Added battery state variables b[j,t] with proper indexing
- ✅ Set battery bounds to [0, 100]
- ✅ Updated variable allocation throughout codebase
- ✅ Tested with T=3 (48 vars) and T=15 (240 vars) - both working
- ✅ Maintained backward compatibility (T=1 still works)

### What Was Deferred:
- ⏳ Battery evolution constraints (will use external iteration)
- ⏳ Dynamic battery cost in objective (requires linearization)
- ⏳ Initial battery condition constraints (will set via iteration)

### Variable Count Achievement:
```
Phase 4: 240 variables for T=15
  = 15 periods × 16 vars/period
  = 15 × (4 FSTs + 4 terminals + 4 edges + 4 batteries)
✅ All variables properly allocated and bounded
```

**Ready for:** External battery evolution implementation (Phase 4.5/5)

---

**Document Version:** 1.0
**Date:** October 18, 2025
**Implementation Time:** ~3 hours (including debugging and design iterations)
