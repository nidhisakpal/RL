#!/usr/bin/env python3
"""
Analyze a single iteration and create detailed FST analysis
Usage: python3 analyze_single_iteration.py <iteration_number> <results_directory>
"""

import os
import re
import sys
from pathlib import Path

def extract_selected_fsts(solution_file):
    """Extract which FSTs were selected based on LP variable values x[i]=1"""
    selected_fsts = []
    try:
        with open(solution_file, 'r') as f:
            content = f.read()

        # Look for LP variable assignments: "DEBUG LP_VARS: x[i] = 1.000000"
        # Pattern matches x[i] = 1.0 (selected FSTs)
        fst_pattern = r'DEBUG LP_VARS: x\[(\d+)\] = 1\.0+\s+\(FST \d+\)'
        matches = re.findall(fst_pattern, content)

        for match in matches:
            selected_fsts.append(int(match))

    except Exception as e:
        print(f"Error reading {solution_file}: {e}")

    return selected_fsts

def extract_fst_details(fsts_dump_file):
    """Extract FST details from dump file"""
    fst_details = []
    try:
        with open(fsts_dump_file, 'r') as f:
            lines = f.readlines()

        for i, line in enumerate(lines):
            terminals = [int(x) for x in line.strip().split()]
            fst_details.append({
                'id': i,
                'terminals': terminals,
                'terminal_count': len(terminals)
            })

    except Exception as e:
        print(f"Error reading {fsts_dump_file}: {e}")

    return fst_details

def extract_fst_costs(solution_file):
    """Extract normalized tree and battery costs for each FST from solution file"""
    fst_costs = {}
    try:
        with open(solution_file, 'r') as f:
            content = f.read()

        # Pattern: OBJ[0]: tree=0.407 (scaled=6.912), battery_sum_cost=-0.672000, obj=6.240114
        obj_pattern = r'OBJ\[(\d+)\]:\s+tree=([\d.]+)\s+\(scaled=([\d.]+)\),\s+battery_sum_cost=([-\d.]+),\s+obj=([\d.]+)'
        matches = re.findall(obj_pattern, content)

        for match in matches:
            fst_id = int(match[0])
            tree_raw = float(match[1])
            tree_scaled = float(match[2])
            battery_cost = float(match[3])
            obj_value = float(match[4])

            fst_costs[fst_id] = {
                'tree_raw': tree_raw,
                'tree_scaled': tree_scaled,
                'battery_cost': battery_cost,
                'objective': obj_value
            }

    except Exception as e:
        print(f"Error extracting FST costs from {solution_file}: {e}")

    return fst_costs

def extract_budget_info(solution_file):
    """Extract budget information from solution file"""
    budget_info = {}
    try:
        with open(solution_file, 'r') as f:
            content = f.read()
            
        # Extract budget limit
        budget_match = re.search(r'DEBUG BUDGET: Budget limit: ([\d.]+)', content)
        if budget_match:
            budget_info['budget_limit'] = float(budget_match.group(1))
            
        # Extract max tree cost
        max_tree_match = re.search(r'DEBUG BUDGET: max_tree_cost = ([\d.]+)', content)
        if max_tree_match:
            budget_info['max_tree_cost'] = float(max_tree_match.group(1))
            
        # Extract scale factor
        constraint_match = re.search(r'DEBUG BUDGET: Constraint: Σ \(norm_cost \* (\d+)\) \* x\[i\] ≤ (\d+)', content)
        if constraint_match:
            budget_info['scale_factor'] = int(constraint_match.group(1))
            budget_info['budget_rhs'] = int(constraint_match.group(2))
            
    except Exception as e:
        print(f"Error reading {solution_file}: {e}")
        
    return budget_info

