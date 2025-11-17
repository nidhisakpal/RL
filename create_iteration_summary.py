#!/usr/bin/env python3
"""
Create a comprehensive summary table of all iterations with normalized costs, budget, MIP gap, etc.
"""

import os
import re
import glob
from pathlib import Path

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
            
        # Extract constraint formula
        constraint_match = re.search(r'DEBUG BUDGET: Constraint: Σ \(norm_cost \* (\d+)\) \* x\[i\] ≤ (\d+)', content)
        if constraint_match:
            budget_info['scale_factor'] = int(constraint_match.group(1))
            budget_info['budget_rhs'] = int(constraint_match.group(2))
            
        # Extract alpha and beta values from debug output
        alpha_match = re.search(r'alpha = ([\d.]+)', content)
        if alpha_match:
            budget_info['alpha'] = float(alpha_match.group(1))
            
        beta_match = re.search(r'beta = ([\d.]+)', content)
        if beta_match:
            budget_info['beta'] = float(beta_match.group(1))
            
        # Extract normalization info
        norm_match = re.search(r'DEBUG NORMALIZATION: max_tree_cost=([\d.]+), max_battery_cost=([\d.]+)', content)
        if norm_match:
            budget_info['max_tree_cost_norm'] = float(norm_match.group(1))
            budget_info['max_battery_cost_norm'] = float(norm_match.group(2))
            
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
            
        # Extract budget constraint info
        budget_constraint_match = re.search(r'Tree costs \(([\d,]+)\) ≤ Budget \(([\d,]+)\)', content)
        if budget_constraint_match:
            viz_info['used_budget'] = int(budget_constraint_match.group(1).replace(',', ''))
            viz_info['total_budget'] = int(budget_constraint_match.group(2).replace(',', ''))
            
    except Exception as e:
        print(f"Error reading {viz_file}: {e}")
        
    return viz_info

def extract_terminal_info(terminals_file):
    """Extract battery information from terminals file"""
    terminal_info = {}
    try:
        with open(terminals_file, 'r') as f:
            lines = f.readlines()
            
        battery_levels = []
        for line in lines:
            parts = line.strip().split()
            if len(parts) >= 3:
                battery_levels.append(float(parts[2]))
                
        if battery_levels:
            terminal_info['avg_battery'] = sum(battery_levels) / len(battery_levels)
            terminal_info['min_battery'] = min(battery_levels)
            terminal_info['max_battery'] = max(battery_levels)
            terminal_info['low_battery_count'] = sum(1 for b in battery_levels if b < 20)
            terminal_info['total_terminals'] = len(battery_levels)
            
    except Exception as e:
        print(f"Error reading {terminals_file}: {e}")
        
    return terminal_info

