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
#include <spot/twa/twagraph.hh>

namespace spot
{
  namespace
  {
    // 'tae' stands for 'Two-Automaton Emptiness check'

    // A helper class to manage iterators
    template<bool is_explicit>
    class tae_iterator
    {};

    template<>
    class tae_iterator<true>
    {
    public:
      tae_iterator(const const_twa_graph_ptr& aut, const unsigned s)
      {
        auto out = aut->get_graph().out(s);
        begin_ = out.begin();
        end_ = out.end();
        it_ = begin_;
      }

      bool first()
      {
        it_ = begin_;
        return done();
      }

      bool done() const
      {
        return it_ == end_;
      }

      bool next()
      {
        ++it_;
        return !done();
      }

      acc_cond::mark_t acc() const
      {
        return it_->acc;
      }

      bdd cond() const
      {
        return it_->cond;
      }

      unsigned dst() const
      {
        return it_->dst;
      }

    private:
      twa_graph::graph_t::const_iterator begin_;
      twa_graph::graph_t::const_iterator it_;
      twa_graph::graph_t::const_iterator end_;
    };

    template<>
    class tae_iterator<false>
    {
    public:
      tae_iterator(const const_twa_ptr& aut, const state* s)
        : aut_(aut), it_(aut->succ_iter(s))
      {
        it_->first();
      }

      ~tae_iterator()
      {
        aut_->release_iter(it_);
      }

      bool first()
      {
        return it_->first();
      }

      bool done() const
      {
        return it_->done();
      }

      bool next()
      {
        return it_->next();
      }

      acc_cond::mark_t acc() const
      {
        return it_->acc();
      }

      bdd cond() const
      {
        return it_->cond();
      }

      const state* dst() const
      {
        return it_->dst();
      }

    private:
      const const_twa_ptr& aut_;
      twa_succ_iterator* it_;
    };

    // A helper class, templated over expliciteness. Defines the type of states
    // and iterators, and operations over them.
    template<bool is_explicit>
    struct tae_element {};

    template<>
    struct tae_element<true>
    {
      using aut_t = const const_twa_graph_ptr;
      using state_t = unsigned;
      using iterator_t = tae_iterator<true>;

      static
      bool state_equals(state_t l, state_t r)
      {
        return l == r;
      }

      static
      size_t state_hash(state_t s)
      {
        return wang32_hash(s);
      }

      static
      state_t init(aut_t a)
      {
        return a->get_init_state_number();
      }

      static void destroy(state_t) {}
    };

    template<>
    struct tae_element<false>
    {
      using aut_t = const const_twa_ptr;
      using state_t = const state*;
      using iterator_t = tae_iterator<false>;

      static
      bool state_equals(state_t l, state_t r)
      {
        return l->compare(r) == 0;
      }

      static
      size_t state_hash(state_t s)
      {
        return s->hash();
      }

      static
      state_t init(aut_t a)
      {
        return a->get_init_state();
      }

      static
      void destroy(state_t s)
      {
        s->destroy();
      }
    };

    // Represents a state in the product. Essentially a pair of states.
    template<bool exp_l, bool exp_r>
    class product_state
    {
    public:
      using state_l_t = typename tae_element<exp_l>::state_t;
      using state_r_t = typename tae_element<exp_r>::state_t;

      product_state() {}

      product_state(state_l_t left, state_r_t right)
        : left_(left), right_(right)
      {}

      bool operator==(const product_state<exp_l, exp_r>& other) const
      {
        return tae_element<exp_l>::state_equals(left_, other.left_)
            && tae_element<exp_r>::state_equals(right_, other.right_);
      }

      size_t hash() const
      {
        return wang32_hash(tae_element<exp_l>::state_hash(left_)
                         ^ tae_element<exp_r>::state_hash(right_));
      }

    private:
      state_l_t left_;
      state_r_t right_;
    };

