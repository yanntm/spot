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
#include <algorithm>
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
      size_t size = out_->size();
      for (; pos < size && (*out_)[pos] != s; ++pos)
        continue;
      // Make sure the transition was found.
      assert(pos < size);

      std::iter_swap(out_->begin() + pos, out_->end() - 1);
      out_->pop_back();
    }

    // Remove the transition coming from s.
    void
    bidistate::remove_pred(const bidistate* s)
    {
      size_t pos = 0;
      size_t size = in_->size();
      for (; pos < size && (*in_)[pos] != s; ++pos)
        continue;
      // Make sure the transition was found.
      assert(pos < size);

      std::iter_swap(in_->begin() + pos, in_->end() - 1);
      in_->pop_back();
    }

    /////////////////////////////////////
    // Gaph implementation
    /////////////////////////////////////

    bidigraph::bidigraph(const tgba* g)
      : tgba_reachable_iterator(g)
    {
      run();
      order_ = states_.size();
      deltas_.resize(2 * order_ - 1);
      max_index_ = 0;
      for (auto bds : states_)
        {
          if (bds->get_out_degree() == 0)
            sinks_.emplace_back(bds);
          else if (bds->get_in_degree() == 0)
            sources_.emplace_back(bds);
          else
            {
              unsigned delta_value = bds->get_out_degree() -
                                     bds->get_in_degree() + order_ - 1;
              deltas_[delta_value].emplace_back(bds);
              if (max_index_ < delta_value)
                max_index_ = delta_value;
            }
        }
    }

    void
    bidigraph::add_state(const state* s)
    {
      create_bidistate(s->clone());
      todo.push_back(s);
    }

    const state*
    bidigraph::next_state()
    {
      if (todo.empty())
        return nullptr;
      const state* s = todo.front();
      todo.pop_front();
      return s;
    }

    void
    bidigraph::process_link(const state* in_s, int,
                            const state* out_s, int,
                            const tgba_succ_iterator*)
    {
      bidistate* src = get_bidistate(in_s);
      bidistate* dst = get_bidistate(out_s);
      src->add_succ(dst);
      dst->add_pred(src);
    }


    bidigraph::~bidigraph()
    {
      // Remove all bidistates of bidigraph
      for (auto bds : states_)
        delete bds;

      // Remove every spot::state* associated to the bidistates
      for (std::unordered_map<bidistate*, const spot::state*>::iterator it =
           bidistate_to_tgba_.begin(); it != bidistate_to_tgba_.end(); ++it)
        {
          assert(it->second);
          it->second->destroy();
        }
    }

    void
    bidigraph::move_delta(bidistate* s, int step)
    {
      size_t pos = 0;
      unsigned delta_value = s->get_out_degree() - s->get_in_degree() + order_ -
                             1 + step;
      size_t size = deltas_[delta_value].size();
      std::vector<bidistate*>& delta_class = deltas_[delta_value];
      for (; pos < size && delta_class[pos] != s; ++pos)
        continue;
      // Make sure the transition was found.
      assert(pos < delta_class.size());
      std::iter_swap(delta_class.begin() + pos, delta_class.end() - 1);
      delta_class.pop_back();
    }

    void
    bidigraph::remove_state(bidistate* s)
    {
      // Remove transitions to s.
      bidigraph_states* pred = s->get_pred();
      for (bidigraph_states::iterator it = pred->begin();
           it != pred->end(); ++it)
        {
          (*it)->remove_succ(s);
          // *it was either source or delta
          if ((*it)->get_out_degree() == 0)
            {
              // Check if *it was a delta.
              if ((*it)->get_in_degree())
                {
                  sinks_.emplace_back(*it);
                  move_delta(*it, 1);
                }
            }
          // Move to lower delta class
          else
            {
              move_delta(*it, 1);
              int delta_value = (*it)->get_out_degree() - (*it)->get_in_degree()
                                + order_ - 1;
              deltas_[delta_value].emplace_back(*it);
            }
        }

      // Remove transitions from s.
      bidigraph_states* succ = s->get_succ();
      for (bidigraph_states::iterator it = succ->begin();
           it != succ->end(); ++it)
        {
          (*it)->remove_pred(s);
          // *it was either sink or delta
          if ((*it)->get_in_degree() == 0)
            {
              // Check if *it was a delta.
              if ((*it)->get_out_degree())
                {
                  sources_.emplace_back(*it);
                  move_delta(*it, -1);
                }
            }
          else
            {
              move_delta(*it, -1);
              unsigned delta_value = (*it)->get_out_degree() -
                                     (*it)->get_in_degree() + order_ - 1;
              deltas_[delta_value].emplace_back(*it);
              if (delta_value > max_index_)
                max_index_ = delta_value;
            }
        }

      // Find position of s in vector and remove it.
      size_t pos = 0;
      for (; pos < states_.size() && states_[pos] != s; ++pos)
        continue;
      assert(pos < states_.size());
      std::iter_swap(states_.begin() + pos, states_.end() - 1);
      states_.pop_back();

      // remove state from maps.
      std::unordered_map<bidistate*, const spot::state*>::iterator
        tgba_state_it = bidistate_to_tgba_.find(s);
      assert(tgba_state_it != bidistate_to_tgba_.end());

      tgba_to_bidistate_.erase(tgba_state_it->second);

      // destroy spot::state
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

    bidigraph_states&
    bidigraph::get_sources()
    {
      return sources_;
    }

    bidigraph_states&
    bidigraph::get_sinks()
    {
      return sinks_;
    }

    bidigraph_states&
    bidigraph::get_max_delta()
    {
      while (deltas_[max_index_].empty() && max_index_)
        --max_index_;
      return deltas_[max_index_];
    }

    const graph::bidigraph_states&
    bidigraph::get_bidistates() const
    {
      return states_;
    }

    void
    bidigraph::create_bidistate(const spot::state* tgba_state)
    {
      spot::graph::bidistate* bidistate = new spot::graph::bidistate();
      tgba_to_bidistate_[tgba_state] = bidistate;
      bidistate_to_tgba_[bidistate] = tgba_state;
      // Add bidistate to list of bidistates of bidgraph
      states_.emplace_back(bidistate);
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
