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

  twa_graph_ptr statelabeldiff(const const_twa_graph_ptr& i_twa)
  {
    if (SPOT_UNLIKELY(!(i_twa->is_existential())))
      throw std::runtime_error
        ("statelabeldiff() does not support alternating automata");

    auto res = make_twa_graph(i_twa->get_dict());
    res->copy_ap_of(i_twa);

    auto v = new std::vector<std::string>;
    res->set_named_prop("state-names", v);
    res->set_named_prop("testing-automaton", new bool);
    res->copy_acceptance_of(i_twa);

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
          bdd_print_formula(ss, i_twa->get_dict(), cond);
          v->emplace_back(ss.str());
        }
        return p.first->second;
      };

    res->set_init_state(new_state(i_twa->get_init_state_number(), bddtrue));

    bdd allap = i_twa->ap_vars();

    while (!todo.empty())
    {
      auto top = todo.front();
      todo.pop_front();
      for (auto& e : i_twa->out(top.first.first))
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
}
