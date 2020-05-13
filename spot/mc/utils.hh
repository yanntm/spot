// -*- coding: utf-8 -*-
// Copyright (C) 2016, 2020 Laboratoire de Recherche et
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

#include <spot/mc/reachability.hh>
#include <spot/mc/intersect.hh>
#include <spot/twa/twa.hh>
#include <spot/twacube_algos/convert.hh>

namespace spot
{
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class SPOT_API kripke_to_twa :
    public seq_reach_kripke<State, SuccIterator,
                            StateHash, StateEqual,
                            kripke_to_twa<State, SuccIterator,
                                          StateHash, StateEqual>>
  {
  public:
    kripke_to_twa(kripkecube<State, SuccIterator>& sys, bdd_dict_ptr dict)
      : seq_reach_kripke<State, SuccIterator, StateHash, StateEqual,
                         kripke_to_twa<State, SuccIterator,
                                       StateHash, StateEqual>>(sys),
      dict_(dict)
      {}

    ~kripke_to_twa()
      {
      }

    void setup()
    {
      res_ = make_twa_graph(dict_);
      names_ = new std::vector<std::string>();

      // padding to simplify computation.
      res_->new_state();

      // Compute the reverse binder.
      auto aps = this->sys_.ap();
      for (unsigned i = 0; i < aps.size(); ++i)
        {
          auto k = res_->register_ap(aps[i]);
          reverse_binder_.insert({i, k});
        }
    }

    void push(State s, unsigned i)
    {
      unsigned st = res_->new_state();
      names_->push_back(this->sys_.to_string(s));
      SPOT_ASSERT(st == i);
    }

    void edge(unsigned src, unsigned dst)
    {
      cubeset cs(this->sys_.ap().size());
      bdd cond = cube_to_bdd(this->todo.back().it->condition(),
                             cs, reverse_binder_);
      res_->new_edge(src, dst, cond);
    }

    void finalize()
    {
      res_->set_init_state(1);
      res_->purge_unreachable_states();
      res_->set_named_prop<std::vector<std::string>>("state-names", names_);
    }

    twa_graph_ptr twa()
    {
      return res_;
    }

  private:
    spot::twa_graph_ptr res_;
    std::vector<std::string>* names_;
    bdd_dict_ptr dict_;
    std::unordered_map<int, int> reverse_binder_;
  };

  // FIXME: should be merge into the next algorithm.
  /// \brief This class explores (with a DFS) a product between a
  /// system and a twa. This exploration is performed on-the-fly.
  /// Since this exploration aims to be a generic we need to define
  /// hooks to the various emptiness checks.
  /// Actually, we use "mixins templates" in order to efficiently
  /// call emptiness check procedure. This means that we add
  /// a template \a EmptinessCheck that will be called though
  /// four functions:
  ///    - setup: called before any operation
  ///    - push: called for every new state
  ///    - pop: called every time a state leave the DFS stack
  ///    - update: called for every closing edge
  ///    - trace: must return a counterexample (if exists)
  ///
  /// The other template parameters allows to consider any kind
  /// of state (and so any kind of kripke structures).
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual,
           typename EmptinessCheck>
  class SPOT_API intersect
  {
  public:
    intersect(const intersect<State, SuccIterator, StateHash,
                              StateEqual, EmptinessCheck>& i) = default;

    intersect(kripkecube<State, SuccIterator>& sys,
              twacube_ptr twa, unsigned tid, bool& stop):
      sys_(sys), twa_(twa), tid_(tid), stop_(stop)
    {
      static_assert(spot::is_a_kripkecube_ptr<decltype(&sys),
                                             State, SuccIterator>::value,
                    "error: does not match the kripkecube requirements");
      map.reserve(2000000);
      todo.reserve(100000);
    }

    ~intersect()
    {
      map.clear();
    }

    /// \brief In order to implement "mixin paradigm", we
    /// must be abble to access the acual definition of
    /// the emptiness check that, in turn, has to access
    /// local variables.
    EmptinessCheck& self()
    {
      return static_cast<EmptinessCheck&>(*this);
    }

    /// \brief The main function that will perform the
    /// product on-the-fly and call the emptiness check
    /// when necessary.
    bool run()
    {
      self().setup();
      product_state initial = {sys_.initial(tid_), twa_->get_initial()};
      if (SPOT_LIKELY(self().push_state(initial, dfs_number+1, {})))
        {
          todo.push_back({initial, sys_.succ(initial.st_kripke, tid_),
                twa_->succ(initial.st_prop)});

          // Not going further! It's a product with a single state.
          if (todo.back().it_prop->done())
            return false;

          forward_iterators(sys_, twa_, todo.back().it_kripke,
                            todo.back().it_prop, true, 0);
          map[initial] = ++dfs_number;
        }
      while (!todo.empty() && !stop_)
        {
          // Check the kripke is enough since it's the outer loop. More
          // details in forward_iterators.
          if (todo.back().it_kripke->done())
            {
              bool is_init = todo.size() == 1;
              auto newtop = is_init? todo.back().st: todo[todo.size() -2].st;
              if (SPOT_LIKELY(self().pop_state(todo.back().st,
                                             map[todo.back().st],
                                             is_init,
                                             newtop,
                                             map[newtop])))
                {
                  sys_.recycle(todo.back().it_kripke, tid_);
                  // FIXME a local storage for twacube iterator?
                  todo.pop_back();
                  if (SPOT_UNLIKELY(self().counterexample_found()))
                    return true;
                }
            }
          else
            {
              ++transitions;
              product_state dst = {
                todo.back().it_kripke->state(),
                twa_->trans_storage(todo.back().it_prop, tid_).dst
              };
              auto acc = twa_->trans_data(todo.back().it_prop, tid_).acc_;
              forward_iterators(sys_, twa_, todo.back().it_kripke,
                                todo.back().it_prop, false, 0);
              auto it  = map.find(dst);
              if (it == map.end())
                {
                  if (SPOT_LIKELY(self().push_state(dst, dfs_number+1, acc)))
                    {
                      map[dst] = ++dfs_number;
                      todo.push_back({dst, sys_.succ(dst.st_kripke, tid_),
                            twa_->succ(dst.st_prop)});
                      forward_iterators(sys_, twa_, todo.back().it_kripke,
                                        todo.back().it_prop, true, 0);
                    }
                }
              else if (SPOT_UNLIKELY(self().update(todo.back().st,
                                                 dfs_number,
                                                 dst, map[dst], acc)))
                return true;
            }
        }
      return false;
    }

