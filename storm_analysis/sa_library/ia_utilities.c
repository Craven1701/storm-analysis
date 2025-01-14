/*
 * Utility functions for image analysis.
 *
 * Hazen 10/17
 */

/* Include */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "multi_fit.h"
#include <string.h>

#ifndef _KDTREE_H_
#define _KDTREE_H_

#ifdef __cplusplus
extern "C" {
#endif

struct kdtree;
struct kdres;


/* create a kd-tree for "k"-dimensional data */
struct kdtree *kd_create(int k);

/* free the struct kdtree */
void kd_free(struct kdtree *tree);

/* remove all the elements from the tree */
void kd_clear(struct kdtree *tree);

/* if called with non-null 2nd argument, the function provided
 * will be called on data pointers (see kd_insert) when nodes
 * are to be removed from the tree.
 */
void kd_data_destructor(struct kdtree *tree, void (*destr)(void*));

/* insert a node, specifying its position, and optional data */
int kd_insert(struct kdtree *tree, const double *pos, void *data);
int kd_insertf(struct kdtree *tree, const float *pos, void *data);
int kd_insert3(struct kdtree *tree, double x, double y, double z, void *data);
int kd_insert3f(struct kdtree *tree, float x, float y, float z, void *data);

/* Find the nearest node from a given point.
 *
 * This function returns a pointer to a result set with at most one element.
 */
struct kdres *kd_nearest(struct kdtree *tree, const double *pos);
struct kdres *kd_nearestf(struct kdtree *tree, const float *pos);
struct kdres *kd_nearest3(struct kdtree *tree, double x, double y, double z);
struct kdres *kd_nearest3f(struct kdtree *tree, float x, float y, float z);

/* Find the N nearest nodes from a given point.
 *
 * This function returns a pointer to a result set, with at most N elements,
 * which can be manipulated with the kd_res_* functions.
 * The returned pointer can be null as an indication of an error. Otherwise
 * a valid result set is always returned which may contain 0 or more elements.
 * The result set must be deallocated with kd_res_free after use.
 */
/*
struct kdres *kd_nearest_n(struct kdtree *tree, const double *pos, int num);
struct kdres *kd_nearest_nf(struct kdtree *tree, const float *pos, int num);
struct kdres *kd_nearest_n3(struct kdtree *tree, double x, double y, double z);
struct kdres *kd_nearest_n3f(struct kdtree *tree, float x, float y, float z);
*/

/* Find any nearest nodes from a given point within a range.
 *
 * This function returns a pointer to a result set, which can be manipulated
 * by the kd_res_* functions.
 * The returned pointer can be null as an indication of an error. Otherwise
 * a valid result set is always returned which may contain 0 or more elements.
 * The result set must be deallocated with kd_res_free after use.
 */
struct kdres *kd_nearest_range(struct kdtree *tree, const double *pos, double range);
struct kdres *kd_nearest_rangef(struct kdtree *tree, const float *pos, float range);
struct kdres *kd_nearest_range3(struct kdtree *tree, double x, double y, double z, double range);
struct kdres *kd_nearest_range3f(struct kdtree *tree, float x, float y, float z, float range);

/* frees a result set returned by kd_nearest_range() */
void kd_res_free(struct kdres *set);

/* returns the size of the result set (in elements) */
int kd_res_size(struct kdres *set);

/* rewinds the result set iterator */
void kd_res_rewind(struct kdres *set);

/* returns non-zero if the set iterator reached the end after the last element */
int kd_res_end(struct kdres *set);

/* advances the result set iterator, returns non-zero on success, zero if
 * there are no more elements in the result set.
 */
int kd_res_next(struct kdres *set);

/* returns the data pointer (can be null) of the current result set item
 * and optionally sets its position to the pointers(s) if not null.
 */
void *kd_res_item(struct kdres *set, double *pos);
void *kd_res_itemf(struct kdres *set, float *pos);
void *kd_res_item3(struct kdres *set, double *x, double *y, double *z);
void *kd_res_item3f(struct kdres *set, float *x, float *y, float *z);

/* equivalent to kd_res_item(set, 0) */
void *kd_res_item_data(struct kdres *set);


#ifdef __cplusplus
}
#endif

#endif	/* _KDTREE_H_ */

#if defined(WIN32) || defined(__WIN32__)
#include <malloc.h>
#endif

#ifdef USE_LIST_NODE_ALLOCATOR

#ifndef NO_PTHREADS
#include <pthread.h>
#else

#ifndef I_WANT_THREAD_BUGS
#error "You are compiling with the fast list node allocator, with pthreads disabled! This WILL break if used from multiple threads."
#endif	/* I want thread bugs */

