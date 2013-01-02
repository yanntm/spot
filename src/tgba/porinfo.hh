// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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

#ifndef SPOT_TGBA_PORINFO_HH
# define SPOT_TGBA_PORINFO_HH

#include "ltlast/atomic_prop.hh"
#include "tgba/state.hh"

namespace spot
{
  typedef std::list<ltl::atomic_prop*> prop_list;

  class por_info
  {
  public:
    // virtual bool visited(const state* s) = 0;
    // virtual prop_list visible_prop() = 0;

    bdd cur_ap;
  };
}

#endif /// SPOT_TGBA_PORINFO_HH
