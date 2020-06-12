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

namespace spot
{
  /// \brief Preprocessing step for parity game solving
  ///
  /// Takes an automaton that is already (!) correctly split
  /// and computes the owner of each state. This information is stored
  /// as the named property "state-player"
  /// \param arena    automaton representing the arena
  /// \param outs     conjuction of all output ap.
  SPOT_API void
  make_arena(twa_graph_ptr& arena, bdd outs);

  /// \brief Solves the parity game
  ///
  /// Takes an arena given as an automaton
  /// and  computes the winning-region for player and environment
  /// as well as a (memoryless) winning-strategy for each player.
  /// These attributes are stored in the named properties
  /// "winning-region" and "strategy". To solve the game we use a variant of
  /// Zielonka's algorithm \cite zielonka.98.tcs inspired by the implementation
  /// \cite van2018oink
  /// \param arena correctly split automaton
  /// \return whether the player wins the initial state
  SPOT_API bool
  solve_parity_game(const twa_graph_ptr& arena);

  /// \brief  Reduces a solved parity to a strategy automaton
  ///
  /// Takes a solved parity game (with the named properties winning-region and
  /// strategy filled, "synthesis-outputs" must also be set) and creates a new
  /// automaton that corresponds to the restriction of the initial
  /// automaton to the strategy of the player
  /// \param arena Solved arena to be transformed
  /// \param unsplit whether to merge intermediate states or keep the two player
  ///                layout
  /// \param keep_acc whether or not to keep acceptance conditions on the
  ///                 automaton and the edges
  /// \param leave_choice whether to select a minterm in the condition for
  ///                      outs or leave the original condition
  SPOT_API twa_graph_ptr
  apply_strategy(const twa_graph_ptr& arena,
                 bool unsplit=true, bool keep_acc=false,
                 bool leave_choice=false);

  /// \ingroup twa_io
  /// \brief Prints the arena in parity-game file format
  ///
  /// Takes an arena and outputs it as a parity game. Attention, parity-games
  /// have state-based acceptance. To facilitate output,
  /// this function does _NOT_ really on the named property "state-owner"
  /// as all computations on arenas are done using transition-based acceptance.
  /// \param os   The output stream to print on.
  /// \param aut  arena to output
  SPOT_API std::ostream&
  print_pg(std::ostream& os, const twa_ptr& aut);
}
