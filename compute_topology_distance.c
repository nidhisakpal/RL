/*
 * Standalone tool to compute topology distance between iterations
 *
 * Usage:
 *   ./compute_topology_distance [options] <fst_file> <solution_prev> <solution_curr>
 *   ./compute_topology_distance -m l1 fsts.txt solution1.txt solution2.txt
 *
 * Options:
 *   -m <method>   Distance method: l1 (default), l2
 *   -v            Verbose output
 *   -h            Help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "topology_distance.h"

static void print_usage(const char* prog_name) {
    printf("Usage: %s [options] <fst_file> <solution_prev> <solution_curr>\n", prog_name);
    printf("\n");
    printf("Compute topology distance between two network solutions.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -m <method>   Distance method:\n");
    printf("                  fst = FST set difference (default, counts changed FSTs)\n");
    printf("                  l1  = Manhattan distance on edge vectors (for linearization)\n");
    printf("                  l2  = Euclidean distance on edge vectors (L2 norm)\n");
    printf("  -D            Detailed output: edge_length (edge_count)\n");
    printf("  -v            Verbose output (show edge sets)\n");
    printf("  -h            Show this help message\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  fst_file        Path to FST file with candidate trees\n");
    printf("  solution_prev   Previous iteration solution (use 'NONE' for first iteration)\n");
    printf("  solution_curr   Current iteration solution\n");
    printf("\n");
    printf("Output:\n");
    printf("  Prints the topology distance value to stdout\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # First iteration (distance = 0)\n");
    printf("  %s fsts_iter1.txt NONE solution_iter1.txt\n", prog_name);
    printf("\n");
    printf("  # Compare iterations 1 and 2 with Manhattan distance\n");
    printf("  %s -m l1 fsts_iter2.txt solution_iter1.txt solution_iter2.txt\n", prog_name);
    printf("\n");
    printf("  # Compare with Euclidean distance\n");
    printf("  %s -m l2 fsts_iter2.txt solution_iter1.txt solution_iter2.txt\n", prog_name);
    printf("\n");
}

int main(int argc, char** argv) {
    distance_method_t method = DISTANCE_FST_SET;  /* Default to FST set distance */
    int verbose = 0;
    int detailed = 0;
    int opt;

    /* Parse options */
    while ((opt = getopt(argc, argv, "m:vhD")) != -1) {
        switch (opt) {
            case 'm':
                if (strcmp(optarg, "fst") == 0 || strcmp(optarg, "set") == 0) {
                    method = DISTANCE_FST_SET;
                } else if (strcmp(optarg, "l1") == 0 || strcmp(optarg, "manhattan") == 0) {
                    method = DISTANCE_L1_MANHATTAN;
                } else if (strcmp(optarg, "l2") == 0 || strcmp(optarg, "euclidean") == 0) {
                    method = DISTANCE_L2_EUCLIDEAN;
                } else {
                    fprintf(stderr, "Error: Unknown distance method '%s'\n", optarg);
                    fprintf(stderr, "Valid methods: fst, l1, l2\n");
                    return 1;
                }
                break;
            case 'D':
                detailed = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* Check arguments */
    if (argc - optind != 3) {
        fprintf(stderr, "Error: Expected 3 arguments, got %d\n", argc - optind);
        fprintf(stderr, "\n");
        print_usage(argv[0]);
        return 1;
    }

    const char* fst_file = argv[optind];
    const char* solution_prev = argv[optind + 1];
    const char* solution_curr = argv[optind + 2];

    /* Handle first iteration case */
    if (strcmp(solution_prev, "NONE") == 0) {
        if (verbose) {
            printf("First iteration - no previous solution to compare\n");
        }
        if (detailed) {
            printf("0.0 (0)\n");
        } else {
            printf("0.0\n");
        }
        return 0;
    }

    /* Detailed mode: compute edge length and edge count */
    if (detailed) {
        topology_distance_result_t result = compute_topology_distance_detailed(
            fst_file, solution_prev, solution_curr);

        if (verbose) {
            printf("Topology distance (detailed):\n");
            printf("  Edges changed: %d\n", result.edge_count);
            printf("  Total edge length: %.3f\n", result.edge_length);
            printf("  FSTs changed: %d\n", result.fst_count);
            printf("  Format: %d (%.3f)\n", result.edge_count, result.edge_length);
        } else {
            /* Output format: edge_count (edge_length) */
            printf("%d (%.3f)\n", result.edge_count, result.edge_length);
        }
        return 0;
    }

    /* Standard mode: compute single distance value */
    if (verbose) {
        const char* method_name;
        switch (method) {
            case DISTANCE_FST_SET:
                method_name = "FST Set Difference";
                break;
            case DISTANCE_L1_MANHATTAN:
                method_name = "L1 (Manhattan)";
                break;
            case DISTANCE_L2_EUCLIDEAN:
                method_name = "L2 (Euclidean)";
                break;
            default:
                method_name = "Unknown";
        }
        printf("Computing topology distance using %s\n", method_name);
        printf("  FST file: %s\n", fst_file);
        printf("  Previous solution: %s\n", solution_prev);
        printf("  Current solution: %s\n", solution_curr);
        printf("\n");
    }

    double distance = compute_topology_distance(fst_file, solution_prev, solution_curr, method);

    if (distance < 0) {
        fprintf(stderr, "Error: Failed to compute topology distance\n");
        return 1;
    }

    if (verbose) {
        printf("Topology distance: %.6f\n", distance);
    } else {
        printf("%.6f\n", distance);
    }

    return 0;
}
