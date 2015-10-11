// -*- coding: utf-8 -*-
// Copyright (C)  2015, 2017 Laboratoire de Recherche et
// Developpement de l'Epita (LRDE)
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

#include <spot/twaalgos/reachiter.hh>
#include <spot/twaalgos/stats.hh>
#include <spot/ltsmin/proviso.hh>

namespace spot
{
  // \brief DFS Exploration easily extendable for POR purpose.
  class SPOT_API stats_dfs: public twa_reachable_iterator_depth_first
  {
  public:
    stats_dfs(const const_twa_ptr& a, twa_statistics& s,
	      proviso& proviso, bool dotty = false)
      : twa_reachable_iterator_depth_first(a), s_(s),
      proviso_(proviso), dotty_(dotty)
    {
      last_ = -1;
      expanded_ = 0;
      already_expanded_ = 0;
    }

    void
    process_state(const state* s, int n, twa_succ_iterator* it)
    {
      ++s_.states;
      last_ = n;
      if (it->all_enabled())
	++already_expanded_;

      if (proviso_.expand_new_state(n, it->all_enabled(), it, seen, s, aut_))
	{
	  it->fire_all();
	  ++expanded_;
	}
    }

    bool
    will_pop_state(const state*, int n, twa_succ_iterator* it)
    {
      assert(it->done());
      if (proviso_.expand_before_pop(n, it, seen))
	{
	  it->fire_all();
	  it->next();
	  ++expanded_;
	  assert(!it->done());
	  return false;
	}

      return true;
    }

    void
    process_link(const state*, int src, const state*, int dst,
		 const twa_succ_iterator* it)
    {
      if (dotty_)
	std::cout << ' ' << src << " -> " << dst << std::endl;
      ++s_.edges;

      // Since all edges are processed we do not consider edges
      // leading to new states (not used for proviso)
      if (dst > last_)
	return;

      if (proviso_.expand_src_closingedge(src, dst))
	{
	  it->fire_all();
	  ++expanded_;
	}
    }

    void start()
    {
      if (dotty_)
	std::cout << "digraph G {\n";
    }

    void end()
    {
      if (dotty_)
	std::cout << '}' << std::endl;
    }


    void dump()
    {
      std::cout << "expanded: " << expanded_ << std::endl;
      std::cout << "already_expanded: " << already_expanded_ << std::endl;
    }
  private:
    int last_;
    int expanded_;
    int already_expanded_;
    twa_statistics& s_;
    proviso& proviso_;
    bool dotty_;
  };
}
