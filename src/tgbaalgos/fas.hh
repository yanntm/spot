// -*- coding: utf-8 -*-
// Copyright (C) 2012, 2013, 2014 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
// Copyright (C) 2003, 2004  Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
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

# include "tgba/state.hh"
# include "tgba/tgba.hh"
# include <utility>
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
    fas(const tgba* g);
    ~fas();

    /// Check if the transition from \a src to \dst is part of the fas.
    bool operator()(const spot::state* src, const spot::state* dst);

  private:
    std::unordered_map<const spot::state*, unsigned, const spot::state_ptr_hash,
                       spot::state_ptr_equal> ordered_states;
  };
}

#endif // SPOT_TGBAALGOS_FAS_HH
