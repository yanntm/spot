// Copyright (C) 2011, 2012 Laboratoire de Recherche et Developpement de
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

#ifndef SPOT_TGBAALGOS_BDDSIMUL_HH
# define SPOT_TGBAALGOS_BDDSIMUL_HH

#include "tgba/tgbaexplicit.hh"

namespace spot
{
  /// \brief Reduce a TGBA using by simulation.
  ///
  /// This simulation-based reduction uses a symbolic (BDD-based)
  /// representation of the input automaton \a a to construct a
  /// simulation relation between its states, and construct a smaller
  /// automaton.
  tgba_explicit* bdd_simulation(const tgba* a);
}

#endif // SPOT_TGBAALGOS_BDDSIMUL_HH