    unsigned int states()
    {
      return dfs_number;
    }

    unsigned int trans()
    {
      return transitions;
    }

    std::string counterexample()
    {
      return self().trace();
    }

  public:
    struct product_state
    {
      State st_kripke;
      unsigned st_prop;
    };

    struct product_state_equal
    {
      bool
      operator()(const product_state lhs,
                 const product_state rhs) const
      {
        StateEqual equal;
        return (lhs.st_prop == rhs.st_prop) &&
          equal(lhs.st_kripke, rhs.st_kripke);
      }
    };

    struct product_state_hash
    {
      size_t
      operator()(const product_state that) const noexcept
      {
        // FIXME! wang32_hash(that.st_prop) could have
        // been pre-calculated!
        StateHash hasher;
        return  wang32_hash(that.st_prop) ^ hasher(that.st_kripke);
      }
    };

    struct todo_element
    {
      product_state st;
      SuccIterator* it_kripke;
      std::shared_ptr<trans_index> it_prop;
    };
    kripkecube<State, SuccIterator>& sys_;
    twacube_ptr twa_;
    std::vector<todo_element> todo;
    typedef std::unordered_map<const product_state, int,
                               product_state_hash,
                               product_state_equal> visited_map;
    visited_map map;
    unsigned int dfs_number = 0;
    unsigned int transitions = 0;
    unsigned tid_;
    bool& stop_; // Do not need to be atomic.
  };





















  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class SPOT_API product_to_twa :
    public intersect<State, SuccIterator,
                     StateHash, StateEqual,
                     product_to_twa<State, SuccIterator,
                                    StateHash, StateEqual>>
  {
    // Ease the manipulation
    using typename intersect<State, SuccIterator, StateHash, StateEqual,
                             product_to_twa<State, SuccIterator,
                                            StateHash,
                                            StateEqual>>::product_state;

  public:
    product_to_twa(kripkecube<State, SuccIterator>& sys,
                   twacube_ptr twa)
      : intersect<State, SuccIterator, StateHash, StateEqual,
                  product_to_twa<State, SuccIterator,
                                 StateHash, StateEqual>>(sys, twa)
      {
      }

    ~product_to_twa()
      {
      }

    twa_graph_ptr twa()
    {
      res_->set_named_prop<std::vector<std::string>>("state-names", names_);
      return res_;
    }

    void setup()
    {
      auto d = spot::make_bdd_dict();
      res_ = make_twa_graph(d);
      names_ = new std::vector<std::string>();

      int i = 0;
      for (auto ap : this->twa_->ap())
        {
          auto idx = res_->register_ap(ap);
          reverse_binder_[i++] = idx;
        }
    }

    bool push_state(product_state s, unsigned i, acc_cond::mark_t)
    {
      // push also implies edge (when it's not the initial state)
      if (this->todo.size())
        {
          auto c = this->twa_->get_cubeset()
            .intersection(this->twa_->trans_data
                          (this->todo.back().it_prop).cube_,
                          this->todo.back().it_kripke->condition());

          bdd x = spot::cube_to_bdd(c, this->twa_->get_cubeset(),
                                    reverse_binder_);
          this->twa_->get_cubeset().release(c);
          res_->new_edge(this->map[this->todo.back().st]-1, i-1, x,
                         this->twa_->trans_data
                         (this->todo.back().it_prop).acc_);
        }

      unsigned st = res_->new_state();
      names_->push_back(this->sys_.to_string(s.st_kripke) +
                        ('*' + std::to_string(s.st_prop)));
      SPOT_ASSERT(st+1 == i);
      return true;
    }


    bool update(product_state, unsigned src,
                product_state, unsigned dst,
                acc_cond::mark_t cond)
    {
      auto c = this->twa_->get_cubeset()
        .intersection(this->twa_->trans_data
                      (this->todo.back().it_prop).cube_,
                      this->todo.back().it_kripke->condition());

      bdd x = spot::cube_to_bdd(c, this->twa_->get_cubeset(),
                                reverse_binder_);
      this->twa_->get_cubeset().release(c);
      res_->new_edge(src-1, dst-1, x, cond);
      return false;
    }

    // These callbacks are useless here
    bool counterexample_found()
    {
      return false;
    }

    std::string trace()
    {
      return "";
    }

    bool pop_state(product_state, unsigned, bool, product_state, unsigned)
    {
      return true;
    }

  private:
    spot::twa_graph_ptr res_;
    std::vector<std::string>* names_;
    std::unordered_map<int, int> reverse_binder_;
  };
}
