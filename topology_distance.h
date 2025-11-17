/*
 * Topology Distance Metrics for Battery-Aware Network Optimization
 *
 * Computes distance between network topologies across iterations
 * to measure how much the physical robot network changes.
 */

#ifndef TOPOLOGY_DISTANCE_H
#define TOPOLOGY_DISTANCE_H

/* Distance metric methods */
typedef enum {
    DISTANCE_FST_SET = 0,       /* Simple FST set difference (count of changed FSTs) */
    DISTANCE_L1_MANHATTAN = 1,  /* Manhattan distance on edge vectors (easiest to linearize) */
    DISTANCE_L2_EUCLIDEAN = 2,  /* Euclidean distance on edge vectors (L2 norm) */
} distance_method_t;

/* Edge representation - canonical form for comparison */
typedef struct {
    double x1, y1;  /* First endpoint */
    double x2, y2;  /* Second endpoint */
} edge_t;

/* Edge set for a solution */
typedef struct {
    edge_t* edges;      /* Array of edges */
    int num_edges;      /* Number of edges */
    int capacity;       /* Allocated capacity */
} edge_set_t;

/*
 * Result structure for topology distance computation
 */
typedef struct {
    double edge_length;      /* Total geometric length of changed edges */
    int edge_count;          /* Number of edges that changed */
    int fst_count;           /* Number of FSTs that changed */
} topology_distance_result_t;

/*
 * Compute topology distance between two solutions
 *
 * Parameters:
 *   fst_file: Path to FST file containing all candidate trees
 *   solution_file_prev: Previous iteration solution (or NULL for first iteration)
 *   solution_file_curr: Current iteration solution
 *   method: Distance metric to use
 *
 * Returns:
 *   Distance value (0.0 for first iteration when solution_file_prev is NULL)
 */
double compute_topology_distance(
    const char* fst_file,
    const char* solution_file_prev,
    const char* solution_file_curr,
    distance_method_t method
);

/*
 * Compute detailed topology distance metrics
 *
 * Returns topology_distance_result_t with edge length, edge count, and FST count
 */
topology_distance_result_t compute_topology_distance_detailed(
    const char* fst_file,
    const char* solution_file_prev,
    const char* solution_file_curr
);

/*
 * Parse solution file to extract selected FST indices
 *
 * Returns array of selected FST indices and sets num_selected
 * Caller must free the returned array
 */
int* parse_selected_fsts(const char* solution_file, int* num_selected);

/*
 * Build edge set from selected FSTs
 *
 * Returns edge_set_t containing all edges from selected FSTs
 * Caller must free the edge set using free_edge_set()
 */
edge_set_t* build_edge_set(const char* fst_file, int* selected_fsts, int num_selected);

/*
 * Compute L1 (Manhattan) distance between two edge sets
 *
 * Distance = |edges in A but not in B| + |edges in B but not in A|
 */
double edge_set_distance_l1(edge_set_t* set1, edge_set_t* set2);

/*
 * Compute L2 (Euclidean) distance between two edge sets
 *
 * Distance = sqrt(|symmetric difference|)
 */
double edge_set_distance_l2(edge_set_t* set1, edge_set_t* set2);

/*
 * Compute FST set distance - simple count of changed FSTs
 *
 * Distance = |FSTs in A but not in B| + |FSTs in B but not in A|
 */
double fst_set_distance(int* fsts1, int num1, int* fsts2, int num2);

/*
 * Free edge set memory
 */
void free_edge_set(edge_set_t* set);

/*
 * Print edge set for debugging
 */
void print_edge_set(edge_set_t* set);

#endif /* TOPOLOGY_DISTANCE_H */
