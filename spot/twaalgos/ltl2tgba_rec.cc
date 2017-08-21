// -*- coding: utf-8 -*-
// Copyright (C) 2013-2017 Laboratoire de Recherche et Développement
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

#include <spot/twaalgos/ltl2tgba_rec.hh>

#include <spot/tl/nenoform.hh>
#include <spot/tl/unabbrev.hh>
#include <spot/twa/formula2bdd.hh>
#include <spot/twaalgos/alternation.hh>
#include <spot/twaalgos/postproc.hh>
#include <spot/twaalgos/product.hh>
#include <spot/twaalgos/sum.hh>

#include <spot/twaalgos/toweak.hh>

namespace spot
{
  namespace
  {
    twa_graph_ptr
    make_ap(const formula& ap, const bdd_dict_ptr& dict)
    {
      if (SPOT_UNLIKELY(!ap.is_boolean()))
        throw std::runtime_error("not a Boolean formula");

      auto res = make_twa_graph(dict);
      bdd cond = formula_to_bdd(ap, res->get_dict(), res);
      res->register_aps_from_dict();

      unsigned init = res->new_state();
      res->set_init_state(init);

      // optimize for tt() and ff()
      if (cond == bddfalse)
        return res;

      if (cond == bddtrue)
        res->new_acc_edge(init, init, cond);
      else
        {
          unsigned sink = res->new_state();
          res->new_acc_edge(sink, sink, bddtrue);
          // NB: does not matter if acc or not
          res->new_acc_edge(init, sink, cond);
        }
      return res;
    }

    twa_graph_ptr
    make_next(const twa_graph_ptr& aut)
    {
      // TODO optimize for suspendable automata
      auto res = make_twa_graph(aut, twa::prop_set::all());
      int newinit = res->new_state();
      res->new_acc_edge(newinit, res->get_init_state_number(), bddtrue);
      res->set_init_state(newinit);
      return res;
    }

    twa_graph_ptr
    make_or(const twa_graph_ptr& lhs,
            const twa_graph_ptr& rhs)
    {
      auto res = sum(lhs, rhs);
      res->purge_unreachable_states();
      return res;
    }
    twa_graph_ptr
    make_and(const twa_graph_ptr& lhs,
             const twa_graph_ptr& rhs)
    {
      return product(lhs, rhs);
      //return remove_alternation_buchi(sum_and(lhs, rhs));
    }

    using state_num = twa_graph::state_num;

    // \param strict (true for U, false for W)
    // TODO optimize when both operands are weak
    // TODO handle alternating automata in input
    twa_graph_ptr
    _make_until(const twa_graph_ptr& lhs, const twa_graph_ptr& rhs,
                bool strict, bool remove_alt)
    {
      auto res = make_twa_graph(rhs, {});
      // TODO the following line should be unnecessary as lhs and rhs
      // should already use the same dict
      res->copy_ap_of(lhs);

      // add an initial state
      {
        state_num init = res->new_state();
        for (const auto& e : rhs->out(rhs->get_init_state_number()))
          res->new_edge(init, e.dst, e.cond, e.acc);
        res->set_init_state(init);
      }

      acc_cond::mark_t all_lhs = lhs->acc().all_sets();
      acc_cond::mark_t all_rhs = rhs->acc().all_sets() << all_lhs.count();
      // build the acceptance condition of the automaton
      unsigned num_sum = lhs->num_sets() + rhs->num_sets();
      auto promise = acc_cond::mark_t({ num_sum });
      {
        if (strict)
          res->set_generalized_buchi(num_sum + 1);
        else
          res->set_generalized_buchi(num_sum);

        // update acceptance marks of existing edges
        unsigned offset = all_lhs.count();
        for (auto& e : res->edges())
          {
            e.acc = all_lhs | (e.acc << offset);
            if (strict)
              e.acc |= promise;
          }
      }

      unsigned offset = res->num_states();
      res->new_states(lhs->num_states());
      state_num init = res->get_init_state_number();
      for (const auto& e : lhs->out(lhs->get_init_state_number()))
        res->new_univ_edge(init, {init, e.dst + offset}, e.cond,
                           all_lhs | all_rhs);
      if (strict)
        all_rhs |= promise;
      for (const auto& e : lhs->edges())
        res->new_edge(e.src + offset, e.dst + offset, e.cond, e.acc | all_rhs);

      if (remove_alt)
        return remove_alternation_buchi(res);
      else
        return res;
    }

  }

