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

#include "config.h"
#include <cassert>

#include <spot/ltsmin/tree_state.hh>
#include <spot/misc/hashfunc.hh>

namespace spot
{
  tree_state_manager::tree_state_manager(unsigned state_size)
    : state_size_(state_size), tree_(state_size)
  {}

  tree_state_manager::~tree_state_manager()
  {
    /* FIXME */
  }

  size_t tree_state_manager::get_state_size()
  {
    return this->state_size_;
  }

  std::pair<const void*, bool>
  tree_state_manager::find_or_put(int *state, size_t size)
  {
    return rec_find_or_put(state, size, &tree_);
  }

  std::pair<const void*, bool>
  tree_state_manager::rec_find_or_put(int* state, size_t size, tree* t)
  {
    if (!size)
      return std::make_pair(nullptr, false);
    else if (size == 1)
      {
        int_pair elt = std::make_pair(*state, 0);
        return table_find_or_put(elt, t->node_->table_);
      }
    else if (size == 2)
      {
        int_pair elt = std::make_pair(*state, *(state + 1));
        return table_find_or_put(elt, t->node_->table_);
      }

    size_t right_size = size / TSM_LEAF_NUM;
    size_t left_size = size - right_size;
    int* right_state = state + left_size;
    int* left_state = state;
    auto left_res = rec_find_or_put(left_state, left_size, t->node_->left_);
    auto right_res = rec_find_or_put(right_state, right_size, t->node_->right_);

    int_pair element =
      std::make_pair(reinterpret_cast<intptr_t>(left_res.first),
                     reinterpret_cast<intptr_t>(right_res.first));
    return table_find_or_put(element, t->node_->table_);
  }

  std::pair<const void*, bool>
  tree_state_manager::table_find_or_put(int_pair element, int_pair_set& table)
  {
    auto&& search = table.find(element);
    if (search != table.end())
      {
        const void* res_ptr = &(*(search));
        return std::make_pair(res_ptr, false);
      }
    else
      {
        auto res = table.insert(element);
        const void* res_ptr = &(*(res.first));
        return std::make_pair(res_ptr, true);
      }
  }

  int* tree_state_manager::get_state(const void* ref)
  {
    int* res = new int[state_size_ + 2];
    int hash_value = 0;
    rec_get_state(ref, res + 2, &tree_, state_size_);

    for (unsigned i = 2; i < state_size_ + 2; i++)
      hash_value = wang32_hash(hash_value ^ res[i]);
    res[0] = hash_value;
    res[1] = state_size_;

    return res;
  }

  void tree_state_manager::rec_get_state(const void* ref, int* res,
                                         tree* t, size_t s)
  {
    while (!t->leaf_ || s > TSM_LEAF_NUM)
      {
        size_t l_s = s - s / TSM_LEAF_NUM;
        void* ref2 = const_cast<void*>(ref);
        int_pair ref_pair = *(static_cast<int_pair*>(ref2));
        rec_get_state((void*)(ref_pair.second), res + l_s,
                      t->node_->right_, s / TSM_LEAF_NUM);

        ref = (void*)(ref_pair.first);
        t = t->node_->left_;
        s = l_s;
      }

    if (t->leaf_ && s <= TSM_LEAF_NUM)
      {
        void* ref2 = const_cast<void*>(ref);
        int_pair ref_pair = *(static_cast<int_pair*>(ref2));
        if (s == 1)
          {
            *res = ref_pair.first;
          }
        else if (s == TSM_LEAF_NUM)
          {
            *res = ref_pair.first;
            *(res + 1) = ref_pair.second;
          }
      }
    else if (t->leaf_ || s <= TSM_LEAF_NUM)
      SPOT_UNREACHABLE();
  }

  tree_state_manager::node::node(unsigned size)
    : table_(size)
  {
    if (size <= TSM_LEAF_NUM)
      {
        left_ = nullptr;
        right_ = nullptr;
      }
    else
      {
        left_ = new tree(size - size / TSM_LEAF_NUM);
        right_ = new tree(size / TSM_LEAF_NUM);
      }
    k = size;
  }

  tree_state_manager::node::~node()
  {
    delete(left_);
    delete(right_);
  }

  tree_state_manager::tree::tree(unsigned size)
  {
    assert(size != 0);
    this->node_ = new node(size);
    this->leaf_ = size <= TSM_LEAF_NUM;
  }

  tree_state_manager::tree::~tree()
  {
    delete(node_);
  }
}
