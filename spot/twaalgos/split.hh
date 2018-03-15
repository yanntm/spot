// -*- coding: utf-8 -*-
// Copyright (C) 2017, 2018 Laboratoire de Recherche et Développement
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

#pragma once

#include <spot/twa/twagraph.hh>

namespace spot
{
  /// \ingroup twa_misc
  /// \brief transform edges into transitions
  ///
  /// Create a new version of the automaton where all edges are split
  /// so that they are all labeled by a conjunction of all atomic
  /// propositions.  After this we can consider that each edge of the
  /// automate is a transition labeled by one letter.
  SPOT_API twa_graph_ptr split_edges(const const_twa_graph_ptr& aut);

  // Take an automaton and a set of atomic propositions I, and split each
  // transition
  //
  //     p -- cond --> q                cond in 2^2^AP
  //
  // into a set of transitions of the form
  //
  //     p -- i1 --> r1 -- o1 --> q     i1 in 2^I
  //                                    o1 in 2^O
  //
  //     p -- i2 --> r2 -- o2 --> q     i2 in 2^I
  //                                    o2 in 2^O
  //     ...
  //
  // where O = AP\I, and such that cond = (i1 & o1) | (i2 & o2) | ...
  //
  // When determinized, this encodes a game automaton that has a winning
  // strategy iff aut has an accepting run for any valuation of atomic
  // propositions in I.
  SPOT_API twa_graph_ptr
  split_automaton(const const_twa_graph_ptr& aut, bdd input_bdd);
}
