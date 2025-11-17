# Graph Distance - Quick Start Guide

## What It Does

Computes the **transition cost** between two network topologies:
- **Scenario 1:** Matched edges (same position) → Cost = 0
- **Scenario 2:** Unmatched edges → Cost = Hausdorff distance + length

## Quick Example

```c
#include "graph_distance.h"

// Terminal coordinates: (x, y) pairs
double terminals[] = {0.0, 0.0,  1.0, 0.0,  0.0, 1.0,  1.0, 1.0};

// Old graph: edges 0-1, 1-3, 2-3
int edges_old[] = {0, 1,  1, 3,  2, 3};

// New graph: edges 0-1, 1-2, 2-3 (middle edge changed!)
int edges_new[] = {0, 1,  1, 2,  2, 3};

// Create and compute
Graph *g_old = create_graph_from_terminals(terminals, 4, edges_old, 3);
Graph *g_new = create_graph_from_terminals(terminals, 4, edges_new, 3);

double cost = compute_graph_distance(g_old, g_new);
printf("Transition cost: %.6f\n", cost);  // 1.000000

free_graph(g_old);
free_graph(g_new);
```

## Compile and Test

```bash
# Compile
gcc -O3 -Wall -c graph_distance.c -o graph_distance.o -lm

# Test
gcc -O3 -Wall test_graph_distance.c graph_distance.o -o test_graph_distance -lm
./test_graph_distance
```

## Key Functions

| Function | Purpose | Returns |
|----------|---------|---------|
| `compute_graph_distance(g1, g2)` | Compute transition cost | double |
| `create_graph_from_terminals(...)` | Create graph from data | Graph* |
| `free_graph(g)` | Free memory | void |
| `hausdorff_edge_distance(e1, e2)` | Distance between edges | double |

## Algorithm

1. **Build distance matrix:** Compute Hausdorff distance for all edge pairs
2. **Find matching:** Assign each old edge to nearest new edge (greedy)
3. **Add unmatched cost:** New edges without match incur their length
4. **Return total cost:** Sum of matched distances + unmatched lengths

## Files

- **[graph_distance.h](graph_distance.h)** - Header with API
- **[graph_distance.c](graph_distance.c)** - Implementation
- **[test_graph_distance.c](test_graph_distance.c)** - Test suite
- **[GRAPH_DISTANCE_GUIDE.md](GRAPH_DISTANCE_GUIDE.md)** - Full documentation

## Typical Values

- **Identical graphs:** 0.0
- **Minor changes:** 0.5 - 2.0
- **Major reconfiguration:** 5.0 - 20.0
- **Complete rebuild:** > 50.0

## Integration Pattern

```c
Graph *g_prev = NULL;

for (int iter = 1; iter <= max_iter; iter++) {
    // Run optimization
    run_optimization(iter, ...);

    // Extract graph from solution
    Graph *g_curr = extract_solution_graph(...);

    // Compute transition cost
    if (g_prev) {
        double cost = compute_graph_distance(g_prev, g_curr);
        printf("Iter %d->%d cost: %.6f\n", iter-1, iter, cost);
        free_graph(g_prev);
    }

    g_prev = g_curr;
}
```

## Debug Output

Set stderr to see detailed matching:

```
=== COMPUTING GRAPH DISTANCE ===
Matching details:
  Old edge 0 (t0-t1) -> New edge 0 (t0-t1), dist=0.000
  Old edge 1 (t1-t3) -> New edge 1 (t1-t2), dist=1.000
Total graph distance: 1.000000
=================================
```

## Performance

- **10 edges:** < 0.1 ms
- **100 edges:** < 10 ms
- **1000 edges:** < 1 sec

## Next Steps

1. Read **[GRAPH_DISTANCE_GUIDE.md](GRAPH_DISTANCE_GUIDE.md)** for details
2. See **[GRAPH_DISTANCE_IMPLEMENTATION.md](GRAPH_DISTANCE_IMPLEMENTATION.md)** for technical info
3. Check **test_graph_distance.c** for more examples
