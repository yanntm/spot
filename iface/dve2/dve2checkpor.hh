// Copyright (C) 2012 Laboratoire de Recherche et Developpement
// de l'Epita (LRDE)
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

#ifndef SPOT_IFACE_DVE2_DVE2CHECKPOR_HH
# define SPOT_IFACE_DVE2_DVE2CHECKPOR_HH

#include "tgbaalgos/reachiter.hh"
#include "dve2state.hh"

namespace spot
{
  /// \addtogroup por_check
  /// @{

  /// \brief Check the state space for erroneous states possibly
  /// created by a partial order reduction algorithm
  ///
  /// Partial order reduction algorithms are hard to check because the
  /// reduced state space depends both on the model, the formula and
  /// the algorithm used. The more obvious way to check a reduced
  /// state space is to make sure that it gives the same result for
  /// the formula. This algorithm is a little bit more advanced as it
  /// will search the reduced state space for any state that do not
  /// exist in the full state space.
  ///
  /// To do it, it just creates the product automaton without any
  /// reduction, explores and stores all its states into a hash
  /// map. In a second time, the reduced product automaton is explored
  /// and each state is checked to see if it appears on the hash map.
  ///
  /// \param prod: the reduced product automaton.
  /// \param full_prod: the full product automaton.
  /// \return nothing but update the status of the public variable ok.
  class dve2_checkpor: public tgba_reachable_iterator_depth_first
  {
  protected:
    typedef Sgi::hash_set<const dve2_state*, state_ptr_hash,
			  state_ptr_equal> state_set;

    class keep_states: public tgba_reachable_iterator_depth_first
    {
    public:
      keep_states(const tgba* a);

      virtual void process_state(const state* s, int, tgba_succ_iterator*);

      state_set visited;
    };

  public:
    dve2_checkpor(const tgba* prod, const tgba* full_prod);
    virtual void process_state(const state* s, int, tgba_succ_iterator*);

    bool ok;

  protected:
    keep_states ks_;
  };

  /// @}
} // End namespace spot.


#endif // SPOT_IFACE_DVE2_CHECKPOR_HH
