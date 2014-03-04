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
#include <algorithm>
#include <climits>
#include <vector>

namespace spot
{
  bool
  fas::operator()(const spot::state* src, const spot::state* dst)
  {
    // States are ordered from 0, ..., n. The transitions form j -> i with
    // j > i belong to the fas.
    return ordered_states[src] > ordered_states[dst];
  }

  fas::~fas()
  {
  }

  fas::fas(const spot::tgba* g)
  {
    unsigned order = 0;
    spot::graph::bidigraph bdg(g, false);
    // Need to push_front, will push_back and iterate backwards
    // s2 contains Sources and deltas.
    std::vector<const spot::state*> s2;
    // Used to get every sinks, and sources.
    std::vector<spot::graph::bidistate*> container;
    unsigned counter = bdg.get_bidistates().size();
    while (counter)
      {
        // At each iteration we delete every sinks, update the graph, and check
        // for new sinks.
        std::vector<spot::graph::bidistate*>& sinks = bdg.get_sinks();
        while (!sinks.empty())
          {
            spot::graph::bidistate* sink = sinks.back();
            sinks.pop_back();
            const spot::state* tgba_sink = bdg.get_tgba_state(sink);
            s2.emplace_back(tgba_sink);
            bdg.remove_state(sink);
            --counter;
          }
        std::vector<spot::graph::bidistate*>& sources = bdg.get_sources();
        while (!sources.empty())
          {
            spot::graph::bidistate* source = sources.back();
            sources.pop_back();
            const spot::state* tgba_source = bdg.get_tgba_state(source);
            ordered_states[tgba_source] = order++;
            bdg.remove_state(source);
            --counter;
          }
        graph::bidigraph_states& max_deltas = bdg.get_max_delta();
        if (!max_deltas.empty())
          {
            spot::graph::bidistate* delta = max_deltas.back();
            max_deltas.pop_back();
            const spot::state* tgba_delta = bdg.get_tgba_state(delta);
            ordered_states[tgba_delta] = order++;
            bdg.remove_state(delta);
            --counter;
          }
      }
    for (auto it = s2.rbegin(); it != s2.rend(); ++it)
      ordered_states[*it] = order++;
  }
}
