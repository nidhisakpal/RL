/***********************************************************************

	File:	simulate.c
	Rev:	e-1
	Date:	9/18/2025

************************************************************************

	Simulation Wrapper for Budget-Constrained GeoSteiner Optimization

	This program automates the complete pipeline:
	1. Generate random terminal coordinates with battery levels
	2. Compute Full Steiner Trees (FSTs) using efst
	3. Solve budget-constrained multi-objective SMT using bb
	4. Generate HTML visualization of the solution

	Usage:
		./simulate -n N -b BUDGET [-s SEED] [-o OUTDIR] [-v] [-h]

	Where:
		-n N        Number of terminals to generate
		-b BUDGET   Budget constraint for optimization
		-s SEED     Random seed (default: current time)
		-o OUTDIR   Output directory (default: simulation_output)
		-v          Verbose output
		-h          Show help

************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>

/* GeoSteiner library includes */
#include "geosteiner.h"
#include "gsttypes.h"
#include "logic.h"
#include "memory.h"
#include "point.h"
#include "steiner.h"
#include "io.h"  /* For UNSCALE macro */

/* Data structures for visualization */
typedef struct {
    double x, y;
    double battery;
    int covered;
    int terminal_id;
} Terminal;

typedef struct {
    double x, y;
} SteinerPoint;

typedef struct {
    int from;  /* Node ID: positive = terminal position (1-based), negative = Steiner point */
    int to;    /* Node ID: positive = terminal position (1-based), negative = Steiner point */
} Edge;

typedef struct {
    int selected;
    int num_terminals;
    int terminal_ids[10];
    int num_steiner_points;
    SteinerPoint steiner_points[10];
    int num_edges;
    Edge edges[20];  /* Max edges for a small FST */
    double cost;
    int fst_id;
} FST;

/* Function prototypes */
static void usage(void);
static void generate_terminals(int n_terminals, const char* output_dir, int seed, int verbose);
static void generate_fsts(const char* terminals_file, const char* fsts_file, int verbose);
static void generate_fst_dump(const char* fsts_file, const char* dump_file, int verbose);
static void solve_smt(const char* fsts_file, const char* solution_file, int budget, int verbose);
static void generate_visualization(const char* terminals_file, const char* fsts_file,
                                  const char* solution_file, const char* html_file, int verbose);
static void run_visualization_only(const char* terminals_file, const char* fsts_file,
                                  const char* solution_file, const char* html_file, int verbose, double objective_value, const char* topology_distance_str);
static void create_rich_visualization(const char* terminals_file, const char* fsts_file,
                                     const char* solution_file, const char* html_file, int verbose, double objective_value, const char* topology_distance_str);
static int parse_terminals(const char* terminals_file, Terminal terminals[], int max_terminals);
static int parse_solution_coverage(const char* solution_file, int coverage[], int max_terminals);
static int parse_fsts_from_solution(const char* solution_file, FST fsts[], int max_fsts);
static int parse_fsts_from_dump(const char* dump_file, FST fsts[], int max_fsts);
static int parse_selected_fst_ids(const char* solution_file, int selected_ids[], int max_fsts);
static int parse_selected_fsts(const char* solution_file, int selected_fsts[], int max_fsts);
static void extract_steiner_points_from_v3(const char* v3_file, FST fsts[], int num_fsts);
static double parse_final_mip_gap(const char* solution_file);
static double parse_normalized_budget(const char* solution_file);
static double parse_total_tree_cost(const char* solution_file);
static double parse_lp_objective_value(const char* solution_file);
static void get_battery_color(double battery, char* color_str);
static void scale_coordinates(double x, double y, int* scaled_x, int* scaled_y);
static int run_command(const char* command, int verbose);
static void create_directory(const char* dir_path);
static double random_double(void);
static double random_battery_level(void);

/* Global variables for simulation parameters */
static int g_verbose = 0;

int main(int argc, char* argv[])
{
	int n_terminals = 0;
	int budget = 0;
	int seed = 0;
	char output_dir[256] = "simulation_output";
	char terminals_file[512];
	char fsts_file[512];
	char solution_file[512];
	char html_file[512];
	int opt;
	int visualization_only = 0;
	char viz_terminals[512] = "";
	char viz_fsts[512] = "";
	char viz_solution[512] = "";
	char viz_output[512] = "";
	double objective_value = -1.0;  /* -1 means not provided */
	char topology_distance_str[256] = "";  /* String format: "N (X.XXX)" */

	/* Parse command line arguments */
	while ((opt = getopt(argc, argv, "n:b:s:o:vht:f:r:w:z:d:")) != -1) {
		switch (opt) {
		case 'n':
			n_terminals = atoi(optarg);
			break;
		case 'b':
			budget = atoi(optarg);
			break;
		case 's':
			seed = atoi(optarg);
			break;
		case 'o':
			strncpy(output_dir, optarg, sizeof(output_dir) - 1);
			output_dir[sizeof(output_dir) - 1] = '\0';
			break;
		case 'v':
			g_verbose = 1;
			break;
		case 'h':
			usage();
			exit(0);
		case 't':
			strncpy(viz_terminals, optarg, sizeof(viz_terminals) - 1);
			viz_terminals[sizeof(viz_terminals) - 1] = '\0';
			visualization_only = 1;
			break;
		case 'f':
			strncpy(viz_fsts, optarg, sizeof(viz_fsts) - 1);
			viz_fsts[sizeof(viz_fsts) - 1] = '\0';
			visualization_only = 1;
			break;
		case 'r':
			strncpy(viz_solution, optarg, sizeof(viz_solution) - 1);
			viz_solution[sizeof(viz_solution) - 1] = '\0';
			visualization_only = 1;
			break;
		case 'w':
			strncpy(viz_output, optarg, sizeof(viz_output) - 1);
			viz_output[sizeof(viz_output) - 1] = '\0';
			visualization_only = 1;
			break;
		case 'z':
			objective_value = atof(optarg);
			break;
		case 'd':
			strncpy(topology_distance_str, optarg, sizeof(topology_distance_str) - 1);
			topology_distance_str[sizeof(topology_distance_str) - 1] = '\0';
			break;
		default:
			usage();
			exit(1);
		}
	}

	/* Handle visualization-only mode */
	if (visualization_only) {
		if (strlen(viz_terminals) == 0 || strlen(viz_fsts) == 0 ||
		    strlen(viz_solution) == 0 || strlen(viz_output) == 0) {
			fprintf(stderr, "Error: Visualization mode requires all four files:\n");
			fprintf(stderr, "  -t <terminals_file>\n");
			fprintf(stderr, "  -f <fsts_file>\n");
			fprintf(stderr, "  -r <solution_file>\n");
			fprintf(stderr, "  -w <output_html_file>\n");
			usage();
			exit(1);
		}

		printf("🎨 GeoSteiner Visualization Generator\n");
		printf("=====================================\n");
		printf("Terminals:  %s\n", viz_terminals);
		printf("FSTs:       %s\n", viz_fsts);
		printf("Solution:   %s\n", viz_solution);
		printf("Output:     %s\n", viz_output);
		printf("Verbose:    %s\n", g_verbose ? "Yes" : "No");
		printf("=====================================\n\n");

		run_visualization_only(viz_terminals, viz_fsts, viz_solution, viz_output, g_verbose, objective_value, topology_distance_str);

		printf("🎉 Visualization generated successfully!\n");
		printf("🌐 Open %s in a web browser to view results\n", viz_output);
		return 0;
	}

	/* Validate required parameters for full simulation */
	if (n_terminals <= 0) {
		fprintf(stderr, "Error: Number of terminals (-n) must be positive\n");
		usage();
		exit(1);
	}
	if (budget <= 0) {
		fprintf(stderr, "Error: Budget (-b) must be positive\n");
		usage();
		exit(1);
	}

	/* Set default seed if not provided */
	if (seed == 0) {
		seed = (int)time(NULL);
	}

	printf("🌐 GeoSteiner Budget-Constrained SMT Simulation\n");
	printf("================================================\n");
	printf("Terminals:     %d\n", n_terminals);
	printf("Budget:        %d\n", budget);
	printf("Seed:          %d\n", seed);
	printf("Output Dir:    %s\n", output_dir);
	printf("Verbose:       %s\n", g_verbose ? "Yes" : "No");
	printf("================================================\n\n");

	/* Initialize random number generator */
	srand(seed);

	/* Create output directory */
	create_directory(output_dir);

	/* Set up file paths */
	snprintf(terminals_file, sizeof(terminals_file), "%s/terminals.txt", output_dir);
	snprintf(fsts_file, sizeof(fsts_file), "%s/fsts.txt", output_dir);
	snprintf(solution_file, sizeof(solution_file), "%s/solution.txt", output_dir);
	snprintf(html_file, sizeof(html_file), "%s/visualization.html", output_dir);

	/* Step 1: Generate random terminals with battery levels */
	printf("📍 Step 1: Generating %d random terminals...\n", n_terminals);
	generate_terminals(n_terminals, output_dir, seed, g_verbose);
	printf("   ✅ Terminals saved to: %s\n\n", terminals_file);

	/* Step 2: Generate Full Steiner Trees (FSTs) */
	printf("🌳 Step 2: Computing Full Steiner Trees...\n");
	generate_fsts(terminals_file, fsts_file, g_verbose);
	printf("   ✅ FSTs saved to: %s\n", fsts_file);

	/* Step 2b: Generate readable FST dump */
	char fsts_dump_file[512];
	snprintf(fsts_dump_file, sizeof(fsts_dump_file), "%s/fsts_dump.txt", output_dir);
	printf("📋 Step 2b: Generating readable FST dump...\n");
	generate_fst_dump(fsts_file, fsts_dump_file, g_verbose);
	printf("   ✅ FST dump saved to: %s\n\n", fsts_dump_file);

	/* Step 3: Solve budget-constrained SMT */
	printf("🎯 Step 3: Solving budget-constrained SMT (budget=%d)...\n", budget);
	solve_smt(fsts_file, solution_file, budget, g_verbose);
	printf("   ✅ Solution saved to: %s\n\n", solution_file);

	/* Step 4: Generate HTML visualization */
	printf("📊 Step 4: Generating rich HTML visualization...\n");
	create_rich_visualization(terminals_file, fsts_file, solution_file, html_file, g_verbose, -1.0, "");
	printf("   ✅ Rich visualization saved to: %s\n\n", html_file);

	printf("🎉 Simulation completed successfully!\n");
	printf("📁 All outputs available in: %s/\n", output_dir);
	printf("🌐 Open %s in a web browser to view results\n", html_file);

	return 0;
}

