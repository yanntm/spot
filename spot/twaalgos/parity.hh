// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et Développement
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

#include <spot/misc/common.hh>
#include <spot/twa/fwd.hh>

namespace spot
{
  /// \ingroup twa_algorithms
  /// \brief Parity acceptation change order type
  enum parity_order
  {
    parity_order_max,      /// The new acceptance will be a parity max
    parity_order_min,      /// The new acceptance will be a parity min
    parity_order_same,     /// The new acceptance will keep the order
    parity_order_dontcare  /// The new acceptance may change the order
  };

  /// \brief Parity acceptation change style type
  enum parity_style
  {
    parity_style_odd,      /// The new acceptance will be a parity odd
    parity_style_even,     /// The new acceptance will be a parity even
    parity_style_same,     /// The new acceptance will keep the style
    parity_style_dontcare  /// The new acceptance may change the style
  };

  /// \brief Change the parity acceptance of an automaton
  ///
  /// The parity acceptance condition of an automaton is characterized by
  ///    - The priority order of the acceptance sets (min or max).
  ///    - The parity style, i.e. parity of the sets seen infinitely often
  //       (odd or even).
  ///    - The number of acceptance sets.
  ///
  /// The output will be an equivalent automaton with the new parity acceptance.
  /// The automaton must have a parity acceptance. The number of acceptance sets
  /// may be increased by one. Every transition will belong to at most one
  /// acceptance set.
  ///
  /// The parity order is defined only if the number of acceptance sets
  /// is strictly greater than 1. The parity_style is defined only if the number
  /// of acceptance sets is non-zero. Some values of order and style may result
  /// in equivalent ouputs if the number of acceptance sets of the input
  /// automaton is not great enough.
  ///
  /// \param aut the input automaton
  ///
  /// \param order the parity order of the output automaton
  ///
  /// \param style the parity style of the output automaton
  ///
  /// \return the automaton with the new acceptance
  SPOT_API twa_graph_ptr
  change_parity(const const_twa_graph_ptr& aut,
                parity_order order, parity_style style);

  /// \brief Colorize automaton
  ///
  /// \param aut the input automaton
  ///
  /// \param keep_style whether the style of the parity acc is kept.
  ///
  /// \return the colorized automaton
  /// @{
  SPOT_API twa_graph_ptr
  colorize_parity(const const_twa_graph_ptr& aut, bool keep_style = false);

  SPOT_API twa_graph_ptr
  colorize_parity_here(twa_graph_ptr aut, bool keep_style = false);
  /// @}
}
