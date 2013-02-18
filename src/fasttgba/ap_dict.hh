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
  class fasttgba;

  /// \brief This class is used to create a dictionary that will
  /// contain all atomic propositions that are needed by an (and
  /// possibly many) automaton
  ///
  /// All atomatic propsitions are register in this class using the
  /// class ltl::atomic_prop that is used during the translation
  /// algorithm.
  ///
  /// Each atomic proposition is associated to a unique identifier
  ///
  /// If this Dicitonnary is used by two automata using the atomic
  /// proposition 'a' then the id of 'a' will be the same.
  ///
  class ap_dict
  {
  public:
    /// \brief a default constructor that construct an empty dictionary
    ap_dict();

    /// \brief a simple destructor
    virtual ~ap_dict();

    /// \brief add an atomic proposition into this dictionary
    ///
    /// All returned values start from 0 to size ()
    ///
    /// Return the unique identifier associated to this \a ap
    virtual int register_ap_for_aut(const ltl::atomic_prop* ap,
				    const spot::fasttgba* a);

    /// \brief This provide the reference to the \a i th
    /// variable that is contained in this dictionary
    virtual const ltl::atomic_prop* get(int i);

    /// \brief Return the size of this dictionary
    size_t size();

    /// \brief Return true if Dicitionnary is empty
    bool empty ();

  protected:
    int id_;			///< Unique id counter

    /// Formula to int converter
    std::map<const ltl::atomic_prop*, int> aps_;

    /// Int to formula converter
    std::map<int, const ltl::atomic_prop*> apsback_;
  };
}
#endif // SPOT_FASTTGBA_AP_DICT_HH
