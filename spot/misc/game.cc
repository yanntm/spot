// -*- coding: utf-8 -*-
// Copyright (C) 2017, 2018 Laboratoire de Recherche et Développement
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

parity_game::parity_game(const twa_graph_ptr& arena,
                         const std::vector<bool>& owner)
  : arena_(arena)
  , owner_(owner)
{
  bool max, odd;
  arena_->acc().is_parity(max, odd, true);
  if (!(max && odd))
    throw std::runtime_error("arena must have max-odd acceptance condition");

  for (const auto& e : arena_->edges())
    if (e.acc.max_set() == 0)
      throw std::runtime_error("arena must be colorized");

  assert(owner_.size() == arena_->num_states());
}

void parity_game::print(std::ostream& os)
{
  os << "parity " << num_states() - 1 << ";\n";
  std::vector<bool> seen(num_states(), false);
  std::vector<unsigned> todo({get_init_state_number()});
  while (!todo.empty())
    {
      unsigned src = todo.back();
      todo.pop_back();
      seen[src] = true;
      os << src << ' ';
      os << out(src).begin()->acc.max_set() - 1 << ' ';
      os << owner(src) << ' ';
      bool first = true;
      for (auto& e: out(src))
        {
          if (!first)
            os << ',';
          first = false;
          os << e.dst;
          if (!seen[e.dst])
            todo.push_back(e.dst);
        }
      if (src == get_init_state_number())
        os << " \"INIT\"";
      os << ";\n";
    }
}

void parity_game::solve(region_t (&w)[2], strategy_t (&s)[2]) const
{
  region_t states_;
  for (unsigned i = 0; i < num_states(); ++i)
    states_.insert(i);
  unsigned m = max_parity();
  solve_rec(states_, m, w, s);
}

bool parity_game::solve_qp() const
{
  return reachability_game(*this).is_reachable();
}

parity_game::strategy_t
parity_game::attractor(const region_t& subgame, region_t& set,
                       unsigned max_parity, int p, bool attr_max) const
{
  strategy_t strategy;
  unsigned size;
  std::unordered_set<unsigned> complement = subgame;
  do
    {
      size = set.size();
      for (unsigned s: set)
        complement.erase(s);
      for (auto it = complement.begin(); it != complement.end(); )
        {
          unsigned s = *it;
          bool any = false;
          bool all = true;
          unsigned i = 0;
          for (const auto& e: out(s))
            {
              if (e.acc.max_set() - 1 <= max_parity && subgame.count(e.dst))
                {
                  if (set.count(e.dst)
                      || (attr_max && e.acc.max_set() - 1 == max_parity))
                    {
                      if (!any && (owner_[s] == p))
                        strategy[s] = i;
                      any = true;
                    }
                  else
                    all = false;
                }
              ++i;
            }
          bool owner_is_odd = owner_[s] == p;
          if ((owner_is_odd && any) || (!owner_is_odd && all))
            {
              set.insert(s);
              it = complement.erase(it);
            }
          else
            ++it;
        }
    } while (set.size() != size);
  return strategy;
}

void parity_game::solve_rec(region_t& subgame, unsigned max_parity,
                            region_t (&w)[2], strategy_t (&s)[2]) const
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
  auto strat_u = attractor(subgame, u, max_parity, p, true);

  if (max_parity == 0)
    {
      s[p].insert(strat_u.begin(), strat_u.end());
      return;
    }

  for (unsigned s: u)
    subgame.erase(s);
  region_t w0[2]; // Player's winning region in the first recursive call.
  strategy_t s0[2]; // Player's winning strategy in the first recursive call.
  solve_rec(subgame, max_parity - 1, w0, s0);
  if (w0[p].size() != subgame.size())
    for (unsigned s: subgame)
      if (w0[p].find(s) == w0[p].end())
        w0[!p].insert(s);
  subgame.insert(u.begin(), u.end());

  if (w0[p].size() + u.size() == subgame.size())
    {
      s[p].insert(s0[p].begin(), s0[p].end());
      s[p].insert(strat_u.begin(), strat_u.end());
      w[p].insert(subgame.begin(), subgame.end());
      return;
    }

  // Recursion on game size.
  auto strat_wnp = attractor(subgame, w0[!p], max_parity, !p);
  strat_wnp.insert(s0[!p].begin(), s0[!p].end());

  for (unsigned s: w0[!p])
    subgame.erase(s);

  region_t w1[2]; // Odd's winning region in the second recursive call.
  strategy_t s1[2]; // Odd's winning strategy in the second recursive call.
  solve_rec(subgame, max_parity, w1, s1);

  w[p].insert(w1[p].begin(), w1[p].end());
  s[p].insert(s1[p].begin(), s1[p].end());

  w[!p].insert(w0[!p].begin(), w0[!p].end());
  w[!p].insert(w1[!p].begin(), w1[!p].end());
  s[!p].insert(strat_wnp.begin(), strat_wnp.end());
  s[!p].insert(s1[!p].begin(), s1[!p].end());

  subgame.insert(w0[!p].begin(), w0[!p].end());
}