#endif	/* pthread support */
#endif	/* use list node allocator */

struct kdhyperrect {
	int dim;
	double *min, *max;              /* minimum/maximum coords */
};

struct kdnode {
	double *pos;
	int dir;
	void *data;

	struct kdnode *left, *right;	/* negative/positive side */
};

struct res_node {
	struct kdnode *item;
	double dist_sq;
	struct res_node *next;
};

struct kdtree {
	int dim;
	struct kdnode *root;
	struct kdhyperrect *rect;
	void (*destr)(void*);
};

struct kdres {
	struct kdtree *tree;
	struct res_node *rlist, *riter;
	int size;
};

#define SQ(x)			((x) * (x))


static void clear_rec(struct kdnode *node, void (*destr)(void*));
static int insert_rec(struct kdnode **node, const double *pos, void *data, int dir, int dim);
static int rlist_insert(struct res_node *list, struct kdnode *item, double dist_sq);
static void clear_results(struct kdres *set);

static struct kdhyperrect* hyperrect_create(int dim, const double *min, const double *max);
static void hyperrect_free(struct kdhyperrect *rect);
static struct kdhyperrect* hyperrect_duplicate(const struct kdhyperrect *rect);
static void hyperrect_extend(struct kdhyperrect *rect, const double *pos);
static double hyperrect_dist_sq(struct kdhyperrect *rect, const double *pos);

#ifdef USE_LIST_NODE_ALLOCATOR
static struct res_node *alloc_resnode(void);
static void free_resnode(struct res_node*);
#else
#define alloc_resnode()		malloc(sizeof(struct res_node))
#define free_resnode(n)		free(n)
#endif



struct kdtree *kd_create(int k)
{
	struct kdtree *tree;

	if(!(tree = malloc(sizeof *tree))) {
		return 0;
	}

	tree->dim = k;
	tree->root = 0;
	tree->destr = 0;
	tree->rect = 0;

	return tree;
}

void kd_free(struct kdtree *tree)
{
	if(tree) {
		kd_clear(tree);
		free(tree);
	}
}

static void clear_rec(struct kdnode *node, void (*destr)(void*))
{
	if(!node) return;

	clear_rec(node->left, destr);
	clear_rec(node->right, destr);
	
	if(destr) {
		destr(node->data);
	}
	free(node->pos);
	free(node);
}

void kd_clear(struct kdtree *tree)
{
	clear_rec(tree->root, tree->destr);
	tree->root = 0;

	if (tree->rect) {
		hyperrect_free(tree->rect);
		tree->rect = 0;
	}
}

void kd_data_destructor(struct kdtree *tree, void (*destr)(void*))
{
	tree->destr = destr;
}


static int insert_rec(struct kdnode **nptr, const double *pos, void *data, int dir, int dim)
{
	int new_dir;
	struct kdnode *node;

	if(!*nptr) {
		if(!(node = malloc(sizeof *node))) {
			return -1;
		}
		if(!(node->pos = malloc(dim * sizeof *node->pos))) {
			free(node);
			return -1;
		}
		memcpy(node->pos, pos, dim * sizeof *node->pos);
		node->data = data;
		node->dir = dir;
		node->left = node->right = 0;
		*nptr = node;
		return 0;
	}

	node = *nptr;
	new_dir = (node->dir + 1) % dim;
	if(pos[node->dir] < node->pos[node->dir]) {
		return insert_rec(&(*nptr)->left, pos, data, new_dir, dim);
	}
	return insert_rec(&(*nptr)->right, pos, data, new_dir, dim);
}

int kd_insert(struct kdtree *tree, const double *pos, void *data)
{
	if (insert_rec(&tree->root, pos, data, 0, tree->dim)) {
		return -1;
	}

	if (tree->rect == 0) {
		tree->rect = hyperrect_create(tree->dim, pos, pos);
	} else {
		hyperrect_extend(tree->rect, pos);
	}

	return 0;
}

int kd_insertf(struct kdtree *tree, const float *pos, void *data)
{
	static double sbuf[16];
	double *bptr, *buf = 0;
	int res, dim = tree->dim;

	if(dim > 16) {
#ifndef NO_ALLOCA
		if(dim <= 256)
			bptr = buf = alloca(dim * sizeof *bptr);
		else
#endif
			if(!(bptr = buf = malloc(dim * sizeof *bptr))) {
				return -1;
			}
	} else {
		bptr = buf = sbuf;
	}

	while(dim-- > 0) {
		*bptr++ = *pos++;
	}

	res = kd_insert(tree, buf, data);
#ifndef NO_ALLOCA
	if(tree->dim > 256)
#else
	if(tree->dim > 16)
#endif
		free(buf);
	return res;
}

