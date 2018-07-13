// -*- coding: utf-8 -*-
// Copyright (C) 2018 Laboratoire de Recherche et Développement
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

#include "config.h"
#include <spot/misc/clz.hh>

namespace spot
{
  // use gcc and clang built-in functions
  // both compilers use the same function names, and define __GNUC__
#if __GNUC__
  template<class T>
  struct _clz;

  template<>
  struct _clz<unsigned>
  {
    unsigned
    operator()(unsigned n) const noexcept
    {
      return __builtin_clz(n);
    }
  };

  template<>
  struct _clz<unsigned long>
  {
    unsigned long
    operator()(unsigned long n) const noexcept
    {
      return __builtin_clzl(n);
    }
  };

  template<>
  struct _clz<unsigned long long>
  {
    unsigned long long
    operator()(unsigned long long n) const noexcept
    {
      return __builtin_clzll(n);
    }
  };

  size_t
  clz(unsigned n)
  {
    return _clz<unsigned>()(n);
  }

  size_t
  clz(unsigned long n)
  {
    return _clz<unsigned long>()(n);
  }

  size_t
  clz(unsigned long long n)
  {
    return _clz<unsigned long long>()(n);
  }
#else
  size_t
  clz(unsigned n)
  {
    size_t res = CHAR_BIT*sizeof(size_t);
    while (n)
      {
        --res;
        n >>= 1;
      }
    return res;
  }

  size_t
  clz(unsigned long n)
  {
    size_t res = CHAR_BIT*sizeof(unsigned long);
    while (n)
      {
        --res;
        n >>= 1;
      }
    return res;
  }

  size_t
  clz(unsigned long long n)
  {
    size_t res = CHAR_BIT*sizeof(unsigned long long);
    while (n)
      {
        --res;
        n >>= 1;
      }
    return res;
  }
#endif
}
