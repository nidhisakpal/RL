#!/bin/bash

# Battery-Aware Network Optimization - Automated Iterative Runner
# This script runs the complete iterative pipeline with battery dynamics

set -e  # Exit on any error

# Configuration
NUM_TERMINALS=${1:-20}           # Number of terminals (default: 20)
BUDGET=${2:-1.6 }                   # Budget constraint in NORMALIZED units (default: 1.6)
                                  # NOTE: Budget is normalized (tree costs divided by max)
                                  # Each FST contributes 0-1 normalized tree cost
                                  # With beta=0, budget controls coverage (alpha=10.0)
                                  # Typical range: 0.5 (very tight) to 3.0 (loose) for 20 terminals
CHARGE_RATE=${3:-10.0}           # Charge rate for connected terminals (default: 10.0%)
DEMAND_RATE=${4:-5.0}            # Demand rate for all terminals (default: 5.0%)
NUM_ITERATIONS=${5:-10}          # Number of iterations (default: 10)
OUTPUT_DIR=${6:-"results"}       # Output directory (default: results)
REUSE_TERMINALS=${7:-"no"}       # Reuse existing terminals? "yes" or "no" (default: no)

echo "🚀 Battery-Aware Network Optimization - Iterative Runner"
echo "=========================================================="
echo "Configuration:"
echo "  Terminals:    $NUM_TERMINALS"
echo "  Budget:       $BUDGET"
echo "  Charge rate:  $CHARGE_RATE%"
echo "  Demand rate:  $DEMAND_RATE%"
echo "  Iterations:   $NUM_ITERATIONS"
echo "  Output dir:   $OUTPUT_DIR"
echo "  Reuse terms:  $REUSE_TERMINALS"
echo "=========================================================="
echo ""

# Find next available results folder if OUTPUT_DIR already exists
# (but skip auto-increment if reusing terminals - work in same directory)
if [ -d "$OUTPUT_DIR" ] && [ "$REUSE_TERMINALS" != "yes" ]; then
    COUNTER=1
    BASE_DIR="$OUTPUT_DIR"
    while [ -d "${BASE_DIR}${COUNTER}" ]; do
        COUNTER=$((COUNTER + 1))
    done
    OUTPUT_DIR="${BASE_DIR}${COUNTER}"
    echo "📁 Directory exists, using new: $OUTPUT_DIR"
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"
if [ "$REUSE_TERMINALS" = "yes" ]; then
    echo "📁 Using directory: $OUTPUT_DIR (will reuse terminals if present)"
else
    echo "📁 Created output directory: $OUTPUT_DIR"
fi

# Generate or reuse initial terminals
if [ "$REUSE_TERMINALS" = "yes" ] && [ -f "$OUTPUT_DIR/terminals_iter1.txt" ]; then
    echo "♻️  Reusing existing terminals from $OUTPUT_DIR/terminals_iter1.txt"
    echo "   (To generate new terminals, delete the file or set REUSE_TERMINALS=no)"
else
    echo "🎲 Generating $NUM_TERMINALS random terminals..."
    ./rand_points  $NUM_TERMINALS > "$OUTPUT_DIR/terminals_iter1.txt"
    echo "✅ Generated new random terminals: $OUTPUT_DIR/terminals_iter1.txt"
fi

echo ""