int kd_insert3(struct kdtree *tree, double x, double y, double z, void *data)
{
	double buf[3];
	buf[0] = x;
	buf[1] = y;
	buf[2] = z;
	return kd_insert(tree, buf, data);
}

int kd_insert3f(struct kdtree *tree, float x, float y, float z, void *data)
{
	double buf[3];
	buf[0] = x;
	buf[1] = y;
	buf[2] = z;
	return kd_insert(tree, buf, data);
}

static int find_nearest(struct kdnode *node, const double *pos, double range, struct res_node *list, int ordered, int dim)
{
	double dist_sq, dx;
	int i, ret, added_res = 0;

	if(!node) return 0;

	dist_sq = 0;
	for(i=0; i<dim; i++) {
		dist_sq += SQ(node->pos[i] - pos[i]);
	}
	if(dist_sq <= SQ(range)) {
		if(rlist_insert(list, node, ordered ? dist_sq : -1.0) == -1) {
			return -1;
		}
		added_res = 1;
	}

	dx = pos[node->dir] - node->pos[node->dir];

	ret = find_nearest(dx <= 0.0 ? node->left : node->right, pos, range, list, ordered, dim);
	if(ret >= 0 && fabs(dx) < range) {
		added_res += ret;
		ret = find_nearest(dx <= 0.0 ? node->right : node->left, pos, range, list, ordered, dim);
	}
	if(ret == -1) {
		return -1;
	}
	added_res += ret;

	return added_res;
}

#if 0
static int find_nearest_n(struct kdnode *node, const double *pos, double range, int num, struct rheap *heap, int dim)
{
	double dist_sq, dx;
	int i, ret, added_res = 0;

	if(!node) return 0;
	
	/* if the photon is close enough, add it to the result heap */
	dist_sq = 0;
	for(i=0; i<dim; i++) {
		dist_sq += SQ(node->pos[i] - pos[i]);
	}
	if(dist_sq <= range_sq) {
		if(heap->size >= num) {
			/* get furthest element */
			struct res_node *maxelem = rheap_get_max(heap);

			/* and check if the new one is closer than that */
			if(maxelem->dist_sq > dist_sq) {
				rheap_remove_max(heap);

				if(rheap_insert(heap, node, dist_sq) == -1) {
					return -1;
				}
				added_res = 1;

				range_sq = dist_sq;
			}
		} else {
			if(rheap_insert(heap, node, dist_sq) == -1) {
				return =1;
			}
			added_res = 1;
		}
	}


	/* find signed distance from the splitting plane */
	dx = pos[node->dir] - node->pos[node->dir];

	ret = find_nearest_n(dx <= 0.0 ? node->left : node->right, pos, range, num, heap, dim);
	if(ret >= 0 && fabs(dx) < range) {
		added_res += ret;
		ret = find_nearest_n(dx <= 0.0 ? node->right : node->left, pos, range, num, heap, dim);
	}

}
#endif

static void kd_nearest_i(struct kdnode *node, const double *pos, struct kdnode **result, double *result_dist_sq, struct kdhyperrect* rect)
{
	int dir = node->dir;
	int i;
	double dummy, dist_sq;
	struct kdnode *nearer_subtree, *farther_subtree;
	double *nearer_hyperrect_coord, *farther_hyperrect_coord;

	/* Decide whether to go left or right in the tree */
	dummy = pos[dir] - node->pos[dir];
	if (dummy <= 0) {
		nearer_subtree = node->left;
		farther_subtree = node->right;
		nearer_hyperrect_coord = rect->max + dir;
		farther_hyperrect_coord = rect->min + dir;
	} else {
		nearer_subtree = node->right;
		farther_subtree = node->left;
		nearer_hyperrect_coord = rect->min + dir;
		farther_hyperrect_coord = rect->max + dir;
	}

	if (nearer_subtree) {
		/* Slice the hyperrect to get the hyperrect of the nearer subtree */
		dummy = *nearer_hyperrect_coord;
		*nearer_hyperrect_coord = node->pos[dir];
		/* Recurse down into nearer subtree */
		kd_nearest_i(nearer_subtree, pos, result, result_dist_sq, rect);
		/* Undo the slice */
		*nearer_hyperrect_coord = dummy;
	}

	/* Check the distance of the point at the current node, compare it
	 * with our best so far */
	dist_sq = 0;
	for(i=0; i < rect->dim; i++) {
		dist_sq += SQ(node->pos[i] - pos[i]);
	}
	if (dist_sq < *result_dist_sq) {
		*result = node;
		*result_dist_sq = dist_sq;
	}

	if (farther_subtree) {
		/* Get the hyperrect of the farther subtree */
		dummy = *farther_hyperrect_coord;
		*farther_hyperrect_coord = node->pos[dir];
		/* Check if we have to recurse down by calculating the closest
		 * point of the hyperrect and see if it's closer than our
		 * minimum distance in result_dist_sq. */
		if (hyperrect_dist_sq(rect, pos) < *result_dist_sq) {
			/* Recurse down into farther subtree */
			kd_nearest_i(farther_subtree, pos, result, result_dist_sq, rect);
		}
		/* Undo the slice on the hyperrect */
		*farther_hyperrect_coord = dummy;
	}
}