int reachability_state::compare(const state* other) const
{
  auto o = down_cast<const reachability_state*>(other);
  assert(o);
  if (num_ != o->num())
    return num_ - o->num();
  if (b_ < o->b())
    return -1;
  if (b_ > o->b())
    return 1;
  return 0;
}

bool reachability_state::operator<(const reachability_state& o) const
{
  // Heuristic to process nodes with a higher chance of leading to a target
  // node first.
  assert(b_.size() == o.b().size());
  for (unsigned i = b_.size(); i > 0; --i)
    if (b_[i - 1] != o.b()[i - 1])
      return b_[i - 1] > o.b()[i - 1];
  return num_ < o.num();
}



const reachability_state* reachability_game_succ_iterator::dst() const
{
  // NB: colors are indexed at 1 in Calude et al.'s paper and at 0 in spot
  // All acceptance sets are therefore incremented (which is already done by
  // max_set), so that 0 can be kept as a special value indicating that no
  // i-sequence is tracked at this index. Hence the parity switch in the
  // following implementation, compared to the paper.
  std::vector<unsigned> b = state_.b();
  unsigned a = it_->acc.max_set();
  assert(a);
  unsigned i = -1U;
  bool all_even = a % 2 == 0;
  for (unsigned j = 0; j < b.size(); ++j)
    {
      if ((b[j] % 2 == 1 || b[j] == 0) && all_even)
        i = j;
      else if (b[j] > 0 && a > b[j])
        i = j;
      all_even = all_even && b[j] > 0 && b[j] % 2 == 0;
    }
  if (i != -1U)
    {
      b[i] = a;
      for (unsigned j = 0; j < i; ++j)
        b[j] = 0;
    }
  return new reachability_state(it_->dst, b, !state_.anke());
}



const reachability_state* reachability_game::get_init_state() const
{
  // b[ceil(log(n + 1))] != 0 implies there is an i-sequence of length
  // 2^(ceil(log(n + 1))) >= 2^log(n + 1) = n + 1, so it has to contain a
  // cycle.
  unsigned i = std::ceil(std::log2(pg_.num_states() + 1));
  return new reachability_state(pg_.get_init_state_number(),
                                std::vector<unsigned>(i + 1),
                                false);
}

reachability_game_succ_iterator*
reachability_game::succ_iter(const state* s) const
{
  auto state = down_cast<const reachability_state*>(s);
  return new reachability_game_succ_iterator(pg_, *state);
}

std::string reachability_game::format_state(const state* s) const
{
  auto state = down_cast<const reachability_state*>(s);
  std::ostringstream fmt;
  bool first = true;
  fmt << state->num() << ", ";
  fmt << '[';
  for (unsigned b : state->b())
    {
      if (!first)
        fmt << ',';
      else
        first = false;
      fmt << b;
    }
  fmt << ']';
  return fmt.str();
}

bool reachability_game::is_reachable()
{
  std::set<spot::reachability_state> todo{*init_state_};
  while (!todo.empty())
    {
      spot::reachability_state v = *todo.begin();
      todo.erase(todo.begin());

      std::vector<spot::const_reachability_state_ptr> succs;
      spot::reachability_game_succ_iterator* it = succ_iter(&v);
      for (it->first(); !it->done(); it->next())
        succs.push_back(spot::const_reachability_state_ptr(it->dst()));

      if (is_target(v))
        {
          c_[v] = 1;
          if (mark(v))
            return true;
          continue;
        }
      else if (v.anke())
        c_[v] = 1;
      else
        c_[v] = succs.size();
      for (auto succ: succs)
        {
          if (parents_[*succ].empty())
            {
              if (*succ != *init_state_)
                {
                  todo.insert(*succ);
                  parents_[*succ] = { v };
                  c_[*succ] = -1U;
                }
            }
          else
            {
              parents_[*succ].push_back(v);
              if (c_[*succ] == 0 && mark(v))
                return true;
            }
        }
    }
  return false;
}

bool reachability_game::mark(const spot::reachability_state& s)
{
  if (c_[s] > 0)
    {
      --c_[s];
      if (c_[s] == 0)
        {
          if (s == *init_state_)
            return true;
          for (auto& u: parents_[s])
            if (mark(u))
              return true;
        }
    }
  return false;
}

bool reachability_game::is_target(const reachability_state& v)
{
  return v.b().back();
}

}
