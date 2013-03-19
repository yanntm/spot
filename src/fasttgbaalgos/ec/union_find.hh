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

#include "misc/hash.hh"
#include "fasttgba/fasttgba.hh"

namespace spot
{
  /// This class is a wrapper for manipulating Union
  /// Find structure in emptiness check algotithms
  ///
  /// It's an efficient data structure to compute SCC
  /// In order to be efficient, some methods are added
  /// to impove the efficience of emptiness check algorithm
  ///
  /// Performing operation with a state == 0 will be analysed
  /// as an operation with a dead state cf cou99_uf
  class union_find
  {
  public:

    /// \brief The constructor for the Union-Find structure
    /// \a acc the acceptance dictionary
    union_find (acc_dict& acc);

    /// \brief A simple destructor
    virtual ~union_find ();

    /// \brief Add a partition that contains only \a s
    /// Suppose a clone() has been done and the union-find
    /// is in charge to perform the destroy()
    void add (const fasttgba_state* s);

    // /// \brief Return true if the Union-Find have a
    // /// partition that contains \a s
    // bool contains (const fasttgba_state* s);

    /// \brief Perform the union of the two partition containing
    /// \a left and \a right. No assumptions over the resulting
    /// parent can be do.
    void unite (const fasttgba_state* left,
    		const fasttgba_state* right);

    /// \brief Return true if the partition of \a left is the
    /// same that the partition of \a right
    bool same_partition (const fasttgba_state* left,
    			 const fasttgba_state* right);

    /// \brief Add the acceptance set to the partition that contains
    /// the state \a s
    void add_acc (const fasttgba_state* s, markset m);

    /// \brief return the acceptance set of the partition containing
    /// \a s
    markset get_acc (const fasttgba_state* s);

    /// \brief Return wether a state belong to the Union-Find structure
    bool contains (const fasttgba_state* s);


    void make_dead (const fasttgba_state* s);

    bool is_dead (const fasttgba_state* s);

  protected:

    /// \brief grab the id of the root associated to an element.
    int root (int i);

    // type def for the main structure of the Union-Find
    typedef Sgi::hash_map<const fasttgba_state*, int,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> uf_map;

    /// \brief the structure used to the storage
    /// An element is associated to an integer
    uf_map el;

    /// \brief For each element store the id of the parent
    std::vector<int> id;

    /// \brief rank associated to each subtrees.
    std::vector<int> rk;

    /// \brief Acceptance associated to each element
    std::vector<markset> acc;

    /// \brief The acceptance dictionary
    acc_dict& acc_;

    /// Avoid to re-create some elements
    markset empty;
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_UNION_FIND_HH
