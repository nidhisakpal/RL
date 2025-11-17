# Graph Distance Computation - User Guide

## Overview

The graph distance module computes the transition cost between two network topologies, capturing the effort required to reconfigure from an old graph (G_old) to a new graph (G_new).

## Mathematical Formulation

### Two-Scenario Cost Model

The distance between two graphs considers two scenarios for each edge:

**Scenario 1:** If edge `e` in G_new was also in G_old
- **Cost = 0** (no reconfiguration needed)

**Scenario 2:** If edge `e` in G_new was NOT in G_old
- **Cost reflects distance from nearest edge in G_old**
- Uses Hausdorff distance to measure edge similarity

### Algorithm

**Step 1: Compute Edge Distances**
For each pair of edges (e ∈ G_old, e' ∈ G_new), compute the Hausdorff distance d(e, e').

The Hausdorff distance between two edges measures:
```
H(e1, e2) = max(h(e1, e2), h(e2, e1))
```
where `h(A, B) = max_{a ∈ A} min_{b ∈ B} distance(a, b)`

For line segments, this captures the maximum distance from any point on one edge to the closest point on the other edge.

**Step 2: Solve Bipartite Matching**
Find optimal matching π(e) that minimizes:
```
Σ d(e, π(e)) for all e ∈ G_old
```

This is formulated as an integer optimization problem:

**Variables:**
- `x[i][j] = 1` if edge i in G_old is matched to edge j in G_new
- `x[i][j] = 0` otherwise

**Objective:**
```
Minimize: Σᵢ Σⱼ d(eᵢ, e'ⱼ) × x[i][j] + Σₖ length(e'ₖ) for unmatched e'ₖ
```

**Constraints:**
- Each edge in G_old matched to at most one edge in G_new
- Each edge in G_new matched to at most one edge in G_old

**Step 3: Add Unmatched Edge Costs**
For edges in G_new that are not matched to any edge in G_old, add their edge length to the total cost (representing the cost of building entirely new edges).

## API Reference

### Core Functions

#### `compute_graph_distance`
```c
double compute_graph_distance(const Graph *G_old, const Graph *G_new);
```
Computes the total transition cost from G_old to G_new.

**Parameters:**
- `G_old`: The current/old graph topology
- `G_new`: The desired/new graph topology

**Returns:** Total distance (transition cost)

#### `create_graph_from_terminals`
```c
Graph *create_graph_from_terminals(double *terminals, int num_terminals,
                                    int *edge_pairs, int num_edges);
```
Creates a Graph structure from terminal coordinates and edge list.

**Parameters:**
- `terminals`: Array of (x, y) coordinates, length = 2 × num_terminals
- `num_terminals`: Number of terminals
- `edge_pairs`: Array of edge terminal indices, length = 2 × num_edges
- `num_edges`: Number of edges

**Returns:** Allocated Graph structure

#### `free_graph`
```c
void free_graph(Graph *g);
```
Frees memory allocated for a Graph.

#### `hausdorff_edge_distance`
```c
double hausdorff_edge_distance(const GraphEdge *e1, const GraphEdge *e2);
```
Computes Hausdorff distance between two edges.

### Data Structures

#### `GraphEdge`
```c
typedef struct {
    int terminal1;      /* First terminal index */
    int terminal2;      /* Second terminal index */
    double x1, y1;      /* Coordinates of first terminal */
    double x2, y2;      /* Coordinates of second terminal */
    double length;      /* Edge length */
} GraphEdge;
```

#### `Graph`
```c
typedef struct {
    GraphEdge *edges;   /* Array of edges */
    int num_edges;      /* Number of edges */
    int num_terminals;  /* Number of terminals */
} Graph;
```

## Usage Examples

### Example 1: Basic Usage

```c
#include "graph_distance.h"

int main() {
    /* Define terminal coordinates */
    double terminals[] = {
        0.0, 0.0,   /* Terminal 0 */
        1.0, 0.0,   /* Terminal 1 */
        0.0, 1.0,   /* Terminal 2 */
        1.0, 1.0    /* Terminal 3 */
    };

    /* Define old graph edges */
    int edges_old[] = {
        0, 1,  /* Edge between terminal 0 and 1 */
        1, 3,  /* Edge between terminal 1 and 3 */
        2, 3   /* Edge between terminal 2 and 3 */
    };

    /* Define new graph edges */
    int edges_new[] = {
        0, 1,  /* Same edge */
        1, 2,  /* DIFFERENT: now connects 1-2 instead of 1-3 */
        2, 3   /* Same edge */
    };

    /* Create graphs */
    Graph *g_old = create_graph_from_terminals(terminals, 4, edges_old, 3);
    Graph *g_new = create_graph_from_terminals(terminals, 4, edges_new, 3);

    /* Compute distance */
    double distance = compute_graph_distance(g_old, g_new);

    printf("Transition cost: %.6f\\n", distance);

    /* Clean up */
    free_graph(g_old);
    free_graph(g_new);

    return 0;
}
```

