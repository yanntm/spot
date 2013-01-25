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

#include "acc_dict.hh"

namespace spot
{
  acc_dict::acc_dict() : id_(0)
  {
  }

  acc_dict::~acc_dict()
  {
  }

  int
  acc_dict::register_acc_for_aut(std::string acc,
				const spot::fasttgba*)
  {
    std::map<std::string, int>::iterator it = accs_.find(acc);
    if (it != accs_.end())
      return it->second;

    accs_.insert(std::make_pair(acc, id_));
    accsback_.insert(std::make_pair(id_, acc));
    ++id_;
    return (id_-1);
  }

  std::string
  acc_dict::get(int index)
  {
    std::map<int, std::string>::iterator it = accsback_.find(index);
    if (it != accsback_.end())
      return it->second;
    assert(false);
    return 0;
  }

  size_t
  acc_dict::size()
  {
    return accs_.size();
  }
}
