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
    spot::graph::bidistate*
    has_source(spot::graph::bidigraph& bdg)
    {
      for (auto bds : bdg.get_bidistates())
        {
          if (bds->get_in_degree() == 0)
            return bds;
        }
      return nullptr;
    }

    spot::graph::bidistate*
    has_sinks(spot::graph::bidigraph& bdg)
    {
      for (auto bds : bdg.get_bidistates())
        {
          if (bds->get_out_degree() == 0)
            return bds;
        }
      return nullptr;
    }

    spot::graph::bidistate*
    get_delta(spot::graph::bidigraph& bdg)
    {
      spot::graph::bidistate* res = nullptr;
      unsigned in;
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

    void
    add_ordered_state(fas_order* res, spot::state* s, unsigned order)
    {
      auto& vect = (*res)[s->hash()];
      vect.emplace_back(s, order);
    }

  }

  unsigned
  get_state_order(fas_order* res, const spot::state* s)
  {
    auto& vect = (*res)[s->hash()];
    for (auto p : vect)
      {
        if (!s->compare(p.first))
          return p.second;
      }
    assert(false);
    return 0;

  }

  void
  destroy_fas_order(fas_order* ordered_states)
  {
    for (auto p : *ordered_states)
      for (auto s : p.second)
        s.first->destroy();
    delete ordered_states;
  }

  fas_order*
  fas(const spot::tgba* g)
  {
    unsigned order = 0;
    // Ideally we shold use the hash function of spot::state
    fas_order* res = new fas_order();
    spot::graph::bidigraph bdg(g);
    // Need to push_front, will push_back and iterate backwards
    std::vector<spot::state*> s2;
    while (!bdg.get_bidistates().empty())
      {
        spot::graph::bidistate* sink;
        // At each iteration we delete a sink and update the graph.
        while ((sink = has_sinks(bdg)))
          {
            spot::state* tgba_sink = bdg.get_tgba_state(sink);
            s2.emplace_back(tgba_sink);
            bdg.cache_tgba_state(sink);
            bdg.remove_state(sink);
          }
        spot::graph::bidistate* source;
        while ((source = has_source(bdg)))
          {
            spot::state* tgba_source = bdg.get_tgba_state(source);
            add_ordered_state(res, tgba_source, order++);
            bdg.cache_tgba_state(source);
            bdg.remove_state(source);
          }
        if (!bdg.get_bidistates().empty())
          {
            spot::graph::bidistate* delta = get_delta(bdg);
            spot::state* tgba_delta = bdg.get_tgba_state(delta);
            add_ordered_state(res, tgba_delta, order++);
            bdg.cache_tgba_state(delta);
            bdg.remove_state(delta);
          }
      }
    for (auto it = s2.rbegin(); it != s2.rend(); ++it)
      add_ordered_state(res, *it, order++);
    return res;
  }
}
