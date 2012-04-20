// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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

#include "propagation.hh"
#include "tgba/tgbaexplicit.hh"
#include "misc/hash.hh"
#include "reachiter.hh"
#include "dupexp.hh"

namespace spot
{
  namespace
  {
    typedef Sgi::hash_map<const state*, bdd,
			  state_ptr_hash, state_ptr_equal> state_map;

    // Recursively traverse the automaton, updating two maps: accin,
    // and accout.  Accout[s] contain the acceptance conditions common
    // to all outgoing arcs of s, it is always safe to use this value.
    // Accin[s] is used to compute the acceptance conditions common to
    // all ingoing arcs of s: it may contain intermediate results and
    // should not be used before the end of the recursion.

    // return true iff a change has been made to accout.
    bool
    propagation_dfs(const tgba* a, const state* s,
		    state_map& accout, state_map& accin)
    {
      state_map::iterator out =
	accout.insert(std::make_pair(s, bddfalse)).first;

      assert(accin.find(s) != accin.end());

      // Gather acceptance conditions common to all successors.
      bdd agregacc = bddtrue;
      tgba_succ_iterator* i = a->succ_iter(s);
      for (i->first(); !i->done(); i->next())
	if (i->current_state() != s) // Ignore self-loops.
	  agregacc &= i->current_acceptance_conditions();

      bool changed = false;
      if (agregacc != bddtrue)
	{
	  bdd old = out->second;
	  out->second |= agregacc;
	  changed = old != out->second;
	}

      agregacc = bddtrue;
      for (i->first(); !i->done(); i->next())
	{
	  state* to = i->current_state();

	  if (to == s)		// Ignore self-loops.
	    continue;

	  bdd acc = i->current_acceptance_conditions() | out->second;
	  state_map::iterator into = accin.find(to);
	  if (into == accin.end())
	    {
	      accin[to] = acc;
	      changed |= propagation_dfs(a, to, accout, accin);
	    }
	  else
	    {
	      into->second &= acc;
	    }
	  agregacc &= accout[to];
	}
      delete i;

      if (agregacc != bddtrue)
	{
	  bdd old = out->second;
	  out->second |= agregacc;
	  changed |= old != out->second;
	}

      return changed;
    }

    template <typename State>
    class rewrite_inplace: public tgba_reachable_iterator_depth_first
    {
    public:
      rewrite_inplace(tgba_explicit<State>* a,
		      const state_map& accout, const state_map& accin)
	: tgba_reachable_iterator_depth_first(a),
	  accout_(accout), accin_(accin), ea_(a)
      {
      }

      void
      process_link(const state* in_s, int, const state* out_s, int,
		   const tgba_succ_iterator* si)
      {
	const tgba_explicit_succ_iterator<State>* it =
	  down_cast<const tgba_explicit_succ_iterator<State>*>(si);

	bdd& acc = ea_->get_transition(it)->acceptance_conditions;
	bdd toadd = accout_.find(out_s)->second | accin_.find(in_s)->second;

	if (in_s == out_s)
	  {
	    // If we have a non-accepting self-loop.  As an extra
	    // simplification, remove any acceptance condition that is
	    // common to all outgoing transitions of this state, and
	    // all those that are common to all ingoing transitions.
	    bdd allacc = ea_->all_acceptance_conditions();
	    if (acc != allacc)
	      acc &= allacc - toadd;
	  }
	else			// Regular transitions.
	  {
	    acc |= toadd;
	  }
      }

    protected:
      const state_map& accout_;
      const state_map& accin_;
      tgba_explicit<State>* ea_;
    };

    template <typename State>
    void
    do_propagate_acceptance_conditions_inplace(tgba_explicit<State>* a)
    {
      state_map accout;
      state_map accin;
      state* init_state = a->get_init_state();
      bool changed;
      do
	{
	  accin.clear();
	  accin[init_state] = bddtrue;
	  changed = propagation_dfs(a, init_state, accout, accin);

	  // Now that the values in accin are correctly computed,
	  // propagate these accout so we act is of these acceptance
	  // conditions where on the outgoing transition on the next
	  // iteration.
	  for (state_map::iterator in = accin.begin(); in != accin.end(); ++in)
	    {
	      bdd acc = in->second;
	      if (acc != bddtrue && acc != bddfalse)
		{
		  state_map::iterator out = accout.find(in->first);
		  assert(out != accout.end());
		  bdd old = out->second;
		  out->second |= acc;
		  changed |= old != out->second;
		}
	    }
	}
      while (changed);

      // If no transition go to init_state, mark that it has no
      // incoming acceptance condition.
      state_map::iterator it = accin.find(init_state);
      if (it->second == bddtrue)
	it->second = bddfalse;

      rewrite_inplace<State> ri(a, accout, accin);
      ri.run();
    }
  }

  tgba*
  propagate_acceptance_conditions(const tgba* a)
  {
    tgba_explicit_number* res = tgba_dupexp_dfs(a);
    do_propagate_acceptance_conditions_inplace(res);
    return res;
  }

  tgba*
  propagate_acceptance_conditions_inplace(tgba* a)
  {
    if (spot::tgba_explicit_string* e =
	dynamic_cast<spot::tgba_explicit_string*>(a))
      do_propagate_acceptance_conditions_inplace(e);
    else if (spot::tgba_explicit_number* e =
	     dynamic_cast<spot::tgba_explicit_number*>(a))
      do_propagate_acceptance_conditions_inplace(e);
    else if (spot::tgba_explicit_formula* e =
	     dynamic_cast<spot::tgba_explicit_formula*>(a))
      do_propagate_acceptance_conditions_inplace(e);
    else
      return propagate_acceptance_conditions(a);
    return a;
  }

}
