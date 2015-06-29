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
  lf_queue_node_t* sentinel =
    (lf_queue_node_t*) malloc (sizeof(lf_queue_node_t));
  sentinel->data_ = 0;
  sentinel->next_ = 0;
  os_->head_ = sentinel;
  os_->tail_ = sentinel;

  os_->prealloc_idx_ = 0;
  os_->prealloc_size_ = 1000;
  os_->prealloc_ = (lf_queue_node_t*)
    malloc (sizeof(lf_queue_node_t)*(os_->prealloc_size_+1));
  os_->prealloc_first_ =   os_->prealloc_;
  os_->prealloc_[os_->prealloc_size_].data_ = 0;
  os_->prealloc_[os_->prealloc_size_].next_ = 0;
  return os_;
}

void lf_queue_free(lf_queue_t* os)
{
  assert(os->head_->next_ == 0);
  lf_queue_node_t* tmp =   os->prealloc_first_;
  while (tmp)
    {
      lf_queue_node_t* old = tmp;
      tmp = tmp[os->prealloc_size_].next_;
      free(old);
    }
  free(os);
}


int lf_queue_put(lf_queue_t* os, void* data)
{
  lf_queue_node_t* tail;
  lf_queue_node_t* next;
  lf_queue_node_t* old_node;
  /* lf_queue_node_t* node = */
  /*   (lf_queue_node_t*) malloc (sizeof(lf_queue_node_t)); */

  // Use preallocation
  lf_queue_node_t* node;
  if(os->prealloc_idx_ < os->prealloc_size_)
    {
      node = &os->prealloc_[os->prealloc_idx_];
      ++os->prealloc_idx_;
    }
  else
    {
      // End of the prealloc, use the last room to
      // store pointer to a new prealloc range.
      node =  (lf_queue_node_t*)
	malloc (sizeof(lf_queue_node_t)*(os->prealloc_size_+1));
      os->prealloc_[os->prealloc_size_].next_ = node;
      os->prealloc_idx_ = 1; // since node refer prealloc_[0]
      os->prealloc_ = node;
      os->prealloc_[os->prealloc_size_].data_ = 0;
      os->prealloc_[os->prealloc_size_].next_ = 0;
    }

  node->data_ = (void*)data;
  node->next_ = 0;

  do
    {
      tail = ((lf_queue_t)atomic_read(os)).tail_;
      next = ((lf_queue_node_t)atomic_read(tail)).next_;

      // Was next pointing to the last node?
      if (!next)
	{
	  old_node = cas_ret(&tail->next_, 0, node);
	  // Insertion success
	  if (old_node == 0)
	    break;
	}
      else
	{
	  // Tail was not pointing to the last node
	  // Try to swing to the next node
	  old_node = cas_ret(&os->tail_, tail, next);
	}
    } while (1);

  // Enqueue done, try to swing tail to the inserted node.
  (void)cas_ret(&os->tail_, tail, node);

  return 0;
}

void* lf_queue_get_one(lf_queue_t* os)
{
  lf_queue_node_t* head;
  lf_queue_node_t* tail;
  lf_queue_node_t* next;
  lf_queue_node_t* old_node;
  void *result;

  do
    {
      head = ((lf_queue_t)atomic_read(os)).head_;
      tail = ((lf_queue_t)atomic_read(os)).tail_;
      next = ((lf_queue_node_t)atomic_read(head)).next_;

      if (head == tail)
	{
	  // Empty queue, could not dequeue
	  if (!next)
	    return 0;

	  // Tail is falling behind, try to advance it.
	  (void)cas_ret(&os->tail_, tail, next);
	}
      else
	{
	  // FIXME: Should this be atomic for multi-consumer?
	  result = next->data_;

	  old_node = cas_ret(&os->head_, head, next);
	  if (old_node == head)
	    break;
	}
    } while (1);

  // Done in free since we use preallocation
  //free(head);
  return result;
}
