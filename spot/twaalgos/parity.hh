// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et Développement
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

#include <spot/misc/common.hh>
#include <spot/twa/fwd.hh>

namespace spot
{
  /// \ingroup twa_algorithms
  /// \brief Change the parity acceptance of an automaton
  ///
  /// The parity acceptance condition of an automaton is characterized by
  ///    - The priority order of the acceptance sets (min or max).
  ///    - The parity of the sets seen infinitely often (odd or even).
  ///    - The number of acceptance sets.
  ///
  /// The output will be an equivalent automaton with the new parity acceptance.
  /// The automaton must have a parity acceptance. The number of acceptance sets
  /// may be increased by one. Every transition will belong to at most one
  /// acceptance set.
  ///
  /// \param aut the input parity automaton
  ///
  /// \param max whether the priority order of the acceptance sets will be max.
  ///
  /// \param odd whether the parity of the sets seen infinitely often will be
  ///            odd.
  SPOT_API
  twa_graph_ptr change_parity_acceptance(const const_twa_graph_ptr& aut,
                                         bool max,
                                         bool odd);
}
