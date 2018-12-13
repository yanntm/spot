// -*- coding: utf-8 -*-
// Copyright (C) 2018 Laboratoire de Recherche et
// Developpement de l'Epita
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

#pragma once

#include <atomic>
#include <chrono>
#include <spot/bricks/brick-hashset>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <utility>
#include <spot/mc/renault_cond_dest.hh>
#include <spot/misc/common.hh>
#include <spot/kripke/kripke.hh>
#include <spot/misc/fixpool.hh>
#include <spot/misc/timer.hh>

namespace spot
{

  /// \brief This object is returned by the algorithm below
  struct SPOT_API rsscc_none_stats
  {
    unsigned inserted;          ///< \brief Number of states inserted
    unsigned states;            ///< \brief Number of states visited
    unsigned transitions;       ///< \brief Number of transitions visited
    unsigned sccs;              ///< \brief Number of SCCs visited
    unsigned walltime;          ///< \brief Walltime for this thread in ms
  };

  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_rsscc_none
  {
  public:

    swarmed_rsscc_none(kripkecube<State, SuccIterator>& sys,
                    uf<State, StateHash, StateEqual>& uf,
                    unsigned tid):
      sys_(sys),  uf_(uf), tid_(tid),
      nb_th_(std::thread::hardware_concurrency())
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    using st_uf = uf<State, StateHash, StateEqual>;
    using uf_element = typename st_uf::uf_element;

    void run()
    {
      tm_.start("DFS thread " + std::to_string(tid_));
      unsigned w_id = (1U << tid_);


      // Insert the initial state
      {
        State init = sys_.initial(tid_);
        auto pair = uf_.make_claim(init);
        std::vector<bool>* v = nullptr;

        if (pair.second->ok_reduced_.load())
          v = &pair.second->reduced_;

        auto it = sys_.succ(pair.second->st_, tid_, v);
        if (!pair.second->ok_reduced_.load() &&
            pair.second->m_reduced_.try_lock())
          {
            pair.second->reduced_ = it->reduced();
            pair.second->ok_reduced_ = true;
            pair.second->m_reduced_.unlock();
          }
        unsigned p = livenum_.size();
        livenum_.insert({pair.second, p});
        Rp_.push_back({pair.second, todo_.size()});
        todo_.push_back({pair.second, it, p, true});
        ++states_;
      }

      while (!todo_.empty())
        {
          if (todo_.back().it->done())
            {
              // The state is no longer on thread's stack
              atomic_fetch_and(&(todo_.back().e->onstack_), ~w_id);
              sys_.recycle(todo_.back().it, tid_);

              // Effectivelly backtrack the root
              uf_element* s = todo_.back().e;

              bool isterm = todo_.back().isterm;
              todo_.pop_back();
              if (todo_.size() == Rp_.back().second)
                {
                  if (todo_.size())
                    todo_.back().isterm &= isterm;

                  bool sccfound = false;
                  Rp_.pop_back();
                  uf_.make_dead(s, &sccfound);
                  sccs_ += sccfound;
                }
            }
          else
            {
              ++transitions_;
              State dst = todo_.back().it->state();
              auto w = uf_.make_claim(dst);

              // Move forward iterators...
              todo_.back().it->next();

              if (w.first == st_uf::claim_status::CLAIM_NEW)
                {
                  // ... and insert the new state
                  std::vector<bool>* v = nullptr;
                  if (w.second->ok_reduced_.load())
                    v = &w.second->reduced_;

                  auto it = sys_.succ(w.second->st_, tid_, v);
                  if (!w.second->ok_reduced_.load() &&
                      w.second->m_reduced_.try_lock())
                    {
                      w.second->reduced_ = it->reduced();
                      w.second->ok_reduced_ = true;
                      w.second->m_reduced_.unlock();
                    }
                  unsigned p = livenum_.size();
                  livenum_.insert({w.second, p});
                  Rp_.push_back({w.second, todo_.size()});
                  todo_.push_back({w.second, it, p, true});

                  ++states_;
                }
              else if (w.first == st_uf::claim_status::CLAIM_FOUND)
                {
                  unsigned dpos = livenum_[w.second];
                  unsigned r = Rp_.back().second;
                  while (dpos < todo_[r].pos)
                    {
                      uf_.unite(w.second, todo_[r].e);
                      Rp_.pop_back();
                      r = Rp_.back().second;
                    }
                }
              else if (w.first == st_uf::claim_status::CLAIM_DEAD)
                {
                  todo_.back().isterm = false;
                }
            }
        }
      tm_.stop("DFS thread " + std::to_string(tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(tid_)).walltime();
    }

    rsscc_none_stats stats()
    {
      return {uf_.inserted(), states_, transitions_, sccs_, walltime()};
    }

  private:
    struct todo_element
    {
      uf_element* e;
      SuccIterator* it;
      unsigned pos;
      bool isterm;
    };



    kripkecube<State, SuccIterator>& sys_;   ///< \brief The system to check
    std::vector<todo_element> todo_;          ///< \brief The "recursive" stack
    std::vector<std::pair<uf_element*,
                          unsigned>> Rp_;  ///< \brief The root stack
    st_uf uf_; ///< Copy!
    unsigned tid_;
    unsigned nb_th_;
    unsigned inserted_ = 0;           ///< \brief Number of states inserted
    unsigned states_  = 0;            ///< \brief Number of states visited
    unsigned transitions_ = 0;        ///< \brief Number of transitions visited
    unsigned sccs_ = 0;               ///< \brief Number of SCC visited
    spot::timer_map tm_;              ///< \brief Time execution
    std::unordered_map<uf_element*, unsigned> livenum_;
  };
}
