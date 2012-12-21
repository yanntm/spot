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

#ifndef SPOT_FASTTGBA_ACC_DICT_HH
# define SPOT_FASTTGBA_ACC_DICT_HH

#include <string>
#include "ltlast/atomic_prop.hh"

namespace spot
{
  class fasttgba;

  class acc_dict
  {
  public:
    acc_dict();

    virtual ~acc_dict();

    int register_acc_for_aut(std::string acc,
			     const spot::fasttgba*);
    std::string get(int index);

    size_t size();

  protected:
    int id_;
    std::map<std::string, int> accs_;
    std::map<int, std::string> accsback_;
  };
}
#endif // SPOT_FASTTGBA_ACC_DICT_HH
