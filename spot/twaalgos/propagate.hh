// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita.
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
  /// \brief Propagate acceptance marks.
  ///
  /// This algorithm pushes on outgoing and incoming edges all marks that appear
  /// on every incoming edge or on every outgoing edge (from and to the same
  /// SCC). Marks are not propagated on transitions between SCCs.
  /// This propagation is expected to help the simulation.
  /// Currently, this function does not work on alternating automata.
  SPOT_API twa_graph_ptr
  propagate_acc(const const_twa_graph_ptr& aut);
}
