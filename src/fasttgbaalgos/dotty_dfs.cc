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
#include "dotty_dfs.hh"
#include "fasttgba/fasttgbaexplicit.hh"

namespace spot
{
  dotty_dfs::dotty_dfs(const fasttgba* a):
    generic_dfs(a)
  { }

  dotty_dfs::~dotty_dfs()
  { }

  bool
  dotty_dfs::want_state(const fasttgba_state*) const
  {
    return true;
  }

  void
  dotty_dfs::start()
  {
    std::cout << ("digraph G {\n"
		  "  0 [label=\"\", style=invis, height=0]\n"
		  "  0 -> 1\n");
  }

  void
  dotty_dfs::end()
  {
    std::cout << "}" << std::endl;
  }

  void
  dotty_dfs::process_state(const fasttgba_state* s, int in,
			   fasttgba_succ_iterator*)
  {
    std::cout << "  " << in
	      << " [label = \""
	      << a_->format_state(s)
	      << "\"]"
	      << std::endl;
  }

  void
  dotty_dfs::process_link(const fasttgba_state* , int in,
			  const fasttgba_state* , int out,
			  const fasttgba_succ_iterator* t)
  {
    std::cout << "  " << in << " -> " << out
	      << " [label = \""
	      << a_->transition_annotation(t)
	      << "\"]"
	      << std::endl;
  }
}
