/***********************************************************************

	File:	edge_map.c
	Rev:	1.0
	Date:	2025-01-18

	Multi-Temporal Optimization: Edge Enumeration Implementation

	This module implements the edge mapping infrastructure for multi-
	temporal optimization. It scans all FSTs and builds a global map
	of unique edges with bidirectional FST-edge relationships.

************************************************************************/

#include "edge_map.h"
#include "steiner.h"
#include "memory.h"
#include "logic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Local Constants
 */

#define INITIAL_EDGE_CAPACITY	256
#define INITIAL_FST_CAPACITY	16
#define HASH_TABLE_SIZE		1021	/* Prime number for better distribution */

/*
 * Local Routines
 */

static int		edge_hash (int p1, int p2, int hash_size);
static void		add_edge_to_hash (struct edge_map * emap,
					  int edge_index,
					  int p1,
					  int p2);
static int		find_or_create_edge (struct edge_map * emap,
					     int p1,
					     int p2,
					     double length);
static void		add_fst_to_edge (struct edge_map * emap,
					 int edge_index,
					 int fst_index);

/*
 * Hash function for edges.
 * Uses simple polynomial rolling hash on the two endpoints.
 */

	static
	int
edge_hash (

int	p1,		/* IN - first endpoint (smaller) */
int	p2,		/* IN - second endpoint (larger) */
int	hash_size	/* IN - hash table size */
)
{
	/* Simple hash: (p1 * 31 + p2) % hash_size */
	return ((p1 * 31 + p2) % hash_size);
}

/*
 * Add an edge to the hash table for fast lookup.
 */

	static
	void
add_edge_to_hash (

struct edge_map *	emap,		/* IN/OUT - edge map */
int			edge_index,	/* IN - index in edges array */
int			p1,		/* IN - first endpoint */
int			p2		/* IN - second endpoint */
)
{
	int			bucket;
	struct edge_hash_node *	node;

	/* Allocate new hash node */
	node = NEW (struct edge_hash_node);
	node -> p1 = p1;
	node -> p2 = p2;
	node -> edge_index = edge_index;

	/* Add to front of collision chain */
	bucket = edge_hash (p1, p2, emap -> hash_size);
	node -> next = emap -> hash_table [bucket];
	emap -> hash_table [bucket] = node;
}

/*
 * Find an edge in the hash table, or create it if it doesn't exist.
 * Returns the edge index.
 */

	static
	int
find_or_create_edge (

struct edge_map *	emap,		/* IN/OUT - edge map */
int			p1,		/* IN - first endpoint */
int			p2,		/* IN - second endpoint */
double			length		/* IN - edge length */
)
{
	int			bucket;
	int			edge_index;
	struct edge_hash_node *	node;
	struct edge_info *	edge;

	/* Ensure p1 < p2 (canonical form) */
	if (p1 > p2) {
		int tmp = p1;
		p1 = p2;
		p2 = tmp;
	}

	/* Search hash table */
	bucket = edge_hash (p1, p2, emap -> hash_size);
	for (node = emap -> hash_table [bucket]; node NE NULL; node = node -> next) {
		if ((node -> p1 EQ p1) AND (node -> p2 EQ p2)) {
			/* Found existing edge */
			return node -> edge_index;
		}
	}

	/* Edge not found - create new one */

	/* Expand edges array if needed */
	if (emap -> num_edges >= emap -> edge_capacity) {
		int new_capacity = emap -> edge_capacity * 2;
		emap -> edges = (struct edge_info *) realloc (
			emap -> edges,
			new_capacity * sizeof (struct edge_info));
		if (emap -> edges EQ NULL) {
			fprintf (stderr, "ERROR: Failed to allocate edge array\n");
			exit (1);
		}
		emap -> edge_capacity = new_capacity;
	}

	/* Initialize new edge */
	edge_index = emap -> num_edges++;
	edge = &(emap -> edges [edge_index]);
	edge -> p1 = p1;
	edge -> p2 = p2;
	edge -> length = length;
	edge -> num_fsts = 0;
	edge -> fst_capacity = INITIAL_FST_CAPACITY;
	edge -> fst_list = NEWA (INITIAL_FST_CAPACITY, int);

	/* Add to hash table */
	add_edge_to_hash (emap, edge_index, p1, p2);

	return edge_index;
}

