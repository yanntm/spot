#ifndef NB_OPEN_SET_H
#define NB_OPEN_SET_H

#include <stdio.h>
#include "hashtable.h"


//
// This struture represents the open set  list used in the
// Global Open Set structure
//
typedef struct open_set_node_
{
  struct open_set_node_ *next;
  void *                 data;
} open_set_node_t;

typedef struct open_set_
{
  hashtable_t      *table_;
  open_set_node_t  *head_;
  open_set_node_t  **to_free_;
} open_set_t;



open_set_t*   open_set_alloc(const datatype_t *key_type,
			     size_t size, size_t tn);
void          open_set_free(open_set_t* os, int verbose, size_t tn);
int           open_set_find_or_put(open_set_t* os, map_key_t key);
void*         open_set_get_one(open_set_t* os, int tn);


#endif // NB_OPEN_SET_H
