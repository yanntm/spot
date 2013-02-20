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

#ifndef SPOT_FASTTGBAALGOS_EC_UNION_FIND_HH
# define SPOT_FASTTGBAALGOS_EC_UNION_FIND_HH

#include "fasttgba/fasttgba.hh"

namespace spot
{
  /// This class is the basis class to be manipulated
  /// by the union find structure
  class uf_element
  {
  public:
    /// The constructor for a such element
    uf_element(const fasttgba_state* s, markset m):
      parent_(s), acc_(m)
    {
    }

    /// \brief A Reference to the root of this SCC
    const fasttgba_state* parent ()
    {
      return parent_;
    }

    /// \brief set the parent of this element
    void set_parent (const fasttgba_state* p)
    {
      parent_ = p;
    }

    /// The acceptance set of this SCC
    /// Note that only root that are their own
    /// parent collect all the acceptance set
    markset acceptance()
    {
      return acc_;
    }

    void add_acceptance(markset m)
    {
      acc_ |= m;
    }

  protected:
    const fasttgba_state* parent_; ///< The parent
    markset acc_;		   ///< The markset
  };


  /// This class is a wrapper for manipulating Union
  /// Find structure in emptiness check algotithms
  ///
  /// It's an efficient data structure to compute SCC
  /// In order to be efficient, some methods are added
  /// to impove the efficience of emptiness check algo
  class union_find
  {
  public:

    /// The constructor for the union find structure
    union_find (acc_dict&);

    /// A simple destructor
    virtual ~union_find ();

    /// \brief Return the parent of \a s
    const fasttgba_state* find (const fasttgba_state* s);

    /// \brief Return the acceptance set associated
    markset findacc (const fasttgba_state*);

    /// \brief Perform the union of the two SCC represented
    /// by this two elements
    void make_union (const fasttgba_state*,
		     const fasttgba_state*);

    /// \brief Create a new set if it does not yet exist
    void make_set (const fasttgba_state*, markset m);

    /// \brief The SCC represented by this element is now
    /// labelled as dead
    void make_dead_set (const fasttgba_state*);

    /// \brief Create a Dead Fake element
    void insert_dead ();

  protected:
    /// \brief the structure used to the storage
    std::map<const fasttgba_state*, uf_element*> UF;

    /// \brief The acceptance dictionary
    acc_dict& acc_;
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_UNION_FIND_HH
