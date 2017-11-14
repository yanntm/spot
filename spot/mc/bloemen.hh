// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016, 2017 Laboratoire de Recherche et
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
  class iterable_uf
  {

  public:
    enum class uf_status  { LIVE, LOCK, DEAD };
    enum class list_status  { BUSY, LOCK, DONE };
    enum class claim_status  { CLAIM_FOUND, CLAIM_NEW, CLAIM_DEAD };

    /// \brief Represents an Union-Find element
    struct uf_element
    {
      /// \brief the state handled by the element
      State st;
      /// \brief reference to the pointer
      std::atomic<uf_element*> parent;
      /// The set of worker for a given state
      std::atomic<unsigned> worker;
      /// \brief next element for work stealing
      std::atomic<uf_element*> next;
      /// \brief current status for the element
      std::atomic<uf_status> uf_status;
      ///< \brief current status for the list
      std::atomic<list_status> list_status;
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
        unsigned u = hash(lhs->st) % (1<<30);
        return {u, u};
      }

      bool equal(const uf_element* lhs,
                 const uf_element* rhs) const
      {
        StateEqual equal;
        return equal(lhs->st, rhs->st);
      }
    };

    ///< \brief Shortcut to ease shared map manipulation
    using shared_map = brick::hashset::FastConcurrent <uf_element*,
                                                       uf_element_hasher>;


    iterable_uf(shared_map& map, unsigned tid):
      map_(map), tid_(tid), size_(std::thread::hardware_concurrency()),
      nb_th_(std::thread::hardware_concurrency())
    {
    }

    ~iterable_uf() {}

    std::pair<claim_status, uf_element*>
    make_claim(State a)
    {
      unsigned w_id = (1U << tid_);

      // Setup and try to insert the new state in the shared map.
      uf_element* v = new uf_element();
      v->st = a;
      v->parent = v;
      v->next = v;
      v->worker = 0;
      v->uf_status = uf_status::LIVE;
      v->list_status = list_status::BUSY;

      auto it = map_.insert({v});
      bool b = it.isnew();

      // Insertion failed, delete element
      // FIXME Should we add a local cache to avoid useless allocations?
      if (!b)
        delete v;

      uf_element* a_root = find(*it);
      if (a_root->uf_status.load() == uf_status::DEAD)
        return {claim_status::CLAIM_DEAD, *it};

      if ((a_root->worker.load() & w_id) != 0)
        return {claim_status::CLAIM_FOUND, *it};

      atomic_fetch_or(&(a_root->worker), w_id);
      while (a_root->parent.load() != a_root)
        {
          a_root = find(a_root);
          atomic_fetch_or(&(a_root->worker), w_id);
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
      uf_element* a_root = find(a);
      uf_element* b_root = find(b);
      if (a_root == b_root)
        return true;

      if (a_root->parent.load() == a_root)
        return false;

      return sameset(a_root, b_root);
    }

    bool lock_root(uf_element* a)
    {
      uf_status expected = uf_status::LIVE;
      if (a->uf_status.load() == expected)
        {
          if (std::atomic_compare_exchange_strong
              (&(a->uf_status), &expected, uf_status::LOCK))
            {
              if (a->parent.load() == a)
                return true;
              unlock_root(a);
            }
        }
      return false;
    }

    inline void unlock_root(uf_element* a)
    {
      a->uf_status.store(uf_status::LIVE);
    }

    uf_element* lock_list(uf_element* a)
    {
      uf_element* a_list = a;
      while (true)
        {
          bool dontcare = false;
          a_list = pick_from_list(a_list, &dontcare);
          if (a_list == nullptr)
            {
              assert(false);
              return nullptr;
            }

          auto expected = list_status::BUSY;
          bool b = std::atomic_compare_exchange_strong
            (&(a_list->list_status), &expected, list_status::LOCK);

          if (b)
            return a_list;

          a_list = a_list->next.load();
        }
    }

    void unlock_list(uf_element* a)
    {
      a->list_status.store(list_status::BUSY);
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

          if (!lock_root(q))
            continue;

          break;
        }

      uf_element* a_list = lock_list(a);
      if (a_list == nullptr)
        return;

      uf_element* b_list = lock_list(b);
      if (b_list == nullptr)
        {
          unlock_list(a_list);
          return;
        }

      assert(a_list->list_status.load() == list_status::LOCK);
      assert(b_list->list_status.load() == list_status::LOCK);


      //  Swapping
      uf_element* a_next =  a_list->next.load();
      uf_element* b_next =  b_list->next.load();
      assert(a_next != 0);
      assert(b_next != 0);

      a_list->next.store(b_next);
      b_list->next.store(a_next);
      q->parent.store(r);

      // Update workers
      unsigned q_worker = q->worker.load();
      unsigned r_worker = r->worker.load();
      if ((q_worker|r_worker) != r_worker)
        {
          atomic_fetch_or(&(r->worker), q_worker);
          while(r->parent.load() != r)
            {
              r = find(r);
              atomic_fetch_or(&(r->worker), q_worker);
            }
        }

      unlock_list(a_list);
      unlock_list(b_list);
      unlock_root(q);
    }

    uf_element* pick_from_list(uf_element* u, bool* sccfound)
    {
      uf_element* a = u;
      while (true)
        {
          list_status a_status;
          while (true)
            {
              a_status = a->list_status.load();

              if (a_status == list_status::BUSY)
                {
                  return a;
                }

              if (a_status == list_status::DONE)
                break;
            }

          uf_element* b = a->next.load();

          // ------------------------------ NO LAZY : start
          // if (b == u)
          //   {
          //     uf_element* a_root = find(a);
          //     uf_status status = a_root->uf_status.load();
          //     while (status != uf_status::DEAD)
          //       {
          //         if (status == uf_status::LIVE)
          //           *sccfound = std::atomic_compare_exchange_strong
          //             (&(a_root->uf_status), &status, uf_status::DEAD);
          //         status = a_root->uf_status.load();
          //       }
          //     return nullptr;
          //   }
          // a = b;
          // ------------------------------ NO LAZY : end

          if (a == b)
            {
              uf_element* a_root = find(u);
              uf_status status = a_root->uf_status.load();
              while (status != uf_status::DEAD)
                {
                  if (status == uf_status::LIVE)
                    *sccfound = std::atomic_compare_exchange_strong
                      (&(a_root->uf_status), &status, uf_status::DEAD);
                  status = a_root->uf_status.load();
                }
              return nullptr;
            }

          list_status b_status;
          while (true)
            {
              b_status = b->list_status.load();

              if (b_status == list_status::BUSY)
                {
                  return b;
                }

              if (b_status == list_status::DONE)
                break;
            }

          assert(b_status == list_status::DONE);
          assert(a_status == list_status::DONE);

          uf_element* c = b->next.load();
          a->next.store(c);
          a = c;
        }
    }

    void remove_from_list(uf_element* a)
    {
      while (true)
        {
          list_status a_status = a->list_status.load();

          if (a_status == list_status::DONE)
            break;

          if (a_status == list_status::BUSY)
              std::atomic_compare_exchange_strong
                (&(a->list_status), &a_status, list_status::DONE);
        }
    }

  private:
    shared_map map_;      ///< \brief Map shared by threads copy!
    unsigned tid_;        ///< \brief The Id of the current thread
    unsigned size_;       ///< \brief Maximum number of thread
    unsigned nb_th_;      ///< \brief Current number of threads
  };

  /// \brief This object is returned by the algorithm below
  struct SPOT_API bloemen_stats
  {
    unsigned states;            ///< \brief Number of states visited
    unsigned transitions;       ///< \brief Number of transitions visited
    unsigned sccs;              ///< \brief Number of SCCs visited
    unsigned walltime;          ///< \brief Walltime for this thread in ms
  };

  /// \brief This class implements the SCC decomposition algorithm of bloemen
  /// as described in PPOPP'16. It uses a shared union-find augmented to manage
  /// work stealing between threads.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_bloemen
  {
  public:

    swarmed_bloemen(kripkecube<State, SuccIterator>& sys,
                    iterable_uf<State, StateHash, StateEqual>& uf,
                    unsigned tid):
      sys_(sys),  uf_(uf), tid_(tid),
      nb_th_(std::thread::hardware_concurrency())
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    using uf = iterable_uf<State, StateHash, StateEqual>;
    using uf_element = typename uf::uf_element ;


    void run()
    {
      tm_.start("DFS thread " + std::to_string(tid_));

      State init = sys_.initial(tid_);
      auto pair = uf_.make_claim(init);
      todo_.push_back(pair.second);
      Rp_.push_back(pair.second);
      ++states_;

      while (!todo_.empty())
        {
        bloemen_recursive_start:
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

              auto it = sys_.succ(v_prime->st, tid_);
              while (!it->done())
                {
                  auto w = uf_.make_claim(it->state());
                  it->next();
                  ++transitions_;
                  if (w.first == uf::claim_status::CLAIM_NEW)
                    {
                      todo_.push_back(w.second);
                      Rp_.push_back(w.second);
                      ++states_;
                      goto bloemen_recursive_start;
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

    bloemen_stats stats()
    {
      return {states_, transitions_, sccs_, walltime()};
    }

  private:
    kripkecube<State, SuccIterator>& sys_;   ///< \brief The system to check
    std::vector<uf_element*> todo_;          ///< \brief The "recursive" stack
    std::vector<uf_element*> Rp_;            ///< \brief The DFS stack
    iterable_uf<State, StateHash, StateEqual> uf_; ///< Copy!
    unsigned tid_;
    unsigned nb_th_;
    unsigned states_  = 0;            ///< \brief Number of states visited
    unsigned transitions_ = 0;        ///< \brief Number of transitions visited
    unsigned sccs_ = 0;               ///< \brief Number of SCC visited
    spot::timer_map tm_;              ///< \brief Time execution
  };
}
