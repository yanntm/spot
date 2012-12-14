// Copyright (C) 2012 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
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


#ifndef SPOT_FASTTGBA_MARK_HH
# define SPOT_FASTTGBA_MARk_HH

#include <boost/dynamic_bitset.hpp>

namespace spot
{
  /// \brief This class is used to represent acceptance mark
  class mark
  {
  public:
    /// \brief wrapper of type
    typedef boost::dynamic_bitset<> storage;

    // \brief wrapper for a type
    typedef boost::dynamic_bitset<>::reference storage_elt;

    /// \brief Initialize a mark of size \a size
    mark(int size);

    /// \brief A copy constructor
    mark(const mark& b);

    /// \brief perform a logic AND with \a b
    ///
    /// Affect the result into this
    mark& operator&=(const mark& b);

    /// \brief perform a logic OR with \a b
    ///
    /// Affect the result into this
    mark& operator|=(const mark& b);

    /// \brief perform a logic XOR with \a b
    ///
    /// Affect the result into this
    mark& operator^=(const mark& b);

    /// \brief Compute the difference with \a b
    ///
    /// Affect the result into this
    mark& operator-=(const mark& b);

    /// \brief Perform a shift on the left of size \a b
    ///
    /// Affect the result into this
    mark& operator<<=(int n);

    /// \brief Perform a shift on the right of size \a b
    ///
    /// Affect the result into this
    mark& operator>>=(int n);

    /// \brief Affect the value of  \a b in this
    mark& operator=(const mark& b);

    storage_elt operator[](int pos);

  protected:
    storage mark_;   ///< the set of acceptance
  };
}

#endif  // SPOT_FASTTGBA_MARK_HH
