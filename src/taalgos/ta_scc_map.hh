// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement
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

#ifndef SPOT_TAALGOS_TA_SCC_MAP_HH
# define SPOT_TAALGOS_TA_SCC_MAP_HH

#include "ta/ta.hh"
#include <iosfwd>

namespace spot
{

  /// \addtogroup ta_misc
  /// @{

  /// \brief This class computes the SCC of the TA automaton
  class SPOT_API ta_scc_map
  {
  public:
    /// \brief Simple constructor
    ta_scc_map(const ta* t, bool remove_trivial_livelock_buchi_acc = true);

    /// \brief Simple Destructor
    virtual ~ta_scc_map();

    /// \brief Number of transitions in the TA
    unsigned transitions();

    /// \brief Number of states in the TA
    unsigned states();

    /// \brief Number of scc in the automaton
    int sccs();

    /// \brief display infos about SCCs
    void dump_infos();

  private:
    /// \brief Construct the SCC map of the TA automaton using a
    /// Tarjan-like algorithm
    void build_map();

    /// \brief When a trivial state is livelock and Buchi we can
    /// reduce it to livelock only.
    void scc_prune_livelock_buchi();

    /// \brief Set what kind of SCCs can be reached
    void scc_reach();

    /// \brief The TA considered
    const ta* t_;

    /// \brief Count the number of transitions
    unsigned transitions_;

    /// \brief Count the number of states
    unsigned states_;

    /// \brief Count the number of sccs
    int sccs_;

    /// \brief The informations associated to an SCC
    struct scc_info
    {
      bool contains_livelock_acc_state;
      bool contains_buchi_acc_state;
      bool can_reach_livelock_acc_state;
      bool can_reach_buchi_acc_state;
      bool is_trivial;
      std::vector<int> *scc_reachable;
    };

    /// \brief The informations of each SCCs
    std::vector<scc_info> infos;

    // -------------------------------------------------------
    // This part contains variables used to perform the scc
    // computation
    // -------------------------------------------------------

    /// \brief Element of the DFS stack
    struct dfs_element
    {
      const state* st;
      ta_succ_iterator* si;
    };

    /// \brief The DFS stack
    std::vector<dfs_element> todo;

    /// \brief Uniq identifier for each state during DFS
    int id_;

    /// \brief Call when a new state is discovered
    void dfs_push(const spot::state*);

    /// \brief Call when a state has been fully explored
    void dfs_pop();

    /// \brief Call when a closing edge is detected
    void dfs_update(const spot::state*, const spot::state*);

    /// \brief The lowlink stack
    std::vector<int> lowlink_stack;

    /// \brief The live stack (could integrate Nuutila's optimisation)
    std::vector<const state*> live_stack;

    /// \brief The map that will contain all states.
    typedef Sgi::hash_map<const state*, int, state_ptr_hash, state_ptr_equal>
        seen_map;
    seen_map seen;
  };

  /// \brief Compute statistics for an automaton.
  SPOT_API void ta_build_scc_map(const ta* t);

  /// @}
}

#endif // SPOT_TAALGOS_TA_SCC_MAP_HH
