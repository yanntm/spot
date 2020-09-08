// -*- coding: utf-8 -*-
// Copyright (C) 2017, 2018, 2020 Laboratoire de Recherche et Développement
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

#include <cmath>
#include <spot/misc/game.hh>

namespace spot
{
  namespace
  {

    static const std::vector<bool>*
    ensure_parity_game(const const_twa_graph_ptr& arena, const char* fnname)
    {
      bool max, odd;
      arena->acc().is_parity(max, odd, true);
      if (!(max && odd))
        throw std::runtime_error
          (std::string(fnname) +
           ": arena must have max-odd acceptance condition");

      for (const auto& e : arena->edges())
        if (!e.acc)
          throw std::runtime_error
            (std::string(fnname) + ": arena must be colorized");

      auto owner = arena->get_named_prop<std::vector<bool>>("state-player");
      if (!owner)
        throw std::runtime_error
          (std::string(fnname) + ": automaton should define \"state-player\"");

      return owner;
    }


    strategy_t attractor(const const_twa_graph_ptr& arena,
                         const std::vector<bool>* owner,
                         const region_t& subgame, region_t& set,
                         unsigned max_parity, int p,
                         bool attr_max)
    {
      strategy_t strategy;
      std::set<unsigned> complement(subgame.begin(), subgame.end());
      for (unsigned s: set)
        complement.erase(s);

      acc_cond::mark_t max_acc({});
      for (unsigned i = 0; i <= max_parity; ++i)
        max_acc.set(i);

      bool once_more;
      do
        {
          once_more = false;
          for (auto it = complement.begin(); it != complement.end();)
            {
              unsigned s = *it;
              unsigned i = 0;

              bool is_owned = (*owner)[s] == p;
              bool wins = !is_owned;

              for (const auto& e: arena->out(s))
                {
                  if ((e.acc & max_acc) && subgame.count(e.dst))
                    {
                      if (set.count(e.dst)
                          || (attr_max && e.acc.max_set() - 1 == max_parity))
                        {
                          if (is_owned)
                            {
                              strategy[s] = i;
                              wins = true;
                              break; // no need to check all edges
                            }
                        }
                      else
                        {
                          if (!is_owned)
                            {
                              wins = false;
                              break; // no need to check all edges
                            }
                        }
                    }
                  ++i;
                }

              if (wins)
                {
                  // FIXME C++17 extract/insert could be useful here
                  set.emplace(s);
                  it = complement.erase(it);
                  once_more = true;
                }
              else
                ++it;
            }
        } while (once_more);
      return strategy;
    }

    void solve_rec(const const_twa_graph_ptr& arena,
                   const std::vector<bool>* owner,
                   region_t& subgame, unsigned max_parity,
                   region_t (&w)[2], strategy_t (&s)[2])
    {
      assert(w[0].empty());
      assert(w[1].empty());
      assert(s[0].empty());
      assert(s[1].empty());
      // The algorithm works recursively on subgames. To avoid useless copies of
      // the game at each call, subgame and max_parity are used to filter states
      // and transitions.
      if (subgame.empty())
        return;
      int p = max_parity % 2;

      // Recursion on max_parity.
      region_t u;
      auto strat_u = attractor(arena, owner, subgame, u, max_parity, p, true);

      if (max_parity == 0)
        {
          s[p] = std::move(strat_u);
          w[p] = std::move(u);
          // FIXME what about w[!p]?
          return;
        }

      for (unsigned s: u)
        subgame.erase(s);
      region_t w0[2]; // Player's winning region in the first recursive call.
      strategy_t s0[2]; // Player's winning strategy in the first
                        // recursive call.
      solve_rec(arena, owner, subgame, max_parity - 1, w0, s0);
      if (w0[0].size() + w0[1].size() != subgame.size())
        throw std::runtime_error("size mismatch");
      //if (w0[p].size() != subgame.size())
      //  for (unsigned s: subgame)
      //    if (w0[p].find(s) == w0[p].end())
      //      w0[!p].insert(s);
      subgame.insert(u.begin(), u.end());

      if (w0[p].size() + u.size() == subgame.size())
        {
          s[p] = std::move(strat_u);
          s[p].insert(s0[p].begin(), s0[p].end());
          w[p].insert(subgame.begin(), subgame.end());
          return;
        }

      // Recursion on game size.
      auto strat_wnp = attractor(arena, owner,
                                 subgame, w0[!p], max_parity, !p, false);

      for (unsigned s: w0[!p])
        subgame.erase(s);

      region_t w1[2]; // Odd's winning region in the second recursive call.
      strategy_t s1[2]; // Odd's winning strategy in the second recursive call.
      solve_rec(arena, owner, subgame, max_parity, w1, s1);
      if (w1[0].size() + w1[1].size() != subgame.size())
        throw std::runtime_error("size mismatch");

      w[p] = std::move(w1[p]);
      s[p] = std::move(s1[p]);

      w[!p] = std::move(w1[!p]);
      w[!p].insert(w0[!p].begin(), w0[!p].end());
      s[!p] = std::move(strat_wnp);
      s[!p].insert(s0[!p].begin(), s0[!p].end());
      s[!p].insert(s1[!p].begin(), s1[!p].end());

      subgame.insert(w0[!p].begin(), w0[!p].end());
    }

  } // anonymous

