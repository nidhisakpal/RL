#!/usr/bin/env python3
"""
Compute edge statistics from dumpfst and solution files
This provides edge count and total edge length for topology distance computation
"""

import sys
import re
import math

def parse_terminals(terminals_file):
    """Parse terminal coordinates from terminals file"""
    terminals = {}
    with open(terminals_file, 'r') as f:
        term_id = 0
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 2:
                try:
                    x = float(parts[0])
                    y = float(parts[1])
                    terminals[term_id] = (x, y)
                    term_id += 1
                except ValueError:
                    continue
    return terminals

def parse_selected_fsts(solution_file):
    """Parse selected FST indices from solution file"""
    selected = []
    with open(solution_file, 'r') as f:
        in_lp_vars = False
        for line in f:
            if 'LP_VARS' in line:
                in_lp_vars = True
            if in_lp_vars and 'x[' in line and 'not_covered' not in line:
                # Parse: x[123] = 1.000000
                match = re.search(r'x\[(\d+)\]\s*=\s*([\d.]+)', line)
                if match:
                    fst_id = int(match.group(1))
                    value = float(match.group(2))
                    if value >= 0.5:
                        selected.append(fst_id)
    return selected

def parse_fst_edges_from_dump(dump_file, selected_fsts):
    """
    Parse edge list from dumpfst output for selected FSTs
    Returns set of edges as tuples (v1, v2) where v1 < v2
    """
    edges = set()

    with open(dump_file, 'r') as f:
        lines = f.readlines()

    # Find where FST definitions end and edge list begins
    # The edge list starts after FST terminal listings
    edge_start = 0
    for i, line in enumerate(lines):
        parts = line.strip().split()
        # Edge lines have exactly 2 numbers
        if len(parts) == 2:
            try:
                int(parts[0])
                int(parts[1])
                edge_start = i
                break
            except ValueError:
                continue

    if edge_start == 0:
        return edges

    # Parse FST definitions to build FST->edges mapping
    fst_edges_map = {}  # fst_id -> list of edges
    current_fst = 0

    for line in lines[:edge_start]:
        parts = line.strip().split()
        if len(parts) >= 3:
            # This is an FST definition: "5 1 4" or "11 2 7 9"
            terminals = [int(p) for p in parts]

            # For a tree connecting N terminals, there are N-1 edges
            # We'll store the terminals and later map to edges
            fst_edges_map[current_fst] = terminals
            current_fst += 1

    # Parse edge list - these are the actual edges in the solution
    # Format: pairs of vertex indices
    for line in lines[edge_start:]:
        parts = line.strip().split()
        if len(parts) == 2:
            try:
                v1, v2 = int(parts[0]), int(parts[1])
                # Canonicalize edge (smaller vertex first)
                edge = (min(v1, v2), max(v1, v2))
                edges.add(edge)
            except ValueError:
                continue

    return edges

def compute_edge_length(v1, v2, terminals):
    """Compute Euclidean distance between two vertices"""
    if v1 not in terminals or v2 not in terminals:
        return 0.0
    x1, y1 = terminals[v1]
    x2, y2 = terminals[v2]
    return math.sqrt((x2 - x1)**2 + (y2 - y1)**2)

def compute_topology_distance(dump_file_prev, dump_file_curr,
                              solution_prev, solution_curr,
                              terminals_file):
    """
    Compute topology distance between two iterations
    Returns (edge_count, total_edge_length)
    """
    # Parse terminals
    terminals = parse_terminals(terminals_file)

    # Parse selected FSTs
    selected_prev = parse_selected_fsts(solution_prev)
    selected_curr = parse_selected_fsts(solution_curr)

    # Parse edges from dumpfst
    edges_prev = parse_fst_edges_from_dump(dump_file_prev, selected_prev)
    edges_curr = parse_fst_edges_from_dump(dump_file_curr, selected_curr)

    # Compute symmetric difference
    edges_added = edges_curr - edges_prev
    edges_removed = edges_prev - edges_curr
    edges_changed = edges_added | edges_removed

    # Count and measure changed edges
    edge_count = len(edges_changed)
    total_length = 0.0

    for v1, v2 in edges_changed:
        total_length += compute_edge_length(v1, v2, terminals)

    return edge_count, total_length

def main():
    if len(sys.argv) < 6:
        print("Usage: compute_edge_stats.py <dump_prev> <dump_curr> <sol_prev> <sol_curr> <terminals>")
        print("       For first iteration, use 'NONE' for dump_prev and sol_prev")
        sys.exit(1)

    dump_prev = sys.argv[1]
    dump_curr = sys.argv[2]
    sol_prev = sys.argv[3]
    sol_curr = sys.argv[4]
    terminals = sys.argv[5]

    # Handle first iteration
    if dump_prev == 'NONE' or sol_prev == 'NONE':
        print("0 (0.000)")
        return

    try:
        edge_count, total_length = compute_topology_distance(
            dump_prev, dump_curr, sol_prev, sol_curr, terminals)

        # Output format: edge_count (edge_length)
        print(f"{edge_count} ({total_length:.3f})")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        print("0 (0.000)")
        sys.exit(1)

if __name__ == '__main__':
    main()
