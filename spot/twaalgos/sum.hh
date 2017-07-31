// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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
  /// \brief Sum two twa into a new twa. \a sum performs the union of the
  /// (languages of the) input automata, while \a sum_and performs their
  /// intersection.
  ///
  /// The sum is performed by adding a new initial state and a transition from
  /// this new initial states to the initial states of the arguments. \a uses an
  /// existential transition, and \a sum_and uses a universal one.
  /// The acceptance condition is the disjunction of those of the arguments.
  /// If both input automata are TGBA, the output automaton is also a TGBA,
  /// whose acceptance condition is the conjunction of the input acceptance
  /// conditions. This reduces the number of acceptance marks from
  ///   left->num_sets() + right->num_sets()
  /// to
  ///   max(left->num_sets(), right->num_sets())
  ///
  /// \a sum and \a sum_and also have a version to specify explicitely the
  /// states to be considered as initial. This is useful when considering the
  /// language recognized from a given state.
  ///
  /// \param left Left-hand-side term of the sum.
  /// \param right Right-hand-side term of the sum.
  /// \param left_state Initial state of the lhs term of the sum.
  /// \param right_state Initial state of the rhs term of the sum.
  /// \return A spot::twa_graph containing the sum of \a left and \a right.
  /// \@{
  SPOT_API
  twa_graph_ptr sum(const const_twa_graph_ptr& left,
                    const const_twa_graph_ptr& right,
                    unsigned left_state,
                    unsigned right_state);

  SPOT_API
  twa_graph_ptr sum(const const_twa_graph_ptr& left,
                    const const_twa_graph_ptr& right);

  SPOT_API
  twa_graph_ptr sum_and(const const_twa_graph_ptr& left,
                        const const_twa_graph_ptr& right,
                        unsigned left_state,
                        unsigned right_state);

  SPOT_API
  twa_graph_ptr sum_and(const const_twa_graph_ptr& left,
                        const const_twa_graph_ptr& right);
  /// \@}
}
