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


    class relabel_iter: public tgba_reachable_iterator_depth_first
    {
    public:
      relabel_iter(const spot::tgba* a, const spot::scc_map& sm)
	: tgba_reachable_iterator_depth_first(a),
	  out_(new  spot::tgba_explicit_number(a->get_dict())),
	  sm_(sm)
      {
	// Preconditions to apply algo
	assert(a->number_of_acceptance_conditions() == 1);
	assert (a->all_acceptance_conditions() != bddtrue);

	// Register variable
	int v = out_->get_dict()
	  ->register_acceptance_variable
	  (ltl::constant::strong_scc_instance(), out_);
	strong_acc = bdd_ithvar(v);
	v = out_->get_dict()
	  ->register_acceptance_variable
	  (ltl::constant::weak_scc_instance(), out_);
	weak_acc = bdd_ithvar(v);
	v = out_->get_dict()
	  ->register_acceptance_variable
	  (ltl::constant::terminal_scc_instance(), out_);
	terminal_acc = bdd_ithvar(v);

	// out_->set_acceptance_conditions
	//   (a->all_acceptance_conditions() |
	//    terminal_acc | weak_acc | strong_acc);

      }

      spot::tgba*
      result()
      {
	std::cout << out_->number_of_acceptance_conditions() << std::endl;
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
	    out_->add_acceptance_conditions (t, strong_acc);
	  }
	else if (scc_in == scc_out && sm_.terminal_accepting(scc_out))
	  {
	    out_->add_acceptance_conditions (t, terminal_acc);
	    out_->add_acceptance_conditions (t, all_cond);
	  }
	else if (scc_in == scc_out && sm_.weak_accepting(scc_out))
	  {
	    out_->add_acceptance_conditions (t, weak_acc);
	    out_->add_acceptance_conditions (t, all_cond);
	  }
      }

    private:
      spot::tgba_explicit_number* out_;
      const scc_map& sm_;
      bdd strong_acc;
      bdd weak_acc;
      bdd terminal_acc;
      bdd  all_cond;
    };


// class check_iter: public tgba_reachable_iterator_depth_first
//     {
//     public:
//       check_iter(const spot::tgba* a, const spot::scc_map& sm)
// 	: tgba_reachable_iterator_depth_first(a),
// 	  out_(new  spot::tgba_explicit_number(a->get_dict())),
// 	  sm_(sm)
//       {
// 	bdd_dict::fv_map::iterator i =  out_->get_dict()
// 	  ->acc_map.find(ltl::constant::strong_scc_instance());
// 	strong_acc = bdd_ithvar(i->second);
// 	bdd_dict::fv_map::iterator j =  out_->get_dict()
// 	  ->acc_map.find(ltl::constant::weak_scc_instance());
// 	weak_acc = bdd_ithvar(j->second);
// 	bdd_dict::fv_map::iterator k =  out_->get_dict()
// 	  ->acc_map.find(ltl::constant::terminal_scc_instance());
// 	terminal_acc = bdd_ithvar(k->second);
// 	assert (a->all_acceptance_conditions() != bddtrue);
// 	assert(a->number_of_acceptance_conditions() == 4);
//       }

//       spot::tgba*
//       result()
//       {
// 	return out_;
//       }

//       bool
//       want_state(const state*) const
//       {
// 	// we want to walk accross all the product
// 	// so we allways want a state
// 	return true;
//       }

//       void
//       process_link(const state* , int in,
// 		   const state* , int out,
// 		   const tgba_succ_iterator* si)
//       {

// 	if ((si->current_acceptance_conditions() & strong_acc) ==  strong_acc)
// 	  {
// 	    if ((si->current_acceptance_conditions() - strong_acc) != bddfalse)
// 	      std::cout << in << "--"<< out << "   Strong Acc \n";
// 	    else
// 	      std::cout << in << "--"<< out << "   Strong\n";
// 	  }
// 	if ((si->current_acceptance_conditions() & weak_acc) == weak_acc)
// 	  std::cout << in << "--"<< out << "   Weak\n";
// 	if ((si->current_acceptance_conditions() & terminal_acc) == terminal_acc)
// 	  std::cout << in << "--"<< out << "   terminal\n";
//       }

//     private:
//       spot::tgba_explicit_number* out_;
//       const scc_map& sm_;
//       bdd strong_acc;
//       bdd weak_acc;
//       bdd terminal_acc;
// };

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


      // void
      // process_link(const state* , int in,
      // 		   const state* , int out,
      // 		   const tgba_succ_iterator* si)
      // {
      // 	if ((si->current_acceptance_conditions() & the_new_acc) != bddfalse)
      // 	  {
      // 	    if ((si->current_acceptance_conditions() - the_new_acc) != bddfalse)
      // 	      std::cout << in << "--"<< out << "   Strong accepting\n";
      // 	    else
      // 	      std::cout << in << "--"<< out << "   Strong\n";
      // 	  }
      // 	else
      // 	  std::cout << in << "--"<< out << "Not Strong\n";
      // }
