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

#include <memory>

#include <spot/misc/common.hh>
#include <spot/twa/fwd.hh>
#include <spot/twa/twa.hh>

namespace spot
{
  /// \addtogroup emptiness_check_algorithms
  /// @{

  /// \brief The result of a two-automaton emptiness check.
  ///
  /// Instances of this class are created when the languages of two automata
  /// fed into the two-automaton emptiness check intersect. Instances of this
  /// class should not live longer than these automata.
  class SPOT_API two_aut_res
  {
  public:
    two_aut_res()
    {
    }

    virtual ~two_aut_res()
    {
    }

    /// \brief Retrieves equivalent accepting runs over the two automata.
    virtual std::pair<twa_run_ptr, twa_run_ptr> accepting_runs() const = 0;

    /// \brief Retrieves an accepting run over the left automaton.
    ///
    /// Equivalent to
    /// \code
    ///   this.accepting_runs().first;
    /// \endcode
    virtual twa_run_ptr left_accepting_run() const = 0;

    /// \brief Retrieves an accepting run over the right automaton.
    ///
    /// Equivalent to
    /// \code
    ///   this.accepting_runs().second;
    /// \endcode
    virtual twa_run_ptr right_accepting_run() const = 0;
  };

  typedef std::shared_ptr<two_aut_res> two_aut_res_ptr;

  /// \brief Check whether the languages of the two automata intersect.
  ///
  /// \return An null std::shared_ptr if the languages do not intersect, a
  /// shared pointer to an instance of spot::two_aut_res otherwise.
  SPOT_API
  two_aut_res_ptr
  two_aut_ec(const const_twa_ptr& left, const const_twa_ptr& right);

  /// @}
}
