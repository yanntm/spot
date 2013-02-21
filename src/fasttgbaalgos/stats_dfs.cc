// Copyright (C) 2012 Laboratoire de Recherche et Développement
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

#include <iostream>
#include <sstream>
#include "stats_dfs.hh"

namespace spot
{
  stats_dfs::stats_dfs(const fasttgba* a):
    generic_dfs(a), nb_states(0), nb_transitions(0)
  { }


  bool
  stats_dfs::want_state(const fasttgba_state*) const
  {
    return true;
  }

  void
  stats_dfs::start()
  {
  }

  void
  stats_dfs::end()
  {
    // std::cout << "   * " << nb_states << " states\n";
    // std::cout << "   * " << nb_transitions << " trans.\n";
  }

  void
  stats_dfs::process_state(const fasttgba_state*, int,
			   fasttgba_succ_iterator*)
  {
    ++nb_states;
  }

  void
  stats_dfs::process_link(const fasttgba_state* , int,
			  const fasttgba_state* , int,
			  const fasttgba_succ_iterator*)
  {
    ++nb_transitions;
  }

  std::string stats_dfs::dump()
  {
    std::ostringstream oss;
    oss << nb_states <<  "," << nb_transitions;
    return oss.str();
  }
}
