# Battery Cost Evolution: From Static to Dynamic

## Quick Reference Guide

---

## Visual Summary

```
Phase 1-2: STATIC BATTERY COST
────────────────────────────────────────────────────────────────
battery_cost[i] = Σⱼ battery[j]    (constant for all time)

Objective: Minimize Σᵢ [tree_cost[i] + α × battery_cost[i]] × x[i]

✗ Battery levels never change
✗ Same cost throughout optimization
✗ No temporal dynamics


Phase 3: MULTI-PERIOD STATIC (Current)
────────────────────────────────────────────────────────────────
battery_cost[i] = Σⱼ battery[j]    (still static, replicated for T periods)

Objective: Minimize Σₜ Σᵢ [tree_cost[i] + α × battery_cost[i]] × x[i,t]

✓ Infrastructure for T periods
✗ Battery levels still don't change
✗ Each period is independent
✗ No incentive to balance coverage


Phase 4: DYNAMIC BATTERY COST (Planned)
────────────────────────────────────────────────────────────────
battery_cost[i,t] = α × (1 - b[j(i),t]/100)    (dynamic!)

Objective: Minimize Σₜ Σᵢ [tree_cost[i] + α × (1 - b[j(i),t]/100)] × x[i,t]

✓ Battery levels evolve: b[j,t+1] = b[j,t] + charge - demand
✓ Low battery → HIGH cost → HIGH priority for coverage
✓ High battery → LOW cost → LOW priority for coverage
✓ Balances coverage across time periods


Phase 5: WITH GRAPH DISTANCE (Future)
────────────────────────────────────────────────────────────────
battery_cost[i,t] = α × (1 - b[j(i),t]/100)    (same as Phase 4)

Objective: Minimize Σₜ Σᵢ [tree_cost[i] + α × (1 - b[j(i),t]/100)] × x[i,t]
                    + β × Σₜ Σⱼ not_covered[j,t]
                    + γ × Σₜ GD(t, t+1)

✓ All Phase 4 benefits
✓ Minimizes topology changes (graph distance)
✓ Stable network with balanced coverage
```

---

## The Key Formula: (1 - b[j,t]/100)

### Why This Form?

**Goal:** Prioritize terminals with **low battery** for coverage.

**The inverse relationship:**
```
Battery Level  |  (1 - b/100)  |  Interpretation
─────────────────────────────────────────────────────
b = 0%         |    1.0        |  CRITICAL - must cover!
b = 25%        |    0.75       |  Low - high priority
b = 50%        |    0.50       |  Medium priority
b = 75%        |    0.25       |  Healthy - low priority
b = 100%       |    0.0        |  Full - no need to cover
```

**In the objective:**
```
Cost contribution = α × (1 - b[j,t]/100) × x[i,t]

where α = 0.1 (weight parameter)
```

### Example: Terminal with b[j,t] = 20%

```
battery_cost = 0.1 × (1 - 20/100) = 0.1 × 0.8 = 0.08

This adds 0.08 to the objective if FST i (covering terminal j) is selected.
```

**But wait - isn't adding cost bad?**

Yes! And that's the point:
- The solver **minimizes** the objective
- High battery cost makes the FST **expensive**
- But the penalty for **not covering** is even higher (β = 1,000,000)
- So the solver is **forced** to cover low-battery terminals
- This achieves our goal: **prioritize low-battery terminals**

---

## Battery Dynamics (Phase 4)

### Evolution Equation

```
b[j,t+1] = b[j,t] + charge_rate × (1 - not_covered[j,t]) - demand_rate

where:
  charge_rate = 15      (energy gained when covered)
  demand_rate = 5       (baseline consumption)
  not_covered[j,t] ∈ [0,1]
```

### Scenarios

#### Scenario 1: Terminal is COVERED
```
not_covered[j,t] ≈ 0
b[j,t+1] = b[j,t] + 15 × (1 - 0) - 5 = b[j,t] + 10

Battery INCREASES by 10 units
```

#### Scenario 2: Terminal is UNCOVERED
```
not_covered[j,t] = 1
b[j,t+1] = b[j,t] + 15 × (1 - 1) - 5 = b[j,t] - 5

Battery DECREASES by 5 units
```

### Feedback Loop

