// -*- coding: utf-8 -*-
// Copyright (C) 2017-2020 Laboratoire de Recherche et Développement
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

#include <algorithm>

namespace{
  // Computes and stores the restriction
  // of each cond to the input domain and the support
  // This is useful as it avoids recomputation
  // and often many conditions are mapped to the same
  // restriction
  struct small_cacher_t
  {
    //e to e_in and support
    std::unordered_map<bdd, std::pair<bdd, bdd>, spot::bdd_hash> cond_hash_;

    void fill(const spot::const_twa_graph_ptr& aut, bdd output_bdd)
    {
      cond_hash_.reserve(aut->num_edges()/5+1);
      // 20% is about lowest number of different edge conditions
      // for benchmarks taken from syntcomp

      for (const auto& e : aut->edges())
        {
          // Check if stored
          if (cond_hash_.find(e.cond) != cond_hash_.end())
            continue;

          cond_hash_[e.cond] =
              std::pair<bdd, bdd>(
                  bdd_exist(e.cond, output_bdd),
                  bdd_exist(bdd_support(e.cond), output_bdd));
        }
    }

    // Get the condition restricted to input and support of a condition
    const std::pair<bdd, bdd>& operator[](const bdd& econd) const
    {
      return cond_hash_.at(econd);
    }
  };

  // Struct to locally store the information of all outgoing edges
  // of the state.
  struct e_info_t
  {
    e_info_t(const spot::twa_graph::edge_storage_t& e,
             const small_cacher_t& sm)
        : dst(e.dst),
          econd(e.cond),
          einsup(sm[e.cond]),
          acc(e.acc)
    {
      pre_hash = (spot::wang32_hash(dst)
                 ^ std::hash<spot::acc_cond::mark_t>()(acc))
                 * spot::fnv<size_t>::prime;
    }

    inline size_t hash() const
    {
      return spot::wang32_hash(spot::bdd_hash()(econdout)) ^ pre_hash;
    }

    unsigned dst;
    bdd econd;
    mutable bdd econdout;
    std::pair<bdd, bdd> einsup;
    spot::acc_cond::mark_t acc;
    size_t pre_hash;
  };
  // We define a order between the edges to avoid creating multiple
  // states that in fact correspond to permutations of the order of the
  // outgoing edges
  struct less_info_t
  {
    // Note: orders via !econd!
    inline bool operator()(const e_info_t &lhs, const e_info_t &rhs) const
    {
      const int l_id = lhs.econd.id();
      const int r_id = rhs.econd.id();
      return std::tie(lhs.dst, lhs.acc, l_id)
             < std::tie(rhs.dst, rhs.acc, r_id);
    }
  }less_info;

  struct less_info_ptr_t
  {
    // Note: orders via !econdout!
    inline bool operator()(const e_info_t* lhs, const e_info_t* rhs)const
    {
      const int l_id = lhs->econdout.id();
      const int r_id = rhs->econdout.id();
      return std::tie(lhs->dst, lhs->acc, l_id)
             < std::tie(rhs->dst, rhs->acc, r_id);
    }
  }less_info_ptr;
}

