// -*- coding: utf-8 -*-
// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE)
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include "dve2state.hh"

#include <cstring>

namespace spot
{
  ////////////////////////////////////////////////////////////////////////
  // STATE

  dve2_state::dve2_state(int s, fixed_size_pool* p, int* state, bool exp)
  : pool(p)
    , hash_value(0)
    , expanded(exp)
    , size(s)
    , count(1)
  {
    if (state)
    {
      memcpy(vars, state, s * sizeof(int));
      compute_hash();
    }
  }

  void
  dve2_state::compute_hash()
  {
    hash_value = 0;
    for (int i = 0; i < size; ++i)
      hash_value = wang32_hash(hash_value ^ vars[i]);
  }

  dve2_state*
  dve2_state::clone() const
  {
    ++count;
    return const_cast<dve2_state*>(this);
  }

  void
  dve2_state::destroy() const
  {
    if (--count)
      return;
    pool->deallocate(this);
  }

  size_t
  dve2_state::hash() const
  {
    return hash_value;
  }

  int
  dve2_state::compare(const state* other) const
  {
    if (this == other)
      return 0;
    const dve2_state* o = down_cast<const dve2_state*>(other);
    assert(o);
    if (hash_value < o->hash_value)
      return -1;
    if (hash_value > o->hash_value)
      return 1;
    return memcmp(vars, o->vars, size * sizeof(*vars));
  }

  ////////////////////////////////////////////////////////////////////////
  // COMPRESSED STATE

  dve2_compressed_state::dve2_compressed_state(int s, multiple_size_pool* p,
					       bool exp)
  : pool(p)
    , hash_value(0)
    , expanded(exp)
    , size(s)
    , count(1)
  {
  }

  void
  dve2_compressed_state::compute_hash()
  {
    hash_value = 0;
    for (int i = 0; i < size; ++i)
      hash_value = wang32_hash(hash_value ^ vars[i]);
  }

  dve2_compressed_state*
  dve2_compressed_state::clone() const
  {
    ++count;
    return const_cast<dve2_compressed_state*>(this);
  }

  void
  dve2_compressed_state::destroy() const
  {
    if (--count)
      return;
    pool->deallocate(this, sizeof(*this) + size * sizeof(*vars));
  }

  size_t
  dve2_compressed_state::hash() const
  {
    return hash_value;
  }

  int
  dve2_compressed_state::compare(const state* other) const
  {
    if (this == other)
      return 0;
    const dve2_compressed_state* o =
      down_cast<const dve2_compressed_state*>(other);
    assert(o);
    if (hash_value < o->hash_value)
      return -1;
    if (hash_value > o->hash_value)
      return 1;

    if (size < o->size)
      return -1;
    if (size > o->size)
      return 1;

    return memcmp(vars, o->vars, size * sizeof(*vars));
  }
}
