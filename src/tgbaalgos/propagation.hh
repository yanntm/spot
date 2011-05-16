// Copyright (C) 2009, 2011 Laboratoire de Recherche et Developpement
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

#ifndef SPOT_TGBAALGOS_PROPAGATION_HH
# define SPOT_TGBAALGOS_PROPAGATION_HH

#include "tgba/tgbaexplicit.hh"
#include <sstream>

namespace spot
{
  /// Apply the acceptance condition's propagation to the TGBA
  /// \ingroup tgba_algorithms
  ///
  /// When we want to degeneralize an automaton we can gain states
  /// on some optimization.
  ///
  /// Here we use the fact that if all out transitions of a state
  /// have the same acceptance_condition we can propagate it to
  /// all in transitions of the state.
  ///
  /// Doing this iteratively untill there is no more propagation to
  /// do.
  class Propagation
  {
  public:
    typedef std::set<size_t> state_set;
    typedef std::map<size_t, bdd> acc_map;
    typedef std::map<size_t, int> exp_map;

    /// \brief Constructor.
    ///
    /// This will not propagate the acceptance conditions initially.
    /// You must call Propagation () to do so.
    Propagation (const spot::tgba* a);

    ~Propagation ();

    /// Actually compute the propagation of acceptance conditions
    /// Output a tgba_explicit_string.
    spot::tgba* propagate ();

  protected:
    const spot::tgba* a_;
    acc_map* acc_;
    exp_map* rec_;

    //labeling the new automaton
    int count_;
    bool again_;

    std::string tostr (int i)
    {
      std::stringstream s;
      s << i;
      return s.str ();
    }

    void dfs (spot::state* s, state_set* seen);
    void propagate_run (spot::tgba_explicit_string* a,
			spot::state* s,
			state_set* seen);
  };
}

#endif /// SPOT_TGBAALGOS_PROPAGATION_HH
