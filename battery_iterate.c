/***********************************************************************

	File:	battery_iterate.c
	Rev:	1.0
	Date:	10/18/2025

************************************************************************

	Phase 4.5: Battery Evolution via External Iteration

	This program implements iterative battery-aware optimization:
	1. Initialize battery levels
	2. Solve multi-period LP with current battery levels
	3. Extract coverage from solution
	4. Update battery levels based on coverage
	5. Repeat until convergence

	Usage:
		./battery_iterate -n NUM_TERMINALS -b BUDGET -t TIME_PERIODS -i MAX_ITERS

************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "src/logger.h"
#include "src/nn_infer.h"


#define MAX_TERMINALS 100
#define MAX_ITERS 50
#define CHARGE_RATE 15.0
#define DEMAND_RATE 5.0
#define CONVERGENCE_THRESHOLD 1.0  /* Max battery change for convergence */

typedef struct {
    int id;
    double x, y;
    double battery;
    int covered[20];  /* Coverage history over iterations */
} Terminal;

/* Function prototypes */
static void usage(void);
static void initialize_batteries(Terminal terminals[], int n, double initial_level);
static int solve_iteration(const char* fst_file, double budget, int time_periods,
                          Terminal terminals[], int n, int iteration);
static int parse_coverage_from_solution(const char* solution_file, Terminal terminals[],
                                       int n, int time_periods);
static void update_batteries(Terminal terminals[], int n, int time_periods);
static double check_convergence(Terminal old_batteries[], Terminal new_batteries[], int n);
static void write_battery_report(const char* output_file, Terminal terminals[],
                                 int n, int num_iterations);
static void print_iteration_summary(int iteration, Terminal terminals[], int n);

int main(int argc, char* argv[])
{
    int n_terminals = 0;
    double budget = 0.0;
    int time_periods = 3;
    int max_iterations = 10;
    char fst_file[256] = "";
    int opt;

    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "n:b:t:i:f:h")) != -1) {
        switch (opt) {
            case 'n':
                n_terminals = atoi(optarg);
                break;
            case 'b':
                budget = atof(optarg);
                break;
            case 't':
                time_periods = atoi(optarg);
                break;
            case 'i':
                max_iterations = atoi(optarg);
                break;
            case 'f':
                strncpy(fst_file, optarg, sizeof(fst_file) - 1);
                break;
            case 'h':
            default:
                usage();
                return 1;
        }
    }

    /* Validate inputs */
    if (n_terminals <= 0 || budget <= 0.0 || strlen(fst_file) == 0) {
        fprintf(stderr, "Error: Missing required arguments\n");
        usage();
        return 1;
    }

    printf("=== Phase 4.5: Battery Evolution via External Iteration ===\n");
	// Initialize logger and neural network
csv_logger_t* logger = csv_logger_open("battery_training_data.csv");

// Load neural network (expects 2 inputs → 1 output)
nn_model_t* nn = nn_load("model.onnx", 2, 1);

    printf("Terminals: %d\n", n_terminals);
    printf("Budget: %.2f\n", budget);
    printf("Time periods: %d\n", time_periods);
    printf("Max iterations: %d\n", max_iterations);
    printf("FST file: %s\n", fst_file);
    printf("\n");

    /* Initialize terminals with batteries */
    Terminal terminals[MAX_TERMINALS];
    initialize_batteries(terminals, n_terminals, 50.0);  /* Start at 50% */

    /* Iterative optimization loop */
    Terminal old_batteries[MAX_TERMINALS];
    double convergence = 999.0;
    int iteration;

    for (iteration = 0; iteration < max_iterations; iteration++) {
        printf("\n=== ITERATION %d ===\n", iteration + 1);

        /* Save old battery levels for convergence check */
        memcpy(old_batteries, terminals, n_terminals * sizeof(Terminal));

        /* Solve LP with current battery levels */
        if (solve_iteration(fst_file, budget, time_periods, terminals, n_terminals, iteration) != 0) {
            fprintf(stderr, "Error: Failed to solve iteration %d\n", iteration + 1);
            return 1;
        }

        /* Parse solution to get coverage */
        char solution_file[512];
        snprintf(solution_file, sizeof(solution_file), "battery_iter%d_solution.txt", iteration + 1);

        if (parse_coverage_from_solution(solution_file, terminals, n_terminals, time_periods) != 0) {
            fprintf(stderr, "Warning: Could not parse coverage from iteration %d\n", iteration + 1);
        }

        /* Update battery levels based on coverage */
        update_batteries(terminals, n_terminals, time_periods);
		// Log state of each terminal into CSV
for (int t = 0; t < n_terminals; t++) {
    csv_logger_write(
        logger,
        iteration,
        t,
        terminals[t].battery,
        terminals[t].covered[0]
    );
}

        /* Print iteration summary */
        print_iteration_summary(iteration + 1, terminals, n_terminals);
		

        /* Check convergence */
        convergence = check_convergence(old_batteries, terminals, n_terminals);
        printf("Convergence metric: %.4f\n", convergence);

        if (convergence < CONVERGENCE_THRESHOLD) {
            printf("\n*** CONVERGED after %d iterations ***\n", iteration + 1);
            break;
        }
    }

    /* Write final report */
    write_battery_report("battery_evolution_report.txt", terminals, n_terminals, iteration + 1);

    printf("\n=== Battery Evolution Complete ===\n");
    printf("Total iterations: %d\n", iteration + 1);
    printf("Final convergence: %.4f\n", convergence);
    printf("Report written to: battery_evolution_report.txt\n");

    return 0;
}