struct kdres *kd_nearest(struct kdtree *kd, const double *pos)
{
	struct kdhyperrect *rect;
	struct kdnode *result;
	struct kdres *rset;
	double dist_sq;
	int i;

	if (!kd) return 0;
	if (!kd->rect) return 0;

	/* Allocate result set */
	if(!(rset = malloc(sizeof *rset))) {
		return 0;
	}
	if(!(rset->rlist = alloc_resnode())) {
		free(rset);
		return 0;
	}
	rset->rlist->next = 0;
	rset->tree = kd;

	/* Duplicate the bounding hyperrectangle, we will work on the copy */
	if (!(rect = hyperrect_duplicate(kd->rect))) {
		kd_res_free(rset);
		return 0;
	}

	/* Our first guesstimate is the root node */
	result = kd->root;
	dist_sq = 0;
	for (i = 0; i < kd->dim; i++)
		dist_sq += SQ(result->pos[i] - pos[i]);

	/* Search for the nearest neighbour recursively */
	kd_nearest_i(kd->root, pos, &result, &dist_sq, rect);

	/* Free the copy of the hyperrect */
	hyperrect_free(rect);

	/* Store the result */
	if (result) {
		if (rlist_insert(rset->rlist, result, -1.0) == -1) {
			kd_res_free(rset);
			return 0;
		}
		rset->size = 1;
		kd_res_rewind(rset);
		return rset;
	} else {
		kd_res_free(rset);
		return 0;
	}
}

struct kdres *kd_nearestf(struct kdtree *tree, const float *pos)
{
	static double sbuf[16];
	double *bptr, *buf = 0;
	int dim = tree->dim;
	struct kdres *res;

	if(dim > 16) {
#ifndef NO_ALLOCA
		if(dim <= 256)
			bptr = buf = alloca(dim * sizeof *bptr);
		else
#endif
			if(!(bptr = buf = malloc(dim * sizeof *bptr))) {
				return 0;
			}
	} else {
		bptr = buf = sbuf;
	}

	while(dim-- > 0) {
		*bptr++ = *pos++;
	}

	res = kd_nearest(tree, buf);
#ifndef NO_ALLOCA
	if(tree->dim > 256)
#else
	if(tree->dim > 16)
#endif
		free(buf);
	return res;
}

struct kdres *kd_nearest3(struct kdtree *tree, double x, double y, double z)
{
	double pos[3];
	pos[0] = x;
	pos[1] = y;
	pos[2] = z;
	return kd_nearest(tree, pos);
}

struct kdres *kd_nearest3f(struct kdtree *tree, float x, float y, float z)
{
	double pos[3];
	pos[0] = x;
	pos[1] = y;
	pos[2] = z;
	return kd_nearest(tree, pos);
}

/* ---- nearest N search ---- */
/*
static kdres *kd_nearest_n(struct kdtree *kd, const double *pos, int num)
{
	int ret;
	struct kdres *rset;

	if(!(rset = malloc(sizeof *rset))) {
		return 0;
	}
	if(!(rset->rlist = alloc_resnode())) {
		free(rset);
		return 0;
	}
	rset->rlist->next = 0;
	rset->tree = kd;

	if((ret = find_nearest_n(kd->root, pos, range, num, rset->rlist, kd->dim)) == -1) {
		kd_res_free(rset);
		return 0;
	}
	rset->size = ret;
	kd_res_rewind(rset);
	return rset;
}*/

struct kdres *kd_nearest_range(struct kdtree *kd, const double *pos, double range)
{
	int ret;
	struct kdres *rset;

	if(!(rset = malloc(sizeof *rset))) {
		return 0;
	}
	if(!(rset->rlist = alloc_resnode())) {
		free(rset);
		return 0;
	}
	rset->rlist->next = 0;
	rset->tree = kd;

	if((ret = find_nearest(kd->root, pos, range, rset->rlist, 0, kd->dim)) == -1) {
		kd_res_free(rset);
		return 0;
	}
	rset->size = ret;
	kd_res_rewind(rset);
	return rset;
}

