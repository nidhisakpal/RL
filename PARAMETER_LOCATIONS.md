# Alpha and Beta Parameter Locations

This document lists ALL locations in the codebase where α (alpha) and β (beta) parameters are defined and must be kept consistent.

## ✅ ALL FIXED - Current Values:
- **α (alpha) = 1.0** - Balanced battery/tree weight
- **β (beta) = 1000.0** - Uncovered terminal penalty (normalized scale)

## File Locations

### 1. constrnt.c - Line 1046 ✅
**Context:** Objective coefficient setup for CPLEX LP
```c
double alpha = 1.0;  /* BALANCED: Battery and tree cost have equal priority */
```

### 2. constrnt.c - Line 1126 ✅
**Context:** Not_covered penalty for CPLEX LP
```c
double beta = 1000.0;  /* Penalty for uncovered terminals (matches normalized scale) */
```

### 3. constrnt.c - Line 1463 ✅
**Context:** Alternative LP formulation path
```c
double alpha = 1.0;  /* BALANCED: Battery and tree cost have equal priority */
```

### 4. constrnt.c - Line 1464 ✅
**Context:** Alternative LP formulation path - beta
```c
double beta = 1000.0;  /* NORMALIZED SCALE: Penalty for uncovered terminals (matches normalized FST objectives 0-3) */
```

### 5. solver.c - Line 1327 ✅
**Context:** Objective cost computation
```c
double alpha = 1.0;  /* Equal weight for both components after normalization */
```

### 6. solver.c - Line 1328 ✅
**Context:** Penalty for solution evaluation
```c
double beta = 1000.0;  /* Penalty for uncovered terminals */
```

### 7. ub.c - Line 909 ✅
**Context:** Upper bound heuristic - Kruskal algorithm
```c
double alpha = 1.0;  /* Equal weight for both components after normalization */
```

**Note:** ub.c does NOT use β (no uncovered terminal penalty in upper bound heuristic)

## Battery Normalization

### 1. constrnt.c - Line 1114 ✅
```c
double normalized_battery_cost = battery_cost / 100.0;  /* Divide by 100 for battery normalization */
```

### 2. solver.c - Line 1395 ✅
```c
double normalized_battery_cost = battery_cost / 100.0;  /* Divide by 100 for battery normalization */
```

### 3. ub.c - Line 970 ✅
```c
double normalized_battery_cost = battery_cost / 100.0;  /* Divide by 100 for battery normalization */
```

## How to Change Parameters

### To Change Alpha (Battery Weight):

Edit ALL of these lines:
1. [constrnt.c:1046](constrnt.c#L1046) - Change `double alpha = 1.0;`
2. [constrnt.c:1463](constrnt.c#L1463) - Change `double alpha = 1.0;`
3. [solver.c:1327](solver.c#L1327) - Change `double alpha = 1.0;`
4. [ub.c:909](ub.c#L909) - Change `double alpha = 1.0;`

Then rebuild: `make bb`

### To Change Beta (Uncovered Penalty):

Edit ALL of these lines:
1. [constrnt.c:1126](constrnt.c#L1126) - Change `double beta = 1000.0;`
2. [constrnt.c:1464](constrnt.c#L1464) - Change `double beta = 1000.0;`
3. [solver.c:1328](solver.c#L1328) - Change `double beta = 1000.0;`

Then rebuild: `make bb`

### To Change Budget:

Budget is passed via environment variable:
```bash
GEOSTEINER_BUDGET=2.5 ./bb test.fst
```

Or use the run_optimization.sh script:
```bash
./run_optimization.sh 20 4.0 10.0 5.0 5 output_dir no
#                         ^^^ budget parameter
```

## Verification Script

To verify all parameters are consistent:

```bash
#!/bin/bash
echo "=== Checking Alpha Values ==="
grep -n "double alpha = " constrnt.c solver.c ub.c | grep -v "//"

echo ""
echo "=== Checking Beta Values ==="
grep -n "double beta = " constrnt.c solver.c | grep -v "//"

echo ""
echo "=== Checking Battery Normalization ==="
grep -n "battery_cost / 100" constrnt.c solver.c ub.c
```

## Common Mistakes to Avoid

1. **Forgetting ub.c** - The ub.c file has its own alpha definition that must match
2. **Missing battery normalization** - Must divide by 100, not by max_battery_cost
3. **Inconsistent β values** - All β values must be the same across files
4. **Not rebuilding** - Must run `make bb` after any changes

## Testing After Parameter Changes

After changing parameters, run a test:
```bash
./run_optimization.sh 8 2.5 10.0 5.0 3 test_params no
```

Check that:
- All 8 terminals are covered (100% coverage)
- Low-battery terminals are prioritized
- Objective values are reasonable (10-100 range)
- Budget utilization is high (>80%)
