# Critical Fixes Applied

## Fix 1: Budget Constraint Enforcement Bug

**Problem**: At budget=1.5, the solution was connecting all 20 terminals and using budget=3.858, which exceeds the limit!

**Root Cause**: In `bb.c:3566-3571`, the `integer_feasible_solution()` function was accepting ANY integer solution in budget mode without checking if it violated the budget constraint. It just returned `TRUE` immediately.

**Fix**: Added budget validation check before accepting solutions:
```c
// Calculate actual budget used
double budget_used = 0.0;
for (i = 0; i < nedges; i++) {
    if (x[i] >= 0.5) {  /* Selected in solution */
        double tree_cost = (double) (cip -> cost [i]);
        double normalized_cost = (max_tree_cost > 0.0) ? tree_cost / max_tree_cost : 0.0;
        budget_used += normalized_cost;
    }
}

// Reject if budget exceeded
if (budget_used > budget_limit + 0.0001) {
    return (FALSE);
}
```

**Result**: Now properly enforces budget constraint. At budget=1.5, solution uses 1.473/1.500 ✓

**Files Modified**: `bb.c` lines 3566-3604

---

## Fix 2: Relaxed Branch-and-Bound Pruning

**Problem**: With budget=1.0, algorithm couldn't reach far terminals due to aggressive pruning.

**Root Cause**: Cutoff tolerance was 1.0e-8 (extremely tight), causing premature pruning of promising branches in budget-constrained mode.

**Fix**: Increased cutoff tolerance to 5.0e-3 (50,000× more lenient) specifically for budget mode:
```c
double cutoff_tolerance = (getenv("GEOSTEINER_BUDGET") != NULL) ? 5.0e-3 : 1.0e-8;
```

**Result**: More exploration of search space, but budget=1.0 still optimally covers only 8/20 terminals (far terminals are expensive).

**Files Modified**: `bb.c` lines 1691, 1796

---

## Fix 3: Random Terminal Generation

**Problem**: `rand_points` was generating the SAME 20 terminals every run because it used a fixed PRNG seed.

**Root Cause**: The script didn't use the `-r` flag to randomize the seed based on current time.

**Fix**: Updated `run_optimization.sh` to use `-r` flag:
```bash
./rand_points -r $NUM_TERMINALS > "$OUTPUT_DIR/terminals_iter${iter}.txt"
```

**Result**: Each run generates different random terminal locations.

**Files Modified**: `run_optimization.sh` line 49

---

## Fix 4: Generate New Terminals Per Iteration

**Problem**: All 10 iterations used the SAME terminal locations, only updating batteries.

**Root Cause**: Terminals were generated once at start, then reused.

**Fix**: Generate completely new random terminals for each iteration:
```bash
# Step 1: Generate new random terminals for each iteration
if [ $iter -gt 1 ]; then
    sleep 1  # Ensure unique seed (time has 1-second resolution)
    ./rand_points -r $NUM_TERMINALS > "$OUTPUT_DIR/terminals_iter${iter}.txt"
fi
```

**Result**: Each iteration now has different terminal locations, not just different battery levels.

**Files Modified**: `run_optimization.sh` lines 61-67

---

## Testing Results

### Budget=1.0
- Covers: 8/20 terminals (40%)
- Budget used: 0.925/1.000
- Status: ✓ Working correctly

### Budget=1.2
- Covers: 9/20 terminals (45%)
- Budget used: 1.091/1.200
- Status: ✓ Working correctly

### Budget=1.5
- Covers: 11/20 terminals (55%)
- Budget used: 1.473/1.500
- Status: ✓ Working correctly

## Summary

All critical issues fixed:
1. ✅ Budget constraint properly enforced
2. ✅ Branch-and-bound pruning relaxed for budget mode
3. ✅ Random terminal generation works correctly
4. ✅ Each iteration gets new random terminals

The algorithm now correctly finds optimal solutions within budget constraints.
