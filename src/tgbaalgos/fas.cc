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
#include <climits>
#include <vector>

namespace spot
{
  namespace
  {
    bool
    has_source(spot::graph::bidigraph& bdg,
               std::vector<spot::graph::bidistate*>& container)
    {
      for (auto bds : bdg.get_bidistates())
        {
          if (bds->get_in_degree() == 0)
            container.emplace_back(bds);
        }
      return !container.empty();
    }

    bool
    has_sinks(spot::graph::bidigraph& bdg,
              std::vector<spot::graph::bidistate*>& container)
    {
      for (auto bds : bdg.get_bidistates())
        {
          if (bds->get_out_degree() == 0)
            container.emplace_back(bds);
        }
      return !container.empty();
    }

    spot::graph::bidistate*
    get_delta(spot::graph::bidigraph& bdg)
    {
      spot::graph::bidistate* res = nullptr;
      unsigned in = 0;
      int delta = INT_MIN;
      for (auto bds : bdg.get_bidistates())
        {
          unsigned new_in = bds->get_in_degree();
          int new_delta = bds->get_out_degree() - new_in;
          // When delta are equal, chosse smaller in_degree.  This diminishes
          // the number or arcs.
          if (new_delta > delta || (new_delta == delta && new_in < in))
            {
              res = bds;
              in = new_in;
              delta = new_delta;
            }
        }
      assert(res);
      return res;
    }
  }

  bool
  fas::operator()(const spot::state* src, const spot::state* dst)
  {
    // States are ordered from 0, ..., n. The transitions form j -> i with
    // j > i belong to the fas.
    return ordered_states[src] > ordered_states[dst];
  }

  fas::~fas()
  {
    for (auto p : ordered_states)
      p.first->destroy();
  }

  fas::fas(const spot::tgba* g)
  {
    unsigned order = 0;
    spot::graph::bidigraph bdg(g);
    // Need to push_front, will push_back and iterate backwards
    // s2 contains Sources and deltas.
    std::vector<const spot::state*> s2;
    // Used to get every sinks, and sources.
    std::vector<spot::graph::bidistate*> container;
    while (!bdg.get_bidistates().empty())
      {
        // At each iteration we delete every sinks, update the graph, and check
        // for new sinks.
        while (has_sinks(bdg, container))
          {
            for (auto sink : container)
              {
                const spot::state* tgba_sink = bdg.get_tgba_state(sink);
                s2.emplace_back(tgba_sink);
                bdg.remove_state(sink);
              }
            container.resize(0);
          }
        while (has_source(bdg, container))
          {
            for (auto source : container)
              {
                const spot::state* tgba_source = bdg.get_tgba_state(source);
                ordered_states[tgba_source] = order++;
                bdg.remove_state(source);
              }
            container.resize(0);
          }
        if (!bdg.get_bidistates().empty())
          {
            spot::graph::bidistate* delta = get_delta(bdg);
            const spot::state* tgba_delta = bdg.get_tgba_state(delta);
            ordered_states[tgba_delta] = order++;
            bdg.remove_state(delta);
          }
      }
    for (auto it = s2.rbegin(); it != s2.rend(); ++it)
      ordered_states[*it] = order++;
  }
}
