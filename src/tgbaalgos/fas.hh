// -*- coding: utf-8 -*-
// Copyright (C) 2014, 2015 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
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

#ifndef SPOT_TGBAALGOS_FAS_HH
# define SPOT_TGBAALGOS_FAS_HH

# include "tgba/tgba.hh"
# include "tgbaalgos/sccinfo.hh"
# include <utility>
# include "graph/bidigraph.hh"
# include <vector>

namespace spot
{
  class SPOT_API fas
  {
  public:
    /// \brief Find a feed back arc-set from a TGBA.
    ///
    /// For the FAS problem we chose to use the GR algorithm. To use this
    /// algorithm we need a new representation of our automaton with it's in and
    /// out degree.
    ///
    /// \param g The automata to search the FAS.
    fas(const const_tgba_digraph_ptr& g);
    ~fas();

    /// Check if the transition from \a src to \a dst is part of the fas.
    bool operator()(unsigned int src, unsigned int dst);

    /// Compute the fas, call this before using operator ()
    void build(void);

    /// Displays how the states have been ordered.  On each line, the left
    /// number represents the state's id and the right number represents the
    /// position in the vector of ordered states.  Each transition who's source
    /// position is smaller than the destination position is part of the FAS.
    void print(std::ostream& os);

  private:
    const spot::const_tgba_digraph_ptr aut_;
    // Map between spot_states and their FAS position
    std::unordered_map<unsigned int, unsigned> ordered_states;
    // SCC of aut_ allowing FAS to be applied on each one of these individually
    spot::scc_info scc_;

    // Compute fas on a sub-part of the graph, one of its SCCs
    void worker_build(spot::graph::bidigraph& bdg, unsigned& order);
  };
}

#endif // SPOT_TGBAALGOS_FAS_HH
