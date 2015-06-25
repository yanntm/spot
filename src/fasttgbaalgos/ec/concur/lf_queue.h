#ifndef NB_LF_QUEUE_H
#define NB_LF_QUEUE_H

#include <stdio.h>
#include "hashtable.h"


//
// This struture represents a lockfree queue
//
typedef struct lf_queue_node_
{
  struct lf_queue_node_ *next;
  void *                 data;
} lf_queue_node_t;

typedef struct lf_queue_
{
  lf_queue_node_t  *head_;
} lf_queue_t;



lf_queue_t*   lf_queue_alloc();
void          lf_queue_free(lf_queue_t* os);
int           lf_queue_put(lf_queue_t* os, void* key);
void*         lf_queue_get_one(lf_queue_t* os);


#endif // NB_LF_QUEUE_H