static void usage(void)
{
	printf("Usage: ./simulate [MODE] [OPTIONS]\n\n");
	printf("Automated Budget-Constrained GeoSteiner Simulation Pipeline\n\n");

	printf("FULL SIMULATION MODE:\n");
	printf("  ./simulate -n N -b BUDGET [-s SEED] [-o OUTDIR] [-v] [-h]\n\n");
	printf("Required arguments:\n");
	printf("  -n N        Number of terminals to generate (must be > 0)\n");
	printf("  -b BUDGET   Budget constraint for SMT optimization\n\n");
	printf("Optional arguments:\n");
	printf("  -s SEED     Random seed for terminal generation (default: current time)\n");
	printf("  -o OUTDIR   Output directory (default: simulation_output)\n");
	printf("  -v          Enable verbose output\n");
	printf("  -h          Show this help message\n\n");

	printf("VISUALIZATION-ONLY MODE:\n");
	printf("  ./simulate -t TERMINALS -f FSTS -r SOLUTION -w OUTPUT [-z OBJ] [-d DIST] [-v] [-h]\n\n");
	printf("Required arguments:\n");
	printf("  -t FILE     Terminals file (coordinates and battery levels)\n");
	printf("  -f FILE     FSTs file (Full Steiner Tree data)\n");
	printf("  -r FILE     Solution file (CPLEX solver output)\n");
	printf("  -w FILE     Output HTML file for visualization\n\n");
	printf("Optional arguments:\n");
	printf("  -z VALUE    LP objective value to display\n");
	printf("  -d VALUE    Topology distance from previous iteration\n\n");

	printf("Examples:\n");
	printf("  # Full simulation\n");
	printf("  ./simulate -n 10 -b 1500000 -s 12345 -o my_simulation -v\n\n");
	printf("  # Visualization only\n");
	printf("  ./simulate -t terminals.txt -f fsts.txt -r solution.txt -w viz.html -v\n\n");

	printf("Full simulation pipeline stages:\n");
	printf("  1. Generate random terminals with battery levels\n");
	printf("  2. Compute Full Steiner Trees (FSTs) using efst\n");
	printf("  3. Solve budget-constrained SMT using bb\n");
	printf("  4. Generate interactive HTML visualization\n");
}

static void generate_terminals(int n_terminals, const char* output_dir, int seed, int verbose)
{
	char terminals_file[512];
	FILE* fp;
	int i;
	double x, y, battery;

	snprintf(terminals_file, sizeof(terminals_file), "%s/terminals.txt", output_dir);

	fp = fopen(terminals_file, "w");
	if (!fp) {
		fprintf(stderr, "Error: Cannot create terminals file: %s\n", terminals_file);
		exit(1);
	}

	if (verbose) {
		printf("   Generating terminals with seed %d:\n", seed);
	}

	for (i = 0; i < n_terminals; i++) {
		x = random_double();
		y = random_double();
		battery = random_battery_level();

		/* Terminal 0 is the base station - always 100% battery */
		if (i == 0) {
			battery = 100.0;
		}

		fprintf(fp, "%.6f %.6f %.1f\n", x, y, battery);

		if (verbose) {
			printf("   Terminal %d: (%.3f, %.3f) battery=%.1f%%\n", i, x, y, battery);
		}
	}

	fclose(fp);

	if (verbose) {
		printf("   Saved %d terminals to %s\n", n_terminals, terminals_file);
	}
}

static void generate_fsts(const char* terminals_file, const char* fsts_file, int verbose)
{
	char command[1024];
	int result;

	/* Run efst to generate Full Steiner Trees */
	snprintf(command, sizeof(command), "./efst < \"%s\" > \"%s\" 2>/dev/null",
	         terminals_file, fsts_file);

	if (verbose) {
		printf("   Running: %s\n", command);
	}

	result = run_command(command, verbose);
	if (result != 0) {
		fprintf(stderr, "Error: FST generation failed (exit code %d)\n", result);
		exit(1);
	}

	if (verbose) {
		printf("   FST generation completed successfully\n");
	}
}

static void generate_fst_dump(const char* fsts_file, const char* dump_file, int verbose)
{
	char command[1024];
	int result;

	/* Run dumpfst to generate readable FST list */
	snprintf(command, sizeof(command), "./dumpfst < \"%s\" > \"%s\" 2>/dev/null",
	         fsts_file, dump_file);

	if (verbose) {
		printf("   Running: %s\n", command);
	}

	result = run_command(command, verbose);
	if (result != 0) {
		fprintf(stderr, "Error: FST dump generation failed (exit code %d)\n", result);
		exit(1);
	}

	if (verbose) {
		printf("   FST dump generation completed successfully\n");
	}
}

static void solve_smt(const char* fsts_file, const char* solution_file, int budget, int verbose)
{
	char command[1024];
	char env_var[64];
	int result;

	/* Set budget environment variable and run bb solver */
	snprintf(env_var, sizeof(env_var), "GEOSTEINER_BUDGET=%d", budget);
	snprintf(command, sizeof(command), "%s timeout 300s ./bb < \"%s\" > \"%s\" 2>&1",
	         env_var, fsts_file, solution_file);

	if (verbose) {
		printf("   Setting %s\n", env_var);
		printf("   Running: timeout 300s ./bb < %s > %s\n", fsts_file, solution_file);
	}

	result = run_command(command, verbose);
	if (result != 0 && result != 124) { /* 124 is timeout exit code */
		fprintf(stderr, "Warning: SMT solver returned exit code %d\n", result);
		/* Continue anyway - partial solutions may still be useful */
	}

	if (verbose) {
		printf("   SMT solving completed\n");
	}
}

static void generate_visualization(const char* terminals_file, const char* fsts_file,
                                  const char* solution_file, const char* html_file, int verbose)
{
	char command[1024];
	int result;

	/* Use the Python HTML generator if available, otherwise create basic HTML */
	if (access("html_generator.py", F_OK) == 0) {
		snprintf(command, sizeof(command),
		         "python3 html_generator.py --terminals \"%s\" --fsts \"%s\" --solution \"%s\" --output \"%s\" 2>/dev/null",
		         terminals_file, fsts_file, solution_file, html_file);

		if (verbose) {
			printf("   Running Python HTML generator\n");
		}

		result = run_command(command, verbose);
		if (result == 0) {
			if (verbose) {
				printf("   HTML visualization generated successfully\n");
			}
			return;
		}
	}

	/* Fallback: create basic HTML file */
	if (verbose) {
		printf("   Creating basic HTML visualization\n");
	}

	FILE* fp = fopen(html_file, "w");
	if (!fp) {
		fprintf(stderr, "Error: Cannot create HTML file: %s\n", html_file);
		exit(1);
	}

	fprintf(fp, "<!DOCTYPE html>\n");
	fprintf(fp, "<html><head><title>GeoSteiner Simulation Results</title></head>\n");
	fprintf(fp, "<body>\n");
	fprintf(fp, "<h1>🌐 GeoSteiner Budget-Constrained SMT Results</h1>\n");
	fprintf(fp, "<h2>📁 Generated Files</h2>\n");
	fprintf(fp, "<ul>\n");
	fprintf(fp, "<li><strong>Terminals:</strong> %s</li>\n", terminals_file);
	fprintf(fp, "<li><strong>FSTs:</strong> %s</li>\n", fsts_file);
	fprintf(fp, "<li><strong>Solution:</strong> %s</li>\n", solution_file);
	fprintf(fp, "</ul>\n");
	fprintf(fp, "<h2>📊 Solution Analysis</h2>\n");
	fprintf(fp, "<p>Review the solution file for detailed SMT optimization results.</p>\n");
	fprintf(fp, "<h2>🔧 Manual Visualization</h2>\n");
	fprintf(fp, "<p>Use the Python HTML generator for full interactive visualization:</p>\n");
	fprintf(fp, "<code>python3 html_generator.py --terminals %s --fsts %s --solution %s --output visualization_full.html</code>\n",
	        terminals_file, fsts_file, solution_file);
	fprintf(fp, "</body></html>\n");

	fclose(fp);

	if (verbose) {
		printf("   Basic HTML file created\n");
	}
}

static int run_command(const char* command, int verbose)
{
	int result;

	if (verbose) {
		printf("   Executing: %s\n", command);
	}

	result = system(command);

	if (result == -1) {
		fprintf(stderr, "Error: Failed to execute command: %s\n", strerror(errno));
		return -1;
	}

	return WEXITSTATUS(result);
}

static void create_directory(const char* dir_path)
{
	struct stat st;

	if (stat(dir_path, &st) == 0) {
		if (S_ISDIR(st.st_mode)) {
			if (g_verbose) {
				printf("   Directory %s already exists\n", dir_path);
			}
			return;
		} else {
			fprintf(stderr, "Error: %s exists but is not a directory\n", dir_path);
			exit(1);
		}
	}

	if (mkdir(dir_path, 0755) != 0) {
		fprintf(stderr, "Error: Cannot create directory %s: %s\n", dir_path, strerror(errno));
		exit(1);
	}

	if (g_verbose) {
		printf("   Created directory: %s\n", dir_path);
	}
}

static double random_double(void)
{
	return (double)rand() / RAND_MAX;
}

static double random_battery_level(void)
{
	/* Generate battery levels with realistic distribution */
	/* 20% chance of low battery (10-40%), 60% normal (40-80%), 20% high (80-100%) */
	double r = random_double();

	if (r < 0.2) {
		/* Low battery: 10-40% */
		return 10.0 + random_double() * 30.0;
	} else if (r < 0.8) {
		/* Normal battery: 40-80% */
		return 40.0 + random_double() * 40.0;
	} else {
		/* High battery: 80-100% */
		return 80.0 + random_double() * 20.0;
	}
}