struct kdres *kd_nearest_rangef(struct kdtree *kd, const float *pos, float range)
{
	static double sbuf[16];
	double *bptr, *buf = 0;
	int dim = kd->dim;
	struct kdres *res;

	if(dim > 16) {
#ifndef NO_ALLOCA
		if(dim <= 256)
			bptr = buf = alloca(dim * sizeof *bptr);
		else
#endif
			if(!(bptr = buf = malloc(dim * sizeof *bptr))) {
				return 0;
			}
	} else {
		bptr = buf = sbuf;
	}

	while(dim-- > 0) {
		*bptr++ = *pos++;
	}

	res = kd_nearest_range(kd, buf, range);
#ifndef NO_ALLOCA
	if(kd->dim > 256)
#else
	if(kd->dim > 16)
#endif
		free(buf);
	return res;
}

struct kdres *kd_nearest_range3(struct kdtree *tree, double x, double y, double z, double range)
{
	double buf[3];
	buf[0] = x;
	buf[1] = y;
	buf[2] = z;
	return kd_nearest_range(tree, buf, range);
}

struct kdres *kd_nearest_range3f(struct kdtree *tree, float x, float y, float z, float range)
{
	double buf[3];
	buf[0] = x;
	buf[1] = y;
	buf[2] = z;
	return kd_nearest_range(tree, buf, range);
}

void kd_res_free(struct kdres *rset)
{
	clear_results(rset);
	free_resnode(rset->rlist);
	free(rset);
}

int kd_res_size(struct kdres *set)
{
	return (set->size);
}

void kd_res_rewind(struct kdres *rset)
{
	rset->riter = rset->rlist->next;
}

int kd_res_end(struct kdres *rset)
{
	return rset->riter == 0;
}

int kd_res_next(struct kdres *rset)
{
	rset->riter = rset->riter->next;
	return rset->riter != 0;
}

void *kd_res_item(struct kdres *rset, double *pos)
{
	if(rset->riter) {
		if(pos) {
			memcpy(pos, rset->riter->item->pos, rset->tree->dim * sizeof *pos);
		}
		return rset->riter->item->data;
	}
	return 0;
}

void *kd_res_itemf(struct kdres *rset, float *pos)
{
	if(rset->riter) {
		if(pos) {
			int i;
			for(i=0; i<rset->tree->dim; i++) {
				pos[i] = rset->riter->item->pos[i];
			}
		}
		return rset->riter->item->data;
	}
	return 0;
}

void *kd_res_item3(struct kdres *rset, double *x, double *y, double *z)
{
	if(rset->riter) {
		if(*x) *x = rset->riter->item->pos[0];
		if(*y) *y = rset->riter->item->pos[1];
		if(*z) *z = rset->riter->item->pos[2];
	}
	return 0;
}

void *kd_res_item3f(struct kdres *rset, float *x, float *y, float *z)
{
	if(rset->riter) {
		if(*x) *x = rset->riter->item->pos[0];
		if(*y) *y = rset->riter->item->pos[1];
		if(*z) *z = rset->riter->item->pos[2];
	}
	return 0;
}

void *kd_res_item_data(struct kdres *set)
{
	return kd_res_item(set, 0);
}

/* ---- hyperrectangle helpers ---- */
static struct kdhyperrect* hyperrect_create(int dim, const double *min, const double *max)
{
	size_t size = dim * sizeof(double);
	struct kdhyperrect* rect = 0;

	if (!(rect = malloc(sizeof(struct kdhyperrect)))) {
		return 0;
	}

	rect->dim = dim;
	if (!(rect->min = malloc(size))) {
		free(rect);
		return 0;
	}
	if (!(rect->max = malloc(size))) {
		free(rect->min);
		free(rect);
		return 0;
	}
	memcpy(rect->min, min, size);
	memcpy(rect->max, max, size);

	return rect;
}

static void hyperrect_free(struct kdhyperrect *rect)
{
	free(rect->min);
	free(rect->max);
	free(rect);
}

static struct kdhyperrect* hyperrect_duplicate(const struct kdhyperrect *rect)
{
	return hyperrect_create(rect->dim, rect->min, rect->max);
}

static void hyperrect_extend(struct kdhyperrect *rect, const double *pos)
{
	int i;

	for (i=0; i < rect->dim; i++) {
		if (pos[i] < rect->min[i]) {
			rect->min[i] = pos[i];
		}
		if (pos[i] > rect->max[i]) {
			rect->max[i] = pos[i];
		}
	}
}

static double hyperrect_dist_sq(struct kdhyperrect *rect, const double *pos)
{
	int i;
	double result = 0;

	for (i=0; i < rect->dim; i++) {
		if (pos[i] < rect->min[i]) {
			result += SQ(rect->min[i] - pos[i]);
		} else if (pos[i] > rect->max[i]) {
			result += SQ(rect->max[i] - pos[i]);
		}
	}

	return result;
}

