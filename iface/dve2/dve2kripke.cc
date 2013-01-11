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

#include "dve2kripke.hh"

#include "dve2callback.hh"
#include "dve2state.hh"
#include "dve2succiter.hh"
#include "dve2twophase.hh"

#include "tgba/tgbaproduct.hh"

#include "misc/intvcomp.hh"
#include "misc/intvcmp2.hh"
#include "misc/casts.hh"

#include <cstring>
#include <sstream>

namespace spot
{
  dve2_kripke::dve2_kripke(const dve2_interface* d, bdd_dict* dict,
			   const dve2_prop_set* ps, const ltl::formula* dead,
			   int compress, por::type por)
  : d_(d)
    , state_size_(d_->get_state_variable_count())
    , dict_(dict)
    , ps_(ps)
    , compress_(compress == 0 ? 0
		: compress == 1 ? int_array_array_compress
		: int_array_array_compress2)
    , decompress_(compress == 0 ? 0
		  : compress == 1 ? int_array_array_decompress
		  : int_array_array_decompress2)
    , uncompressed_(compress ? new int[state_size_ + 30] : 0)
    , compressed_(compress ? new int[state_size_ * 2] : 0)
    , statepool_(compress ? sizeof(dve2_compressed_state) :
		 (sizeof(dve2_state) + state_size_ * sizeof(int)))
    , state_condition_last_state_(0), state_condition_last_cc_(0)
    , por_ (por)
    , cur_process_(0)
  {
    vname_ = new const char*[state_size_];
    format_filter_ = new bool[state_size_];
    for (int i = 0; i < state_size_; ++i)
      {
	vname_[i] = d_->get_state_variable_name(i);
	// We don't want to print variables that can take a single
	// value (e.g. process with a single state) to shorten the
	// output.
	int type = d->get_state_variable_type(i);
	format_filter_[i] =
	  (d->get_state_variable_type_value_count(type) != 1);
      }

    // Register the "dead" proposition.  There are three cases to
    // consider:
    //  * If DEAD is "false", it means we are not interested in finite
    //    sequences of the system.
    //  * If DEAD is "true", we want to check finite sequences as well
    //    as infinite sequences, but do not need to distinguish them.
    //  * If DEAD is any other string, this is the name a property
    //    that should be true when looping on a dead state, and false
    //    otherwise.
    // We handle these three cases by setting ALIVE_PROP and DEAD_PROP
    // appropriately.  ALIVE_PROP is the bdd that should be ANDed
    // to all transitions leaving a live state, while DEAD_PROP should
    // be ANDed to all transitions leaving a dead state.
    if (dead == ltl::constant::false_instance())
      {
	alive_prop = bddtrue;
	dead_prop = bddfalse;
      }
    else if (dead == ltl::constant::true_instance())
      {
	alive_prop = bddtrue;
	dead_prop = bddtrue;
      }
    else
      {
	int var = dict->register_proposition(dead, d_);
	dead_prop = bdd_ithvar(var);
	alive_prop = bdd_nithvar(var);
      }


    //Find the different processes for the partial order reduction
    for (int i = 0; i < d->get_state_variable_count(); ++i)
      {
	//ugly, find a better way to find the number of processes.
	// std::string tmp(d->get_state_variable_type_name(i));
	// if (tmp != "byte" && tmp != "integer")
	//   processes_.push_back(i);

	//better?
	if (d->get_state_variable_type_value(d->get_state_variable_type(i), 0))
	  processes_.push_back(i);
	else
	{
	  std::string tmp = d->get_state_variable_name(i);
	  if (tmp.find (".") == std::string::npos)
	    global_vars_.push_back (i);
	}

      }
  }

