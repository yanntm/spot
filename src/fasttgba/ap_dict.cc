// Copyright (C) 2012 Laboratoire de Recherche et Développement
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


#include "ap_dict.hh"

namespace spot
{
  ap_dict::ap_dict() : id_(0)
  {
  }

  ap_dict::~ap_dict()
  {
    std::map<const ltl::atomic_prop*, int>::const_iterator itr;

    for (itr = aps_.begin(); itr != aps_.end(); ++itr)
      {
	itr->first->destroy();
      }
    aps_.clear();
    apsback_.clear();
  }

  int
  ap_dict::register_ap_for_aut(const ltl::atomic_prop* ap,
			       const spot::fasttgba*)
  {
    //ap->clone();
    std::map<const ltl::atomic_prop*, int>::iterator it = aps_.find(ap);
    if (it != aps_.end())
      return it->second;

    aps_.insert(std::make_pair(ap, id_));
    apsback_.insert(std::make_pair(id_, ap));
    ++id_;
    return id_-1;
  }

  const ltl::atomic_prop*
  ap_dict::get(int index)
  {
    std::map<int, const ltl::atomic_prop*>::iterator it = apsback_.find(index);
    if (it != apsback_.end())
      return it->second;
    assert(false);
    return 0;
  }

  size_t
  ap_dict::size()
  {
    return aps_.size();
  }

  bool
  ap_dict::empty()
  {
    return aps_.empty();
  }
}