/* ---- static helpers ---- */

#ifdef USE_LIST_NODE_ALLOCATOR
/* special list node allocators. */
static struct res_node *free_nodes;

#ifndef NO_PTHREADS
static pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static struct res_node *alloc_resnode(void)
{
	struct res_node *node;

#ifndef NO_PTHREADS
	pthread_mutex_lock(&alloc_mutex);
#endif

	if(!free_nodes) {
		node = malloc(sizeof *node);
	} else {
		node = free_nodes;
		free_nodes = free_nodes->next;
		node->next = 0;
	}

#ifndef NO_PTHREADS
	pthread_mutex_unlock(&alloc_mutex);
#endif

	return node;
}

static void free_resnode(struct res_node *node)
{
#ifndef NO_PTHREADS
	pthread_mutex_lock(&alloc_mutex);
#endif

	node->next = free_nodes;
	free_nodes = node;

#ifndef NO_PTHREADS
	pthread_mutex_unlock(&alloc_mutex);
#endif
}
#endif	/* list node allocator or not */


/* inserts the item. if dist_sq is >= 0, then do an ordered insert */
/* TODO make the ordering code use heapsort */
static int rlist_insert(struct res_node *list, struct kdnode *item, double dist_sq)
{
	struct res_node *rnode;

	if(!(rnode = alloc_resnode())) {
		return -1;
	}
	rnode->item = item;
	rnode->dist_sq = dist_sq;

	if(dist_sq >= 0.0) {
		while(list->next && list->next->dist_sq < dist_sq) {
			list = list->next;
		}
	}
	rnode->next = list->next;
	list->next = rnode;
	return 0;
}

static void clear_results(struct kdres *rset)
{
	struct res_node *tmp, *node = rset->rlist->next;

	while(node) {
		tmp = node;
		node = node->next;
		free_resnode(tmp);
	}

	rset->rlist->next = 0;
}

/*
 * This is matched in Python. A description of the fields is
 * provided with the Python version.
 */
typedef struct flmData
{
  int margin;
  int n_peaks;
  int z_range;
  
  int xsize;
  int ysize;
  int zsize;

  double radius;
  double threshold;
  
  double *z_values;

  int32_t **taken;
  double **images;
} flmData;


/* Function Declarations */
int calcMaxPeaks(flmData *);
struct kdtree *createKDTree(double *, double *, int);
void findLocalMaxima(flmData *, double *, double *, double *, double *);
void freeKDTree(struct kdtree *);
int isLocalMaxima(flmData *, double, int, int, int, int, int, int, int, int);
int markDimmerPeaks(double *, double *, double *, int32_t *, double, double, int);
int markLowSignificancePeaks(double *, double *, double *, int32_t *, double, double, int);
void nearestKDTree(struct kdtree *, double *, double *, double *, int32_t *, double, int);
void runningIfHasNeighbors(double *, double *, double *, double *, int32_t *, double, int, int);


/*
 * calcMaxPeaks()
 *
 * Return the maximum number of peaks that could be in an image stack. This 
 * is just the number of pixels above threshold.
 */
int calcMaxPeaks(flmData *flm_data)
{
  int np,xi,yi,zi;

  np = 0;
  for(zi=0;zi<flm_data->zsize;zi++){
    for(yi=flm_data->margin;yi<(flm_data->ysize - flm_data->margin);yi++){
      for(xi=flm_data->margin;xi<(flm_data->xsize - flm_data->margin);xi++){
	if(flm_data->images[zi][yi*flm_data->xsize+xi]>flm_data->threshold){
	  if(flm_data->taken[zi][yi*flm_data->xsize+xi]<1){
	    np++;
	  }
	}
      }
    }
  }
  return np;
}

/*
 * createKDTree()
 *
 * Create a KD tree from two arrays of positions.
 */
struct kdtree *createKDTree(double *x, double *y, int n)
{
  int i;
  double pos[2];
  struct kdtree *kd;

  kd = kd_create(2);
  for(i=0;i<n;i++){
    pos[0] = x[i];
    pos[1] = y[i];
    kd_insert(kd, pos, (void *)(intptr_t)i);
  }

  return kd;
}

/*
 * findLocalMaxima()
 *
 * Finds the locations of all the local maxima in a stack of images with
 * intensity greater than threshold. Adds them to the list if that location 
 * has not already been used.
 */