  // TODO make a general version for strict and non-strict (M and R)
  twa_graph_ptr
  make_release(const twa_graph_ptr& lhs, const twa_graph_ptr& rhs,
               bool strict, bool remove_alt)
  {
    auto res = make_twa_graph(lhs, {});
    // TODO the following line should be unnecessary as lhs and rhs
    // should already use the same dict
    res->copy_ap_of(rhs);

    // add an initial state
    {
      state_num init = res->new_state();
      res->set_init_state(init);
    }

    acc_cond::mark_t all_rhs = rhs->acc().all_sets();
    acc_cond::mark_t all_lhs = lhs->acc().all_sets() << all_rhs.count();
    // build the acceptance condition of the automaton
    unsigned num_sum = lhs->num_sets() + rhs->num_sets();
    //auto promise = acc_cond::mark_t({ num_sum });
    {
      if (strict)
        res->set_generalized_buchi(num_sum + 1);
      else
        res->set_generalized_buchi(num_sum);

      // update acceptance marks of existing edges
      unsigned offset = all_lhs.count();
      for (auto& e : res->edges())
        {
          e.acc = all_rhs | (e.acc << offset);
          // TODO not sure where to put the promise
          //if (strict)
          //  e.acc |= promise;
        }
    }

    unsigned offset = res->num_states();
    res->new_states(rhs->num_states());
    state_num init = res->get_init_state_number();
    for (const auto& e : rhs->out(rhs->get_init_state_number()))
      {
        res->new_univ_edge(init, {init, e.dst + offset}, e.cond,
                           all_lhs | all_rhs);
        // for the following transitions, the acceptance mark is irrelevant
        for (const auto& f : lhs->out(lhs->get_init_state_number()))
          res->new_univ_edge(init, { f.dst, e.dst + offset }, e.cond & f.cond,
                             0U);
      }

    // TODO not sure where to put the promise
    //if (strict)
    //  all_rhs |= promise;
    for (const auto& e : rhs->edges())
      res->new_edge(e.src + offset, e.dst + offset, e.cond, e.acc | all_lhs);

    if (remove_alt)
      return remove_alternation_buchi(res);
    else
      return res;
  }

  twa_graph_ptr
  make_weak_until(const twa_graph_ptr& lhs, const twa_graph_ptr& rhs,
                  bool rem_alt)
  {
    return _make_until(lhs, rhs, false, rem_alt);
  }

  twa_graph_ptr
  make_until(const twa_graph_ptr& lhs, const twa_graph_ptr& rhs,
             bool rem_alt)
  {
    return _make_until(lhs, rhs, true, rem_alt);
  }

  twa_graph_ptr
  ltl_to_tgba_rec(const formula& form, const bdd_dict_ptr& dict)
  {
    assert(form.is_ltl_formula());

    auto recurse = [&dict](const formula& f)
      {
        return ltl_to_tgba_rec(f, dict);
      };

    // put in negative normal form and remove unhandled operators
    formula f = unabbreviate(negative_normal_form(form), "ei^FGMR");
    assert(f.is_in_nenoform());

    //std::cerr << "doing " << f << std::endl;

    if (f.is_boolean())
      return make_ap(f, dict);

    twa_graph_ptr res;
    switch (f.kind())
    {
      case op::And:
        res = recurse(f[0]);
        for (unsigned i = 1; i != f.size(); ++i)
          res = make_and(res, recurse(f[i]));
        break;
      case op::Or:
        res = recurse(f[0]);
        for (unsigned i = 1; i != f.size(); ++i)
          res = make_or(res, recurse(f[i]));
        break;
      case op::X:
        res = make_next(recurse(f[0]));
        break;
      case op::U:
        res = make_until(recurse(f[0]), recurse(f[1]));
        break;
      case op::W:
        res = make_weak_until(recurse(f[0]), recurse(f[1]));
        break;
      case op::R:
        res = make_release(recurse(f[0]), recurse(f[1]), false, true);
        break;
      default:
        throw std::runtime_error("operator not handled by ltl_to_tgba_rec");
    }
    postprocessor post_proc;
    return post_proc.run(res);
  }
}
