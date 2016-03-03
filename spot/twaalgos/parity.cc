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

    std::function<unsigned(unsigned)> change_set;
    if (current_max != max)
    {
      // Reverse a bit position
      change_set =
        [ num_sets ] (unsigned x) { return num_sets - x - 1; };

      // If the number of acceptance sets is even, the parity is toggled.
      current_odd = current_odd != (num_sets % 2 == 0);
    }
    else
      change_set =
        [] (unsigned x) { return x; };

    // If the parity neeeds to be changed, then a new acceptance set is created.
    // The old acceptance sets are shifted
    if (odd != current_odd)
    {
      ++num_sets;
      change_set =
        [ change_set ] (unsigned x) { return change_set(x) + 1; };
    }
    if (max != current_max || current_odd != odd)
    {
      auto new_acc = acc_cond::acc_code::parity(max, odd, num_sets);
      result->set_acceptance(num_sets, new_acc);
    }

    std::function<void(acc_cond::mark_t&)> change_acc;
    if (max)
      change_acc =
        [ odd, current_odd, change_set ] (acc_cond::mark_t& acc)
        {
          acc_cond::mark_t new_mark(0U);
          auto max_set = acc.max_set();
          // If the parity is changed, a new set is introduced.
          // In max parity the transitions which do not belong to any set will
          // belong to this new set.
          if (odd != current_odd && max_set == 0)
            new_mark.set(0);
          else
            new_mark.set(change_set(max_set - 1));
          acc = new_mark;
        };
    else
      change_acc =
        [ num_sets, change_set ] (acc_cond::mark_t& acc)
        {
          if (acc)
            for (auto i = 0U; i < num_sets; ++i)
              if (acc.has(i))
              {
                acc_cond::mark_t new_mark(0U);
                new_mark.set(change_set(i));
                acc = new_mark;
                break;
              }
        };

    for (auto& i: result->edge_vector())
      change_acc(i.acc);
    return result;
  }
}