/*
 * Add an FST to an edge's FST list.
 */

	static
	void
add_fst_to_edge (

struct edge_map *	emap,		/* IN/OUT - edge map */
int			edge_index,	/* IN - edge index */
int			fst_index	/* IN - FST index to add */
)
{
	struct edge_info *	edge;

	edge = &(emap -> edges [edge_index]);

	/* Expand FST list if needed */
	if (edge -> num_fsts >= edge -> fst_capacity) {
		int new_capacity = edge -> fst_capacity * 2;
		edge -> fst_list = (int *) realloc (
			edge -> fst_list,
			new_capacity * sizeof (int));
		if (edge -> fst_list EQ NULL) {
			fprintf (stderr, "ERROR: Failed to allocate FST list\n");
			exit (1);
		}
		edge -> fst_capacity = new_capacity;
	}

	/* Add FST to list */
	edge -> fst_list [edge -> num_fsts++] = fst_index;
}

/*
 * Build edge map from hypergraph's Full Steiner Trees.
 * This scans all FSTs and extracts all unique edges, building
 * bidirectional mappings between edges and FSTs.
 */

	struct edge_map *
build_edge_map (

struct gst_hypergraph *	cip		/* IN - hypergraph with FSTs */
)
{
	int			i, j;
	int			nedges_total;
	int			edge_index;
	struct edge_map *	emap;
	struct full_set **	fsts;
	struct full_set *	fst;
	struct edge *		fst_edge;

	fprintf (stderr, "\n=== BUILDING EDGE MAP ===\n");

	/* Allocate edge map structure */
	emap = NEW (struct edge_map);
	emap -> num_edges = 0;
	emap -> edge_capacity = INITIAL_EDGE_CAPACITY;
	emap -> edges = NEWA (INITIAL_EDGE_CAPACITY, struct edge_info);

	/* Initialize hash table */
	emap -> hash_size = HASH_TABLE_SIZE;
	emap -> hash_table = NEWA (HASH_TABLE_SIZE, struct edge_hash_node *);
	for (i = 0; i < HASH_TABLE_SIZE; i++) {
		emap -> hash_table [i] = NULL;
	}

	/* Check if we have FSTs */
	if ((cip -> full_trees EQ NULL) OR (cip -> num_edges EQ 0)) {
		fprintf (stderr, "WARNING: No FSTs found in hypergraph\n");
		fprintf (stderr, "=== EDGE MAP COMPLETE: 0 edges ===\n\n");
		return emap;
	}

	fsts = cip -> full_trees;
	nedges_total = 0;

	fprintf (stderr, "Scanning %d FSTs...\n", cip -> num_edges);

	/* Scan all FSTs */
	for (i = 0; i < cip -> num_edges; i++) {
		fst = fsts [i];
		if (fst EQ NULL) continue;

		/* Scan all edges in this FST */
		for (j = 0; j < fst -> nedges; j++) {
			fst_edge = &(fst -> edges [j]);

			/* Get edge endpoints (p1 and p2) */
			int p1 = fst_edge -> p1;
			int p2 = fst_edge -> p2;
			double length = fst_edge -> len;

			/* Find or create edge in global map */
			edge_index = find_or_create_edge (emap, p1, p2, length);

			/* Add this FST to the edge's FST list */
			add_fst_to_edge (emap, edge_index, i);

			nedges_total++;
		}
	}

	fprintf (stderr, "Total edge instances across all FSTs: %d\n", nedges_total);
	fprintf (stderr, "Unique edges found: %d\n", emap -> num_edges);
	fprintf (stderr, "=== EDGE MAP COMPLETE ===\n\n");

	return emap;
}

