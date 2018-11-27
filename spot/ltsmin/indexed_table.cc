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

#include <queue>

#include "spot/ltsmin/indexed_table.hh"
#include "spot/misc/common.hh"
#include "spot/misc/hashfunc.hh"

namespace spot
{
  indexed_table::indexed_table(const size_t state_size)
    : max_size_(IDX_TABLE_MAX_SIZE), nb_element_(0u), min_space_(1u),
      table_(new idx_tuple[IDX_TABLE_MAX_SIZE])
  {
    while (this->min_space_ < state_size)
      this->min_space_ = this->min_space_ << 1;
    this->init(table_, max_size_);
  }

  indexed_table::indexed_table(const size_t size, const size_t state_size)
    : max_size_(size), nb_element_(0u), min_space_(1u),
      table_(new idx_tuple[size])
  {
    while (this->min_space_ < state_size)
      this->min_space_ = this->min_space_ << 1;
    this->init(table_, max_size_);
  }

  void indexed_table::init(idx_tuple_list& t, const size_t size)
  {
    for (size_t i = 0u; i < size; i++)
      {
        t[i] = std::make_tuple(size + 1, size + 1, 0u);
      }
  }

  bool indexed_table::is_empty() const
  {
    return nb_element_ == 0u;
  }

  bool indexed_table::is_full() const
  {
    return (float)(nb_element_) / (float)(max_size_) >= IDX_TABLE_FULL
           || min_space_ > max_size_ - nb_element_;
  }

  void indexed_table::clear()
  {
    if (!this->is_empty())
      {
        table_ = idx_tuple_list(new idx_tuple[max_size_]);
        roots_.clear();
        nb_element_ = 0u;
      }
  }

  indexed_table::isnew_ref
  indexed_table::find_or_put(indexed_table::idx_pair p, bool is_leaf)
  {
    unsigned flag = is_leaf ? LEAF : NODE;
    unsigned h = this->hash(p);
    // collision management
    h = get_free_pos(table_, max_size_, p, h);

    // New element
    if (std::get<2>(table_[h]) == 0u)
      {
        table_[h] = std::make_tuple(p.first, p.second, flag);
        nb_element_++;
        return std::make_pair(h, true);
      }
    // Element already here
    else if (std::get<0>(table_[h]) == p.first
             && std::get<1>(table_[h]) == p.second)
      {
        std::get<2>(table_[h]) |= flag;
        return std::make_pair(h, false);
      }
    else
      SPOT_UNREACHABLE();
  }

  indexed_table::isnew_ref
  indexed_table::find_or_put_root(indexed_table:: idx_pair p, bool is_leaf)
  {
    isnew_ref inserted = this->find_or_put(p, is_leaf);
    unsigned pos = std::get<2>(table_[inserted.first]) >> 2;
    // Check if the root is already declared as root
    if (!inserted.second && pos != 0u)
      return std::make_pair(pos - 1, false);
    else
      return find_or_put_root(inserted.first);
  }

  indexed_table::isnew_ref
  indexed_table::find_or_put_root(unsigned idx)
  {
    unsigned flag = std::get<2>(table_[idx]);
    if (flag >> 2 != 0u)
      return std::make_pair((flag >> 2) - 1, false);

    roots_.push_back(idx);
    std::get<2>(table_[idx]) |= roots_.size() << 2;
    return std::make_pair(roots_.size() - 1, true);
  }

  indexed_table::idx_pair
  indexed_table::get_root(const unsigned idx) const
  {
    return get_pair(roots_[idx]);
  }

  indexed_table::idx_pair
  indexed_table::get_pair(const unsigned idx) const
  {
    SPOT_ASSERT(idx < max_size_);
    auto tmp = table_[idx];
    return std::make_pair(std::get<0>(tmp), std::get<1>(tmp));
  }

  unsigned indexed_table::hash(const indexed_table::idx_pair& p) const
  {
    return this->hash(p, max_size_);
  }

