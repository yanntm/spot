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
    typedef Sgi::hash_set<const state*,
			  state_ptr_hash, state_ptr_equal> state_set;
    typedef Sgi::hash_map<const state*, bdd,
			  state_ptr_hash, state_ptr_equal> state_map;

    // Principle: acc contains a map state -> BDD
    // On the descendent part, the common acceptance condition(s) going
    // out of the state  is computed and acc is updated.
    // One the ascendant part,

    // return true iff a change has been made to acc
    bool
    propagation_dfs(const tgba* a, const state* s,
		    state_set& seen, state_map& acc)
    {
      seen.insert(s);

      state_map::iterator it = acc.find(s);
      if (it == acc.end())
	it = acc.insert(std::make_pair(s, bddfalse)).first;

      // Gather acceptance conditions common to all successors.
      bdd agregacc = bddtrue;
      tgba_succ_iterator* i = a->succ_iter(s);
      for (i->first(); !i->done(); i->next())
	agregacc &= i->current_acceptance_conditions();

      bool changed = false;
      if (agregacc != bddtrue)
	{
	  bdd old = it->second;
	  it->second |= agregacc;
	  changed = old != it->second;
	}

      agregacc = bddtrue;
      for (i->first(); !i->done(); i->next())
	{
	  bool inmap = false;
	  state* to = i->current_state();

	  if (seen.find(to) == seen.end())
	    {
	      changed |= propagation_dfs(a, to, seen, acc);
	      inmap = true;
	    }

	  agregacc &= acc[to];

	  if (!inmap)
	    to->destroy();
	}
      delete i;

      if (agregacc != bddtrue)
	{
	  bdd old = it->second;
	  it->second |= agregacc;
	  changed |= old != it->second;
	}

      return changed;
    }

    template <typename State>
    class rewrite_inplace: public tgba_reachable_iterator_depth_first
    {
    public:
      rewrite_inplace(tgba_explicit<State>* a,
		      state_map& acc)
	: tgba_reachable_iterator_depth_first(a),
	  acc_(acc),
	  ea_(a)
      {
      }

      void
      process_link(const state*, int,
		   const state* out_s, int,
		   const tgba_succ_iterator* si)
      {
	const tgba_explicit_succ_iterator<State>* tmpit =
	  down_cast<const tgba_explicit_succ_iterator<State>*>(si);

	typename tgba_explicit<State>::transition* t =
	  ea_->get_transition(tmpit);

	t->acceptance_conditions |= acc_[out_s];
      }

    protected:
      state_map& acc_;
      tgba_explicit<State>* ea_;
    };
  }


  template <typename State>
  void
  do_propagate_acceptance_conditions_inplace(tgba_explicit<State>* a)
  {
    state_set seen;
    state_map acc;

    state* init_state = a->get_init_state();

    while (propagation_dfs(a, init_state, seen, acc))
      {
      }

    rewrite_inplace<State> ri(a, acc);
    ri.run();
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
