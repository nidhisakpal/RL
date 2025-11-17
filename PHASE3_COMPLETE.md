# Phase 3: Multi-Period Variables - COMPLETE ✅

## Date: October 18, 2025

## Overview
Phase 3 successfully implements multi-temporal optimization with T time periods, extending the single-period formulation to handle multiple time steps simultaneously in a single MIP model.

---

## Implementation Summary

### Variable Structure (Before → After)

**Phase 2 (Single Period):**
```
Variables: x[i], not_covered[j], Z[e]
Total: nedges + nterms + num_edges
Example (4 FSTs, 4 terminals, 4 edges): 4 + 4 + 4 = 12 variables
```

**Phase 3 (Multi-Period):**
```
Variables: x[i,t], not_covered[j,t], Z[e,t]  for t = 0, 1, ..., T-1
Total: T × (nedges + nterms + num_edges)
Example with T=15: 15 × 12 = 180 variables
```

### Variable Indexing Formula

Linear indexing across time periods:
```c
var_idx = t × vars_per_period + offset

where:
  t = time period (0 to T-1)
  vars_per_period = nedges + nterms + num_edges
  offset = position within period (0 for FST 0, nedges for terminal 0, etc.)

Examples:
  x[2,5] = 5 × 12 + 2 = 62
  not_covered[3,7] = 7 × 12 + 4 + 3 = 91
  Z[1,10] = 10 × 12 + 8 + 1 = 129
```

---

## Files Modified

### 1. constrnt.c (Main constraint generation)

#### Added Time Period Configuration (lines 221-230):
```c
/* PSW: Phase 3 - Get number of time periods */
int num_time_periods = 1;  /* Default: single period */
char* num_periods_env = getenv("GEOSTEINER_TIME_PERIODS");
if (num_periods_env != NULL) {
    num_time_periods = atoi(num_periods_env);
    if (num_time_periods < 1) num_time_periods = 1;
    if (num_time_periods > 100) num_time_periods = 100;
    fprintf(stderr, "DEBUG PHASE3 CONSTR: Multi-temporal constraints for T=%d periods\n", num_time_periods);
}
int vars_per_period = nedges + num_z_vars;
```

#### Added Main Time Loop (lines 510-533):
```c
/* PSW: Phase 3 - MAIN TIME LOOP - Generate constraints for all T periods */
for (int time_period = 0; time_period < num_time_periods_constr; time_period++) {
    int period_offset = time_period * vars_per_period_constr;
    fprintf(stderr, "DEBUG PHASE3 CONSTR: Generating constraints for period t=%d (offset=%d)\n",
        time_period, period_offset);

    // All constraint generation happens here
}
```

#### Updated All Constraint Types:

1. **Spanning constraints** (lines 551-564):
   - Added `period_offset` to x[i,t] indices
   - Added `period_offset` to not_covered[j,t] indices

2. **Hard cutset constraints** (line 600):
   - Updated to `(period_offset + k) + RC_VAR_BASE`

3. **Soft cutset constraints** (lines 654, 658, 675, 680):
   - Type 1: x[k,t] + not_covered[terminal_idx,t] ≤ 1
   - Type 2: Σx[i,t] + n·not_covered[j,t] ≤ n

4. **Source terminal constraint** (lines 689-700):
   - not_covered[0,t] = 0 for each period

5. **Incompatibility constraints** (lines 721-725):
   - x[j,t] + x[i,t] ≤ 1

6. **2-terminal SEC constraints** (line 780):
   - Updated to use period_offset

7. **Budget constraint** (lines 808-847):
   - Σ normalized_tree_cost × x[i,t] ≤ budget_limit (per period)

8. **"At least one FST" constraint** (lines 849-861):
   - Σ x[i,t] ≥ 1 (per period)

9. **Z[e,t] linking constraints** (lines 863-907):
   - Z[e,t] ≤ Σ(x[i,t]: FST i contains edge e)
   - Updated variable indexing: `period_offset + nedges + num_terminals_global + edge_idx`

#### Closed Time Loop (lines 909-911):
```c
/* PSW: Phase 3 - Close time loop */
}  /* End of for (time_period = 0; time_period < num_time_periods_constr; time_period++) */
fprintf(stderr, "DEBUG PHASE3 CONSTR: Completed constraint generation for all %d time periods\n", num_time_periods_constr);
```

### 2. constrnt.c (Objective function - already updated in earlier work)

Updated objective function loop (lines 1228-1278):
```c
/* PSW: Phase 3 - Set objectives for all T time periods */
for (int t = 0; t < num_time_periods; t++) {
    for (i = 0; i < nedges; i++) {
        int var_idx = t * vars_per_period + i;
        objx [var_idx] = normalized_tree_cost + battery_cost_term;
    }

    for (i = 0; i < num_not_covered_lp; i++) {
        int var_idx = t * vars_per_period + nedges + i;
        objx [var_idx] = beta;
    }

    for (i = 0; i < num_z_vars_lp; i++) {
        int var_idx = t * vars_per_period + nedges + num_not_covered_lp + i;
        objx [var_idx] = 0.0;
    }
}
```

### 3. run_optimization.sh

Updated implementation status header (lines 14-21):
```bash
# ✅ Phase 3: Multi-Period Variables - COMPLETE
#    - Multi-temporal variable structure: x[i,t], not_covered[j,t], Z[e,t]
#    - Environment variable: GEOSTEINER_TIME_PERIODS=T (default: 1)
#    - Total variables: T × (nedges + nterms + num_edges)
#    - Objective function updated for all T periods
#    - All constraints replicated for all T periods with correct indexing
#    - Variable indexing: var_idx = t × vars_per_period + offset
#    - Tested with T=3 (36 vars) and T=15 (180 vars) - working correctly
```

---

## Testing & Verification

