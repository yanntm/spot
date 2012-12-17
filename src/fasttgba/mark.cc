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
#include "mark.hh"

namespace spot
{
  mark::mark(boost::dynamic_bitset<> m)
  {
    mark_ = m;
  }

  mark::mark(int size):
    mark_(size)
  { }

  mark::mark(const mark& b)
  {
    mark_ = b.mark_;
  }

  mark&
  mark::operator&=(const mark& b)
  {
    mark_ &= b.mark_;
    return *this;
  }

  mark&
  mark::operator|=(const mark& b)
  {
    mark_ |= b.mark_;
    return *this;
  }

  mark&
  mark::operator^=(const mark& b)
  {
    mark_ ^= b.mark_;
    return *this;
  }

  mark&
  mark::operator-=(const mark& b)
  {
    mark_ -= b.mark_;
    return *this;
  }

  mark&
  mark::operator <<= (int n)
  {
    mark_ <<= n;
    return *this;
  }

  mark&
  mark::operator >>= (int n)
  {
    mark_ >>= n;
    return *this;
  }

  mark&
  mark::operator=(const mark& b)
  {
    mark_ = b.mark_;
    return *this;
  }

  mark::storage_elt
  mark::operator[](int pos)
  {
    return mark_[pos];
  }

  bool
  mark::null()
  {
    size_t i;
    for (i = 0; i < mark_.size(); ++i)
      if (mark_[i] == 1)
	return false;
    return true;
  }

  mark
  mark::operator~() const
  {
   return mark(~mark_);
  }

  std::string
  mark::dump(std::vector<std::string> acc)
  {
    std::ostringstream oss;
    if (acc.empty())
      oss << mark_;
    else if  (acc.size() == 1 && mark_[0])
      {
	oss << "[1] ";
      }
    else
      {
	assert(acc.size() == mark_.size());
	size_t i;
	for (i = 0; i < mark_.size(); ++i)
	  if (mark_[i])
	    oss << "[" << acc[i] << "] ";
      }
    return oss.str();
  }
}
