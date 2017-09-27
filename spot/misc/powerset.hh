// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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

#include <vector>
#include <spot/misc/common.hh>

#pragma once

namespace spot
{
  /// \brief Container interface for iterating over the subsets of a
  /// container of type T.
  template <typename T>
  class powerset
  {
  public:
    class iterator
    {
    public:
      iterator(const std::vector<typename T::key_type>& vec,
               std::vector<bool> filter,
               bool end = false)
        : vec_(vec), filter_(filter), end_(end)
      {
        SPOT_ASSERT(filter.size() == vec_.size() + 1);
      }

      iterator& operator++()
      {
        // Next binary digit in gray code order.
        unsigned nb1 = 0;
        for (unsigned i = 0; i < filter_.size(); ++i)
          nb1 += filter_[i];
        unsigned i;
        if (nb1 % 2 == 0)
            i = 0;
        else
          {
            unsigned j = 0;
            for (; !filter_[j]; ++j)
              continue;
            i = j + 1;
          }
        filter_[i] = !filter_[i];
        if (i < vec_.size())
          {
            if (!filter_[i])
              subset_.erase(vec_[i]);
            else
              subset_.insert(vec_[i]);
          }
        return *this;
      }

      bool operator==(const iterator& o)
      {
        return filter_ == o.filter_;
      }

      bool operator!=(const iterator& o)
      {
        return !(*this == o);
      }

      T operator*()
      {
        return subset_;
      }

      private:
        std::vector<typename T::key_type> vec_;
        T subset_;
        std::vector<bool> filter_;
        bool end_;
    };

    powerset(const T& set)
      : vec_(set.begin(), set.end())
    {}

    iterator begin()
    {
      return iterator(vec_, std::vector<bool>(vec_.size() + 1));
    }

    iterator end()
    {
      std::vector<bool> filt(vec_.size() + 1, false);
      filt.back() = true;
      if (filt.size() >= 2)
        filt[filt.size() - 2] = true;
      return iterator(vec_, filt);
    }

  private:
    std::vector<typename T::key_type> vec_;
  };


  template <typename T>
  powerset<T> make_powerset(const T& set)
  {
    return powerset<T>(set);
  }
}
