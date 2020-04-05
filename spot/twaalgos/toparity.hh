// -*- coding: utf-8 -*-
// Copyright (C) 2018-2020 Laboratoire de Recherche et Développement
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
  /// \ingroup twa_acc_transform
  /// \brief Options to control various optimizations of to_parity().
  struct to_parity_options
  {
    /// If \c search_ex is true, whenever CAR or IAR have to move
    /// several elements in a record, it tries to find an order such
    /// that the new permutation already exists.
    bool search_ex      = true;
    /// If \a use_last is true and \a search_ex are true, we use the
    /// most recent state when we find multiple existing state
    /// compatible with the current move.
    bool use_last       = true;
    bool force_order    = true;
    /// If \c partial_degen is true, apply a partial
    /// degeneralization to remove occurrences of acceptance
    /// subformulas such as `Fin(x) | Fin(y)` or `Inf(x) & Inf(y)`.
    bool partial_degen  = true;
    /// If \c scc_acc_clean is true, to_parity() will ignore colors
    /// no occoring in an SCC while processing this SCC.
    bool acc_clean      = true;
    /// If \a parity_equiv is true, to_parity() will check if there
    /// exists a permutations of colors such that the acceptance
    /// condition is a parity condition.
    bool parity_equiv   = true;
    bool parity_prefix  = true;
    bool rabin_to_buchi = true;
    bool reduce_col_deg = false;
    bool propagate_col  = true;
    /// If \a pretty_print is true, states of the output automaton are
    /// named to help debugging.
    bool pretty_print   = false;
  };



  /// \ingroup twa_acc_transform
  /// \brief Take an automaton with any acceptance condition and return an
  /// equivalent parity automaton.
  ///
  /// The parity condition of the returned automaton is either max even or
  /// max odd.
  ///
  /// This procedure combines many strategies in an attempt to produce
  /// the smallest possible parity automaton.  Some of the strategies
  /// include CAR (color acceptance record), IAR (index appearance
  /// record), partial degenerazation, conversion from Rabin to Büchi
  /// when possible, etc.
  ///
  /// The \a options argument can be used to selectively disable some of the
  /// optimizations.
  SPOT_API twa_graph_ptr
  to_parity(const twa_graph_ptr &aut,
            const to_parity_options options = to_parity_options());

  /// \ingroup twa_acc_transform
  /// \brief Take an automaton with any acceptance condition and return an
  /// equivalent parity automaton.
  ///
  /// The parity condition of the returned automaton is max even.
  ///
  /// This implements a straightforward adaptation of the LAR (latest
  /// appearance record) to automata with transition-based marks.  We
  /// call this adaptation the CAR (color apperance record), as it
  /// tracks colors (i.e., acceptance sets) instead of states.
  ///
  /// It is better to use to_parity() instead, as it will use better
  /// strategies when possible, and has additional optimizations.
  SPOT_API twa_graph_ptr
  to_parity_old(const const_twa_graph_ptr& aut, bool pretty_print = false);

  /// \ingroup twa_acc_transform
  /// \brief Turn a Rabin-like or Streett-like automaton into a parity automaton
  /// based on the index appearence record (IAR)
  ///
  /// This is an implementation of \cite kretinsky.17.tacas .
  /// If the input automaton has n states and k pairs, the output automaton has
  /// at most k!*n states and 2k+1 colors. If the input automaton is
  /// deterministic, the output automaton is deterministic as well, which is the
  /// intended use case for this function. If the input automaton is
  /// non-deterministic, the result is still correct, but way larger than an
  /// equivalent Büchi automaton.
  ///
  /// If the input automaton is Rabin-like (resp. Streett-like), the output
  /// automaton has max odd (resp. min even) acceptance condition.
  ///
  /// Throws an std::runtime_error if the input is neither Rabin-like nor
  /// Street-like.
  ///
  /// It is better to use to_parity() instead, as it will use better
  /// strategies when possible, and has additional optimizations.
  SPOT_DEPRECATED("use to_parity() instead") // deprecated since Spot 2.9
  SPOT_API twa_graph_ptr
  iar(const const_twa_graph_ptr& aut, bool pretty_print = false);

  /// \ingroup twa_acc_transform
  /// \brief Turn a Rabin-like or Streett-like automaton into a parity automaton
  /// based on the index appearence record (IAR)
  ///
  /// Returns nullptr if the input automaton is neither Rabin-like nor
  /// Streett-like, and calls spot::iar() otherwise.
  SPOT_DEPRECATED("use to_parity() and spot::acc_cond::is_rabin_like() instead")
  SPOT_API twa_graph_ptr   // deprecated since Spot 2.9
  iar_maybe(const const_twa_graph_ptr& aut, bool pretty_print = false);

} // namespace spot
