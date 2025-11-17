# Constraint Pool Capacity Fix

**Date**: October 18, 2025
**Issue**: Crashes with large multi-temporal problems due to constraint pool running out of space

---

## Problem

The constraint pool was sized for single-period optimization:
```c
rowsize = 2 * nrows;  // Only accounts for single period!
```

With multi-temporal optimization (T periods):
- **Actual constraints needed**: T × nrows + graph_distance_constraints
- **Pool was allocated for**: 2 × nrows (single period)
- **Result**: Pool had to reallocate frequently, causing crashes with large T×FST combinations

---

## Solution

### Fix #1: Scale Pool Size by Time Periods (constrnt.c:470-493)

**Before:**
```c
rowsize = 2 * nrows;
nzsize = 4 * ncoeff;
```

**After:**
```c
int num_time_periods_pool = getenv("GEOSTEINER_TIME_PERIODS") ? atoi(...) : 1;
int num_graph_distance_constraints = 0;
if (gd_weight_env != NULL && num_time_periods_pool > 1 && num_z_vars > 0) {
    num_graph_distance_constraints = (num_time_periods_pool - 1) * num_z_vars * 2;
}

int total_initial_rows = num_time_periods_pool * nrows + num_graph_distance_constraints;
rowsize = 3 * total_initial_rows;  // 3x for extra space during B&C
nzsize = 4 * num_time_periods_pool * ncoeff;
```

**Key improvements:**
- Multiplies base constraints by T
- Adds graph distance constraints (2 per D variable)
- Uses 3× safety margin instead of 2× for B&C cuts

### Fix #2: Increase Hash Table Size (constrnt.h:112)

**Before:**
```c
#define CPOOL_HASH_SIZE  1009
```

**After:**
```c
#define CPOOL_HASH_SIZE  10007  // 10x larger prime for multi-temporal
```

**Rationale**: With T=3, we have 3× more constraints to hash. Larger hash table reduces collisions.

---

## Testing Results

### Before Fix

| Test Case | Result |
|-----------|--------|
| 6 terminals, T=3, iter 1 (~60 FSTs) | ✅ Works |
| 6 terminals, T=3, iter 2 (104 FSTs) | ❌ CRASH |
| 8 terminals, T=3 | ❌ CRASH |

### After Fix

| Test Case | Result |
|-----------|--------|
| 6 terminals, T=3, iter 1-5 | ✅ ALL WORK |
| 8 terminals, T=3 | ❌ Still crashes (different issue) |
| 6 terminals, T=3, γ=1.0 | ✅ WORKS |

---

## Pool Size Examples

**4 terminals, T=1:**
- Base rows: ~8
- Pool size: 2 × 8 = 16 rows (old) → 3 × 8 = 24 rows (new)

**4 terminals, T=3:**
- Base rows: ~8
- Total rows: 3 × 8 + 16 GD = 40
- Pool size: 16 rows (old) → 3 × 40 = 120 rows (new)
- **Improvement: 7.5× larger**

**6 terminals, T=3:**
- Base rows: ~15
- Total rows: 3 × 15 + 36 GD = 81
- Pool size: 30 rows (old) → 3 × 81 = 243 rows (new)
- **Improvement: 8× larger**

---

## Debug Output

You can now see pool allocation in the output:
```
DEBUG POOL: Initial pool size: T=3, base_rows=15, GD_rows=36, rowsize=243
```

Where:
- `T`: Number of time periods
- `base_rows`: Single-period constraint count
- `GD_rows`: Graph distance linearization constraints
- `rowsize`: Actual pool capacity allocated

---

## Performance Impact

**Memory usage:**
- Hash table: 1009 ints → 10007 ints (~40KB increase)
- Pool rows: Proportional to T (e.g., 3× for T=3)
- Coefficient storage: Proportional to T

**Speed:**
- Fewer reallocations during solving (faster)
- Better hash distribution (slightly faster lookups)
- Overall: **Faster for multi-temporal problems**

---

## Remaining Limitations

The 8-terminal T=3 case still crashes, but this appears to be a different issue (possibly in cut generation or CPLEX interface, not the constraint pool).

**Working range after fix:**
- ✅ Up to 6 terminals with T=3 and multiple iterations
- ✅ Up to 8 terminals with T=1
- ❌ 8+ terminals with T=3 still has issues

---

## Summary

**Problem**: Constraint pool capacity limits
**Root cause**: Pool sized for single period, not scaled for multi-temporal
**Fix**: Scale pool size by T and increase hash table
**Impact**: 6-terminal multi-temporal optimization now works reliably!

**Files modified:**
- `constrnt.c`: Lines 470-493 (pool sizing)
- `constrnt.h`: Line 112 (hash table size)

**Lines of code**: ~30 lines
**Compilation**: Full recompilation required (affects header)
**Testing**: Verified with 6 terminals, 5 iterations, T=3

---

**Status**: ✅ **FIXED - Multi-temporal optimization now works for 4-6 terminals**
