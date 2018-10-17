// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016, 2017, 2018 Laboratoire de Recherche et
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
#include <spot/mc/cond_dest.hh>
#include <spot/misc/common.hh>
#include <spot/kripke/kripke.hh>
#include <spot/misc/fixpool.hh>
#include <spot/misc/timer.hh>

namespace spot
{

  /// \brief This object is returned by the algorithm below
  struct SPOT_API laarman_source_stats
  {
    unsigned inserted;          ///< \brief Number of states inserted
    unsigned states;            ///< \brief Number of states visited
    unsigned transitions;       ///< \brief Number of transitions visited
    unsigned sccs;              ///< \brief Number of SCCs visited
    unsigned walltime;          ///< \brief Walltime for this thread in ms
  };

  /// \brief This class implements a swarmed laarman_source as described in
  // ATVA'16. It uses a shared store to share information between threads.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_laarman_source
  {
  public:

    swarmed_laarman_source(kripkecube<State, SuccIterator>& sys,
                    store<State, StateHash, StateEqual>& store,
                    unsigned tid):
      sys_(sys),  store_(store), tid_(tid),
      nb_th_(std::thread::hardware_concurrency())
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    using st_store = store<State, StateHash, StateEqual>;
    using store_element = typename st_store::store_element;

    void run()
    {
      tm_.start("DFS thread " + std::to_string(tid_));
      unsigned w_id = (1U << tid_);


      // Insert the initial state
      {
        State init = sys_.initial(tid_);
        auto pair = store_.make_claim(init);
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
        todo_.push_back({pair.second, it, false});
        Rp_.emplace_back(pair.second);
        ++states_;
      }

      while (!todo_.empty())
        {
          if (todo_.back().e->expanded_)
            todo_.back().it->fireall();
          else if (todo_.back().it->naturally_expanded())
            todo_.back().e->expanded_.store(true);

          if (todo_.back().it->done())
            {
              if (todo_.back().cyan_succ &&
                  !todo_.back().e->expanded_ &&
                  !todo_.back().e->not_to_expand_)
                {
                  // Systematic expansion
                  todo_.back().e->expanded_.store(true);
                  todo_.back().it->fireall();
                  continue;
                }

              if (!todo_.back().e->expanded_)
                {
                  todo_.back().e->not_to_expand_.store(true);
                }

              // Useless ? This test is always true?
              if (todo_.back().e == Rp_.back())
                {
                  Rp_.pop_back();
                  store_.make_dead(todo_.back().e);
                }
              // The state is no longer on thread's stack
              atomic_fetch_and(&(todo_.back().e->onstack_), ~w_id);
              sys_.recycle(todo_.back().it, tid_);
              todo_.pop_back();
            }
          else
            {
              ++transitions_;
              State dst = todo_.back().it->state();
              auto w = store_.make_claim(dst);

              // Move forward iterators...
              todo_.back().it->next();

              if (w.first == st_store::claim_status::CLAIM_NEW)
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
                  todo_.push_back({w.second, it, false});
                  Rp_.emplace_back(w.second);
                  ++states_;
                }
              else if (w.first == st_store::claim_status::CLAIM_FOUND)
                {
                  // Collect if we have one cyan successor
                  if (w.second->onstack_.load() & w_id)
                    todo_.back().cyan_succ = true;
                }
            }
        }
      tm_.stop("DFS thread " + std::to_string(tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(tid_)).walltime();
    }

    laarman_source_stats stats()
    {
      return {store_.inserted(), states_, transitions_, sccs_, walltime()};
    }

  private:
    struct todo_element
    {
      store_element* e;
      SuccIterator* it;
      bool cyan_succ;
    };

    kripkecube<State, SuccIterator>& sys_;   ///< \brief The system to check
    std::vector<todo_element> todo_;          ///< \brief The "recursive" stack
    std::vector<store_element*> Rp_;            ///< \brief The DFS stack
    st_store store_; ///< Copy!
    unsigned tid_;
    unsigned nb_th_;
    unsigned inserted_ = 0;           ///< \brief Number of states inserted
    unsigned states_  = 0;            ///< \brief Number of states visited
    unsigned transitions_ = 0;        ///< \brief Number of transitions visited
    unsigned sccs_ = 0;               ///< \brief Number of SCC visited
    spot::timer_map tm_;              ///< \brief Time execution
  };
}
