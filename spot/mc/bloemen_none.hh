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
#include <spot/misc/common.hh>
#include <spot/kripke/kripke.hh>
#include <spot/misc/fixpool.hh>
#include <spot/misc/timer.hh>
#include <spot/mc/bloemen.hh>

namespace spot
{

  /// \brief This object is returned by the algorithm below
  struct SPOT_API bloemen_none_stats
  {
    unsigned inserted;          ///< \brief Number of states inserted
    unsigned states;            ///< \brief Number of states visited
    unsigned transitions;       ///< \brief Number of transitions visited
    unsigned sccs;              ///< \brief Number of SCCs visited
    unsigned walltime;          ///< \brief Walltime for this thread in ms
  };

  /// \brief This class implements the SCC decomposition algorithm of
  /// bloemen_none as described in PPOPP'16. It uses a shared
  /// union-find augmented to manage work stealing between threads.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_bloemen_none
  {
  public:

    swarmed_bloemen_none(kripkecube<State, SuccIterator>& sys,
                    iterable_uf<State, StateHash, StateEqual>& uf,
                    unsigned tid):
      sys_(sys),  uf_(uf), tid_(tid),
      nb_th_(std::thread::hardware_concurrency())
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    using uf = iterable_uf<State, StateHash, StateEqual>;
    using uf_element = typename uf::uf_element;

    void run()
    {
      tm_.start("DFS thread " + std::to_string(tid_));
      unsigned w_id = (1U << tid_);

      State init = sys_.initial(tid_);
      auto pair = uf_.make_claim(init);
      todo_.emplace_back(pair.second);
      Rp_.emplace_back(pair.second);
      ++states_;

      while (!todo_.empty())
        {
        bloemen_none_recursive_start:
          while (true)
            {
              bool sccfound = false;
              uf_element* v_prime = uf_.pick_from_list(todo_.back(), &sccfound);
              if (v_prime == nullptr)
                {
                  // The SCC has been explored!
                  sccs_ += sccfound;
                  break;
                }

              // ---------------------------------------------------
              // This part is used to reduce the amount of time the
              // reduced set is computed. In other word, it avoid
              // a recomputation of the reduced set every time a
              //  state if picked.
              std::vector<bool>* v = nullptr;
              if (v_prime->ok_reduced_.load())
                v = &v_prime->reduced_;

              auto it = sys_.succ(v_prime->st_, tid_, v);
              if (!v_prime->ok_reduced_.load() &&
                  v_prime->m_reduced_.try_lock())
                {
                  v_prime->reduced_ = it->reduced();
                  v_prime->ok_reduced_ = true;
                  v_prime->m_reduced_.unlock();
                }
              // ---------------------------------------------------

              // This thread is actively working on this state
              atomic_fetch_or(&(v_prime->wip_), w_id);

              while (!it->done())
                {
                  auto w = uf_.make_claim(it->state());
                  it->next();
                  ++transitions_;
                  if (w.first == uf::claim_status::CLAIM_NEW)
                    {
                      todo_.emplace_back(w.second);
                      Rp_.emplace_back(w.second);
                      ++states_;
                      sys_.recycle(it, tid_);
                      // This thread is no longer actively working on this state
                      atomic_fetch_and(&(v_prime->wip_), ~w_id);
                      goto bloemen_none_recursive_start;
                    }
                  else if (w.first == uf::claim_status::CLAIM_FOUND)
                    {

                      while (!uf_.sameset(todo_.back(), w.second))
                        {
                          uf_element* r = Rp_.back();
                          Rp_.pop_back();
                          uf_.unite(r, Rp_.back());
                        }
                    }
                }
              uf_.remove_from_list(v_prime);
              // This thread is no longer actively working on this state
              // FIXME: This is maybe useless for POR, since v_prime is now DONE
              // atomic_fetch_and(&(v_prime->wip_), ~w_id);
              sys_.recycle(it, tid_);
            }

          if (todo_.back() == Rp_.back())
            Rp_.pop_back();
          todo_.pop_back();
        }
      tm_.stop("DFS thread " + std::to_string(tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(tid_)).walltime();
    }

    bloemen_none_stats stats()
    {
      return {uf_.inserted(), states_, transitions_, sccs_, walltime()};
    }

  private:
    kripkecube<State, SuccIterator>& sys_;   ///< \brief The system to check
    std::vector<uf_element*> todo_;          ///< \brief The "recursive" stack
    std::vector<uf_element*> Rp_;            ///< \brief The DFS stack
    iterable_uf<State, StateHash, StateEqual> uf_; ///< Copy!
    unsigned tid_;
    unsigned nb_th_;
    unsigned inserted_ = 0;           ///< \brief Number of states inserted
    unsigned states_  = 0;            ///< \brief Number of states visited
    unsigned transitions_ = 0;        ///< \brief Number of transitions visited
    unsigned sccs_ = 0;               ///< \brief Number of SCC visited
    spot::timer_map tm_;              ///< \brief Time execution
  };
}
