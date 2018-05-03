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
#include <bricks/brick-hashset>
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
  class uf
  {

  public:
    enum class uf_status  { LIVE, LOCK, DEAD };
    enum class list_status  { BUSY, LOCK, DONE };
    enum class claim_status  { CLAIM_FOUND, CLAIM_NEW, CLAIM_DEAD };

    /// \brief Represents a Union-Find element
    struct uf_element
    {
      /// \brief the state handled by the element
      State st_;
      /// \brief reference to the pointer
      std::atomic<uf_element*> parent;
      /// The set of worker for a given state
      std::atomic<unsigned> worker_;
      /// The set of worker for which this state is on DFS
      std::atomic<unsigned> onstack_;

      /// The local expanded view
      std::atomic<unsigned> expanded_local_;

      /// \brief next element for work stealing
      std::atomic<uf_element*> next_;
      /// \brief current status for the element
      std::atomic<uf_status> uf_status_;
      /// \brief current status for the list
      std::atomic<list_status> list_status_;
      /// \brief The set of active worker for a state
      std::atomic<unsigned> wip_;
      /// \brief The state has been expanded by some thread
      std::atomic<bool> expanded_;
      /// \brief the reduced set has been computed by some thread
      std::atomic<bool> ok_reduced_;
      /// \brief the mutex for updating the reduced set
      std::mutex m_reduced_;
      /// \brief the shared reduced set.
      std::vector<bool> reduced_;

    };

    /// \brief The haser for the previous uf_element.
    struct uf_element_hasher
    {
      uf_element_hasher(const uf_element*)
      { }

      uf_element_hasher() = default;

      brick::hash::hash128_t
      hash(const uf_element* lhs) const
      {
        StateHash hash;
        // Not modulo 31 according to brick::hashset specifications.
        unsigned u = hash(lhs->st_) % (1<<30);
        return {u, u};
      }

      bool equal(const uf_element* lhs,
                 const uf_element* rhs) const
      {
        StateEqual equal;
        return equal(lhs->st_, rhs->st_);
      }
    };

    ///< \brief Shortcut to ease shared map manipulation
    using shared_map = brick::hashset::FastConcurrent <uf_element*,
                                                       uf_element_hasher>;


    uf(shared_map& map, unsigned tid):
      map_(map), tid_(tid),
      size_(std::thread::hardware_concurrency()),
      nb_th_(std::thread::hardware_concurrency()), inserted_(0)
    {
    }

    ~uf() {}

    std::pair<claim_status, uf_element*>
    make_claim(State a)
    {
      unsigned w_id = (1U << tid_);

      // Setup and try to insert the new state in the shared map.
      uf_element* v = new uf_element();
      v->st_ = a;
      v->parent = v;
      v->next_ = v;
      v->worker_ = 0;
      v->uf_status_ = uf_status::LIVE;
      v->list_status_ = list_status::BUSY;
      v->wip_ = 0;
      v->expanded_ = false;
      v->expanded_local_ = 0;
      auto it = map_.insert({v});
      bool b = it.isnew();

      // Insertion failed, delete element
      // FIXME Should we add a local cache to avoid useless allocations?
      if (!b)
        delete v;
      else
        ++inserted_;

      uf_element* a_root = find(*it);
      if (a_root->uf_status_.load() == uf_status::DEAD)
        return {claim_status::CLAIM_DEAD, *it};

      if ((a_root->worker_.load() & w_id) != 0)
        return {claim_status::CLAIM_FOUND, *it};

      atomic_fetch_or(&((*it)->onstack_), w_id);
      atomic_fetch_or(&(a_root->worker_), w_id);
      while (a_root->parent.load() != a_root)
        {
          a_root = find(a_root);
          atomic_fetch_or(&(a_root->worker_), w_id);
        }

      return {claim_status::CLAIM_NEW, *it};
    }

    uf_element* find(uf_element* a)
    {
      uf_element* parent = a->parent.load();
      uf_element* x = a;
      uf_element* y;

      while (x != parent)
        {
          y = parent;
          parent = y->parent.load();
          if (parent == y)
            return y;
          x->parent.store(parent);
          x = parent;
          parent = x->parent.load();
        }
      return x;
    }

    bool sameset(uf_element* a, uf_element* b)
    {
      while (true)
        {
          uf_element* a_root = find(a);
          uf_element* b_root = find(b);
          if (a_root == b_root)
            return true;

          if (a_root->parent.load() == a_root)
            return false;
        }
    }

    void unite(uf_element* a, uf_element* b)
    {
      uf_element* a_root;
      uf_element* b_root;
      uf_element* q;
      uf_element* r;

      while (true)
        {
          a_root = find(a);
          b_root = find(b);

          if (a_root == b_root)
            return;

          r = std::max(a_root, b_root);
          q = std::min(a_root, b_root);

          if (std::atomic_compare_exchange_strong(&(q->parent), &q, r))
            return;
        }
    }

    void make_dead(uf_element* a, bool* sccfound)
    {
      uf_element* a_root = find(a);
      uf_status status = a_root->uf_status_.load();
      while (status != uf_status::DEAD)
        {
          if (status == uf_status::LIVE)
            *sccfound = std::atomic_compare_exchange_strong
              (&(a_root->uf_status_), &status, uf_status::DEAD);
          status = a_root->uf_status_.load();
        }
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
  struct SPOT_API renault_cond_dest_stats
  {
    unsigned inserted;          ///< \brief Number of states inserted
    unsigned states;            ///< \brief Number of states visited
    unsigned transitions;       ///< \brief Number of transitions visited
    unsigned sccs;              ///< \brief Number of SCCs visited
    unsigned walltime;          ///< \brief Walltime for this thread in ms
  };

  /// \brief This class implements the SCC decomposition algorithm of
  /// renault_cond_dest inspired from STTT'16 and ATVA'16. It uses a shared
  /// union-find to tag states as dead.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_renault_cond_dest
  {
  public:

    swarmed_renault_cond_dest(kripkecube<State, SuccIterator>& sys,
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
        todo_.push_back({pair.second, it, p});
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
              bool sccfound = false;
              // The state is no longer on thread's stack
              atomic_fetch_and(&(todo_.back().e->onstack_), ~w_id);
              sys_.recycle(todo_.back().it, tid_);

              uf_element* s = todo_.back().e;
              todo_.pop_back();
              if (todo_.size() == Rp_.back().second)
                {
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
                  todo_.push_back({w.second, it, p});
                  ++states_;
                }
              else if (w.first == st_uf::claim_status::CLAIM_FOUND)
                {
                  // An expansion is required
                  if ((w.second->onstack_.load() & w_id) &&
                      !todo_.back().e->expanded_.load())
                    w.second->expanded_.store(true);

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
                }
            }
        }
      tm_.stop("DFS thread " + std::to_string(tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(tid_)).walltime();
    }

    renault_cond_dest_stats stats()
    {
      return {uf_.inserted(), states_, transitions_, sccs_, walltime()};
    }

  private:
    struct todo_element
    {
      uf_element* e;
      SuccIterator* it;
      unsigned pos;
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
