// Copyright (C) 2004, 2005  Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
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



#include "tgbaalgos/emptiness_specifier.hh"
#include "tgba/tgbatba.hh"
#include "ltlast/constant.hh"

//#define SPECTRACE
#include <iostream>
#ifdef SPECTRACE
#define sptrace std::cerr
#else
#define sptrace while (0) std::cerr
#endif

namespace spot
{

  formula_emptiness_specifier::formula_emptiness_specifier (const tgba * a)
    : sys_(a) , f_(a), s_(a)
  {
    const tgba_explicit_formula* bf =
      dynamic_cast<const tgba_explicit_formula*> (a);
    assert(bf);
    both_formula = false;
    sm = new scc_map(f_);
    sm->build_map();
  }

  formula_emptiness_specifier::formula_emptiness_specifier (const tgba * a,
							    const tgba * f)
    : sys_(a) , f_(f), s_(a)
  {
    const tgba_explicit_formula* bf =
      dynamic_cast<const tgba_explicit_formula*> (f);
    assert(bf);
    both_formula = false;
    sm = new scc_map(f_);
    sm->build_map();
  }

  /// Create the specifier taking the automaton of the system and 
  /// the automaton of the formula 
  formula_emptiness_specifier::formula_emptiness_specifier (const tgba * a,
							    const tgba * f,
							    const tgba *s)
    : sys_(a) , f_(f), s_(s)
  {
    assert(f && a && s);
    const tgba_explicit_formula* bs =
      dynamic_cast<const tgba_explicit_formula*> (s_);
    const tgba_explicit_formula* bf =
      dynamic_cast<const tgba_explicit_formula*> (f_);
    assert(bf || bs);
    both_formula = (bf && bs);

    if (bf == 0 && !both_formula)
      {
	const tgba* tmp = f_;
	f_ = s_;
	s_ = tmp;
      }
    sm = new scc_map(f_);
    sm->build_map();
  }

  formula_emptiness_specifier::~formula_emptiness_specifier()
  {
    delete sm;
  }

  const ltl::formula*
  formula_emptiness_specifier::formula_from_state (const state *s) const
  {
    const ltl::formula* f = 0;
    if (!both_formula)
      {
	state * sproj = sys_->project_state(s, f_);
	assert(sproj);

	const state_explicit* fstate =
	  dynamic_cast<const state_explicit*> (sproj);
	assert(fstate);
	f  = dynamic_cast<const ltl::formula*>
	  (dynamic_cast<const tgba_explicit_formula*> (f_)->get_label(sproj));
	sproj->destroy();
      }
    else
      // Both are formula we construct the result by a LOGICAL AND between 
      // the both formula that are given
      {
	// Compute first formula 
	state * sproj1 = sys_->project_state(s, f_);
	assert(sproj1);
	const state_explicit* fstate1 =
	  dynamic_cast<const state_explicit*> (sproj1);
	const ltl::formula* f1  = dynamic_cast<const ltl::formula*>
	  (dynamic_cast<const tgba_explicit_formula*> (f_)->get_label(fstate1));
	sproj1->destroy();

	// Compute second formula 
	state * sproj2 = sys_->project_state(s, s_);
	assert(sproj1);
	const state_explicit* fstate2 =
	  dynamic_cast<const state_explicit*> (sproj2);
	const ltl::formula* f2  = dynamic_cast<const ltl::formula*>
	  (dynamic_cast<const tgba_explicit_formula*> (s_)->get_label(fstate2));
	sproj1->destroy();

	// Construct the resulting formula
	f = dynamic_cast <const ltl::formula*>
	  (ltl::multop::instance(ltl::multop::And,
				 const_cast <ltl::formula*>(f1),
				 const_cast <ltl::formula*>(f2)));
      }
    sptrace << "\t > recurrence  ?" << f->is_syntactic_recurrence()
	    << std::endl
	    << "\t > persistence ?" << f->is_syntactic_persistence()
	    << std::endl
	    << "\t > obligation  ?" << f->is_syntactic_obligation()
	    << std::endl
	    << "\t > safety      ?" << f->is_syntactic_safety()
	    << std::endl
	    << "\t > guarantee   ?" << f->is_syntactic_guarantee()
	    << std::endl;

    return f;
  }