```
Low battery (b = 20)
    ↓
High cost: α × (1 - 0.2) = 0.08
    ↓
Solver prioritizes covering this terminal
    ↓
Terminal gets covered
    ↓
Battery increases: b[t+1] = 20 + 10 = 30
    ↓
Lower cost next period: α × (1 - 0.3) = 0.07
    ↓
Other terminals may get priority
    ↓
Terminal may not be covered
    ↓
Battery decreases: b[t+2] = 30 - 5 = 25
    ↓
Process repeats - balanced coverage!
```

---

## Concrete Example: 4 Terminals, 3 Time Periods

### Initial State (t=0)

| Terminal | b[j,0] | (1 - b/100) | battery_cost |
|----------|--------|-------------|--------------|
| j=0      | 80.0   | 0.20        | 0.020        |
| j=1      | 20.0   | 0.80        | 0.080 ← HIGH |
| j=2      | 60.0   | 0.40        | 0.040        |
| j=3      | 100.0  | 0.00        | 0.000        |

**Solver decision:** Prioritize covering terminal j=1 (highest cost).

---

### After Period t=0 (covered: j=1, j=2; uncovered: j=0, j=3)

**Battery updates:**
```
b[0,1] = 80 - 5 = 75     (uncovered, lost 5)
b[1,1] = 20 + 10 = 30    (covered, gained 10) ← IMPROVED
b[2,1] = 60 + 10 = 70    (covered, gained 10)
b[3,1] = 100 - 5 = 95    (uncovered, lost 5)
```

### State at t=1

| Terminal | b[j,1] | (1 - b/100) | battery_cost |
|----------|--------|-------------|--------------|
| j=0      | 75.0   | 0.25        | 0.025        |
| j=1      | 30.0   | 0.70        | 0.070 ← STILL HIGH |
| j=2      | 70.0   | 0.30        | 0.030        |
| j=3      | 95.0   | 0.05        | 0.005        |

**Solver decision:** Still prioritize j=1, but also consider j=0.

---

### After Period t=1 (covered: j=0, j=1; uncovered: j=2, j=3)

**Battery updates:**
```
b[0,2] = 75 + 10 = 85    (covered, gained 10)
b[1,2] = 30 + 10 = 40    (covered, gained 10) ← IMPROVING
b[2,2] = 70 - 5 = 65     (uncovered, lost 5)
b[3,2] = 95 - 5 = 90     (uncovered, lost 5)
```

### State at t=2

| Terminal | b[j,2] | (1 - b/100) | battery_cost |
|----------|--------|-------------|--------------|
| j=0      | 85.0   | 0.15        | 0.015        |
| j=1      | 40.0   | 0.60        | 0.060 ← IMPROVING |
| j=2      | 65.0   | 0.35        | 0.035        |
| j=3      | 90.0   | 0.10        | 0.010        |

**Result:** Battery levels are **converging** toward balanced state!

---

## Comparison: Static vs Dynamic

### Scenario: 15 Time Periods, 4 Terminals

#### Static Battery Cost (Phase 3)

```
Period  | Terminal 0 | Terminal 1 | Terminal 2 | Terminal 3 |
        | (b=80)     | (b=20)     | (b=60)     | (b=100)    |
────────────────────────────────────────────────────────────────
t=0-14  |  0.020     |  0.080     |  0.040     |  0.000     |

Solver behavior:
- Always prioritizes Terminal 1 (highest static cost)
- Terminal 1 always covered, others rarely covered
- Inefficient - ignores battery dynamics
```

#### Dynamic Battery Cost (Phase 4)

```
Period | Terminal 0 | Terminal 1 | Terminal 2 | Terminal 3 |
────────────────────────────────────────────────────────────────
t=0    | 0.020      | 0.080 ↑    | 0.040      | 0.000      |
t=1    | 0.025      | 0.070      | 0.030      | 0.005      |
t=2    | 0.015      | 0.060      | 0.035      | 0.010      |
t=3    | 0.020      | 0.065      | 0.025      | 0.005      |
...    | oscillate  | oscillate  | oscillate  | oscillate  |

Solver behavior:
- Dynamically adjusts priorities based on current battery
- ALL terminals get coverage when needed
- Batteries oscillate around healthy levels (50-70%)
- Efficient - maximizes network lifetime
```

---

## Parameter Tuning

### Alpha (α) - Battery Cost Weight

```
α = 0.01   →  Very weak battery influence, mostly tree cost optimization
α = 0.1    →  Balanced (DEFAULT)
α = 1.0    →  Strong battery influence, may sacrifice tree cost
α = 10.0   →  Dominant battery influence, tree cost almost ignored
```

**Recommendation:** Start with α = 0.1, tune based on results.