namespace spot
{
    twa_graph_ptr
  split_2step(const const_twa_graph_ptr& aut, const bdd& input_bdd,
              const bdd& output_bdd, bool complete_env)
  {
    auto split = make_twa_graph(aut->get_dict());
    split->copy_ap_of(aut);
    split->copy_acceptance_of(aut);
    split->new_states(aut->num_states());
    split->set_init_state(aut->get_init_state_number());

    // a sort of hash-map for all new intermediate states
    std::unordered_multimap<size_t, unsigned> env_hash;
    env_hash.reserve((int) 1.5 * aut->num_states());
    // a local map for edges leaving the current src
    // this avoids creating and then combining edges for each minterm
    // Note there are usually "few" edges leaving a state
    // and map has shown to be faster than unordered_map for
    // syntcomp examples
    std::map<unsigned, std::pair<unsigned, bdd>> env_edge_hash;
    typedef std::map<unsigned, std::pair<unsigned, bdd>>::mapped_type eeh_t;

    small_cacher_t small_cacher;
    small_cacher.fill(aut, output_bdd);

    // Cache vector for all outgoing edges of this states
    std::vector<e_info_t> e_cache;

    // Vector of destinations actually reachable for a given
    // minterm in ins
    // Automatically "almost" sorted due to the sorting of e_cache
    std::vector<const e_info_t*> dests;

    // If a complete automaton for environment is demanded
    // we might need a sink
    unsigned sink_con=0;

    // Loop over all states
    for (unsigned src = 0; src < aut->num_states(); ++src)
      {
        env_edge_hash.clear();
        e_cache.clear();

        // Avoid looping over all minterms
        // we only loop over the minterms that actually exist
        bdd all_letters = bddfalse;
        bdd support = bddtrue;

        for (const auto& e : aut->out(src))
          {
            e_cache.emplace_back(e, small_cacher);
            all_letters |= e_cache.back().einsup.first;
            support &= e_cache.back().einsup.second;
          }
        // Complete for env
        if (complete_env && (all_letters != bddtrue))
          {
            if (sink_con == 0)
              sink_con = split->new_state();
            split->new_edge(src, sink_con, bddtrue - all_letters);
          }

        // Sort to avoid that permutations of the same edges
        // get different intermediate states
        std::sort(e_cache.begin(), e_cache.end(), less_info);

        while (all_letters != bddfalse)
          {
            bdd one_letter = bdd_satoneset(all_letters, support, bddtrue);
            all_letters -= one_letter;

            dests.clear();
            for (const auto& e_info : e_cache)
              // implies is faster than and
              if (bdd_implies(one_letter, e_info.einsup.first))
                {
                  e_info.econdout =
                      bdd_appex(e_info.econd, one_letter, bddop_and, input_bdd);
                  dests.push_back(&e_info);
                  assert(e_info.econdout != bddfalse);
                }
            // By construction this should not be empty
            assert(!dests.empty());
            // # dests is almost sorted -> insertion sort
            if (dests.size()>1)
              for (auto it = dests.begin(); it != dests.end(); ++it)
                std::rotate(std::upper_bound(dests.begin(), it, *it,
                                             less_info_ptr),
                            it, it + 1);

            bool to_add = true;
            size_t h = fnv<size_t>::init;
            for (const auto& t: dests)
              h ^= t->hash();

            auto range_h = env_hash.equal_range(h);
            for (auto it_h = range_h.first; it_h != range_h.second; ++it_h)
              {
                unsigned i = it_h->second;
                auto out = split->out(i);
                if (std::equal(out.begin(), out.end(),
                               dests.begin(), dests.end(),
                               [](const twa_graph::edge_storage_t& x,
                                  const e_info_t* y)
                               {
                                 return   x.dst == y->dst
                                          &&  x.acc == y->acc
                                          &&  x.cond.id() == y->econdout.id();
                               }))
                  {
                    to_add = false;
                    auto it = env_edge_hash.find(i);
                    if (it != env_edge_hash.end())
                      it->second.second |= one_letter;
                    else
                      env_edge_hash.emplace(i,
                          eeh_t(split->new_edge(src, i, bddtrue), one_letter));
                    break;
                  }
              }

            if (to_add)
              {
                unsigned d = split->new_state();
                unsigned n_e = split->new_edge(src, d, bddtrue);
                env_hash.emplace(h, d);
                env_edge_hash.emplace(d, eeh_t(n_e, one_letter));
                for (const auto &t: dests)
                  split->new_edge(d, t->dst, t->econdout, t->acc);
              }
          } // letters
        // save locally stored condition
        for (auto& elem : env_edge_hash)
          split->edge_data(elem.second.first).cond = elem.second.second;
      } // v-src

    split->merge_edges();
    split->prop_universal(spot::trival::maybe());
    return split;
  }

  twa_graph_ptr unsplit_2step(const const_twa_graph_ptr& aut)
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
