// Copyright (C) 2012 Laboratoire de Recherche et Développement de
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

#ifndef SPOT_TGBAALGOS_ACCPOSTPROC_HH
# define SPOT_TGBAALGOS_ACCPOSTPROC_HH

#include "tgba/tgba.hh"
#include "tgbaalgos/scc.hh"
#include "ltlenv/declenv.hh"

namespace spot
{

  /// \addtogroup tgba_reduction
  /// @{

  /// This method add a fake transition condition
  /// on all Strong SCC
  const tgba* add_fake_acceptance_condition (const tgba *a,
					      ltl::declarative_environment* env,
					      spot::scc_map* sm = 0);


  ltl::declarative_environment* create_env_acc()
  {
    ltl::declarative_environment* envacc =
      new spot::ltl::declarative_environment ();
    envacc->declare("[S]");
    envacc->declare("[W]");
    envacc->declare("[T]");
    return envacc;
  }

  /// @}
}

#endif // SPOT_TGBAALGOS_ACCPOSTPROC_HH
