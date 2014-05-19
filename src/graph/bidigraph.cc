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
    /////////////////////////////////////
    // Gaph implementation
    /////////////////////////////////////

    bidigraph::bidigraph(const tgba* g, bool do_loops)
      : tgba_reachable_iterator(g),
        do_loops_(do_loops),
        max_index_(0),
        max_out_(0),
        max_in_(0),
        scc_index_(-1),
        scc_(nullptr)
    {
    }

    bidigraph::bidigraph(const tgba* g, bool do_loops, spot::scc_map* scc,
                         int scc_index)
        : tgba_reachable_iterator(g),
        do_loops_(do_loops),
        max_index_(0),
        max_out_(0),
        max_in_(0),
        scc_index_(scc_index),
        scc_(scc)
    {
      std::unordered_map<int, unsigned> nb_sub_tr;
      bdd ap = scc_->ap_set_of(scc_index_);
      for (bdd cond : scc_->cond_set_of(scc_index_))
        {
          unsigned count = 0;
          int bdd_id = cond.id();
          while (cond != bddfalse)
            {
              cond -= bdd_satoneset(cond, ap, bddtrue);
              ++count;
            }
          nb_sub_tr_[bdd_id] = count;
        }
    }

    void
    bidigraph::build()
    {
      run();
      // Compute max_out_ and max_in_ degree for every node of graph
      {
        for (auto bds : states_)
          {
            if (bds->get_out_degree() > max_out_)
              max_out_ = bds->get_out_degree();
            if (bds->get_in_degree() > max_in_)
              max_in_ = bds->get_in_degree();
          }
      }
      deltas_.resize(max_in_ + max_out_ + 1, nullptr);
      // add nodes to their delta class
      for (auto bds : states_)
        {
          if (bds->get_out_degree() == 0)
            add_delta(0, bds);
          else if (bds->get_in_degree() == 0)
            add_delta(max_in_ + max_out_, bds);
          else
            {
              unsigned delta_value = bds->get_out_degree() -
                                     bds->get_in_degree() + max_in_;
              add_delta(delta_value, bds);
              if (max_index_ < delta_value)
                max_index_ = delta_value;
            }
        }
    }

    void
    bidigraph::add_delta(unsigned delta_class, bidistate* bds)
    {
      // Adding bds at head of list
      bds->prev_delta = nullptr;
      bidistate* head = deltas_[delta_class];
      if (!head)
        {
          deltas_[delta_class] = bds;
          bds->next_delta = nullptr;
        }
      else
        {
          head->prev_delta = bds;
          bds->next_delta = head;
          deltas_[delta_class] = bds;
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
    bidigraph::process_link(const state* in_s, int in,
                            const state* out_s, int out,
                            const tgba_succ_iterator* it)
    {
      bidistate* src = get_bidistate(in_s);
      bidistate* dst = get_bidistate(out_s);
      // Ignore loops.
      if (do_loops_ || in != out)
        {
          bdd cond = it->current_condition();
          unsigned count = scc_index_ == -1 ? 1 : nb_sub_tr_[cond.id()];
          // add arc for each sub transition if an scc_ was given
          for (unsigned i = 0; i < count; ++i)
            {
              src->add_succ(dst);
              dst->add_pred(src);
            }
        }
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

    // Remove bidistate from one of the delta_clases, these are deleted after
    // being updated, so we take in consideration \a step with values +1, or -1
    // depending on what kind of transition was removed.
    void
    bidigraph::remove_delta(bidistate* s, int step)
    {
      assert(s->next_delta != s);
      if (s->next_delta)
        s->next_delta->prev_delta = s->prev_delta;

      if (s->prev_delta)
        s->prev_delta->next_delta = s->next_delta;
      else
        {
          unsigned delta_value = s->get_out_degree() - s->get_in_degree() +
                                 max_in_ + step;
          assert(deltas_[delta_value] == s);
          deltas_[delta_value] = s->next_delta;
        }
    }

    void
    bidigraph::update_succs(bidigraph_states* succ)
    {
      for (bidigraph_states::iterator it = succ->begin();
           it != succ->end(); ++it)
        {
          // ignore deleted states
          if (!(*it)->is_alive())
            continue;

          (*it)->remove_pred();

          // *it could have become a sink from the removal of s's successors.
          if ((*it)->get_out_degree() == 0)
            continue;

          // *it is now either a source or a delta of upper class.
          else if ((*it)->get_in_degree() == 0)
            {
              remove_delta(*it, -1);
              add_delta(max_in_ + max_out_, *it);
            }
          // Move to upper class.
          else
            {
              remove_delta(*it, -1);
              unsigned delta_value = (*it)->get_out_degree() -
                                     (*it)->get_in_degree() + max_in_;
              add_delta(delta_value, *it);
              // Get next maximal delta index.
              if (delta_value > max_index_)
                max_index_ = delta_value;
            }
        }
    }

    void
    bidigraph::update_preds(bidigraph_states* pred)
    {
      for (bidigraph_states::iterator it = pred->begin();
           it != pred->end(); ++it)
        {
          // ignore deleted states
          if (!(*it)->is_alive())
            continue;

          (*it)->remove_succ();

          // *it could have become a source from previous loup.
          if ((*it)->get_in_degree() == 0)
            continue;

          // *it is now either a sink or a delta of lower class.
          else if ((*it)->get_out_degree() == 0)
            {
              remove_delta(*it, 1);
              add_delta(0, *it);
            }
          // Move to lower delta class
          else
            {
              remove_delta(*it, 1);
              unsigned delta_value = (*it)->get_out_degree() -
                                     (*it)->get_in_degree() + max_in_;
              add_delta(delta_value, *it);
            }
        }
    }

    void
    bidigraph::remove_state(bidistate* s)
    {
      s->set_is_alive(false);
      update_preds(s->get_pred());
      update_succs(s->get_succ());

      // remove state from maps.
      std::unordered_map<bidistate*, const spot::state*>::iterator
        tgba_state_it = bidistate_to_tgba_.find(s);
      assert(tgba_state_it != bidistate_to_tgba_.end());

      tgba_to_bidistate_.erase(tgba_state_it->second);

      // destroy spot::state
      tgba_state_it->second->destroy();
      bidistate_to_tgba_.erase(tgba_state_it);
    }

    // From a tgba state's hash, get a list of state*. To get the
    // correct state*, iterate through a list of state*.
    // For each of these bidistates, get the corresponding tgba_state and
    // compare it to \a tgba_state.
    bidistate*
    bidigraph::get_bidistate(const spot::state* tgba_state) const
    {
      // Correpsonding bidistate always exists
      return tgba_to_bidistate_.find(tgba_state)->second;
    }

    const spot::state*
    bidigraph::get_tgba_state(bidistate* bds) const
    {
      // Corresponding state always exists
      return bidistate_to_tgba_.find(bds)->second;
    }

    bidistate*
    bidigraph::pop_source()
    {
      return pop(max_in_ + max_out_);
    }

    bidistate*
    bidigraph::pop(unsigned delta_class)
    {
      bidistate* res = deltas_[delta_class];
      if (res)
        {
          deltas_[delta_class] = res->next_delta;
          if (deltas_[delta_class])
            deltas_[delta_class]->prev_delta = nullptr;
        }
      return res;
    }

    bidistate*
    bidigraph::pop_sink()
    {
      return pop(0);
    }

    bidistate*
    bidigraph::pop_max_delta()
    {
      while (!deltas_[max_index_] && max_index_)
        --max_index_;
      // if max_index_ == 0, it's ok.  It means that there are only sinks and
      // sources left and we are about to remove a sink.
      return pop(max_index_);
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
