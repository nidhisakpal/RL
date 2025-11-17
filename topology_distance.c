/*
 * Topology Distance Metrics Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "topology_distance.h"

#define MAX_LINE 4096
#define EDGE_EPSILON 1e-6  /* Tolerance for comparing edge coordinates */

/*
 * Canonicalize edge - ensure first endpoint has smaller coordinates
 * This allows us to compare edges consistently
 */
static void canonicalize_edge(edge_t* e) {
    if (e->x1 > e->x2 || (fabs(e->x1 - e->x2) < EDGE_EPSILON && e->y1 > e->y2)) {
        /* Swap endpoints */
        double temp;
        temp = e->x1; e->x1 = e->x2; e->x2 = temp;
        temp = e->y1; e->y1 = e->y2; e->y2 = temp;
    }
}

/*
 * Compare two edges for equality (within epsilon tolerance)
 */
static int edges_equal(edge_t* e1, edge_t* e2) {
    return (fabs(e1->x1 - e2->x1) < EDGE_EPSILON &&
            fabs(e1->y1 - e2->y1) < EDGE_EPSILON &&
            fabs(e1->x2 - e2->x2) < EDGE_EPSILON &&
            fabs(e1->y2 - e2->y2) < EDGE_EPSILON);
}

/*
 * Check if edge exists in edge set
 */
static int edge_exists(edge_set_t* set, edge_t* e) {
    for (int i = 0; i < set->num_edges; i++) {
        if (edges_equal(&set->edges[i], e)) {
            return 1;
        }
    }
    return 0;
}

/*
 * Add edge to edge set (if not already present)
 */
static void add_edge(edge_set_t* set, double x1, double y1, double x2, double y2) {
    edge_t e = {x1, y1, x2, y2};
    canonicalize_edge(&e);

    /* Check if edge already exists */
    if (edge_exists(set, &e)) {
        return;
    }

    /* Expand capacity if needed */
    if (set->num_edges >= set->capacity) {
        set->capacity = (set->capacity == 0) ? 16 : set->capacity * 2;
        set->edges = realloc(set->edges, set->capacity * sizeof(edge_t));
        if (!set->edges) {
            fprintf(stderr, "Error: Failed to allocate memory for edges\n");
            exit(1);
        }
    }

    /* Add edge */
    set->edges[set->num_edges++] = e;
}

/*
 * Parse solution file to extract selected FST indices
 */
int* parse_selected_fsts(const char* solution_file, int* num_selected) {
    FILE* fp = fopen(solution_file, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open solution file %s\n", solution_file);
        return NULL;
    }

    int* selected = NULL;
    int capacity = 16;
    *num_selected = 0;

    selected = malloc(capacity * sizeof(int));
    if (!selected) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        fclose(fp);
        return NULL;
    }

    char line[MAX_LINE];
    int in_lp_vars = 0;

    while (fgets(line, sizeof(line), fp)) {
        /* Look for LP_VARS section */
        if (strstr(line, "LP_VARS:") || strstr(line, "DEBUG LP_VARS:")) {
            in_lp_vars = 1;
        }

        /* Stop at end of LP_VARS section (when we hit not_covered variables) */
        if (in_lp_vars && strstr(line, "not_covered")) {
            break;
        }

        /* Parse variable assignments: x[123] = 1.000000 */
        if (in_lp_vars && strstr(line, "x[") && !strstr(line, "not_covered")) {
            int fst_index;
            double value;
            if (sscanf(line, "%*[^x]x[%d] = %lf", &fst_index, &value) == 2) {
                /* Consider FST selected if value >= 0.5 */
                if (value >= 0.5) {
                    if (*num_selected >= capacity) {
                        capacity *= 2;
                        selected = realloc(selected, capacity * sizeof(int));
                        if (!selected) {
                            fprintf(stderr, "Error: Failed to allocate memory\n");
                            fclose(fp);
                            return NULL;
                        }
                    }
                    selected[(*num_selected)++] = fst_index;
                }
            }
        }
    }

    fclose(fp);
    return selected;
}

/*
 * Parse a single FST from the FST file format
 *
 * FST format (from efst):
 * Line 1: num_terminals num_steiner_points total_length
 * Line 2+: terminal_index x y
 * Line N+: S x y (for Steiner points)
 * Line M+: edge definitions (pairs of point indices or coordinates)
 */
