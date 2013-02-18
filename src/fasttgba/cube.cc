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

#include <sstream>
#include <iostream>
#include "cube.hh"

namespace spot
{
  cube::cube (ap_dict& aps) : aps_(aps), valid(true)
  {
    true_var = boost::dynamic_bitset<>(aps_.size());
    false_var = boost::dynamic_bitset<>(aps_.size());
  }

  cube::cube (const cube& c): aps_(c.aps_)
  {
    true_var = c.true_var;
    false_var = c.false_var;
  }

  bool
  cube::operator==(const cube::cube& rhs)
  {
    return (true_var == rhs.true_var)
      &&  (false_var == rhs.false_var);
  }

  cube
  cube::operator&(const cube::cube& rhs) const
  {
    cube result (rhs);
    result.true_var |= true_var;
    result.false_var |=  false_var;
    return result;
  }

  void
  cube::set_true_var(size_t index)
  {
    assert(index < size());
    true_var[index] = 1;
    false_var[index] = 0;
  }

  void
  cube::set_false_var(size_t index)
  {
    assert(index < size());
    true_var[index] = 0;
    false_var[index] = 1;
  }

  void
  cube::set_free_var(size_t index)
  {
    assert(index < size());
    true_var[index] = 0;
    false_var[index] = 0;
  }

  size_t
  cube::size() const
  {
    return true_var.size();
  }

  bool
  cube::is_valid() const
  {
    // Here we check if there is no variable that must
    // be true and false at the same time.
    boost::dynamic_bitset<> r =  true_var & false_var;
    if (r.none())
      {
    	return true;
      }
    return false;
  }

  std::string
  cube::dump()
  {
    std::ostringstream oss;
    size_t i;
    bool all_free = true;
    for (i = 0; i < size(); ++i)
      {
	if (true_var[i])
	  {
	    oss << aps_.get(i)->name()
		<< (i != (size() - 1) ? " ": "");
	    all_free = false;
	  }
	else if (false_var[i])
	  {
	    oss << "! " << aps_.get(i)->name()
	      	<< (i != (size() - 1) ? " ": "");
	    all_free = false;
	  }
      }
    if (all_free)
      oss << "1";
    return oss.str();
  }
}
