// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita
// (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <unordered_map>
#include <vector>

#include <spot/misc/common.hh>

namespace spot
{

  // a helper class to hash things
  template<class T>
  struct hash
  {
    size_t
    operator()(const T& t) const
    {
      return t.hash();
    }
  };

  template<class T>
  struct equal
  {
    bool
    operator()(const T& lhs, const T& rhs) const
    {
      return std::equal_to<T>()(lhs, rhs);
    }
  };

  template<class T>
  struct less_than
  {
    bool
    operator()(const T& lhs, const T& rhs) const
    {
      return std::less<T>()(lhs, rhs);
    }
  };

  // A helper class that stores elements associated with an index.
  template<class T>
  class index_map
  {
  public:
    explicit index_map(const hash<T>& h, const equal<T>& e)
    : indices_(0, h, e)
    {}
    index_map(): index_map(hash<T>(), equal<T>()) {}

    unsigned
    find_or_add(T&& t)
    {
      // look for the element in the hashmap, and insert it if not found.
      auto it = indices_.emplace(t, data_.size());
      if (it.second) // insertion took place, the element is new
        data_.push_back(&it.first->first);
      return it.first->second;
    }
    unsigned
    find_or_add(const T& t)
    {
      // look for the element in the hashmap, and insert it if not found.
      auto it = indices_.insert(std::make_pair(t, data_.size()));
      if (it.second) // insertion took place, the element is new
        data_.push_back(&it.first->first);
      return it.first->second;
    }

    unsigned
    find(const T& t) const
    {
      return indices_.at(t);
    }
    const T&
    get(unsigned i) const
    {
      SPOT_ASSERT(i < data_.size());
      return *data_[i];
    }
    unsigned size() const { return data_.size(); }

    // iterator interface
    class iterator
    {
      typename std::vector<const T*>::const_iterator it;
    public:
      explicit iterator(typename std::vector<const T*>::const_iterator&& i)
      : it(i)
      {}

      const T& operator*() const { return **it; }
      const T* operator->() const { return *it; }
      iterator& operator++() { ++it; return *this; }
      bool operator!=(const iterator& o) const { return it != o.it; }
    };
    iterator begin() const { return iterator(data_.begin()); }
    iterator end() const { return iterator(data_.end()); }
  private:
    std::unordered_map<T, unsigned, hash<T>, equal<T>> indices_;
    std::vector<const T*> data_;
  };
}
