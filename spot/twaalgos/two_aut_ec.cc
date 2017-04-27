// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
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

#include "config.h"

#include <stack>
#include <unordered_map>

#include <spot/twaalgos/two_aut_ec.hh>

#include <spot/misc/hash.hh>
#include <spot/twa/twa.hh>

namespace spot
{
  namespace
  {
    // Represents a state in the product. Essentially a pair of states.
    class product_state
    {
    public:
      const state* left;
      const state* right;

      product_state() {}

      product_state(const state* left, const state* right)
        : left(left), right(right)
      {}

      bool operator==(const product_state& other) const
      {
        return left->compare(other.left) == 0
            && right->compare(other.right) == 0;
      }
    };

    struct product_state_hash
    {
      size_t
      operator()(const product_state& s) const
      {
        return wang32_hash(s.left->hash()) ^ wang32_hash(s.right->hash());
      }
    };

    // An acceptance mark in the product
    class product_mark
    {
    public:
      product_mark() : left{}, right{}
      {}

      product_mark(acc_cond::mark_t l, acc_cond::mark_t r)
        : left(l), right(r)
      {}

      product_mark& operator|=(const product_mark& o)
      {
        left |= o.left;
        right |= o.right;
        return *this;
      }

      acc_cond::mark_t left;
      acc_cond::mark_t right;
    };

    // A pseudo iterator to iterate over successors of a state in the product
    class product_state_iterator
    {
    public:
      product_state_iterator(twa_succ_iterator* l, twa_succ_iterator* r)
        : left(l), right(r)
      {
        if (!(left->first() && right->first()))
          // If we cant' iterate, invalidate this iterator
          {
            delete right;
            right = nullptr;
          }
      }

      bool step_()
      {
        if (left->next())
          return true;
        left->first();
        return right->next();
      }

      bool next()
      {
        assert(!done());
        while (step_())
          {
            bdd l = left->cond();
            bdd r = right->cond();
            bdd cond = l & r;

            if (cond != bddfalse)
              return true;
          }
        return false;
      }

      bool done() const
      {
        return !right || right->done();
      }

      product_mark acc() const
      {
        return product_mark(left->acc(), right->acc());
      }

      twa_succ_iterator* left;
      twa_succ_iterator* right;
    };

    // Represents an SCC
    struct scc
    {
      scc(unsigned i) : index(i), condition() {}

      unsigned index;
      product_mark condition;
    };
  }

  bool two_aut_ec(const const_twa_ptr& left,
                  const const_twa_ptr& right)
  {
    if (left->get_dict() != right->get_dict())
      throw std::runtime_error("two_aut_ec: left and right automata should "
                               "share their bdd_dict");

    if (left->acc().uses_fin_acceptance()
        || right->acc().uses_fin_acceptance())
      throw std::runtime_error
        ("Fin acceptance is not supported by two_aut_ec()");

    if (left->acc().is_f() || right->acc().is_f())
      //Resulting acceptance is false
      return true;

    // number of visited nodes ; order/id of the next node to be created
    unsigned num = 1;
    // states of the product and their order (Hash in Couvreur's algorithm)
    std::unordered_map<product_state, unsigned, product_state_hash> states;
    // the roots of our SCCs
    std::stack<scc> root;
    // states yet to explore
    std::stack<std::pair<product_state, product_state_iterator>> todo;
    // acceptance conditions between SCCs
    std::stack<product_mark> arc;
    // the depth-first-search stack. Used to mark nodes as dead when we pop
    // their SCC.
    std::stack<product_state> live;

    auto new_state =
      [&](const state* left_state, const state* right_state)
      {
        product_state x(left_state, right_state);
        auto p = states.emplace(x, 0);
        if (p.second)
          // This is a new state
          {
            p.first->second = num;
            product_state_iterator it(left->succ_iter(left_state),
                                      right->succ_iter(right_state));
            root.push(num++);
            todo.emplace(x, it);
            live.emplace(x);
          }
        else
          {
            left_state->destroy();
            right_state->destroy();
          }
        return p;
      };

    new_state(left->get_init_state(), right->get_init_state());
    arc.emplace();

    while (!todo.empty())
      {
        auto top = todo.top();

        product_state_iterator& succ = top.second;

        if (succ.done())
          // No more successors, backtrack
          {
            product_state curr = top.first;

            todo.pop();

            if (root.top().index == states[curr])
              // We are backtracking the root of an SCC
              {
                arc.pop();
                product_state s;
                // pop from live to find curr, mark as order 0, curr included
                do
                  {
                    s = live.top();
                    states[s] = 0;
                    live.pop();
                  }
                while (!(curr == s));

                root.pop();
              }

            left->release_iter(succ.left);
            right->release_iter(succ.right);

            continue;
          }

        auto p = new_state(succ.left->dst(), succ.right->dst());
        product_mark acc = succ.acc();
        succ.next();

        if (p.second)
          // This is a new state
          {
            arc.push(acc);
            continue;
          }

        if (p.first->second == 0)
          // This state is dead
          continue;

        // This is not a new nor a dead state : we have already visited it in
        // our depth-first search, it is part of the same SCC as the current
        // state.

        // The order of the destination state
        unsigned threshold = p.first->second;

        // Merge all SCCs visited after the destination
        while (threshold < root.top().index)
          {
            acc |= root.top().condition;
            acc |= arc.top();
            root.pop();
            arc.pop();
          }
        root.top().condition |= acc;

        if (left->acc().accepting(root.top().condition.left)
            && right->acc().accepting(root.top().condition.right))
          // This SCC is accepting
          {
            while (!todo.empty())
              {
                left->release_iter(todo.top().second.left);
                right->release_iter(todo.top().second.right);
                todo.pop();
              }
            return false;
          }
      } // end while (!todo.empty())

    return true;
  }
}