void findLocalMaxima(flmData *flm_data, double *z, double *y, double *x, double *h)
{
  int np,xi,yi,zi;
  int ex,ey,ez,sx,sy,sz;
  double cur;

  np = 0;
  for(zi=0;zi<flm_data->zsize;zi++){

    /* Set z search range. */
    sz = zi - flm_data->z_range;
    if(sz<0){ sz = 0;}
    ez = zi + flm_data->z_range;
    if(ez>=flm_data->zsize){ ez = flm_data->zsize-1; }
    
    for(yi=flm_data->margin;yi<(flm_data->ysize - flm_data->margin);yi++){

      /* Set y search range. */
      sy = yi - flm_data->radius;
      if(sy<0){ sy = 0; }
      ey = yi + flm_data->radius;
      if(ey>=flm_data->ysize){ ey = flm_data->ysize-1; }

      for(xi=flm_data->margin;xi<(flm_data->xsize - flm_data->margin);xi++){
	if(flm_data->images[zi][yi*flm_data->xsize+xi]>flm_data->threshold){
	  if(flm_data->taken[zi][yi*flm_data->xsize+xi]<1){

	    /* Set x search range. */
	    sx = xi - ceil(flm_data->radius);
	    if(sx<0){ sx = 0; }
	    ex = xi + ceil(flm_data->radius);
	    if(ex>=flm_data->xsize){ ex = flm_data->xsize-1; }

	    cur = flm_data->images[zi][yi*flm_data->xsize+xi];
	    if(isLocalMaxima(flm_data, cur, sz, ez, sy, yi, ey, sx, xi, ex)){
	      flm_data->taken[zi][yi*flm_data->xsize+xi]++;
	      z[np] = flm_data->z_values[zi];
	      y[np] = yi;
	      x[np] = xi;
	      h[np] = cur;
	      np++;
	    }

	    if (np >= flm_data->n_peaks){
	      printf("Warning! Found maximum number of peaks!\n");
	      return;
	    }
	  }
	}
      }
    }
  }

  flm_data->n_peaks = np;
}

/*
 * freeKDTree()
 *
 * Frees an existing kdtree.
 */
void freeKDTree(struct kdtree *kd)
{
  kd_free(kd);
}

/*
 * isLocalMaxima()
 *
 * Does a local search to check if the current pixel is a maximum. The search area
 * is a cylinder with it's axis pointed along the z axis.
 */
int isLocalMaxima(flmData *flm_data, double cur, int sz, int ez, int sy, int cy, int ey, int sx, int cx, int ex)
{
  int dx,dy,rr,xi,yi,zi;

  rr = flm_data->radius * flm_data->radius;
  
  for(zi=sz;zi<=ez;zi++){
    for(yi=sy;yi<=ey;yi++){
      dy = (yi - cy)*(yi - cy);
      for(xi=sx;xi<=ex;xi++){
	dx = (xi - cx)*(xi - cx);
	if((dx+dy)<=rr){

	  /*
	   * This is supposed to deal with two pixels that have exactly the same intensity
	   * and that are within radius of each other. In this case we'll choose the one
	   * with greater xi,yi. Note also that this order is such that we avoid the problem
	   * of the pixel not being greater than itself without explicitly testing for
	   * this condition.
	   */
	  if((yi<=cy)&&(xi<=cx)){
	    if(flm_data->images[zi][yi*flm_data->xsize+xi]>cur){
	      return 0;
	    }
	  }
	  else{
	    if(flm_data->images[zi][yi*flm_data->xsize+xi]>=cur){
	      return 0;
	    }
	  }
	}
      }
    }
  }
  return 1;
}

/*
 * markDimmerPeaks()
 *
 * For each peak, check if it has a brighter neighbor within radius, and if it
 * does mark the peak for removal (by setting the status to ERROR) and the 
 * neighbors as running.
 */
int markDimmerPeaks(double *x, double *y, double *h, int32_t *status, double r_removal, double r_neighbors, int np)
{
  int i,j,k;
  int is_dimmer, removed;
  double pos[2];
  struct kdres *set_r, *set_n;
  struct kdtree *kd;

  removed = 0;
  kd = createKDTree(x, y, np);

  for(i=0;i<np;i++){

    /* Skip error peaks. */
    if(status[i] == ERROR){
      continue;
    }

    /* Check for neighbors within r_removal. */
    pos[0] = x[i];
    pos[1] = y[i];
    set_r = kd_nearest_range(kd, pos, r_removal);

    /* Every point will have at least itself as a neighbor. */
    if(kd_res_size(set_r) < 2){
      kd_res_free(set_r);
      continue;
    }

    /* Check for brighter neighbors. */
    is_dimmer = 0;
    for(j=0;j<kd_res_size(set_r);j++){
      k = (intptr_t)kd_res_item_data(set_r);
      if(h[k] > h[i]){
	is_dimmer = 1;
	break;
      }
      kd_res_next(set_r);
    }
    kd_res_free(set_r);

    if(is_dimmer){
      removed++;
      status[i] = ERROR;

      /* Check for neighbors within r_neighbors. */
      set_n = kd_nearest_range(kd, pos, r_neighbors);
      for(j=0;j<kd_res_size(set_n);j++){
	k = (intptr_t)kd_res_item_data(set_n);
	if (status[k] == CONVERGED){
	  status[k] = RUNNING;
	}
	kd_res_next(set_n);
      }
      kd_res_free(set_n);
    }    
  }

  freeKDTree(kd);

  return removed;
}


