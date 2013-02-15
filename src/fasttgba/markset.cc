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
  markset::markset(boost::dynamic_bitset<> m, acc_dict& acc):
    accs_(acc)
  {
    markset_ = m;
  }

  markset::markset(acc_dict& acc):
    markset_(acc.size()),
    accs_(acc)
  { }

  markset::markset(const markset& b):
    accs_(b.accs_)
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
  markset::operator-=(const mark b)
  {
    markset_[b] = 0;
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
    assert(((size_t) m) < markset_.size());
    markset_[m] = 1;
  }

  mark
  markset::one()
  {
    size_t pos = markset_.find_first();
    if ( pos != markset_.npos)
      return pos;
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
    return markset_.count();
  }

  markset
  markset::operator~() const
  {
    return markset(~markset_, accs_);
  }

  markset
  markset::operator&(const markset& b) const
  {
    return markset(markset_ &  b.markset_, accs_);
  }
  markset
  markset::operator|(const markset& b) const
  {
    return markset(markset_ |  b.markset_, accs_);
  }

  std::string
  markset::dump()
  {
    std::ostringstream oss;
    if (accs_.size() == 0)
      oss << markset_;
    else if  (accs_.size() == 1 && markset_[0])
      {
	oss << "[1] ";
      }
    else
      {
	assert(accs_.size() == markset_.size());
	size_t i;
	for (i = 0; i < markset_.size(); ++i)
	  if (markset_[i])
	    oss << "[" << accs_.get(i) << "] ";
      }
    return oss.str();
  }
}