static void usage(void)
{
    printf("Usage: battery_iterate -n NUM_TERMINALS -b BUDGET -f FST_FILE [OPTIONS]\n");
    printf("\nRequired arguments:\n");
    printf("  -n NUM    Number of terminals\n");
    printf("  -b BUDGET Budget constraint (normalized)\n");
    printf("  -f FILE   FST input file\n");
    printf("\nOptional arguments:\n");
    printf("  -t NUM    Number of time periods (default: 3)\n");
    printf("  -i NUM    Maximum iterations (default: 10)\n");
    printf("  -h        Show this help\n");
    printf("\nExample:\n");
    printf("  ./battery_iterate -n 4 -b 1.8 -f test_4.fst -t 3 -i 10\n");
}

static void initialize_batteries(Terminal terminals[], int n, double initial_level)
{
    int i;

    printf("Initializing %d terminals with battery level %.1f%%\n", n, initial_level);

    for (i = 0; i < n; i++) {
        terminals[i].id = i;
        terminals[i].battery = initial_level;
        memset(terminals[i].covered, 0, sizeof(terminals[i].covered));
    }
}

static int solve_iteration(const char* fst_file, double budget, int time_periods,
                          Terminal terminals[], int n, int iteration)
{
    char command[1024];
    char solution_file[512];

    snprintf(solution_file, sizeof(solution_file), "battery_iter%d_solution.txt", iteration + 1);

    /* Build bb command with environment variables */
    snprintf(command, sizeof(command),
        "GEOSTEINER_BUDGET=%.2f GEOSTEINER_TIME_PERIODS=%d ./bb < %s > %s 2>&1",
        budget, time_periods, fst_file, solution_file);

    printf("Running: %s\n", command);

    int ret = system(command);
    if (ret != 0) {
        fprintf(stderr, "Error: Command failed with code %d\n", ret);
        return -1;
    }

    printf("Solution written to: %s\n", solution_file);
    return 0;
}

static int parse_coverage_from_solution(const char* solution_file, Terminal terminals[],
                                       int n, int time_periods)
{
    /* TODO: Parse solution file to extract which terminals were covered
     * For now, simulate: assume 60% of terminals are covered each iteration */
    int i;
    srand(time(NULL));

    for (i = 0; i < n; i++) {
        terminals[i].covered[0] = (rand() % 100) < 60 ? 1 : 0;  /* 60% coverage */
    }

    printf("Coverage parsed (simulated): %d/%d terminals covered\n",
           (int)(n * 0.6), n);
    return 0;
}

static void update_batteries(Terminal terminals[], int n, int time_periods)
{
    int i;

    printf("Updating battery levels...\n");

    for (i = 0; i < n; i++) {
        double old_battery = terminals[i].battery;

        if (terminals[i].covered[0]) {
            /* Covered: gain charge */
            terminals[i].battery += CHARGE_RATE;
        } else {
            /* Not covered: lose charge */
            terminals[i].battery -= DEMAND_RATE;
        }

        /* Clamp to [0, 100] */
        if (terminals[i].battery < 0.0) terminals[i].battery = 0.0;
        if (terminals[i].battery > 100.0) terminals[i].battery = 100.0;

        printf("  Terminal %d: %.1f%% -> %.1f%% (%s)\n",
               i, old_battery, terminals[i].battery,
               terminals[i].covered[0] ? "covered" : "uncovered");
    }
}

static double check_convergence(Terminal old_batteries[], Terminal new_batteries[], int n)
{
    double max_change = 0.0;
    int i;

    for (i = 0; i < n; i++) {
        double change = fabs(new_batteries[i].battery - old_batteries[i].battery);
        if (change > max_change) {
            max_change = change;
        }
    }

    return max_change;
}

static void print_iteration_summary(int iteration, Terminal terminals[], int n)
{
    int i;
    double avg_battery = 0.0;
    int num_covered = 0;

    printf("\n--- Iteration %d Summary ---\n", iteration);
    printf("Terminal  Battery   Status\n");
    printf("--------  --------  --------\n");

    for (i = 0; i < n; i++) {
        printf("   %2d     %6.1f%%   %s\n",
               i, terminals[i].battery,
               terminals[i].covered[0] ? "Covered" : "Uncovered");
        avg_battery += terminals[i].battery;
        if (terminals[i].covered[0]) num_covered++;
    }

    avg_battery /= n;
    printf("\nAverage battery: %.1f%%\n", avg_battery);
    printf("Coverage: %d/%d terminals (%.1f%%)\n", num_covered, n, 100.0 * num_covered / n);
}

static void write_battery_report(const char* output_file, Terminal terminals[],
                                 int n, int num_iterations)
{
    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: Could not write report to %s\n", output_file);
        return;
    }

    fprintf(fp, "=== Battery Evolution Report ===\n");
    fprintf(fp, "Total Iterations: %d\n", num_iterations);
    fprintf(fp, "Charge Rate: %.1f\n", CHARGE_RATE);
    fprintf(fp, "Demand Rate: %.1f\n", DEMAND_RATE);
    fprintf(fp, "\n");

    fprintf(fp, "Final Battery Levels:\n");
    fprintf(fp, "Terminal  Battery\n");
    fprintf(fp, "--------  --------\n");

    int i;
    double avg_battery = 0.0;
    for (i = 0; i < n; i++) {
        fprintf(fp, "   %2d     %6.1f%%\n", i, terminals[i].battery);
        avg_battery += terminals[i].battery;
    }

    avg_battery /= n;
    fprintf(fp, "\nAverage Final Battery: %.1f%%\n", avg_battery);

    fclose(fp);
}