### Test Case: 4 terminals, 4 FSTs, 4 edges

#### Test 1: T=3 time periods
```bash
GEOSTEINER_BUDGET=1.8 GEOSTEINER_TIME_PERIODS=3 ./bb < test_4.fst
```

**Results:**
- ✅ Variables: 36 cols (3 × 12 = 36)
- ✅ Constraints: Generated for all 3 periods (t=0, t=1, t=2)
- ✅ Period offsets: 0, 12, 24 (correct)
- ✅ Debug output confirms all constraint types replicated

**CPLEX Output:**
```
DEBUG PHASE3 CONSTR: Completed constraint generation for all 3 time periods
DEBUG PHASE3: Total variables = 3 periods × 12 vars/period = 36 total
% cpx allocation: 132 rows, 36 cols, 720 nz
% @PL 60 rows, 36 cols, 168 nonzeros, 39 slack, 21 tight.
% @PL 27 rows, 36 cols, 75 nonzeros, 0 slack, 27 tight.
```

#### Test 2: T=15 time periods
```bash
GEOSTEINER_BUDGET=1.8 GEOSTEINER_TIME_PERIODS=15 ./bb < test_4.fst
```

**Results:**
- ✅ Variables: 180 cols (15 × 12 = 180)
- ✅ Constraints: Generated for all 15 periods (t=0 to t=14)
- ✅ Scales correctly without errors

**CPLEX Output:**
```
DEBUG PHASE3 CONSTR: Completed constraint generation for all 15 time periods
DEBUG PHASE3: Total variables = 15 periods × 12 vars/period = 180 total
% cpx allocation: 660 rows, 180 cols, 3600 nz
% @PL 300 rows, 180 cols, 840 nonzeros, 195 slack, 105 tight.
% @PL 135 rows, 180 cols, 375 nonzeros, 0 slack, 135 tight.
```

---

## Key Technical Details

### Environment Variables
- **GEOSTEINER_TIME_PERIODS**: Number of time periods T (default: 1)
  - Range: 1-100
  - Example: `export GEOSTEINER_TIME_PERIODS=15`

### Constraint Count (for T periods, n FSTs, m terminals)
- Spanning: T constraints
- Soft cutset (type 1): T × Σ(degree per terminal) constraints
- Soft cutset (type 2): T × m constraints
- Source terminal: T constraints
- Budget: T constraints
- "At least one FST": T constraints
- Z-linking: T × num_edges constraints
- Incompatibility: T × num_incompatible_pairs constraints
- 2-SEC: T × num_2secs constraints

**Total ≈ T × (constraints from single-period formulation)**

### Memory Allocation
All arrays properly sized for multi-temporal:
```c
macsz = num_time_periods * vars_per_period;
// Total variables = T × (nedges + nterms + num_z_vars)
```

---

## Debug Output Examples

### Constraint Generation:
```
DEBUG PHASE3 CONSTR: Multi-temporal constraints for T=3 periods
DEBUG PHASE3 CONSTR: Generating constraints for period t=0 (offset=0)
DEBUG SPANNING: Added modified spanning constraint: Σ(|FST|-1)*x + Σnot_covered = 3
DEBUG CONSTRAINT: Added constraint x[0,t=0] + not_covered[0,t=0] ≤ 1 for terminal 0
DEBUG BUDGET: Budget constraint added to pool with 4 FSTs for t=0
DEBUG PHASE3: Added 4 Z[e,t] linking constraints for t=0
DEBUG PHASE3 CONSTR: Generating constraints for period t=1 (offset=12)
...
DEBUG PHASE3 CONSTR: Completed constraint generation for all 3 time periods
```

### Objective Function:
```
DEBUG PHASE3: Multi-temporal optimization with T=3 time periods
DEBUG PHASE3: Total variables = 3 periods × 12 vars/period = 36 total
```

---

## Known Limitations (To be addressed in Phase 4)

1. **No inter-period linking constraints yet**
   - Currently, each time period is independent
   - Phase 4 will add battery dynamics linking periods

2. **No battery state transitions**
   - Battery levels not yet evolving across periods
   - Phase 4 will implement b[j,t+1] = b[j,t] + charge - demand

3. **Objective function not yet time-aware**
   - Current objective: Minimize Σₜ Σᵢ (cost[i] + battery_cost[i]) × x[i,t]
   - Phase 5 will add graph distance term for topology changes

---

## Next Steps: Phase 4 - Battery Dynamics

### Planned Implementation:

1. **Battery state variables**: b[j,t] for each terminal j, time period t
   - Continuous variables: 0 ≤ b[j,t] ≤ 100

2. **Battery evolution constraints**:
   ```
   b[j,t+1] = b[j,t] + charge_rate × coverage[j,t] - demand_rate

   where coverage[j,t] = 1 - not_covered[j,t]
   ```

3. **Initial battery conditions**: b[j,0] = initial_battery[j]

4. **Battery-dependent costs**: Use b[j,t] in objective (low battery = high priority)

5. **Inter-period FST stability**: Optional constraints to limit topology changes

---

## Compilation

```bash
make clean
make bb
```

No warnings or errors (except 2 benign unused variable warnings in solver.c).

---

## Summary

**Phase 3 Status: COMPLETE ✅**

All Phase 3 objectives achieved:
- ✅ Multi-temporal variable structure implemented
- ✅ Variable indexing with time dimension working correctly
- ✅ All constraints replicated for T time periods
- ✅ Objective function extended to all periods
- ✅ Tested with T=3 and T=15 successfully
- ✅ Code documented and ready for Phase 4

**Total Time**: ~2 hours of systematic implementation and testing

**Lines of Code Modified**: ~200 lines in constrnt.c

**Ready for**: Phase 4 - Battery Dynamics (inter-period linking)
