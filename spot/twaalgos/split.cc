// -*- coding: utf-8 -*-
// Copyright (C) 2017-2018 Laboratoire de Recherche et Développement
// de l'Epita.
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
#include <spot/twaalgos/split.hh>
#include <spot/misc/minato.hh>
#include <spot/twaalgos/totgba.hh>
#include <spot/misc/bddlt.hh>

namespace spot
{
  twa_graph_ptr
  split_automaton(const const_twa_graph_ptr& aut, bdd input_bdd)
  {
    // FIXME choose to rely on tgba or on aut, not on both
    auto tgba = to_generalized_buchi(aut);
    auto split = make_twa_graph(tgba->get_dict());
    split->copy_ap_of(tgba);
    split->copy_acceptance_of(tgba);
    split->new_states(tgba->num_states());
    split->set_init_state(tgba->get_init_state_number());

    std::unordered_map<bdd, unsigned, spot::bdd_hash> sig2state;

    unsigned set_num = split->get_dict()
      ->register_anonymous_variables(aut->num_states()+1, &sig2state);
    bdd all_states = bddtrue;
    for (unsigned i = 0; i <= aut->num_states(); ++i)
      all_states &= bdd_ithvar(set_num + i);

    unsigned acc_vars = split->get_dict()
      ->register_anonymous_variables(aut->num_sets(), &sig2state);
    bdd all_acc = bddtrue;
    for (unsigned i = 0; i < aut->num_sets(); ++i)
      all_acc &= bdd_ithvar(acc_vars + i);

    for (unsigned src = 0; src < tgba->num_states(); ++src)
      {
        std::unordered_map<bdd, bdd, spot::bdd_hash> input2sig;
        bdd support = bddtrue;
        for (const auto& e : tgba->out(src))
          support &= bdd_support(e.cond);
        support = bdd_existcomp(support, input_bdd);

        bdd all_letters = bddtrue;
        while (all_letters != bddfalse)
          {
            bdd one_letter = bdd_satoneset(all_letters, support, bddtrue);
            all_letters -= one_letter;
            auto it = input2sig.emplace(one_letter, bddfalse).first;

            for (const auto& e : tgba->out(src))
              {
                bdd sig = bddtrue;
                sig &= bdd_exist(e.cond & one_letter, input_bdd);

                for (auto n : e.acc.sets())
                  sig &= bdd_ithvar(acc_vars + n);
                sig &= bdd_ithvar(set_num + e.dst);

                it->second |= sig;
              }
          }

        for (const auto& in : input2sig)
          {
            bdd sig = in.second;
            auto it = sig2state.find(sig);
            if (it == sig2state.end())
              {
                unsigned ns = split->new_state();
                it = sig2state.emplace(sig, ns).first;
              }
            split->new_edge(src, it->second, in.first);
          }
      }

    // Now add all states based on their signatures
    // This is very similar to the code of the simulation
    bdd nonapvars = all_acc & all_states;
    for (const auto& sig : sig2state)
      {
        // for each valuation, extract cond, acc and dst
        bdd sup_sig = bdd_support(sig.first);
        bdd sup_all_atomic_prop = bdd_exist(sup_sig, nonapvars);
        bdd all_atomic_prop = bdd_exist(sig.first, nonapvars);

        // FIXME this is overkill, is not it?
        while (all_atomic_prop != bddfalse)
          {
            bdd one = bdd_satoneset(all_atomic_prop, sup_all_atomic_prop,
                                    bddtrue);
            all_atomic_prop -= one;

            minato_isop isop(sig.first & one);
            bdd cond_acc_dest;
            while ((cond_acc_dest = isop.next()) != bddfalse)
              {

                bdd cond = bdd_existcomp(cond_acc_dest, sup_all_atomic_prop);
                unsigned dst =
                  bdd_var(bdd_existcomp(cond_acc_dest, all_states)) - set_num;

                bdd acc = bdd_existcomp(cond_acc_dest, all_acc);
                spot::acc_cond::mark_t m({});
                while (acc != bddtrue)
                  {
                    m.set(bdd_var(acc) - acc_vars);
                    acc = bdd_high(acc);
                  }

                split->new_edge(sig.second, dst, cond, m);
              }
          }
      }

    split->get_dict()->unregister_all_my_variables(&sig2state);

    split->merge_edges();

    split->prop_universal(spot::trival::maybe());
    return split;
  }

  twa_graph_ptr unsplit(const const_twa_graph_ptr& aut)
  {
    twa_graph_ptr out = make_twa_graph(aut->get_dict());
    out->copy_acceptance_of(aut);
    out->copy_ap_of(aut);
    out->new_states(aut->num_states());
    out->set_init_state(aut->get_init_state_number());

    std::vector<bool> seen(aut->num_states(), false);
    std::deque<unsigned> todo;
    todo.push_back(aut->get_init_state_number());
    seen[aut->get_init_state_number()] = true;
    while (!todo.empty())
      {
        unsigned cur = todo.front();
        todo.pop_front();
        seen[cur] = true;

        for (const auto& i : aut->out(cur))
          for (const auto& o : aut->out(i.dst))
            {
              out->new_edge(cur, o.dst, i.cond & o.cond, i.acc | o.acc);
              if (!seen[o.dst])
                todo.push_back(o.dst);
            }
      }
    return out;
  }

  twa_graph_ptr split_edges(const const_twa_graph_ptr& aut)
  {
    twa_graph_ptr out = make_twa_graph(aut->get_dict());
    out->copy_acceptance_of(aut);
    out->copy_ap_of(aut);
    out->prop_copy(aut, twa::prop_set::all());
    out->new_states(aut->num_states());
    out->set_init_state(aut->get_init_state_number());

    internal::univ_dest_mapper<twa_graph::graph_t> uniq(out->get_graph());

    bdd all = aut->ap_vars();
    for (auto& e: aut->edges())
      {
        bdd cond = e.cond;
        if (cond == bddfalse)
          continue;
        unsigned dst = e.dst;
        if (aut->is_univ_dest(dst))
          {
            auto d = aut->univ_dests(dst);
            dst = uniq.new_univ_dests(d.begin(), d.end());
          }
        while (cond != bddfalse)
          {
            bdd cube = bdd_satoneset(cond, all, bddfalse);
            cond -= cube;
            out->new_edge(e.src, dst, cube, e.acc);
          }
      }
    return out;
  }
}