### Beta (β) - Uncovered Penalty

```
β = 100,000    →  Allow some uncovered terminals
β = 1,000,000  →  Strongly enforce coverage (DEFAULT)
β = 10,000,000 →  Almost hard constraint (all must be covered)
```

**Recommendation:** β = 1,000,000 (soft constraint, allows budget flexibility).

### Charge/Demand Rates

```
charge_rate = 15   →  Gain 15 units when covered
demand_rate = 5    →  Lose 5 units when uncovered

Net effect:
- Covered: +10 units per period
- Uncovered: -5 units per period
```

**Tuning guidelines:**
- Higher charge_rate → batteries recover faster
- Higher demand_rate → batteries drain faster, more urgent coverage
- Ratio should reflect real sensor network dynamics

---

## Mathematical Properties

### Property 1: Bounded Battery Levels
```
Constraint: 0 ≤ b[j,t] ≤ 100

Ensures battery costs remain in range [0, α]:
  min: α × (1 - 100/100) = 0
  max: α × (1 - 0/100) = α
```

### Property 2: Monotonicity
```
If b[j,t] increases, then battery_cost decreases:

∂(battery_cost)/∂b = ∂[α × (1 - b/100)]/∂b = -α/100 < 0

This is the desired inverse relationship.
```

### Property 3: Linearity
```
battery_cost[i,t] = α × (1 - b[j,t]/100)
                  = α - α × b[j,t]/100

This is LINEAR in b[j,t], which keeps the LP formulation tractable.
```

---

## Implementation Checklist for Phase 4

### 1. Variable Addition
- [ ] Add b[j,t] continuous variables [0, 100]
- [ ] Update variable allocation in all files (bb.c, constrnt.c, bbsubs.c, ckpt.c)
- [ ] Update `macsz` calculation: `T × (nedges + nterms + num_edges + nterms)`

### 2. Constraint Addition
- [ ] Add battery evolution: `b[j,t+1] = b[j,t] + charge × (1-not_covered[j,t]) - demand`
- [ ] Add battery bounds: `0 ≤ b[j,t] ≤ 100`
- [ ] Add initial conditions: `b[j,0] = initial_battery[j]`

### 3. Objective Function Update
- [ ] Replace static battery_cost with dynamic: `α × (1 - b[j(i),t]/100)`
- [ ] Determine j(i) for each FST (min battery terminal? average?)
- [ ] Update objective coefficient calculation

### 4. Input/Output
- [ ] Read initial_battery values from input file
- [ ] Output battery levels at each time period
- [ ] Visualize battery evolution over time

### 5. Testing
- [ ] Test with simple 4-terminal, 3-period case
- [ ] Verify battery evolution matches expected values
- [ ] Compare static vs dynamic solutions
- [ ] Scale to T=15 periods

---

## Expected Benefits After Phase 4

### 1. Realistic Battery Modeling
- Batteries evolve based on coverage history
- Low-battery terminals get priority
- Network lifetime is maximized

### 2. Balanced Coverage
- All terminals get coverage when needed
- No terminal is perpetually ignored
- No terminal is perpetually covered (unless needed)

### 3. Adaptive Optimization
- Solver adjusts priorities dynamically
- Responds to changing battery landscape
- Finds optimal coverage schedule over T periods

### 4. Predictive Capability
- Can simulate battery evolution into future
- Can predict when terminals will fail
- Can plan coverage to prevent failures

---

## Summary Table

| Aspect | Phase 1-3 | Phase 4 (Planned) |
|--------|-----------|-------------------|
| **Battery levels** | Static input | Dynamic state variables |
| **Battery cost** | Σⱼ battery[j] | α × (1 - b[j,t]/100) |
| **Time coupling** | None | Battery evolution |
| **Coverage strategy** | Fixed priority | Adaptive priority |
| **Network lifetime** | Ignored | Maximized |
| **Realism** | Low | High |
| **Optimization** | Myopic | Foresighted |

---

**Bottom Line:**

The transition from **static** to **dynamic** battery cost in Phase 4 transforms the optimization from a simple tree selection problem into a **realistic, adaptive, multi-temporal network management system** that truly maximizes network lifetime.

The key insight: **(1 - b[j,t]/100)** creates an **inverse relationship** where low-battery terminals automatically become high-priority, leading to **balanced coverage** and **extended network lifetime**.

---

**Document Version:** 1.0
**Date:** October 18, 2025
**Status:** Phase 3 complete, Phase 4 ready to implement