  bool
  formula_emptiness_specifier::is_part_of_weak_acc (const state *s) const
  {
    bool res = false;
    //scc_map* sm = new scc_map(f_);
    //sm->build_map();

    if (!both_formula)
      {
	state * sproj = sys_->project_state(s, f_);
	assert(sproj);

	const state_explicit* fstate =
	  dynamic_cast<const state_explicit*> (sproj);

	unsigned id_scc = sm->scc_of_state(fstate);
	res = sm->weak_accepting(id_scc);

	sproj->destroy();
      }
    else
      assert (false);

    //delete sm;
    return res;
  }

  bool
  formula_emptiness_specifier::same_weak_acc (const state *s1,
					      const state *s2) const
  {
    bool res = false;
//     scc_map* sm = new scc_map(f_);
//     sm->build_map();

    if (!both_formula)
      {
	state * sproj1 = sys_->project_state(s1, f_);
	assert(sproj1);
	const state_explicit* fstate1 =
	  dynamic_cast<const state_explicit*> (sproj1);
	unsigned id_scc1 = sm->scc_of_state(fstate1);
	state * sproj2 = sys_->project_state(s2, f_);
	assert(sproj2);
	const state_explicit* fstate2 =
	  dynamic_cast<const state_explicit*> (sproj2);
	unsigned id_scc2 = sm->scc_of_state(fstate2);

	sproj1->destroy();
	sproj2->destroy();
	res = (sm->weak_accepting(id_scc1)) && (id_scc1 == id_scc2);
      }
    //    delete sm;
    return res;
  }

  std::list<const state *>*
  formula_emptiness_specifier::collect_self_loop_acc_terminal_nodes()
  {
    //
    std::list<const state*> *result = 
      new std::list<const state*> ();

    // Visited states
    std::set <spot::state *>*
      visited_tmp = new std::set< spot::state *>();
    
    // todo 
    std::stack
      <spot::state_explicit*> todo_tmp;

    // Initial state of the automaton
    spot::state_explicit *i_src =
      (spot::state_explicit *) f_->get_init_state();
    visited_tmp->insert(i_src->clone());
    todo_tmp.push (i_src->clone());

      // Perform work for the initial state 
      {
	const ltl::formula* f = 0;
	f = formula_from_state(i_src->clone());
	if (f == ltl::constant::true_instance())
	  {
	    // std::cout << f_->format_state(i_src);
	    result->push_back(i_src->clone());
	  }
      }

      while (!todo_tmp.empty())	// We have always an initial state 
	{
	  // Init all vars 
	  spot::state_explicit *s_src;
	  s_src = todo_tmp.top();
	  todo_tmp.pop();

	  // Iterator over the successor of the src
	  spot::tgba_explicit_succ_iterator *si =
	    (tgba_explicit_succ_iterator*) f_->succ_iter (s_src);
	  for (si->first(); !si->done(); si->next())
	    {
	      // Get successor of the src
	      spot::state_explicit * succ_src = si->current_state();

	      // It's a new state we have to visit it
	      if (visited_tmp->find (succ_src) == visited_tmp->end())
		{
		  // Mark as visited 
		  visited_tmp->insert(succ_src);

		  todo_tmp.push (succ_src);

		  {
		    const ltl::formula* f = 0;
		    f = formula_from_state(succ_src);
		    if (f == ltl::constant::true_instance())
		      {
			//std::cout << f_->format_state(succ_src);
			result->push_back(succ_src->clone());
		      }
		  }
		}

	      // Destroy theses states that are now unused
	      succ_src->destroy();
	    }
	  delete si;

	  // CLEAN
	  s_src->destroy();
	}

      // No more in use
      i_src->destroy();

      std::set<spot::state *>::iterator it;
      for (it=visited_tmp->begin(); it != visited_tmp->end(); ++it)
	(*it)->destroy();
      delete visited_tmp;




      return result;
//     return new std::list<const state *>();
  } 
}