def extract_visualization_info(viz_file):
    """Extract information from visualization HTML file"""
    viz_info = {}
    try:
        with open(viz_file, 'r') as f:
            content = f.read()
            
        # Extract MIP Gap
        mip_gap_match = re.search(r'<strong>MIP Gap:</strong></td><td>([\d.]+)% \(([\d.]+)\)</td>', content)
        if mip_gap_match:
            viz_info['mip_gap_percent'] = float(mip_gap_match.group(1))
            viz_info['mip_gap_decimal'] = float(mip_gap_match.group(2))
            
        # Extract Total Cost
        total_cost_match = re.search(r'<strong>Total Cost:</strong></td><td>([\d.]+)</td>', content)
        if total_cost_match:
            viz_info['total_cost'] = float(total_cost_match.group(1))
            
        # Extract Coverage info
        coverage_match = re.search(r'<strong>Covered Terminals:</strong></td><td>(\d+)</td>', content)
        if coverage_match:
            viz_info['covered_terminals'] = int(coverage_match.group(1))
            
        uncovered_match = re.search(r'<strong>Uncovered Terminals:</strong></td><td>(\d+)</td>', content)
        if uncovered_match:
            viz_info['uncovered_terminals'] = int(uncovered_match.group(1))
            
        coverage_rate_match = re.search(r'<strong>Coverage Rate:</strong></td><td>([\d.]+)%</td>', content)
        if coverage_rate_match:
            viz_info['coverage_rate'] = float(coverage_rate_match.group(1))
            
        # Extract FST selection info
        fst_match = re.search(r'<strong>Selected FSTs:</strong></td><td>(\d+) of (\d+)</td>', content)
        if fst_match:
            viz_info['selected_fsts'] = int(fst_match.group(1))
            viz_info['total_fsts'] = int(fst_match.group(2))
            
    except Exception as e:
        print(f"Error reading {viz_file}: {e}")
        
    return viz_info

def extract_terminal_info(terminals_file):
    """Extract battery information from terminals file"""
    terminal_info = []
    try:
        with open(terminals_file, 'r') as f:
            lines = f.readlines()
            
        for i, line in enumerate(lines):
            parts = line.strip().split()
            if len(parts) >= 3:
                terminal_info.append({
                    'id': i,
                    'x': float(parts[0]),
                    'y': float(parts[1]),
                    'battery': float(parts[2])
                })
                
    except Exception as e:
        print(f"Error reading {terminals_file}: {e}")
        
    return terminal_info

