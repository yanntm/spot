// -*- coding: utf-8 -*-
// Copyright (C) 2014-2018 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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
#include <spot/twaalgos/product.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/complete.hh>
#include <deque>
#include <unordered_map>
#include <spot/misc/hash.hh>

namespace spot
{
  namespace
  {
    typedef std::pair<unsigned, unsigned> product_state;

    struct product_state_hash
    {
      size_t
      operator()(product_state s) const noexcept
      {
        return wang32_hash(s.first ^ wang32_hash(s.second));
      }
    };


    template<typename T>
    static
    void product_main(const const_twa_graph_ptr& left,
                      const const_twa_graph_ptr& right,
                      unsigned left_state,
                      unsigned right_state,
                      twa_graph_ptr res, T merge_acc)
    {
      std::unordered_map<product_state, unsigned, product_state_hash> s2n;
      std::deque<std::pair<product_state, unsigned>> todo;

      auto v = new product_states;
      res->set_named_prop("product-states", v);

      auto new_state =
        [&](unsigned left_state, unsigned right_state) -> unsigned
        {
          product_state x(left_state, right_state);
          auto p = s2n.emplace(x, 0);
          if (p.second)                // This is a new state
            {
              p.first->second = res->new_state();
              todo.emplace_back(x, p.first->second);
              assert(p.first->second == v->size());
              v->emplace_back(x);
            }
          return p.first->second;
        };

      res->set_init_state(new_state(left_state, right_state));
      if (res->acc().is_f())
        // Do not bother doing any work if the resulting acceptance is
        // false.
        return;
      while (!todo.empty())
        {
          auto top = todo.front();
          todo.pop_front();
          for (auto& l: left->out(top.first.first))
            for (auto& r: right->out(top.first.second))
              {
                auto cond = l.cond & r.cond;
                if (cond == bddfalse)
                  continue;
                auto dst = new_state(l.dst, r.dst);
                res->new_edge(top.second, dst, cond,
                              merge_acc(l.acc, r.acc));
                // If right is deterministic, we can abort immediately!
              }
        }
    }

