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
      State st;                 ///< \brief the state handled by the element
      uf_element* parent;       ///< \brief reference to the pointer
      int* workers;             ///< \brief assign only once at allocation
      uf_element* next;         ///< \brief next element for work stealing
      uf_status uf_status;      ///< \brief current status for the element
      list_status list_status;  ///< \brief current status for the list
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
      p_(sizeof(int)*std::thread::hardware_concurrency()),
      map_(map), tid_(tid), size_(std::thread::hardware_concurrency()),
      nb_th_(std::thread::hardware_concurrency())
    {
    }

    ~iterable_uf() {}

    std::pair<claim_status, uf_element*>
    make_claim(State a)
    {
      // Prepare data for a newer allocation
      int* ref = (int*) p_.allocate();
      for (unsigned i = 0; i < nb_th_; ++i)
        ref[i] = 0;

      // Setup and try to insert the new state in the shared map.
      uf_element* v = new uf_element();
      v->st = a;
      v->parent = v;
      v->workers = ref;
      v->next = v;
      v->uf_status = uf_status::LIVE;
      v->list_status = list_status::BUSY;

      auto it = map_.insert({v});
      bool b = it.isnew();

      // Insertion failed, delete element
      // FIXME Should we add a local cache to avoid useless allocations?
      if (!b)
        {
          p_.deallocate(ref);
          delete v;
        }

      uf_element* a_root = find(*it); // FIXME??
      if (a_root->uf_status == uf_status::DEAD)
        return {claim_status::CLAIM_DEAD, a_root};

      if (a_root->workers[tid_])
        return {claim_status::CLAIM_FOUND, a_root};

      while (!a_root->workers[tid_])
        {
          a_root->workers[tid_] = 1;
          a_root = find(a_root);
        }

      return {claim_status::CLAIM_NEW, a_root};
    }

    uf_element* find(uf_element* a)
    {
      if (a->parent != a)
        a->parent = find(a->parent);
      return a->parent;
    }


    bool sameset(uf_element* a, uf_element* b)
    {
      uf_element* a_root = find(a);
      uf_element* b_root = find(b);

      assert(a_root != nullptr);
      assert(b_root != nullptr);

      if (a_root == b_root)
        return true;

      if (a_root->parent == a_root)
        return false;

      return sameset(a_root, b_root);
    }


    bool lock_root(uf_element* a)
    {
      if (CAS(&(a->uf_status), uf_status::LIVE, uf_status::LOCK))
        {
          if (a->parent == a)
            return true;
          unlock_root(a);
        }
      return false;
    }

    void unlock_root(uf_element* a)
    {
      a->uf_status = uf_status::LIVE;
    }

    uf_element* lock_list(uf_element* a)
    {
      assert(a != nullptr);
      bool dontcare = false;
      uf_element* a_list = pick_from_list(a, &dontcare);
      if (a_list == nullptr)
        return nullptr;

      if (CAS(&(a->list_status), list_status::BUSY, list_status::LOCK))
        return a_list;

      return lock_list(a_list->next);
    }

    void unite(uf_element* a, uf_element* b)
    {
      uf_element* a_root = find(a);
      uf_element* b_root = find(b);
      if (a_root == b_root)
        return;

      uf_element* r = std::max(a_root, b_root);
      uf_element* q = std::min(a_root, b_root);

      if (!lock_root(q))
        {
          unite(a_root, b_root);
          return;
        }

      uf_element* a_list = lock_list(a);
      uf_element* b_list = lock_list(b);

      if (a_list == nullptr || b_list == nullptr)
        return;

      //  Swapping
      uf_element* tmp = a_list->next;
      a_list->next = b_list->next;
      b_list->next = tmp;
      q->parent = r;

      do
        {
          r = find(r);
          for(unsigned i = 0; i < nb_th_; ++i)
            r->workers[i] |= q->workers[i];
        }
      while(r->parent != r);

      a_list->list_status = list_status::BUSY;
      b_list->list_status = list_status::BUSY;
      q->uf_status = uf_status::LIVE;
    }

    uf_element* pick_from_list(uf_element* a, bool* sccfound)
    {
      assert(a != nullptr);
      do
        {
          if (a->list_status == list_status::BUSY)
            return a;
        }
      while (a->list_status == list_status::LOCK);
      uf_element* b = a->next;

      assert(b != nullptr);
      if (a == b)
        {
          uf_element* a_root = find(a);
          if (CAS(&(a_root->uf_status), uf_status::LIVE, uf_status::DEAD))
            *sccfound = true;   // Report An SCC.
          return nullptr;
        }

      do
        {
          if (b->list_status == list_status::BUSY)
            return b;
        }
      while (b->list_status == list_status::LOCK);

      uf_element* c = b->next;
      a->next = c;
      return pick_from_list(c, sccfound);
    }

    void remove_from_list(uf_element* a)
    {
      while (a->list_status != list_status::DONE)
        {
          CAS(&(a->list_status), list_status::BUSY, list_status::DONE);
        }
    }

    bool CAS(list_status* ls, list_status ls_old, list_status ls_new)
    {
      if (*ls != ls_old)
        return false;
      *ls = ls_new;
      return true;
    }

    bool CAS(uf_status* uf, uf_status uf_old, uf_status uf_new)
    {
      if (*uf != uf_old)
        return false;
      *uf = uf_new;
      return true;
    }

  private:
    fixed_size_pool p_;   ///< \brief Element Allocator
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
