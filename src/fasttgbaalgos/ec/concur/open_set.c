#include <atomics.h>
#include <open_set.h>
#include <hashtable.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


open_set_t* open_set_alloc(const datatype_t *key_type, size_t size, size_t tn)
{
  /* Instanciate the open set
   */
  open_set_t* os_ = (open_set_t*) malloc(sizeof(open_set_t));

  /* Instanciate the table
   */
  os_->table_ = ht_alloc (key_type, size);
  assert(os_->table_);

  os_->to_free_ = (open_set_node_t**) malloc (sizeof(open_set_node_t)*tn);
  int i;
  for (i = 0; i < tn; ++i)
    os_->to_free_[i] = 0;

  /* Initialize the open set (aka linked list)
   */
  os_->head_ = 0;
  return os_;
}

void open_set_free(open_set_t* os, int verbose, size_t tn)
{
  /* Now Destroy the open_set . This is thread safe
   * The open set can be not empty if a counterexample has been found
   */
  open_set_node_t *curr = os->head_;
  if (!curr)
    goto end;

  while (curr->next != 0)
    {
      open_set_node_t *tmp = curr->next;

      // TODO
      // The data must be delete by thread or pass a delete function?
      free(curr);
      curr = tmp;
    }
  free(curr);

 end:

  /* Destroy the table
   */
  if (verbose)
    {
      ht_print(os->table_, true);
    }

  ht_free(os->table_);
  free(os);
}


int open_set_find_or_put(open_set_t* os, map_key_t key)
{
  map_val_t old = 0;
  map_key_t clone = 0;
  int result = 0;

  // Hashmap.insert(map,   key, val, res, context)
  //      Ne pas inserer de clefs negatives
  //      Ne pas inserer de valeur negatives
  old = ht_cas_empty (os->table_, key, (map_val_t)1, &clone, (void*)NULL);

  if (old == DOES_NOT_EXIST)
    {
      result = 1;

      // Now the value must be insert in the open set
      open_set_node_t* old_node;
      open_set_node_t* tmp;
      open_set_node_t* node =
	(open_set_node_t*) malloc (sizeof(open_set_node_t));
      node->data = (void*)key;

      do
	{
	  tmp = ((open_set_t)atomic_read(os)).head_;//os->head_;
	  node->next = tmp;
	  old_node = cas_ret(&os->head_, tmp, node);
	}
      while (old_node != tmp);
    }

  return result;
}

void* open_set_get_one(open_set_t* os, int tn)
{
  open_set_node_t* old_node;
  open_set_node_t* tmp;
  open_set_node_t* current = ((open_set_t)atomic_read(os)).head_;

  while (current)
    {
      tmp = ((open_set_node_t)atomic_read(current)).next;//current->next;
      old_node = cas_ret(&os->head_, current, tmp);

      if (old_node == current)
	{
	  void* result =
	    ((open_set_node_t)atomic_read(current)).data;//current->data;
	  if (os->to_free_[tn])
	    {
	      free (os->to_free_[tn]);
	    }
	  os->to_free_[tn] = current;
	  //free (current);
	  return result;
	}
      current = ((open_set_t)atomic_read(os)).head_;//os->head_;
    }

  return 0;
}