    static
    twa_graph_ptr product_aux(const const_twa_graph_ptr& left,
                              const const_twa_graph_ptr& right,
                              unsigned left_state,
                              unsigned right_state,
                              bool and_acc)
    {
      if (SPOT_UNLIKELY(!(left->is_existential() && right->is_existential())))
        throw std::runtime_error
          ("product() does not support alternating automata");
      if (SPOT_UNLIKELY(left->get_dict() != right->get_dict()))
        throw std::runtime_error("product: left and right automata should "
                                 "share their bdd_dict");

      auto res = make_twa_graph(left->get_dict());
      res->copy_ap_of(left);
      res->copy_ap_of(right);

      bool leftweak = left->prop_weak().is_true();
      bool rightweak = right->prop_weak().is_true();
      // We have optimization to the standard product in case one
      // of the arguments is weak.  However these optimizations
      // are pointless if the said arguments are "t" or "f".
      if ((leftweak || rightweak)
          && (left->num_sets() > 0) && (right->num_sets() > 0))
        {
          // If both automata are weak, we can restrict the result to
          // Büchi or co-Büchi.  We will favor Büchi unless the two
          // operands are co-Büchi.
          if (leftweak && rightweak)
            {
            weak_weak:
              acc_cond::mark_t accmark = {0};
              acc_cond::mark_t rejmark = {};
              if (left->acc().is_co_buchi() && right->acc().is_co_buchi())
                {
                  res->set_co_buchi();
                  std::swap(accmark, rejmark);
                }
              else
                {
                  res->set_buchi();
                }
              res->prop_weak(true);
              auto& lacc = left->acc();
              auto& racc = right->acc();
              if (and_acc)
                product_main(left, right, left_state, right_state, res,
                             [&] (acc_cond::mark_t ml, acc_cond::mark_t mr)
                             {
                               if (lacc.accepting(ml) && racc.accepting(mr))
                                 return accmark;
                               else
                                 return rejmark;
                             });
              else
                product_main(left, right, left_state, right_state, res,
                             [&] (acc_cond::mark_t ml, acc_cond::mark_t mr)
                             {
                               if (lacc.accepting(ml) || racc.accepting(mr))
                                 return accmark;
                               else
                                 return rejmark;
                             });
            }
          else if (!rightweak)
            {
              if (and_acc)
                {
                  auto rightunsatmark = right->acc().unsat_mark();
                  if (!rightunsatmark.first)
                    {
                      // Left is weak.  Right was not weak, but it is
                      // always accepting.  We can therefore pretend
                      // that right is weak.
                      goto weak_weak;
                    }
                  res->copy_acceptance_of(right);
                  acc_cond::mark_t rejmark = rightunsatmark.second;
                  auto& lacc = left->acc();
                  product_main(left, right, left_state, right_state, res,
                               [&] (acc_cond::mark_t ml, acc_cond::mark_t mr)
                               {
                                 if (lacc.accepting(ml))
                                   return mr;
                                 else
                                   return rejmark;
                               });
                }
              else
                {
                  auto rightsatmark = right->acc().sat_mark();
                  if (!rightsatmark.first)
                    {
                      // Left is weak.  Right was not weak, but it is
                      // always rejecting.  We can therefore pretend
                      // that right is weak.
                      goto weak_weak;
                    }
                  res->copy_acceptance_of(right);
                  acc_cond::mark_t accmark = rightsatmark.second;
                  auto& lacc = left->acc();
                  product_main(left, right, left_state, right_state, res,
                               [&] (acc_cond::mark_t ml, acc_cond::mark_t mr)
                               {
                                 if (!lacc.accepting(ml))
                                   return mr;
                                 else
                                   return accmark;
                               });

                }
            }
          else
            {
              assert(!leftweak);
              if (and_acc)
                {
                  auto leftunsatmark = left->acc().unsat_mark();
                  if (!leftunsatmark.first)
                    {
                      // Right is weak.  Left was not weak, but it is
                      // always accepting.  We can therefore pretend
                      // that left is weak.
                      goto weak_weak;
                    }
                  res->copy_acceptance_of(left);
                  acc_cond::mark_t rejmark = leftunsatmark.second;
                  auto& racc = right->acc();
                  product_main(left, right, left_state, right_state, res,
                               [&] (acc_cond::mark_t ml, acc_cond::mark_t mr)
                               {
                                 if (racc.accepting(mr))
                                   return ml;
                                 else
                                   return rejmark;
                               });
                }
              else
                {
                  auto leftsatmark = left->acc().sat_mark();
                  if (!leftsatmark.first)
                    {
                      // Right is weak.  Left was not weak, but it is
                      // always rejecting.  We can therefore pretend
                      // that left is weak.
                      goto weak_weak;
                    }
                  res->copy_acceptance_of(left);
                  acc_cond::mark_t accmark = leftsatmark.second;
                  auto& racc = right->acc();
                  product_main(left, right, left_state, right_state, res,
                               [&] (acc_cond::mark_t ml, acc_cond::mark_t mr)
                               {
                                 if (!racc.accepting(mr))
                                   return ml;
                                 else
                                   return accmark;
                               });

                }
            }
        }
      else // general case
        {
          auto left_num = left->num_sets();
          auto right_acc = right->get_acceptance() << left_num;
          if (and_acc)
            right_acc &= left->get_acceptance();
          else
            right_acc |= left->get_acceptance();
          res->set_acceptance(left_num + right->num_sets(), right_acc);

          product_main(left, right, left_state, right_state, res,
                       [&] (acc_cond::mark_t ml, acc_cond::mark_t mr)
                       {
                         return ml | (mr << left_num);
                       });

        }

      // The product of two non-deterministic automata could be
      // deterministic.  Likewise for non-complete automata.
      if (left->prop_universal() && right->prop_universal())
        res->prop_universal(true);
      if (left->prop_complete() && right->prop_complete())
        res->prop_complete(true);
      if (left->prop_stutter_invariant() && right->prop_stutter_invariant())
        res->prop_stutter_invariant(true);
      if (left->prop_inherently_weak() && right->prop_inherently_weak())
        res->prop_inherently_weak(true);
      if (left->prop_weak() && right->prop_weak())
        res->prop_weak(true);
      if (left->prop_terminal() && right->prop_terminal())
        res->prop_terminal(true);
      res->prop_state_acc(left->prop_state_acc() && right->prop_state_acc());
      return res;
    }
  }

  twa_graph_ptr product(const const_twa_graph_ptr& left,
                        const const_twa_graph_ptr& right,
                        unsigned left_state,
                        unsigned right_state)
  {
    return product_aux(left, right, left_state, right_state, true);
  }

  twa_graph_ptr product(const const_twa_graph_ptr& left,
                        const const_twa_graph_ptr& right)
  {
    return product(left, right,
                   left->get_init_state_number(),
                   right->get_init_state_number());
  }

  twa_graph_ptr product_or(const const_twa_graph_ptr& left,
                           const const_twa_graph_ptr& right,
                           unsigned left_state,
                           unsigned right_state)
  {
    return product_aux(complete(left), complete(right),
                       left_state, right_state, false);
  }

  twa_graph_ptr product_or(const const_twa_graph_ptr& left,
                           const const_twa_graph_ptr& right)
  {
    return product_or(left, right,
                      left->get_init_state_number(),
                      right->get_init_state_number());
  }

}
