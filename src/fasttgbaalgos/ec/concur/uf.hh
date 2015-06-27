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

#ifdef __cplusplus
extern "C" {
#endif

#include "fasttgbaalgos/ec/concur/unionfind.h"

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
  // FIXME?
  assert(key);
  return key;
  (void)ctx;
}

void
fasttgba_state_ptrint_free_wrapper(void *elt)
{
  assert(elt);
  //const spot::fasttgba_state* l = (const spot::fasttgba_state*)elt;
  //l->destroy();
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
namespace spot
{
  class uf
  {
  public :
    uf(int thread_number) : thread_number_(thread_number), size_(0)
    {
      effective_uf = uf_alloc(&DATATYPE_INT_PTRINT, INIT_SCALE, thread_number);
    }

    virtual ~uf()
    {
      uf_free (effective_uf, size_, thread_number_);
    }

    /// \brief insert a new element in the union find
    bool make_set(const fasttgba_state* key, int tn)
    {
      // The Concurent Hash Map doesn't support key == 0 because
      // it represent a special tag.
      assert(key);

      int inserted = 0;
      // uf_node_t* node;
      // node =
      uf_make_set(effective_uf, (map_key_t) key, &inserted, tn);
      if (inserted)
	++size_;
      return inserted;
    }

    /// \brief Mark an elemnent as dead
    void make_dead(const fasttgba_state* key)
    {
      // The Concurent Hash Map doesn't support key == 0 because
      // it represent a special tag.
      assert(key);

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
    markset unite(const fasttgba_state* left, const fasttgba_state* right,
		  const markset acc, bool *is_dead)
    {
      uf_node_t* l = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) left);
      uf_node_t* r = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) right);
      unsigned long ret_acc = 0;

      uf_node_t* fast_ret = uf_unite (effective_uf, l, r, &ret_acc);
      *is_dead = fast_ret == effective_uf->dead_;

      unsigned long tmp = acc.to_ulong();
      // printf("%s %zu %zu\n", acc.dump().c_str(), tmp, ret_acc);
      // The markset is empty don't need to go further

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
    bool is_dead(const fasttgba_state* key)
    {
      uf_node_t* node = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) key);

      if (node == DOES_NOT_EXIST)
	return false;
      return uf_is_dead(effective_uf, node);
    }

    /// \brief check wether an element is possibly linked
    /// to dead
    bool is_maybe_dead(const fasttgba_state* key)
    {
      uf_node_t* node = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) key);

      if (node == DOES_NOT_EXIST)
	return false;
      return uf_maybe_dead(effective_uf, node);
    }


    markset make_and_unite(const fasttgba_state* left,
			   const fasttgba_state* right,
			   const markset acc, bool *is_dead,
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

      unsigned long tmp = acc.to_ulong();
      // printf("%s %zu %zu\n", acc.dump().c_str(), tmp, ret_acc);
      // The markset is empty don't need to go further

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

    /// \brief Mark an elemnent as dead
    void make_and_makedead(const fasttgba_state* key, int tn)
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


    int size()
    {
      return size_;
    }

  private:
    uf_t* effective_uf;		///< \brief the union find
    int thread_number_;
    std::atomic<int> size_;
  };
}
#endif // SPOT_FASTTGBAALGOS_EC_CONCUR_UF_HH
