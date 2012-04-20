// Copyright (C) 2012 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#ifndef SPOT_TGBAALGOS_PROPAGATION_HH
# define SPOT_TGBAALGOS_PROPAGATION_HH

namespace spot
{
  class tgba;

  /// \brief Propagate acceptance conditions through the automaton.
  ///
  /// If all output arcs of a state possess the same acceptance condition
  /// then it can be put on all input arcs.
  ///
  /// \param a the automaton on which to propagate acceptance conditions.
  /// \return a new automaton
  tgba* propagate_acceptance_conditions(const tgba* a);

  /// \brief Propagate acceptance conditions through the automaton, in place.
  ///
  /// If all output arcs of a state possess the same acceptance condition
  /// then it can be put on all input arcs.
  ///
  /// This function directly modify the given automaton if it is an explicit
  /// automaton.  Otherwise, it creates a new automaton but does not
  /// destroy the old one.  You can tell if a new automaton has been created
  /// by checking whether the returned value is equal to \a a.
  ///
  /// \param a the automaton on which to propagate acceptance conditions.
  /// \return a new automaton, or the input automaton modified in place.
  tgba* propagate_acceptance_conditions_inplace(tgba* a);

}

#endif /// SPOT_TGBAALGOS_PROPAGATION_HH
