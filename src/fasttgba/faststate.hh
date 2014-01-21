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
#include "misc/casts.hh"
#include <boost/dynamic_bitset.hpp>

namespace spot
{

  /// \brief Abstract class for states.
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
    ///
    /// \sa spot::state_ptr_less_than
    virtual int compare(const state* other) const = 0;

    /// \brief Hash a state.
    ///
    /// This method returns an integer that can be used as a
    /// hash value for this state.
    ///
    /// Note that the hash value is guaranteed to be unique for all
    /// equal states (in compare()'s sense) for only has long has one
    /// of these states exists.  So it's OK to use a spot::state as a
    /// key in a \c hash_map because the mere use of the state as a
    /// key in the hash will ensure the state continues to exist.
    ///
    /// However if you create the state, get its hash key, delete the
    /// state, recreate the same state, and get its hash key, you may
    /// obtain two different hash keys if the same state were not
    /// already used elsewhere.  In practice this weird situation can
    /// occur only when the state is BDD-encoded, because BDD numbers
    /// (used to build the hash value) can be reused for other
    /// formulas.  That probably doesn't matter, since the hash value
    /// is meant to be used in a \c hash_map, but it had to be noted.
    virtual size_t hash() const = 0;

    /// Duplicate a state.
    virtual state* clone() const = 0;

    /// \brief Release a state.
    ///
    /// Methods from the tgba or tgba_succ_iterator always return a
    /// new state that you should deallocate with this function.
    /// Before Spot 0.7, you had to "delete" your state directly.
    /// Starting with Spot 0.7, you update your code to this function
    /// instead (which simply calls "delete").  In a future version,
    /// some subclasses will redefine destroy() to allow better memory
    /// management (e.g. no memory allocation for explicit automata).
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

}

#endif // SPOT_FASTTGBA_FASTSTATE_HH
