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

namespace spot
{
  
  namespace parity_game
  {
    typedef std::unordered_set<unsigned> region_old_t;
    // Map state number to index of the transition to take.
    typedef std::unordered_map<unsigned, unsigned> strategy_old_t;
  }
  
  /// \brief Takes a file describing a parity game
  /// in the pgsolver format and returns the corresponding
  /// twa.
  /// \note The twa has max odd acceptance and will be colorized
  /// \note pgsolver uses state-acceptance, however to be compatible with
  /// parity_game and ltlsynt, this is transformed to transition based acceptance
  SPOT_API std::pair<twa_graph_ptr, std::vector<bool>>
  parse_pg_file(const std::string& fname);
  
  /// \brief Preprocessing step for pg solving
  ///
  /// Takes an automaton that is already
  // (!) correctly split
  /// and computes the owner of each state. This information is stored
  /// as the named property "state-player"
  SPOT_API void
  make_arena(twa_graph_ptr& arena);
  
  /// \brief Solves the parity game
  ///
  /// Takes an arena given as an automaton
  /// and  computes the winning-region for player and environment
  /// as well as a (memoryless) winning-strategy for each player
  /// \param arena correctly split automaton
  /// \param owner vector indicating whether states are player or env owned
  /// \param w winning region
  /// \param s strategy
  /// \return whether the player wins the initial state
  SPOT_API bool
  solve_parity_game_old(const twa_graph_ptr& arena,
                        const std::vector<bool>& owner,
                        parity_game::region_old_t (&w)[2],
                        parity_game::strategy_old_t (&s)[2]);
  
  /// \brief
  ///
  /// Takes an arena given as an automaton
  /// and  computes the winning-region for player and environment
  /// as well as a (memoryless) winning-strategy for each player.
  /// These attributes are stored in the named properties
  /// winning-region and strategy
  /// \param arena correctly split automaton
  /// \return whether the player wins the initial state
  SPOT_API bool
  solve_parity_game(const twa_graph_ptr& arena);
  
  /// \brief  Reduces a solved parity to a strategy automaton
  ///
  /// Takes a solved parity game (with winning-region and
  /// strategy filled) and creates a new automaton that corresponds to
  /// the restriction of the initial automaton to the strategy
  /// \param arena Solved arena to be transformed
  /// \param all_outputs conjunction of all output aps
  /// \param do_purge whether or not to purge the resulting strategy automaton
  /// \param unsplit whether to merge intermediate state or keep the two player
  ///                layout
  /// \param keep_acc whether or not to keep acceptance conditions on the
  ///                 automaton and the edges
  /// \param leave_choice whether to select a minterm in the condition for
  //                      outs or leave the original condition
  SPOT_API twa_graph_ptr
  apply_strategy(const twa_graph_ptr& arena, bdd all_outputs,
                 bool do_purge=true, bool unsplit=true, bool keep_acc=false,
                 bool leave_choice=false);
}
