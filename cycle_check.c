/*
 * Cycle detection for battery-aware network optimization
 * Validates integer solutions to ensure no cycles exist
 */

#include "bb.h"
#include "constrnt.h"
#include "geosteiner.h"
#include "logic.h"
#include "memory.h"
#include "steiner.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Union-Find data structure for cycle detection
 */
typedef struct {
	int* parent;
	int* rank;
	int size;
} union_find_t;

	static
	union_find_t*
create_union_find(int n)
{
	union_find_t* uf = NEW(union_find_t);
	uf->parent = NEWA(n, int);
	uf->rank = NEWA(n, int);
	uf->size = n;

	for (int i = 0; i < n; i++) {
		uf->parent[i] = i;
		uf->rank[i] = 0;
	}

	return uf;
}

	static
	void
free_union_find(union_find_t* uf)
{
	if (uf) {
		free(uf->parent);
		free(uf->rank);
		free(uf);
	}
}

	static
	int
uf_find(union_find_t* uf, int x)
{
	if (uf->parent[x] != x) {
		uf->parent[x] = uf_find(uf, uf->parent[x]);  /* Path compression */
	}
	return uf->parent[x];
}

	static
	bool
uf_union(union_find_t* uf, int x, int y)
{
	int root_x = uf_find(uf, x);
	int root_y = uf_find(uf, y);

	if (root_x == root_y) {
		return FALSE;  /* Already in same set - would create cycle */
	}

	/* Union by rank */
	if (uf->rank[root_x] < uf->rank[root_y]) {
		uf->parent[root_x] = root_y;
	} else if (uf->rank[root_x] > uf->rank[root_y]) {
		uf->parent[root_y] = root_x;
	} else {
		uf->parent[root_y] = root_x;
		uf->rank[root_x]++;
	}

	return TRUE;
}

/*
 * Check if the integer solution contains cycles by examining selected FSTs.
 * Returns a violated SEC constraint if a cycle is found, NULL otherwise.
 */
	struct constraint*
_gst_check_integer_solution_for_cycles(
double*			x,		/* IN - integer LP solution */
struct bbinfo*		bbip		/* IN - branch-and-bound info */
)
{
	int			i, j, k;
	int			nedges;
	int			nverts;
	int*			vp1;
	int*			vp2;
	union_find_t*		uf;
	struct constraint*	cp;
	bitmap_t*		cycle_verts;
	int			num_cycle_verts;
	int			kmasks;
	struct gst_hypergraph*	cip;

	cip = bbip -> cip;
	nedges = cip -> num_edges;
	nverts = cip -> num_verts;
	kmasks = BMAP_ELTS(nverts);

	/* Create union-find structure for cycle detection */
	uf = create_union_find(nverts);

	/* Track which vertices are covered by selected FSTs */
	cycle_verts = NEWA(kmasks, bitmap_t);
	for (i = 0; i < kmasks; i++) {
		cycle_verts[i] = 0;
	}

	/* Process each selected FST (x[i] >= 0.5) */
	for (i = 0; i < nedges; i++) {
		if (x[i] < 0.5) continue;

		/* Get vertices in this FST */
		vp1 = cip -> edge[i];
		vp2 = cip -> edge[i + 1];

		/* Mark all vertices in this FST */
		while (vp1 < vp2) {
			j = *vp1++;
			SETBIT(cycle_verts, j);
		}

		/* Try to add edges from this FST to the union-find structure */
		/* For an FST with k vertices, we need k-1 edges to form a tree */
		/* If adding any edge creates a cycle, we have a violation */
		vp1 = cip -> edge[i];
		vp2 = cip -> edge[i + 1];

		int num_verts_in_fst = vp2 - vp1;

		/* Connect all vertices in this FST in star topology */
		/* (all vertices connect to the first vertex) */
		if (num_verts_in_fst >= 2) {
			int root = *vp1;
			vp1++;

			while (vp1 < vp2) {
				j = *vp1++;

				/* Try to union root and j */
				if (!uf_union(uf, root, j)) {
					/* Cycle detected! Build SEC constraint */
					fprintf(stderr, "CYCLE DETECTED: FST %d creates cycle between vertices %d and %d\n",
						i, root, j);

					/* Find all vertices in the cycle component */
					bitmap_t* sec_mask = NEWA(kmasks, bitmap_t);
					for (k = 0; k < kmasks; k++) {
						sec_mask[k] = 0;
					}

					int cycle_root = uf_find(uf, root);
					num_cycle_verts = 0;

					for (k = 0; k < nverts; k++) {
						if (BITON(cycle_verts, k) && uf_find(uf, k) == cycle_root) {
							SETBIT(sec_mask, k);
							num_cycle_verts++;
						}
					}

					fprintf(stderr, "  Cycle involves %d vertices\n", num_cycle_verts);

					/* Build SEC constraint for this cycle */
					cp = NEW(struct constraint);
					cp->next = NULL;
					cp->iteration = -1;
					cp->type = CT_SUBTOUR;
					cp->mask = sec_mask;

					free_union_find(uf);
					free(cycle_verts);

					return cp;
				}
			}
		}
	}

	/* No cycles found */
	free_union_find(uf);
	free(cycle_verts);

	return NULL;
}
