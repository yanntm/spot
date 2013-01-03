// -*- coding: utf-8 -*-
// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE)
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

#include "dve2twophase.hh"

#include <cstring>

namespace spot
{
  dve2_twophase::dve2_twophase(const dve2_kripke* k)
  : k_(k)
  {
  }

  bool
  dve2_twophase::internal(const trans& t, int p) const
  {
    const int* dep = k_->d_->get_transition_read_dependencies(t.id);

    for (std::vector<int>::const_iterator it = k_->processes_.begin ();
    	 it != k_->processes_.end (); ++it)
    {
      if (dep[*it] && *it != k_->processes_[p])
    	return false;
    }

    for (std::vector<int>::const_iterator it = k_->global_vars_.begin ();
	 it != k_->global_vars_.end (); ++it)
      {
	if (dep[*it])
	  return false;
      }

    // for (int i = 0; i < k_->d_->get_state_variable_count(); ++i)
    //   {
    // 	if (dep[i] && i != k_->processes_.at(p))
    // 	  return false;
    //   }

    return true;
  }

  bool
  dve2_twophase::only_one_enabled(const int* s, int p,
				  const std::list<trans>& tr, trans& t,
				  const std::set<unsigned>& visited) const
  {
    int cpt = 0;

    for (std::list<trans>::const_iterator it = tr.begin();
	 it != tr.end(); ++it)
      {
	if (s[k_->processes_[p]] != it->dst[k_->processes_[p]] &&
	    visited.find(k_->hash_state(it->dst)) == visited.end())
	  {
	    ++cpt;
	    t = *it;
	  }

	if (cpt > 1)
	  return false;
      }

    return cpt == 1;
  }

  bool
  dve2_twophase::deterministic(const int* s, unsigned p,
			       const std::list<trans>& tr, trans& res,
			       const std::set<unsigned>& visited,
			       bdd form_vars) const
  {
    if (only_one_enabled(s, p, tr, res, visited))
      {
	if (k_->invisible(s, res.dst, form_vars) && internal(res, p))
	  return true;
      }

    return false;
  }

  const dve2_state*
  dve2_twophase::phase1(const int* in, bdd form_vars) const
  {
    const int* s = in;
    std::set<unsigned> visited;

    visited.insert(k_->hash_state(in));

    por_callback pc(k_->state_size_);

    std::list<trans> tr;
    // k_->d_->get_successors(0, const_cast<int*>(s),
    // 			   fill_trans_callback, &pc);

    trans t(-1, 0);
    for (unsigned p = 0; p < k_->processes_.size(); ++p)
      {
	if (!pc.tr.empty ())
	  pc.clear ();
	assert(pc.tr.empty ());
	k_->d_->get_successors(0, const_cast<int*>(s),
			       fill_trans_callback, &pc);

	while (deterministic(s, p, pc.tr, t, visited, form_vars))
	  {
	    if (s != in)
	      delete[] s;

	    s = new int[k_->state_size_ * sizeof(int)];
	    memcpy(const_cast<int*>(s),
		   t.dst, k_->state_size_ * sizeof(int));

	    if (visited.find(k_->hash_state(s)) != visited.end())
	      break;

	    pc.clear ();

	    visited.insert(k_->hash_state(s));
	    assert (pc.tr.empty ());
	    k_->d_->get_successors(0, const_cast<int*>(s),
				   fill_trans_callback, &pc);
	  }
      }

    if (s == in)
      return 0;

    fixed_size_pool* p = const_cast<fixed_size_pool*>(&(k_->statepool_));
    dve2_state* res = new(p->allocate()) dve2_state(k_->state_size_, 0, false);
    memcpy(res->vars, s, k_->state_size_ * sizeof(int));

    return res;
  }
}