/*
 * Free edge map and all associated memory.
 */

	void
free_edge_map (

struct edge_map *	emap		/* IN - edge map to free */
)
{
	int			i;
	struct edge_hash_node *	node;
	struct edge_hash_node *	next;

	if (emap EQ NULL) return;

	/* Free edge FST lists */
	for (i = 0; i < emap -> num_edges; i++) {
		free (emap -> edges [i].fst_list);
	}

	/* Free edges array */
	free (emap -> edges);

	/* Free hash table */
	for (i = 0; i < emap -> hash_size; i++) {
		node = emap -> hash_table [i];
		while (node NE NULL) {
			next = node -> next;
			free (node);
			node = next;
		}
	}
	free (emap -> hash_table);

	/* Free edge map itself */
	free (emap);
}

/*
 * Lookup edge index by endpoints.
 * Returns edge index if found, -1 if not found.
 */

	int
lookup_edge (

struct edge_map *	emap,		/* IN - edge map */
int			p1,		/* IN - first endpoint */
int			p2		/* IN - second endpoint */
)
{
	int			bucket;
	struct edge_hash_node *	node;

	/* Ensure p1 < p2 (canonical form) */
	if (p1 > p2) {
		int tmp = p1;
		p1 = p2;
		p2 = tmp;
	}

	/* Search hash table */
	bucket = edge_hash (p1, p2, emap -> hash_size);
	for (node = emap -> hash_table [bucket]; node NE NULL; node = node -> next) {
		if ((node -> p1 EQ p1) AND (node -> p2 EQ p2)) {
			return node -> edge_index;
		}
	}

	return -1;  /* Not found */
}

/*
 * Get FST list for a given edge.
 * Returns pointer to FST list and sets num_fsts_out.
 */

	int *
get_edge_fsts (

struct edge_map *	emap,		/* IN - edge map */
int			edge_index,	/* IN - edge index */
int *			num_fsts_out	/* OUT - number of FSTs */
)
{
	if ((edge_index < 0) OR (edge_index >= emap -> num_edges)) {
		*num_fsts_out = 0;
		return NULL;
	}

	*num_fsts_out = emap -> edges [edge_index].num_fsts;
	return emap -> edges [edge_index].fst_list;
}

/*
 * Print edge map statistics and contents for debugging.
 */

	void
print_edge_map (

struct edge_map *	emap		/* IN - edge map */
)
{
	int			i, j;
	struct edge_info *	edge;

	fprintf (stderr, "\n=== EDGE MAP DETAILS ===\n");
	fprintf (stderr, "Total unique edges: %d\n", emap -> num_edges);
	fprintf (stderr, "Hash table size: %d\n", emap -> hash_size);
	fprintf (stderr, "\nEdge List (first 20):\n");
	fprintf (stderr, "Index | Endpoints | Length      | #FSTs | FST List\n");
	fprintf (stderr, "------|-----------|-------------|-------|----------\n");

	for (i = 0; (i < emap -> num_edges) AND (i < 20); i++) {
		edge = &(emap -> edges [i]);
		fprintf (stderr, "%5d | (%3d,%3d) | %11.6f | %5d | ",
			i, edge -> p1, edge -> p2, edge -> length, edge -> num_fsts);

		/* Print first 10 FSTs */
		for (j = 0; (j < edge -> num_fsts) AND (j < 10); j++) {
			fprintf (stderr, "%d ", edge -> fst_list [j]);
		}
		if (edge -> num_fsts > 10) {
			fprintf (stderr, "...");
		}
		fprintf (stderr, "\n");
	}

	if (emap -> num_edges > 20) {
		fprintf (stderr, "... (%d more edges)\n", emap -> num_edges - 20);
	}

	fprintf (stderr, "=== END EDGE MAP ===\n\n");
}
