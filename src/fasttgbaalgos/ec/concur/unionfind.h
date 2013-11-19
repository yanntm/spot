#ifndef NB_UNIONFIND_H
#define NB_UNIONFIND_H

#include <stdio.h>
#include "hashtable.h"


//
// This struture represents the parent list used in the
// Union Find
//
typedef struct uf_node_
{
  struct uf_node_ *parent;
  unsigned long    markset;
} uf_node_t;

typedef struct uf_
{
  hashtable_t *table;
  uf_node_t *dead_;
  uf_node_t **prealloc_;
} uf_t;



uf_t*         uf_alloc(const datatype_t *key_type, size_t size, size_t tn);
void          uf_free(uf_t* uf, int verbose, size_t tn);
uf_node_t*    uf_find(uf_node_t* n);
uf_node_t*    uf_unite(uf_t* uf, uf_node_t* left, uf_node_t* right,
		       unsigned long* acc);
unsigned long uf_add_acc(uf_t* uf, uf_node_t* n, unsigned long acc);
uf_node_t*    uf_make_set(uf_t* uf, map_key_t key, int *inserted, int tn);
void          uf_make_dead(uf_t* uf, uf_node_t* n);
int           uf_is_dead(uf_t* uf, uf_node_t* n);
void          print_uf_node_t (void *node);

#endif // NB_UNIONFIND_H