def analyze_single_iteration(iter_num, results_dir):
    """Analyze a single iteration and create detailed report"""
    
    # File paths
    solution_file = f"{results_dir}/solution_iter{iter_num}.txt"
    viz_file = f"{results_dir}/visualization_iter{iter_num}.html"
    terminals_file = f"{results_dir}/terminals_iter{iter_num}.txt"
    fsts_dump_file = f"{results_dir}/fsts_dump_iter{iter_num}.txt"
    
    # Check if files exist
    if not os.path.exists(solution_file):
        print(f"Error: {solution_file} not found")
        return
    
    # Extract data
    selected_fsts = extract_selected_fsts(solution_file)
    fst_details = extract_fst_details(fsts_dump_file)
    fst_costs = extract_fst_costs(solution_file)
    budget_info = extract_budget_info(solution_file)
    viz_info = extract_visualization_info(viz_file)
    terminal_info = extract_terminal_info(terminals_file)
    
    # Create output file
    output_file = f"{results_dir}/iter{iter_num}_analysis.txt"
    
    with open(output_file, 'w') as f:
        f.write(f"🔋 ITERATION {iter_num} - DETAILED FST ANALYSIS\n")
        f.write("=" * 60 + "\n\n")
        
        # Summary information
        f.write("📊 SUMMARY\n")
        f.write("-" * 20 + "\n")
        f.write(f"Budget Limit: {budget_info.get('budget_limit', 'N/A')}\n")
        f.write(f"Scale Factor: {budget_info.get('scale_factor', 'N/A')}\n")
        f.write(f"Max Tree Cost: {budget_info.get('max_tree_cost', 'N/A'):,.0f}\n" if isinstance(budget_info.get('max_tree_cost'), (int, float)) else f"Max Tree Cost: {budget_info.get('max_tree_cost', 'N/A')}\n")
        f.write(f"MIP Gap: {viz_info.get('mip_gap_percent', 'N/A')}%\n")
        f.write(f"Total Cost: {viz_info.get('total_cost', 'N/A')}\n")
        f.write(f"Coverage Rate: {viz_info.get('coverage_rate', 'N/A')}%\n")
        f.write(f"Selected FSTs: {viz_info.get('selected_fsts', 'N/A')}/{viz_info.get('total_fsts', 'N/A')}\n")
        f.write(f"Covered Terminals: {viz_info.get('covered_terminals', 'N/A')}\n")
        f.write(f"Uncovered Terminals: {viz_info.get('uncovered_terminals', 'N/A')}\n\n")
        
        # Terminal information
        f.write("🔋 TERMINAL BATTERY LEVELS\n")
        f.write("-" * 30 + "\n")
        for term in terminal_info:
            battery_status = "🔴 LOW" if term['battery'] < 20 else "🟡 MEDIUM" if term['battery'] < 50 else "🟢 HIGH"
            f.write(f"Terminal {term['id']:2d}: {term['battery']:6.1f}% {battery_status}\n")
        f.write("\n")
        
        # FST detailed analysis
        f.write("🌳 FST DETAILED ANALYSIS\n")
        f.write("-" * 105 + "\n")
        f.write(f"{'FST':<4} {'Selected':<10} {'Terminals':<20} {'Count':<6} {'Norm Tree':<12} {'Norm Battery':<14} {'Objective':<12}\n")
        f.write("-" * 105 + "\n")

        for fst_detail in fst_details:
            fst_id = fst_detail['id']
            terminals_str = ','.join(map(str, fst_detail['terminals']))
            is_selected = "✅ YES" if fst_id in selected_fsts else "❌ NO"

            # Get cost information if available
            if fst_id in fst_costs:
                cost = fst_costs[fst_id]
                norm_tree = f"{cost['tree_scaled']:8.3f}"
                norm_battery = f"{cost['battery_cost']:9.3f}"
                objective = f"{cost['objective']:8.3f}"
            else:
                norm_tree = "N/A"
                norm_battery = "N/A"
                objective = "N/A"

            f.write(f"{fst_id:<4} {is_selected:<10} {terminals_str:<20} {fst_detail['terminal_count']:<6} {norm_tree:<12} {norm_battery:<14} {objective:<12}\n")

        f.write("\n")
        
        # Selected FSTs analysis
        f.write("✅ SELECTED FSTs ANALYSIS\n")
        f.write("-" * 30 + "\n")
        
        for fst_id in selected_fsts:
            fst_detail = next((d for d in fst_details if d['id'] == fst_id), None)

            if fst_detail:
                terminals_str = ','.join(map(str, fst_detail['terminals']))
                f.write(f"FST {fst_id}: Terminals [{terminals_str}] ({fst_detail['terminal_count']} terminals)\n")

                # Show cost information
                if fst_id in fst_costs:
                    cost = fst_costs[fst_id]
                    f.write(f"  Normalized Tree Cost:    {cost['tree_scaled']:8.3f}\n")
                    f.write(f"  Normalized Battery Cost: {cost['battery_cost']:8.3f}\n")
                    f.write(f"  Combined Objective:      {cost['objective']:8.3f}\n")

                # Show battery levels of terminals in this FST
                f.write(f"  Terminal battery levels:\n")
                for term_id in fst_detail['terminals']:
                    term_info = next((t for t in terminal_info if t['id'] == term_id), None)
                    if term_info:
                        battery_status = "🔴 LOW" if term_info['battery'] < 20 else "🟡 MEDIUM" if term_info['battery'] < 50 else "🟢 HIGH"
                        f.write(f"    Terminal {term_id}: {term_info['battery']:6.1f}% {battery_status}\n")
                f.write("\n")
        
        # Budget constraint analysis
        f.write(f"\n💰 BUDGET CONSTRAINT ANALYSIS\n")
        f.write("-" * 35 + "\n")
        f.write(f"Formula: Σ (normalized_tree_cost × {budget_info.get('scale_factor', 'N/A')}) × x[i] ≤ {budget_info.get('budget_limit', 'N/A')}\n")
        f.write(f"Budget limit: {budget_info.get('budget_limit', 'N/A')}\n")
        f.write(f"Scale factor: {budget_info.get('scale_factor', 'N/A')}\n")
    
    print(f"✅ Analysis saved to: {output_file}")

def main():
    """Main function"""
    if len(sys.argv) != 3:
        print("Usage: python3 analyze_single_iteration.py <iteration_number> <results_directory>")
        print("Example: python3 analyze_single_iteration.py 1 results3")
        sys.exit(1)
    
    iter_num = int(sys.argv[1])
    results_dir = sys.argv[2]
    
    if not os.path.exists(results_dir):
        print(f"Error: Directory '{results_dir}' does not exist")
        sys.exit(1)
    
    try:
        analyze_single_iteration(iter_num, results_dir)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()