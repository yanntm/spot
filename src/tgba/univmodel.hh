// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement de
// l'Epita.
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

#ifndef SPOT_TGBA_UNIVMODEL_HH
# define SPOT_TGBA_UNIVMODEL_HH

#include "tgba.hh"
#include "ltlvisit/apcollect.hh"

namespace spot
{
  /// \brief Build a universal model.
  ///
  /// Build a TGBA in which states are labeled all possible
  /// assignments of \a ap that are compatible with \a restrictval.  All
  /// states (but the initial one) have incoming transition labeled by
  /// the label of the state and coming from all existing states.
  ///
  ///@{
  SPOT_API tgba*
  universal_model(bdd_dict* d,
		  ltl::atomic_prop_set* ap, bdd restrictval = bddtrue);

  SPOT_API tgba*
  universal_model(bdd_dict* d,
		  ltl::atomic_prop_set* ap, const ltl::formula* restrictval);
  /// }@
}

#endif