  unsigned indexed_table::hash(const indexed_table::idx_pair& p,
                               const size_t modulo)
  {
    if (p.first == 0u && p.second == 0u)
      return 0u;
    return wang32_hash(p.first ^ wang32_hash(p.second)) % modulo;
  }

  unsigned
  indexed_table::get_free_pos(idx_tuple_list& t, size_t size,
                              idx_pair& p, unsigned h) const
  {
    // collision management
    while ((std::get<2>(t[h]) != 0u)
           && (std::get<0>(t[h]) != p.first || std::get<1>(t[h]) != p.second))
      {
        h = (h + 1) % size;
      }

    return h;
  }

  bool indexed_table::check_size()
  {
    bool res = false;
    while (this->is_full())
      {
        res = true;
        this->resize(2 * max_size_);
      }
    return res;
  }

  void indexed_table::resize(const size_t new_size)
  {
    idx_tuple_list table_resized = idx_tuple_list(new idx_tuple[new_size]);

    idx_list new_idx = idx_list(new unsigned[max_size_]);
    idx_list leaf_idx = idx_list(new unsigned[max_size_]);
    std::queue<unsigned> todo;
    for (size_t i = 0u; i < max_size_; i++)
      {
        leaf_idx[i] = new_size + 1;
        new_idx[i] = new_size + 1;
      }

    // Initialisation of new lists
    this->init(table_resized, new_size);

    // First, insert only leaves
    for (size_t i = 0u; i < max_size_; i++)
      {
        unsigned flag = std::get<2>(table_[i]);
        if (flag & LEAF)
          {
            idx_pair p = std::make_pair(std::get<0>(table_[i]),
                                        std::get<1>(table_[i]));
            unsigned h = hash(p, new_size);
            // Collision management
            h = get_free_pos(table_resized, new_size, p, h);
            // Insertion
            table_resized[h] = std::make_tuple(std::get<0>(table_[i]),
                                               std::get<1>(table_[i]), LEAF);
            leaf_idx[i] = h;
          }
        if (flag & NODE)
          todo.push(i);
      }

    // Then we managed the particular case of nodes just above leaves
    bool is_first_step = true;

    // Then, insert nodes
    while (!todo.empty())
      {
        todo.push(max_size_ + 1);
        unsigned index = todo.front();
        todo.pop();
        while (index != max_size_ + 1)
          {
            // First step after leaves particular case
            idx_tuple working = table_[index];
            unsigned first = is_first_step ? leaf_idx[std::get<0>(working)]
                                           : new_idx[std::get<0>(working)];
            unsigned second = is_first_step ? leaf_idx[std::get<1>(working)]
                                            : new_idx[std::get<1>(working)];
            unsigned flag = std::get<2>(working) & LEAF_NODE;
            // Node without fixed sons
            if (first == new_size + 1 || second == new_size + 1)
              todo.push(index);
            else
              {
                idx_pair p = std::make_pair(first, second);
                unsigned h = hash(p, new_size);
                // collision management
                h = get_free_pos(table_resized, new_size, p, h);

                // Update or insertion
                if (std::get<2>(table_resized[h]) == 0u)
                  {
                    table_resized[h] = std::make_tuple(first, second, NODE);
                    new_idx[index] = h;
                  }
                else if (std::get<0>(table_resized[h]) == first
                         && std::get<1>(table_resized[h]) == second)
                  {
                    std::get<2>(table_resized[h]) |= flag;
                    new_idx[index] = h;
                  }
                else
                  SPOT_UNREACHABLE();
              }
            index = todo.front();
            todo.pop();
          }
        is_first_step = false;
      }

    // Change values pointed by roots
    for (unsigned i = 0u; i < roots_.size(); i++)
      {
        if (new_idx[roots_[i]] == new_size + 1
            && leaf_idx[roots_[i]] != new_size + 1)
          roots_[i] = leaf_idx[roots_[i]];
        roots_[i] = new_idx[roots_[i]];
        std::get<2>(table_resized[roots_[i]]) |= (i + 1) << 2;
      }

    table_ = std::move(table_resized);
    max_size_ = new_size;
  }
}
