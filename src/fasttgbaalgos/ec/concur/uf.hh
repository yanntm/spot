// Copyright (C) 2013 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef SPOT_FASTTGBAALGOS_EC_CONCUR_UF_HH
# define SPOT_FASTTGBAALGOS_EC_CONCUR_UF_HH

#include "fasttgba/fasttgba.hh"
#include <iosfwd>
#include <atomic>
#include <mutex>

#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include "fasttgbaalgos/ec/concur/unionfind.h"
#include "fasttgbaalgos/ec/concur/lf_queue.h"
#include "fasttgbaalgos/ec/concur/hashtable.h"

#ifdef __cplusplus
} // extern "C"
#endif


// ------------------------------------------------------------
//  Wrapper for union find
// ============================================================

static int
fasttgba_state_ptrint_cmp_wrapper(void *hash1, void *hash2)
{
  const spot::fasttgba_state* l = (const spot::fasttgba_state*)hash1;
  const spot::fasttgba_state* r = (const spot::fasttgba_state*)hash2;
  assert(l && r);
  return l->compare(r);
}

uint32_t
fasttgba_state_ptrint_hash_wrapper(void *elt, void *ctx)
{
  const spot::fasttgba_state* l = (const spot::fasttgba_state*)elt;
  int hash = l->hash();
  assert(hash);
  return hash;
  (void)ctx;
}

void *
fasttgba_state_ptrint_clone_wrapper(void *key, void *ctx)
{
  assert(key);
  return key;
  (void)ctx;
}

void
fasttgba_state_ptrint_free_wrapper(void *elt)
{
  assert(elt);
  (void) elt;
}

const size_t INIT_SCALE = 12;
static const datatype_t DATATYPE_INT_PTRINT =
  {
    fasttgba_state_ptrint_cmp_wrapper,
    fasttgba_state_ptrint_hash_wrapper,
    fasttgba_state_ptrint_clone_wrapper,
    fasttgba_state_ptrint_free_wrapper
  };


namespace spot
{


  /// This class  export the interface of the union-find
  /// data structure. This interface has been done after
  /// first experiments and really need to be refreshed!
  class uf
  {
  public :
    virtual ~uf()
    { } ;
    virtual bool make_set(const fasttgba_state*, int) = 0;
    virtual void make_dead(const fasttgba_state*, int) = 0;
    virtual bool is_dead(const fasttgba_state*) = 0;
    virtual markset unite(const fasttgba_state*,
		  const fasttgba_state*,
		  const markset,
		  bool *, int) = 0;
    virtual unsigned long make_and_unite(const fasttgba_state*,
				 const fasttgba_state*,
				 unsigned long,
				 bool *,
				 int) = 0;
    virtual bool is_maybe_dead(const fasttgba_state*) = 0;
    virtual int size() = 0;
    virtual void make_and_makedead(const fasttgba_state*, int) = 0;
  };

  /// \brief this class acts like a wrapper to the
  /// C code of the union find.
  ///
  /// The Union find has been built using a C++ version
  /// of the concurent_hash_map that can be downloaded
  /// at :
  ///
  /// Therefore this version includes many adaptation
  /// that comes from the LTSmin tool that can be
  /// download here :
  class uf_lock_free : public uf
  {
  public :
    uf_lock_free(int thread_number) : thread_number_(thread_number), size_(0)
    {
      effective_uf = uf_alloc(&DATATYPE_INT_PTRINT, INIT_SCALE, thread_number);
    }

    virtual ~uf_lock_free()
    {
      uf_free (effective_uf, size_ /*FIXME*/, thread_number_);
    }

    /// \brief insert a new element in the union find
    virtual bool make_set(const fasttgba_state* key, int tn)
    {
      // The Concurent Hash Map doesn't support key == 0 because
      // it represent a special tag.
      assert(key);

      int inserted = 0;
      uf_make_set(effective_uf, (map_key_t) key, &inserted, tn);
      if (inserted)
	++size_;
      return inserted;
    }

