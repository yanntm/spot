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
#include <iosfwd>
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
  class scc_uf
  {

  public:
    enum class scc_uf_status  { LIVE, LOCK, DEAD };
    enum class list_status  { BUSY, LOCK, DONE };
    enum class claim_status  { CLAIM_FOUND, CLAIM_NEW, CLAIM_DEAD };

    /// \brief Represents a Union-Find element
    struct scc_uf_element
    {
      /// \brief the state handled by the element
      State st_;
      /// \brief reference to the pointer
      std::atomic<scc_uf_element*> parent;
      /// The set of worker for a given state
      std::atomic<unsigned> worker_;
      /// The set of worker for which this state is on DFS
      std::atomic<unsigned> onstack_;
      /// \brief next element for work stealing
      std::atomic<scc_uf_element*> next_;
      /// \brief current status for the element
      std::atomic<scc_uf_status> scc_uf_status_;
      /// \brief current status for the list
      std::atomic<list_status> list_status_;
    };

    /// \brief The haser for the previous scc_uf_element.
    struct scc_uf_element_hasher
    {
      scc_uf_element_hasher(const scc_uf_element*)
      { }

      scc_uf_element_hasher() = default;

      brick::hash::hash128_t
      hash(const scc_uf_element* lhs) const
      {
        StateHash hash;
        // Not modulo 31 according to brick::hashset specifications.
        unsigned u = hash(lhs->st_) % (1<<30);
        return {u, u};
      }

      bool equal(const scc_uf_element* lhs,
                 const scc_uf_element* rhs) const
      {
        StateEqual equal;
        return equal(lhs->st_, rhs->st_);
      }
    };

    ///< \brief Shortcut to ease shared map manipulation
    using shared_map = brick::hashset::FastConcurrent <scc_uf_element*,
                                                       scc_uf_element_hasher>;


    scc_uf(shared_map& map, unsigned tid):
      map_(map), tid_(tid),
      size_(std::thread::hardware_concurrency()),
      nb_th_(std::thread::hardware_concurrency()), inserted_(0)
    {
    }

    ~scc_uf() {}

    std::pair<claim_status, scc_uf_element*>
    make_claim(State a)
    {
      unsigned w_id = (1U << tid_);

      // Setup and try to insert the new state in the shared map.
      scc_uf_element* v = new scc_uf_element();
      v->st_ = a;
      v->parent = v;
      v->next_ = v;
      v->worker_ = 0;
      v->scc_uf_status_ = scc_uf_status::LIVE;
      v->list_status_ = list_status::BUSY;
      auto it = map_.insert({v});
      bool b = it.isnew();

      // Insertion failed, delete element
      // FIXME Should we add a local cache to avoid useless allocations?
      if (!b)
        delete v;
      else
        ++inserted_;

      scc_uf_element* a_root = find(*it);
      if (a_root->scc_uf_status_.load() == scc_uf_status::DEAD)
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

    scc_uf_element* find(scc_uf_element* a)
    {
      scc_uf_element* parent = a->parent.load();
      scc_uf_element* x = a;
      scc_uf_element* y;

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

    bool sameset(scc_uf_element* a, scc_uf_element* b)
    {
      while (true)
        {
          scc_uf_element* a_root = find(a);
          scc_uf_element* b_root = find(b);
          if (a_root == b_root)
            return true;

          if (a_root->parent.load() == a_root)
            return false;
        }
    }

    void unite(scc_uf_element* a, scc_uf_element* b)
    {
      scc_uf_element* a_root;
      scc_uf_element* b_root;
      scc_uf_element* q;
      scc_uf_element* r;

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

    void make_dead(scc_uf_element* a, bool* sccfound)
    {
      scc_uf_element* a_root = find(a);
      scc_uf_status status = a_root->scc_uf_status_.load();
      while (status != scc_uf_status::DEAD)
        {
          if (status == scc_uf_status::LIVE)
            *sccfound = std::atomic_compare_exchange_strong
              (&(a_root->scc_uf_status_), &status, scc_uf_status::DEAD);
          status = a_root->scc_uf_status_.load();
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
  struct SPOT_API display_scc_stats
  {
    unsigned inserted;          ///< \brief Number of states inserted
    unsigned states;            ///< \brief Number of states visited
    unsigned transitions;       ///< \brief Number of transitions visited
    unsigned sccs;              ///< \brief Number of SCCs visited
    unsigned walltime;          ///< \brief Walltime for this thread in ms
  };

  /// \brief This class implements the SCC decomposition algorithm inspired
  /// inspired from STTT'16.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class display_scc
  {
  public:

    display_scc(kripkecube<State, SuccIterator>& sys,
                    scc_uf<State, StateHash, StateEqual>& scc_uf,
                    unsigned tid):
      sys_(sys),  scc_uf_(scc_uf), tid_(tid),
      nb_th_(std::thread::hardware_concurrency())
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    using st_scc_uf = scc_uf<State, StateHash, StateEqual>;
    using scc_uf_element = typename st_scc_uf::scc_uf_element;

    void run()
    {
      tm_.start("DFS thread " + std::to_string(tid_));
      unsigned w_id = (1U << tid_);


      // Insert the initial state
      {
        State init = sys_.initial(tid_);
        auto pair = scc_uf_.make_claim(init);

        auto it = sys_.succ(pair.second->st_, tid_);
        unsigned p = livenum_.size();
        livenum_.insert({pair.second, p});
        Rp_.push_back({pair.second, todo_.size()});
        dfs_.push_back(pair.second);
        todo_.push_back({pair.second, it, p});
        ++states_;
      }

      while (!todo_.empty())
        {
          if (todo_.back().it->done())
            {
              bool sccfound = false;
              // The state is no longer on thread's stack
              atomic_fetch_and(&(todo_.back().e->onstack_), ~w_id);
              sys_.recycle(todo_.back().it, tid_);

              scc_uf_element* s = todo_.back().e;
              todo_.pop_back();
              if (todo_.size() == Rp_.back().second)
                {
                  Rp_.pop_back();
                  scc_uf_.make_dead(s, &sccfound);
                  sccs_ += sccfound;
                }
            }
          else
            {
              ++transitions_;
              State dst = todo_.back().it->state();
              auto w = scc_uf_.make_claim(dst);

              // Move forward iterators...
              todo_.back().it->next();

              if (w.first == st_scc_uf::claim_status::CLAIM_NEW)
                {
                  // ... and insert the new state
                  auto it = sys_.succ(w.second->st_, tid_);
                  unsigned p = livenum_.size();
                  livenum_.insert({w.second, p});
                  Rp_.push_back({w.second, todo_.size()});
                  dfs_.push_back(w.second);
                  todo_.push_back({w.second, it, p});
                  ++states_;
                }
              else if (w.first == st_scc_uf::claim_status::CLAIM_FOUND)
                {

                  unsigned dpos = livenum_[w.second];
                  unsigned r = Rp_.back().second;
                  while (dpos < todo_[r].pos)
                    {
                      scc_uf_.unite(w.second, todo_[r].e);
                      Rp_.pop_back();
                      r = Rp_.back().second;
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

    display_scc_stats stats()
    {
      return {scc_uf_.inserted(), states_, transitions_, sccs_, walltime()};
    }

    void print()
    {
      int cpt = 0;
      std::unordered_map<scc_uf_element*, unsigned> mapping;
      //map to store the scc the state belong to
      std::unordered_map<State, unsigned, StateHash, StateEqual> s_map;
      //map to store the number of states per scc
      std::unordered_map<unsigned, unsigned> nb_states;
      for (auto state: dfs_)
        {
          scc_uf_element* root = scc_uf_.find(state);
          if (mapping.find(root) == mapping.end())
            {
              mapping[root] = ++cpt;
              nb_states[cpt] = 1;
            }
          else
            nb_states[mapping[root]]++;
          s_map[state->st_] = mapping[root];
          std::cout << mapping[root] << std::endl;
        }

      //map to store the scc graph
      std::unordered_map<unsigned, std::set<unsigned>> map;
      for (auto state: dfs_)
        {
          std::set<unsigned> set;
          scc_uf_element* root = scc_uf_.find(state);
          if (map.find(mapping[root]) == map.end())
              map[mapping[root]] = set;
          auto iter = sys_.succ(state->st_, 0);
          while (!iter->done())
            {
              if (s_map[iter->state()] != mapping[root])
                map[mapping[root]].emplace(s_map[iter->state()]);
              iter->next();
            }
        }

      //FIXME: save the graph here

      add_states(&map, 1);

      auto cpy = nb_states;

      unsigned m = 0;
      for (auto tuple: map)
        if (m < std::get<0>(tuple))
          m = std::get<0>(tuple);

      for (unsigned i = 1; i <= m; ++i)
        for (unsigned scc: map[i])
          nb_states[i] += cpy[scc];
    }

    /// \brief Recursively add the accessible scc in the scc graph
    std::set<unsigned> add_states(std::unordered_map<unsigned,
                                                     std::set<unsigned>> *map,
                                  unsigned state)
    {
      for (auto scc: (*map)[state])
        for (auto scc2: add_states(map, scc))
          (*map)[state].emplace(scc2);
      return (*map)[state];
    }

  private:
    struct todo_element
    {
      scc_uf_element* e;
      SuccIterator* it;
      unsigned pos;
    };

    kripkecube<State, SuccIterator>& sys_;   ///< \brief The system to check
    std::vector<todo_element> todo_;          ///< \brief The "recursive" stack
    std::vector<std::pair<scc_uf_element*,
                          unsigned>> Rp_;  ///< \brief The root stack
    st_scc_uf scc_uf_; ///< Copy!
    unsigned tid_;
    unsigned nb_th_;
    unsigned inserted_ = 0;           ///< \brief Number of states inserted
    unsigned states_  = 0;            ///< \brief Number of states visited
    unsigned transitions_ = 0;        ///< \brief Number of transitions visited
    unsigned sccs_ = 0;               ///< \brief Number of SCC visited
    spot::timer_map tm_;              ///< \brief Time execution
    std::unordered_map<scc_uf_element*, unsigned> livenum_;
    std::vector<scc_uf_element*> dfs_; ///< \brief Handle all states
  };
}
