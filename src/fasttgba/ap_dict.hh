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

#ifndef SPOT_FASTTGBA_AP_DICT_HH
# define SPOT_FASTTGBA_AP_DICT_HH

#include "ltlast/atomic_prop.hh"

namespace spot
{
  class ap_dict
  {
  public:
    /// \brief a default constructor that construct an empty dictionnary
    ap_dict();

    /// \brief a simple destructor
    virtual ~ap_dict();

    /// \brief add an atomic proposition into this dictionnary
    ///
    /// All returned values start from 0 to size ()
    ///
    /// Return the unique identifier associated to this \a ap
    virtual int add_ap(const ltl::atomic_prop* ap);

    /// \brief
    ///
    ///
    virtual const ltl::atomic_prop* get(int i);

    size_t size();

  protected:
    int id_;
    std::map<const ltl::atomic_prop*, int> aps_;      ///< formula to int converter
    std::map<int, const ltl::atomic_prop*> apsback_; ///< int to formula converter
  };
}
#endif // SPOT_FASTTGBA_AP_DICT_HH