def create_summary_table(results_dir):
    """Create a comprehensive summary table"""
    
    # Find all iteration files
    solution_files = sorted(glob.glob(f"{results_dir}/solution_iter*.txt"))
    
    print("🔋 BATTERY-AWARE NETWORK OPTIMIZATION - ITERATION SUMMARY")
    print("=" * 80)
    print()
    
    # Table header
    print(f"{'Iter':<4} {'Budget':<8} {'Alpha':<6} {'Beta':<6} {'MIP Gap':<10} {'Total Cost':<12} {'Coverage':<10} {'FSTs':<8} {'Avg Battery':<12} {'Low Bat':<8} {'Trans Cost':<12}")
    print("-" * 115)
    
    all_data = []
    
    for solution_file in solution_files:
        # Extract iteration number
        iter_match = re.search(r'iter(\d+)\.txt', solution_file)
        if not iter_match:
            continue
            
        iter_num = int(iter_match.group(1))
        
        # Get corresponding files
        viz_file = solution_file.replace('solution_', 'visualization_').replace('.txt', '.html')
        terminals_file = solution_file.replace('solution_', 'terminals_').replace('.txt', '.txt')
        transition_file = solution_file.replace('solution_', 'transition_cost_')
        
        # Extract data
        budget_info = extract_budget_info(solution_file)
        viz_info = extract_visualization_info(viz_file)
        terminal_info = extract_terminal_info(terminals_file)
        
        # Combine all data
        row_data = {
            'iteration': iter_num,
            'budget_limit': budget_info.get('budget_limit', 'N/A'),
            'alpha': budget_info.get('alpha', 'N/A'),
            'beta': budget_info.get('beta', 'N/A'),
            'mip_gap': viz_info.get('mip_gap_percent', 'N/A'),
            'total_cost': viz_info.get('total_cost', 'N/A'),
            'coverage_rate': viz_info.get('coverage_rate', 'N/A'),
            'selected_fsts': viz_info.get('selected_fsts', 'N/A'),
            'total_fsts': viz_info.get('total_fsts', 'N/A'),
            'avg_battery': terminal_info.get('avg_battery', 'N/A'),
            'low_battery_count': terminal_info.get('low_battery_count', 'N/A'),
            'covered_terminals': viz_info.get('covered_terminals', 'N/A'),
            'uncovered_terminals': viz_info.get('uncovered_terminals', 'N/A'),
            'used_budget': viz_info.get('used_budget', 'N/A'),
            'total_budget': viz_info.get('total_budget', 'N/A'),
            'max_tree_cost': budget_info.get('max_tree_cost', 'N/A'),
            'scale_factor': budget_info.get('scale_factor', 'N/A'),
            'max_tree_cost_norm': budget_info.get('max_tree_cost_norm', 'N/A'),
            'max_battery_cost_norm': budget_info.get('max_battery_cost_norm', 'N/A')
        }
        
        all_data.append(row_data)
        
        # Print row
        budget_str = f"{row_data['budget_limit']:.1f}" if isinstance(row_data['budget_limit'], (int, float)) else str(row_data['budget_limit'])
        alpha_str = f"{row_data['alpha']:.1f}" if isinstance(row_data['alpha'], (int, float)) else str(row_data['alpha'])
        beta_str = f"{row_data['beta']:.1f}" if isinstance(row_data['beta'], (int, float)) else str(row_data['beta'])
        mip_gap_str = f"{row_data['mip_gap']:.4f}%" if isinstance(row_data['mip_gap'], (int, float)) else str(row_data['mip_gap'])
        total_cost_str = f"{row_data['total_cost']:.3f}" if isinstance(row_data['total_cost'], (int, float)) else str(row_data['total_cost'])
        coverage_str = f"{row_data['coverage_rate']:.1f}%" if isinstance(row_data['coverage_rate'], (int, float)) else str(row_data['coverage_rate'])
        fsts_str = f"{row_data['selected_fsts']}/{row_data['total_fsts']}" if isinstance(row_data['selected_fsts'], int) else str(row_data['selected_fsts'])
        avg_battery_str = f"{row_data['avg_battery']:.1f}%" if isinstance(row_data['avg_battery'], (int, float)) else str(row_data['avg_battery'])
        low_battery_str = f"{row_data['low_battery_count']}" if isinstance(row_data['low_battery_count'], int) else str(row_data['low_battery_count'])
        
        print(f"{iter_num:<4} {budget_str:<8} {alpha_str:<6} {beta_str:<6} {mip_gap_str:<10} {total_cost_str:<12} {coverage_str:<10} {fsts_str:<8} {avg_battery_str:<12} {low_battery_str:<8}")
    
    print()
    print("📊 DETAILED ANALYSIS")
    print("=" * 80)
    
    # Calculate statistics
    valid_data = [d for d in all_data if isinstance(d['total_cost'], (int, float))]
    if valid_data:
        avg_cost = sum(d['total_cost'] for d in valid_data) / len(valid_data)
        avg_coverage = sum(d['coverage_rate'] for d in valid_data if isinstance(d['coverage_rate'], (int, float))) / len([d for d in valid_data if isinstance(d['coverage_rate'], (int, float))])
        avg_fsts = sum(d['selected_fsts'] for d in valid_data if isinstance(d['selected_fsts'], int)) / len([d for d in valid_data if isinstance(d['selected_fsts'], int)])
        
        print(f"Average Total Cost: {avg_cost:.3f}")
        print(f"Average Coverage Rate: {avg_coverage:.1f}%")
        print(f"Average FSTs Selected: {avg_fsts:.1f}")
        print()
    
    # Show configuration info from first iteration
    if all_data:
        first_data = all_data[0]
        print("⚙️  CONFIGURATION")
        print("=" * 40)
        print(f"Scale Factor: {first_data['scale_factor']}")
        print(f"Max Tree Cost: {first_data['max_tree_cost']:,.0f}" if isinstance(first_data['max_tree_cost'], (int, float)) else f"Max Tree Cost: {first_data['max_tree_cost']}")
        print(f"Budget Formula: Σ (normalized_tree_cost × {first_data['scale_factor']}) × x[i] ≤ {first_data['budget_limit']}")
        print()
    
    # Show budget utilization
    print("💰 BUDGET UTILIZATION")
    print("=" * 40)
    for data in all_data[:5]:  # Show first 5 iterations
        if isinstance(data['used_budget'], int) and isinstance(data['total_budget'], int):
            utilization = (data['used_budget'] / data['total_budget']) * 100
            print(f"Iteration {data['iteration']}: {data['used_budget']:,} / {data['total_budget']:,} ({utilization:.1f}%)")
    
    print()
    print("🔋 BATTERY EVOLUTION")
    print("=" * 40)
    for data in all_data[:5]:  # Show first 5 iterations
        if isinstance(data['avg_battery'], (int, float)) and isinstance(data['low_battery_count'], int):
            print(f"Iteration {data['iteration']}: Avg={data['avg_battery']:.1f}%, Low Battery Terminals={data['low_battery_count']}")
    
    return all_data

def main():
    """Main function"""
    import sys
    
    if len(sys.argv) != 2:
        print("Usage: python3 create_iteration_summary.py <results_directory>")
        print("Example: python3 create_iteration_summary.py results3")
        sys.exit(1)
    
    results_dir = sys.argv[1]
    
    if not os.path.exists(results_dir):
        print(f"Error: Directory '{results_dir}' does not exist")
        sys.exit(1)
    
    try:
        data = create_summary_table(results_dir)
        
        # Save detailed data to file
        output_file = f"{results_dir}/iteration_summary_detailed.txt"
        with open(output_file, 'w') as f:
            f.write("DETAILED ITERATION DATA\n")
            f.write("=" * 50 + "\n\n")
            
            for data_row in data:
                f.write(f"Iteration {data_row['iteration']}:\n")
                for key, value in data_row.items():
                    if key != 'iteration':
                        f.write(f"  {key}: {value}\n")
                f.write("\n")
        
        print(f"\n📁 Detailed data saved to: {output_file}")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
