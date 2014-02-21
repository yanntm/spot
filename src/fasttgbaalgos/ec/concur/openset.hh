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

#ifndef SPOT_FASTTGBAALGOS_EC_CONCUR_OPENSET_HH
# define SPOT_FASTTGBAALGOS_EC_CONCUR_OPENSET_HH

#include "fasttgba/fasttgba.hh"
#include <iosfwd>
#include <atomic>

// For comparison def!
#include "uf.hh"


#ifdef __cplusplus
extern "C" {
#endif

#include "fasttgbaalgos/ec/concur/open_set.h"

#ifdef __cplusplus
} // extern "C"
#endif


/// \brief this class acts like a wrapper to the
/// C code of the open_set.
namespace spot
{
  class openset
  {
  public :

    /// Constructor
    openset(int thread_number) : thread_number_(thread_number), size_(0)
    {
      effective_os = open_set_alloc(&DATATYPE_INT_PTRINT,
				    INIT_SCALE, thread_number);
    }

    /// Basic destructor
    virtual ~openset()
    {
      open_set_free (effective_os, size_, thread_number_);
    }


    bool find_or_put(const fasttgba_state* key)
    {
      bool b = open_set_find_or_put(effective_os, (map_key_t) key);
      if (b)
	{
	  ++size_;
	}
      return b;
    }

    /// \brief Get the next state to deal with
    /// Return a new state or 0
    const fasttgba_state*  get_one()
    {
      return (const fasttgba_state*)open_set_get_one(effective_os);
    }

    /// \brief the current size of the open_set
    int size()
    {
      return size_;
    }

  private:
    open_set_t* effective_os;		///< The open set
    int thread_number_;			///< The number of threads
    std::atomic<int> size_;		///< The size of the open set
  };
}
#endif // SPOT_FASTTGBAALGOS_EC_CONCUR_OPEN_HH
