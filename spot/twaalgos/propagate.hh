// -*- coding: utf-8 -*-
// Copyright (C)  2017 Laboratoire de Recherche et Développement de l'Epita
// (LRDE).
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
  /// \brief propagate acceptance marks
  ///
  /// This algorithm is composed of the following steps:
  /// 2. For every states, five  of transitions are built:
  ///   - incoming transitions in the scc
  //    - outgoing transitions
  //    - self-loops
  //  3. If a mark is common to all the incoming transitions of a state, this
  //  mark is propagated to all the outgoing transitions of this state.
  //  4. If a mark is common to all the outgoing transitions of a state, this
  //  mark is propagated to all the incoming transitions of this state.
  //  5. If a state has no outgoing transitions, all the marks on self-loops
  //  can be propagated to incoming transitions
  //  6. If a state has no incoming transitions, all the marks on self-loops
  //  can be propagated to outgoing transitions
  //  7. Redo the steps 2-6 until a fixpoint is found.
  /// @{
  twa_graph_ptr propagate_here(const const_twa_graph_ptr& aut);
  twa_graph_ptr propagate(twa_graph_ptr aut);
  /// @}
}