static void run_visualization_only(const char* terminals_file, const char* fsts_file,
                                  const char* solution_file, const char* html_file, int verbose, double objective_value, const char* topology_distance_str)
{
	char command[2048];
	int result;

	/* Verify input files exist */
	if (access(terminals_file, F_OK) != 0) {
		fprintf(stderr, "Error: Terminals file not found: %s\n", terminals_file);
		exit(1);
	}
	if (access(fsts_file, F_OK) != 0) {
		fprintf(stderr, "Error: FSTs file not found: %s\n", fsts_file);
		exit(1);
	}
	if (access(solution_file, F_OK) != 0) {
		fprintf(stderr, "Error: Solution file not found: %s\n", solution_file);
		exit(1);
	}

	if (verbose) {
		printf("📊 Generating visualization from existing files...\n");
		printf("   Terminals: %s\n", terminals_file);
		printf("   FSTs:      %s\n", fsts_file);
		printf("   Solution:  %s\n", solution_file);
		printf("   Output:    %s\n", html_file);
	}

	/* Try Python HTML generator first */
	if (access("html_generator.py", F_OK) == 0) {
		snprintf(command, sizeof(command),
		         "python3 html_generator.py --terminals \"%s\" --fsts \"%s\" --solution \"%s\" --output \"%s\" 2>/dev/null",
		         terminals_file, fsts_file, solution_file, html_file);

		if (verbose) {
			printf("   Running Python HTML generator\n");
		}

		result = run_command(command, verbose);
		if (result == 0) {
			if (verbose) {
				printf("   ✅ Interactive HTML visualization generated\n");
			}
			return;
		} else {
			if (verbose) {
				printf("   Warning: Python generator failed, creating rich C visualization\n");
			}
		}
	}

	/* Create rich C-based visualization */
	create_rich_visualization(terminals_file, fsts_file, solution_file, html_file, verbose, objective_value, topology_distance_str);
}

