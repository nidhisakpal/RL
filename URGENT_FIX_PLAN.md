# URGENT FIX PLAN - Disconnected Trees & High MIP Gap

## Problem Summary

### Issue 1: Two Disconnected Trees in Iter 9/10
- **Symptoms:** results5 shows terminals {0,1} and {13-18} forming separate trees
- **Root Cause:** Soft spanning constraint doesn't enforce connectivity - only counts edges
- **Impact:** CRITICAL - violates fundamental requirement that all covered terminals connect to source

### Issue 2: MIP Gap 50%+ (Was 6-12%)
- **Symptoms:** All iterations in results5 show 44-58% gap
- **Root Cause:** alpha=1.0 makes problem too hard for CPLEX
- **Evidence:** results_corrected (alpha=50.0) had 6-12% gap
- **Impact:** CRITICAL - solution quality is poor

## Root Cause Analysis

### Why Disconnected Trees Happen

Current soft spanning constraint:
```
Σ(|FST[i]| - 1) × x[i] + Σnot_covered[j] = n-1
```

This says: "number of edges + uncovered terminals = n-1"

**Problem:** This counts EDGES but doesn't ensure CONNECTIVITY!

**Example:**
- Tree A: Terminal 0 → Terminal 1 (1 edge)
- Tree B: Terminals 13→14→15→16→17→18 (5 edges)
- Total: 6 edges, 8 covered terminals, 12 uncovered
- Constraint: 6 + 12 = 18 = 20-1-1 ✓ (satisfied!)
- But trees are DISCONNECTED! ❌

### Why MIP Gap is 50%+

**Comparison:**
- results_corrected: alpha=50.0 → gap=6-12% ✅
- results5: alpha=1.0 → gap=50%+ ❌

**Why alpha=1.0 is too hard:**
- Tree costs are ~0.3-0.5 (normalized)
- Battery rewards with alpha=1.0: -1.0 to 0.0 (for 0-100% battery)
- Rewards are comparable to tree costs → LP relaxation is very tight
- CPLEX struggles to find integer solutions

**Why alpha=50.0 works better:**
- Battery rewards with alpha=50.0: -50.0 to 0.0
- Rewards DOMINATE tree costs (50x larger!)
- LP relaxation is looser → easier to solve
- Gap: 6-12% ✅

## Proposed Fixes

### FIX #1: Increase Alpha (IMMEDIATE)

**Change:** alpha = 1.0 → alpha = 10.0 or 20.0

**Why 10-20 instead of 50:**
- alpha=50 works (6-12% gap) but may cover too many terminals
- alpha=10-20 provides:
  - Reasonable battery influence (10-20x tree cost)
  - Lower gap than alpha=1.0
  - More selective coverage than alpha=50

**Implementation:**
```bash
# In bb.c or lpinit.c, change default alpha
#define DEFAULT_ALPHA 10.0  // was 1.0
```

### FIX #2: Fix Disconnected Trees

**Option A: Remove Soft Spanning Constraint** (RECOMMENDED)

**Rationale:**
- Cutset constraints already ensure each terminal is covered by at least one FST
- Source constraint ensures terminal 0 is covered
- Budget constraint limits total cost
- Soft spanning constraint is REDUNDANT and causes disconnectivity

**Implementation:**
Comment out soft spanning constraint in constrnt.c around line 580:

```c
/* COMMENTED OUT - Causes disconnected trees
if (num_terminals > 1) {
    // ... soft spanning constraint code ...
}
*/
```

**Option B: Add Connectivity Constraints** (MORE COMPLEX - NOT RECOMMENDED)

Would need to add flow-based or subtour elimination constraints. Much more complex.

## Recommended Action Plan

### Step 1: Increase Alpha (Quick Fix)
1. Find where alpha is set (lpinit.c or bb.c)
2. Change default from 1.0 to 10.0
3. Recompile: `make bb`
4. Test on small instance

### Step 2: Remove Soft Spanning Constraint (Proper Fix)
1. Open constrnt.c
2. Find soft spanning constraint code (around line 580-600)
3. Comment out the entire block
4. Recompile: `make bb`
5. Test thoroughly

### Step 3: Validate
1. Run optimization: `./run_optimization.sh 20 1.0 10`
2. Check results:
   - MIP gap should be <15%
   - All covered terminals should form ONE connected tree with terminal 0
   - 8-12 terminals covered (not 16+)

## Testing Checklist

After fixes:
- [ ] MIP gap <15% across all iterations
- [ ] Terminal 0 always covered
- [ ] All covered terminals form single connected tree
- [ ] 8-12 terminals covered per iteration (adjust budget/alpha if needed)
- [ ] Battery levels evolve correctly (covered: +5%, uncovered: -5%)
- [ ] No compilation errors or warnings

## Files to Modify

1. **lpinit.c or bb.c** - Change default alpha from 1.0 to 10.0
2. **constrnt.c** - Comment out soft spanning constraint (lines ~580-600)

## Expected Outcomes

After fixes:
- **MIP Gap:** 50%+ → 6-15% ✓
- **Connectivity:** Two trees → One tree ✓
- **Coverage:** 16+ terminals → 8-12 terminals ✓ (may need budget adjustment)
- **Performance:** Poor solutions → Good solutions ✓

## Risk Assessment

**Low Risk:**
- Increasing alpha is safe - just changes objective weights
- Removing soft spanning is safe - constraint was redundant

**Testing Required:**
- Verify connectivity on multiple random instances
- Check that all solutions form single tree rooted at terminal 0