  dve2_kripke::~dve2_kripke()
  {
    delete[] format_filter_;
    delete[] vname_;
    if (compress_)
      {
	delete[] uncompressed_;
	delete[] compressed_;
      }
    lt_dlclose(d_->handle);

    dict_->unregister_all_my_variables(d_);

    delete d_;
    delete ps_;
    lt_dlexit();

    if (state_condition_last_state_)
      state_condition_last_state_->destroy();
    delete state_condition_last_cc_; // Might be 0 already.
  }

  state*
  dve2_kripke::get_init_state() const
  {
    if (compress_)
      {
	d_->get_initial_state(uncompressed_);
	size_t csize = state_size_ * 2;
	compress_(uncompressed_, state_size_, compressed_, csize);

	multiple_size_pool* p =
	  const_cast<multiple_size_pool*>(&compstatepool_);
	void* mem = p->allocate(sizeof(dve2_compressed_state)
				+ sizeof(int) * csize);
	dve2_compressed_state* res = new(mem)
	  dve2_compressed_state(csize, p);
	memcpy(res->vars, compressed_, csize * sizeof(int));
	res->compute_hash();
	return res;
      }
    else
      {
	fixed_size_pool* p = const_cast<fixed_size_pool*>(&statepool_);
	dve2_state* res = new(p->allocate()) dve2_state(state_size_, p);
	d_->get_initial_state(res->vars);
	res->compute_hash();
	return res;
      }
  }

  bdd
  dve2_kripke::compute_state_condition_aux(const int* vars) const
  {
    bdd res = bddtrue;
    for (dve2_prop_set::const_iterator i = ps_->begin();
	 i != ps_->end(); ++i)
      {
	int l = vars[i->var_num];
	int r = i->val;

	bool cond = false;
	switch (i->op)
	  {
	  case OP_EQ:
	    cond = (l == r);
	    break;
	  case OP_NE:
	    cond = (l != r);
	    break;
	  case OP_LT:
	    cond = (l < r);
	    break;
	  case OP_GT:
	    cond = (l > r);
	    break;
	  case OP_LE:
	    cond = (l <= r);
	    break;
	  case OP_GE:
	    cond = (l >= r);
	    break;
	  }

	if (cond)
	  res &= bdd_ithvar(i->bddvar);
	else
	  res &= bdd_nithvar(i->bddvar);
      }
    return res;
  }

  dve2_callback_context*
  dve2_kripke::build_cc(const int* vars, int& t) const
  {
    dve2_callback_context* cc = new dve2_callback_context;
    cc->state_size = state_size_;
    cc->pool =
      const_cast<void*>(compress_
			? static_cast<const void*>(&compstatepool_)
			: static_cast<const void*>(&statepool_));
    cc->compress = compress_;
    cc->compressed = compressed_;
    t = d_->get_successors(0, const_cast<int*>(vars),
			   compress_
			   ? transition_callback_compress
			   : transition_callback,
			   cc);
    assert((unsigned)t == cc->transitions.size());
    return cc;
  }

  bdd
  dve2_kripke::compute_state_condition(const state* st) const
  {
    // If we just computed it, don't do it twice.
    if (st == state_condition_last_state_)
      return state_condition_last_cond_;

    if (state_condition_last_state_)
      {
	state_condition_last_state_->destroy();
	delete state_condition_last_cc_; // Might be 0 already.
	state_condition_last_cc_ = 0;
      }

    const int* vars = get_vars(st);

    bdd res = compute_state_condition_aux(vars);
    int t;
    dve2_callback_context* cc = build_cc(vars, t);

    if (t)
      {
	res &= alive_prop;
      }
    else
      {
	res &= dead_prop;

	// Add a self-loop to dead-states if we care about these.
	if (res != bddfalse)
	  cc->transitions.push_back(st->clone());
      }

    state_condition_last_cc_ = cc;
    state_condition_last_cond_ = res;
    state_condition_last_state_ = st->clone();

    return res;
  }

