// Copyright (C) 2012 Laboratoire de Recherche et Développement
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



#ifndef SPOT_FASTTGBAALGOS_TGBA2FASTTGBA_HH
# define SPOT_FASTTGBAALGOS_TGBA2FASTTGBA_HH

#include "tgba/tgba.hh"
#include "tgbaalgos/scc.hh"
#include "tgbaalgos/isweakscc.hh"

#include "fasttgba/ap_dict.hh"
#include "fasttgba/acc_dict.hh"
#include "fasttgba/fasttgba.hh"

namespace spot
{

  /// \brief Perform a translation from a Tgba to a Fasttgba
  ///
  /// This method is the only method that should use old Tgba
  /// \a param t the source tgba
  /// \a param aps the dictionnary of atomic propositions
  /// \a param accs the dictionnary of acceptance set
  SPOT_API const fasttgba*
  tgba_2_fasttgba(const spot::tgba* t,
		  spot::ap_dict& aps,
		  spot::acc_dict& accs);

}



#endif // SPOT_FASTTGBAALGOS_TGBA2FASTTGBA_HH