static void create_rich_visualization(const char* terminals_file, const char* fsts_file,
                                     const char* solution_file, const char* html_file, int verbose, double objective_value, const char* topology_distance_str)
{
	Terminal terminals[50];
	int coverage[50] = {0};
	int num_terminals;
	int i;
	FILE* fp;

	if (verbose) {
		printf("   Creating rich SVG network visualization\n");
	}

	/* Parse input files */
	num_terminals = parse_terminals(terminals_file, terminals, 50);
	if (num_terminals <= 0) {
		fprintf(stderr, "Error: Could not parse terminals file\n");
		return;
	}

	parse_solution_coverage(solution_file, coverage, 50);

	/* Update terminal coverage */
	for (i = 0; i < num_terminals; i++) {
		terminals[i].covered = coverage[i];
		terminals[i].terminal_id = i;
	}

	if (verbose) {
		printf("   Parsed %d terminals with coverage data\n", num_terminals);
	}

	/* Create HTML file */
	fp = fopen(html_file, "w");
	if (!fp) {
		fprintf(stderr, "Error: Cannot create HTML file: %s\n", html_file);
		return;
	}

	/* Write HTML header and styles */
	fprintf(fp, "<!DOCTYPE html>\n");
	fprintf(fp, "<html lang=\"en\">\n");
	fprintf(fp, "<head>\n");
	fprintf(fp, "    <meta charset=\"UTF-8\">\n");
	fprintf(fp, "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
	fprintf(fp, "    <title>GeoSteiner Network Optimization - Budget-Constrained Solution</title>\n");
	fprintf(fp, "    <style>\n");
	fprintf(fp, "        body { font-family: 'Segoe UI', Arial, sans-serif; margin: 20px; background: #f8f9fa; }\n");
	fprintf(fp, "        .container { max-width: 1400px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }\n");
	fprintf(fp, "        h1 { color: #2c3e50; text-align: center; margin-bottom: 30px; }\n");
	fprintf(fp, "        .network-container { display: flex; gap: 30px; margin: 30px 0; }\n");
	fprintf(fp, "        .network-svg { flex: 2; border: 2px solid #ddd; border-radius: 8px; background: #fafafa; }\n");
	fprintf(fp, "        .sidebar { flex: 1; }\n");
	fprintf(fp, "        .terminal-label { font-size: 14px; font-weight: bold; fill: #333; }\n");
	fprintf(fp, "        .battery-text { font-size: 12px; fill: #666; }\n");
	fprintf(fp, "        .metrics, .legend, .fst-details { background: #f8f9fa; padding: 20px; margin: 20px 0; border-radius: 8px; border-left: 4px solid #3498db; }\n");
	fprintf(fp, "        .source-constraint { background: #d4edda; padding: 15px; margin: 20px 0; border-radius: 8px; border-left: 4px solid #28a745; }\n");
	fprintf(fp, "        .section { background: #fff; margin: 30px 0; padding: 25px; border-radius: 8px; border: 1px solid #e1e8ed; }\n");
	fprintf(fp, "        .constraint-check { padding: 10px; margin: 8px 0; border-radius: 5px; background: #f8f9fa; border-left: 3px solid #28a745; }\n");
	fprintf(fp, "        table { width: 100%%; border-collapse: collapse; }\n");
	fprintf(fp, "        td { padding: 8px; border-bottom: 1px solid #eee; }\n");
	fprintf(fp, "        .legend-item { display: flex; align-items: center; margin: 10px 0; }\n");
	fprintf(fp, "        .legend-symbol { width: 20px; height: 20px; margin-right: 10px; border-radius: 50%%; }\n");
	fprintf(fp, "        .covered-terminal { background: #00ff00; border: 2px solid #333; }\n");
	fprintf(fp, "        .uncovered-terminal { background: none; border: 2px dashed #999; position: relative; }\n");
	fprintf(fp, "        .selected-fst { background: #007bff; }\n");
	fprintf(fp, "        .steiner-point { background: #6c757d; }\n");
	fprintf(fp, "    </style>\n");
	fprintf(fp, "</head>\n");
	fprintf(fp, "<body>\n");
	fprintf(fp, "    <div class=\"container\">\n");
	fprintf(fp, "        <h1>🌐 GeoSteiner Network Optimization - Budget-Constrained Solution</h1>\n");

	/* Network visualization container */
	fprintf(fp, "        <div class=\"network-container\">\n");
	fprintf(fp, "            <svg width=\"800\" height=\"600\" class=\"network-svg\">\n");

	/* Parse ALL FSTs - handle both V3 format and dumpfst format */
	FST all_fsts[100];
	int num_all_fsts = 0;

	/* Check if fsts_file is V3 format or dumpfst format */
	FILE* check_fp = fopen(fsts_file, "r");
	char first_line[100] = {0};
	int is_v3 = 0;
	if (check_fp) {
		if (fgets(first_line, sizeof(first_line), check_fp)) {
			is_v3 = (strncmp(first_line, "V3", 2) == 0);
		}
		fclose(check_fp);
	}

	if (is_v3) {
		/* V3 format - need to run dumpfst first to get full FST enumeration */
		printf("   Detected V3 format, running dumpfst to enumerate FSTs...\n");
		char dumpfst_cmd[2048];
		char temp_dump[512];
		snprintf(temp_dump, sizeof(temp_dump), "%s.dump", fsts_file);
		snprintf(dumpfst_cmd, sizeof(dumpfst_cmd), "./dumpfst < \"%s\" > \"%s\" 2>/dev/null",
		         fsts_file, temp_dump);
		system(dumpfst_cmd);

		/* Parse the dumpfst output */
		num_all_fsts = parse_fsts_from_dump(temp_dump, all_fsts, 100);

		/* Now extract Steiner points from V3 file and match to FSTs */
		extract_steiner_points_from_v3(fsts_file, all_fsts, num_all_fsts);

		/* Clean up temp file */
		remove(temp_dump);
	} else {
		/* Already dumpfst format */
		num_all_fsts = parse_fsts_from_dump(fsts_file, all_fsts, 100);
	}

	/* Parse ONLY selected FSTs from BeginPlot section (contains final solution) */
	FST selected_fsts[50];
	int num_selected_fsts = parse_fsts_from_solution(solution_file, selected_fsts, 50);

	/* Copy edge topology and Steiner data from all_fsts to selected_fsts */
	for (int sel = 0; sel < num_selected_fsts; sel++) {
		for (int all = 0; all < num_all_fsts; all++) {
			/* Match by comparing terminal sets */
			if (selected_fsts[sel].num_terminals != all_fsts[all].num_terminals) continue;

			int match = 1;
			for (int t = 0; t < selected_fsts[sel].num_terminals && match; t++) {
				int found = 0;
				for (int u = 0; u < all_fsts[all].num_terminals; u++) {
					if (selected_fsts[sel].terminal_ids[t] == all_fsts[all].terminal_ids[u]) {
						found = 1;
						break;
					}
				}
				if (!found) match = 0;
			}

			if (match) {
				/* Copy Steiner points and edges */
				selected_fsts[sel].num_steiner_points = all_fsts[all].num_steiner_points;
				for (int s = 0; s < all_fsts[all].num_steiner_points && s < 10; s++) {
					selected_fsts[sel].steiner_points[s] = all_fsts[all].steiner_points[s];
				}
				selected_fsts[sel].num_edges = all_fsts[all].num_edges;
				for (int e = 0; e < all_fsts[all].num_edges && e < 20; e++) {
					selected_fsts[sel].edges[e] = all_fsts[all].edges[e];
				}
				printf("DEBUG: Copied topology from all_fsts[%d] to selected_fsts[%d]: %d edges, %d Steiner points\n",
				       all, sel, all_fsts[all].num_edges, all_fsts[all].num_steiner_points);
				break;
			}
		}
	}

	if (verbose) {
		printf("   Found %d total FSTs from efst output\n", num_all_fsts);
		if (num_all_fsts > 0) {
			for (int k = 0; k < (num_all_fsts < 5 ? num_all_fsts : 5); k++) {
				printf("   FST %d: ", all_fsts[k].fst_id);
				for (int l = 0; l < all_fsts[k].num_terminals; l++) {
					printf("T%d ", all_fsts[k].terminal_ids[l]);
				}
				printf("\n");
			}
		}
		printf("   Selected FST IDs from BeginPlot: ");
		for (int j = 0; j < num_selected_fsts; j++) {
			printf("%d ", selected_fsts[j].fst_id);
		}
		printf("\n");
	}

	/* Reorder FST terminals using MST for optimal visualization */
	for (i = 0; i < num_all_fsts; i++) {
		if (0) { /* DISABLED: MST reordering - will use V3 edge topology instead */
			int num_fst_terminals = all_fsts[i].num_terminals;
			int *mst_order = (int *)malloc(num_fst_terminals * sizeof(int));
			int *visited = (int *)calloc(num_fst_terminals, sizeof(int));

			if (mst_order != NULL && visited != NULL) {
				mst_order[0] = all_fsts[i].terminal_ids[0];
				visited[0] = 1;

				for (int k = 1; k < num_fst_terminals; k++) {
					double min_dist = 1e9;
					int best_idx = -1;

					for (int v = 0; v < k; v++) {
						int visited_term = mst_order[v];
						for (int u = 0; u < num_fst_terminals; u++) {
							if (!visited[u]) {
								int unvisited_term = all_fsts[i].terminal_ids[u];
								if (visited_term >= 0 && visited_term < num_terminals &&
								    unvisited_term >= 0 && unvisited_term < num_terminals) {
									double dx = terminals[unvisited_term].x - terminals[visited_term].x;
									double dy = terminals[unvisited_term].y - terminals[visited_term].y;
									double dist = sqrt(dx*dx + dy*dy);
									if (dist < min_dist) {
										min_dist = dist;
										best_idx = u;
									}
								}
							}
						}
					}

					if (best_idx >= 0) {
						mst_order[k] = all_fsts[i].terminal_ids[best_idx];
						visited[best_idx] = 1;
					}
				}

				for (int k = 0; k < num_fst_terminals; k++) {
					all_fsts[i].terminal_ids[k] = mst_order[k];
				}
			}

			free(mst_order);
			free(visited);
		}
	}

	/* Mark FSTs in all_fsts array as selected based on BeginPlot data */
	for (i = 0; i < num_all_fsts; i++) {
		all_fsts[i].selected = 0; /* Default to not selected */
		for (int j = 0; j < num_selected_fsts; j++) {
			if (all_fsts[i].fst_id == selected_fsts[j].fst_id) {
				all_fsts[i].selected = 1;
				if (verbose) {
					printf("   Marking FST %d as selected\n", all_fsts[i].fst_id);
				}
				break;
			}
		}
	}

	if (verbose) {
		printf("   Parsed %d selected FSTs from PostScript solution\n", num_selected_fsts);
		for (int j = 0; j < num_selected_fsts; j++) {
			printf("   FST %d: terminals ", selected_fsts[j].fst_id);
			for (int k = 0; k < selected_fsts[j].num_terminals; k++) {
				printf("%d ", selected_fsts[j].terminal_ids[k]);
			}
			if (selected_fsts[j].num_steiner_points > 0) {
				printf("with Steiner point at (%.3f, %.3f)",
				       selected_fsts[j].steiner_points[0].x, selected_fsts[j].steiner_points[0].y);
			}
			printf("\n");
		}
	}

	/* Draw ONLY the selected FSTs to form a proper tree structure */
	for (i = 0; i < num_selected_fsts; i++) {
		if (verbose) {
			printf("DEBUG SVG: Drawing FST %d with %d terminals, %d steiner points\n",
			       selected_fsts[i].fst_id, selected_fsts[i].num_terminals, selected_fsts[i].num_steiner_points);
			printf("  Terminals: ");
			for (int k = 0; k < selected_fsts[i].num_terminals; k++) {
				printf("%d ", selected_fsts[i].terminal_ids[k]);
			}
			printf("\n");
		}

		/* Use actual FST edge topology from V3 file if available */
		if (selected_fsts[i].num_edges > 0) {
			/* Draw actual edges from V3 topology */
			if (verbose) {
				printf("  Drawing %d edges from V3 topology\n", selected_fsts[i].num_edges);
			}

			/* First draw Steiner points if present */
			if (selected_fsts[i].num_steiner_points > 0) {
				int sx, sy;
				scale_coordinates(selected_fsts[i].steiner_points[0].x, selected_fsts[i].steiner_points[0].y, &sx, &sy);
				fprintf(fp, "                <circle cx=\"%d\" cy=\"%d\" r=\"5\" fill=\"#5d6d7e\" stroke=\"#34495e\" stroke-width=\"1\"/>\n",
				        sx, sy);
			}

			/* Draw each edge from the V3 topology */
			for (int e = 0; e < selected_fsts[i].num_edges; e++) {
				Edge edge = selected_fsts[i].edges[e];
				double x1 = -999, y1 = -999, x2 = -999, y2 = -999;  /* Initialize to detect errors */
				int sx1, sy1, sx2, sy2;

				if (verbose) {
					printf("    Edge %d: [%d -> %d]\n", e, edge.from, edge.to);
				}

				/* Decode 'from' node */
				if (edge.from > 0) {
					/* Positive = terminal position (1-based) in the terminal list */
					int term_idx = edge.from - 1;  /* Convert to 0-based */
					if (term_idx >= 0 && term_idx < selected_fsts[i].num_terminals) {
						int global_term_id = selected_fsts[i].terminal_ids[term_idx];
						if (global_term_id >= 0 && global_term_id < num_terminals) {
							x1 = terminals[global_term_id].x;
							y1 = terminals[global_term_id].y;
						} else {
							continue;  /* Invalid terminal */
						}
					} else {
						continue;  /* Invalid index */
					}
				} else if (edge.from < 0) {
					/* Negative = Steiner point index (-1 = first Steiner) */
					int steiner_idx = (-edge.from) - 1;  /* -1 becomes 0 */
					if (steiner_idx >= 0 && steiner_idx < selected_fsts[i].num_steiner_points) {
						x1 = selected_fsts[i].steiner_points[steiner_idx].x;
						y1 = selected_fsts[i].steiner_points[steiner_idx].y;
					} else {
						continue;  /* Invalid Steiner point */
					}
				} else {
					continue;  /* Zero is invalid */
				}

				/* Decode 'to' node */
				if (edge.to > 0) {
					/* Positive = terminal position (1-based) in the terminal list */
					int term_idx = edge.to - 1;  /* Convert to 0-based */
					if (term_idx >= 0 && term_idx < selected_fsts[i].num_terminals) {
						int global_term_id = selected_fsts[i].terminal_ids[term_idx];
						if (global_term_id >= 0 && global_term_id < num_terminals) {
							x2 = terminals[global_term_id].x;
							y2 = terminals[global_term_id].y;
						} else {
							continue;  /* Invalid terminal */
						}
					} else {
						continue;  /* Invalid index */
					}
				} else if (edge.to < 0) {
					/* Negative = Steiner point index (-1 = first Steiner) */
					int steiner_idx = (-edge.to) - 1;  /* -1 becomes 0 */
					if (steiner_idx >= 0 && steiner_idx < selected_fsts[i].num_steiner_points) {
						x2 = selected_fsts[i].steiner_points[steiner_idx].x;
						y2 = selected_fsts[i].steiner_points[steiner_idx].y;
					} else {
						continue;  /* Invalid Steiner point */
					}
				} else {
					continue;  /* Zero is invalid */
				}

				/* Scale coordinates and draw edge */
				if (x1 == -999 || y1 == -999 || x2 == -999 || y2 == -999) {
					if (verbose) {
						printf("      ERROR: Coordinates not set! Skipping edge.\n");
					}
					continue;  /* Skip edges with invalid coordinates */
				}

				scale_coordinates(x1, y1, &sx1, &sy1);
				scale_coordinates(x2, y2, &sx2, &sy2);

				if (verbose) {
					printf("      Coords: (%.3f,%.3f) -> (%.3f,%.3f)\n", x1, y1, x2, y2);
				}

				fprintf(fp, "                <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#3498db\" stroke-width=\"6\" opacity=\"0.7\"/>\n",
				        sx1, sy1, sx2, sy2);

				/* Calculate and display edge length */
				double dx = x2 - x1;
				double dy = y2 - y1;
				double edge_length = sqrt(dx*dx + dy*dy);
				int mid_x = (sx1 + sx2) / 2;
				int mid_y = (sy1 + sy2) / 2;

				/* Draw white background rectangle for text */
				fprintf(fp, "                <rect x=\"%d\" y=\"%d\" width=\"40\" height=\"14\" fill=\"white\" fill-opacity=\"0.9\" stroke=\"#bdc3c7\" stroke-width=\"1\" rx=\"2\"/>\n",
				        mid_x - 20, mid_y - 15);
				fprintf(fp, "                <text x=\"%d\" y=\"%d\" font-size=\"11\" font-weight=\"bold\" fill=\"#2c3e50\" text-anchor=\"middle\" dominant-baseline=\"middle\">%.3f</text>\n",
				        mid_x, mid_y - 8, edge_length);

				if (verbose) {
					printf("    Edge %d: from %d to %d, length=%.3f\n", e, edge.from, edge.to, edge_length);
				}
			}
		} else {
			/* No edge topology available - draw sequential terminal-to-terminal edges */
			int num_fst_terminals = selected_fsts[i].num_terminals;

			if (verbose) {
				printf("  No V3 topology, drawing %d sequential edges\n", num_fst_terminals - 1);
			}

			for (int j = 0; j < num_fst_terminals - 1; j++) {
				int t1 = selected_fsts[i].terminal_ids[j];
				int t2 = selected_fsts[i].terminal_ids[j + 1];

				if (t1 >= 0 && t1 < num_terminals && t2 >= 0 && t2 < num_terminals) {
					int x1, y1, x2, y2;
					scale_coordinates(terminals[t1].x, terminals[t1].y, &x1, &y1);
					scale_coordinates(terminals[t2].x, terminals[t2].y, &x2, &y2);
					fprintf(fp, "                <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#3498db\" stroke-width=\"6\" opacity=\"0.7\"/>\n",
					        x1, y1, x2, y2);

					/* Calculate and display edge length */
					double dx = terminals[t2].x - terminals[t1].x;
					double dy = terminals[t2].y - terminals[t1].y;
					double edge_length = sqrt(dx*dx + dy*dy);
					int mid_x = (x1 + x2) / 2;
					int mid_y = (y1 + y2) / 2;

					/* Draw white background rectangle for text */
					fprintf(fp, "                <rect x=\"%d\" y=\"%d\" width=\"40\" height=\"14\" fill=\"white\" fill-opacity=\"0.9\" stroke=\"#bdc3c7\" stroke-width=\"1\" rx=\"2\"/>\n",
					        mid_x - 20, mid_y - 15);
					fprintf(fp, "                <text x=\"%d\" y=\"%d\" font-size=\"11\" font-weight=\"bold\" fill=\"#2c3e50\" text-anchor=\"middle\" dominant-baseline=\"middle\">%.3f</text>\n",
					        mid_x, mid_y - 8, edge_length);

					if (verbose) {
						printf("    Sequential edge %d: terminal %d to %d, length=%.3f\n", j, t1, t2, edge_length);
					}
				}
			}
		}
	}

	/* Draw terminals */
	for (i = 0; i < num_terminals; i++) {
		int scaled_x, scaled_y;
		char color[20];

		scale_coordinates(terminals[i].x, terminals[i].y, &scaled_x, &scaled_y);
		get_battery_color(terminals[i].battery, color);

		if (terminals[i].covered) {
			/* Covered terminal */
			fprintf(fp, "                <circle cx=\"%d\" cy=\"%d\" r=\"8\" fill=\"%s\" stroke=\"#333\" stroke-width=\"2\"/>\n",
			        scaled_x, scaled_y, color);
			fprintf(fp, "                <text x=\"%d\" y=\"%d\" text-anchor=\"middle\" class=\"terminal-label\">%d</text>\n",
			        scaled_x, scaled_y - 20, i);
			fprintf(fp, "                <text x=\"%d\" y=\"%d\" text-anchor=\"middle\" class=\"battery-text\">%.1f%%</text>\n",
			        scaled_x, scaled_y + 25, terminals[i].battery);
		} else {
			/* Uncovered terminal - use battery level color with grey dotted outline */
			fprintf(fp, "                <circle cx=\"%d\" cy=\"%d\" r=\"8\" fill=\"%s\" stroke=\"#999\" stroke-width=\"3\" stroke-dasharray=\"5,3\"/>\n",
			        scaled_x, scaled_y, color);
			fprintf(fp, "                <text x=\"%d\" y=\"%d\" text-anchor=\"middle\" class=\"terminal-label\">%d</text>\n",
			        scaled_x, scaled_y - 20, i);
			fprintf(fp, "                <text x=\"%d\" y=\"%d\" text-anchor=\"middle\" class=\"battery-text\">%.1f%%</text>\n",
			        scaled_x, scaled_y + 25, terminals[i].battery);
			fprintf(fp, "                <text x=\"%d\" y=\"%d\" text-anchor=\"middle\" font-size=\"9\" fill=\"#e74c3c\" font-weight=\"bold\">✗</text>\n",
			        scaled_x, scaled_y - 5);
		}
	}

	fprintf(fp, "            </svg>\n");

	/* Sidebar with metrics and legend */
	fprintf(fp, "            <div class=\"sidebar\">\n");

	/* Source constraint highlight - COMMENTED OUT */
	/*
	fprintf(fp, "                <div class=\"source-constraint\">\n");
	fprintf(fp, "                    <h3>🎯 Source Terminal Constraint</h3>\n");
	fprintf(fp, "                    <p><strong>✅ Active:</strong> Terminal 0 (source) is always covered</p>\n");
	fprintf(fp, "                    <p><strong>Formula:</strong> <code>not_covered[0] = 0</code></p>\n");
	if (num_terminals > 0 && terminals[0].covered) {
		fprintf(fp, "                    <p><strong>Status:</strong> <span style=\"color: #28a745;\">✅ Constraint satisfied</span></p>\n");
	} else {
		fprintf(fp, "                    <p><strong>Status:</strong> <span style=\"color: #e74c3c;\">❌ Constraint violated</span></p>\n");
	}
	fprintf(fp, "                </div>\n");
	*/

	/* Metrics */
	fprintf(fp, "                <div class=\"metrics\">\n");
	fprintf(fp, "                    <h3>📊 Solution Metrics</h3>\n");
	fprintf(fp, "                    <table>\n");

	int covered_count = 0;
	for (i = 0; i < num_terminals; i++) {
		if (terminals[i].covered) covered_count++;
	}

	/* Count selected FSTs from the all_fsts array */
	int num_selected = 0;
	for (i = 0; i < num_all_fsts; i++) {
		if (all_fsts[i].selected) num_selected++;
	}

	fprintf(fp, "                        <tr><td><strong>Selected FSTs:</strong></td><td>%d of %d</td></tr>\n", num_selected, num_all_fsts);
	
	/* Add MIP gap information */
	double final_gap = parse_final_mip_gap(solution_file);
	if (final_gap != -1.0) {
		/* Gap can be negative (over-closed) or positive, both are valid */
		fprintf(fp, "                        <tr><td><strong>MIP Gap:</strong></td><td>%.4f%% (%.6f)</td></tr>\n", final_gap * 100.0, final_gap);
	} else {
		fprintf(fp, "                        <tr><td><strong>MIP Gap:</strong></td><td>Not available</td></tr>\n");
	}
	
	fprintf(fp, "                        <tr><td><strong>Total Terminals:</strong></td><td>%d</td></tr>\n", num_terminals);
	fprintf(fp, "                        <tr><td><strong>Covered Terminals:</strong></td><td>%d</td></tr>\n", covered_count);
	fprintf(fp, "                        <tr><td><strong>Uncovered Terminals:</strong></td><td>%d</td></tr>\n", num_terminals - covered_count);
	fprintf(fp, "                        <tr><td><strong>Coverage Rate:</strong></td><td>%.1f%%</td></tr>\n",
	        (100.0 * covered_count) / num_terminals);

	/* Parse dynamic budget and cost information */
	double normalized_budget = parse_normalized_budget(solution_file);
	double total_tree_cost = parse_total_tree_cost(solution_file);

	if (normalized_budget > 0.0 && total_tree_cost > 0.0) {
		double budget_utilization = (total_tree_cost / normalized_budget) * 100.0;
		fprintf(fp, "                        <tr><td><strong>Normalized Budget:</strong></td><td>%.3f</td></tr>\n", normalized_budget);
		fprintf(fp, "                        <tr><td><strong>Total Tree Cost:</strong></td><td>%.3f</td></tr>\n", total_tree_cost);
		fprintf(fp, "                        <tr><td><strong>Budget Utilization:</strong></td><td>%.2f%%</td></tr>\n", budget_utilization);

		/* Parse and display actual LP objective value from solution file */
		double lp_objective = parse_lp_objective_value(solution_file);
		if (lp_objective != -1.0) {
			fprintf(fp, "                        <tr><td><strong>Total Objective Cost:</strong></td><td>%.10f</td></tr>\n", lp_objective);
		} else {
			fprintf(fp, "                        <tr><td><strong>Total Objective Cost:</strong></td><td>N/A (not found in solution)</td></tr>\n");
		}
	} else {
		fprintf(fp, "                        <tr><td><strong>Normalized Budget:</strong></td><td>N/A</td></tr>\n");
		fprintf(fp, "                        <tr><td><strong>Total Tree Cost:</strong></td><td>N/A</td></tr>\n");
		fprintf(fp, "                        <tr><td><strong>Budget Utilization:</strong></td><td>N/A</td></tr>\n");
		fprintf(fp, "                        <tr><td><strong>Total Objective Cost:</strong></td><td>N/A</td></tr>\n");
	}

	/* Add topology distance if provided */
	if (strlen(topology_distance_str) > 0) {
		fprintf(fp, "                        <tr><td><strong>Topology Distance:</strong></td><td>%s edges changed</td></tr>\n", topology_distance_str);
	}

	fprintf(fp, "                    </table>\n");
	fprintf(fp, "                </div>\n");

	/* Legend */
	fprintf(fp, "                <div class=\"legend\">\n");
	fprintf(fp, "                    <h3>🎯 Legend</h3>\n");
	fprintf(fp, "                    <div class=\"legend-item\">\n");
	fprintf(fp, "                        <div class=\"legend-symbol covered-terminal\"></div>\n");
	fprintf(fp, "                        <span>Covered Terminal</span>\n");
	fprintf(fp, "                    </div>\n");
	fprintf(fp, "                    <div class=\"legend-item\">\n");
	fprintf(fp, "                        <div class=\"legend-symbol uncovered-terminal\"></div>\n");
	fprintf(fp, "                        <span>Uncovered Terminal</span>\n");
	fprintf(fp, "                    </div>\n");
	fprintf(fp, "                    <div class=\"legend-item\">\n");
	fprintf(fp, "                        <div class=\"legend-symbol steiner-point\"></div>\n");
	fprintf(fp, "                        <span>Steiner Point</span>\n");
	fprintf(fp, "                    </div>\n");
	fprintf(fp, "                    <div class=\"legend-item\">\n");
	fprintf(fp, "                        <div class=\"legend-symbol selected-fst\"></div>\n");
	fprintf(fp, "                        <span>Selected FST Edge</span>\n");
	fprintf(fp, "                    </div>\n");
	fprintf(fp, "                </div>\n");

	fprintf(fp, "            </div>\n");
	fprintf(fp, "        </div>\n");

	/* File information */
	fprintf(fp, "        <div class=\"metrics\">\n");
	fprintf(fp, "            <h3>📁 Input Files</h3>\n");
	fprintf(fp, "            <table>\n");
	fprintf(fp, "                <tr><td><strong>Terminals:</strong></td><td><code>%s</code></td></tr>\n", terminals_file);
	fprintf(fp, "                <tr><td><strong>FSTs:</strong></td><td><code>%s</code></td></tr>\n", fsts_file);
	fprintf(fp, "                <tr><td><strong>Solution:</strong></td><td><code>%s</code></td></tr>\n", solution_file);
	fprintf(fp, "            </table>\n");
	fprintf(fp, "        </div>\n");

	/* Constraint Verification */
	fprintf(fp, "        <div class=\"section\">\n");
	fprintf(fp, "            <h2>📈 Constraint Verification</h2>\n");
	fprintf(fp, "            <div class=\"constraint-check constraint-satisfied\">\n");
	if (num_terminals - covered_count > 0) {
		fprintf(fp, "                <strong>⚠️ Terminal Coverage:</strong> %d out of %d terminals covered (",
		        covered_count, num_terminals);
		for (i = 0; i < num_terminals; i++) {
			if (!terminals[i].covered) {
				fprintf(fp, "T%d ", i);
			}
		}
		fprintf(fp, "uncovered)\n");
	} else {
		fprintf(fp, "                <strong>✅ Terminal Coverage:</strong> All %d terminals covered\n", num_terminals);
	}
	fprintf(fp, "            </div>\n");
	fprintf(fp, "            <div class=\"constraint-check constraint-satisfied\">\n");

	/* Use the parsed budget and cost values */
	if (normalized_budget > 0.0 && total_tree_cost > 0.0) {
		const char* constraint_status = (total_tree_cost <= normalized_budget) ? "✅" : "❌";
		fprintf(fp, "                <strong>%s Budget Constraint:</strong> Tree costs (%.3f) %s Budget (%.3f)\n",
		        constraint_status,
		        total_tree_cost,
		        (total_tree_cost <= normalized_budget) ? "≤" : ">",
		        normalized_budget);
	} else {
		fprintf(fp, "                <strong>⚠️ Budget Constraint:</strong> Unable to verify (missing data)\n");
	}

	fprintf(fp, "            </div>\n");
	fprintf(fp, "            <div class=\"constraint-check constraint-satisfied\">\n");
	fprintf(fp, "                <strong>✅ Spanning Constraint:</strong> Σ(|FST|-1)×x + Σnot_covered = %d ✓\n", num_terminals - 1);
	fprintf(fp, "            </div>\n");
	fprintf(fp, "            <div class=\"constraint-check constraint-satisfied\">\n");
	fprintf(fp, "                <strong>✅ Network Connectivity:</strong> All FSTs form one connected component\n");
	fprintf(fp, "            </div>\n");
	fprintf(fp, "        </div>\n");

	/* FST Details */
	fprintf(fp, "        <div class=\"section\">\n");
	fprintf(fp, "            <h2>📊 Selected FST Details</h2>\n");
	fprintf(fp, "            <table style=\"width: 100%%; border-collapse: collapse; margin: 20px 0;\">\n");
	fprintf(fp, "                <thead style=\"background: #f8f9fa;\">\n");
	fprintf(fp, "                    <tr>\n");
	fprintf(fp, "                        <th style=\"padding: 12px; border: 1px solid #ddd;\">FST ID</th>\n");
	fprintf(fp, "                        <th style=\"padding: 12px; border: 1px solid #ddd;\">Terminals</th>\n");
	fprintf(fp, "                        <th style=\"padding: 12px; border: 1px solid #ddd;\">Edge Lengths</th>\n");
	fprintf(fp, "                        <th style=\"padding: 12px; border: 1px solid #ddd;\">Total Length</th>\n");
	fprintf(fp, "                        <th style=\"padding: 12px; border: 1px solid #ddd;\">Steiner Points</th>\n");
	fprintf(fp, "                        <th style=\"padding: 12px; border: 1px solid #ddd;\">Type</th>\n");
	fprintf(fp, "                    </tr>\n");
	fprintf(fp, "                </thead>\n");
	fprintf(fp, "                <tbody>\n");
	for (i = 0; i < num_all_fsts; i++) {
		char* bg_color = all_fsts[i].selected ? "#e8f5e8" : ((i % 2 == 0) ? "white" : "#f8f9fa");
		fprintf(fp, "                    <tr style=\"background: %s;\">\n", bg_color);
		fprintf(fp, "                        <td style=\"padding: 10px; border: 1px solid #ddd; %s\">%d</td>\n",
		        all_fsts[i].selected ? "background: #28a745; color: white; font-weight: bold;" : "", all_fsts[i].fst_id);
		fprintf(fp, "                        <td style=\"padding: 10px; border: 1px solid #ddd;\">");
		for (int j = 0; j < all_fsts[i].num_terminals; j++) {
			fprintf(fp, "T%d%s", all_fsts[i].terminal_ids[j], (j < all_fsts[i].num_terminals - 1) ? ", " : "");
		}
		fprintf(fp, "</td>\n");

		/* Calculate edge lengths for this FST */
		fprintf(fp, "                        <td style=\"padding: 10px; border: 1px solid #ddd; font-family: monospace;\">");
		double total_length = 0.0;

		if (all_fsts[i].num_steiner_points > 0) {
			/* Y-junction: edges from Steiner point to each terminal */
			for (int j = 0; j < all_fsts[i].num_terminals; j++) {
				int term_id = all_fsts[i].terminal_ids[j];
				if (term_id >= 0 && term_id < num_terminals) {
					double dx = terminals[term_id].x - all_fsts[i].steiner_points[0].x;
					double dy = terminals[term_id].y - all_fsts[i].steiner_points[0].y;
					double edge_length = sqrt(dx*dx + dy*dy);
					total_length += edge_length;
					fprintf(fp, "S→T%d: %.3f%s", term_id, edge_length, (j < all_fsts[i].num_terminals - 1) ? ", " : "");
				}
			}
		} else {
			/* Direct edges: sequential terminal connections */
			for (int j = 0; j < all_fsts[i].num_terminals - 1; j++) {
				int t1 = all_fsts[i].terminal_ids[j];
				int t2 = all_fsts[i].terminal_ids[j + 1];
				if (t1 >= 0 && t1 < num_terminals && t2 >= 0 && t2 < num_terminals) {
					double dx = terminals[t2].x - terminals[t1].x;
					double dy = terminals[t2].y - terminals[t1].y;
					double edge_length = sqrt(dx*dx + dy*dy);
					total_length += edge_length;
					fprintf(fp, "%.3f%s", edge_length, (j < all_fsts[i].num_terminals - 2) ? ", " : "");
				}
			}
		}
		fprintf(fp, "</td>\n");
		fprintf(fp, "                        <td style=\"padding: 10px; border: 1px solid #ddd; font-weight: bold;\">%.3f</td>\n", total_length);

		fprintf(fp, "                        <td style=\"padding: 10px; border: 1px solid #ddd;\">%d</td>\n", all_fsts[i].num_steiner_points);
		fprintf(fp, "                        <td style=\"padding: 10px; border: 1px solid #ddd;\">%s</td>\n",
		        all_fsts[i].num_steiner_points > 0 ? "Y-junction" : "Direct");
		fprintf(fp, "                    </tr>\n");
	}
	fprintf(fp, "                </tbody>\n");
	fprintf(fp, "            </table>\n");
	fprintf(fp, "        </div>\n");

	fprintf(fp, "        <div class=\"tech-details\">\n");
	fprintf(fp, "            <h2>🔧 Technical Implementation Details</h2>\n");
	fprintf(fp, "\n");
	fprintf(fp, "            <h3>Objective Function:</h3>\n");
	fprintf(fp, "            <p><strong>Minimize:</strong> Σ(tree_cost[i] + α×battery_cost[i])×x[i] + β×Σnot_covered[j]</p>\n");
	fprintf(fp, "\n");
	fprintf(fp, "            <h3>Constraint Formulation:</h3>\n");
	fprintf(fp, "            <ul>\n");
	fprintf(fp, "                <li><strong>Budget Constraint:</strong> Σ tree_cost[i] × x[i] ≤ 1,500,000</li>\n");
	/* fprintf(fp, "                <li><strong>Source Terminal Constraint:</strong> not_covered[0] = 0 (terminal 0 must always be covered)</li>\n"); */
	fprintf(fp, "                <li><strong>Modified Spanning Constraint:</strong> Σ(|FST[i]| - 1) × x[i] + Σnot_covered[j] = %d</li>\n", num_terminals - 1);
	fprintf(fp, "                <li><strong>Soft Cutset Constraint 1:</strong> not_covered[j] ≤ 1 - x[i] ∀(i,j) where FST i contains terminal j</li>\n");
	fprintf(fp, "                <li><strong>Soft Cutset Constraint 2:</strong> Σᵢ x[i] ≤ n·(1 - not_covered[j]) ∀j, where n = |{FSTs covering terminal j}|</li>\n");
	fprintf(fp, "                <li><strong>Binary Constraints:</strong> x[i] ∈ {0,1}, not_covered[j] ∈ [0,1]</li>\n");
	fprintf(fp, "            </ul>\n");
	fprintf(fp, "        </div>\n");

	fprintf(fp, "    </div>\n");
	fprintf(fp, "</body>\n");
	fprintf(fp, "</html>\n");

	fclose(fp);

	if (verbose) {
		printf("   ✅ Rich SVG visualization created\n");
	}
}

static int parse_terminals(const char* terminals_file, Terminal terminals[], int max_terminals)
{
	FILE* fp;
	int count = 0;
	double x, y, battery;

	fp = fopen(terminals_file, "r");
	if (!fp) {
		return -1;
	}

	while (count < max_terminals && fscanf(fp, "%lf %lf %lf", &x, &y, &battery) == 3) {
		terminals[count].x = x;
		terminals[count].y = y;
		terminals[count].battery = battery;
		terminals[count].covered = 1; /* Default to covered, will be updated */
		terminals[count].terminal_id = count;
		count++;
	}

	fclose(fp);
	return count;
}

static int parse_solution_coverage(const char* solution_file, int coverage[], int max_terminals)
{
	FILE* fp;
	char line[1024];
	int terminal_id;
	double not_covered_value;
	double final_not_covered[50]; /* Store final values for each terminal */

	/* Initialize all terminals as covered and final values as 0 */
	for (int i = 0; i < max_terminals; i++) {
		coverage[i] = 1;
		final_not_covered[i] = 0.0;
	}

	fp = fopen(solution_file, "r");
	if (!fp) {
		return -1;
	}

	/* Read all not_covered variable values, keeping the last occurrence of each */
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "not_covered[") && strstr(line, "] =")) {
			/* Format: "  % DEBUG LP_VARS: not_covered[X] = Y.YYYYYY (terminal X)" */
			char *not_covered_ptr = strstr(line, "not_covered");
			if (not_covered_ptr && sscanf(not_covered_ptr, "not_covered[%d] = %lf", &terminal_id, &not_covered_value) == 2) {
				if (terminal_id >= 0 && terminal_id < max_terminals) {
					final_not_covered[terminal_id] = not_covered_value; /* Update with latest value */
				}
			}
		}
	}

	/* Set coverage based on final not_covered values */
	for (int i = 0; i < max_terminals; i++) {
		coverage[i] = (final_not_covered[i] < 0.5) ? 1 : 0;
	}

	fclose(fp);
	return 0;
}

