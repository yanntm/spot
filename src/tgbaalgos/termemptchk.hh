// Copyright (C) 2013 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
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

#ifndef SPOT_TGBAALGOS_TERMEMPTCHK_HH
# define SPOT_TGBAALGOS_TERMEMPTCHK_HH

#include <cstddef>
#include "misc/optionmap.hh"

namespace spot
{
  class tgba;
  class emptiness_check;

  /// \addtogroup emptiness_check_algorithms
  /// @{

  /// \brief Returns an emptiness check on the spot::tgba automaton \a a.
  ///
  /// This emptiness algorithm work only on terminal automaton i.e.
  /// terminal automaton or terminal property automaton synchronized
  /// with a left total system.
  emptiness_check* terminal_search(const tgba *a,
				   option_map o = option_map());
  /// @}
}

#endif // SPOT_TGBAALGOS_TERMEMPTCHK_HH
