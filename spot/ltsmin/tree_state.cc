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
    : state_size_(state_size), table_(state_size_)
  {}

  size_t tree_state_manager::get_state_size()
  {
    return this->state_size_;
  }

  std::pair<unsigned, bool>
  tree_state_manager::find_or_put(int *state)
  {
    table_.check_size();
    if (state_size_ > 2u)
      return rec_find_or_put(state, state_size_);
    else if (state_size_ == 1u)
      return table_.find_or_put_root(std::make_pair((unsigned)(*state), 0u),
                                     true);
    else if (state_size_ == 2u)
      return table_.find_or_put_root(std::make_pair((unsigned)(*state),
                                                    (unsigned)(*(state + 1))),
                                     true);
    else
      return std::make_pair(0u, false);
  }

  std::pair<unsigned, bool>
  tree_state_manager::rec_find_or_put(int* state, size_t size)
  {
    if (!size)
      return std::make_pair(0u, false);
    else if (size == 1)
      {
        int_pair elt = std::make_pair((unsigned)(*state), 0u);
        return table_.find_or_put(elt, true);
      }
    else if (size == 2)
      {
        int_pair elt = std::make_pair((unsigned)(*state),
                                      (unsigned)(*(state + 1)));
        return table_.find_or_put(elt, true);
      }

    size_t right_size = size / TSM_LEAF_NUM;
    size_t left_size = size - right_size;
    int* right_state = state + left_size;
    int* left_state = state;
    auto left_res = rec_find_or_put(left_state, left_size);
    auto right_res = rec_find_or_put(right_state, right_size);

    int_pair element =
      std::make_pair(left_res.first, right_res.first);
    if (size == state_size_)
      return table_.find_or_put_root(element, false);
    else
      return table_.find_or_put(element, false);
  }

  int* tree_state_manager::get_state(unsigned idx)
  {
    int* res = new int[state_size_ + 2];
    int hash_value = 0;
    int_pair root = table_.get_root(idx);
    if (state_size_ > TSM_LEAF_NUM)
      {
        rec_get_state(root.first, res + 2,
                      state_size_ - state_size_ / TSM_LEAF_NUM);
        rec_get_state(root.second,
                      res + 2 + state_size_ - state_size_ / TSM_LEAF_NUM,
                      state_size_ / TSM_LEAF_NUM);
      }
    else if (state_size_ == 2)
      {
        res[2] = root.first;
        res[3] = root.second;
      }
    else
      res[2] = root.first;

    for (unsigned i = 2; i < state_size_ + 2; i++)
      hash_value = wang32_hash(hash_value ^ res[i]);
    res[0] = hash_value;
    res[1] = state_size_;

    return res;
  }

  void tree_state_manager::rec_get_state(unsigned ref, int* res, size_t s)
  {
    while (s > TSM_LEAF_NUM)
      {
        size_t l_s = s - s / TSM_LEAF_NUM;
        int_pair ref_pair = table_.get_pair(ref);
        rec_get_state(ref_pair.second, res + l_s, s / TSM_LEAF_NUM);

        ref = ref_pair.first;
        s = l_s;
      }

    if (s <= TSM_LEAF_NUM)
      {
        int_pair ref_pair = table_.get_pair(ref);
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
  }
}
