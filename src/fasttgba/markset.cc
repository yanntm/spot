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
#include "markset.hh"

namespace spot
{
  markset::markset(boost::dynamic_bitset<> m)
  {
    markset_ = m;
  }

  markset::markset(size_t size):
    markset_(size)
  { }

  markset::markset(const markset& b)
  {
    markset_ = b.markset_;
  }

  markset&
  markset::operator&=(const markset& b)
  {
    markset_ &= b.markset_;
    return *this;
  }

  markset&
  markset::operator|=(const markset& b)
  {
    markset_ |= b.markset_;
    return *this;
  }

  markset&
  markset::operator^=(const markset& b)
  {
    markset_ ^= b.markset_;
    return *this;
  }

  markset&
  markset::operator-=(const markset& b)
  {
    markset_ -= b.markset_;
    return *this;
  }

  markset&
  markset::operator=(const markset& b)
  {
    markset_ = b.markset_;
    return *this;
  }

  void
  markset::set_mark(mark m)
  {
    markset_[m];
  }

  mark
  markset::one()
  {
    assert(false);
  }

  bool
  markset::empty()
  {
    return markset_.none();
  }

  size_t
  markset::size()
  {
    return markset_.size();//FIXME
  }

  markset
  markset::operator~() const
  {
   return markset(~markset_);
  }

  std::string
  markset::dump(std::vector<std::string> acc)
  {
    std::ostringstream oss;
    if (acc.empty())
      oss << markset_;
    else if  (acc.size() == 1 && markset_[0])
      {
	oss << "[1] ";
      }
    else
      {
	assert(acc.size() == markset_.size());
	size_t i;
	for (i = 0; i < markset_.size(); ++i)
	  if (markset_[i])
	    oss << "[" << acc[i] << "] ";
      }
    return oss.str();
  }
}
