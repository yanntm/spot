// Copyright (C) 2012 Laboratoire de Recherche et Développement de
// l'Epita.
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
// 02111-1307, USA

#ifndef SPOT_TGBAALGOS_DEGENERALIZE_HH
# define SPOT_TGBAALGOS_DEGENERALIZE_HH

#include "tgba/tgbaexplicit.hh"

namespace spot
{
  /// \brief Degeneralize a spot::tgba, producing a TBA.
  /// \ingroup tgba_algorithm
  ///
  /// The degeneralization is done by synchronizing the input
  /// automaton with a "counter" automaton such as the one shown in
  /// "On-the-fly Verification of Linear Temporal Logic" (Jean-Michel
  /// Couveur, FME99).
  ///
  /// If the input automaton uses N acceptance conditions, the output
  /// automaton can have at most max(N,1) times more states and
  /// transitions.
  ///
  tgba_explicit_number* degeneralize_tba(tgba* a);

  //put tgbaexplicit here?

  /// \brief Degeneralize a spot::tgba, producing a SBA.
  /// \ingroup tgba_algorithm
  ///
  /// This version produces a state based buchi automaton
  sba_explicit_number* degeneralize_sba(tgba* a);
}


#endif // !SPOT_TGBAALGOS_DEGENERALIZATION_HH 
