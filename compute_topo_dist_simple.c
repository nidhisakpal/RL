/*
 * Simple topology distance computation from dumpfst files
 * Counts edges in selected FSTs and computes symmetric difference
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_LINE 1024
#define MAX_FSTS 1000
#define MAX_TERMINALS 100
#define MAX_EDGES 1000

typedef struct {
    int v1, v2;  /* Edge between terminals v1 and v2 */
} Edge;

typedef struct {
    int num_terminals;
    int terminals[10];  /* List of terminals in this FST */
    int num_edges;
    Edge edges[10];
} FST;

/* Parse terminals file to get coordinates */
typedef struct {
    double x, y;
} Point;

Point terminals[MAX_TERMINALS];
int num_terminals_global = 0;

/* Parse selected FST indices from solution file */
int parse_selected_fsts(const char* solution_file, int* selected, int max_fsts) {
    FILE* fp = fopen(solution_file, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open solution file %s\n", solution_file);
        return 0;
    }

    char line[MAX_LINE];
    int num_selected = 0;
    int in_lp_vars = 0;

    while (fgets(line, sizeof(line), fp) && num_selected < max_fsts) {
        if (strstr(line, "LP_VARS")) {
            in_lp_vars = 1;
        }
        if (in_lp_vars && strstr(line, "not_covered")) {
            break;
        }
        if (in_lp_vars && strstr(line, "x[")) {
            int fst_id;
            double value;
            if (sscanf(line, "%*[^x]x[%d] = %lf", &fst_id, &value) == 2) {
                if (value >= 0.5) {
                    selected[num_selected++] = fst_id;
                }
            }
        }
    }

    fclose(fp);
    return num_selected;
}

/* Parse FST definitions from dumpfst file */
int parse_fst_definitions(const char* dump_file, FST* fsts, int max_fsts) {
    FILE* fp = fopen(dump_file, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open dump file %s\n", dump_file);
        return 0;
    }

    char line[MAX_LINE];
    int fst_count = 0;

    while (fgets(line, sizeof(line), fp) && fst_count < max_fsts) {
        int terms[10];
        int n = 0;

        /* Parse terminal list from line */
        char* token = strtok(line, " \t\n");
        while (token && n < 10) {
            terms[n++] = atoi(token);
            token = strtok(NULL, " \t\n");
        }

        if (n >= 2) {
            /* This is an FST connecting n terminals */
            fsts[fst_count].num_terminals = n;
            for (int i = 0; i < n; i++) {
                fsts[fst_count].terminals[i] = terms[i];
            }

            /* For a tree connecting n terminals, we need n-1 edges */
            /* Create edges as a spanning tree: connect each terminal to the first one */
            fsts[fst_count].num_edges = n - 1;
            for (int i = 1; i < n; i++) {
                int v1 = terms[0];
                int v2 = terms[i];
                /* Canonicalize: smaller index first */
                if (v1 > v2) {
                    int tmp = v1;
                    v1 = v2;
                    v2 = tmp;
                }
                fsts[fst_count].edges[i-1].v1 = v1;
                fsts[fst_count].edges[i-1].v2 = v2;
            }

            fst_count++;
        }
    }

    fclose(fp);
    return fst_count;
}

/* Parse terminals file */
int parse_terminals_file(const char* terminals_file) {
    FILE* fp = fopen(terminals_file, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open terminals file %s\n", terminals_file);
        return 0;
    }

    int count = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp) && count < MAX_TERMINALS) {
        double x, y, battery;
        if (sscanf(line, "%lf %lf %lf", &x, &y, &battery) >= 2) {
            terminals[count].x = x;
            terminals[count].y = y;
            count++;
        }
    }

    fclose(fp);
    return count;
}

/* Check if two edges are equal */
int edges_equal(Edge* e1, Edge* e2) {
    return (e1->v1 == e2->v1 && e1->v2 == e2->v2);
}

/* Compute edge length */
double edge_length(Edge* e) {
    if (e->v1 >= num_terminals_global || e->v2 >= num_terminals_global) {
        return 0.0;
    }
    double dx = terminals[e->v2].x - terminals[e->v1].x;
    double dy = terminals[e->v2].y - terminals[e->v1].y;
    return sqrt(dx * dx + dy * dy);
}

