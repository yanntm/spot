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
  class scc_uf_csv
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


    scc_uf_csv(shared_map& map):
      map_(map), inserted_(0)
    {
    }

    ~scc_uf_csv() {}

    std::pair<claim_status, scc_uf_element*>
    make_claim(State a)
    {
      unsigned w_id = (1U << 0);

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
    unsigned inserted_;   ///< \brief The number of insert succes
  };

  /// \brief This class implements the SCC decomposition algorithm inspired
  /// inspired from STTT'16.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class csv_maker
  {
  public:

    csv_maker(kripkecube<State, SuccIterator>& sys,
                    scc_uf_csv<State, StateHash, StateEqual>& scc_uf,
                    std::string file):
      sys_(sys),  scc_uf_(scc_uf), file_(file)
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    using st_scc_uf = scc_uf_csv<State, StateHash, StateEqual>;
    using scc_uf_element = typename st_scc_uf::scc_uf_element;

    void run()
    {
      unsigned w_id = (1U << 0);
      int depth = 1;

      // Insert the initial state
      {
        State init = sys_.initial(0);
        auto pair = scc_uf_.make_claim(init);

        auto it = sys_.succ(pair.second->st_, 0);
        unsigned p = livenum_.size();
        livenum_.insert({pair.second, p});
        Rp_.push_back({pair.second, todo_.size()});
        dfs_.push_back(pair.second);
        todo_.push_back({pair.second, it, p});
        ++states_;

        v_names_ = sys_.to_header(init);
        v_values_.push_back(sys_.to_csv(init));
        succ_labels_.push_back(todo_.back().it->size());
        depth_labels_.push_back(depth);
      }

      while (!todo_.empty())
        {
          if (todo_.back().it->done())
            {
              bool sccfound = false;
              // The state is no longer on thread's stack
              atomic_fetch_and(&(todo_.back().e->onstack_), ~w_id);
              sys_.recycle(todo_.back().it, 0);

              scc_uf_element* s = todo_.back().e;
              todo_.pop_back();
              if (todo_.size() == Rp_.back().second)
                {
                  Rp_.pop_back();
                  scc_uf_.make_dead(s, &sccfound);
                  sccs_ += sccfound;
                }
              --depth;
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
                  auto it = sys_.succ(w.second->st_, 0);
                  unsigned p = livenum_.size();
                  livenum_.insert({w.second, p});
                  Rp_.push_back({w.second, todo_.size()});
                  dfs_.push_back(w.second);
                  todo_.push_back({w.second, it, p});
                  ++states_;

                  v_values_.push_back(sys_.to_csv(dst));
                  succ_labels_.push_back(todo_.back().it->size());
                  ++depth;
                  depth_labels_.push_back(depth);
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
      scc();
      print();
    }

    void scc()
    {
      int cpt = 0;
      std::unordered_map<scc_uf_element*, unsigned> mapping;
      for (auto state: dfs_)
        {
          scc_uf_element* root = scc_uf_.find(state);
          if (mapping.find(root) == mapping.end())
            mapping[root] = ++cpt;
          scc_labels_.push_back(mapping[root]);
        }
    }

    void print()
    {
      std::ofstream ofs;
      ofs.open(file_);
      ofs << "#states:" << states_ << std::endl;
      ofs << "#transitions:" << transitions_ << std::endl;
      ofs << "#sccs:" << sccs_ << std::endl;
      ofs << "#exploration:" << "DFS" << std::endl;
      ofs << "#variables_names:" << v_names_ << std::endl;
      ofs << "#variables_types:";
      std::string token;
      std::stringstream ss(v_names_);
      int c = 0;
      std::string str;
      while (std::getline(ss, token, ','))
        {
          if (token.find('.') != std::string::npos)
            {
              if (token.find('[') != std::string::npos)
                str.append("la");
              else
                str.append("l");
            }
          else if (token.find('[') != std::string::npos)
            str.append("sa");
          else if (sys_.type_name(c).compare("int") == 0
              || sys_.type_name(c).compare("byte") == 0)
            str.append("s");
          else
            str.append("p");
          c++;
          str.append(",");
        }
      str.erase(str.end() - 1);
      ofs << str << std::endl;
      ofs << "#labels:" << c << "," << c + 1 << "," << c + 2 << "," << c + 3;
      ofs << std::endl;
      int count = 0;
      for (std::string s: v_values_)
        {
          ofs << s << "," << count << "," << depth_labels_[count] << ","
              << succ_labels_[count] << "," << scc_labels_[count] << std::endl;
          ++count;
        }
      ofs.close();
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
    unsigned inserted_ = 0;           ///< \brief Number of states inserted
    unsigned states_  = 0;            ///< \brief Number of states visited
    unsigned transitions_ = 0;        ///< \brief Number of transitions visited
    unsigned sccs_ = 0;               ///< \brief Number of SCC visited
    std::unordered_map<scc_uf_element*, unsigned> livenum_;
    std::vector<scc_uf_element*> dfs_; ///< \brief Handle all states
    std::string file_; ///< \brief The path to the csv file
    std::string v_names_; ///< \brief The name of the variables
    std::vector<std::string> v_values_; ///< \brief The values of the variables
    std::vector<int> scc_labels_; ///< \brief The scc per state
    std::vector<int> depth_labels_; ///< \brief The depth per state
    std::vector<int> succ_labels_; ///< \brief The number of successors
  };
}
