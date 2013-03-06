// Copyright (C) 2009, 2010 Laboratoire de Recherche et Developpement
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

#include "scc_decompose.hh"
#include "tgba/tgbaunion.hh"
#include "tgbaalgos/reachiter.hh"
#include "tgba/tgbaexplicit.hh"
#include "tgbaalgos/dotty.hh"
#include "ltlast/constant.hh"
#include "tgbaalgos/minimize.hh"
#include "tgbaalgos/simulation.hh"
#include "tgbaalgos/save.hh"
#include "tgbaalgos/postproc.hh"

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

    static
    state_explicit_string::transition*
    create_transition(const tgba* aut, tgba_explicit_string* out_aut,
		      const state* in_s, int, const state* out_s, int)
    {
      const tgba_explicit_string* a =
	static_cast<const tgba_explicit_string*>(aut);
      return out_aut->create_transition(a->get_label(in_s),
					a->get_label(out_s));
    }

    static
    tgba_explicit_formula::transition*
    create_transition(const tgba* aut, tgba_explicit_formula* out_aut,
		      const state* in_s, int, const state* out_s, int)
    {
      const tgba_explicit_formula* a =
	static_cast<const tgba_explicit_formula*>(aut);
      const ltl::formula* in_f = a->get_label(in_s);
      const ltl::formula* out_f = a->get_label(out_s);
      if (!out_aut->has_state(in_f))
	in_f->clone();
      if ((in_f != out_f) && !out_aut->has_state(out_f))
	out_f->clone();
      return out_aut->create_transition(in_f, out_f);
    }

    // Tool function used to facilitate the creation of the automaton
    // for being Generic
    static
    state_explicit_number::transition*
    create_transition(const tgba*, sba_explicit_number* out_aut,
		      const state*, int in, const state*, int out)
    {
      return out_aut->create_transition(in, out);
    }

    static
    state_explicit_string::transition*
    create_transition(const tgba* aut, sba_explicit_string* out_aut,
		      const state* in_s, int, const state* out_s, int)
    {
      const sba_explicit_string* a =
	static_cast<const sba_explicit_string*>(aut);
      return out_aut->create_transition(a->get_label(in_s),
					a->get_label(out_s));
    }

    static
    sba_explicit_formula::transition*
    create_transition(const tgba* aut, sba_explicit_formula* out_aut,
		      const state* in_s, int, const state* out_s, int)
    {
      const sba_explicit_formula* a =
	static_cast<const sba_explicit_formula*>(aut);
      const ltl::formula* in_f = a->get_label(in_s);
      const ltl::formula* out_f = a->get_label(out_s);
      if (!out_aut->has_state(in_f))
	in_f->clone();
      if ((in_f != out_f) && !out_aut->has_state(out_f))
	out_f->clone();
      return out_aut->create_transition(in_f, out_f);
    }




    /// This type is used to detect if the decomposition must
    /// focus on making a strong weak or terminal automaton
    enum decomptype {STRONG, WEAK, TERMINAL};

    // Template class that walk the automaton to create another
    // one repecting the strategy
    template<class T>
    class decomp_iter: public tgba_reachable_iterator_depth_first
    {
    public:
      typedef T output_t;

      decomp_iter(const tgba* a,
		  const scc_map& sm,
		  decomptype dc)
	: tgba_reachable_iterator_depth_first(a),
	  out_(new T(a->get_dict())),
	  sm_(sm),
	  dc_(dc)
      {
	if (dc == STRONG)
	  out_->set_acceptance_conditions(aut_->all_acceptance_conditions());
	else
	  {
	    int v = out_->get_dict()
	      ->register_acceptance_variable
	      (ltl::constant::true_instance(), out_);
	    the_acc = bdd_ithvar(v);
	    out_->set_acceptance_conditions(the_acc);
	  }
	all = out_->all_acceptance_conditions();
      }

      T*
      result()
      {
	return out_;
      }

      bool
      want_state(const state* s) const
      {
	if (dc_ == STRONG)
	  return !sm_.weak_subautomaton(sm_.scc_of_state(s));
	if (dc_ == WEAK)
	  return !sm_.terminal_subautomaton(sm_.scc_of_state(s)) &&
	    !sm_.strong_hard(sm_.scc_of_state(s));
	return !sm_.strong_hard(sm_.scc_of_state(s)) &&
	  !sm_.weak_hard(sm_.scc_of_state(s));
      }

      void
      process_link(const state* in_s, int in,
		   const state* out_s, int out,
		   const tgba_succ_iterator* si)
      {
	typename output_t::state::transition* t =
	  create_transition(this->aut_, out_, in_s, in, out_s, out);
	out_->add_conditions(t, si->current_condition());

	switch (dc_)
	  {
	    // In this case all acc conditions have to be the same
	    // but only strong scc are examined
	  case STRONG:
	    out_->add_acceptance_conditions
	      (t, si->current_acceptance_conditions());
	    break;

	    // In this case all acceptance conditions are
	    // replaced by a single one (Acc[True])
	    // In strong (not hard) SCC all acceptance conditions
	    // are removed and terminal SCC are ignored
	  case WEAK:
	    {
	      if (sm_.scc_of_state(in_s) != sm_.scc_of_state(out_s))
		out_->add_acceptance_conditions (t, bddfalse);
	      else
		if (sm_.scc_of_state(in_s) == sm_.scc_of_state(out_s) &&
		    sm_.weak_accepting(sm_.scc_of_state(in_s)))
		  out_->add_acceptance_conditions(t, the_acc);
		else
		  out_->add_acceptance_conditions
		    (t, bddfalse);
	      break;
	    }

	    // It's the terminal case, all the automaton (except
	    // strong hard and weak hard) scc are considered and
	    // acceptance condition is added Acc[True].
	    // All ohter acceptance conditions are removed from
	    // strong and weak SCC
	  default:
	      if (sm_.scc_of_state(in_s) != sm_.scc_of_state(out_s))
		out_->add_acceptance_conditions (t, bddfalse);
	      else
		if (sm_.scc_of_state(in_s) == sm_.scc_of_state(out_s) &&
		    sm_.terminal_accepting(sm_.scc_of_state(in_s)))
		  out_->add_acceptance_conditions(t, the_acc);
		else
		  out_->add_acceptance_conditions
		    (t, bddfalse);
	      break;
	  }
      }

    private:
      T* out_;
      const scc_map& sm_;
      decomptype dc_;
      bdd the_acc;
      bdd all;
    };
  }

  void
  scc_decompose::decompose_strong ()
  {
    // There is no such SCC
    if (!sm->has_strong_scc())
      {
	strong_ = 0;
	return;
      }

    if (dynamic_cast<const tgba_explicit_formula*>(src_))
      {
	decomp_iter<tgba_explicit_formula>
	  di  (src_, *sm, STRONG);
	di.run();
	strong_ = di.result();
      }
    else if (dynamic_cast<const tgba_explicit_number*>(src_))
      {
	decomp_iter<tgba_explicit_number>
	  di  (src_, *sm, STRONG);
	di.run();
	strong_ = di.result();
      }
    else if (dynamic_cast<const tgba_explicit_string*>(src_))
      {
	decomp_iter<tgba_explicit_string>
	  di  (src_, *sm, STRONG);
	di.run();
	strong_ = di.result();
      }
    else if (dynamic_cast<const sba_explicit_formula*>(src_))
      {
	decomp_iter<sba_explicit_formula>
	  di  (src_, *sm, STRONG);
	di.run();
	strong_ = di.result();
      }
    else if (dynamic_cast<const sba_explicit_number*>(src_))
      {
	decomp_iter<sba_explicit_number>
	  di  (src_, *sm, STRONG);
	di.run();
	strong_ = di.result();
      }
    else if (dynamic_cast<const sba_explicit_string*>(src_))
      {
	decomp_iter<sba_explicit_string>
	  di  (src_, *sm, STRONG);
	di.run();

	strong_ = di.result();
      }
    else
      {
	strong_ = 0;
	assert(false);
      }
    if (!strong_ || strong_->number_of_acceptance_conditions() == 0)
      {
    	delete strong_;
    	strong_ = 0;
    	return;
      }

    // Minimise
    if (minimize)
      {
    	spot::postprocessor pp;
    	strong_ =  pp.run(strong_, 0);
      }
    else
      {
	// Remove useless
	tgba *tmp  = spot::scc_filter(strong_, true);
	delete strong_;
	strong_ = tmp;
      }
  }

  void
  scc_decompose::decompose_weak ()
  {
    // There is no such SCC
    if (!sm->has_weak_scc())
      {
	weak_ = 0;
	return;
      }

    // Oterwise compute it
    if (dynamic_cast<const tgba_explicit_formula*>(src_))
      {
	decomp_iter<tgba_explicit_formula>
	  di  (src_, *sm, WEAK);
	di.run();
	weak_ = di.result();
      }
    else if (dynamic_cast<const tgba_explicit_number*>(src_))
      {
	decomp_iter<tgba_explicit_number>
	  di  (src_, *sm, WEAK);
	di.run();
	weak_ = di.result();
      }
    else if (dynamic_cast<const tgba_explicit_string*>(src_))
      {
	decomp_iter<tgba_explicit_string>
	  di  (src_, *sm, WEAK);
	di.run();
	weak_ = di.result();
      }
    else if (dynamic_cast<const sba_explicit_formula*>(src_))
      {
	decomp_iter<sba_explicit_formula>
	  di  (src_, *sm, WEAK);
	di.run();
	weak_ = di.result();
      }
    else if (dynamic_cast<const sba_explicit_number*>(src_))
      {
	decomp_iter<sba_explicit_number>
	  di  (src_, *sm, WEAK);
	di.run();
	weak_ = di.result();
      }
    else if (dynamic_cast<const sba_explicit_string*>(src_))
      {
	decomp_iter<sba_explicit_string>
	  di  (src_, *sm, WEAK);
	di.run();
	weak_ = di.result();
      }
    else
      {
	weak_ = 0;
	assert(false);
      }
    if (!weak_ || weak_->number_of_acceptance_conditions() == 0)
      {
    	delete weak_;
    	weak_ = 0;
    	return;
      }

    // Minimise
    if (minimize)
      {
    	spot::postprocessor pp;
    	weak_ = pp.run(weak_, 0);
      }
    else
      {
	// Remove useless
	tgba *tmp  = spot::scc_filter(weak_, true);
	delete weak_;
	weak_ = tmp;
      }
  }

  void
  scc_decompose::decompose_terminal ()
  {
    // There is no such SCC
    if (!sm->has_terminal_scc())
      {
	terminal_ = 0;
	return;
      }

    // Otherwise compute it
    if (dynamic_cast<const tgba_explicit_formula*>(src_))
      {
	decomp_iter<tgba_explicit_formula>
	  di  (src_, *sm, TERMINAL);
	di.run();
	terminal_ = di.result();
      }
    else if (dynamic_cast<const tgba_explicit_number*>(src_))
      {
	decomp_iter<tgba_explicit_number>
	  di  (src_, *sm, TERMINAL);
	di.run();
	terminal_ = di.result();
      }
    else if (dynamic_cast<const tgba_explicit_string*>(src_))
      {
	decomp_iter<tgba_explicit_string>
	  di  (src_, *sm, TERMINAL);
	di.run();
	terminal_ = di.result();
      }
    else if (dynamic_cast<const sba_explicit_formula*>(src_))
      {
	decomp_iter<sba_explicit_formula>
	  di  (src_, *sm, TERMINAL);
	di.run();
	terminal_ = di.result();
      }
    else if (dynamic_cast<const sba_explicit_number*>(src_))
      {
	decomp_iter<sba_explicit_number>
	  di  (src_, *sm, TERMINAL);
	di.run();
	terminal_ = di.result();
      }
    else if (dynamic_cast<const sba_explicit_string*>(src_))
      {
	decomp_iter<sba_explicit_string>
	  di  (src_, *sm, TERMINAL);
	di.run();
	terminal_ = di.result();
      }

    else
      {
	terminal_ = 0;
	assert(false);
      }

    if (!terminal_ || terminal_->number_of_acceptance_conditions() == 0)
      {
    	delete terminal_;
    	terminal_ = 0;
    	return;
      }

    // Minimise
    if (minimize)
      {
    	spot::postprocessor pp;
    	terminal_ =  pp.run(terminal_, 0);
      }
    else
      {
	//Remove useless
	tgba *tmp  = spot::scc_filter(terminal_, true);
	delete terminal_;
	terminal_ = tmp;
      }
  }

  const tgba*
  scc_decompose::terminal_automaton ()
  {
    // if (terminal_->number_of_acceptance_conditions() == 0)
    //   return 0;
    return terminal_;
  }

  const tgba*
  scc_decompose::weak_automaton ()
  {
    return weak_;
  }

  const tgba*
  scc_decompose::strong_automaton ()
  {
    return strong_;
  }

  void
  scc_decompose::decompose()
  {
    return;
  }

  tgba*
  scc_decompose::recompose()
  {
    // NOT Yet Supported
    // terminal_automaton();
    // strong_automaton();
    // std::cout << "I recompose" << std::endl;
    // tgba* aa = new tgba_union (strong_,0);
    // spot::dotty_reachable(std::cout, aa);
    return 0;
  }

}
