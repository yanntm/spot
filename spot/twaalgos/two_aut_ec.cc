// -*- coding: utf-8 -*-
// Copyright (C) 2017, 2018 Laboratoire de Recherche et Développement de
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

#include <algorithm>
#include <deque>
#include <stack>
#include <unordered_map>

#include <spot/twaalgos/two_aut_ec.hh>

#include <spot/kripke/kripke.hh>
#include <spot/misc/hash.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/emptiness.hh>

namespace spot
{
  namespace
  {
    // 'tae' stands for 'Two-Automaton Emptiness check'

    // The types of automata we optimize on
    enum tae_aut_type { EXPLICIT, OTF, KRIPKE };

    // The pairs of strengths we deal with: Strong-Strong, Weak-Strong, and
    // Weak-Weak
    enum tae_strength { STRONG, WEAK_L, WEAK };

    // A helper class to manage iterators
    template<tae_aut_type type>
    class tae_iterator
    {};

    template<>
    class tae_iterator<EXPLICIT>
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

    // The common part of OTF and KRIPKE templates of tae_iterator
    class tae_iterator_otf_common
    {
    public:
      tae_iterator_otf_common(const const_twa_ptr& aut, const state* s)
        : aut_(aut), it_(aut->succ_iter(s))
      {}

      ~tae_iterator_otf_common()
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

      const state* dst() const
      {
        return it_->dst();
      }

    protected:
      const const_twa_ptr& aut_;
      twa_succ_iterator* it_;
    };

    template<>
    class tae_iterator<OTF> : public tae_iterator_otf_common
    {
    public:
      tae_iterator(const const_twa_ptr& aut, const state* s)
        : tae_iterator_otf_common(aut, s)
      {
        it_->first();
      }

      bdd cond() const
      {
        return it_->cond();
      }
    };

    template<>
    class tae_iterator<KRIPKE> : public tae_iterator_otf_common
    {
    public:
      tae_iterator(const const_fair_kripke_ptr& aut, const state* s)
        : tae_iterator_otf_common(aut, s), cond_(bddfalse)
      {
        if (it_->first())
          cond_ = it_->cond();
      }

      bdd cond() const
      {
        return cond_;
      }

    private:
      bdd cond_;
    };

    // A helper class, templated over expliciteness. Defines the type of states
    // and iterators, and operations over them.
    template<tae_aut_type type>
    struct tae_element {};

    template<>
    struct tae_element<EXPLICIT>
    {
      using aut_t = const const_twa_graph_ptr;
      using state_t = unsigned;
      using iterator_t = tae_iterator<EXPLICIT>;

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

      static
      const state* get_state(aut_t a, state_t s)
      {
        return a->state_from_number(s);
      }
    };

    // The common part of OTF and KRIPKE templates of tae_element
    struct tae_element_otf_common
    {
      using state_t = const state*;

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
      state_t init(const const_twa_ptr a)
      {
        return a->get_init_state();
      }

      static
      void destroy(state_t s)
      {
        s->destroy();
      }

      static
      const state* get_state(const const_twa_ptr, state_t s)
      {
        return s;
      }
    };

    template<>
    struct tae_element<OTF> : public tae_element_otf_common
    {
      using aut_t = const const_twa_ptr;
      using iterator_t = tae_iterator<OTF>;
    };

    template<>
    struct tae_element<KRIPKE> : public tae_element_otf_common
    {
      using aut_t = const const_fair_kripke_ptr;
      using iterator_t = tae_iterator<KRIPKE>;
    };

    // Represents a state in the product. Essentially a pair of states.
    template<tae_aut_type aut_type_l, tae_aut_type aut_type_r>
    class product_state
    {
    public:
      using state_l_t = typename tae_element<aut_type_l>::state_t;
      using state_r_t = typename tae_element<aut_type_r>::state_t;

      product_state() {}

      product_state(state_l_t left, state_r_t right)
        : left_(left), right_(right)
      {}

      bool operator==(const product_state<aut_type_l, aut_type_r>& other) const
      {
        return tae_element<aut_type_l>::state_equals(left_, other.left_)
            && tae_element<aut_type_r>::state_equals(right_, other.right_);
      }

      size_t hash() const
      {
        return wang32_hash(tae_element<aut_type_l>::state_hash(left_)
                         ^ tae_element<aut_type_r>::state_hash(right_));
      }

      const state_l_t get_left() const
      {
        return left_;
      }

      const state_r_t get_right() const
      {
        return right_;
      }

      const state *get_left_state(typename tae_element<aut_type_l>::aut_t a)
        const
      {
        return tae_element<aut_type_l>::get_state(a, left_);
      }

