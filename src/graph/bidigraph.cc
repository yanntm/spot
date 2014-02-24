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

#include "graph/bidigraph.hh"
#include "tgba/succiter.hh"
#include "tgba/tgba.hh"
#include <unordered_map>
#include <vector>

namespace spot
{
  namespace graph
  {

    void
    bidistate::remove_succ(const bidistate* s)
    {
      size_t pos = 0;
      for (; pos < out_->size() && (*out_)[pos] != s; ++pos)
        continue;
      // Make sure the transition was found.
      assert(pos < out_->size());

      out_->erase(out_->begin() + pos);
    }

    // Remove the transition coming from s.
    void
    bidistate::remove_pred(const bidistate* s)
    {
      size_t pos = 0;
      for (; pos < in_->size() && (*in_)[pos] != s; ++pos)
        continue;
      // Make sure the transition was found.
      assert(pos < in_->size());

      in_->erase(in_->begin() + pos);
    }

/////////////////////////////////////
// Gaph implementation
/////////////////////////////////////

    bidigraph::bidigraph(const tgba* g)
      : tgba_reachable_iterator_breadth_first(g), states_(new graph_states())
    {
      run();
    }

    void
    bidigraph::process_state(const state* s, int, tgba_succ_iterator*)
    {
      if (!exist(s))
        create_bidistate(s);
    }

    void
    bidigraph::process_link(const state* in_s, int,
                            const state* out_s, int,
                            const tgba_succ_iterator*)
    {
      if (!exist(in_s))
        create_bidistate(in_s);
      bidistate* src = get_bidistate(in_s);
      if (!exist(out_s))
        create_bidistate(out_s);
      bidistate* dst = get_bidistate(out_s);
      src->add_succ(dst);
      dst->add_pred(src);
    }


    bidigraph::~bidigraph()
    {
      // Remove all bidistates of bidigraph
      for (auto bds : *states_)
        delete bds;
      delete states_;

      // Remove every non cached spot::state* associated to the bidistates
      for (std::unordered_map<bidistate*, const spot::state*>::iterator it =
           bidistate_to_tgba_.begin(); it != bidistate_to_tgba_.end(); ++it)
        {
          assert(it->second);
          if (cached_tgba_.find(it->second) != cached_tgba_.end())
            continue;
          it->second->destroy();
        }
    }

    const spot::state*
    bidigraph::cache_tgba_state(bidistate* bidistate)
    {
      auto res = get_tgba_state(bidistate);
      cached_tgba_.emplace(res);
      return res;
    }

    void
    bidigraph::remove_state(bidistate* s)
    {
      // Remove transitions to s.
      graph_states* pred = s->get_pred();
      for (graph_states::iterator it = pred->begin();
           it != pred->end(); ++it)
        (*it)->remove_succ(s);

      // Remove transitions from s.
      graph_states* succ = s->get_succ();
      for (graph_states::iterator it = succ->begin();
           it != succ->end(); ++it)
        (*it)->remove_pred(s);

      // Find position of s in vector and remove it.
      size_t pos = 0;
      for (; pos < states_->size() && (*states_)[pos] != s; ++pos)
        continue;
      assert(pos < states_->size());
      states_->erase(states_->begin() + pos);

      std::unordered_map<bidistate*, const spot::state*>::iterator
        tgba_state_it = bidistate_to_tgba_.find(s);
      assert(tgba_state_it != bidistate_to_tgba_.end());

      tgba_to_bidistate_.erase(tgba_state_it->second);

      // If not cached destroy spot::state
      if (cached_tgba_.find(tgba_state_it->second) == cached_tgba_.end())
        tgba_state_it->second->destroy();
      bidistate_to_tgba_.erase(tgba_state_it);
      delete s;
    }

    // From a tgba state's hash, get a list of state*. To get the
    // correct state*, iterate through a list of state*.
    // For each of these bidistates, get the corresponding tgba_state and
    // compare it to \a tgba_state.
    bidistate*
    bidigraph::get_bidistate(const spot::state* tgba_state) const
    {
      return tgba_to_bidistate_.find(tgba_state)->second;
    }

    const spot::state*
    bidigraph::get_tgba_state(bidistate* bds) const
    {
      return bidistate_to_tgba_.find(bds)->second;
    }

    bidigraph::graph_states&
    bidigraph::get_bidistates() const
    {
      return *states_;
    }

    bool
    bidigraph::exist(const spot::state* tgba_state) const
    {
      auto tgba_to_bidistate_it = tgba_to_bidistate_.find(tgba_state);

      return tgba_to_bidistate_it != tgba_to_bidistate_.end();
    }

    void
    bidigraph::create_bidistate(const spot::state* tgba_state)
    {
      spot::graph::bidistate* bidistate = new spot::graph::bidistate();
      tgba_to_bidistate_[tgba_state] = bidistate;
      bidistate_to_tgba_[bidistate] = tgba_state;
      // Add bidistate to list of bidistates of bidgraph
      states_->emplace_back(bidistate);
    }

    std::ostream& operator<<(std::ostream& os, const bidigraph& bdg)
    {
      os << "digraph G {" << std::endl;
      std::unordered_map<spot::graph::bidistate*, unsigned> bds_id;
      unsigned id = 1;
      for (auto bidistate : bdg.get_bidistates())
        {
          if (bds_id.find(bidistate) == bds_id.end())
            bds_id[bidistate] = id++;
          for (auto succ : *(bidistate->get_succ()))
            {
              if (bds_id.find(succ) == bds_id.end())
                bds_id[succ] = id++;
              os << "  " << bds_id[bidistate] << " -> " << bds_id[succ]
                 << std::endl;
            }
        }
      os << '}' << std::endl;
      return os;
    }
  } // graph namespace
} // spot namespace
