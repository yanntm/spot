// -*- coding: utf-8 -*-
// Copyright (C) 2016, 2018, 2019 Laboratoire de Recherche et Développement
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

#include "config.h"
#include <spot/twaalgos/parity.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/product.hh>
#include <spot/twaalgos/complete.hh>
#include <spot/twaalgos/sccinfo.hh>
#include <vector>
#include <utility>
#include <functional>
#include <queue>

namespace spot
{
  namespace
  {
    unsigned change_set(unsigned x, unsigned num_sets,
                        bool change_kind, bool change_style)
    {
      // If the parity acceptance kind is changed,
      // then the index of the sets are reversed
      if (change_kind)
        x = num_sets - x - 1;
      // If the parity style is changed, then all the existing acceptance
      // sets are shifted
      x += change_style;
      return x;
    }

    void
    change_acc(twa_graph_ptr& aut, unsigned num_sets, bool change_kind,
               bool change_style, bool output_max, bool input_max)
    {
      for (auto& e: aut->edge_vector())
        if (e.acc)
          {
            unsigned msb = (input_max ? e.acc.max_set() : e.acc.min_set()) - 1;
            e.acc = acc_cond::mark_t{change_set(msb, num_sets, change_kind,
                                                change_style)};
          }
        else if (output_max && change_style)
          {
            // If the parity is changed, a new set is introduced.
            // This new set is used to mark all the transitions of the input
            // that don't belong to any acceptance sets.
            e.acc.set(0);
          }
    }

     [[noreturn]] static void
     input_is_not_parity(const char* fun)
     {
       throw std::runtime_error(std::string(fun) +
                                "(): input should have "
                                "parity acceptance");
     }
  }

  twa_graph_ptr
  change_parity(const const_twa_graph_ptr& aut,
                parity_kind kind, parity_style style)
  {
    return change_parity_here(make_twa_graph(aut, twa::prop_set::all()),
                              kind, style);
  }