      const state *get_right_state(typename tae_element<aut_type_r>::aut_t a)
        const
      {
        return tae_element<aut_type_r>::get_state(a, right_);
      }

      void destroy()
      {
        tae_element<aut_type_l>::destroy(left_);
        tae_element<aut_type_r>::destroy(right_);
      }

    private:
      state_l_t left_;
      state_r_t right_;
    };

    template<tae_aut_type aut_type_l, tae_aut_type aut_type_r>
    struct product_state_hash
    {
      size_t
      operator()(const product_state<aut_type_l, aut_type_r>& s) const
      {
        return s.hash();
      }
    };

    // A map from a product_state to its order
    template<tae_aut_type aut_type_l, tae_aut_type aut_type_r>
    using order_map =
      std::unordered_map<product_state<aut_type_l, aut_type_r>, unsigned,
                         product_state_hash<aut_type_l, aut_type_r>>;

    // An acceptance mark in the product
    template<tae_strength strength>
    struct product_mark {};

    // The algorithm manipulates only STRONG marks, which hold both marks, but
    // the data structures store any type. So we need to define, in that order:
    //  - every mark,
    //  - STRONG-to-any conversions, through their constructors, for when we
    //    store a STRONG mark in a data structure,
    //  - operations (just |= actually) on STRONG marks with any other type of
    //    mark.

    template<>
    struct product_mark<STRONG>
    {
      product_mark() : left{}, right{}
      {}

      product_mark(acc_cond::mark_t l, acc_cond::mark_t r)
        : left(l), right(r)
      {}

      product_mark(const product_mark<STRONG>& p_m)
        : left(p_m.left), right(p_m.right)
      {}

      product_mark& operator|=(const product_mark<STRONG>& o)
      {
        left |= o.left;
        right |= o.right;
        return *this;
      }

      product_mark& operator|=(const product_mark<WEAK_L>& o);
      product_mark& operator|=(const product_mark<WEAK>&);

      acc_cond::mark_t left;
      acc_cond::mark_t right;
    };

    template<>
    struct product_mark<WEAK_L>
    {
      product_mark() : mark{}
      {}

      product_mark(const product_mark<STRONG>& p_m)
        : mark(p_m.right)
      {}

      acc_cond::mark_t mark;
    };

    template<>
    struct product_mark<WEAK>
    {
      product_mark() {}
      product_mark(const product_mark<STRONG>&) {}
    };

    product_mark<STRONG>&
    product_mark<STRONG>::operator|=(const product_mark<WEAK_L>& o)
    {
      right |= o.mark;
      return *this;
    }

    product_mark<STRONG>&
    product_mark<STRONG>::operator|=(const product_mark<WEAK>&)
    {
      return *this;
    }

    // A pseudo iterator to iterate over successors of a state in the product
    template<tae_aut_type aut_type_l, tae_aut_type aut_type_r>
    class product_iterator
    {
    public:
      using iterator_l_t = typename tae_element<aut_type_l>::iterator_t;
      using iterator_r_t = typename tae_element<aut_type_r>::iterator_t;

      product_iterator(typename tae_element<aut_type_l>::aut_t l_aut,
                       typename tae_element<aut_type_l>::state_t l_state,
                       typename tae_element<aut_type_r>::aut_t r_aut,
                       typename tae_element<aut_type_r>::state_t r_state)
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

      product_mark<STRONG> acc() const
      {
        return product_mark<STRONG>(left.acc(), right.acc());
      }

      iterator_l_t left;
      iterator_r_t right;
    };

    // Represents an SCC
    template<tae_strength strength>
    struct scc
    {
      scc() : index(0), condition() {}

      scc(unsigned i) : index(i), condition() {}

      unsigned index;
      product_mark<strength> condition;
    };