  void pg_print(std::ostream& os, const const_twa_graph_ptr& arena)
  {
    auto owner = ensure_parity_game(arena, "pg_print");

    unsigned ns = arena->num_states();
    unsigned init = arena->get_init_state_number();
    os << "parity " << ns - 1 << ";\n";
    std::vector<bool> seen(ns, false);
    std::vector<unsigned> todo({init});
    while (!todo.empty())
      {
        unsigned src = todo.back();
        todo.pop_back();
        if (seen[src])
          continue;
        seen[src] = true;
        os << src << ' ';
        os << arena->out(src).begin()->acc.max_set() - 1 << ' ';
        os << (*owner)[src] << ' ';
        bool first = true;
        for (auto& e: arena->out(src))
          {
            if (!first)
              os << ',';
            first = false;
            os << e.dst;
            if (!seen[e.dst])
              todo.push_back(e.dst);
          }
        if (src == init)
          os << " \"INIT\"";
        os << ";\n";
      }
  }

  void parity_game_solve(const const_twa_graph_ptr& arena,
                         region_t (&w)[2], strategy_t (&s)[2])
  {
    const std::vector<bool>* owner =
      ensure_parity_game(arena, "parity_game_solve");

    region_t states_;
    unsigned ns = arena->num_states();
    for (unsigned i = 0; i < ns; ++i)
      states_.insert(i);

    acc_cond::mark_t m{};
    for (const auto& e: arena->edges())
      m |= e.acc;

    solve_rec(arena, owner, states_, m.max_set(), w, s);
  }

  void propagate_players(spot::twa_graph_ptr& arena,
                         bool first_player, bool complete0)
  {
    auto um = arena->acc().unsat_mark();
    if (!um.first)
      throw std::runtime_error("game winning condition is a tautology");

    unsigned sink_env = 0;
    unsigned sink_con = 0;

    std::vector<bool> seen(arena->num_states(), false);
    unsigned init = arena->get_init_state_number();
    std::vector<unsigned> todo({init});
    auto owner = new std::vector<bool>(arena->num_states(), false);
    (*owner)[init] = first_player;
    while (!todo.empty())
      {
        unsigned src = todo.back();
        todo.pop_back();
        seen[src] = true;
        bdd missing = bddtrue;
        for (const auto& e: arena->out(src))
          {
            bool osrc = (*owner)[src];
            if (complete0 && !osrc)
              missing -= e.cond;

            if (!seen[e.dst])
              {
                (*owner)[e.dst] = !osrc;
                todo.push_back(e.dst);
              }
            else if ((*owner)[e.dst] == osrc)
              {
                delete owner;
                throw
                  std::runtime_error("propagate_players(): odd cycle detected");
              }
          }
        if (complete0 && !(*owner)[src] && missing != bddfalse)
          {
            if (sink_env == 0)
              {
                sink_env = arena->new_state();
                sink_con = arena->new_state();
                owner->push_back(true);
                owner->push_back(false);
                arena->new_edge(sink_con, sink_env, bddtrue, um.second);
                arena->new_edge(sink_env, sink_con, bddtrue, um.second);
              }
            arena->new_edge(src, sink_con, missing, um.second);
          }
      }

    arena->set_named_prop("state-player", owner);
  }
}
