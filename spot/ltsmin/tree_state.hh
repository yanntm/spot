// -*- coding: utf-8 -*-
// Copyright (C) 2018 Laboratoire de Recherche et Développement de
// l'Epita (LRDE)
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

#pragma once

#include "spot/misc/common.hh"
#include <unordered_set>
#include <utility>

#define TSM_LEAF_NUM 2

namespace std
{
  /// \brief Hash function for unordered_set of pairs
  template<>
  struct hash<std::pair<std::intptr_t, std::intptr_t>>
  {
    size_t operator()(std::pair<std::intptr_t,std::intptr_t> const& p) const noexcept
    {
      std::hash<int> hash_int;
      // Change this (really ugly)
      size_t res = hash_int(p.first + hash_int(p.second));
      return res;
    }
  };
}

namespace spot
{
  /// \brief This class manages the states of an automaton with the
  /// datastructure describe in the following paper:
  ///
  /// @InProceedings{10.1007/978-3-642-22306-8_4,
  /// author="Laarman, Alfons and van de Pol, Jaco and Weber, Michael",
  /// editor="Groce, Alex and Musuvathi, Madanlal",
  /// title="Parallel Recursive State Compression for Free",
  /// booktitle="Model Checking Software",
  /// year="2011",
  /// publisher="Springer Berlin Heidelberg",
  /// address="Berlin, Heidelberg",
  /// pages="38--56",
  /// }

  class SPOT_API tree_state_manager final
  {
  public:
    using int_pair = std::pair<std::intptr_t, std::intptr_t>;
    using int_pair_set = std::unordered_set<int_pair>;

    tree_state_manager(unsigned state_size);
    tree_state_manager(const tree_state_manager&) = delete;
    tree_state_manager& operator=(const tree_state_manager&) = delete;
    ~tree_state_manager();

    /// \brief Find or put a value in the tree
    ///
    /// \return A pair containing the reference to the found or inserted value
    /// and a boolean at true if the value has been inserted, or false if it
    /// has been found.
    std::pair<const void*, bool> find_or_put(int *state, size_t size);

    /// \bief Get a state from a reference to a root of a state tree
    int* get_state(const void* ref);

    size_t get_state_size();

  private:
    /// \brief Node structure for the tree of the state
    struct tree;

    struct node
    {
      tree* left_;
      tree* right_;
      int_pair_set table_;
      int k;

      node(unsigned size);
      ~node();
    };

    /// \brief Tree structure to register the state
    struct tree
    {
      node* node_;
      bool leaf_;

      tree(unsigned size);
      ~tree();
    };

    /// \brief Recursive \cr find_or_put function with the tree in added parameter
    std::pair<const void*, bool>
      rec_find_or_put(int *state, size_t size, tree* t);
    /// \brief \cr find_or_put function for the table
    std::pair<const void*, bool> table_find_or_put(
        int_pair& element,
        int_pair_set& table);
    /// \brief Recursive \cr get_state function
    void rec_get_state(const void* ref, int* res, tree* t, size_t s);

    size_t state_size_;
    tree tree_;
  };
}
