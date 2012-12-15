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

#ifndef SPOT_FASTTGBA_FASTSTATE_HH
# define SPOT_FASTTGBA_FASTSTATE_HH

#include <cstddef>
#include <cassert>
#include <functional>
#include <boost/shared_ptr.hpp>
#include <string>
#include "misc/casts.hh"
#include <boost/dynamic_bitset.hpp>

namespace spot
{

  /// \brief This class act as an interface for all classes
  class faststate
  {
  public:
    /// \brief Compares two states (that come from the same automaton).
    ///
    /// This method returns an integer less than, equal to, or greater
    /// than zero if \a this is found, respectively, to be less than, equal
    /// to, or greater than \a other according to some implicit total order.
    ///
    /// This method should not be called to compare states from
    /// different automata.
    virtual int compare(const faststate* other) const = 0;

    /// \brief Hash a state.
    ///
    /// This method returns an integer that can be used as a
    /// hash value for this state.
    virtual size_t hash() const = 0;

    /// Duplicate a state.
    virtual faststate* clone() const = 0;

    /// Allow to add more information inside of a state.
    /// It can be used to store the strength of the current
    /// SCC or the strength of the strength of the subautomaton
    virtual void* external_information() const = 0;

    /// \brief Release a state.
    ///
    /// Sub class can refined this method to be memory efficient
    virtual void destroy() const
    {
      delete this;
    }

  protected:
    /// \brief Destructor.
    ///
    /// \deprecated Client code should now call
    /// <code>s->destroy();</code> instead of <code>delete s;</code>.
    virtual ~faststate()
    {
    }
  };


  struct faststate_ptr_equal:
    public std::binary_function<const faststate*, const faststate*, bool>
  {
    bool
    operator()(const faststate* left, const faststate* right) const
    {
      assert(left);
      return 0 == left->compare(right);
    }
  };

  struct faststate_ptr_hash:
    public std::unary_function<const faststate*, size_t>
  {
    size_t
    operator()(const faststate* that) const
    {
      assert(that);
      return that->hash();
    }
  };

}

#endif // SPOT_FASTTGBA_FASTSTATE_HH