static void parse_fst_edges(FILE* fp, edge_set_t* edge_set) {
    char line[MAX_LINE];
    int num_terminals, num_steiner;
    double length;

    /* Read header line */
    if (!fgets(line, sizeof(line), fp)) {
        return;
    }

    if (sscanf(line, "%d %d %lf", &num_terminals, &num_steiner, &length) != 3) {
        return;
    }

    int total_points = num_terminals + num_steiner;

    /* Arrays to store point coordinates */
    double* x_coords = malloc(total_points * sizeof(double));
    double* y_coords = malloc(total_points * sizeof(double));

    if (!x_coords || !y_coords) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    /* Read terminal coordinates */
    for (int i = 0; i < num_terminals; i++) {
        if (!fgets(line, sizeof(line), fp)) {
            free(x_coords);
            free(y_coords);
            return;
        }

        int term_idx;
        if (sscanf(line, "%d %lf %lf", &term_idx, &x_coords[i], &y_coords[i]) != 3) {
            free(x_coords);
            free(y_coords);
            return;
        }
    }

    /* Read Steiner point coordinates */
    for (int i = 0; i < num_steiner; i++) {
        if (!fgets(line, sizeof(line), fp)) {
            free(x_coords);
            free(y_coords);
            return;
        }

        char S;
        int idx = num_terminals + i;
        if (sscanf(line, " %c %lf %lf", &S, &x_coords[idx], &y_coords[idx]) != 3) {
            free(x_coords);
            free(y_coords);
            return;
        }
    }

    /* Read edge definitions - remaining lines until blank or EOF */
    while (fgets(line, sizeof(line), fp)) {
        /* Stop at blank line or next FST */
        if (line[0] == '\n' || strlen(line) <= 1) {
            break;
        }

        int v1, v2;
        if (sscanf(line, "%d %d", &v1, &v2) == 2) {
            /* Edge between vertices v1 and v2 */
            if (v1 >= 0 && v1 < total_points && v2 >= 0 && v2 < total_points) {
                add_edge(edge_set,
                        x_coords[v1], y_coords[v1],
                        x_coords[v2], y_coords[v2]);
            }
        }
    }

    free(x_coords);
    free(y_coords);
}

/*
 * Build edge set from selected FSTs
 */
edge_set_t* build_edge_set(const char* fst_file, int* selected_fsts, int num_selected) {
    FILE* fp = fopen(fst_file, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open FST file %s\n", fst_file);
        return NULL;
    }

    edge_set_t* set = calloc(1, sizeof(edge_set_t));
    if (!set) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(fp);
        return NULL;
    }

    /* Read through FST file and parse selected FSTs */
    int fst_index = 0;
    int selected_idx = 0;

    /* Sort selected FSTs for efficient lookup */
    /* Simple bubble sort since arrays are typically small */
    for (int i = 0; i < num_selected - 1; i++) {
        for (int j = 0; j < num_selected - i - 1; j++) {
            if (selected_fsts[j] > selected_fsts[j + 1]) {
                int temp = selected_fsts[j];
                selected_fsts[j] = selected_fsts[j + 1];
                selected_fsts[j + 1] = temp;
            }
        }
    }

    /* Parse each FST */
    while (!feof(fp) && selected_idx < num_selected) {
        /* Check if this FST is selected */
        if (fst_index == selected_fsts[selected_idx]) {
            /* Parse this FST's edges */
            parse_fst_edges(fp, set);
            selected_idx++;
        } else {
            /* Skip this FST - read until blank line */
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '\n' || strlen(line) <= 1) {
                    break;
                }
            }
        }

        fst_index++;
    }

    fclose(fp);
    return set;
}

/*
 * Compute edge length (Euclidean distance between endpoints)
 */
static double edge_length(edge_t* e) {
    double dx = e->x2 - e->x1;
    double dy = e->y2 - e->y1;
    return sqrt(dx * dx + dy * dy);
}

/*
 * Compute L1 (Manhattan) distance between two edge sets
 *
 * For the edge-based formulation:
 * GD = sum over all edges |Z_t+1[e] - Z_t[e]|
 *    = |edges in A but not in B| + |edges in B but not in A|
 *    = |symmetric difference|
 */
double edge_set_distance_l1(edge_set_t* set1, edge_set_t* set2) {
    int diff_count = 0;

    /* Count edges in set1 but not in set2 */
    for (int i = 0; i < set1->num_edges; i++) {
        if (!edge_exists(set2, &set1->edges[i])) {
            diff_count++;
        }
    }

    /* Count edges in set2 but not in set1 */
    for (int i = 0; i < set2->num_edges; i++) {
        if (!edge_exists(set1, &set2->edges[i])) {
            diff_count++;
        }
    }

    return (double)diff_count;
}

/*
 * Compute detailed edge distance metrics
 * Returns both the total length and count of changed edges
 */
static void edge_set_distance_detailed(edge_set_t* set1, edge_set_t* set2,
                                       double* total_length, int* edge_count) {
    *total_length = 0.0;
    *edge_count = 0;

    /* Add edges in set1 but not in set2 */
    for (int i = 0; i < set1->num_edges; i++) {
        if (!edge_exists(set2, &set1->edges[i])) {
            *total_length += edge_length(&set1->edges[i]);
            (*edge_count)++;
        }
    }

    /* Add edges in set2 but not in set1 */
    for (int i = 0; i < set2->num_edges; i++) {
        if (!edge_exists(set1, &set2->edges[i])) {
            *total_length += edge_length(&set2->edges[i]);
            (*edge_count)++;
        }
    }
}

/*
 * Compute L2 (Euclidean) distance between two edge sets
 *
 * For binary vectors: L2 = sqrt(|symmetric difference|)
 */
double edge_set_distance_l2(edge_set_t* set1, edge_set_t* set2) {
    double diff_count = edge_set_distance_l1(set1, set2);
    return sqrt(diff_count);
}

