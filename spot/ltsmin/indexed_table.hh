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

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#define IDX_TABLE_MAX_SIZE 2048
#define IDX_TABLE_FULL 0.9

namespace spot
{
  /// \brief Indexed hash table used for the compression tree data structure
  ///
  /// The purpose of this table is to keep states of a tree with indexes on the
  /// sons, which are also in the table. In addition, there is a list of roots
  /// to reconstruct these trees.
  ///
  /// The resize function guarantees that the indexes are still valid, and the
  /// reconstruction of the trees are also enable because of the roots'list.
  class indexed_table final
  {
  public:
    using idx_pair = std::pair<unsigned, unsigned>;
    using idx_tuple = std::tuple<unsigned, unsigned, unsigned>;
    using idx_tuple_list = std::unique_ptr<idx_tuple[]>;
    using idx_list = std::unique_ptr<unsigned[]>;
    using isnew_ref = std::pair<unsigned, bool>;

    /// \brief Type of state registered in the table
    enum use
    {
      EMPTY = 0x00,
      LEAF = 0x01,
      NODE = 0x02,
      LEAF_NODE = 0x03
    };

    /// \brief Basic constructor.
    ///
    /// Size intialized at IDX_TABLE_MAX_SIZE by default.
    indexed_table(const size_t state_size);
    /// \brief Constructor with the initial size at \a size.
    indexed_table(const unsigned long size, const size_t state_size);

    /// \brief Check if the table is empty or not.
    bool is_empty() const;
    /// \brief Check if the table is near to be full .
    ///
    /// Compute the ratio number of elements by maximum size and compare it
    /// with IDX_TABLE_FULL.
    bool is_full() const;
    /// \brief Clear the content of the table.
    ///
    /// Remove all roots and nodes. However, the maximum size is kept and not
    /// reset to the first size at the initialization.
    void clear();

    /// \brief Insert a node or a leaf in the table.
    ///
    /// Insert a node or a leaf in the table. The parameter \a is_leaf shoud
    /// be set to true if it is a node, false otherwise.
    /// \return The index in the table where the value has been inserted or
    /// found, and a boolean set to true if the value is new, false otherwise.
    isnew_ref find_or_put(idx_pair p, bool is_leaf);
    /// \brief Insert a root in the table.
    ///
    /// Insert the new node and then add it to the root table.
    /// \return The index in the table where the value has been inserted or
    /// found in the root table, and a boolean set to true if the value is
    /// new, false otherwise.
    isnew_ref find_or_put_root(idx_pair p, bool is_leaf);
    isnew_ref find_or_put_root(unsigned idx);
    /// \brief Get a root from the index of the root table.
    idx_pair get_root(const unsigned idx) const;
    /// \brief Get a node or a leaf of the table from an index.
    idx_pair get_pair(const unsigned idx) const;

    bool check_size();

  private:
    /// \brief Hash function of the table
    unsigned hash(const idx_pair& p) const;
    /// \brief Hash function used by the table with a maximum parameter.
    ///
    /// \a modulo is the maximum used by the hash function to be in the range
    /// of the table.
    static unsigned hash(const idx_pair& p, const size_t modulo);
    /// \brief Initialisation function for the table usedin constructors.
    void init(idx_tuple_list& t, const size_t size);
    /// \brief Get an empty place in the table from a hash.
    unsigned get_free_pos(idx_tuple_list& t, size_t size,
                          idx_pair& p, unsigned h) const;
    /// \brief Resize the table to the \a size.
    void resize(const size_t size);

    unsigned long max_size_;
    unsigned long nb_element_;
    unsigned long min_space_;
    
    idx_tuple_list table_;
    std::vector<unsigned> roots_;
  };
}
