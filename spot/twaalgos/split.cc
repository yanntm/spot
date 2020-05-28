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

#include <algorithm>

namespace{
  // Computes and stores the restriction
  // of each cond to the input domain and the support
  // This is usefull as it avoids recomputation
  // and often many conditions are mapped to the same
  // restriction
  struct small_cacher_t
  {
    //e to e_in
    std::unordered_map<bdd, std::pair<bdd, bdd>, spot::bdd_hash>
        cond_hash_;
  
    void fill(const spot::const_twa_graph_ptr& aut, bdd output_bdd)
    {
      cond_hash_.reserve((size_t) .2*aut->num_edges()+1);
      
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
    const std::pair<bdd,bdd>& operator[](const bdd& econd) const
    {
      return cond_hash_.at(econd);
    }
  };
  
  // Struct to locally store the informations of all outgoing edges
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
    bdd econd, econdout;
    std::pair<bdd,bdd> einsup;
    spot::acc_cond::mark_t acc;
    size_t pre_hash;
  };
  // We define a order between the edges to avoid creating multiple
  // states that in fact correspond to permutations of the order of the
  // outgoing edges
  struct less_info_t {
    // Note: orders via econd
    inline bool operator()(const e_info_t& lhs, const e_info_t& rhs)const
    {
      if (lhs.dst < rhs.dst)
        return true;
      else if (lhs.dst > rhs.dst)
        return false;
      
      if (lhs.acc < rhs.acc)
        return true;
      else if (lhs.acc > rhs.acc)
        return false;
      
      if (lhs.econd.id() < rhs.econd.id())
        return true;
      else if (lhs.econd.id() > rhs.econd.id())
        return false;
      
      return false;
    }
    // Note: orders via econdout
    inline bool operator()(const e_info_t* lhs, const e_info_t* rhs)const
    {
      if (lhs->dst < rhs->dst)
        return true;
      else if (lhs->dst > rhs->dst)
        return false;
      
      if (lhs->acc < rhs->acc)
        return true;
      else if (lhs->acc > rhs->acc)
        return false;
      
      if (lhs->econdout.id() < rhs->econdout.id())
        return true;
      else if (lhs->econdout.id() > rhs->econdout.id())
        return false;
      
      return false;
    }
  };
}



namespace spot
{
  twa_graph_ptr
  split_2step_old(const const_twa_graph_ptr& aut, bdd input_bdd)
  {
    auto split = make_twa_graph(aut->get_dict());
    split->copy_ap_of(aut);
    split->copy_acceptance_of(aut);
    split->new_states(aut->num_states());
    split->set_init_state(aut->get_init_state_number());
    
    // a sort of hash-map
    std::map<size_t, std::set<unsigned>> env_hash;
    
    struct trans_t
    {
      unsigned dst;
      bdd cond;
      acc_cond::mark_t acc;
      
      size_t hash() const
      {
        return bdd_hash()(cond)
               ^ wang32_hash(dst) ^ std::hash<acc_cond::mark_t>()(acc);
      }
    };
    
    std::vector<trans_t> dests;
    for (unsigned src = 0; src < aut->num_states(); ++src)
    {
      bdd support = bddtrue;
      for (const auto& e : aut->out(src))
        support &= bdd_support(e.cond);
      support = bdd_existcomp(support, input_bdd);
      
      bdd all_letters = bddtrue;
      while (all_letters != bddfalse)
      {
        bdd one_letter = bdd_satoneset(all_letters, support, bddtrue);
        all_letters -= one_letter;
        
        dests.clear();
        for (const auto& e : aut->out(src))
        {
          bdd cond = bdd_exist(e.cond & one_letter, input_bdd);
          if (cond != bddfalse)
            dests.emplace_back(trans_t{e.dst, cond, e.acc});
        }
        
        bool to_add = true;
        size_t h = fnv<size_t>::init;
        for (const auto& t: dests)
        {
          h ^= t.hash();
          h *= fnv<size_t>::prime;
        }
        
        for (unsigned i: env_hash[h])
        {
          auto out = split->out(i);
          if (std::equal(out.begin(), out.end(),
                         dests.begin(), dests.end(),
                         [](const twa_graph::edge_storage_t& x,
                            const trans_t& y)
                         {
                           return   x.dst == y.dst
                                    &&  x.cond.id() == y.cond.id()
                                    &&  x.acc == y.acc;
                         }))
          {
            to_add = false;
            split->new_edge(src, i, one_letter);
            break;
          }
        }
        
        if (to_add)
        {
          unsigned d = split->new_state();
          split->new_edge(src, d, one_letter);
          env_hash[h].insert(d);
          for (const auto& t: dests)
            split->new_edge(d, t.dst, t.cond, t.acc);
        }
      }
    }
    
    split->merge_edges();
    
    split->prop_universal(spot::trival::maybe());
    return split;
  }
  
  twa_graph_ptr
  split_2step(const const_twa_graph_ptr& aut, bdd input_bdd, bdd output_bdd,
              bool complete_env)
  {
    auto split = make_twa_graph(aut->get_dict());
    split->copy_ap_of(aut);
    split->copy_acceptance_of(aut);
    split->new_states(aut->num_states());
    split->set_init_state(aut->get_init_state_number());
    
    // a sort of hash-map
    std::unordered_multimap<size_t, unsigned> env_hash;
    env_hash.reserve((int) 1.5 * aut->num_states());
    std::map<unsigned, std::pair<unsigned,bdd>> env_edge_hash;
    
    small_cacher_t small_cacher;
    small_cacher.fill(aut, output_bdd);
    
    // A order for cached edges
    less_info_t less_info;
    
    // Cache vector for all outgoing edges of this states
    std::vector<e_info_t> e_cache;
    
    // Vector of destinations actually reachable for a given
    // minterm in ins
    // Automatically "almost" sorted due to the sorting of e_cache
    std::vector<e_info_t*> dests;
    
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
        bdd remaining = bddtrue - all_letters;
        split->new_edge(src, sink_con, remaining);
      }
      
      // Sort to avoid that permutations of the same edges
      // get different states
      std::sort(e_cache.begin(), e_cache.end(), less_info);
      
      while (all_letters != bddfalse)
      {
        bdd one_letter = bdd_satoneset(all_letters, support, bddtrue);
        all_letters -= one_letter;
        
        dests.clear();
        for (auto& e_info : e_cache)
          // implies is faster than and
          if (bdd_implies(one_letter, e_info.einsup.first))
          {
            e_info.econdout =
                bdd_appex(e_info.econd, one_letter, bddop_and, input_bdd);
            dests.push_back(&e_info);
            SPOT_ASSERT(e_info.econdout != bddfalse);
          }
        SPOT_ASSERT(!dests.empty());
        // # dests is almost sorted -> insertion sort
        if (dests.size()>1)
          for (auto it = dests.begin(); it != dests.end(); ++it)
            std::rotate(std::upper_bound(dests.begin(), it, *it,less_info),
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
                  std::make_pair(split->new_edge(src, i, bddtrue), one_letter));
            break;
          }
        }
        
        if (to_add)
        {
          unsigned d = split->new_state();
          unsigned n_e = split->new_edge(src, d, bddtrue);
          env_hash.emplace(h,d);
          env_edge_hash.emplace(d, std::make_pair(n_e, one_letter));
          for (const auto &t: dests)
            split->new_edge(d, t->dst, t->econdout, t->acc);
        }
      }
      // save locally stored condition
      for (auto& elem : env_edge_hash)
        split->edge_data(elem.second.first).cond = elem.second.second;
    }
    
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