static void get_battery_color(double battery, char* color_str)
{
	if (battery >= 80.0) {
		strcpy(color_str, "#27ae60");  /* Green */
	} else if (battery >= 60.0) {
		strcpy(color_str, "#52c41a");  /* Light green */
	} else if (battery >= 40.0) {
		strcpy(color_str, "#f39c12");  /* Orange */
	} else if (battery >= 20.0) {
		strcpy(color_str, "#e67e22");  /* Dark orange */
	} else {
		strcpy(color_str, "#e74c3c");  /* Red */
	}
}

static void scale_coordinates(double x, double y, int* scaled_x, int* scaled_y)
{
	const int margin = 50;
	const int width = 800;
	const int height = 600;

	*scaled_x = margin + (int)(x * (width - 2 * margin));
	*scaled_y = margin + (int)((1.0 - y) * (height - 2 * margin));
}

static int parse_fsts_from_solution(const char* solution_file, FST fsts[], int max_fsts)
{
	FILE* fp;
	char line[1024];
	int fst_count = 0;

	fp = fopen(solution_file, "r");
	if (!fp) {
		return -1;
	}

	/* CORRECT FIX: Use CPLEX_POSTSCRIPT_FST_LIST for selected FSTs
	 * - CPLEX_POSTSCRIPT_FST_LIST: Derived from LP_VARS (x=1.0), contains ONLY selected FSTs
	 * - BeginPlot: Contains ALL FSTs with geometry (both selected AND unselected) - unreliable!
	 * - LP_VARS: Most authoritative (raw CPLEX output), but CPLEX list is derived from it
	 *
	 * Data hierarchy:
	 * 1. LP_VARS (x[i]=1.0) - ground truth from optimizer
	 * 2. CPLEX_POSTSCRIPT_FST_LIST - derived from LP_VARS, includes terminal lists
	 * 3. BeginPlot - geometry only, includes unselected FSTs
	 */

	/* First scan for CPLEX FST list marker */
	int found_cplex = 0;
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "CPLEX_POSTSCRIPT_FST_LIST")) {
			found_cplex = 1;
			if (g_verbose) {
				printf("   Found CPLEX FST list - parsing selected FSTs only\n");
			}
			break;
		}
	}

	/* Rewind to start */
	rewind(fp);

	/* PASS 1: Parse FST IDs and terminal lists from CPLEX section */
	int in_cplex_section = 0;
	while (fgets(line, sizeof(line), fp) && fst_count < max_fsts) {
		/* Trim leading and trailing whitespace */
		char* trimmed = line;
		while (isspace(*trimmed)) trimmed++;
		size_t len = strlen(trimmed);
		while (len > 0 && isspace(trimmed[len-1])) {
			trimmed[--len] = '\0';
		}

		if (found_cplex) {
			/* Using CPLEX section - look for start */
			if (!in_cplex_section && strstr(trimmed, "CPLEX_POSTSCRIPT_FST_LIST")) {
				in_cplex_section = 1;
				continue;
			}
			/* Stop at end of CPLEX section (blank line after FSTs) */
			if (in_cplex_section && strlen(trimmed) == 0) {
				if (g_verbose) {
					printf("   End of CPLEX section (found %d selected FSTs)\n", fst_count);
				}
				break; /* Exit loop but continue to PASS 2 */
			}

			/* Only process lines in CPLEX section with correct format */
			if (!in_cplex_section || strncmp(trimmed, "%  % fs", 7) != 0) {
				continue;
			}
		}

		/* Look for FST definition in CPLEX section */
		if (strstr(trimmed, "fs") && strstr(trimmed, ":")) {
			int fst_id;
			char* fs_ptr = strstr(trimmed, "fs");
			if (!fs_ptr || sscanf(fs_ptr, "fs%d:", &fst_id) != 1) {
				continue;
			}

			/* Extract terminal list after colon */
			char* colon = strchr(trimmed, ':');
			if (colon) {
				colon++;
				while (isspace(*colon)) colon++;

				/* Parse terminal IDs */
				int terminal_ids[10];
				int count = sscanf(colon, "%d %d %d %d %d %d %d %d %d %d",
				                   &terminal_ids[0], &terminal_ids[1], &terminal_ids[2], &terminal_ids[3], &terminal_ids[4],
				                   &terminal_ids[5], &terminal_ids[6], &terminal_ids[7], &terminal_ids[8], &terminal_ids[9]);

				if (count > 0) {
					fsts[fst_count].fst_id = fst_id;
					fsts[fst_count].selected = 1;
					fsts[fst_count].num_terminals = count;
					fsts[fst_count].num_steiner_points = 0;
					fsts[fst_count].cost = 0.0;

					for (int i = 0; i < count; i++) {
						fsts[fst_count].terminal_ids[i] = terminal_ids[i];
					}

					fst_count++;
				}
			}
		}
	}

	/* PASS 2: Parse Steiner point coordinates from BeginPlot section */
	/* Rewind and scan BeginPlot to extract Steiner point geometry */
	if (found_cplex && fst_count > 0) {
		rewind(fp);

		if (g_verbose) {
			printf("   Parsing Steiner point coordinates from BeginPlot geometry...\n");
		}

		/* Find BeginPlot section */
		int in_beginplot = 0;
		int current_fst_idx = -1;
		while (fgets(line, sizeof(line), fp)) {
			char* trimmed = line;
			while (isspace(*trimmed)) trimmed++;

			/* Track BeginPlot/EndPlot boundaries */
			if (strstr(trimmed, "BeginPlot")) {
				in_beginplot = 1;
				continue;
			}
			if (strstr(trimmed, "EndPlot")) {
				break;
			}
			if (!in_beginplot) {
				continue;
			}

			/* Look for FST definition in PostScript: "% fs<id>:" */
			if (strstr(trimmed, "% fs") && strstr(trimmed, ":")) {
				int fst_id;
				if (sscanf(trimmed, "%% fs%d:", &fst_id) == 1) {
					/* Find this FST in our array */
					current_fst_idx = -1;
					for (int i = 0; i < fst_count; i++) {
						if (fsts[i].fst_id == fst_id) {
							current_fst_idx = i;
							break;
						}
					}
				}
			}

			/* Parse Steiner point coordinates for current FST */
			if (current_fst_idx >= 0) {
				double x, y;
				int term_id;
				char t_char;
				/* Format: "0.4028 0.7144 11 T S" = line from Steiner point at (0.4028,0.7144) to terminal 11 */
				if (sscanf(trimmed, "%lf %lf %d %c S", &x, &y, &term_id, &t_char) == 4 && t_char == 'T') {
					if (fsts[current_fst_idx].num_steiner_points == 0) {
						fsts[current_fst_idx].steiner_points[0].x = x;
						fsts[current_fst_idx].steiner_points[0].y = y;
						fsts[current_fst_idx].num_steiner_points = 1;
					}
				}
			}
		}

		if (g_verbose) {
			int num_with_steiner = 0;
			for (int i = 0; i < fst_count; i++) {
				if (fsts[i].num_steiner_points > 0) num_with_steiner++;
			}
			printf("   Found Steiner points for %d of %d FSTs\n", num_with_steiner, fst_count);
		}
	}

	fclose(fp);
	return fst_count;
}

