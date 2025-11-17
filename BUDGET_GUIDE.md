# Budget Constraint Guide (UPDATED - Normalized Budget)

## ✅ Budget Now Uses Normalized Values!

### The Budget Constraint Formula:

```
Σ (tree_cost[i] / max_tree_cost) * x[i] ≤ budget_limit
```

Where:
- `tree_cost[i]` = raw Euclidean distance for FST i
- `max_tree_cost` = maximum tree cost among all FSTs
- Normalized cost = `tree_cost[i] / max_tree_cost` ∈ [0, 1]
- `x[i]` = binary variable (0 or 1) for FST selection
- `budget_limit` = your budget in **normalized units**

---

## Recommended Budget Values

### For 10 terminals:
- `BUDGET=3` - Very tight (covers 1-2 FSTs)
- `BUDGET=5` - Moderate (covers 3-5 FSTs) ✓ **RECOMMENDED**
- `BUDGET=8` - Loose (covers most terminals)

### For 20 terminals:
- `BUDGET=5` - Tight (covers ~3-5 FSTs)
- `BUDGET=10` - Moderate (covers ~5-8 FSTs) ✓ **DEFAULT**
- `BUDGET=15` - Loose (covers ~8-12 FSTs)

### For 30 terminals:
- `BUDGET=10` - Tight
- `BUDGET=15` - Moderate ✓ **RECOMMENDED**
- `BUDGET=20` - Loose

---

## Understanding Budget=10

**With 20 terminals and budget=10:**

Each FST contributes normalized tree cost in [0, 1] range:
- FST with max tree length: contributes 1.0
- FST with half max length: contributes 0.5
- FST with min tree length: contributes ~0.2

**Budget=10 means:**
- Total normalized tree cost ≤ 10
- Could select: 10 FSTs with cost=1.0 each
- Or: 20 FSTs with cost=0.5 each
- Or: Any combination where sum ≤ 10

**In practice** (20 terminals, budget=10):
- Solver might select 5-8 FSTs
- Covers approximately 8-12 terminals
- Some terminals may be uncovered (penalty applies)

---

## Comparison: Objective vs Budget

### Objective Function (what to minimize):
```
(tree_norm + alpha * battery_norm) + beta * not_covered
```
- FST costs: 0-2 per FST (tree + battery, both normalized)
- Not_covered penalty: 1000 per uncovered terminal
- **Typical objective**: 2-10 (if all covered) or 1000-5000 (if many uncovered)

### Budget Constraint (hard limit):
```
Σ tree_norm[i] * x[i] ≤ budget_limit
```
- **Only counts tree cost** (not battery, not penalties)
- Each FST: 0-1 normalized tree cost
- **Budget limit**: 5-20 for typical problems

---

## How to Run

### Quick Start (20 terminals, budget=10):
```bash
./run_optimization.sh
```

### Custom Budget Examples:
```bash
# Tight budget (few terminals covered)
./run_optimization.sh 20 5 10.0 5.0 10 tight-test

# Normal budget (moderate coverage)
./run_optimization.sh 20 10 10.0 5.0 10 normal-test

# Loose budget (most terminals covered)
./run_optimization.sh 20 15 10.0 5.0 10 loose-test

# Small network, tight budget
./run_optimization.sh 10 3 10.0 5.0 10 small-tight

# Large network, loose budget
./run_optimization.sh 30 20 15.0 5.0 10 large-loose
```

---

## Example Output Interpretation

### Debug Output:
```
DEBUG BUDGET: max_tree_cost = 645195.047
DEBUG BUDGET: Budget limit: 10.000 (normalized units)
DEBUG BUDGET:   x[0] coefficient = 641 (tree=413997.047, normalized=0.641662)
DEBUG BUDGET:   x[1] coefficient = 618 (tree=398991.100, normalized=0.618404)
DEBUG BUDGET:   x[4] coefficient = 1000 (tree=645195.047, normalized=1.000000)
```

**Interpretation:**
- FST 0 contributes 0.642 to budget if selected
- FST 1 contributes 0.618 to budget if selected
- FST 4 contributes 1.000 to budget if selected (it has max tree cost)
- Budget=10 allows ~5-8 FSTs to be selected (realistic expectation)

### Solution Output:
```
% Node 0 LP 1 Solution, length = 6.720614
```

**Interpretation:**
- Final objective value: 6.72
- Budget used: Some portion ≤ 10.0
- The solver selected FSTs with total normalized tree cost ≤ 10

---

## Current Settings Summary

- ✅ **Beta (uncovered penalty)**: 1000
- ✅ **Alpha (battery weight)**: 1.0  
- ✅ **Tree normalization**: Divided by max_tree_cost ✓
- ✅ **Battery normalization**: Divided by max_battery_sum ✓
- ✅ **Budget normalization**: Uses tree_cost / max_tree_cost ✓

**Everything is normalized and consistent!** 🎉

