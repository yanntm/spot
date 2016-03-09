// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et Développement
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

#include <spot/twaalgos/parity.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/copy.hh>

namespace spot
{
  namespace
  {
    unsigned change_set(unsigned x,
                        const unsigned num_sets,
                        const bool change_max,
                        const bool change_odd)
    {
      if (change_max)
        x = num_sets - x - 1;
      if (change_odd)
        ++x;
      return x;
    }

    void change_acc(twa_graph_ptr& aut,
                    unsigned num_sets,
                    const bool change_max,
                    const bool change_odd,
                    const bool max)
    {
      for (auto& i: aut->edge_vector())
        if (max)
        {
          auto maxset = i.acc.max_set();
          // If the parity is changed, a new set is introduced.
          // In max parity the transitions which do not belong to any set will
          // belong to this new set.
          i.acc &= 0;
          if (change_odd && maxset == 0)
            i.acc.set(0);
          else
            i.acc.set(change_set(maxset - 1, num_sets, change_max, change_odd));
        }
        else if (i.acc)
          i.acc = change_set(i.acc.lowest(), num_sets, change_max, change_odd);
    }
  }

  twa_graph_ptr change_parity_acceptance(const const_twa_graph_ptr& aut,
                                         bool max,
                                         bool odd)
  {
    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd))
      throw new std::runtime_error("change_parity_acceptance: The first"
                                   "argument aut must be a parity automaton.");
    auto result = copy(aut, twa::prop_set::all());

    unsigned num_sets = result->num_sets();
    bool change_max = false;
    bool change_odd = false;

    if (current_max != max)
    {
      change_max = true;
      // If the number of acceptance sets is even, the parity is toggled.
      current_odd = current_odd != (num_sets % 2 == 0);
    }

    // If the parity neeeds to be changed, then a new acceptance set is created.
    // The old acceptance sets are shifted
    if (odd != current_odd)
    {
      change_odd = true;
      ++num_sets;
    }

    if (max != current_max || current_odd != odd)
    {
      auto new_acc = acc_cond::acc_code::parity(max, odd, num_sets);
      result->set_acceptance(num_sets, new_acc);
    }

    change_acc(result, num_sets, change_max, change_odd, max);
    return result;
  }
}
