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

#include "dve2succiter.hh"

#include <cstring>

namespace spot
{
  ////////////////////////////////////////////////////////////////////////
  // DVE2 ITERATOR

  dve2_succ_iterator::dve2_succ_iterator(const dve2_callback_context* cc,
					 bdd cond)
    : kripke_succ_iterator(cond)
      , cc_(cc)
  {
  }

  dve2_succ_iterator::~dve2_succ_iterator()
  {
    delete cc_;
  }

  void
  dve2_succ_iterator::first()
  {
    it_ = cc_->transitions.begin();
  }

  void
  dve2_succ_iterator::next()
  {
    ++it_;
  }

  bool
  dve2_succ_iterator::done() const
  {
    return it_ == cc_->transitions.end();
  }

  state*
  dve2_succ_iterator::current_state() const
  {
    return (*it_)->clone();
  }

  ////////////////////////////////////////////////////////////////////////
  // ONE STATE ITERATOR

  one_state_iterator::one_state_iterator(const dve2_state* state, bdd cond)
    : kripke_succ_iterator(cond)
      , state_(state)
      , done_(false)
  {
  }

  void
  one_state_iterator::first()
  {
    done_ = false;
  }

  void
  one_state_iterator::next()
  {
    done_ = true;
  }

  bool
  one_state_iterator::done() const
  {
    return done_;
  }

  state*
  one_state_iterator::current_state() const
  {
    return state_->clone();
  }

  ////////////////////////////////////////////////////////////////////////
  // AMPLE SET ITERATOR

  std::list<dve2_state*>
  ample_iterator::my_copy(const Ti& in)
  {
    std::list<dve2_state*> res;

    fixed_size_pool* pool = const_cast<fixed_size_pool*>(&(k_->statepool_));

    for (Ti::const_iterator it = in.begin();
	 it != in.end(); ++it)
      {
	dve2_state* nstate =
	  new(pool->allocate()) dve2_state(k_->state_size_, 0);
	memcpy(nstate->vars, it->dst, k_->state_size_ * sizeof(int));
	res.push_back(nstate);
      }

    return res;
  }

  bool
  ample_iterator::check_c1(unsigned p, const T& procT)
  {
    for (int i = 0; i < k_->d_->get_state_variable_count(); ++i)
      {
	if (i == k_->processes_[p])
	  continue;

	for (Ti::const_iterator it = procT[p].begin();
	     it != procT[p].end(); ++it)
	  {
	    const int* dep =
	      k_->d_->get_transition_read_dependencies(it->id);
	    if (dep[i])
	      return false;
	  }
      }

    return true;
  }

  bool
  ample_iterator::check_c2(const int* s, std::list<por_callback::trans>& t)
  {
    for (Ti::const_iterator it = t.begin();
	 it != t.end(); ++it)
      {
	if (!k_->invisible(s, it->dst))
	  return false;
      }

    return true;
  }

  bool
  ample_iterator::check_c3(Ti& t)
  {
    for (Ti::const_iterator it = t.begin();
	 it != t.end(); ++it)
      if (k_->visited_.find(k_->hash_state(it->dst)) != k_->visited_.end())
	return false;

    return true;
  }

  ample_iterator::ample_iterator(const int* state,
				 bdd cond,
				 const por_callback& pc,
				 const dve2_kripke* k)
    : kripke_succ_iterator(cond)
      , k_(k)
  {
    T procT(k->processes_.size(), Ti());

    for (Ti::const_iterator it = pc.tr.begin();
	 it != pc.tr.end(); ++it)
      {
	for (unsigned p = 0; p < k->processes_.size(); ++p)
	  if (state[k->processes_[p]] != it->dst[k->processes_[p]])
	    procT[p].push_back(*it);
      }

    for (unsigned p = 0;
	 p < k->processes_.size() && !procT[p].empty(); ++p)
      {
	bool c1 = check_c1(p, procT);
	bool c2 = check_c2(state, procT[p]);
	bool c3 = check_c3(procT[p]);

	if (c1 && c2 && c3)
	  {
	    next_ = my_copy(procT[p]);
	    break;
	  }
      }

    if (next_.empty())
	next_ = my_copy(pc.tr);
  }

  void
  ample_iterator::first()
  {
    cur_ = next_.begin();
  }

  void
  ample_iterator::next()
  {
    if (cur_ != next_.end())
      ++cur_;
  }

  bool
  ample_iterator::done() const
  {
    return cur_ == next_.end();
  }

  state*
  ample_iterator::current_state() const
  {
    return (*cur_)->clone();
  }
}
