// -*- coding: utf-8 -*-
// Copyright (C) 2020 Laboratoire de Recherche et Developpement de
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

#include <deque>
#include <unordered_map>
#include <vector>

#include <spot/twacube_algos/determinize.hh>
#include <bddx.h>

namespace spot
{
  namespace
  {
    // TODO: remove this when multithreaded
    const unsigned THREAD_ID = 0;

    struct safra_state final
    {
      // each brace points to its parent.
      // braces_[i] is the parent of i
      // Note that braces_[i] < i, -1 stands for "no parent" (top-level)
      std::vector<int> braces_;

      using state_t = unsigned;
      // index of the node, and index of its enclosing brace, if any (else -1)
      std::vector<std::pair<state_t, int>> nodes_;

      safra_state(state_t state_number, bool is_initial = false);

      size_t hash() const;
      bool operator==(const safra_state&) const;
    }; // struct safra_state

    safra_state::safra_state(state_t state_number, bool is_initial)
      : nodes_{std::make_pair(state_number, -1)}
    {
      // add first pair of curly brackets
      if (is_initial)
        {
          braces_.emplace_back(-1);
          nodes_.back().second = 0;
        }
    }

    bool
    safra_state::operator==(const safra_state& other) const
    {
      return nodes_ == other.nodes_ && braces_ == other.braces_;
    }

    size_t
    safra_state::hash() const
    {
      size_t res = 0;
      //std::cerr << this << " [";
      for (const auto& p : nodes_)
        {
          res ^= (res << 3) ^ p.first;
          res ^= (res << 3) ^ p.second;
          //  std::cerr << '(' << p.first << ',' << p.second << ')';
        }
      //    std::cerr << "][ ";
      for (const auto& b : braces_)
        {
          res ^= (res << 3) ^ b;
          //  std::cerr << b << ' ';
        }
      //    std::cerr << "]: " << std::hex << res << std::dec << '\n';
      return res;
    }

    struct hash_safra
    {
      size_t
      operator()(const safra_state& s) const noexcept
      {
        return s.hash();
      }
    };
  }

  /// \brief compute successor to a state with \a letter
  ///
  /// \returns a pair with the successor and the emitted color if any
  std::pair<safra_state, unsigned>
  compute_succ(twacube_ptr, const safra_state& src, const cube&)
    {
      // TODO
      return std::make_pair(src, -1U);
    }

  /// \brief finds all the variables that \a c depends on.
  cube
  cube_support(const cube& c)
    {
      // TODO(am): just identify variables that aren't free ?
      return c;
    }

  std::vector<cube>
  get_letters(const safra_state& s, const std::vector<cube>& supports, twacube_ptr aut)
    {
      const cubeset& cs = aut->get_cubeset();

      cube supp = cs.alloc();
      for (const auto& [state, _] : s.nodes_)
        supp = cs.intersection(supp, supports[state]);

      // convert support from cube to bdd
      bdd allap = bddtrue;
      for(unsigned i = 0; i < cs.size(); ++i)
        {
            if (cs.is_true_var(supp, i))
              allap &= bdd_ithvar(i);
            else if (cs.is_false_var(supp, i))
              allap &= bdd_nithvar(i);
            // otherwise it 's a free variable, shouldn't even be in the support?
            // TODO: what if both true and false, after intersection?
        }

      std::vector<cube> res;

      // from the BDD support, do satonesets to extract individual atomic propositions
      bdd all = bddtrue;
      while (all != bddfalse)
        {
          bdd one = bdd_satoneset(all, allap, bddfalse);
          all -= one;
          cube one_cube = cs.alloc();
          cs.set_true_var(one_cube, bdd_var(one));
          res.push_back(one_cube);
        }
      return res;
    }

  twacube_ptr
  twacube_determinize(const twacube_ptr aut)
  {
    // TODO(am): check is_existential + is_universal before launching useless
    // computation

    // TODO(am): degeneralize ? might need to filter out TGBAs for now

    auto res = make_twacube(aut->get_ap());

    // computing each state's support, i.e. all variables upon which the
    // transition taken depends
    std::vector<cube> supports(aut->num_states());
    for (unsigned i = 0; i != aut->num_states(); ++i)
      {
        // alloc() returns a cube with all variables marked free
        cube res = aut->get_cubeset().alloc();

        const auto succs = aut->succ(i);
        while (!succs->done())
          {
            auto& trans = aut->trans_data(succs, THREAD_ID);
            res = aut->get_cubeset().intersection(res, cube_support(trans.cube_));
          }

        supports[i] = res;
      }

    // association between safra_state and destination state in resulting
    // automaton
    std::unordered_map<safra_state, unsigned, hash_safra> seen;

    // a safra state and its corresponding state id in the resulting automaton
    std::deque<std::pair<safra_state, unsigned>> todo;

    // find association between safra state and res state, or create one
    auto get_state = [&res, &seen, &todo](const safra_state& s) -> unsigned
      {
        auto it = seen.find(s);
        if (it == seen.end())
          {
            unsigned dst_num = res->new_state();
            it = seen.emplace(s, dst_num).first;
            todo.emplace_back(*it);
          }
        return it->second;
      };

    // initial state creation
    {
      unsigned init_state = aut->get_initial();
      safra_state init(init_state, true);
      unsigned res_init = get_state(init);
      res->set_initial(res_init);
    }

    // core algorithm
    //
    // for every safra state,
    //     for each possible safra successor,
    //         compute successor emitted color
    //         create transition in res automaton, with color
    while (!todo.empty())
      {
        const safra_state curr = todo.front().first;
        const unsigned src_num = todo.front().second;
        todo.pop_front();

        // get each possible transition
        auto letters = get_letters(curr, supports, aut);
        for (const auto& l : letters)
          {
            // returns successor safra_state and emitted color if any
            auto [succ, color] = compute_succ(aut, curr, l);
            unsigned dst_num = get_state(succ);

            if (color != -1U)
              {
                // TODO: acc_cond::mark_t couleur
                res->create_transition(src_num, l, { color }, dst_num);
              }
            else
              {
                // TODO: acc_cond::mark_t 'vide' ?
                res->create_transition(src_num, l, {} , dst_num);
              }
          }
      }

    return res;
  }
}