    template<tae_aut_type aut_type_l, tae_aut_type aut_type_r>
    void tae_run(const typename tae_element<aut_type_l>::aut_t& left,
                 const typename tae_element<aut_type_r>::aut_t& right,
                 const twa_run_ptr ar_l, const twa_run_ptr ar_r,
                 const order_map<aut_type_l, aut_type_r>& states,
                 const unsigned order, product_mark<STRONG> acc)
    {
      using p_state = product_state<aut_type_l, aut_type_r>;
      using p_iterator = product_iterator<aut_type_l, aut_type_r>;

      if (!(ar_l || ar_r))
        return;

#define CHECK_LR(TODO_LEFT, TODO_RIGHT) \
  if (ar_l)                             \
    TODO_LEFT;                          \
  if (ar_r)                             \
    TODO_RIGHT;

#define CHECK_BOTH_ON(TODO) \
  if (ar_l)                 \
    ar_l-> TODO;            \
  if (ar_r)                 \
    ar_r-> TODO;

#define CHECK_LR_ON(TODO_LEFT, TODO_RIGHT) \
  if (ar_l)                             \
    ar_l-> TODO_LEFT;                   \
  if (ar_r)                             \
    ar_r-> TODO_RIGHT;

      // ##### Run setup #####

      CHECK_BOTH_ON(prefix.clear())
      CHECK_BOTH_ON(cycle.clear())
      CHECK_LR_ON(aut = left, aut = right)

      // ##### BFS setup #####

      struct p_step
      {
        p_state src;
        struct { bdd left, right; } cond;
        product_mark<STRONG> acc;
      };

      // An adaptation of spot::bfs_steps
      auto product_bfs_steps =
        [&](const p_state& start, auto match, auto filter,
            twa_run::steps *l_steps, twa_run::steps *r_steps)
        {
          // Map a state to the breadth first discovered step that lead to it.
          std::unordered_map<p_state, p_step,
                             product_state_hash<aut_type_l, aut_type_r>>
                               backlinks;

          std::deque<p_state> bfs_queue;

          bfs_queue.push_back(start);

          /*
          ** std::unsigned_map::operator[] is not const because it also provides
          ** assignation, so we use at() instead.
          */
          unsigned start_order = states.at(start);

          while (!bfs_queue.empty())
            {
              p_state& src_state = bfs_queue.front();

              p_iterator src_out(left, src_state.get_left(),
                                 right, src_state.get_right());

              do
                {
                  p_state dst_state(src_out.left.dst(), src_out.right.dst());

                  auto i = states.find(dst_state);

                  if (i == states.end() // State not visited
                      || i->second == 0 // State marked as dead
                      || filter(dst_state))
                    {
                      dst_state.destroy();
                      continue;
                    }

                  p_step current = {src_state,
                                    {src_out.left.cond(), src_out.right.cond()},
                                    {src_out.left.acc(), src_out.right.acc()}};

                  if (match(current, dst_state))
                    {
                      twa_run::steps steps_l, steps_r;

                      for (;;)
                        {
                          CHECK_LR(
                            steps_l.emplace_front(
                              current.src.get_left_state(left)->clone(),
                              current.cond.left,
                              current.acc.left),
                            steps_r.emplace_front(
                              current.src.get_right_state(right)->clone(),
                              current.cond.right,
                              current.acc.right))
                          if (states.at(current.src) == start_order)
                            break;
                          const auto& j = backlinks.find(current.src);
                          assert(j != backlinks.end());
                          current = j->second;
                        }

                      if (l_steps)
                        l_steps->splice(l_steps->end(), steps_l);
                      if (r_steps)
                        r_steps->splice(r_steps->end(), steps_r);

                      for (auto& j : bfs_queue)
                        j.destroy();

                      for (auto& j : backlinks)
                        j.second.src.destroy();

                      return dst_state;
                    }

                  if (backlinks.emplace(dst_state, current).second)
                    bfs_queue.push_back(dst_state);
                }
              while (src_out.next());

              src_state.destroy();
              bfs_queue.pop_front();
            }

          return start;
        };

      // ##### Prefix search V3 #####

      p_state init(tae_element<aut_type_l>::init(left),
                   tae_element<aut_type_r>::init(right));

      p_state substart =
        product_bfs_steps(init,
                          [&](p_step&, p_state& dest)
                          {
                            return states.at(dest) == order;
                          },
                          [&](p_state&)
                          {
                            return false; // Do not filter states
                          },
                          ar_l ? &(ar_l->prefix) : nullptr,
                          ar_r ? &(ar_r->prefix) : nullptr);

      // ##### Accepting Marks search #####

      /*
      ** Look for unseen marks : once one is found, register the path to it, and
      ** restart a new BFS, until all marks were seen.
      */
      while (acc.left | acc.right)
        substart =
          product_bfs_steps(substart,
                            [&](p_step& step, p_state&)
                            {
                              if ((acc.left & step.acc.left)
                                  || (acc.right & step.acc.right))
                                {
                                  acc.left -= step.acc.left;
                                  acc.right -= step.acc.right;
                                  return true;
                                }
                              return false;
                            },
                            [&](p_state& dest)
                            {
                              // Stay in accepting SCC
                              return states.at(dest) < order;
                            },
                            ar_l ? &(ar_l->cycle) : nullptr,
                            ar_r ? &(ar_r->cycle) : nullptr);


      // Return to cycle start
      product_bfs_steps(substart,
                        [&](p_step&, p_state& dest)
                        {
                          return states.at(dest) == order;
                        },
                        [&](p_state& dest)
                        {
                          // Stay in accepting SCC
                          return states.at(dest) < order;
                        },
                        ar_l ? &(ar_l->cycle) : nullptr,
                        ar_r ? &(ar_r->cycle) : nullptr);
    }

