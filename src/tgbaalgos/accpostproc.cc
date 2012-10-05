// Copyright (C) 2012 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
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


#include "accpostproc.hh"
#include "tgbaalgos/reachiter.hh"
#include "tgba/tgbaexplicit.hh"
#include "tgbaalgos/dotty.hh"
#include "ltlast/constant.hh"
#include "tgbaalgos/minimize.hh"
#include "tgbaalgos/simulation.hh"
#include "tgbaalgos/save.hh"
#include "tgbaalgos/scc.hh"


namespace spot
{

  namespace
  {
    // Tool function used to facilitate the creation of the automaton
    // for being Generic
    static
    state_explicit_number::transition*
    create_transition(const tgba*, tgba_explicit_number* out_aut,
		      const state*, int in, const state*, int out)
    {
      return out_aut->create_transition(in, out);
    }


    // Template class that walk the automaton to create another
    // one repecting the strategy
    class relabel_iter: public tgba_reachable_iterator_depth_first
    {
    public:
      relabel_iter(const spot::tgba* a, const spot::scc_map& sm)
	: tgba_reachable_iterator_depth_first(a),
	  out_(new  spot::tgba_explicit_number(a->get_dict())),
	  sm_(sm)
      {
	int v = out_->get_dict()
	  ->register_acceptance_variable
	  (ltl::constant::strong_scc_instance(), out_);
	the_new_acc = bdd_ithvar(v);
      }

      spot::tgba*
      result()
      {
	return out_;
      }

      bool
      want_state(const state*) const
      {
	// we want to walk accross all the product
	// so we allways want a state
	return true;
      }

      void
      process_link(const state* in_s, int in,
		   const state* out_s, int out,
		   const tgba_succ_iterator* si)
      {
	state_explicit_number::transition* t =
	  create_transition(this->aut_, out_, in_s, in, out_s, out);
	out_->add_conditions(t, si->current_condition());
	out_->add_acceptance_conditions
	  (t, si->current_acceptance_conditions());

	// In the case of a strong / strong transition
	// we just have to add one more acceptance
	// it's the StrongScc acceptance label
	int scc_in = sm_.scc_of_state (in_s);
	int scc_out = sm_.scc_of_state (out_s);
	if (scc_in == scc_out && sm_.strong(scc_out))
	  {
	    out_->add_acceptance_conditions (t, the_new_acc);
	  }
      }

    private:
      spot::tgba_explicit_number* out_;
      const scc_map& sm_;
      bdd the_new_acc;
    };
  }

  const tgba* add__fake_acceptance_condition (const tgba* a,
					      spot::scc_map* sm)
  {
    spot::scc_map* x = sm;

    // Create the map if needed
    if (!sm)
      {
	x = new spot::scc_map(a);
	x->build_map();
      }

    relabel_iter ri  (a, *x);
    ri.run();

    if (!sm)
      delete x;

    return ri.result();;
  }


}
