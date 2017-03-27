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

#include <vector>

#include <spot/misc/common.hh>

namespace spot
{

  // TODO replace this by a matrix library
  template<class T, class Sum, class Prod>
  class matrix
  {
  public:
    explicit matrix(unsigned n)
    : matrix_(n*n)
    , size_(n)
    {}

    /// accessors
    T& operator()(unsigned i, unsigned j)
    {
      SPOT_ASSERT(i < size_);
      SPOT_ASSERT(j < size_);
      return matrix_[i + j*size_];
    }
    const T& operator()(unsigned i, unsigned j) const
    {
      SPOT_ASSERT(i < size_);
      SPOT_ASSERT(j < size_);
      return matrix_[i + j*size_];
    }

    unsigned size() const { return size_; }
    std::size_t hash() const
    {
      std::size_t res = 0;
      auto h = std::hash<T>();
      for (const auto& s : matrix_)
        for (const auto& m : s)
          res ^= h(m) + 0x9e3779b9 + (res << 6) + (res >> 2);
      return res;
    }

    static
    matrix
    matrix_prod(const matrix& lhs, const matrix& rhs)
    {
      SPOT_ASSERT(lhs.size() == rhs.size());
      matrix res(lhs.size());
      auto prod = Prod();
      auto sum = Sum();
      // this is a product of matrices, maybe it could be optimized
      for (unsigned i = 0; i != lhs.size(); ++i)
        for (unsigned j = 0; j != lhs.size(); ++j)
          for (unsigned k = 0; k != lhs.size(); ++k)
            {
              auto tmp = prod(lhs(i, k), rhs(k, j));
              res(i, j) = sum(res(i, j), tmp);
            }
      return res;
    }
  private:
    std::vector<T> matrix_;
    unsigned size_;
  };

}
