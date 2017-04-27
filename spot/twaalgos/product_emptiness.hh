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

  /// \brief The result of a product emptiness check.
  ///
  /// Instances of this class are created when the languages of two automata
  /// fed into the product emptiness check intersect. Instances of this class
  /// should not live longer than these automata.
  class SPOT_API product_emptiness_res
  {
  public:
    product_emptiness_res()
    {
    }

    virtual ~product_emptiness_res()
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

  typedef std::shared_ptr<product_emptiness_res> product_emptiness_res_ptr;

  /// \brief Check whether the languages of the two automata intersect.
  ///
  /// This is based on the following paper.
  /** \verbatim
      @InProceedings{couvreur.99.fm,
        author    = {Jean-Michel Couvreur},
        title     = {On-the-fly Verification of Temporal Logic},
        pages     = {253--271},
        editor    = {Jeannette M. Wing and Jim Woodcock and Jim Davies},
        booktitle = {Proceedings of the World Congress on Formal Methods in
                     the Development of Computing Systems (FM'99)},
        publisher = {Springer-Verlag},
        series    = {Lecture Notes in Computer Science},
        volume    = {1708},
        year      = {1999},
        address   = {Toulouse, France},
        month     = {September},
        isbn      = {3-540-66587-0}
      }
      \endverbatim */
  ///
  /// \return An null std::shared_ptr if the languages do not intersect, a
  /// shared pointer to an instance of spot::product_emptiness_res otherwise.
  SPOT_API
  product_emptiness_res_ptr
  product_emptiness_check(const const_twa_ptr& left,
                          const const_twa_ptr& right);

  /// \brief Check whether the languages of the two automata intersect.
  ///
  /// This variant forces the initial states used in the emptiness check
  /// exploration and the accepting run exploration done by the
  /// product_emptiness_res.
  ///
  /// \note The languages considered will reflect the change in initial states.
  ///
  /// \return An null std::shared_ptr if the languages do not intersect, a
  /// shared pointer to an instance of spot::product_emptiness_res otherwise.
  SPOT_API
  product_emptiness_res_ptr
  product_emptiness_check(const const_twa_ptr& left, const const_twa_ptr& right,
                          const state *left_init, const state *right_init);

  /// @}
}
