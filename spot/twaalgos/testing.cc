// -*- coding: utf-8 -*-
// Copyright (C) 2009, 2010, 2013, 2014, 2015 Laboratoire de Recherche
// et Développement de l'Epita (LRDE).
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
#include <spot/twaalgos/testing.hh>
#include <spot/twaalgos/sccinfo.hh>
#include <spot/twaalgos/mask.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twa/bddprint.hh>
#include <deque>
#include <unordered_map>
#include <spot/misc/hash.hh>

namespace spot
{

  typedef std::pair<unsigned, bdd> diff_state;

  struct diff_state_hash
  {
    size_t
      operator()(diff_state s) const noexcept
      {
        return wang32_hash(s.first ^ wang32_hash(s.second.id()));
      }
  };

  twa_graph_ptr statelabeldiff(const const_twa_graph_ptr& input_twa)
  {
    if (SPOT_UNLIKELY(!(input_twa->is_existential())))
      throw std::runtime_error
        ("statelabeldiff() does not support alternating automata");

    auto res = make_twa_graph(input_twa->get_dict());
    res->copy_ap_of(input_twa);

    auto v = new std::vector<std::string>;
    res->set_named_prop("state-names", v);
    res->set_named_prop("testing-automaton", new bool);
    res->copy_acceptance_of(input_twa);

    std::unordered_map<diff_state, unsigned, diff_state_hash> seen;
    std::deque<std::pair<diff_state, unsigned>> todo;

    auto new_state =
      [&](unsigned twa_state, bdd cond) -> unsigned
      {
        diff_state x(twa_state, cond);
        auto p = seen.emplace(x, 0);
        if (p.second)
        {
          p.first->second = res->new_state();
          todo.emplace_back(x, p.first->second);
          std::stringstream ss;
          ss << twa_state << ',';
          bdd_print_formula(ss, input_twa->get_dict(), cond);
          v->emplace_back(ss.str());
        }
        return p.first->second;
      };

    res->set_init_state(new_state(input_twa->get_init_state_number(), bddtrue));

    bdd allap = input_twa->ap_vars();

    while (!todo.empty())
    {
      auto top = todo.front();
      todo.pop_front();
      for (auto& e : input_twa->out(top.first.first))
      {
        bdd sup = e.cond;
        //create the diff_link
        while (sup != bddfalse)
        {
          bdd one = bdd_satoneset(sup, allap, bddfalse);
          sup -= one;
          auto cond = bdd_setxor(one, top.first.second);
          res->new_edge(top.second, new_state(e.dst, one), cond, e.acc);
        }
      }
    }

    return res;
  }

  twa_graph_ptr remove_testing(const const_twa_graph_ptr& input_diff_twa)
  {
    if (SPOT_UNLIKELY(!(input_diff_twa->is_existential())))
      throw std::runtime_error
        ("remove_testing() does not support alternating automata");

    if (!input_diff_twa->get_named_prop<bool>("testing-automaton"))
      throw std::runtime_error
        ("remove_testing() need a testing automata");

    auto res = make_twa_graph(input_diff_twa->get_dict());
    res->copy_ap_of(input_diff_twa);
    res->copy_acceptance_of(input_diff_twa);

    auto v = new std::vector<std::string>;
    res->set_named_prop("state-names", v);


    std::unordered_map<diff_state, unsigned, diff_state_hash> seen;
    std::deque<std::pair<diff_state, unsigned>> todo;

    auto new_state =
      [&](unsigned input_state, bdd cond) -> unsigned
      {
        diff_state x(input_state, cond);
        auto p = seen.emplace(x, 0);
        if (p.second)
        {
          p.first->second = res->new_state();
          todo.emplace_back(x, p.first->second);

          std::stringstream ss;
          ss << input_state << ',';
          bdd_print_formula(ss, input_diff_twa->get_dict(), cond);
          v->emplace_back(ss.str());
        }
        return p.first->second;
      };

    res->set_init_state(new_state(input_diff_twa->get_init_state_number(),
          bddtrue));

    while (!todo.empty())
    {
      auto top = todo.front();
      todo.pop_front();

      for (auto& e : input_diff_twa->out(top.first.first))
      {
        auto cond = bdd_setxor(e.cond, top.first.second);
        res->new_edge(top.second, new_state(e.dst, cond), cond, e.acc);
      }
    }
    return res;
  }

  twa_graph_ptr remove_stuttering_lasso(const const_twa_graph_ptr& input_twa)
  {
    if (SPOT_UNLIKELY(!(input_twa->is_existential())))
      throw std::runtime_error
        ("remove_testing() does not support alternating automata");

    if (!input_twa->get_named_prop<bool>("testing-automaton"))
      throw std::runtime_error
        ("remove_testing() need a testing automata");

    auto filter_edge = [](const twa_graph::edge_storage_t& e, unsigned,
        void* filter_data)
    {
      auto allap = static_cast<bdd*>(filter_data);

      if (e.cond != *allap)
        return scc_info::edge_filter_choice::cut;
      return scc_info::edge_filter_choice::keep;
    };

    bdd empty = bdd_satoneset(bddtrue, input_twa->ap_vars(), bddfalse);
    scc_info r_scc(input_twa, ~0U, filter_edge, &empty);

    unsigned nb_scc = r_scc.scc_count();
    std::vector<bool> is_livelock(nb_scc, false);
    for (unsigned n = 0; n < nb_scc; ++n)
    {
      if (r_scc.is_accepting_scc(n))
      {
        is_livelock[n] = true;
        continue;
      }

      for (unsigned u : r_scc.succ(n))
      {
        if (is_livelock[u])
        {
          is_livelock[n] = true;
          break;
        }
      }
    }

    twa_graph_ptr res = make_twa_graph(input_twa->get_dict());
    auto acc = input_twa->get_acceptance();
    unsigned fin = input_twa->num_sets();
    acc |= acc_cond::acc_code::fin({fin});
    res->set_acceptance(fin+1, acc);

    transform_accessible(input_twa, res, [&](unsigned src, bdd& cond,
          acc_cond::mark_t& m, unsigned dst)
        {
          //acc.set();
          if (cond == empty && is_livelock[r_scc.scc_of(src)])
          {
            if (src != dst)
              cond = bddfalse;
          }
          else
            m.set(fin);
        });
    res->set_named_prop("testing-automaton", new bool);
    return res;
  }
}