/* Build edge set from selected FSTs */
int build_edge_set(FST* all_fsts, int num_fsts, int* selected, int num_selected,
                   Edge* edge_set) {
    int edge_count = 0;

    for (int i = 0; i < num_selected; i++) {
        int fst_id = selected[i];
        if (fst_id >= num_fsts) continue;

        FST* fst = &all_fsts[fst_id];

        /* Add all edges from this FST */
        for (int j = 0; j < fst->num_edges; j++) {
            /* Check if edge already exists (avoid duplicates) */
            int exists = 0;
            for (int k = 0; k < edge_count; k++) {
                if (edges_equal(&edge_set[k], &fst->edges[j])) {
                    exists = 1;
                    break;
                }
            }

            if (!exists && edge_count < MAX_EDGES) {
                edge_set[edge_count++] = fst->edges[j];
            }
        }
    }

    return edge_count;
}

int main(int argc, char** argv) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <dump_prev> <dump_curr> <sol_prev> <sol_curr> <terminals>\n", argv[0]);
        fprintf(stderr, "       Use 'NONE' for first iteration\n");
        return 1;
    }

    const char* dump_prev = argv[1];
    const char* dump_curr = argv[2];
    const char* sol_prev = argv[3];
    const char* sol_curr = argv[4];
    const char* terminals_file = argv[5];

    /* Handle first iteration */
    if (strcmp(dump_prev, "NONE") == 0 || strcmp(sol_prev, "NONE") == 0) {
        printf("0 (0.000)\n");
        return 0;
    }

    /* Parse terminals for edge length computation */
    num_terminals_global = parse_terminals_file(terminals_file);

    /* Parse FST definitions from dump files */
    FST fsts_prev[MAX_FSTS], fsts_curr[MAX_FSTS];
    int num_fsts_prev = parse_fst_definitions(dump_prev, fsts_prev, MAX_FSTS);
    int num_fsts_curr = parse_fst_definitions(dump_curr, fsts_curr, MAX_FSTS);

    if (num_fsts_prev == 0 || num_fsts_curr == 0) {
        fprintf(stderr, "Error: Failed to parse FST definitions\n");
        printf("0 (0.000)\n");
        return 1;
    }

    /* Parse selected FSTs from solution files */
    int selected_prev[MAX_FSTS], selected_curr[MAX_FSTS];
    int num_sel_prev = parse_selected_fsts(sol_prev, selected_prev, MAX_FSTS);
    int num_sel_curr = parse_selected_fsts(sol_curr, selected_curr, MAX_FSTS);

    if (num_sel_prev == 0 || num_sel_curr == 0) {
        fprintf(stderr, "Error: Failed to parse selected FSTs\n");
        printf("0 (0.000)\n");
        return 1;
    }

    /* Build edge sets from selected FSTs */
    Edge edges_prev[MAX_EDGES], edges_curr[MAX_EDGES];
    int num_edges_prev = build_edge_set(fsts_prev, num_fsts_prev, selected_prev, num_sel_prev, edges_prev);
    int num_edges_curr = build_edge_set(fsts_curr, num_fsts_curr, selected_curr, num_sel_curr, edges_curr);

    /* Compute symmetric difference */
    int edge_count = 0;
    double total_length = 0.0;

    /* Edges in prev but not in curr */
    for (int i = 0; i < num_edges_prev; i++) {
        int found = 0;
        for (int j = 0; j < num_edges_curr; j++) {
            if (edges_equal(&edges_prev[i], &edges_curr[j])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            edge_count++;
            total_length += edge_length(&edges_prev[i]);
        }
    }

    /* Edges in curr but not in prev */
    for (int i = 0; i < num_edges_curr; i++) {
        int found = 0;
        for (int j = 0; j < num_edges_prev; j++) {
            if (edges_equal(&edges_curr[i], &edges_prev[j])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            edge_count++;
            total_length += edge_length(&edges_curr[i]);
        }
    }

    /* Output: edge_count (edge_length) */
    printf("%d (%.3f)\n", edge_count, total_length);

    return 0;
}