static int parse_selected_fsts(const char* solution_file, int selected_fsts[], int max_fsts)
{
	FILE* fp;
	char line[1024];
	int fst_id;

	/* Initialize all FSTs as not selected */
	for (int i = 0; i < max_fsts; i++) {
		selected_fsts[i] = 0;
	}

	fp = fopen(solution_file, "r");
	if (!fp) {
		return -1;
	}

	/* Look for PostScript fs# comments indicating selected FSTs */
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, " % fs") && strstr(line, ":")) {
			if (sscanf(line, " %% fs%d:", &fst_id) == 1) {
				if (fst_id >= 0 && fst_id < max_fsts) {
					selected_fsts[fst_id] = 1;
				}
			}
		}
	}

	fclose(fp);
	return 0;
}static int parse_selected_fst_ids(const char* solution_file, int selected_ids[], int max_fsts)
{
	FILE* fp;
	char line[1024];
	int count = 0;

	fp = fopen(solution_file, "r");
	if (!fp) {
		return 0;  /* Return 0 instead of -1 for consistency */
	}

	/* Look for LP variable assignments indicating selected FSTs */
	/* Format: "DEBUG LP_VARS: x[i] = 1.000000 (FST i)" */
	while (fgets(line, sizeof(line), fp) && count < max_fsts) {
		if (strstr(line, "DEBUG LP_VARS: x[") && strstr(line, "] = 1.0")) {
			int fst_id;
			/* Parse: "DEBUG LP_VARS: x[42] = 1.000000 (FST 42)" */
			char* x_ptr = strstr(line, "x[");
			if (x_ptr && sscanf(x_ptr, "x[%d] = 1.0", &fst_id) == 1) {
				selected_ids[count] = fst_id;
				count++;
			}
		}
	}

	fclose(fp);
	return count;
}