  twa_graph_ptr
  change_parity_here(twa_graph_ptr aut, parity_kind kind, parity_style style)
  {
    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd, true))
      input_is_not_parity("change_parity");
    auto old_num_sets = aut->num_sets();

    bool output_max = true;
    switch (kind)
      {
        case parity_kind_max:
          output_max = true;
          break;
        case parity_kind_min:
          output_max = false;
          break;
        case parity_kind_same:
          output_max = current_max;
          break;
        case parity_kind_any:
          // If we need to change the style we may change the kind not to
          // introduce new accset.
          output_max = (((style == parity_style_odd && !current_odd)
                         || (style == parity_style_even && current_odd))
                        && old_num_sets % 2 == 0) != current_max;
          break;
      }

    bool change_kind = current_max != output_max;
    bool toggle_style = change_kind && (old_num_sets % 2 == 0);

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
        case parity_style_any:
          output_odd = current_odd != toggle_style;
          // If we need to change the kind we may change the style not to
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

    if (change_kind || change_style)
      {
        auto new_acc = acc_cond::acc_code::parity(output_max,
                                                  output_odd, num_sets);
        aut->set_acceptance(num_sets, new_acc);
      }
    change_acc(aut, old_num_sets, change_kind,
               change_style, output_max, current_max);
    return aut;
  }

  twa_graph_ptr
  cleanup_parity(const const_twa_graph_ptr& aut, bool keep_style)
  {
    auto result = make_twa_graph(aut, twa::prop_set::all());
    return cleanup_parity_here(result, keep_style);
  }

  twa_graph_ptr
  cleanup_parity_here(twa_graph_ptr aut, bool keep_style)
  {
    unsigned num_sets = aut->num_sets();
    if (num_sets == 0)
      return aut;

    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd, true))
      input_is_not_parity("cleanup_parity");

    // Gather all the used colors, while leaving only one per edge.
    auto used_in_aut = acc_cond::mark_t();
    acc_cond::mark_t allsets = aut->acc().all_sets();
    if (current_max)
      for (auto& t: aut->edges())
        {
          if (auto maxset = (t.acc & allsets).max_set())
            {
              t.acc = acc_cond::mark_t{maxset - 1};
              used_in_aut |= t.acc;
            }
          else
            {
              t.acc = acc_cond::mark_t{};
            }
        }
    else
      for (auto& t: aut->edges())
        {
          t.acc = (t.acc & allsets).lowest();
          used_in_aut |= t.acc;
        }

    if (used_in_aut)
      {
        if (current_max)
          // If max even or max odd: if 0 is not used, we can remove 1, if
          // 2 is not used, we can remove 3, etc.
          // This is obvious from the encoding:
          // max odd n  = ... Inf(3) | (Fin(2) & (Inf(1) | Fin(0)))
          // max even n = ... Fin(3) & (Inf(2) | (Fin(1) & Inf(0)))
          {
            unsigned n = 0;
            while (n + 1 < num_sets && !used_in_aut.has(n))
              {
                used_in_aut.clear(n + 1);
                n += 2;
              }
          }
        else
          // min even and min odd simply work the other way around:
          // min even 4 = Inf(0) | (Fin(1) & (Inf(2) | Fin(3)))
          // min odd 4  = Fin(0) & (Inf(1) | (Fin(2) & Inf(3)))
          {
            int n = num_sets - 1;
            while (n >= 1 && !used_in_aut.has(n))
              {
                used_in_aut.clear(n - 1);
                n -= 2;
              }
          }
      }

    // If no color needed in the automaton, exit early.
    if (!used_in_aut)
      {
        if ((current_max && current_odd)
            || (!current_max && current_odd == (num_sets & 1)))
          aut->set_acceptance(0, acc_cond::acc_code::t());
        else
          aut->set_acceptance(0, acc_cond::acc_code::f());
        for (auto& e: aut->edges())
          e.acc = {};
        return aut;
      }

    // Renumber colors.  Two used colors separated by a unused color
    // can be merged.
    std::vector<unsigned> rename(num_sets);
    int prev_used = -1;
    bool change_style = false;
    unsigned new_index = 0;
    for (auto i = 0U; i < num_sets; ++i)
      if (used_in_aut.has(i))
        {
          if (prev_used == -1)
            {
              if (i & 1)
                {
                  if (keep_style)
                    new_index = 1;
                  else
                    change_style = true;
                }
            }
          else if ((i + prev_used) & 1)
            ++new_index;
          rename[i] = new_index;
          prev_used = i;
        }
    assert(prev_used >= 0);

    // Update all colors according to RENAME.
    // Using max_set or min_set makes no difference since
    // there is now at most one color per edge.
    for (auto& t: aut->edges())
      {
        acc_cond::mark_t m = t.acc & used_in_aut;
        unsigned color = m.max_set();
        if (color)
          t.acc = acc_cond::mark_t{rename[color - 1]};
        else
          t.acc = m;
      }

    unsigned new_num_sets = new_index + 1;
    if (new_num_sets < num_sets)
      {
        auto new_acc =
          acc_cond::acc_code::parity(current_max,
                                     current_odd != change_style,
                                     new_num_sets);
        aut->set_acceptance(new_num_sets, new_acc);
      }
    else
      {
        assert(!change_style);
      }
    return aut;
  }

  twa_graph_ptr
  colorize_parity(const const_twa_graph_ptr& aut, bool keep_style)
  {
    return colorize_parity_here(make_twa_graph(aut, twa::prop_set::all()),
                                keep_style);
  }

  twa_graph_ptr
  colorize_parity_here(twa_graph_ptr aut, bool keep_style)
  {
    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd, true))
      input_is_not_parity("colorize_parity");
    if (!aut->is_existential())
      throw std::runtime_error
        ("colorize_parity_here() does not support alternation");

    bool has_empty_in_scc = false;
    {
      scc_info si(aut, scc_info_options::NONE);
      for (const auto& e: aut->edges())
        if (!e.acc && si.scc_of(e.src) == si.scc_of(e.dst))
          {
            has_empty_in_scc = true;
            break;
          }
    }
    unsigned num_sets = aut->num_sets();
    bool new_odd = current_odd;
    int incr = 0;
    unsigned empty = current_max ? 0 : num_sets - 1;
    if (has_empty_in_scc)
      {
        // If the automaton has an SCC transition that belongs to no set
        // (called "empty trans." below), we may need to introduce a
        // new acceptance set.  What to do depends on the kind
        // (min/max) and style (odd/even) of parity acceptance and the
        // number (n) of colors used.
        //
        // | kind/style | n   | empty tr.  | other tr. | result       |
        // |------------+-----+------------+-----------+--------------|
        // | max odd    | any | set to {0} | add 1     | max even n+1 |
        // | max even   | any | set to {0} | add 1     | max odd n+1  |
        // | min odd    | any | set to {n} | unchanged | min odd n+1  |
        // | min even   | any | set to {n} | unchanged | min even n+1 |
        //
        // In the above table, the "max" cases both change their style
        // We may want to add a second acceptance set to keep the
        // style:
        //
        // | kind/style | n   | empty tr.  | other tr. | result       |
        // |------------+-----+------------+-----------+--------------|
        // | max odd    | any | set to {1} | add 2     | max odd n+2  |
        // | max even   | any | set to {1} | add 2     | max even n+2 |
        if (current_max)
          {
            incr = 1 + keep_style;
            num_sets += incr;
            new_odd = current_odd == keep_style;
            empty = keep_style;
          }
        else
          {
            empty = num_sets++;
          }

        auto new_acc =
          acc_cond::acc_code::parity(current_max, new_odd, num_sets);
        aut->set_acceptance(num_sets, new_acc);
      }

    if (current_max)
      {
        --incr;
        for (auto& e: aut->edges())
          {
            auto maxset = e.acc.max_set();
            e.acc = acc_cond::mark_t{maxset ? maxset + incr : empty};
          }
      }
    else
      {
        for (auto& e: aut->edges())
          e.acc = e.acc ? e.acc.lowest() : acc_cond::mark_t{empty};
      }

    return aut;
  }

}
