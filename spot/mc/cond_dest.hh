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

namespace spot
{
  template<typename State,
           typename StateHash,
           typename StateEqual>
  class store
  {
  public:
    enum class st_status  { LIVE, DEAD };
    enum class claim_status  { CLAIM_FOUND, CLAIM_NEW, CLAIM_DEAD };

    /// \brief Represents a Union-Find element
    struct store_element
    {
      /// \brief the state handled by the element
      State st_;
      /// The set of worker for a given state
      std::atomic<unsigned> worker_;
      /// The set of worker for which this state is on DFS
      std::atomic<unsigned> onstack_;
      /// \brief current status for the element
      std::atomic<st_status> st_status_;
      /// \brief The state has been expanded by some thread
      std::atomic<bool> expanded_;
      /// \brief the reduced set has been computed by some thread
      std::atomic<bool> ok_reduced_;
      /// \brief the mutex for updating the reduced set
      std::mutex m_reduced_;
      /// \brief the shared reduced set.
      std::vector<bool> reduced_;

    };

    /// \brief The haser for the previous store_element.
    struct store_element_hasher
    {
      store_element_hasher(const store_element*)
      { }

      store_element_hasher() = default;

      brick::hash::hash128_t
      hash(const store_element* lhs) const
      {
        StateHash hash;
        // Not modulo 31 according to brick::hashset specifications.
        unsigned u = hash(lhs->st_) % (1<<30);
        return {u, u};
      }

      bool equal(const store_element* lhs,
                 const store_element* rhs) const
      {
        StateEqual equal;
        return equal(lhs->st_, rhs->st_);
      }
    };

    ///< \brief Shortcut to ease shared map manipulation
    using shared_map = brick::hashset::FastConcurrent <store_element*,
                                                       store_element_hasher>;


    store(shared_map& map, unsigned tid):
      map_(map), tid_(tid),
      size_(std::thread::hardware_concurrency()),
      nb_th_(std::thread::hardware_concurrency()), inserted_(0)
    {
    }

    ~store() {}

    std::pair<claim_status, store_element*>
    make_claim(State a)
    {
      unsigned w_id = (1U << tid_);

      // Setup and try to insert the new state in the shared map.
      store_element* v = new store_element();
      v->st_ = a;
      v->worker_ = 0;
      v->st_status_ = st_status::LIVE;
      v->expanded_ = false;

      auto it = map_.insert({v});
      bool b = it.isnew();

      // Insertion failed, delete element
      // FIXME Should we add a local cache to avoid useless allocations?
      if (!b)
        delete v;
      else
        ++inserted_;

      if ((*it)->st_status_.load() == st_status::DEAD)
        return {claim_status::CLAIM_DEAD, *it};

      if (((*it)->worker_.load() & w_id) != 0)
        return {claim_status::CLAIM_FOUND, *it};

      atomic_fetch_or(&((*it)->onstack_), w_id);
      atomic_fetch_or(&((*it)->worker_), w_id);

      return {claim_status::CLAIM_NEW, *it};
    }

    void make_dead(store_element* a)
    {
      a->st_status_.store(st_status::DEAD);
    }

    unsigned inserted()
    {
      return inserted_;
    }

  private:
    shared_map map_;      ///< \brief Map shared by threads copy!
    unsigned tid_;        ///< \brief The Id of the current thread
    unsigned size_;       ///< \brief Maximum number of thread
    unsigned nb_th_;      ///< \brief Current number of threads
    unsigned inserted_;   ///< \brief The number of insert succes
  };

  /// \brief This object is returned by the algorithm below
  struct SPOT_API cond_dest_stats
  {
    unsigned inserted;          ///< \brief Number of states inserted
    unsigned states;            ///< \brief Number of states visited
    unsigned transitions;       ///< \brief Number of transitions visited
    unsigned sccs;              ///< \brief Number of SCCs visited
    unsigned walltime;          ///< \brief Walltime for this thread in ms
  };

  /// \brief This class implements a swarmed cond_dest as described in
  // ATVA'16. It uses a shared store to share information between threads.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_cond_dest
  {
  public:

    swarmed_cond_dest(kripkecube<State, SuccIterator>& sys,
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
        todo_.push_back({pair.second, it});
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
                  todo_.push_back({w.second, it});
                  Rp_.emplace_back(w.second);
                  ++states_;
                }
              else if (w.first == st_store::claim_status::CLAIM_FOUND)
                {
                  // An expansion is required
                  if ((w.second->onstack_.load() & w_id) &&
                      !todo_.back().e->expanded_.load())
                    w.second->expanded_.store(true);
                }
            }
        }
      tm_.stop("DFS thread " + std::to_string(tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(tid_)).walltime();
    }

    cond_dest_stats stats()
    {
      return {store_.inserted(), states_, transitions_, sccs_, walltime()};
    }

  private:
    struct todo_element
    {
      store_element* e;
      SuccIterator* it;
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
