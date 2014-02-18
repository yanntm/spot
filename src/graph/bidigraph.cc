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
      : states_(new graph_states())
    {
      spot::state* init = g->get_init_state();

      // Each state will appear once in this vector.
      std::vector<spot::state*> states;
      // Create initial state
      {
        states.emplace_back(init);
        create_bidistate(init);
      }
      // N iterations should be made, N = number of vertex.
      while (!states.empty())
        {
          spot::state* current_state = states.back();
          states.pop_back();
          bidistate* current_bidistate = get_bidistate(current_state);
          tgba_succ_iterator* succ = g->succ_iter(current_state);
          for (succ->first(); !succ->done(); succ->next())
            {
              spot::state* succ_state = succ->current_state();
              // Check if succ_state has already been explored.
              if (!exist(succ_state))
                {
                  states.emplace_back(succ_state);
                  create_bidistate(succ_state);
                }
              bidistate* succ_bidistate = get_bidistate(succ_state);
              current_bidistate->add_succ(succ_bidistate);
              succ_bidistate->add_pred(current_bidistate);
            }
          delete succ;
          current_state->destroy();
        }
    }

    bidigraph::~bidigraph()
    {
      // Remove all bidistates of bidigraph with corresponding lists.
      for (std::unordered_map<hash_value,
           std::vector<bidistate*>* >::iterator it =
           tgba_to_bidistate_.begin(); it != tgba_to_bidistate_.end(); ++it)
        {
          assert(it->second);
          std::vector<bidistate*>* bidistate_list = it->second;
          for (std::vector<bidistate*>::iterator bidistate =
               bidistate_list->begin(); bidistate != bidistate_list->end();
               ++bidistate)
            delete *bidistate;
          delete bidistate_list;
        }
      delete states_;

      // Remove every spot::state* associated to the bidistates
      for (std::unordered_map<bidistate*, spot::state*>::iterator it =
           bidistate_to_tgba_.begin(); it != bidistate_to_tgba_.end(); ++it)
        {
          assert(it->second);
          if (cached_tgba_.find(it->second) != cached_tgba_.end())
            continue;
          it->second->destroy();
        }
    }

    spot::state*
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

      std::unordered_map<bidistate*, spot::state*>::iterator tgba_state_it =
        bidistate_to_tgba_.find(s);
      assert(tgba_state_it != bidistate_to_tgba_.end());
      // find vector where s is stored. Delete it from that vector and remove
      // vector from tgba_to_bidistate hash_table if it contains no more
      // elements.
      {
        std::unordered_map<hash_value, std::vector<bidistate*>* >::iterator
          bidistate_list_it =
          tgba_to_bidistate_.find(tgba_state_it->second->hash());
        assert(bidistate_list_it != tgba_to_bidistate_.end());

        std::vector<bidistate*>* tgba_list = bidistate_list_it->second;
        std::vector<bidistate*>::iterator bidistate_it = tgba_list->begin();
        for (; get_tgba_state((*bidistate_it))->compare(tgba_state_it->second)
             && bidistate_it != tgba_list->end(); ++bidistate_it)
          continue;
        assert(bidistate_it != tgba_list->end());
        tgba_list->erase(bidistate_it);
        if (tgba_list->empty())
          {
            tgba_to_bidistate_.erase(bidistate_list_it);
            delete tgba_list;
          }
      }

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
    bidigraph::get_bidistate(spot::state* tgba_state) const
    {
      auto tgba_to_bidistate_it = tgba_to_bidistate_.find(tgba_state->hash());
      auto bidistate_list = tgba_to_bidistate_it->second;
      assert(tgba_to_bidistate_it != tgba_to_bidistate_.end());
      for (auto bds : *bidistate_list)
        {
          // a bidistate always has a corresponding tgba_state
          auto tgba_state = bidistate_to_tgba_.find(bds)->second;
          if (!tgba_state->compare(tgba_state))
            return bds;
        }
      assert(false && "unable to find tgba_state in bidigraph");
      return nullptr;
    }

    spot::state*
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
    bidigraph::exist(spot::state* tgba_state) const
    {
      auto tgba_to_bidistate_it = tgba_to_bidistate_.find(tgba_state->hash());

      if (tgba_to_bidistate_it == tgba_to_bidistate_.end())
        return false;

      auto bidistate_list = tgba_to_bidistate_it->second;
      for (auto bds : *bidistate_list)
        {
          // a bidistate always has a corresponding tgba_state
          auto tgba_state = bidistate_to_tgba_.find(bds)->second;
          if (!tgba_state->compare(tgba_state))
            return true;
        }
      return false;
    }

    void
    bidigraph::create_bidistate(spot::state* tgba_state)
    {
      spot::graph::bidistate* bidistate = new spot::graph::bidistate();
      size_t hash = tgba_state->hash();
      std::vector<spot::graph::bidistate*>* state_list
        = tgba_to_bidistate_[hash];
      if (!state_list)
        {
          state_list = new std::vector<graph::bidistate*>;
          tgba_to_bidistate_[hash] = state_list;
        }
      state_list->emplace_back(bidistate);
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