### Example 2: Integration with Battery-Aware Optimization

```c
/* Extract edges from solution iteration */
Graph *extract_solution_graph(const char *solution_file,
                               double *terminals, int num_terminals) {
    /* Parse solution file to get selected FSTs */
    /* Convert FSTs to edge list */
    /* Return Graph structure */
}

/* In run_optimization.sh loop */
Graph *g_prev = NULL;

for (iter = 1; iter <= max_iterations; iter++) {
    /* Run optimization */
    run_battery_aware_optimization(...);

    /* Extract new solution graph */
    Graph *g_curr = extract_solution_graph(solution_file, terminals, n);

    if (g_prev != NULL) {
        /* Compute transition cost */
        double transition_cost = compute_graph_distance(g_prev, g_curr);

        printf("Iteration %d -> %d transition cost: %.6f\\n",
               iter-1, iter, transition_cost);

        free_graph(g_prev);
    }

    g_prev = g_curr;
}
```

## Compilation

### Compile the Module
```bash
gcc -O3 -Wall -c graph_distance.c -o graph_distance.o -lm
```

### Link with Your Program
```bash
gcc -O3 -Wall your_program.c graph_distance.o -o your_program -lm
```

### Run Tests
```bash
gcc -O3 -Wall test_graph_distance.c graph_distance.o -o test_graph_distance -lm
./test_graph_distance
```

## Integration with Existing Framework

### Adding to run_optimization.sh

You can extend the optimization loop to track transition costs:

```bash
# In run_optimization.sh after each iteration
if [ $iter -gt 1 ]; then
    # Extract graphs and compute distance
    ./compute_transition_cost \\
        "results/solution_iter$((iter-1)).txt" \\
        "results/solution_iter${iter}.txt" \\
        "results/terminals_iter${iter}.txt"
fi
```

### Creating Helper Tool

Create a standalone tool `compute_transition_cost.c` that:
1. Reads two solution files
2. Extracts edge lists from selected FSTs
3. Computes graph distance
4. Outputs transition cost

## Future Enhancements

### Current Implementation
- **Matching Algorithm:** Greedy approximation
- **Time Complexity:** O(n² × m) where n = edges in G_old, m = edges in G_new

### Potential Improvements

1. **Optimal Matching:** Replace greedy with Hungarian algorithm or CPLEX-based optimization
   - Guarantees globally optimal matching
   - Time complexity: O(n³) with Hungarian algorithm

2. **Steiner Point Consideration:** Account for Steiner points in FSTs, not just terminal endpoints

3. **Edge Weights:** Incorporate battery levels or other edge attributes into distance computation

4. **Caching:** Cache distance matrix between iterations to avoid recomputation

## Mathematical Properties

### Distance Metric Properties

The graph distance satisfies several useful properties:

1. **Non-negativity:** `d(G_old, G_new) ≥ 0`
2. **Identity:** `d(G, G) = 0`
3. **Not necessarily symmetric:** `d(G1, G2) ≠ d(G2, G1)` in general
   - Due to unmatched edges being charged at their full length

### Interpretation

- **Small distance:** Topologies are similar, minimal reconfiguration needed
- **Large distance:** Topologies differ significantly, major reconfiguration required
- **Zero distance:** Topologies are identical (or differ only in Steiner points)

## Debugging Output

The module provides detailed debug output (to stderr):

```
=== COMPUTING GRAPH DISTANCE ===
G_old: 3 edges, 4 terminals
G_new: 4 edges, 4 terminals
DEBUG GRAPH_DIST: Unmatched new edge 3 (t0-t2), length=1.000

Matching details:
  Old edge 0 (t0-t1) -> New edge 0 (t0-t1), dist=0.000
  Old edge 1 (t1-t3) -> New edge 1 (t1-t3), dist=0.000
  Old edge 2 (t2-t3) -> New edge 2 (t2-t3), dist=0.000

Total graph distance: 1.000000
=================================
```

## Performance Considerations

- **Small graphs (<100 edges):** Real-time computation (< 1ms)
- **Medium graphs (100-1000 edges):** Fast computation (< 100ms)
- **Large graphs (>1000 edges):** May need optimization

For very large graphs, consider:
- Parallel distance matrix computation
- Approximate matching algorithms
- Spatial indexing for nearest-neighbor search

## References

- Hausdorff distance: https://en.wikipedia.org/wiki/Hausdorff_distance
- Hungarian algorithm: https://en.wikipedia.org/wiki/Hungarian_algorithm
- Assignment problem: https://en.wikipedia.org/wiki/Assignment_problem
