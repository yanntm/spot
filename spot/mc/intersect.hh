// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016, 2018, 2019 Laboratoire de Recherche et
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

#include <spot/kripke/kripke.hh>
#include <spot/twacube/twacube.hh>
#include <queue>

namespace spot
{
  /// \brief Wrapper to accumulate results from intersection
  /// and emptiness checks
  struct SPOT_API istats
  {
    unsigned states;
    unsigned transitions;
    unsigned sccs;
    unsigned instack_sccs;
    unsigned instack_item;
    bool is_empty;
  };

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

          forward_iterators(true);
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
                  sys_.recycle_iterator(todo.back().it_kripke, tid_);
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
              forward_iterators(false);
              auto it  = map.find(dst);
              if (it == map.end())
                {
                  if (SPOT_LIKELY(self().push_state(dst, dfs_number+1, acc)))
                    {
                      map[dst] = ++dfs_number;
                      todo.push_back({dst, sys_.succ(dst.st_kripke, tid_),
                            twa_->succ(dst.st_prop)});
                      forward_iterators(true);
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

    virtual istats stats()
    {
      return {dfs_number, transitions, 0U, 0U, 0U, false};
    }

  protected:

    /// \brief Find the first couple of iterator (from the top of the
    /// todo stack) that intersect. The \a parameter indicates wheter
    /// the state has just been pushed since the underlying job
    /// is slightly different.
    void forward_iterators(bool just_pushed)
    {
      SPOT_ASSERT(!todo.empty());
      SPOT_ASSERT(!(todo.back().it_prop->done() &&
                    todo.back().it_kripke->done()));

      // Sometimes kripke state may have no successors.
      if (todo.back().it_kripke->done())
        return;

      // The state has just been push and the 2 iterators intersect.
      // There is no need to move iterators forward.
      SPOT_ASSERT(!(todo.back().it_prop->done()));
      if (just_pushed && twa_->get_cubeset()
          .intersect(twa_->trans_data(todo.back().it_prop, tid_).cube_,
                     todo.back().it_kripke->condition()))
          return;

      // Otherwise we have to compute the next valid successor (if it exits).
      // This requires two loops. The most inner one is for the twacube since
      // its costless
      if (todo.back().it_prop->done())
        todo.back().it_prop->reset();
      else
        todo.back().it_prop->next();

      while (!todo.back().it_kripke->done())
        {
          while (!todo.back().it_prop->done())
            {
              if (SPOT_UNLIKELY(twa_->get_cubeset()
                .intersect(twa_->trans_data(todo.back().it_prop, tid_).cube_,
                           todo.back().it_kripke->condition())))
                return;
              todo.back().it_prop->next();
            }
          todo.back().it_prop->reset();
          todo.back().it_kripke->next();
        }
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
}
