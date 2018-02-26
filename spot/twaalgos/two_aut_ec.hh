// -*- coding: utf-8 -*-
// Copyright (C) 2017-2018 Laboratoire de Recherche et
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
#include <spot/twa/twa.hh>

namespace spot
{
  /// \brief Check whether the languages of the two automata intersect.
  /// \param ar_l if not null and language is not empty, fill it with an
  //              accepting run over the left automaton.
  /// \param ar_r if not null and language is not empty, fill it with an
  //              accepting run over the right automaton.
  SPOT_API
  bool two_aut_ec(const const_twa_ptr& left, const const_twa_ptr& right,
                  const twa_run_ptr ar_l = nullptr,
                  const twa_run_ptr ar_r = nullptr);
}
