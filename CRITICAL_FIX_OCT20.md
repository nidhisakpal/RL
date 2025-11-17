# Critical Bug Fixes - October 20, 2025

## Problem Summary

You reported three critical issues in results3/:
1. **Battery charging bug** - Uncovered terminals were magically increasing battery levels
2. **Too many terminals covered** - 16-17 terminals connected instead of desired 10-12
3. **Extremely high MIP gap** - Some iterations showing 40% gap

## Root Cause Analysis

### Bug #1: Incorrect Coverage Parsing in battery_wrapper.c

**Root Cause:**
The `parse_coverage_from_solution()` function was parsing coverage from PostScript `% fs` lines, which include ALL FSTs that appear in the solution output, even those with `x[i] = 0.0` (not selected).

**Example:**
- Solution shows `% fs18: 17 12` in PostScript section
- But LP solution shows `x[18] = 0.000000` (NOT selected)
- Old code marked terminals 17 and 12 as covered anyway
- These terminals got +10% charge even though not connected!

**Impact:**
- Uncovered terminals received charge (+10%) instead of just demand (-5%)
- Battery levels didn't reflect actual network connectivity
- Optimization became meaningless as all terminals kept high battery levels

**Fix Applied:**
Changed `parse_coverage_from_solution()` to parse `not_covered[]` LP variables instead:

```c
/* OLD METHOD (BROKEN) - Parsed from PostScript */
while (fgets(line, sizeof(line), f)) {
    if (strstr(line, "% fs") && strstr(line, ":")) {
        // Marked all terminals in FST as covered
        // Even if FST has x[i] = 0!
    }
}

/* NEW METHOD (CORRECT) - Parses from LP variables */
while (fgets(line, sizeof(line), f)) {
    if (strstr(line, "not_covered[") && strstr(line, "] =")) {
        sscanf(line, "%*s %*s %*s not_covered[%d] = %lf", &terminal_id, &not_covered_value);
        final_not_covered[terminal_id] = not_covered_value;
    }
}

/* Terminal is covered only if not_covered[i] < 0.5 */
for (int i=0; i<max; i++) {
    coverage[i] = (final_not_covered[i] < 0.5) ? 1 : 0;
}
```

**Verification:**
```bash
./battery_wrapper -i results3/terminals_iter1.txt -s results3/solution_iter1.txt -o /tmp/test.txt -v

Output shows correct behavior:
 T0: 20.0 -> 100.0 (covered=1)  ✅ Source terminal, always charged
 T1: 49.1 -> 44.1 (covered=0)   ✅ Uncovered: demand only (-5%)
 T2: 35.1 -> 40.1 (covered=1)   ✅ Covered: charge - demand (+10-5 = +5%)
 T17: 22.7 -> 17.7 (covered=0)  ✅ Uncovered: demand only (-5%)
```

### Bug #2: Too Many Terminals Covered

**Current Status:** With budget=1.8 and alpha=1.0, solver covers 14-17 terminals (out of 20)

**Desired:** 10-12 terminals covered per iteration

**Options to Fix:**

#### Option 1: Reduce Budget (Recommended)
Current budget of 1.8 allows ~18 normalized tree edges. To get 10-12 terminals:
- **Try budget = 1.0 to 1.2**
- This limits tree cost, forcing solver to be more selective

#### Option 2: Increase Alpha
Current alpha=1.0 makes battery rewards equal weight to tree costs. To prioritize fewer, low-battery terminals:
- **Try alpha = 0.5 to 0.75**
- Lower alpha reduces battery reward influence
- Solver will focus on cheaper trees rather than low-battery nodes

#### Option 3: Add Hard Constraint
Add explicit constraint: `Σ x[i] ≤ K` where K = desired number of FSTs (e.g., 10-12)

**Recommendation:** Start with Option 1 (reduce budget to 1.0-1.2)

### Bug #3: High MIP Gap (40%+)

**Current Gap Issues:**
```
iter1:  28.48%
iter2:  31.16%
iter10: 40.57%  ← EXTREMELY HIGH
```

**Likely Causes:**
1. **Battery charging bug** (now fixed) - Incorrect battery levels made LP harder to solve
2. **Too many variables** - With 16-17 terminals covered, problem becomes complex
3. **CPLEX parameter tuning** - May need to adjust MIP gap tolerance or time limits

**Next Steps:**
1. Re-run optimization with fixed battery_wrapper
2. Reduce budget to 1.0-1.2 (fewer terminals → simpler problem)
3. Check if gaps improve
4. If still high, investigate CPLEX parameters in `lpsolver.h` or `lpinit.c`

## Files Modified

1. **battery_wrapper.c** (Lines 152-187)
   - Fixed `parse_coverage_from_solution()` to use `not_covered[]` LP variables
   - Compiled and ready to use

2. **simulate.c** (Lines 731-784)
   - Added coverage check before drawing FST edges
   - Only draws edges between covered terminals
   - This was for visualization fix, still valid

## Action Items for You

### IMMEDIATE (Do This First):
1. **Delete broken results:**
   ```bash
   rm -rf results3/
   ```

2. **Re-run optimization with fixed battery_wrapper:**
   ```bash
   # Use reduced budget for 10-12 terminals
   ./run_optimization.sh 20 1.0 10
   ```

3. **Check results:**
   - Verify 10-12 terminals covered per iteration
   - Verify MIP gaps are reasonable (<10%)
   - Verify battery levels change correctly (covered: +5%, uncovered: -5%)

### IF ISSUES PERSIST:

**If still too many terminals covered:**
- Reduce budget further: try 0.8 or 0.9
- OR increase alpha: try 0.5 or 0.75

**If MIP gaps still high (>10%):**
- Check CPLEX parameter files
- May need to increase time limit or adjust gap tolerance
- Consider simplifying problem (fewer terminals, e.g., 15 instead of 20)

## Testing the Fix

Quick test to verify battery_wrapper works:
```bash
# Test on results3 iteration 1
./battery_wrapper -i results3/terminals_iter1.txt \
                   -s results3/solution_iter1.txt \
                   -o /tmp/test_battery.txt \
                   -c 10.0 -d 5.0 -v

# Should show:
# - Covered terminals: +10-5 = +5% net change
# - Uncovered terminals: 0-5 = -5% net change
# - Terminal 0 (source): always 100%
```

## Summary

✅ **FIXED:** Battery charging bug - uncovered terminals no longer receive charge
⚠️ **TODO:** Adjust budget/alpha to get 10-12 terminals instead of 16-17
⚠️ **TODO:** Re-run and verify MIP gaps are acceptable

The fundamental battery charging bug is now fixed. The other issues (too many terminals, high gaps) should improve once you re-run with the fixed code and adjusted parameters.