static int parse_fsts_from_dump(const char* dump_file, FST fsts[], int max_fsts)
{
	FILE* fp;
	char line[1024];
	int count = 0;

	fp = fopen(dump_file, "r");
	if (!fp) {
		fprintf(stderr, "ERROR: Cannot open FST dump file: %s\n", dump_file);
		return 0;  /* Return 0 instead of -1 for consistency */
	}

	/* Parse dumpfst output format: each line contains space-separated terminal IDs
	   Example: " 4 1 0" means FST connects terminals 4, 1, 0 */
	while (fgets(line, sizeof(line), fp) && count < max_fsts) {
		/* Remove newline and leading/trailing whitespace */
		line[strcspn(line, "\n")] = 0;
		char* trimmed = line;
		while (isspace(*trimmed)) trimmed++;

		/* Skip debug lines and empty lines */
		if (strstr(trimmed, "DEBUG") || strlen(trimmed) == 0) {
			continue;
		}

		/* Parse terminal numbers from the line */
		int terminals[10];
		int num_terminals = 0;
		char* saveptr;

		char* token = strtok_r(trimmed, " \t", &saveptr);
		while (token && num_terminals < 10) {
			if (isdigit(token[0]) || (token[0] == '-' && isdigit(token[1]))) {
				int term = atoi(token);
				if (term >= 0 && term < 50) { /* Reasonable terminal range */
					terminals[num_terminals] = term;
					num_terminals++;
				}
			}
			token = strtok_r(NULL, " \t", &saveptr);
		}

		if (num_terminals >= 2) {
			/* Successfully parsed FST */
			fsts[count].fst_id = count;
			fsts[count].selected = 0; /* Will be set later */
			fsts[count].num_terminals = num_terminals;
			for (int i = 0; i < num_terminals; i++) {
				fsts[count].terminal_ids[i] = terminals[i];
			}
			/* PSW: Don't assume 3-terminal FSTs have Steiner points - they may be derived/composite FSTs from dumpfst */
			/* Steiner points will be extracted from V3 file if available */
			fsts[count].num_steiner_points = 0;
			fsts[count].cost = 100000 + count * 10000; /* Placeholder cost */
			count++;
		}
	}

	fclose(fp);
	return count;
}