/*
 * Compute FST set distance - simple symmetric difference
 */
double fst_set_distance(int* fsts1, int num1, int* fsts2, int num2) {
    int diff_count = 0;

    /* Count FSTs in set1 but not in set2 */
    for (int i = 0; i < num1; i++) {
        int found = 0;
        for (int j = 0; j < num2; j++) {
            if (fsts1[i] == fsts2[j]) {
                found = 1;
                break;
            }
        }
        if (!found) {
            diff_count++;
        }
    }

    /* Count FSTs in set2 but not in set1 */
    for (int i = 0; i < num2; i++) {
        int found = 0;
        for (int j = 0; j < num1; j++) {
            if (fsts2[i] == fsts1[j]) {
                found = 1;
                break;
            }
        }
        if (!found) {
            diff_count++;
        }
    }

    return (double)diff_count;
}

/*
 * Compute topology distance between two solutions
 */
double compute_topology_distance(
    const char* fst_file,
    const char* solution_file_prev,
    const char* solution_file_curr,
    distance_method_t method
) {
    /* First iteration - no previous solution */
    if (!solution_file_prev) {
        return 0.0;
    }

    /* Parse selected FSTs from both solutions */
    int num_prev, num_curr;
    int* selected_prev = parse_selected_fsts(solution_file_prev, &num_prev);
    int* selected_curr = parse_selected_fsts(solution_file_curr, &num_curr);

    if (!selected_prev || !selected_curr) {
        fprintf(stderr, "Error: Failed to parse solution files\n");
        free(selected_prev);
        free(selected_curr);
        return -1.0;
    }

    double distance;

    /* Compute distance based on method */
    if (method == DISTANCE_FST_SET) {
        /* Simple FST set distance - no need to parse complex FST format */
        distance = fst_set_distance(selected_prev, num_prev, selected_curr, num_curr);
        free(selected_prev);
        free(selected_curr);
        return distance;
    }

    /* For edge-based methods, build edge sets */
    edge_set_t* set_prev = build_edge_set(fst_file, selected_prev, num_prev);
    edge_set_t* set_curr = build_edge_set(fst_file, selected_curr, num_curr);

    if (!set_prev || !set_curr) {
        fprintf(stderr, "Error: Failed to build edge sets\n");
        free_edge_set(set_prev);
        free_edge_set(set_curr);
        free(selected_prev);
        free(selected_curr);
        return -1.0;
    }

    /* Compute distance */
    switch (method) {
        case DISTANCE_L1_MANHATTAN:
            distance = edge_set_distance_l1(set_prev, set_curr);
            break;
        case DISTANCE_L2_EUCLIDEAN:
            distance = edge_set_distance_l2(set_prev, set_curr);
            break;
        default:
            fprintf(stderr, "Error: Unknown distance method %d\n", method);
            distance = -1.0;
    }

    /* Cleanup */
    free_edge_set(set_prev);
    free_edge_set(set_curr);
    free(selected_prev);
    free(selected_curr);

    return distance;
}

/*
 * Compute detailed topology distance metrics
 */
topology_distance_result_t compute_topology_distance_detailed(
    const char* fst_file,
    const char* solution_file_prev,
    const char* solution_file_curr
) {
    topology_distance_result_t result = {0.0, 0, 0};

    /* First iteration - no previous solution */
    if (!solution_file_prev) {
        return result;
    }

    /* Parse selected FSTs from both solutions */
    int num_prev, num_curr;
    int* selected_prev = parse_selected_fsts(solution_file_prev, &num_prev);
    int* selected_curr = parse_selected_fsts(solution_file_curr, &num_curr);

    if (!selected_prev || !selected_curr) {
        fprintf(stderr, "Error: Failed to parse solution files\n");
        free(selected_prev);
        free(selected_curr);
        return result;
    }

    /* Compute FST count difference */
    result.fst_count = (int)fst_set_distance(selected_prev, num_prev, selected_curr, num_curr);

    /* Build edge sets for detailed metrics */
    edge_set_t* set_prev = build_edge_set(fst_file, selected_prev, num_prev);
    edge_set_t* set_curr = build_edge_set(fst_file, selected_curr, num_curr);

    if (set_prev && set_curr) {
        /* Compute edge-level metrics */
        edge_set_distance_detailed(set_prev, set_curr, &result.edge_length, &result.edge_count);
    }

    /* Cleanup */
    free_edge_set(set_prev);
    free_edge_set(set_curr);
    free(selected_prev);
    free(selected_curr);

    return result;
}

/*
 * Free edge set memory
 */
void free_edge_set(edge_set_t* set) {
    if (set) {
        free(set->edges);
        free(set);
    }
}

/*
 * Print edge set for debugging
 */
void print_edge_set(edge_set_t* set) {
    printf("Edge set with %d edges:\n", set->num_edges);
    for (int i = 0; i < set->num_edges; i++) {
        printf("  Edge %d: (%.6f, %.6f) - (%.6f, %.6f)\n",
               i, set->edges[i].x1, set->edges[i].y1,
               set->edges[i].x2, set->edges[i].y2);
    }
}
