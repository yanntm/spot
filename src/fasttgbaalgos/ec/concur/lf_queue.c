#include <atomics.h>
#include <lf_queue.h>
#include <hashtable.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


lf_queue_t* lf_queue_alloc()
{
  lf_queue_t* os_ = (lf_queue_t*) malloc(sizeof(lf_queue_t));
  os_->head_ = 0;
  return os_;
}

void lf_queue_free(lf_queue_t* os)
{
  assert(os->head_ == 0);
  free(os);
}


int lf_queue_put(lf_queue_t* os, void* data)
{
  // Now the value must be insert in the open set
  lf_queue_node_t* old_node;
  lf_queue_node_t* tmp;
  lf_queue_node_t* node =
    (lf_queue_node_t*) malloc (sizeof(lf_queue_node_t));
  node->data = (void*)data;
  node->next = 0;

  do
    {
      tmp = ((lf_queue_t)atomic_read(os)).head_;//os->head_;
      node->next = tmp;
      old_node = cas_ret(&os->head_, tmp, node);
    }
  while (old_node != tmp);

  return 0;

  /* lf_queue_node_t* old_node; */
  /* lf_queue_node_t* tmp; */
  /* lf_queue_node_t* node = */
  /*   (lf_queue_node_t*) malloc (sizeof(lf_queue_node_t)); */
  /* node->data = (void*)data; */
  /* node->next = 0; */

  /* lf_queue_node_t* current = ((lf_queue_t)atomic_read(os)).head_; */

  /* // The queue is empty! */
  /* if (!current) */
  /*   { */
  /*     old_node = cas_ret(&os->head_, current, node); */
  /*     if (old_node == current) */
  /* 	return 0; */
  /*     //current = old_node; */
  /*     current = ((lf_queue_t)atomic_read(os)).head_; */
  /*   } */

  /* // Otherwise walk the whole queue to put element at the end. */
  /* while (current) */
  /*   { */
  /*     tmp = ((lf_queue_node_t)atomic_read(current)).next; */

  /*     // End of the queue we can try insert */
  /*     if (tmp == 0) */
  /* 	{ */
  /* 	  old_node = cas_ret(&current->next, 0, node); */

  /* 	  if (old_node == tmp) */
  /* 	    return 0; */
  /* 	  // We have to walk the list again */
  /* 	  current = ((lf_queue_t)atomic_read(os)).head_; */
  /* 	} */
  /*     else */
  /* 	{ */
  /* 	// progress in the list */
  /* 	  current = tmp; */
  /* 	} */
  /*   } */
  /* return 0;// FIXME useful ? */
}

void* lf_queue_get_one(lf_queue_t* os)
{
  lf_queue_node_t* old_node;
  lf_queue_node_t* tmp;
  //    lf_queue_node_t* tmp2;
  lf_queue_node_t* current;
  current = ((lf_queue_t)atomic_read(os)).head_;

  // Empty queue
  /* if (!current) */
  /*   return 0; */


  /* // Here we know that the list contains at least 2 elts */
  /* do */
  /* { */
  /*   tmp1 = ((lf_queue_node_t)atomic_read(current)).next; */
  /*   if (!tmp1) */
  /*     { */
  /* 	old_node = cas_ret(&os->head_, current, 0); */
  /* 	if (old_node == current) */
  /* 	  { */
  /* 	    void *result = current->data; */
  /* 	    free (current); */
  /* 	    return result; */
  /* 	  } */
  /* 	current = old_node; */
  /* 	// A new elt has been pushed! */
  /* 	continue; */
  /*     } */

  /*   tmp2 = ((lf_queue_node_t)atomic_read(tmp1)).next; */
  /*   if (!tmp2) */
  /*     { */
  /* 	old_node = cas_ret(&current->next, tmp1, 0); */
  /* 	if (old_node == tmp1) */
  /* 	  { */
  /* 	    void *result = tmp1->data; */
  /* 	    free (tmp1); */
  /* 	    return result; */
  /* 	  } */
  /* 	current = old_node; */
  /* 	continue; */
  /*     } */
  /*   // progress */
  /*   current = tmp1; */
  /* } while(1); */




  while (current)
    {
      tmp = ((lf_queue_node_t)atomic_read(current)).next;
      old_node = cas_ret(&os->head_, current, tmp);

      if (old_node == current)
  	{
  	  void *result = current->data;
  	  free (current);
  	  return result;
  	}

      current = old_node;
    }
  return 0;
}