    /// \brief Mark an elemnent as dead
    virtual void make_dead(const fasttgba_state* key, int tn)
    {
      // The Concurent Hash Map doesn't support key == 0 because
      // it represent a special tag.
      assert(key);
      (void)tn;
      uf_node_t* node = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) key);
      uf_make_dead(effective_uf, node);
    }

    /// \brief Mark two states in the same set
    ///
    /// Can be seen as a binary operation where left and right
    /// operands are unite and where acceptance sets are merged
    /// \param left the left operand
    /// \param right the right operand
    /// \return  the acceptance set to add to the parent of the two
    /// operands once merged.
    virtual markset unite(const fasttgba_state* left,
			  const fasttgba_state* right,
			  const markset acc, bool *is_dead, int tn)
    {
      (void)tn;
      unsigned long tmp = acc.to_ulong();
      uf_node_t* l = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) left);
      uf_node_t* r = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) right);
      unsigned long ret_acc = 0;

      uf_node_t* fast_ret = uf_unite (effective_uf, l, r, &ret_acc);
      *is_dead = fast_ret == effective_uf->dead_;

      if (!tmp || *is_dead || ret_acc == tmp)
	return acc;

      // Grab the representative int.
      unsigned long res = uf_add_acc(effective_uf, fast_ret, tmp);

      // if res equal 0 the state is dead!
      // The add doesn't change anything
      if (!res || tmp == res)
	return acc;
      return acc | res;
    }

    /// \brief check wether an element is linked to dead
    virtual bool is_dead(const fasttgba_state* key)
    {
      uf_node_t* node = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) key);

      if (node == DOES_NOT_EXIST)
	return false;
      return uf_is_dead(effective_uf, node);
    }

    /// \brief check wether an element is possibly linked
    /// to dead
    virtual bool is_maybe_dead(const fasttgba_state* key)
    {
      uf_node_t* node = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) key);

      if (node == DOES_NOT_EXIST)
	return false;
      return uf_maybe_dead(effective_uf, node);
    }

    virtual unsigned long make_and_unite(const fasttgba_state* left,
			   const fasttgba_state* right,
			   unsigned long tmp, bool *is_dead,
			   int tn)
    {
      int inserted = 0;
      uf_node_t* l =
	uf_make_set(effective_uf, (map_key_t) left, &inserted, tn);
      if (inserted)
	++size_;
      inserted = 0;
      uf_node_t* r =
	uf_make_set(effective_uf, (map_key_t) right, &inserted, tn);
      if (inserted)
	++size_;

      unsigned long ret_acc = 0;
      uf_node_t* fast_ret = uf_unite (effective_uf, l, r, &ret_acc);
      *is_dead = fast_ret == effective_uf->dead_;

      if (!tmp || *is_dead || ret_acc == tmp)
	return tmp;

      // Grab the representative int.
      unsigned long res = uf_add_acc(effective_uf, fast_ret, tmp);
      if (!res)
	return 0;
      return res;
    }

    /// \brief Mark an elemnent as dead
    virtual void make_and_makedead(const fasttgba_state* key, int tn)
    {
      // The Concurent Hash Map doesn't support key == 0 because
      // it represent a special tag.
      int inserted = 0;
      uf_node_t* node =
	uf_make_set(effective_uf, (map_key_t) key, &inserted, tn);
      if (inserted)
	++size_;

      uf_make_dead(effective_uf, node);
    }


    virtual
    int size()
    {
      return size_;
    }

  private:
    uf_t* effective_uf;		///< \brief the union find
    int thread_number_;
    std::atomic<int> size_;
  };


  class uf_fine_grain : public uf
  {
  public :
    struct node
    {
      std::atomic<struct node*> parent;
      std::mutex mutex;
      int rank;
      unsigned long markset;
      std::atomic<bool> is_dead;
    };

    uf_fine_grain
    (int thread_number) : thread_number_(thread_number), size_(0)
    {
      visited_ = ht_alloc(&DATATYPE_INT_PTRINT, INIT_SCALE);
      for (int i = 0 ; i < thread_number_; ++i)
	{
	  prealloc_.push_back(new node());
	  prealloc_.back()->rank = 0;
	  prealloc_.back()->is_dead = false;
	  prealloc_.back()->parent = prealloc_.back();
	}
    }

    static void destroy_fun (void* elt)
    {
      if (!elt)
	return;
      struct node *node = (struct node*) elt;
      assert(node);
      delete node;
    }

    virtual ~uf_fine_grain()
    {
      ht_iter_value(visited_, (process_fun_t) uf_fine_grain::destroy_fun);
      ht_free(visited_);
      for (auto* node: prealloc_)
	delete node;
    }

    virtual bool make_set(const fasttgba_state* key, int tn)
    {
      assert(key);
      map_val_t old = 0;
      map_key_t clone = 0;
      node *alloc = prealloc_[tn];

      old = ht_cas_empty (visited_, (map_key_t)key,
			  (map_val_t)alloc, &clone, (void*)NULL);

      if (old == DOES_NOT_EXIST)
      	{
	  ++size_;
      	  prealloc_[tn] = new node();
	  prealloc_[tn]->rank = 0;
	  prealloc_[tn]->is_dead = false;
	  prealloc_[tn]->parent = prealloc_[tn];
      	  return true;
      	}
      return false;
    }

    node* find(node* n)
    {
      assert(n);
      node* p = n;
      while (p != p->parent)
	{
	  p = p->parent;
	}
      return p;
    }

    virtual void make_dead(const fasttgba_state* key, int tn)
    {
      assert(key);
      (void) tn;
      struct node* node = (struct node*) ht_get(visited_, (map_key_t) key);
      node = find(node);

      while(true)
	{
	  node->is_dead = true;
	  node = find(node);
	  if (node == node->parent)
	    break;
	}
    }

    virtual bool is_dead(const fasttgba_state* key)
    {
      struct node* node = (struct node*) ht_get(visited_, (map_key_t) key);
      if (node == DOES_NOT_EXIST)
	return false;
      node = find(node);
      return node->is_dead;
    }

    virtual markset unite(const fasttgba_state* left,
		  const fasttgba_state* right,
		  const markset acc,
		  bool *is_dead, int tn)
    {
      (void) tn;
      unsigned long tmp = acc.to_ulong();
      struct node* l = (struct node*) ht_get(visited_, (map_key_t) left);
      struct node* r = (struct node*) ht_get(visited_, (map_key_t) right);

      struct node *xl, *xr, *xtmp;

      while(true)
	{
	  xl = find(l);
	  xr = find(r);

	  // The two elements are already in the same partition
	  // Consider a special case since locking the same mutex
	  // twice leads to an undefined behavior.
	  if (xl == xr)
	    {
	      if (tmp == 0)
		return acc;

	      // update only markset;
	      while (true)
		{
		  if (xl->mutex.try_lock())
		    {
		      if (xl->parent == xl)
			{
			  tmp = tmp | xl->markset;
			  xl->markset = tmp;

			  // Compress
			  while (l->parent != xl)
			    {
			      xtmp = l->parent;
			      l->parent = xl;
			      l = xtmp;
			    }
			  xl->mutex.unlock();
			  return acc | tmp;
			}
		      xl->mutex.unlock();
		    }
		  xl = find(l);
		}
	    }

	  if (xl->mutex.try_lock())
	    {
	      if (xr->mutex.try_lock())
	      {
		if (xl->parent == xl && xr->parent == xr)
		  break;
		xr->mutex.unlock();
	      }
	      xl->mutex.unlock();
	    }
	}


      // compress path
      while (l->parent != xl)
	{
	  xtmp = l->parent;
	  l->parent = xl;
	  l = xtmp;
	}
      while (r->parent != xr)
	{
	  xtmp = r->parent;
	  r->parent = xr;
	  r = xtmp;
	}
      *is_dead = xl->is_dead || xr->is_dead;
      tmp = tmp | xl->markset | xr->markset;
      if (xl->rank < xr->rank)
	{
	  xl->parent = r;
	  xr->is_dead = *is_dead;
	  xr->markset = tmp;
	}
      else if (xr->rank < xl->rank)
	{
	  xr->parent = xl;
	  xl->is_dead = *is_dead;
	  xl->markset = tmp;
	}
      else
	{
	  xl->parent = xr;
	  xr->rank = xr->rank + 1;
	  xr->is_dead = *is_dead;
	  xr->markset = tmp;
	}

      xr->mutex.unlock();
      xl->mutex.unlock();
      return acc | tmp;
    }

    virtual unsigned long make_and_unite(const fasttgba_state*,
				 const fasttgba_state*,
				 unsigned long,
				 bool *,
				 int)
    {
      assert(false);
      return 0;
    }

    virtual bool is_maybe_dead(const fasttgba_state*)
    {
      assert(false);
      return false;
    }

    virtual int size()
    {
      return size_;
    }

    virtual void make_and_makedead(const fasttgba_state*, int)
    {
      assert(false);
      return;
    }

  private:
    hashtable_t *visited_;
    int thread_number_;
    std::atomic<int> size_;
    std::vector<node*> prealloc_;
  };










    // inline bool try_insert(enum op_type op,
    // 		    const fasttgba_state* left,
    // 		    const fasttgba_state* right,
    // 		    unsigned long acc,
    // 		    int tn)
    // {
    //   shared_op* node = 0;
    //   if(prealloc_idx_[tn] < prealloc_size_)
    // 	{
    // 	  node = &prealloc_[tn][prealloc_idx_[tn]];
    // 	  ++prealloc_idx_[tn];
    // 	}
    //   else
    // 	{
    // 	  // End of the prealloc, use the last room to
    // 	  // store pointer to a new prealloc range.
    // 	  node = new shared_op[prealloc_size_+1];
    // 	  prealloc_[tn][prealloc_size_].arg1_ = (void*) node;
    // 	  prealloc_idx_[tn] = 1; // since node refer prealloc_[0]
    // 	  prealloc_[tn] = node;
    // 	  prealloc_[tn][prealloc_size_].arg1_ = 0;
    // 	}
    //   node->op_ = op;
    //   node->arg1_ = (void*)left;
    //   node->arg2_ = (void*)right;
    //   node->acc_ = acc;
    //   if (node->op_ == makedead)
    // 	node->hash_ = left->hash() + acc;
    //   else
    // 	node->hash_ = left->hash() + right->hash() + acc;

    //   map_key_t clone = 0;
    //   if (ht_cas_empty (mem_table_, (map_key_t)node,
    // 	  		(map_val_t)1, &clone, (void*)NULL)
    // 	  == DOES_NOT_EXIST)
    // 	return true;

    //   // Insertion failed;
    //   --prealloc_idx_[tn];
    //   return false;
    // }
}
#endif // SPOT_FASTTGBAALGOS_EC_CONCUR_UF_HH
