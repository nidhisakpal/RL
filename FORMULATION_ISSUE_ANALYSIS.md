# Battery-Aware Steiner Tree: Formulation Analysis

## Problem Discovered

The optimization produces dramatically different coverage at different alpha values, even though β=0 (no explicit coverage penalty):

- **Alpha = 10:** Coverage = 13/20 terminals (65%)
- **Alpha = 0.004:** Coverage = 2/20 terminals (10%)

## Root Cause Analysis

### 1. Objective Function Sign Change

**Objective per FST:** `tree_cost + Σ[alpha × (-1 + battery_k/100)]`

**At HIGH alpha (10):**
- Battery term dominates: -5.00 (negative!)
- Total objective: 0.1 - 5.0 = -4.9 (NEGATIVE)
- Adding FSTs REDUCES objective → solver wants MORE FSTs

**At LOW alpha (0.004):**
- Tree cost dominates: 0.1
- Battery term negligible: -0.002
- Total objective: 0.1 - 0.002 = +0.098 (POSITIVE)
- Adding FSTs INCREASES objective → solver wants FEWER FSTs

### 2. Budget Constraint is Upper Limit Only

**Constraint:** `Σ tree_cost[i] × x[i] ≤ budget_limit`

- Uses ≤ (not =), so solver can use LESS than budget
- No requirement to spend available budget
- No minimum coverage requirement

**Location:** [constrnt.c:752-786](constrnt.c#L752-L786)

### 3. Spanning Constraint Doesn't Enforce Connectivity

**Constraint:** `Σ(|FST|-1)×x + Σnot_covered ≥ num_terminals - 1`

- Ensures spanning tree STRUCTURE for covered terminals
- But allows ANY number of terminals to remain uncovered
- Satisfied even with 1 FST and 18 uncovered terminals

**Location:** [constrnt.c:456-490](constrnt.c#L456-L490)

### 4. β=0 Means No Coverage Penalty

- Soft cutset constraints allow `not_covered[j] = 1`
- But β=0 means zero penalty in objective function
- Solver can leave terminals uncovered at no cost

**Location:** [solver.c:1328](solver.c#L1328)

## Fundamental Flaw

The formulation **ACCIDENTALLY** relies on negative battery bonuses at high alpha to drive terminal coverage, rather than having an explicit coverage requirement.

This creates an inverted incentive structure:
- **High alpha** → Negative costs → More FSTs → Better coverage (unintended!)
- **Low alpha** → Positive costs → Fewer FSTs → Minimal coverage (broken!)

## Mathematical Standoff

Cannot add hard minimum coverage constraint because it would conflict with hard budget constraint, potentially making problem infeasible.

**Current state:**
```
✓ Budget (hard):   Σ tree_cost × x ≤ budget_limit
✓ Coverage (soft): Allows uncovered terminals with β=0 penalty

→ Result: Coverage depends on alpha value, not on network requirements!
```

## Evidence

### High Alpha Behavior (α=10)
```
Selected FSTs:           11
Covered Terminals:       13 / 20 (65.0%)
Budget Used:             1.256850 / 1.600000 (78.55%)
```

### Low Alpha Behavior (α=0.004)
```
Selected FSTs:           1
Covered Terminals:       2 / 20 (10%)
Budget Used:             ~0.1 / 1.600000 (~6%)
```

## Potential Solutions

1. **Set β > 0** to create soft penalty for uncovered terminals
   - Balances budget constraint vs coverage preference
   - Maintains feasibility

2. **Add minimum coverage constraint** with budget slack
   - Requires reformulation to handle infeasibility cases

3. **Reformulate objective** to separate coverage from battery optimization
   - Two-phase approach: coverage first, then battery optimization

4. **Add connectivity constraints** that enforce minimum spanning structure
   - More complex but theoretically sound

## Key Files

- [solver.c:1326-1379](solver.c#L1326-L1379) - Objective function computation
- [constrnt.c:752-786](constrnt.c#L752-L786) - Budget constraint
- [constrnt.c:456-490](constrnt.c#L456-L490) - Spanning constraint
- [constrnt.c:531-610](constrnt.c#L531-L610) - Soft cutset constraints