  const int*
  dve2_kripke::get_vars(const state* st) const
  {
    const int* vars;
    if (compress_)
      {
	const dve2_compressed_state* s =
	  down_cast<const dve2_compressed_state*>(st);
	assert(s);

	decompress_(s->vars, s->size, uncompressed_, state_size_);
	vars = uncompressed_;
      }
    else
      {
	const dve2_state* s = down_cast<const dve2_state*>(st);
	assert(s);
	vars = s->vars;
      }
    return vars;
  }

  kripke_succ_iterator*
  dve2_kripke::succ_iter(const state* local_state,
			 const state* prod_state,
			 const tgba* prod_tgba) const
  {
    const state_product* sprod =
      down_cast<const state_product*> (prod_state);
    const tgba_product* tprod =
      down_cast<const tgba_product*> (prod_tgba);

    if (por_ == por::TWOP || por_ == por::TWOPD)
      {
	const dve2_state* uncompstate = 0;
	const dve2_compressed_state* compstate = 0;

	uncompstate = static_cast<const dve2_state*> (local_state);
	if (!uncompstate)
	{
	  compstate = static_cast<const dve2_compressed_state*> (local_state);
	  assert (compstate);
	}

	if ((uncompstate && uncompstate->expanded) ||
	    (compstate && compstate->expanded))
	  {
	    bdd form_vars = bddfalse;
	    // assert (po);
	    // if (po)
	    //   form_vars = po->cur_ap;

	    if (por_ == por::TWOPD)
	      {
		if (sprod && tprod)
		  form_vars = tprod->right()->support_variables(sprod->right());
	      }

	    dve2_twophase tp(this);

	    const int* vstate = get_vars(local_state);
	    assert(vstate);
	    bdd scond = compute_state_condition_aux(vstate);
	    const dve2_state* s = tp.phase1(vstate, form_vars);

	    if (s)
	      return new one_state_iterator(s, scond);
	  }
      }
    // This may also compute successors in state_condition_last_cc
    bdd scond = compute_state_condition(local_state);

    dve2_callback_context* cc;
    if (state_condition_last_cc_)
      {
	cc = state_condition_last_cc_;
	state_condition_last_cc_ = 0; // Now owned by the iterator.
      }
    else
      {
	int t;
	cc = build_cc(get_vars(local_state), t);

	// Add a self-loop to dead-states if we care about these.
	if (t == 0 && scond != bddfalse)
	  cc->transitions.push_back(local_state->clone());
      }

    return new dve2_succ_iterator(cc, scond);
  }

  bdd
  dve2_kripke::state_condition(const state* st) const
  {
    return compute_state_condition(st);
  }

  std::string
  dve2_kripke::format_state(const state *st) const
  {
    const int* vars = get_vars(st);

    std::stringstream res;

    if (state_size_ == 0)
      return "empty state";

    int i = 0;
    for (;;)
      {
	if (!format_filter_[i])
	  {
	    ++i;
	    continue;
	  }
	res << vname_[i] << "=" << vars[i];
	++i;
	if (i == state_size_)
	  break;
	res << ", ";
      }
    return res.str();
  }

  spot::bdd_dict*
  dve2_kripke::get_dict() const
  {
    return dict_;
  }

  bool
  dve2_kripke::invisible(const int* start, const int* to,
			 bdd form_vars) const
  {
    assert(start && to);

    if (form_vars == bddtrue)
      return true;
    else if (form_vars != bddfalse)
      {
	while (form_vars != bddtrue)
	{
	  for (dve2_prop_set::const_iterator it = ps_->begin ();
	       it != ps_->end (); ++it)
	    if (start[it->var_num] != to[it->var_num] &&
		it->bddvar == bdd_var(form_vars))
	      return false;
	  form_vars = bdd_high(form_vars);
	}
      }
    else
      {
	for (dve2_prop_set::const_iterator it = ps_->begin();
	     it != ps_->end(); ++it)
	  if (start[it->var_num] != to[it->var_num])
	    return false;
      }
    return true;
  }
}
