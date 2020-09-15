// -*- coding: utf-8 -*-
// Copyright (C) 2017-2020 Laboratoire de Recherche et Développement
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

#include <bddx.h>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/parity.hh>

namespace spot
{

  /// \brief Transform an automaton into a parity game by propagating
  /// players
  ///
  /// This propagate state players, assuming the initial state belong
  /// to \a first_player, and alternating players on each transitions.
  /// If an odd cycle is detected, a runtime_exception is raised.
  ///
  /// If \a complete0 is set, ensure that states of player 0 are
  /// complete.
  SPOT_API
  void propagate_players(spot::twa_graph_ptr& arena,
                         bool first_player = false,
                         bool complete0 = true);


  typedef std::unordered_set<unsigned> region_t;
  typedef std::unordered_map<unsigned, unsigned> strategy_t;


  struct SPOT_API solved_game
  {
    const_twa_graph_ptr arena;

    region_t winning_region[2];
    strategy_t winning_strategy[2];

  /// \brief Highlight the edges of a strategy on the automaton.
    twa_graph_ptr highlight_strategy(unsigned player, unsigned color);

    bool player_winning_at(unsigned player, unsigned state)
    {
      auto& w = winning_region[player];
      return w.find(state) != w.end();
    }
  };

  /// \brief solve a parity-game
  ///
  /// The arena is a deterministic max odd parity automaton with a
  /// "state-player" property.
  ///
  /// This computes the winning strategy and winning region of this
  /// game for player 1 using Zielonka's recursive algorithm.
  /// \cite zielonka.98.tcs
  SPOT_API
  solved_game parity_game_solve(const const_twa_graph_ptr& arena);

  /// \brief Print a max odd parity game using PG-solver syntax
  SPOT_API
  void pg_print(std::ostream& os, const const_twa_graph_ptr& arena);


  /// \brief Highlight the edges of a strategy on an automaton.
  SPOT_API
  twa_graph_ptr highlight_strategy(twa_graph_ptr& arena,
                                   const strategy_t& s,
                                   unsigned color);

  /// \brief Set the owner for all the states.
  SPOT_API
  void set_state_players(twa_graph_ptr arena, std::vector<bool> owners);
  SPOT_API
  void set_state_players(twa_graph_ptr arena, std::vector<bool>* owners);

  /// \brief Set the owner of a state.
  SPOT_API
  void set_state_player(twa_graph_ptr arena, unsigned state, unsigned owner);

  /// \brief Get the owner of all the state.
  SPOT_API
  const std::vector<bool>& get_state_players(const_twa_graph_ptr arena);

  /// \brief Get the owner of a state.
  SPOT_API
  unsigned get_state_player(const_twa_graph_ptr arena, unsigned state);
}
