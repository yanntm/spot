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
#include <spot/twaalgos/product.hh>
#include <vector>
#include <utility>
#include <functional>
#include <queue>
#include <spot/twaalgos/hoa.hh>

namespace spot
{
  namespace
  {
    unsigned change_set(unsigned x, const unsigned num_sets,
                        const bool change_order, const bool change_style)
    {
      if (change_order)
        {
          // If the parity acceptance order is changed,
          // then the index of the sets are reversed
          x = num_sets - x - 1;
        }
      if (change_style)
        {
          // If the parity style is changed, then all the existing acceptance
          // sets are shifted
          ++x;
        }
      return x;
    }

    void
    change_acc(twa_graph_ptr& aut, unsigned num_sets, const bool change_order,
               const bool change_style, const bool output_max,
               const bool input_max)
    {
      for (auto& e: aut->edge_vector())
        if (e.acc)
          {
            unsigned msb = 0U;
            if (input_max)
              msb = e.acc.max_set() - 1;
            else
              for (auto i = 0U; i < num_sets; ++i)
                if (e.acc.has(i))
                  {
                    msb = i;
                    break;
                  }
            e.acc = acc_cond::mark_t();
            e.acc.set(change_set(msb, num_sets, change_order, change_style));
          }
        else if (output_max && change_style)
          {
            // If the parity is changed, a new set is introduced.
            // A parity max acceptance will mark the transitions which do not
            // belong to any set with this new set.
            // A parity min acceptance will introduce a unused acceptance set.
            e.acc.set(0);
          }
    }
  }

  twa_graph_ptr
  change_parity(const const_twa_graph_ptr& aut,
                parity_order order, parity_style style)
  {
    bool current_max;
    bool current_odd;
    //std::cout << "HERE" << std::endl;
    if (!aut->acc().is_parity(current_max, current_odd))
      throw new std::invalid_argument("change_parity_acceptance: The first"
                                      "argument aut must have a parity "
                                      "acceptance.");
    //std::cout << "HERE2" << std::endl;
    auto result = copy(aut, twa::prop_set::all());
    auto old_num_sets = result->num_sets();

    bool output_max = true;
    switch (order)
      {
        case parity_order_max:
          output_max = true;
          break;
        case parity_order_min:
          output_max = false;
          break;
        case parity_order_same:
          output_max = current_max;
          break;
        case parity_order_dontcare:
          // If we need to change the style we may change the order not to
          // introduce new accset.
          output_max = (((style == parity_style_odd && !current_odd)
                         || (style == parity_style_even && current_odd))
                        && old_num_sets % 2 == 0) != current_max;
      }

    bool change_order = current_max != output_max;
    bool toggle_style = change_order && (old_num_sets % 2 == 0);

    bool output_odd = true;
    switch (style)
      {
        case parity_style_odd:
          output_odd = true;
          break;
        case parity_style_even:
          output_odd = false;
          break;
        case parity_style_same:
          output_odd = current_odd;
          break;
        case parity_style_dontcare:
          output_odd = current_odd != toggle_style;
          // If we need to change the order we may change the style not to
          // introduce new accset.
          break;
      }

    current_odd = current_odd != toggle_style;
    bool change_style = false;
    auto num_sets = old_num_sets;
    // If the parity neeeds to be changed, then a new acceptance set is created.
    // The old acceptance sets are shifted
    if (output_odd != current_odd)
      {
        change_style = true;
        ++num_sets;
      }

    if (change_order || change_style)
      {
        auto new_acc = acc_cond::acc_code::parity(output_max,
                                                  output_odd, num_sets);
        result->set_acceptance(num_sets, new_acc);
      }
    change_acc(result, old_num_sets, change_order,
               change_style, output_max, current_max);
    return result;
  }
}
