// Copyright (C) 2012 Laboratoire de Recherche et Developpement
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

#ifndef SPOT_IFACE_DVE2_DVE2STATE_HH
# define SPOT_IFACE_DVE2_DVE2STATE_HH

# include "misc/fixpool.hh"
# include "misc/mspool.hh"

#include "tgba/state.hh"

namespace spot
{
  struct dve2_state: public state
  {
    dve2_state(int s, fixed_size_pool* p);

    void compute_hash();

    dve2_state* clone() const;
    void destroy() const;

    size_t hash() const;
    int compare(const state* other) const;

  private:

    ~dve2_state()
      {
      }

  public:
    fixed_size_pool* pool;
    size_t hash_value: 32;
    int size: 16;
    mutable unsigned count: 16;
    int vars[0];
  };

  ////////////////////////////////////////////////////////////////////////
  // COMPRESSED STATE

  class dve2_compressed_state: public state
  {
  public:
    dve2_compressed_state(int s, multiple_size_pool* p);

    void compute_hash();

    dve2_compressed_state* clone() const;
    void destroy() const;

    size_t hash() const;
    int compare(const state* other) const;

  private:

    ~dve2_compressed_state()
      {
      }

  public:
    multiple_size_pool* pool;
    size_t hash_value: 32;
    int size: 16;
    mutable unsigned count: 16;
    int vars[0];
  };
}

#endif // SPOT_IFACE_DVE2_DVE2STATE_HH
