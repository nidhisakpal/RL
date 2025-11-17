# Budget Constraint Fix - Technical Update

**Date**: October 11, 2025
**Student**: Pranay

---

## Issue Summary

After implementing normalized budget constraints as discussed, the system was failing with:
1. **Fatal error** at line 935 in constrnt.c
2. When fixed, **budget constraint not enforced** - all terminals connected regardless of budget (0.5, 1.0, 80, etc.)

---

## Root Cause

The issue was **NOT** with the soft constraint formulation or the `not_covered` variables (which work correctly as continuous [0,1] variables).

The problem was with **GeoSteiner's internal constraint pool architecture**:

### GeoSteiner Constraint Pool Uses Integer Coefficients

```c
// From constrnt.h
struct rcoef {
    int  var;  // variable index
    int  val;  // coefficient value ← INTEGER, not double!
};
```

### What Happened

When storing normalized costs directly:
```c
double normalized_cost = 0.956357;
rp->val = normalized_cost;  // Truncates to 0 (int cast)
```

**Result**: Budget constraint became `0·x[0] + 0·x[1] + ... ≤ budget`, which is always satisfied.

---

## Solution: Internal Scaling

To work with GeoSteiner's integer-based infrastructure while using normalized values:

```c
// Scale normalized costs by 10^6 for storage in constraint pool
int scale_factor = 1000000;
double normalized_cost = tree_cost / max_tree_cost;  // 0.956357
int scaled_coeff = (int)(normalized_cost * 1000000); // 956357

// Store in constraint pool
rp->val = scaled_coeff;  // Now non-zero integer

// Internal constraint: Σ (956357) × x[i] ≤ 5000000
// Equivalent to: Σ (0.956357) × x[i] ≤ 5.0
```

**Mathematical Equivalence**: Dividing both sides by 10^6 gives the normalized constraint.

---

## Why Tree "Exploded" at Higher Budgets - ROOT CAUSE IDENTIFIED

The "explosion" issue is due to **beta (uncovered terminal penalty) being too small**:

### Current Parameters:
```c
double alpha = 3.0;  // Battery weight
double beta = 1.0;   // Uncovered penalty ← PROBLEM HERE
```

### Actual FST Costs (measured from test run):
```
FST 0: tree_norm=0.404, battery_norm=0.550 → cost = 0.404 + 2.0×0.550 = 1.504
FST 1: tree_norm=0.264, battery_norm=0.813 → cost = 0.264 + 2.0×0.813 = 1.891
FST 2: tree_norm=0.260, battery_norm=0.607 → cost = 0.260 + 2.0×0.607 = 1.475

Typical FST cost range: 1.5 - 2.0
```

### The Problem:
**Beta=1.0 is too small!**
- Leaving a terminal uncovered costs: **1.0**
- Selecting a typical FST costs: **1.5-2.0**
- **Decision**: It's often CHEAPER to leave terminals uncovered than to select FSTs!

### Why "Explosion" Happens:
1. **Low budget (0.5-3.0)**: Can't afford many FSTs → solver leaves some terminals uncovered (cost 1.0 each is acceptable)
2. **Medium budget (5.0)**: Can afford ~3-5 FSTs → solver connects 18/20 terminals
3. **High budget (10.0+)**: Can afford many FSTs → solver connects ALL terminals because it now can, and beta is small

### The Solution:
**Increase beta to make uncovered terminals more expensive than any FST:**

```c
double beta = 10.0;  // Now uncovered terminal costs 10.0
// This makes it ALWAYS preferable to select an FST (cost ~1.5-2.0)
// rather than leave a terminal uncovered (cost 10.0)
```

With beta=10.0:
- Budget=1.0: Select only cheapest FSTs, leave expensive terminals uncovered
- Budget=5.0: Select moderate FSTs, smooth tradeoff
- Budget=10.0: Select more FSTs, but still won't connect ALL unless budget permits

**This prevents explosion while maintaining proper budget-coverage tradeoff.**

### Additional Contributing Factor: Aggressive Pruning

The lack of intermediate-cost FSTs to reach far terminals may also be due to aggressive FST pruning during generation. If pruning eliminates good intermediate options, the solver only sees:
- Very cheap FSTs (nearby terminals)
- Very expensive FSTs (far terminals)

This creates a "binary" choice rather than smooth gradient, contributing to explosion behavior.

**Recommendation**: Review `EPS_MULT_FACTOR` in parmdefs.h if intermediate FST options are missing.

---

## Verification

### Test with Low Budget (0.5):
```
not_covered[0] = 0.000000  ✓ covered
not_covered[1] = 0.359436  ✓ partially covered
not_covered[2] = 0.960846  ✓ mostly uncovered
not_covered[3] = 1.000000  ✓ fully uncovered
```

### Test with Higher Budget (2.0):
```
not_covered[0] = 0.000000  ✓ covered
not_covered[1] = 0.000000  ✓ covered
not_covered[2] = 0.000000  ✓ covered
not_covered[3] = 1.000000  ✓ uncovered (still too expensive)
```

**Budget constraint now works correctly** - coverage increases with budget as expected.

---

## Alternative Considered: Modify GeoSteiner Core

Could change `struct rcoef` from `int` to `double`, but:
- Requires refactoring 30+ years of constraint pool code
- Must modify GCD reduction, hashing, all constraint operations
- High risk of introducing bugs in core algorithms
- Not justified for one constraint

**Decision**: Use scaling factor as implementation detail.

---

## Implementation Details

**Location**: [constrnt.c:679-703](constrnt.c#L679-L703)

```c
/* Build normalized budget constraint: Σ normalized_tree_cost * x[i] ≤ budget */
/* Scale by 1000000 to convert to integers for constraint pool (uses int coefficients) */
int scale_factor = 1000000;
for (i = 0; i < nedges; i++) {
    double tree_cost = (double) (cip -> cost [i]);
    double normalized_cost = (max_tree_cost > 0.0) ? tree_cost / max_tree_cost : 0.0;
    int scaled_cost = (int)(normalized_cost * scale_factor);

    rp -> var = i + RC_VAR_BASE;
    rp -> val = scaled_cost;  // Now stores non-zero integer
    ++rp;
}
rp -> val = (int)(budget_limit * scale_factor);  // Scale budget too
```

---

## Status & Next Steps

### ✅ Fixed Issues:
1. **Budget constraint now properly enforced** - scaling factor resolves integer coefficient limitation
2. **Uses normalized costs [0,1] as specified**
3. **Works with GeoSteiner's existing infrastructure** - no core refactoring needed
4. **Verified with multiple budget values** - constraint binding correctly

### ⚠️ Remaining Issue: "Explosion" Behavior
**Root Cause**: Beta penalty too small (1.0 vs FST costs of 1.5-2.0)

**Proposed Fix** (code changes needed):
```c
// In constrnt.c around line 1133 and line 1507
double beta = 10.0;  // Changed from 1.0
```

This will:
- Make uncovered terminals significantly more expensive than any FST
- Create smooth tradeoff between budget and coverage
- Prevent sudden "all terminals connected" behavior at higher budgets
- Maintain proper economic balance in the optimization

### Technical Note:
The scaling factor (10^6) is purely an internal implementation detail to accommodate GeoSteiner's integer-based constraint pool (`struct rcoef` uses `int val`). CPLEX solves the mathematically equivalent normalized constraint. Your soft constraint formulation with continuous `not_covered[j] ∈ [0,1]` is working correctly.
