#include <atomics.h>
#include <unionfind.h>
#include <hashtable.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


uf_t* uf_alloc(const datatype_t *key_type, size_t size, size_t tn)
{
  uf_t* uf_ = (uf_t*) malloc(sizeof(uf_t));
  uf_->table = ht_alloc (key_type, size);
  assert(uf_->table);
  uf_->dead_ = (uf_node_t*) malloc (sizeof(uf_node_t));
  uf_->dead_->parent = uf_->dead_;
  assert(uf_->dead_ == uf_->dead_->parent);
  uf_->prealloc_ = (uf_node_t**) malloc (sizeof(uf_node_t)*tn);

  int i;
  for (i = 0; i < tn; ++i)
    {
      uf_->prealloc_ [i] = (uf_node_t*)  malloc (sizeof(uf_node_t));
      uf_->prealloc_ [i]->parent = uf_->prealloc_ [i];
      uf_->prealloc_ [i]->markset = 0;
    }
  return uf_;
}

void fun (void* elt)
{
  if (!elt)
    return;
  uf_node_t * node = (uf_node_t*) elt;
  //  printf("delete [%zu]\n", (size_t) node);
  assert(node);
  free (node);
}

void uf_free(uf_t* uf, int verbose, size_t tn)
{
  if (verbose)
    ht_print(uf->table, false);

  // Destroy all table
  free(uf->dead_);
  ht_iter_value(uf->table, (process_fun_t) fun); // Thread Safe!

  int i;
  for (i = 0; i < tn; ++i)
    free(uf->prealloc_[i]);
  free(uf->prealloc_);
  ht_free(uf->table);
  free(uf);
}

// TODO : Here allocate a new value each time but if there
//        if conflicts must be release. Many allocation
//        can be avoid.
uf_node_t* uf_make_set(uf_t* uf, map_key_t key, int *inserted, int tn)
{
  map_val_t old = 0;
  map_key_t clone = 0;
  uf_node_t *alloc = uf->prealloc_[tn];


  // Hashmap.insert(map,   key, val, res, context)
  //      Ne pas inserer de clefs negatives
  //      Ne pas inserer de valeur negatives
  old = ht_cas_empty (uf->table, key, (map_val_t)alloc, &clone, (void*)NULL);

  if (old == DOES_NOT_EXIST)
    {
      *inserted = 1;
      uf->prealloc_[tn] = (uf_node_t*) malloc (sizeof(uf_node_t));
      uf->prealloc_[tn]->parent = uf->prealloc_[tn];
      uf->prealloc_[tn]->markset = 0;
      return alloc;
    }

  *inserted = 0;
  return (uf_node_t*)old;
}

uf_node_t* uf_find(uf_node_t* n)
{
  // Look for the root
   uf_node_t* p = ((uf_node_t)atomic_read(n)).parent;
   if (p == n)
     return p;

   uf_node_t* root = uf_find(p);
   uf_node_t* old = cas_ret(&n->parent, p, root);

   return p == old ? root : uf_find(old);
}

unsigned long uf_add_acc(uf_t* uf, uf_node_t* n, unsigned long acc)
{
  uf_node_t* parent = uf_find(n);
  unsigned long cumulated_acc = acc;

  while (1)
    {
      if (parent == uf->dead_)
	return 0;

      unsigned long tmp = ((uf_node_t)atomic_read(parent)).markset;
      cumulated_acc |= tmp;

      unsigned long old;
      old = cas_ret(&parent->markset, tmp, cumulated_acc);
      if (old == tmp)
	{
	  // success !
	  if (((uf_node_t)atomic_read(parent)).parent == parent)
	    {
	      //printf("%p %zu %zu %zu\n", parent, cumulated_acc, acc, tmp);
	      return cumulated_acc;
	    }
	}
      cumulated_acc |= old;
      parent = uf_find(parent);
    }

}

uf_node_t* uf_unite(uf_t* uf, uf_node_t* left, uf_node_t* right,
		    unsigned long* acc)
{
  uf_node_t* pleft = left;
  uf_node_t* pright = right;
  uf_node_t* old;

  if (left == right)
    return left;

  if (left == uf->dead_)
    {
      uf_make_dead(uf, right);
      return uf->dead_;
    }

  if (right == uf->dead_)
    {
      uf_make_dead(uf, left);
      return uf->dead_;
    }

  while (1)
    {
      pleft = uf_find(pleft);
      pright = uf_find(pright);

      if (pleft == pright)
	{
	  //*acc = ((uf_node_t)atomic_read(pleft)).markset;
	  return pleft;
	}

      if (pleft == uf->dead_)
	{
	  uf_make_dead(uf, pright);
	  return uf->dead_;
	}

      if (pright == uf->dead_)
	{
	  uf_make_dead(uf, pleft);
	  return uf->dead_;
	}

      /*
       * Always link to smaller since memory allocation
       * usually follows a growing style this optimize
       * chances to link to the oldest element
       */
      if (pleft < pright)
	{
	  old = cas_ret(&pright->parent, pright, pleft);
	  if (old == pright)
	    {
	      //*acc = ((uf_node_t)atomic_read(pleft)).markset;
	      return pleft;
	    }
	  //pright = old;
	}
      else
	{
	  old = cas_ret(&pleft->parent, pleft, pright);
	  if (old == pleft)
	    {
	      //*acc = ((uf_node_t)atomic_read(pright)).markset;
	      return pright;
	    }
	  //pleft = old;
	}
    }
}

void uf_make_dead(uf_t* uf, uf_node_t* n)
{
  uf_node_t* node = n;
  uf_node_t* old;

  while (1)
    {
      node = uf_find(node);
      if (node->parent == uf->dead_)
	return;

      old = cas_ret(&node->parent, node, uf->dead_);
      if (old == node)
	  return ;
      //node = old;
    }
}

int uf_is_dead(uf_t* uf, uf_node_t* n)
{
  uf_node_t* node = uf_find(n);
  if (node->parent == uf->dead_)
    return 1;
  return 0;
}

// Warning: This method only look one parent
// above to decide wether a state is DEAD, so
// it can return outdated information.
int uf_maybe_dead(uf_t* uf, uf_node_t* n)
{
  uf_node_t* p = ((uf_node_t)atomic_read(n)).parent;
  if (p == uf->dead_)
    return 1;
  return 0;
}


void print_uf_node_t (void *node)
{
  if (!node)
    return;
  assert(node);
  uf_node_t* pnode = (uf_node_t*) node;
  assert(pnode && pnode->parent);
  if (pnode == pnode->parent)
    printf ("self [%zu]\n", (size_t)pnode->parent);
  else
    printf("     [%zu] ---> [%zu]\n", (size_t)pnode, (size_t)pnode->parent);
}
