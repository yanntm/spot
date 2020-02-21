// -*- coding: utf-8 -*-
// Copyright (C) 2012-2019 Laboratoire de Recherche
// et Développement de l'Epita (LRDE).
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
/// \ingroup twa_acc_transform
/// \brief Take an automaton with any acceptance condition and return an
/// equivalent parity automaton.
///
/// The parity condition of the returned automaton is max even or
/// max odd.
/// If \a search_ex is true, when we move several elements, we
/// try to find an order  such that the new permutation already exists.
/// If \a partial_degen is true, we apply a partial degeneralization to remove
// occurences of Fin | Fin and Inf & Inf.
/// If \a scc_acc_clean is true, we remove for each SCC the colors that don't
// appear.
/// If \a parity_equiv is true, we check if there exists a permutations of
// colors such that the acceptance
/// condition is a partity condition.
/// If \a use_last is true, we use the most recent state when looking for an
// existing state.
/// If \a pretty_print is true, we give a name to the states describing the
// state of the aut_ and the permutation.

  SPOT_API twa_graph_ptr
  remove_false_transitions(const twa_graph_ptr a);

  SPOT_API twa_graph_ptr
  car(const twa_graph_ptr &aut,
    bool search_ex = true,
    bool partial_degen = true,
    bool scc_acc_clean = true,
    bool parity_equiv = true,
    bool use_last = true,
    bool parity_prefix = true,
    bool rabin_to_buchi = true,
    bool pretty_print = false);
} // namespace spot