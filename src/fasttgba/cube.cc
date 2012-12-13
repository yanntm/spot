// Copyright (C) 2012 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
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

#include <iostream>
#include "cube.hh"


namespace spot
{
  cube::cube (int size) : size_(size)
  {
    true_var = boost::dynamic_bitset<>(size);
    false_var = boost::dynamic_bitset<>(size);
  }

  cube::cube
  cube::operator~() const
  {
    cube c (size_);
    std::swap (c.true_var, c.false_var);
    return c;
  }

  bool
  cube::operator==(const cube::cube& rhs)
  {
    return (true_var == rhs.true_var)
      &&  (false_var == rhs.false_var);
  }

  void
  cube::set_true_var(size_t index)
  {
    true_var[index] = 1;
    false_var[index] = 0;
  }

  void
  cube::set_false_var(size_t index)
  {
    true_var[index] = 0;
    false_var[index] = 1;
  }

  void
  cube::set_free_var(size_t index)
  {
    true_var[index] = 0;
    false_var[index] = 0;
  }

  void
  cube::unset_true_var(size_t index)
  {
    true_var[index] = 0;
    false_var[index] = 1;
  }

  void
  cube::unset_false_var(size_t index)
  {
    true_var[index] = 1;
    false_var[index] = 0;
  }

  void
  cube::dump()
  {
    std::cout << "true var  : " << true_var << std::endl;
    std::cout << "false var : " << false_var << std::endl;
  }
}
