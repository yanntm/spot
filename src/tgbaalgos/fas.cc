// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement de
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

#include "fas.hh"
#include "graph/bidigraph.hh"
#include "tgba/state.hh"
#include "tgba/succiter.hh"
#include "tgba/tgbamask.hh"
#include <algorithm>
#include <climits>
#include <vector>

namespace spot
{
  bool
  fas::operator()(const spot::state* src, const spot::state* dst)
  {
    // States are ordered from 0, ..., n. The transitions form j -> i with
    // j < i belong to the fas.  States that where not encountered are not
    // accessible, don't have an ordering value and therefor cannot be part of
    // the FAS.
    if (ordered_states.find(src) == ordered_states.end())
      return false;
    return ordered_states[src] < ordered_states[dst];
  }

  fas::~fas()
  {
  }

  void
  fas::build()
  {
    // Counter associated with number of spot::state*
    unsigned order = 0;
    scc_.build_map();
    // Number of SCCs in aut_
    unsigned scc_count = scc_.scc_count();
    for (unsigned current_scc = 0; current_scc < scc_count; ++current_scc)
      {
        std::list<const spot::state*> state_list = scc_.states_of(current_scc);
        const state_set to_keep(state_list.begin(), state_list.end());
        // Create SCC component, initial state is any of the SCC
        const spot::tgba* scc_aut = build_tgba_mask_keep(aut_, to_keep,
                                                         state_list.front());
        spot::graph::bidigraph bdg(scc_aut, false, &scc_, current_scc);
        bdg.build();
        worker_build(bdg, order);
        delete scc_aut;
      }
  }

  void
  fas::worker_build(spot::graph::bidigraph& bdg, unsigned& order)
  {
    // Need to push_front, will push_back and iterate backwards
    // s2 contains Sources and deltas.
    std::vector<const spot::state*> s2;
    unsigned counter = bdg.get_bidistates().size();
    while (counter)
      {
        // At each iteration we delete every sinks, update the graph, and check
        // for new sinks.
        spot::graph::bidistate* sink;
        while ((sink = bdg.pop_sink()))
          {
            const spot::state* tgba_sink = bdg.get_tgba_state(sink);
            ordered_states[tgba_sink] = order++;
            bdg.remove_state(sink);
            --counter;
          }
        spot::graph::bidistate* source;
        while ((source = bdg.pop_source()))
          {
            const spot::state* tgba_source = bdg.get_tgba_state(source);
            s2.emplace_back(tgba_source);
            bdg.remove_state(source);
            --counter;
          }
        spot::graph::bidistate* delta = bdg.pop_max_delta();
        if (delta)
          {
            const spot::state* tgba_delta = bdg.get_tgba_state(delta);
            s2.emplace_back(tgba_delta);
            bdg.remove_state(delta);
            --counter;
          }
      }
    for (auto it = s2.rbegin(); it != s2.rend(); ++it)
      ordered_states[*it] = order++;
  }

  fas::fas(const spot::tgba* g)
    : aut_(g), scc_(g)
  {
  }
}