    // The result of an accepting run.
    template<tae_aut_type aut_type_l, tae_aut_type aut_type_r>
    class tae_res : public two_aut_res
    {
    public:
      tae_res(typename tae_element<aut_type_l>::aut_t& left,
              typename tae_element<aut_type_r>::aut_t& right, bool swapped)
        : two_aut_res(), left_(left), right_(right), swapped_(swapped)
      {
      }

      ~tae_res() override
      {
      }

      void accepting_scc(unsigned order, product_mark<STRONG> acc)
      {
        accepting_.index = order;
        accepting_.condition = acc;
      }

      std::pair<twa_run_ptr, twa_run_ptr> accepting_runs() const override
      {
        twa_run_ptr ar_l = std::make_shared<twa_run>(left_);
        twa_run_ptr ar_r = std::make_shared<twa_run>(right_);

        tae_run(left_, right_, ar_l, ar_r, states,
                accepting_.index, accepting_.condition);

        if (swapped_)
          return std::make_pair(ar_r, ar_l);
        return std::make_pair(ar_l, ar_r);
      }

      twa_run_ptr left_accepting_run() const override
      {
        twa_run_ptr ar = std::make_shared<twa_run>(left_);

        if (!swapped_)
          tae_run(left_, right_, ar, nullptr, states,
                  accepting_.index, accepting_.condition);
        else
          tae_run(left_, right_, nullptr, ar, states,
                  accepting_.index, accepting_.condition);

        return ar;
      }

      twa_run_ptr right_accepting_run() const override
      {
        twa_run_ptr ar = std::make_shared<twa_run>(right_);

        if (!swapped_)
          tae_run(left_, right_, nullptr, ar, states,
                  accepting_.index, accepting_.condition);
        else
          tae_run(left_, right_, ar, nullptr, states,
                  accepting_.index, accepting_.condition);

        return ar;
      }

      // `Hash` in Couvreur's algorithm.
      order_map<aut_type_l, aut_type_r> states;

    private:
      typename tae_element<aut_type_l>::aut_t left_;
      typename tae_element<aut_type_r>::aut_t right_;
      scc<STRONG> accepting_;
      bool swapped_;
    };

    template<tae_aut_type aut_type_l, tae_aut_type aut_type_r,
             tae_strength strength>
    two_aut_res_ptr
    tae_impl(typename tae_element<aut_type_l>::aut_t& left,
             typename tae_element<aut_type_r>::aut_t& right, bool swapped)
    {
      using p_state = product_state<aut_type_l, aut_type_r>;
      using p_iterator = product_iterator<aut_type_l, aut_type_r>;
      using p_mark = product_mark<strength>;

      if (left->get_dict() != right->get_dict())
        throw std::runtime_error("two_aut_ec: left and right automata should "
                                 "share their bdd_dict");

      if (left->acc().uses_fin_acceptance()
          || right->acc().uses_fin_acceptance())
        throw std::runtime_error
          ("Fin acceptance is not supported by two_aut_ec()");

      if (left->acc().is_f() || right->acc().is_f())
        //Resulting acceptance is false
        return nullptr;

      // number of visited nodes ; order/id of the next node to be created
      unsigned num = 1;
      // the result of the two-automaton emptiness check. Contains the `Hash`
      // structure of Couvreur's algorithm.
      auto res = std::make_shared<tae_res<aut_type_l, aut_type_r>>(left, right,
                                                                   swapped);
      // the roots of our SCCs
      std::stack<scc<strength>> root;
      // states yet to explore
      std::stack<std::pair<p_state, p_iterator>> todo;
      // acceptance conditions between SCCs
      std::stack<p_mark> arc;
      // the depth-first-search stack. Used to mark nodes as dead when we pop
      // their SCC. Used as a stack by Couvreur, but as a list by the run
      // search.
      std::deque<p_state> live;

      auto new_state =
        [&](typename tae_element<aut_type_l>::state_t left_state,
            typename tae_element<aut_type_r>::state_t right_state)
        {
          p_state x(left_state, right_state);
          auto p = res->states.emplace(x, 0);
          if (p.second)
            // This is a new state
            {
              p.first->second = num;
              p_iterator it(left, left_state, right, right_state);
              root.push(num++);
              todo.emplace(x, it);
              live.emplace_back(x);
            }
          else
            {
              x.destroy();
            }
          return p;
        };

      new_state(tae_element<aut_type_l>::init(left),
                tae_element<aut_type_r>::init(right));

      if (strength != WEAK)
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
              if (root.top().index == res->states[curr])
                // We are backtracking the root of an SCC
                {
                  if (strength != WEAK)
                    {
                      assert(!arc.empty());
                      arc.pop();
                    }
                  p_state s;
                  // pop from live to find curr, mark as order 0, curr included
                  do
                    {
                      assert(!live.empty());
                      s = live.back();
                      res->states[s] = 0;
                      live.pop_back();
                    }
                  while (!(curr == s));

                  root.pop();
                }

              // End of the scope, succ dies, iterators are properly released

              continue;
            }

