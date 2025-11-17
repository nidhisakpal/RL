# Quick Start Guide - Battery-Aware Network Optimization

**Status**: ✅ Fully operational for research-sized problems
**Last Updated**: October 18, 2025

---

## Quick Test

```bash
# Verify everything works
./run_optimization.sh 4 1.8 10.0 5.0 3 3 1.0 test no
```

If this completes successfully, your system is working!

---

## Basic Usage

### Running Optimization

```bash
./run_optimization.sh [terminals] [budget] [charge] [demand] [iterations] [T] [gamma] [output_dir] [reuse]
```

**Parameters:**
- `terminals`: Number of network nodes (4-6 recommended)
- `budget`: Normalized budget constraint (0.5-3.0 typical)
- `charge`: Battery charge rate when covered (default: 10.0%)
- `demand`: Battery drain rate (default: 5.0%)
- `iterations`: Number of iterations (3-5 recommended)
- `T`: Time periods for multi-temporal optimization (1-3)
- `gamma`: Graph distance weight for topology stability (0-10.0)
- `output_dir`: Where to save results
- `reuse`: "yes" to reuse terminals, "no" for new random

### Examples

**Small test (recommended starting point):**
```bash
./run_optimization.sh 4 1.8 10.0 5.0 3 3 1.0 demo no
```

**Test battery dynamics:**
```bash
./run_optimization.sh 5 1.5 15.0 5.0 5 3 1.0 battery_test no
```

**Test graph distance effects:**
```bash
# Weak stability
./run_optimization.sh 4 1.8 10.0 5.0 3 3 0.5 weak_stable no

# Strong stability
./run_optimization.sh 4 1.8 10.0 5.0 3 3 10.0 strong_stable no
```

**Single period (faster, larger problems):**
```bash
./run_optimization.sh 8 2.0 10.0 5.0 3 1 0 large_single no
```

---

## Understanding Parameters

### Budget (Normalized)

- **0.5-1.0**: Very tight - forces selective coverage
- **1.5-2.0**: Moderate - good balance
- **2.5-3.0**: Loose - can cover most terminals

### Time Periods (T)

- **T=1**: Single-period optimization (fastest, handles larger problems)
- **T=2-3**: Multi-temporal planning (slower, more realistic)
- **T>3**: Not recommended (may hit constraints limits)

### Graph Distance Weight (γ)

- **γ=0**: No topology stability (maximally responsive to battery changes)
- **γ=0.5**: Weak stability preference
- **γ=1.0**: Balanced (recommended default)
- **γ=5.0-10.0**: Strong stability (network changes rarely)

### Charge/Demand Rates

- **Default**: charge=10.0%, demand=5.0% (net gain when covered: +5%)
- **Aggressive**: charge=15.0%, demand=5.0% (batteries recover quickly)
- **Conservative**: charge=8.0%, demand=5.0% (slower recovery)

---

## Viewing Results

After running, check:

```bash
# View battery evolution report
cat results/battery_evolution_report.txt

# Open visualization in browser
firefox results/visualization_iter1.html

# Compare iterations
diff results/solution_iter1.txt results/solution_iter2.txt
```

---

## Troubleshooting

### "Aborted (core dumped)"

**Problem**: Too many terminals or FSTs for multi-temporal mode

**Solutions:**
1. Reduce number of terminals (try 4-6)
2. Use T=1 instead of T=3
3. Reduce iterations to prevent FST growth
4. Increase budget to reduce FST enumeration

### "No such file: ./bb"

**Problem**: Not compiled

**Solution:**
```bash
make bb
```

### Slow performance

**Problem**: Too many FSTs being generated

**Solutions:**
1. Reduce number of terminals
2. Increase budget constraint
3. Use T=1 for faster solving

---

## Recommended Workflows

### Algorithm Development

```bash
# Start small
./run_optimization.sh 4 1.8 10.0 5.0 3 3 1.0 dev1 no

# Test different parameters
./run_optimization.sh 4 1.8 15.0 5.0 3 3 1.0 dev2 no   # Different charge rate
./run_optimization.sh 4 1.8 10.0 5.0 3 3 5.0 dev3 no   # Different gamma
```

### Parameter Tuning

```bash
# Fix terminals to compare parameters
./run_optimization.sh 5 1.5 10.0 5.0 3 3 1.0 baseline yes
./run_optimization.sh 5 1.5 10.0 5.0 3 3 5.0 stable yes    # Same terminals
./run_optimization.sh 5 1.5 10.0 5.0 3 3 0.5 dynamic yes   # Same terminals
```

### Performance Testing

```bash
# Single period (faster)
time ./run_optimization.sh 6 2.0 10.0 5.0 3 1 0 perf1 no

# Multi-temporal (slower but more realistic)
time ./run_optimization.sh 6 2.0 10.0 5.0 3 3 1.0 perf2 no
```

---

## Current Limitations

- **Max terminals**: 6-8 (for T=3), 8-10 (for T=1)
- **Max iterations**: 3-5 (FST count grows each iteration)
- **Max time periods**: T=3 recommended
- **Very large problems** (20+ terminals): Not yet supported

See [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for details.

---

## Features Reference

### Phase 1: Edge Enumeration ✅
- Global edge map
- Hash-based edge lookup
- Bidirectional FST-edge mapping

### Phase 2: Z Variables ✅
- Binary edge coverage variables
- Z[e] ∈ {0,1} for each unique edge
- Linking constraints: Z[e] ≤ Σ(x[i]: FST i contains e)

### Phase 3: Multi-Temporal ✅
- Time-indexed variables: x[i,t], Z[e,t]
- Environment: `GEOSTEINER_TIME_PERIODS=T`
- Planning horizon: T periods

### Phase 4: Battery State ✅
- Battery variables: b[j,t] ∈ [0,100]
- Battery-aware cost function
- Prioritizes low-battery terminals

### Phase 4.5: Battery Evolution ✅
- External iterative loop
- Battery updates between iterations
- Convergence checking

### Phase 5: Graph Distance ✅
- Topology stability penalty
- D[e,t] auxiliary variables
- Environment: `GRAPH_DISTANCE_WEIGHT=γ`
- Minimizes network reconfiguration

---

## Getting Help

1. **Check documentation**:
   - [PHASE5_COMPLETE.md](PHASE5_COMPLETE.md) - Phase 5 details
   - [KNOWN_ISSUES.md](KNOWN_ISSUES.md) - Current limitations
   - [BUG_FIX_SUMMARY.md](BUG_FIX_SUMMARY.md) - Recent fixes

2. **Verify compilation**:
   ```bash
   make clean
   make bb
   ```

3. **Test with minimal example**:
   ```bash
   ./rand_points 4 | ./efst | GEOSTEINER_BUDGET=1.8 ./bb
   ```

4. **Check environment variables**:
   ```bash
   env | grep GEOSTEINER
   ```

---

## Success Indicators

Your system is working correctly if you see:

```
✅ Optimization completed
✅ Visualization created: ...
✅ FST analysis created: ...
```

And in the output:
```
(Euclidean SMT:  X points,  length = -Y.ZZZZ,  A.BB seconds)
% @0 Euclidean SMT
```

---

**Ready to start? Run:**

```bash
./run_optimization.sh 4 1.8 10.0 5.0 3 3 1.0 my_first_test no
```

Good luck with your research! 🚀
