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

  /// This Class is a dicionary for acceptance set: it
  /// associates a name to each conditions
  class acc_dict
  {
  public:
    /// A basic Constructor
    acc_dict();

    /// Refine the destructor
    virtual ~acc_dict();

    /// Register an acceptance condition for an automaton
    int register_acc_for_aut(std::string acc,
			     const spot::fasttgba*);

    /// An accessor to the ith acceptance condition
    std::string get(int index);

    /// \brief Return the size of the dictionary
    size_t size() const;

    /// \brief return true if the dictionary is empty
    bool empty() const;

  protected:
    int id_;			///< counter for uniq ref
    std::map<std::string, int> accs_;
    std::map<int, std::string> accsback_;
  };
}
#endif // SPOT_FASTTGBA_ACC_DICT_HH