/*
 * markLowSignificancePeaks()
 *
 * For each peak, check if it is above the minimum significance value. If
 * it is not mark the peak for removal (by setting the status to ERROR) and 
 * the neighbors as running.
 */
int markLowSignificancePeaks(double *x, double *y, double *sig, int32_t *status, double min_sig, double r_neighbors, int np)
{
  int i,j,k;
  int removed;
  double pos[2];
  struct kdres *set_n;
  struct kdtree *kd;

  removed = 0;
  kd = createKDTree(x, y, np);

  for(i=0;i<np;i++){

    /* Skip error peaks. */
    if(status[i] == ERROR){
      continue;
    }

    /* Check for minimum significance. */
    if(sig[i] > min_sig){
      continue;
    }

    /* Mark for removal & increment counter. */
    status[i] = ERROR;
    removed += 1;

    /* Check for neighbors within r_neighbors. */    
    pos[0] = x[i];
    pos[1] = y[i];
    
    set_n = kd_nearest_range(kd, pos, r_neighbors);
    for(j=0;j<kd_res_size(set_n);j++){
      k = (intptr_t)kd_res_item_data(set_n);
      if (status[k] == CONVERGED){
	status[k] = RUNNING;
      }
      kd_res_next(set_n);
    }
    kd_res_free(set_n);
  }

  freeKDTree(kd);

  return removed;
}


/*
 * nearestKDTree()
 *
 * Return the distance to and index of the nearest point in a KDTree to each
 * of point. If there are no points in the KDTree within the search radius
 * return -1.0 for the distance and -1 for the index.
 */
void nearestKDTree(struct kdtree *kd, double *x, double *y, double *dist, int32_t *index, double radius, int n)
{
  int i,j,min_i;
  double dd,dx,dy,min_dd,pos[2];
  struct kdres *set;

  for(i=0;i<n;i++){

    /* Query KD tree. */
    pos[0] = x[i];
    pos[1] = y[i];    
    set = kd_nearest_range(kd, pos, radius);

    /* 
     * Go through results and find the closest point. I am assuming, that
     * the result set is not ordered by distance.
     */
    min_i = -1;
    min_dd = radius * radius + 0.1;
    for(j=0;j<kd_res_size(set);j++){
      kd_res_item(set, pos);
      dx = pos[0] - x[i];
      dy = pos[1] - y[i];
      dd = dx*dx + dy*dy;
      if(dd < min_dd){
	min_dd = dd;
	min_i = (intptr_t)kd_res_item_data(set);
      }
      kd_res_next(set);
    }

    if(min_i >= 0){
      dist[i] = sqrt(min_dd);
      index[i] = min_i;
    }
    else {
      dist[i] = -1.0;
      index[i] = -1;
    }

    kd_res_free(set);
  }
}


/*
 * runningIfHasNeighbors()
 *
 * Update status based on proximity of new peaks (n_x, n_y) to current peaks (c_x, c_y).
 *
 * This works the simplest way by making a KD tree from the new peaks then comparing
 * the old peaks against this tree. However this might not be the fastest way given
 * that there will likely be a lot more current peaks then new peaks.
 */
void runningIfHasNeighbors(double *c_x, double *c_y, double *n_x, double *n_y, int32_t *status, double radius, int nc, int nn)
{
  int i;
  double pos[2];
  struct kdres *set;
  struct kdtree *kd;

  kd = createKDTree(n_x, n_y, nn);
  
  for(i=0;i<nc;i++){

    /* Skip RUNNING and ERROR peaks. */
    if((status[i] == RUNNING) || (status[i] == ERROR)){
      continue;
    }
    
    /* Check for new neighbors within radius. */
    pos[0] = c_x[i];
    pos[1] = c_y[i];
    set = kd_nearest_range(kd, pos, radius);

    if (kd_res_size(set) > 0){
      status[i] = RUNNING;
    }
    
    kd_res_free(set);
  }
  
  freeKDTree(kd);
}

/*
 * The MIT License
 *
 * Copyright (c) 2017 Zhuang Lab, Harvard University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