    template<bool exp_l, bool exp_r>
    struct product_state_hash
    {
      size_t
      operator()(const product_state<exp_l, exp_r>& s) const
      {
        return s.hash();
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
    template<bool exp_l, bool exp_r>
    class product_iterator
    {
    public:
      using iterator_l_t = typename tae_element<exp_l>::iterator_t;
      using iterator_r_t = typename tae_element<exp_r>::iterator_t;

      product_iterator(typename tae_element<exp_l>::aut_t l_aut,
                       typename tae_element<exp_l>::state_t l_state,
                       typename tae_element<exp_r>::aut_t r_aut,
                       typename tae_element<exp_r>::state_t r_state)
        : left(l_aut, l_state), right(r_aut, r_state)
      {
        // This initialises the iterator to the first valid transition. However,
        // if the right iterator has no transition, next_non_false_ will fail.
        if (!done())
          next_non_false_();
      }

      bool step_()
      {
        if (left.next())
          return true;
        left.first();
        return right.next();
      }

      bool next_non_false_()
      {
        assert(!done());
        do
          {
            bdd l = left.cond();
            bdd r = right.cond();
            bdd cond = l & r;

            if (cond != bddfalse)
              return true;
          }
        while (step_());
        return false;
      }

      bool next()
      {
        return step_() && next_non_false_();
      }

      bool done() const
      {
        return right.done();
      }

      product_mark acc() const
      {
        return product_mark(left.acc(), right.acc());
      }

      iterator_l_t left;
      iterator_r_t right;
    };

    // Represents an SCC
    struct scc
    {
      scc(unsigned i) : index(i), condition() {}

      unsigned index;
      product_mark condition;
    };

    template<bool exp_l, bool exp_r>
    bool tae_impl(typename tae_element<exp_l>::aut_t& left,
                  typename tae_element<exp_r>::aut_t& right)
    {
      using p_state = product_state<exp_l, exp_r>;
      using p_iterator = product_iterator<exp_l, exp_r>;

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
      std::unordered_map<p_state, unsigned,
                         product_state_hash<exp_l, exp_r>> states;
      // the roots of our SCCs
      std::stack<scc> root;
      // states yet to explore
      std::stack<std::pair<p_state, p_iterator>> todo;
      // acceptance conditions between SCCs
      std::stack<product_mark> arc;
      // the depth-first-search stack. Used to mark nodes as dead when we pop
      // their SCC.
      std::stack<p_state> live;

      auto new_state =
        [&](typename tae_element<exp_l>::state_t left_state,
            typename tae_element<exp_r>::state_t right_state)
        {
          p_state x(left_state, right_state);
          auto p = states.emplace(x, 0);
          if (p.second)
            // This is a new state
            {
              p.first->second = num;
              p_iterator it(left, left_state, right, right_state);
              root.push(num++);
              todo.emplace(x, it);
              live.emplace(x);
            }
          else
            {
              tae_element<exp_l>::destroy(left_state);
              tae_element<exp_r>::destroy(right_state);
            }
          return p;
        };

      new_state(tae_element<exp_l>::init(left),
                tae_element<exp_r>::init(right));
      arc.emplace();

      while (!todo.empty())
        {
          auto& top = todo.top();

          p_iterator& succ = top.second;

          if (succ.done())
            // No more successors, backtrack
            {
              p_state curr = top.first;

              todo.pop();

              assert(!root.empty());
              if (root.top().index == states[curr])
                // We are backtracking the root of an SCC
                {
                  arc.pop();
                  p_state s;
                  // pop from live to find curr, mark as order 0, curr included
                  do
                    {
                      assert(!live.empty());
                      s = live.top();
                      states[s] = 0;
                      live.pop();
                    }
                  while (!(curr == s));

                  root.pop();
                }

              // End of the scope, succ dies, iterators are properly released

              continue;
            }

          auto p = new_state(succ.left.dst(), succ.right.dst());
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
              assert(!root.empty());
              assert(!arc.empty());
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
                // Iterators are properly released.
                todo.pop();
              return false;
            }
        } // end while (!todo.empty())

      // DFS ended and we haven't found any accepting SCCs: left and right's
      // languages do not intersect.
      return true;
    }
  }

  bool two_aut_ec(const const_twa_ptr& left,
                  const const_twa_ptr& right)
  {
    const_twa_graph_ptr l_g = std::dynamic_pointer_cast<const twa_graph>(left);
    const_twa_graph_ptr r_g = std::dynamic_pointer_cast<const twa_graph>(right);

    if (l_g)
    {
      if (r_g)
        return tae_impl<true, true>(l_g, r_g);
      else
        return tae_impl<true, false>(l_g, right);
    }
    else
    {
      if (r_g)
        return tae_impl<false, true>(left, r_g);
      else
        return tae_impl<false, false>(left, right);
    }
  }
}
