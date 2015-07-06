#ifndef NB_LF_QUEUE_H
#define NB_LF_QUEUE_H

#include <stdio.h>
#include "hashtable.h"


//
// This struture represents a lockfree queue
// Adaptation of following  for single producer single consumer
//
// Simple, Fast, and Practical Non-Blocking and
// Blocking Concurrent Queue Algorithms
//
enum op_type
  {
    makeset = 2,		// Ask for creation
    unite = 4,			// Ask for unite operation
    makedead = 8,		// Ask for marking dead
    the_end = 16		// This thread is iddle
  };

typedef struct lf_sop_
{
  enum op_type  op_;
  void*         arg1_;
  void*         arg2_;
  unsigned long acc_;
} shared_op;



typedef struct lf_queue_node_
{
  struct lf_queue_node_ *next_;
  shared_op              data_;
} lf_queue_node_t;

typedef struct lf_queue_
{
  lf_queue_node_t  *head_;
  lf_queue_node_t  *tail_;
  lf_queue_node_t  *prealloc_;
  int prealloc_idx_;
  int prealloc_size_;
  lf_queue_node_t  *prealloc_first_;
} lf_queue_t;



lf_queue_t*   lf_queue_alloc();
void          lf_queue_free(lf_queue_t* os);
int           lf_queue_put(lf_queue_t* os, int op, void* arg1, void* arg2,
			   unsigned long acc);
void*         lf_queue_get_one(lf_queue_t* os);


#endif // NB_LF_QUEUE_H