          auto p = new_state(succ.left.dst(), succ.right.dst());
          auto acc = succ.acc();
          succ.next();

          if (p.second)
            // This is a new state
            {
              if (strength != WEAK)
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
              if (strength != WEAK)
                {
                  assert(!arc.empty());
                  acc |= root.top().condition;
                  acc |= arc.top();
                  arc.pop();
                }
              root.pop();
            }
          acc |= root.top().condition;
          root.top().condition = acc;

          if (left->acc().accepting(acc.left)
              && right->acc().accepting(acc.right))
            // This SCC is accepting
            {
              while (!todo.empty())
                // Iterators are properly released.
                todo.pop();

              res->accepting_scc(root.top().index, acc);
              return std::static_pointer_cast<two_aut_res>(res);
            }
        } // end while (!todo.empty())

      // DFS ended and we haven't found any accepting SCCs: left and right's
      // languages do not intersect.
      return nullptr;
    }

    template<tae_aut_type aut_type_l, tae_aut_type aut_type_r>
    two_aut_res_ptr
    tae_dispatch_strength(typename tae_element<aut_type_l>::aut_t& left,
                          typename tae_element<aut_type_r>::aut_t& right,
                          bool swapped)
    {
      auto l = left->prop_weak();
      auto r = right->prop_weak();

      if (!l && r && aut_type_l != KRIPKE)
        // We save on templates by only having weak automata on the left. This
        // is not compatible with, and less significant than, the Kripke swap.
        return tae_impl<aut_type_r, aut_type_l, WEAK_L>(right, left, !swapped);

      if (l && r)
        return tae_impl<aut_type_l, aut_type_r, WEAK>(left, right, swapped);

      if (l && !r)
        return tae_impl<aut_type_l, aut_type_r, WEAK_L>(left, right, swapped);

      return tae_impl<aut_type_l, aut_type_r, STRONG>(left, right, swapped);
    }

    two_aut_res_ptr
    tae_dispatch_type(const const_twa_ptr& left, const const_twa_ptr& right,
                      bool swapped)
    {
      const_fair_kripke_ptr l_k = std::dynamic_pointer_cast<const fair_kripke>
                                                           (left);
      const_fair_kripke_ptr r_k = std::dynamic_pointer_cast<const fair_kripke>
                                                           (right);

      // We don't often check Kripke against Kripke, so we save on templates by
      // only having Kripke structures on the left.
      if (r_k && !l_k)
        return tae_dispatch_type(right, left, !swapped);

      const_twa_graph_ptr l_e =
        std::dynamic_pointer_cast<const twa_graph>(left);
      const_twa_graph_ptr r_e =
        std::dynamic_pointer_cast<const twa_graph>(right);

      if (l_k)
        {
          if (r_e)
            return tae_dispatch_strength<KRIPKE, EXPLICIT>(l_k, r_e, swapped);
          else
            return tae_dispatch_strength<KRIPKE, OTF>(l_k, right, swapped);
        }

      if (l_e)
        {
          if (r_e)
            return tae_dispatch_strength<EXPLICIT, EXPLICIT>(l_e, r_e, swapped);
          else
            return tae_dispatch_strength<EXPLICIT, OTF>(l_e, right, swapped);
        }
      else
        {
          if (r_e)
            return tae_dispatch_strength<OTF, EXPLICIT>(left, r_e, swapped);
          else
            return tae_dispatch_strength<OTF, OTF>(left, right, swapped);
        }
    }
  } // namespace

  two_aut_res_ptr
  two_aut_ec(const const_twa_ptr& left, const const_twa_ptr& right)
  {
    return tae_dispatch_type(left, right, false);
  }
}
