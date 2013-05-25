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

#ifndef SPOT_FASTTGBAALGOS_EC_DEADSTORE_HH
# define SPOT_FASTTGBAALGOS_EC_DEADSTORE_HH


#include "misc/hash.hh"
#include "fasttgba/fasttgba.hh"
#include "boost/tuple/tuple.hpp"


#include <cassert>
#include <iostream>
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

namespace spot
{

  /// \brief This class represent a dead store.
  /// For now it's just a set but it can be combined
  /// with bitstate hasing
  class deadstore
  {
  public:
    /// \brief A simple constructor that instanciate a store
    deadstore(): store()
    { }

    virtual ~deadstore()
    {
      std::unordered_set
      	<const fasttgba_state*,
      	 fasttgba_state_ptr_hash,
      	 fasttgba_state_ptr_equal>::iterator it = store.begin();
      while (it != store.end())
      	{
	  const fasttgba_state* ptr = *it;
	  ++it;
	  ptr->destroy();
      }
    }

    /// \brief check wheter an element is in the
    /// store
    bool contains (const fasttgba_state*  state)
    {
      return store.find (state) != store.end();
    }

    /// \brief Add a new element in the store
    void add (const fasttgba_state* state)
    {
      assert(!contains(state));
      store.insert(state);
    }

  private :
    /// The map of Dead states
    std::unordered_set<const fasttgba_state*,
		  fasttgba_state_ptr_hash,
		  fasttgba_state_ptr_equal> store;
  };
}
#endif // SPOT_FASTTGBAALGOS_EC_DEASSTORE_HH