# Function to run a single iteration
run_iteration() {
    local iter=$1
    local prev_iter=$2

    echo "🚀 ITERATION $iter"
    echo "==============="

    # Step 1: Update batteries based on previous solution (skip for iteration 1)
    if [ $iter -gt 1 ]; then
        echo "Step 1: Updating battery levels based on previous solution..."
        # Batteries are stored in percentage scale (0-100), so pass rates directly
        ./battery_wrapper \
            -i "$OUTPUT_DIR/terminals_iter${prev_iter}.txt" \
            -s "$OUTPUT_DIR/solution_iter${prev_iter}.txt" \
            -o "$OUTPUT_DIR/terminals_iter${iter}.txt" \
            -c $CHARGE_RATE \
            -d $DEMAND_RATE \
            > "$OUTPUT_DIR/battery_update_iter${iter}.log" 2>&1
        echo "✅ Battery levels updated (connected +${CHARGE_RATE}% charge -${DEMAND_RATE}% demand, disconnected -${DEMAND_RATE}% drain)"
    fi

    # Step 2: Generate FSTs
    echo "Step 2: Generating Full Steiner Trees..."
    ./efst < "$OUTPUT_DIR/terminals_iter${iter}.txt" > "$OUTPUT_DIR/fsts_iter${iter}.txt" 2>/dev/null
    fst_count=$(wc -l < "$OUTPUT_DIR/fsts_iter${iter}.txt")
    echo "✅ Generated $fst_count FSTs"

    # Step 2.5: Generate FST dump file for simulate.c
    echo "Step 2.5: Generating FST dump file..."
    ./dumpfst < "$OUTPUT_DIR/fsts_iter${iter}.txt" > "$OUTPUT_DIR/fsts_dump_iter${iter}.txt" 2>/dev/null
    echo "✅ Generated FST dump file"

    # Step 3: Solve optimization
    echo "Step 3: Solving budget-constrained optimization..."

    # Filter out verbose BB trace lines to reduce file size from 200MB to ~5MB
    # Keep: PostScript (% fs), statistics (% @0-@6), DEBUG output, LP_VARS, and SOLUTION COST BREAKDOWN
    # Remove: Branch-and-bound trace (% @LO, % Node, etc.), congested component debug
    ENABLE_MST_CORRECTION=1 GEOSTEINER_BUDGET=$BUDGET ./bb < "$OUTPUT_DIR/fsts_iter${iter}.txt" 2>&1 | \
        grep -Ev "^ % @(LO|LN|PL|PAP|NC|PMEM)|^% @(LO|LN)|^% Node |^% Resuming|^%  |^  % [^S]|^ %   Final|^ %     [0-9]|^ % suspending|^ % @cutset|^ %       [0-9]|^ % [0-9]+ fractional|^ % initially [0-9]+ congested|^ % _gst_find_congested|^ % \tcomponent [0-9]+" > "$OUTPUT_DIR/solution_iter${iter}.txt"

    # Check if solver found the problem infeasible
    if grep -q "problem is infeasible\|No feasible solutions found" "$OUTPUT_DIR/solution_iter${iter}.txt"; then
        echo "❌ ERROR: Problem is INFEASIBLE at iteration $iter"
        echo "   This usually means the budget is too tight or terminal configuration is unsolvable."
        echo "   Try: increasing budget, reducing terminals, or generating new random terminals"
        echo ""
        echo "INFEASIBLE: Budget too tight or unsolvable configuration" > "$OUTPUT_DIR/cost_summary_iter${iter}.txt"
        return 1
    fi

    # Extract cost breakdown to separate summary file
    grep -A 13 "SOLUTION COST BREAKDOWN" "$OUTPUT_DIR/solution_iter${iter}.txt" | \
        grep -E "Selected FSTs|Covered Terminals|Uncovered Terminals|Tree Length Cost|Battery Cost|Budget" | \
        sed 's/ *% *//g' > "$OUTPUT_DIR/cost_summary_iter${iter}.txt"

    # Check if cost summary is empty (solver didn't finish properly)
    if [ ! -s "$OUTPUT_DIR/cost_summary_iter${iter}.txt" ]; then
        echo "⚠️  WARNING: Solver completed but no cost summary found"
        echo "   Solution may be incomplete or solver output format changed"
    fi

    echo "✅ Optimization completed"
    echo "📊 Cost summary: $OUTPUT_DIR/cost_summary_iter${iter}.txt"

    # Step 3.5: Extract actual LP objective value from solution file
    # Format: LP_OBJECTIVE_VALUE: <value>
    objective_value=$(grep "^LP_OBJECTIVE_VALUE:" "$OUTPUT_DIR/solution_iter${iter}.txt" | awk '{print $2}')

    if [ -z "$objective_value" ]; then
        echo "⚠️  Warning: Could not extract LP objective value from solution file"
        objective_arg=""
    else
        echo "📊 LP Objective value: $objective_value"
        objective_arg="-z $objective_value"
    fi

    # Step 4: Compute topology distance from previous iteration
    if [ $iter -gt 1 ]; then
        echo "Step 4: Computing topology distance from iteration $prev_iter..."

        # Compute detailed topology distance: edge_count (edge_length)
        # Uses dumpfst output and terminals file to compute actual edge changes
        topo_dist=$(./compute_topo_dist_simple \
            "$OUTPUT_DIR/fsts_dump_iter${prev_iter}.txt" \
            "$OUTPUT_DIR/fsts_dump_iter${iter}.txt" \
            "$OUTPUT_DIR/solution_iter${prev_iter}.txt" \
            "$OUTPUT_DIR/solution_iter${iter}.txt" \
            "$OUTPUT_DIR/terminals_iter${iter}.txt" 2>/dev/null)

        if [ $? -eq 0 ]; then
            echo "  Topology distance: $topo_dist"
            echo "$topo_dist" > "$OUTPUT_DIR/topology_distance_iter${iter}.txt"
        else
            echo "  ⚠️  Warning: Failed to compute topology distance"
            topo_dist="0 (0.000)"
            echo "$topo_dist" > "$OUTPUT_DIR/topology_distance_iter${iter}.txt"
        fi
    else
        # First iteration - distance is 0
        topo_dist="0 (0.000)"
        echo "$topo_dist" > "$OUTPUT_DIR/topology_distance_iter${iter}.txt"
    fi

    # Step 5: Generate visualization
    echo "Step 5: Creating visualization..."
    # PSW: Pass the full FST file (not dumpfst output) to get Steiner point coordinates
    ./simulate \
        -t "$OUTPUT_DIR/terminals_iter${iter}.txt" \
        -f "$OUTPUT_DIR/fsts_iter${iter}.txt" \
        -r "$OUTPUT_DIR/solution_iter${iter}.txt" \
        -w "$OUTPUT_DIR/visualization_iter${iter}.html" \
        $objective_arg \
        -d "$topo_dist" \
        -v > "$OUTPUT_DIR/simulate_iter${iter}.log" 2>&1
    echo "✅ Visualization created: $OUTPUT_DIR/visualization_iter${iter}.html"

    # Step 6: Generate detailed FST analysis
    echo "Step 6: Creating detailed FST analysis..."
    python3 analyze_single_iteration.py $iter "$OUTPUT_DIR" > "$OUTPUT_DIR/analysis_iter${iter}.log" 2>&1
    echo "✅ FST analysis created: $OUTPUT_DIR/iter${iter}_analysis.txt"
    echo ""
}

