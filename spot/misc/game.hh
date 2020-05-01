// -*- coding: utf-8 -*-
// Copyright (C) 2017-2019 Laboratoire de Recherche et Développement
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

#pragma once

#include <algorithm>
#include <memory>
#include <ostream>
#include <unordered_map>
#include <vector>
#include <limits>

#include <bddx.h>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/parity.hh>
#include <spot/twaalgos/sccinfo.hh>

#include <chrono>

namespace parity_game_helper{
  struct winner_t{
    std::vector<bool> has_winner_;
    std::vector<bool> winner_;
    
    inline bool operator()(unsigned v, bool p);
    inline void set(unsigned v, bool p);
    inline void unset(unsigned v);
    bool winner(unsigned v);
  };
}

namespace spot
{
  class SPOT_API parity_game
      {
          private:
          const twa_graph_ptr arena_;
          const std::vector<bool>& owner_;
          
          // winning regions for env and player
          mutable unsigned rd_;
          mutable parity_game_helper::winner_t w_;
          // Subgame array similar to the one from oink!
          mutable std::vector<unsigned> subgame_;
          // strategies for env and player; For synthesis only player is needed
          // We need a signed value here in order to "fix" the strategy
          // during construction
          mutable std::vector<long long> s_;
          const unsigned unseen_mark = std::numeric_limits<unsigned>::max();
          
          // When using scc decomposition we need to track the
          // changes made to the graph
          struct edge_stash_t{
            edge_stash_t(unsigned num, unsigned dst, acc_cond::mark_t acc):
            e_num(num), e_dst(dst), e_acc(acc){};
            unsigned e_num, e_dst;
            acc_cond::mark_t e_acc;
          };
          
          mutable unsigned max_abs_par_;
          mutable scc_info* info_;
          // Info on the current scc
          mutable std::vector<scc_info_node>::const_iterator c_scc_;
          mutable unsigned c_scc_idx_;
          // Fixes made to the sccs
          mutable std::vector<edge_stash_t> change_stash_;
          mutable size_t call_count_, direct_solve_count_;
          // Struct to change recursive calls to stack
          struct work_t{
            work_t(unsigned wstep_, unsigned rd_, unsigned min_par_, unsigned max_par_):
            wstep(wstep_), rd(rd_), min_par(min_par_), max_par(max_par_){};
            unsigned wstep, rd, min_par, max_par;
          };
          mutable std::vector<work_t> w_stack_;
          public:
          /// \a parity_game provides an interface to manipulate a colorized parity
          /// automaton as a parity game, including methods to solve the game.
          /// The input automaton (arena) should be colorized and have a max-odd parity
          /// acceptance condition.
          ///
          /// \param arena the underlying parity automaton
          /// \param owner a vector of Booleans indicating the owner of each state:
          ///   true stands for Player 1, false stands for Player 0.
          parity_game(const twa_graph_ptr& arena, const std::vector<bool>& owner);
          
          unsigned num_states() const
          {
            return arena_->num_states();
          }
          
          unsigned get_init_state_number() const
          {
            return arena_->get_init_state_number();
          }
          
          internal::state_out<twa_graph::graph_t>
          out(unsigned src) const
          {
            return arena_->out(src);
          }
          
          internal::state_out<twa_graph::graph_t>
          out(unsigned src)
          {
            return arena_->out(src);
          }
          
          bool owner(unsigned src) const
          {
            return owner_[src];
          }
          
          unsigned max_parity() const
          {
            unsigned max_parity = 0;
            for (const auto& e: arena_->edges())
              max_parity = std::max(max_parity, e.acc.max_set());
            SPOT_ASSERT(max_parity);
            return max_parity - 1;
          }
          
          /// Print the parity game in PGSolver's format.
          void print(std::ostream& os);
          
          typedef std::unordered_set<unsigned> region_t;
          // Map state number to index of the transition to take.
          typedef std::unordered_map<unsigned, unsigned> strategy_t;
          
          // false -> env, true -> player
          typedef std::vector<bool> region_alt_t;
          // state idx -> global edge number
          typedef std::vector<unsigned> strategy_alt_t;
          
          /// Compute the winning strategy and winning region of this game for player
          /// 1 using Zielonka's recursive algorithm. \cite zielonka.98.tcs
          bool solve(region_t (&w)[2], strategy_t (&s)[2]) const;
          
          // Alternative solving algorithm
          // Also zielonka but based on vectors not maps
          // supports scc decomposition and priority compression
          bool solve_alt(region_alt_t& w, strategy_alt_t& s) const;
          
          private:
          typedef twa_graph::graph_t::edge_storage_t edge_t;
          
          // Compute (in place) a set of states from which player can force a visit
          // through set, and a strategy to do it.
          // if attr_max is true, states that can force a visit through an edge with
          // max parity are also counted in.
          strategy_t attractor(const region_t& subgame, region_t& set,
          unsigned max_parity, int odd,
          bool attr_max = false) const;
          
          // Compute the winning strategy and winning region for both players.
          void solve_rec(region_t& subgame, unsigned max_parity,
          region_t (&w)[2], strategy_t (&s)[2]) const;
          
          // Compute the winning strategy and winning region for both players.
          // Strategy for player includes a heuristic to reduce the size
          // of the circuit
          inline void zielonka() const;
          // Compute the attractor
          // Edge larger than max_par are disregarded
          // if acc_par is true, all edge with min_win_par <= parity(acc) <= max_par
          // are considered accepting
          inline bool attr(unsigned& rd, bool p, unsigned max_par,
          bool acc_par, unsigned min_win_par) const;
          void set_up() const;
          // After recursion, check if strategy is still winning
          // and try to fix if no longer winning
          // if not -> states got "stolen"
          inline bool fix_strat_acc(unsigned rd, bool p, unsigned min_win_par,
          unsigned max_par) const;
          // Restore original graph
          inline void restore()const;
          // Check if "directly" winning, that is all states
          // belong to the attractor of an outgoing edge
          inline std::pair<bool, std::set<unsigned, std::greater<unsigned>>> inspect_scc(unsigned max_par)const;
          // Replace outgoing edges with self-loops of correct parity
          inline std::pair<bool, std::set<unsigned, std::greater<unsigned>>> fix_scc()const;
      };
}
