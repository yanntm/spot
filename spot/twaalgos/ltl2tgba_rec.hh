// -*- coding: utf-8 -*-
// Copyright (C) 2013-2017 Laboratoire de Recherche et Développement
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

#include <spot/twa/twagraph.hh>

namespace spot
{
  /*
   * TODO Should those functions be exposed?
  SPOT_API
  twa_graph_ptr
  make_ap(const formula& ap, const bdd_dict_ptr& dict);

  SPOT_API
  twa_graph_ptr
  make_next(const twa_graph_ptr& aut);

  SPOT_API
  twa_graph_ptr
  make_and(const twa_graph_ptr& lhs, const twa_graph_ptr& rhs);

  SPOT_API
  twa_graph_ptr
  make_or(const twa_graph_ptr& lhs, const twa_graph_ptr& rhs);
  */

  SPOT_API
  twa_graph_ptr
  make_until(const twa_graph_ptr& lhs, const twa_graph_ptr& rhs,
             bool rem_alt = true);

  SPOT_API
  twa_graph_ptr
  make_weak_until(const twa_graph_ptr& lhs, const twa_graph_ptr& rhs,
                  bool rem_alt = true);

  SPOT_API
  twa_graph_ptr
  make_release(const twa_graph_ptr& lhs, const twa_graph_ptr& rhs,
               bool strict, bool rem_alt);

  /// Recursive translation of ltl formula to TGBA.
  ///
  /// The translation does not handle all operators yet: only X, U, W, && and ||
  /// are currently handled. The formula will be rewritten to remove unhandled
  /// operators, and be put in negative normal form.
  SPOT_API
  twa_graph_ptr
  ltl_to_tgba_rec(const formula&, const bdd_dict_ptr&);
}

