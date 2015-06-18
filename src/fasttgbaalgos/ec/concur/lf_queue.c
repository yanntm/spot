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

void lf_queue_free(lf_queue_t* os, int verbose)
{
  /* Now Destroy the lf_queue . This is thread safe
   */
  lf_queue_node_t *curr = os->head_;
  if (!curr)
    goto end;

  while (curr->next != 0)
    {
      lf_queue_node_t *tmp = curr->next;
      // FIXME delete curr->data?
      free(curr);
      curr = tmp;
    }
 end:
  free(curr);
}


int lf_queue_put(lf_queue_t* os, void* data)
{
  lf_queue_node_t* old_node;
  lf_queue_node_t* tmp;
  lf_queue_node_t* node =
    (lf_queue_node_t*) malloc (sizeof(lf_queue_node_t));
  node->data = (void*)data;

  lf_queue_node_t* current = ((lf_queue_t)atomic_read(os)).head_;
  while (current)
    {
      tmp = ((lf_queue_node_t)atomic_read(current)).next;//current->next;


      // End of the queue we can try insert
      if (tmp == 0)
	{
	  old_node = cas_ret(&current->next, tmp, node);
	  if (old_node == node)
	    return 0;

	  // We have to walk the list again
	  current = ((lf_queue_t)atomic_read(os)).head_;
	}
      else
	// progress in the list
	current = tmp;
    }
  return 0;// FIXME useful ?
}

void* lf_queue_get_one(lf_queue_t* os)
{
  lf_queue_node_t* old_node;
  lf_queue_node_t* tmp;
  lf_queue_node_t* current = ((lf_queue_t)atomic_read(os)).head_;

  while (current)
    {
      tmp = ((lf_queue_node_t)atomic_read(current)).next;//current->next;
      old_node = cas_ret(&os->head_, current, tmp);

      if (old_node == current)
	{
	  void* result =
	    ((lf_queue_node_t)atomic_read(current)).data;//current->data;
	  return result;
	}
      current = ((lf_queue_t)atomic_read(os)).head_;//os->head_;
    }
  return 0;
}
