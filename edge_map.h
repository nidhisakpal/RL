/***********************************************************************

	File:	edge_map.h
	Rev:	1.0
	Date:	2025-01-18

	Multi-Temporal Optimization: Edge Enumeration Infrastructure

	This module builds a global mapping of all unique edges across all
	Full Steiner Trees (FSTs) in the hypergraph. This is needed for the
	multi-temporal optimization where we need to track which edges are
	active at each time period using binary variables Z[e,t].

************************************************************************/

#ifndef EDGE_MAP_H
#define EDGE_MAP_H

struct gst_hypergraph;
struct full_set;

/*
 * Structure representing a single unique edge.
 * An edge is defined by its two endpoints (terminals or Steiner points).
 */
struct edge_info {
	int		p1;		/* First endpoint (smaller index) */
	int		p2;		/* Second endpoint (larger index) */
	double		length;		/* Edge length (for reference) */
	int		num_fsts;	/* Number of FSTs containing this edge */
	int		fst_capacity;	/* Allocated capacity for fst_list */
	int *		fst_list;	/* Array of FST indices containing this edge */
};

/*
 * Hash table node for fast edge lookup.
 * Maps (p1, p2) -> edge_index
 */
struct edge_hash_node {
	int			p1;		/* Edge endpoint 1 */
	int			p2;		/* Edge endpoint 2 */
	int			edge_index;	/* Index in edges array */
	struct edge_hash_node *	next;		/* Collision chain */
};

/*
 * Global edge map structure.
 * Contains all unique edges across all FSTs and provides fast lookup.
 */
struct edge_map {
	int			num_edges;	/* Total number of unique edges */
	int			edge_capacity;	/* Allocated capacity for edges array */
	struct edge_info *	edges;		/* Array of edge information */

	/* Hash table for fast edge lookup */
	int			hash_size;	/* Hash table size */
	struct edge_hash_node **hash_table;	/* Hash table buckets */
};

/*
 * Global Routines
 */

/* Build edge map from hypergraph's FSTs */
extern struct edge_map *	build_edge_map (struct gst_hypergraph * cip);

/* Free edge map and all associated memory */
extern void			free_edge_map (struct edge_map * emap);

/* Lookup edge index by endpoints (returns -1 if not found) */
extern int			lookup_edge (struct edge_map * emap, int p1, int p2);

/* Get FST list for a given edge index */
extern int *			get_edge_fsts (struct edge_map * emap,
						       int edge_index,
						       int * num_fsts_out);

/* Print edge map statistics and contents (debug) */
extern void			print_edge_map (struct edge_map * emap);

#endif /* EDGE_MAP_H */
