# Normalization Fix - October 23, 2025

## Problem
The normalization implementation was causing all budget constraint coefficients to be 0, leading to a crash at constrnt.c:1037 with error "Fatal software error" (FATAL_ERROR_IF com_factor EQ 0).

## Root Cause Analysis

### Original Implementation Issues:

1. **Edge-Level Normalization (lines 241-301)**:
   - Attempted to normalize individual edge lengths in FST structures
   - Code: `fst -> edges[j].len / max_edge_len`
   - **Problem**: FST edges array doesn't contain length data (all values were 0.000000)
   - This caused all normalized tree costs to be 0

2. **Inconsistent Data Access**:
   - Budget constraint read from: `cip -> full_trees[i] -> tree_len` (line 788)
   - Objective functions read from: `cip -> cost[i]` (lines 1159, 1555)
   - **Problem**: Normalization updated `cip->cost[i]` but budget read from `tree_len`

3. **Double Normalization Path**:
   - Had two branches: edge-level (if has_geometric_fsts) and tree-level (else)
   - Edge-level branch failed silently, producing all zeros

## Solution Implemented

### Changed to Tree-Level Normalization (constrnt.c lines 241-280):

```c
/* Always use tree-level normalization with bounding box diagonal */
fprintf(stderr, "DEBUG NORMALIZE: Using TREE-LEVEL normalization with BOUNDING BOX DIAGONAL\n");

/* Compute bounding box diagonal from terminal coordinates */
double min_x = DBL_MAX, max_x = -DBL_MAX;
double min_y = DBL_MAX, max_y = -DBL_MAX;

if (cip -> pts != NULL) {
    for (i = 0; i < cip -> pts -> n; i++) {
        double x = cip -> pts -> a[i].x;
        double y = cip -> pts -> a[i].y;
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
}

/* Compute diagonal: sqrt((max_x - min_x)^2 + (max_y - min_y)^2) */
double width = max_x - min_x;
double height = max_y - min_y;
double diagonal = sqrt(width * width + height * height);

/* Normalize tree costs by bounding box diagonal */
if (diagonal > 0.0) {
    for (i = 0; i < nedges; i++) {
        if (NOT BITON (edge_mask, i)) continue;
        double tree_cost = (double) (cip -> cost [i]);
        double normalized_cost = tree_cost / diagonal;
        cip -> cost[i] = (dist_t)normalized_cost;
    }
}
```

### Fixed Budget Constraint (constrnt.c line 788):
```c
/* Get normalized tree cost from cip->cost (same as objective function) */
double normalized_tree_cost = (double)(cip -> cost[i]);
```

## Key Improvements

1. **Consistent Data Access**: All components now read from `cip->cost[i]`
   - Budget constraint: `cip -> cost[i]`
   - CPLEX objective: `cip -> cost[i]` (line 1159)
   - lp_solve objective: `cip -> cost[i]` (line 1555)

2. **Geometric Scale**: Bounding box diagonal provides consistent geometric normalization
   - Formula: `diagonal = sqrt((max_x - min_x)² + (max_y - min_y)²)`
   - Example: For 20 terminals, diagonal ≈ 920,000 - 1,330,000 units
   - Normalized costs are typically in range [0.1, 0.7]

3. **Single Normalization**: Removed double normalization
   - Step 1: Normalize by diagonal ONLY
   - No second normalization in budget constraint or objective

4. **Simplified Logic**: Removed edge-level branch that didn't work
   - Always use tree-level normalization
   - Direct computation on `cip->cost[i]`

## Test Results

### Before Fix:
```
DEBUG NORMALIZE: FST 0 edge 0: 0.000000 / 923541.939064 = 0.000000
DEBUG BUDGET:   x[0] coefficient = 0 (normalized_tree_cost=0.000000)
...
Fatal software error at constrnt.c, line 1037
```

### After Fix:
```
DEBUG NORMALIZE: Using TREE-LEVEL normalization with BOUNDING BOX DIAGONAL
DEBUG NORMALIZE: Bounding box: (127064.000000,56589.000000) to (947403.000000,480822.000000)
DEBUG NORMALIZE: Width=820339.000000, Height=424233.000000, Diagonal=923541.939064
DEBUG NORMALIZE: FST 0: 535224.348181 / 923541.939064 = 0.579534
DEBUG BUDGET:   x[0] coefficient = 579534 (normalized_tree_cost=0.579534)
...
Solution completed successfully
```

## Files Modified

1. **constrnt.c**:
   - Lines 241-280: Replaced edge-level with tree-level normalization
   - Line 788: Changed from `full_trees[i]->tree_len` to `cip->cost[i]`

## Commit

```bash
git commit -m "Fix normalization: use tree-level with bounding box diagonal consistently"
```

## Status

✅ Crash fixed - budget coefficients are now non-zero
✅ Normalization consistent across all components
✅ Using geometric scale (bounding box diagonal)
✅ Single normalization pass (no double normalization)
✅ Test runs completing successfully

## Next Steps

- Verify budget constraint violations are fixed (should be ≤100% utilization)
- Check for oscillation issues (1 FST ↔ 19 FSTs)
- Analyze budget utilization across iterations