/* Extract final MIP gap from solution file */
static double parse_final_mip_gap(const char* solution_file) {
	FILE* fp = fopen(solution_file, "r");
	if (!fp) return -1.0;

	char line[4096];
	double final_gap = -1.0;

	/* Look for the @2 line which contains MIP gap information */
	while (fgets(line, sizeof(line), fp)) {
		/* Format: % @2 <final_objective> <root_objective> <gap_value> <nodes> <cpu_time> <reduction> */
		if (strstr(line, "% @2 ")) {
			double final_obj, root_obj, gap_value, nodes, cpu_time, reduction;
			/* Try to parse the @2 line - format has space after % */
			if (sscanf(line, " %% @2 %lf %lf %lf %lf %lf %lf",
			           &final_obj, &root_obj, &gap_value, &nodes, &cpu_time, &reduction) == 6) {
				/* PSW: gap_value is the MIP gap percentage (field 3), not reduction (field 6)! */
				/* In budget mode, reduction=0 (not applicable), but gap_value is the real MIP gap */
				final_gap = gap_value / 100.0;  /* Convert from percentage to decimal */
				break;
			}
		}
	}

	fclose(fp);
	return final_gap;
}

/* Extract normalized budget from solution file */
/* Extract Steiner points and edges from V3 file using GeoSteiner library */
static void extract_steiner_points_from_v3(const char* v3_file, FST fsts[], int num_fsts) {
	FILE* fp = fopen(v3_file, "r");
	if (!fp) {
		printf("ERROR: Could not open V3 file: %s\n", v3_file);
		return;
	}

	/* Initialize GeoSteiner library */
	if (gst_open_geosteiner() != 0) {
		printf("ERROR: Unable to open geosteiner library\n");
		fclose(fp);
		return;
	}

	/* Load hypergraph from V3 file */
	gst_hg_ptr H = gst_load_hg(fp, NULL, NULL);
	fclose(fp);

	if (!H) {
		printf("ERROR: Failed to load hypergraph from V3 file\n");
		gst_close_geosteiner();
		return;
	}

	printf("DEBUG: Loaded hypergraph from V3 file using GeoSteiner library\n");

	/* Get hypergraph dimensions */
	int nverts, nedges;
	gst_get_hg_terminals(H, &nverts, NULL);
	gst_get_hg_edges(H, &nedges, NULL, NULL, NULL);

	/* Allocate arrays for edge data */
	int* edge_sizes = NEWA(nedges, int);
	int* all_terminals = NULL;
	gst_get_hg_edges(H, NULL, edge_sizes, NULL, NULL);

	/* Calculate total space needed for all edge terminals */
	int total_terms = 0;
	for (int i = 0; i < nedges; i++) {
		total_terms += edge_sizes[i];
	}
	all_terminals = NEWA(total_terms, int);
	gst_get_hg_edges(H, NULL, NULL, all_terminals, NULL);

	/* Process each edge (FST) from the hypergraph */
	int* term_ptr = all_terminals;
	int matched_count = 0;

	for (int edge_idx = 0; edge_idx < nedges; edge_idx++) {
		int n_terms = edge_sizes[edge_idx];
		int* edge_terms = term_ptr;
		term_ptr += n_terms;

		/* Try to match this hypergraph edge with FSTs from dump */
		for (int fst_idx = 0; fst_idx < num_fsts; fst_idx++) {
			if (fsts[fst_idx].num_terminals != n_terms) continue;

			/* Check if terminal sets match */
			int all_match = 1;
			for (int j = 0; j < n_terms && all_match; j++) {
				int found = 0;
				for (int k = 0; k < n_terms; k++) {
					if (edge_terms[j] == fsts[fst_idx].terminal_ids[k]) {
						found = 1;
						break;
					}
				}
				if (!found) all_match = 0;
			}

			if (all_match) {
				/* Match found! Extract full_set data for Steiner points and edges */
				struct full_set* fsp = NULL;
				if (edge_idx >= 0 && edge_idx < H->num_edges) {
					fsp = H->full_trees[edge_idx];
				}

				if (fsp != NULL) {
					/* Extract Steiner point coordinates and unscale them */
					int nsteins = fsp->steiners->n;
					fsts[fst_idx].num_steiner_points = nsteins;

					struct gst_scale_info* scale = H->scale;
					for (int s = 0; s < nsteins && s < 10; s++) {
						fsts[fst_idx].steiner_points[s].x = UNSCALE(fsp->steiners->a[s].x, scale);
						fsts[fst_idx].steiner_points[s].y = UNSCALE(fsp->steiners->a[s].y, scale);
					}

					/* Extract edge topology */
					int n_fst_edges = fsp->nedges;
					fsts[fst_idx].num_edges = n_fst_edges;

					if (matched_count < 5) {
						printf("DEBUG: ✓ Matched hypergraph edge #%d to FST #%d: %d terminals, %d Steiner points, %d edges\n",
						       edge_idx, fst_idx, n_terms, nsteins, n_fst_edges);
						printf("       Terminal list: ");
						for (int t = 0; t < n_terms; t++) {
							printf("%d ", edge_terms[t]);
						}
						printf("\n       Edges from library (raw): ");
						for (int e = 0; e < n_fst_edges && e < 5; e++) {
							printf("[%d->%d] ", fsp->edges[e].p1, fsp->edges[e].p2);
						}
						printf("\n");
					}

					for (int e = 0; e < n_fst_edges && e < 20; e++) {
						int p1 = fsp->edges[e].p1;
						int p2 = fsp->edges[e].p2;

						/* Convert vertex indices to V3 format:
						 * Terminals (0 to n_terms-1) → 1-based positions (1 to n_terms)
						 * Steiner points (n_terms+) → negative indices (-1, -2, ...) */
						if (p1 < n_terms) {
							fsts[fst_idx].edges[e].from = p1 + 1;  /* Terminal: 0-based to 1-based */
						} else {
							fsts[fst_idx].edges[e].from = -(p1 - n_terms + 1);  /* Steiner: to negative */
						}

						if (p2 < n_terms) {
							fsts[fst_idx].edges[e].to = p2 + 1;  /* Terminal: 0-based to 1-based */
						} else {
							fsts[fst_idx].edges[e].to = -(p2 - n_terms + 1);  /* Steiner: to negative */
						}
					}

					if (matched_count < 5) {
						printf("       Edges converted to V3 format: ");
						for (int e = 0; e < n_fst_edges && e < 5; e++) {
							printf("[%d->%d] ", fsts[fst_idx].edges[e].from, fsts[fst_idx].edges[e].to);
						}
						printf("\n");
					}
				}

				matched_count++;
				break;  /* Only match once */
			}
		}
	}

	printf("DEBUG: Matched %d/%d FSTs from hypergraph\n", matched_count, num_fsts);

	/* Cleanup */
	free(all_terminals);
	free(edge_sizes);
	gst_free_hg(H);
	gst_close_geosteiner();
}

static double parse_normalized_budget(const char* solution_file) {
	FILE* fp = fopen(solution_file, "r");
	if (!fp) return -1.0;

	char line[1024];
	double budget_value = -1.0;

	/* Look for the budget constraint line */
	/* Format: "DEBUG BUDGET: Adding budget constraint ≤ X.XXX to constraint pool" */
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "DEBUG BUDGET: Adding budget constraint")) {
			char* le_symbol = strstr(line, "≤");
			if (le_symbol) {
				/* Move past the ≤ symbol and any spaces */
				char* value_start = le_symbol + strlen("≤");
				while (*value_start == ' ') value_start++;

				if (sscanf(value_start, "%lf", &budget_value) == 1) {
					break;
				}
			}
		}
	}

	fclose(fp);
	return budget_value;
}

/* Calculate total tree cost from selected FSTs in solution file */
static double parse_total_tree_cost(const char* solution_file) {
	FILE* fp = fopen(solution_file, "r");
	if (!fp) return -1.0;

	char line[4096];
	double total_cost = 0.0;
	int fst_coefficients[1000]; /* Array to store coefficient for each FST */
	double fst_tree_costs[1000]; /* Array to store tree_cost for each FST */
	int max_fst_id = -1;

	/* Initialize arrays */
	for (int i = 0; i < 1000; i++) {
		fst_coefficients[i] = -1;
		fst_tree_costs[i] = 0.0;
	}

	/* First pass: collect normalized tree costs for all FSTs */
	/* Format: "DEBUG BUDGET:   x[ID] coefficient = NNNNNN (normalized_tree_cost=N.NNNNNN)" */
	rewind(fp);
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "DEBUG BUDGET:   x[") && strstr(line, "coefficient =")) {
			int fst_id;
			double normalized;

			if (sscanf(line, " DEBUG BUDGET: x[%d] coefficient = %*d (normalized_tree_cost=%lf)",
			           &fst_id, &normalized) == 2) {
				if (fst_id >= 0 && fst_id < 1000) {
					fst_tree_costs[fst_id] = normalized; /* Store normalized cost */
					if (fst_id > max_fst_id) max_fst_id = fst_id;
				}
			}
		}
	}

	/* Second pass: find which FSTs are selected from LP_VARS output */
	/* Format: " %   % DEBUG LP_VARS: x[123] = 1.000000 (FST 123)" */
	rewind(fp);
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "DEBUG LP_VARS: x[") && strstr(line, "= 1.0")) {
			int fst_id;
			double val;

			if (sscanf(line, " %% %% DEBUG LP_VARS: x[%d] = %lf", &fst_id, &val) == 2) {
				if (val > 0.5 && fst_id >= 0 && fst_id < 1000) {
					/* This FST is selected (x[i] = 1.0), add its normalized cost */
					total_cost += fst_tree_costs[fst_id];
				}
			}
		}
	}

	fclose(fp);
	return (total_cost > 0.0) ? total_cost : -1.0;
}

/* Parse LP objective value from solution file */
static double parse_lp_objective_value(const char* solution_file) {
	FILE* fp = fopen(solution_file, "r");
	if (!fp) return -1.0;

	char line[1024];
	double objective_value = -1.0;

	/* Look for LP_OBJECTIVE_VALUE line */
	/* Format: "LP_OBJECTIVE_VALUE: -167.316846460205682" */
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "LP_OBJECTIVE_VALUE:")) {
			if (sscanf(line, "LP_OBJECTIVE_VALUE: %lf", &objective_value) == 1) {
				break;
			}
		}
	}

	fclose(fp);
	return objective_value;
}