# Run all iterations
for i in $(seq 1 $NUM_ITERATIONS); do
    prev_i=$((i-1))
    if [ $i -eq 1 ]; then
        run_iteration $i 0
    else
        run_iteration $i $prev_i
    fi
done

echo "📊 GENERATING COMPARISON REPORT"
echo "==============================="

# Create comparison report
cat > "$OUTPUT_DIR/battery_evolution_report.txt" << EOF
🔋 BATTERY EVOLUTION ACROSS $NUM_ITERATIONS ITERATIONS
==========================================

This report shows how battery levels evolve across iterations due to:
- Connected terminals: +${CHARGE_RATE}% charge, -${DEMAND_RATE}% demand = +$(echo "$CHARGE_RATE - $DEMAND_RATE" | bc)% net gain
- Disconnected terminals: 0% charge, -${DEMAND_RATE}% demand = -${DEMAND_RATE}% net loss
- Terminal 0 (source): Always maintained at 100%

EOF

echo "Terminal | Iter1 | Iter2 | Iter3 | Iter4 | Iter5 | Net Change | Trend" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "---------|-------|-------|-------|-------|-------|------------|-------" >> "$OUTPUT_DIR/battery_evolution_report.txt"

# Generate battery evolution table (only for first 5 iterations)
for i in $(seq 0 $((NUM_TERMINALS-1))); do
    iter1=$(sed -n "$((i+1))p" "$OUTPUT_DIR/terminals_iter1.txt" | awk '{print $3}')

    # Build values array for all available iterations (up to 5)
    values=($iter1)
    for j in {2..5}; do
        if [ $j -le $NUM_ITERATIONS ] && [ -f "$OUTPUT_DIR/terminals_iter${j}.txt" ]; then
            val=$(sed -n "$((i+1))p" "$OUTPUT_DIR/terminals_iter${j}.txt" | awk '{print $3}')
            values+=($val)
        else
            values+=("N/A")
        fi
    done

    # Calculate net change if we have at least 2 iterations
    if [ ${#values[@]} -ge 2 ] && [ "${values[-1]}" != "N/A" ]; then
        net_change=$(echo "${values[-1]} - ${values[0]}" | bc)
    else
        net_change="0.0"
    fi

    # Determine trend
    if (( $(echo "$net_change > 10" | bc -l) )); then
        trend="🔋 CHARGING"
    elif (( $(echo "$net_change < -5" | bc -l) )); then
        trend="⚡ DRAINING"
    else
        trend="➖ STABLE"
    fi

    # Print row (handle up to 5 iterations)
    printf "T%-8d| %5.1f | %5s | %5s | %5s | %5s | %+10.1f | %s\n" \
        $i ${values[0]} ${values[1]:-"N/A"} ${values[2]:-"N/A"} ${values[3]:-"N/A"} ${values[4]:-"N/A"} $net_change "$trend" >> "$OUTPUT_DIR/battery_evolution_report.txt"
done

echo "" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "📈 SUMMARY STATISTICS:" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "=====================" >> "$OUTPUT_DIR/battery_evolution_report.txt"

# Calculate coverage statistics for each iteration
for i in $(seq 1 $NUM_ITERATIONS); do
    if [ -f "$OUTPUT_DIR/battery_update_iter$((i+1)).log" ]; then
        coverage=$(grep "Coverage Status:" "$OUTPUT_DIR/battery_update_iter$((i+1)).log" | awk '{print $3}' || echo "Unknown")
    else
        coverage="Unknown"
    fi
    echo "Iteration $i: Coverage = $coverage" >> "$OUTPUT_DIR/battery_evolution_report.txt"
done

echo "" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "🔄 TOPOLOGY CHANGES:" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "====================" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "" >> "$OUTPUT_DIR/battery_evolution_report.txt"
printf "%-10s | %-20s\n" "Transition" "Topology Distance" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "-----------|---------------------" >> "$OUTPUT_DIR/battery_evolution_report.txt"

# Show topology distance for each iteration
for i in $(seq 1 $NUM_ITERATIONS); do
    if [ -f "$OUTPUT_DIR/topology_distance_iter${i}.txt" ]; then
        topo_dist=$(cat "$OUTPUT_DIR/topology_distance_iter${i}.txt")
        if [ $i -eq 1 ]; then
            printf "%-10s | %20s\n" "Iter 1" "0.0 (baseline)" >> "$OUTPUT_DIR/battery_evolution_report.txt"
        else
            prev_i=$((i-1))
            printf "%-10s | %20.1f\n" "$prev_i -> $i" "$topo_dist" >> "$OUTPUT_DIR/battery_evolution_report.txt"
        fi
    fi
done

echo "" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "Legend:" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "  - Topology Distance: Number of FST changes between iterations" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "  - Higher values = more restructuring of the network topology" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "  - Lower values = network topology remained more stable" >> "$OUTPUT_DIR/battery_evolution_report.txt"

echo "" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "📁 Generated Files:" >> "$OUTPUT_DIR/battery_evolution_report.txt"
echo "==================" >> "$OUTPUT_DIR/battery_evolution_report.txt"
ls -lah "$OUTPUT_DIR/"*.{txt,html} 2>/dev/null | tail -n +2 >> "$OUTPUT_DIR/battery_evolution_report.txt" || echo "No files found" >> "$OUTPUT_DIR/battery_evolution_report.txt"

echo "✅ Comparison report generated: $OUTPUT_DIR/battery_evolution_report.txt"

echo ""
echo "🎉 ALL $NUM_ITERATIONS ITERATIONS COMPLETED SUCCESSFULLY!"
echo "============================================="
echo ""
echo "📁 Results saved in: $OUTPUT_DIR/"
echo "📊 View report: cat $OUTPUT_DIR/battery_evolution_report.txt"
echo "🌐 Open visualizations:"
for i in $(seq 1 $NUM_ITERATIONS); do
    echo "   Iteration $i: $OUTPUT_DIR/visualization_iter${i}.html"
done
echo ""
echo "🔄 To run again:"
echo "   ./run_optimization.sh [terminals] [budget] [charge_rate] [demand_rate] [iterations] [output_dir] [reuse_terminals]"
echo ""
echo "Examples:"
echo "   # Default (20 terminals, budget=1.6, new random terminals)"
echo "   ./run_optimization.sh"
echo ""
echo "   # Tight budget"
echo "   ./run_optimization.sh 20 0.5 10.0 5.0 10 tight no"
echo ""
echo "   # Moderate budget with NEW random terminals each run"
echo "   ./run_optimization.sh 20 1.6 10.0 5.0 10 moderate no"
echo ""
echo "   # REUSE SAME terminals to compare algorithm improvements"
echo "   ./run_optimization.sh 20 1.6 10.0 5.0 10 moderate yes"
echo ""
echo "   # Run again on existing directory (reuse terminals)"
echo "   ./run_optimization.sh 20 2.0 10.0 5.0 10 moderate yes"
echo ""
echo "NOTE:"
echo "  - Budget is NORMALIZED (0.5-3.0 typical for 20 terminals)"
echo "  - Set reuse_terminals=yes to test on SAME terminal set"
echo "  - Battery objective now INVERTED: low battery = high priority"
echo "  - MST correction uses pre-computation (adjusts FST costs directly)"
