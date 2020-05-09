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
  
  /// \brief Takes an automaton that is already(!) correctly split
  /// and computes the owner of each state. This information is stored
  /// as the named property "owner"
  SPOT_API void
  make_arena(twa_graph_ptr& arena);
  
  SPOT_API bool
  solve_parity_game_old(const twa_graph_ptr& arena,
                        const std::vector<bool>& owner,
                        parity_game::region_old_t (&w)[2],
                        parity_game::strategy_old_t (&s)[2]);
  
  SPOT_API bool
  solve_parity_game(const twa_graph_ptr& arena);
  
  /// \brief Takes a solved parity game (with winning region and
  /// strategy filled) and creates a new automaton that corresponds to
  /// the restriction of the initial automaton to the strategy
  SPOT_API twa_graph_ptr
  apply_strategy(const twa_graph_ptr& arena, bdd all_outputs,
                 bool do_purge=true, bool unsplit=true, bool keep_acc=false);
}
