// -*- coding: utf-8 -*-
// Copyright (C) 2012 Laboratoire de Recherche et Développement
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
    const int* rdep = k_->d_->get_transition_read_dependencies(t.id);
    const int* wdep = k_->d_->get_transition_write_dependencies(t.id);

    for (std::vector<int>::const_iterator it = k_->processes_.begin ();
    	 it != k_->processes_.end (); ++it)
    {
      if ((wdep[*it] || rdep[*it]) && *it != k_->processes_[p])
    	return false;
    }

    for (std::vector<int>::const_iterator it = k_->global_vars_.begin ();
	 it != k_->global_vars_.end (); ++it)
      {
	if (wdep[*it] || rdep[*it])
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
				  const state_set& visited) const
  {
    int cpt = 0;
    fixed_size_pool* pool = const_cast<fixed_size_pool*>(&(k_->statepool_));

    for (std::list<trans>::const_iterator it = tr.begin();
	 it != tr.end(); ++it)
      {
	if (s[k_->processes_[p]] != it->dst[k_->processes_[p]])
	  {
	    dve2_state* tmp =
	      new(pool->allocate()) dve2_state(k_->state_size_, pool, it->dst);

	    if (visited.find(tmp) == visited.end())
	      {
		++cpt;
		t = *it;
	      }

	    tmp->destroy();

	    if (cpt > 1)
	      return false;
	  }
      }

    return cpt == 1;
  }

  bool
  dve2_twophase::deterministic(const int* s, unsigned p,
			       const std::list<trans>& tr, trans& res,
			       const state_set& visited,
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
    state_set visited;

    fixed_size_pool* pool = const_cast<fixed_size_pool*>(&(k_->statepool_));

    dve2_state* ins = new(pool->allocate()) dve2_state(k_->state_size_, pool,
						    const_cast<int*> (in));
    visited.insert(ins);

    dve2_state* ss = ins;

    por_callback pc(k_->state_size_);

    trans t(-1, 0);
    for (unsigned p = 0; p < k_->processes_.size(); ++p)
      {
	if (!pc.tr.empty ())
	  pc.clear ();
	assert(pc.tr.empty ());

	k_->d_->get_successors(0, const_cast<int*>(ss->vars),
			       fill_trans_callback, &pc);

	while (deterministic(ss->vars, p, pc.tr, t, visited, form_vars))
	  {
	    ss = new(pool->allocate()) dve2_state(k_->state_size_, pool,
						  t.dst, false);

	    state_set::const_iterator it = visited.find(ss);
	    if (it != visited.end())
	    {
	      ss->destroy ();
	      ss = *it;
	      break;
	    }

	    pc.clear ();

	    visited.insert(ss);
	    assert (pc.tr.empty ());
	    k_->d_->get_successors(0, const_cast<int*>(ss->vars),
				   fill_trans_callback, &pc);
	  }
	pc.clear ();
      }

    dve2_state* res = 0;

    if (!(ss == ins))
      res = ss->clone ();

    for (state_set::iterator it = visited.begin ();
	it != visited.end ();)
      {
	dve2_state* tmp = *it;
	++it;
	tmp->destroy ();
      }
    visited.clear ();

    return res;
  }
}
