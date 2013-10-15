// Copyright (C) 2012 Laboratoire de Recherche et Développement
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
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include "fasttgbaalgos/ec/concur/unionfind.h"
// #include <hre/config.h>
// #include <assert.h>
// #include <hre/runtime.h>
// #include <mc-lib/hashtable.h>
// #include <mc-lib/unionfind.h>
// #include <mc-lib/atomics.h>
// #include <string.h>

#ifdef __cplusplus
} // extern "C"
#endif


// ------------------------------------------------------------
//  Wrapper for union find
// ============================================================

static int
fasttgba_state_ptrint_cmp_wrapper ( void *hash1, void *hash2)
{
  const spot::fasttgba_state* l = (const spot::fasttgba_state*)hash1;
  const spot::fasttgba_state* r = (const spot::fasttgba_state*)hash2;
  return l->compare(r);
  // int* a = (int*)&hash1;
  // int* b = (int*)&hash2;
  // return *a - *b;
}

uint32_t
fasttgba_state_ptrint_hash_wrapper ( void *elt, void *ctx)
{
  const spot::fasttgba_state* l = (const spot::fasttgba_state*)elt;
  int hash = l->hash();
  assert(hash);
  return hash;
  (void)ctx;
  // int* a = (int*)&elt;
  // return *a;
  // (void)ctx;
}

void *
fasttgba_state_ptrint_clone_wrapper (void *key, void *ctx)
{
  assert(key);
  return key;
  (void)ctx;
}

void
fasttgba_state_ptrint_free_wrapper (void *elt)
{
  assert(elt);
  const spot::fasttgba_state* l = (const spot::fasttgba_state*)elt;
  l->destroy();
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
    uf()
    {
      effective_uf = uf_alloc(&DATATYPE_INT_PTRINT, INIT_SCALE);
    }

    virtual ~uf()
    {
      uf_free (effective_uf, true);
    }

    /// \brief insert a new element in the union find
    bool make_set(const fasttgba_state* key)
    {
      // The Concurent Hash Map doesn't support key == 0 because
      // it represent a special tag.
      assert(key);

      int inserted =0;
      // uf_node_t* node;
      // node =
      uf_make_set(effective_uf, (map_key_t) key, &inserted);
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

    /// \brief check wether an element is linked to dead
    bool is_dead(const fasttgba_state* key)
    {
      uf_node_t* node = (uf_node_t*)
	ht_get(effective_uf->table, (map_key_t) key);
      return  uf_is_dead(effective_uf, node);;
    }

  private:
    uf_t* effective_uf;		///< \brief the union find
  };
}
#endif // SPOT_FASTTGBAALGOS_EC_CONCUR_UF_HH
