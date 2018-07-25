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
  class iterable_uf_safety
  {

  public:
    enum class uf_status  { LIVE, LOCK, DEAD };
    enum class list_status  { BUSY, LOCK, DONE };
    enum class claim_status  { CLAIM_FOUND, CLAIM_NEW, CLAIM_DEAD };

    /// \brief Represents a Union-Find element
    struct uf_element_s
    {
      /// \brief the state handled by the element
      State st_;

      /// \brief the size of the current partition
      std::atomic<unsigned> size_;
      std::atomic<unsigned> done_;
      std::atomic<unsigned> to_expand_;
      std::atomic<bool> is_done_;

      /// \brief reference to the pointer
      std::atomic<uf_element_s*> parent;
      /// The set of worker for a given state
      std::atomic<unsigned> worker_;
      /// \brief next element for work stealing
      std::atomic<uf_element_s*> next_;
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

    /// \brief The haser for the previous uf_element_s.
    struct uf_element_s_hasher
    {
      uf_element_s_hasher(const uf_element_s*)
      { }

      uf_element_s_hasher() = default;

      brick::hash::hash128_t
      hash(const uf_element_s* lhs) const
      {
        StateHash hash;
        // Not modulo 31 according to brick::hashset specifications.
        unsigned u = hash(lhs->st_) % (1<<30);
        return {u, u};
      }

      bool equal(const uf_element_s* lhs,
                 const uf_element_s* rhs) const
      {
        StateEqual equal;
        return equal(lhs->st_, rhs->st_);
      }
    };

    ///< \brief Shortcut to ease shared map manipulation
    using shared_map = brick::hashset::FastConcurrent <uf_element_s*,
                                                       uf_element_s_hasher>;


    iterable_uf_safety(shared_map& map, unsigned tid):
      map_(map), tid_(tid),
      size_(std::thread::hardware_concurrency()),
      nb_th_(std::thread::hardware_concurrency()), inserted_(0)
    {
    }

    ~iterable_uf_safety() {}

    std::pair<claim_status, uf_element_s*>
    make_claim(State a)
    {
      unsigned w_id = (1U << tid_);

      // Setup and try to insert the new state in the shared map.
      uf_element_s* v = new uf_element_s();
      v->st_ = a;
      v->size_ = 1;
      v->done_ = 0;
      v->is_done_ = false;
      v->to_expand_ = true;
      v->parent = v;
      v->next_ = v;
      v->worker_ = 0;
      v->uf_status_ = uf_status::LIVE;
      v->list_status_ = list_status::BUSY;
      v->wip_ = 0;
      v->expanded_ = false;

      auto it = map_.insert({v});
      bool b = it.isnew();

      // Insertion failed, delete element
      // FIXME Should we add a local cache to avoid useless allocations?
      if (!b)
        delete v;
      else
        ++inserted_;

      uf_element_s* a_root = find(*it);
      if (a_root->uf_status_.load() == uf_status::DEAD)
        return {claim_status::CLAIM_DEAD, *it};

      if ((a_root->worker_.load() & w_id) != 0)
        return {claim_status::CLAIM_FOUND, *it};

      atomic_fetch_or(&(a_root->worker_), w_id);
      while (a_root->parent.load() != a_root)
        {
          a_root = find(a_root);
          atomic_fetch_or(&(a_root->worker_), w_id);
        }

      return {claim_status::CLAIM_NEW, *it};
    }

    uf_element_s* find(uf_element_s* a)
    {
      uf_element_s* parent = a->parent.load();
      uf_element_s* x = a;
      uf_element_s* y;

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

    bool sameset(uf_element_s* a, uf_element_s* b)
    {
      while (true)
        {
          uf_element_s* a_root = find(a);
          uf_element_s* b_root = find(b);
          if (a_root == b_root)
            return true;

          if (a_root->parent.load() == a_root)
            return false;
        }
    }

    bool lock_root(uf_element_s* a)
    {
      uf_status expected = uf_status::LIVE;
      if (a->uf_status_.load() == expected)
        {
          if (std::atomic_compare_exchange_strong
              (&(a->uf_status_), &expected, uf_status::LOCK))
            {
              if (a->parent.load() == a)
                return true;
              unlock_root(a);
            }
        }
      return false;
    }

    inline void unlock_root(uf_element_s* a)
    {
      a->uf_status_.store(uf_status::LIVE);
    }

    uf_element_s* lock_list(uf_element_s* a)
    {
      uf_element_s* a_list = a;
      while (true)
        {
          bool dontcare = false;
          a_list = pick_from_list(a_list, &dontcare);
          if (a_list == nullptr)
            {
              return nullptr;
            }

          auto expected = list_status::BUSY;
          bool b = std::atomic_compare_exchange_strong
            (&(a_list->list_status_), &expected, list_status::LOCK);

          if (b)
            return a_list;

          a_list = a_list->next_.load();
        }
    }

    void unlock_list(uf_element_s* a)
    {
      a->list_status_.store(list_status::BUSY);
    }

    void unite(uf_element_s* a, uf_element_s* b)
    {
      uf_element_s* a_root;
      uf_element_s* b_root;
      uf_element_s* q;
      uf_element_s* r;

      while (true)
        {
          a_root = find(a);
          b_root = find(b);

          if (a_root == b_root)
            return;

          r = std::max(a_root, b_root);
          q = std::min(a_root, b_root);

          if (!lock_root(q))
            continue;

          break;
        }

      uf_element_s* a_list = lock_list(a);
      if (a_list == nullptr)
        {
          unlock_root(q);
          return;
        }

      uf_element_s* b_list = lock_list(b);
      if (b_list == nullptr)
        {
          unlock_list(a_list);
          unlock_root(q);
          return;
        }

      SPOT_ASSERT(a_list->list_status_.load() == list_status::LOCK);
      SPOT_ASSERT(b_list->list_status_.load() == list_status::LOCK);

      //  Swapping
      uf_element_s* a_next =  a_list->next_.load();
      uf_element_s* b_next =  b_list->next_.load();
      SPOT_ASSERT(a_next != nullptr);
      SPOT_ASSERT(b_next != nullptr);

      a_list->next_.store(b_next);
      b_list->next_.store(a_next);
      q->parent.store(r);

      // Update workers
      unsigned q_worker = q->worker_.load();
      unsigned r_worker = r->worker_.load();
      if ((q_worker|r_worker) != r_worker)
        {
          atomic_fetch_or(&(r->worker_), q_worker);
          while (r->parent.load() != r)
            {
              r = find(r);
              atomic_fetch_or(&(r->worker_), q_worker);
            }
        }

      // Update the size of the partition
      r->size_.store(r->size_ + r->size_);

      // Update wether an expansion would be required
      r->to_expand_.store(r->to_expand_ && r->to_expand_);

      unlock_list(a_list);
      unlock_list(b_list);
      unlock_root(q);
    }

    unsigned size(uf_element_s* a)
    {
      uf_element_s* parent = find(a);
      return parent->size_.load();
    }

    unsigned done(uf_element_s* a)
    {
      uf_element_s* parent = find(a);
      return parent->done_.load();
    }

    unsigned to_expand(uf_element_s* a)
    {
      uf_element_s* parent = find(a);
      return parent->to_expand_.load();
    }

    uf_element_s* pick_from_list(uf_element_s* u, bool* sccfound)
    {
      uf_element_s* a = u;
      while (true)
        {
          list_status a_status;
          while (true)
            {
              a_status = a->list_status_.load();

              if (a_status == list_status::BUSY)
                {
                  return a;
                }

              if (a_status == list_status::DONE)
                break;
            }

          uf_element_s* b = a->next_.load();

          // ------------------------------ NO LAZY : start
          // if (b == u)
          //   {
          //     uf_element_s* a_root = find(a);
          //     uf_status status = a_root->uf_status_.load();
          //     while (status != uf_status::DEAD)
          //       {
          //         if (status == uf_status::LIVE)
          //           *sccfound = std::atomic_compare_exchange_strong
          //             (&(a_root->uf_status_), &status, uf_status::DEAD);
          //         status = a_root->uf_status_.load();
          //       }
          //     return nullptr;
          //   }
          // a = b;
          // ------------------------------ NO LAZY : end

          if (a == b)
            {
              uf_element_s* a_root = find(u);
              uf_status status = a_root->uf_status_.load();
              while (status != uf_status::DEAD)
                {
                  if (status == uf_status::LIVE)
                    *sccfound = std::atomic_compare_exchange_strong
                      (&(a_root->uf_status_), &status, uf_status::DEAD);
                  status = a_root->uf_status_.load();
                }
              return nullptr;
            }

          list_status b_status;
          while (true)
            {
              b_status = b->list_status_.load();

              if (b_status == list_status::BUSY)
                {
                  return b;
                }

              if (b_status == list_status::DONE)
                break;
            }

          SPOT_ASSERT(b_status == list_status::DONE);
          SPOT_ASSERT(a_status == list_status::DONE);

          uf_element_s* c = b->next_.load();
          a->next_.store(c);
          a = c;
        }
    }

    void remove_from_list(uf_element_s* a)
    {
      while (true)
        {
          list_status a_status = a->list_status_.load();

          if (a_status == list_status::DONE)
            break;

          if (a_status == list_status::BUSY)
              std::atomic_compare_exchange_strong
                (&(a->list_status_), &a_status, list_status::DONE);
        }
    }

    void mark_not_to_expand(uf_element_s* a)
    {
      uf_element_s* parent = a;
      while (true)
        {
          parent = find(parent);
          bool status = parent->to_expand_.load();

          if (status == false)
            break;

          parent->to_expand_ = false;
        }
    }

    bool mark_done(uf_element_s* a, unsigned old_value)
    {
      uf_element_s* parent = find(a);
      return std::atomic_compare_exchange_strong
        (&(parent->done_), &old_value, old_value+1);
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
  struct SPOT_API bloemen_safety_stats
  {
    unsigned inserted;          ///< \brief Number of states inserted
    unsigned states;            ///< \brief Number of states visited
    unsigned transitions;       ///< \brief Number of transitions visited
    unsigned sccs;              ///< \brief Number of SCCs visited
    unsigned walltime;          ///< \brief Walltime for this thread in ms
  };

  /// \brief This class implements the SCC decomposition algorithm of
  /// bloemen_safety as described in PPOPP'16. It uses a shared
  /// union-find augmented to manage work stealing between threads.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_bloemen_safety
  {
  public:

    swarmed_bloemen_safety(kripkecube<State, SuccIterator>& sys,
                    iterable_uf_safety<State, StateHash, StateEqual>& uf,
                    unsigned tid):
      sys_(sys),  uf_(uf), tid_(tid),
      nb_th_(std::thread::hardware_concurrency())
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    using uf = iterable_uf_safety<State, StateHash, StateEqual>;
    using uf_element_s = typename uf::uf_element_s;

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
        bloemen_safety_recursive_start:
          while (true)
            {
              bool sccfound = false;
              uf_element_s* v_prime = uf_.pick_from_list(todo_.back(),
                                                         &sccfound);

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


              bool is_expanded = false;

              // This is an expanded state. Two situtations are of interrest:
              // (1) another thread decided to apply an expansion
              // (2) the state is naturally_expanded.
              if (v_prime->expanded_)
                {
                  it->fireall();
                  is_expanded = true;
                }
              else if (it->naturally_expanded())
                {
                  v_prime->expanded_.store(true);
                  uf_.mark_not_to_expand(v_prime);
                  is_expanded = true;
                }

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
                      // This thread is no longer actively working on this state
                      atomic_fetch_and(&(v_prime->wip_), ~w_id);
                      goto bloemen_safety_recursive_start;
                    }
                  else if (w.first == uf::claim_status::CLAIM_FOUND)
                    {

                      while (!uf_.sameset(todo_.back(), w.second))
                        {
                          uf_element_s* r = Rp_.back();
                          Rp_.pop_back();
                          uf_.unite(r, Rp_.back());
                        }
                    }
                  else if (w.first == uf::claim_status::CLAIM_DEAD)
                    {
                      uf_.mark_not_to_expand(v_prime);
                    }

                }

              // SPINLOCK
              while (!v_prime->is_done_.load())
                {
                  unsigned loc_done = uf_.done(v_prime);

                  if (uf_.size(v_prime) - loc_done == 1 &&
                      uf_.to_expand(v_prime))
                    {
                      v_prime->expanded_ = true;
                      uf_.mark_not_to_expand(v_prime);
                      break;
                    }
                  else
                    {
                      if (!is_expanded && v_prime->expanded_)
                        break;

                      if (uf_.mark_done(v_prime, loc_done))
                        {
                          v_prime->is_done_.store(true);
                          uf_.remove_from_list(v_prime);
                          // This thread is no longer actively working
                          // on this state FIXME: This is maybe
                          // useless for POR, since v_prime is now
                          // DONE atomic_fetch_and(&(v_prime->wip_), ~w_id);
                          sys_.recycle(it, tid_);
                        }
                    }
                }
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

    bloemen_safety_stats stats()
    {
      return {uf_.inserted(), states_, transitions_, sccs_, walltime()};
    }

  private:
    kripkecube<State, SuccIterator>& sys_;   ///< \brief The system to check
    std::vector<uf_element_s*> todo_;          ///< \brief The "recursive" stack
    std::vector<uf_element_s*> Rp_;            ///< \brief The DFS stack
    iterable_uf_safety<State, StateHash, StateEqual> uf_; ///< Copy!
    unsigned tid_;
    unsigned nb_th_;
    unsigned inserted_ = 0;           ///< \brief Number of states inserted
    unsigned states_  = 0;            ///< \brief Number of states visited
    unsigned transitions_ = 0;        ///< \brief Number of transitions visited
    unsigned sccs_ = 0;               ///< \brief Number of SCC visited
    spot::timer_map tm_;              ///< \brief Time execution
  };
}
