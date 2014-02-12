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

#ifndef SPOT_FASTTGBAALGOS_GENERIC_DFS_HH
# define SPOT_FASTTGBAALGOS_GENERIC_DFS_HH

#include <stack>
#include "misc/hash.hh"
#include "fasttgba/fasttgba.hh"

namespace spot
{
  class SPOT_API generic_dfs
  {
  public:
    generic_dfs(const fasttgba* a);

    virtual ~generic_dfs();

    void run();

    virtual void add_state(const fasttgba_state* s);

    virtual const fasttgba_state* next_state();

    virtual bool want_state(const fasttgba_state* s) const = 0;

    virtual void start() = 0;

    virtual void end() = 0;

    virtual void  process_state(const fasttgba_state* s, int n,
				fasttgba_succ_iterator* si) = 0;

    virtual void  process_link(const fasttgba_state* in_s, int in,
			       const fasttgba_state* out_s, int out,
			       const fasttgba_succ_iterator* si) = 0;

  protected:
    const fasttgba* a_;

    std::stack<const fasttgba_state*>
    todo; ///< A stack of states yet to explore.

    typedef Sgi::hash_map<const fasttgba_state*, int,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> seen_map;

    seen_map seen;
  };
}

#endif // SPOT_FASTTGBAALGOS_GENERIC_DFS_HH
