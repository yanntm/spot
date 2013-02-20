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
#include "generic_dfs.hh"

namespace spot
{
  generic_dfs::generic_dfs(const fasttgba* a):
    a_(a)
  { }

  generic_dfs::~generic_dfs()
  {
    seen_map::const_iterator s = seen.begin();
    while (s != seen.end())
      {
	s->first->destroy();
	++s;
      }
  }

  void
  generic_dfs::run()
  {
    int n = 0;
    start();
    fasttgba_state* i = a_->get_init_state();
    if (want_state(i))
      add_state(i);
    seen[i] = ++n;
    const fasttgba_state* t;
    while ((t = next_state()))
      {
	assert(t);
	assert(seen.find(t) != seen.end());
	int tn = seen[t];
	fasttgba_succ_iterator* si = a_->succ_iter(t);
	process_state(t, tn, si);
	for (si->first(); !si->done(); si->next())
	  {
	    const fasttgba_state* current = si->current_state();
	    seen_map::const_iterator s = seen.find(current);
	    bool ws = want_state(current);
	    if (s == seen.end())
	      {
		seen[current] = ++n;
		if (ws)
		  {
		    add_state(current);
		    process_link(t, tn, current, n, si);
		  }
	      }
	    else
	      {
		if (ws)
		  {
		    process_link(t, tn, s->first, s->second, si);
		  }
		current->destroy();
	      }
	  }
	delete si;
      }
    end();
  }

  // bool
  // generic_dfs::want_state(const fasttgba_state*) const
  // {
  //   return true;
  // }

  void
  generic_dfs::add_state(const fasttgba_state* s)
  {
    todo.push(s);
  }

  const fasttgba_state*
  generic_dfs::next_state()
  {
    if (todo.empty())
      return 0;
    const fasttgba_state* s = todo.top();
    todo.pop();
    return s;
  }

}
