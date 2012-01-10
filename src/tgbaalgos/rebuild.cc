// Copyright (C) 2009, 2010, 2011 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
// Copyright (C) 2003, 2004 Laboratoire d'Informatique de Paris 6 (LIP6),
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

#include "rebuild.hh"
#include "emptiness_specifier.hh"
#include "tgba/tgbaexplicit.hh"
#include "ltlast/formula.hh"
#include "ltlast/constant.hh"

//#define REBUILD_TRACE
#ifdef REBUILD_TRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

namespace spot
{

  static bdd all_cond;

  tgba*
  rebuild::reorder_transitions ()
  {
    all_cond = src->all_acceptance_conditions();

    // Get the dictionnary 
    spot::bdd_dict *dict = src->get_dict();

    // Convert once for all TGBA and initialize
    const spot::tgba_explicit_formula *f =
      dynamic_cast<const spot::tgba_explicit_formula*> (src);
    spot::tgba_explicit_formula *t =
      new spot::tgba_explicit_formula (dict);

    dict->register_all_variables_of(f, t);

    // Create the new initial state for the new TGBA
    spot::state_explicit *i_src = (spot::state_explicit *)f->get_init_state();

    // This presuppose that all first state is consider as init 
    // should be documented
    spot::state_explicit *i_dst =
		t->add_state(f->get_label(i_src)->clone());
      //(spot::state_explicit *) t->get_init_state();
    todo.push(std::make_pair (i_src, i_dst));

    trace << "# init  : " << t->format_state(i_dst) << std::endl;
    
    // States that have been or will be visited
    visited = new std::set< spot::state *>();
    visited->insert(i_dst->clone());

    // No more in use
    i_src->destroy();
    i_dst->destroy();

    std::pair <spot::state_explicit *, spot::state_explicit *> apair;
     while (!todo.empty())	// We have always an initial state 
      {
	// Get the pair to work on
	apair = todo.top();
	todo.pop();

	// Init all vars 
	spot::state_explicit *s_src, *s_dst;
	s_src = apair.first;
	s_dst = apair.second;

	// DEBUG
	trace << "WORK  src : " << s_src << std::endl;
	trace << "WORK  dst : " << s_dst << std::endl;
		
	// List of transitions that should be create
 	std::list< rebuild::sort_trans > *alist =
 	  new std::list< rebuild::sort_trans >();

 	// Iterator over the successor of the src
	spot::tgba_explicit_succ_iterator *si =
	  (tgba_explicit_succ_iterator*) f->succ_iter (s_src);
	for (si->first(); !si->done() ; si->next())
	  {
	    // Get successor of the src and dst 
 	    spot::state_explicit * succ_src = si->current_state();
 	    spot::state_explicit * succ_dst ;
	    
	    if (! t->has_state(f->get_label(succ_src)))
	      succ_dst = 
		t->add_state(f->get_label(succ_src)->clone());
	    else 
	      succ_dst = 
		t->add_state(f->get_label(succ_src));

	    // It's a new state we have to visit it
	    if (visited->find (succ_dst) == visited->end())
	      {
		// Mark as visited 
 		visited->insert(succ_dst->clone());

		trace << "ALLOC succ_src : " << succ_src << std::endl;
		trace << "ALLOC succ_dst : " << succ_dst << std::endl;
		todo.push (std::make_pair(succ_src, succ_dst));
	      }


	    bdd cond (si->current_condition());
 	    bdd  acc (si->current_acceptance_conditions());

 	    // Get the info store them to rebuild an ordering set later
	    sort_trans st = {t,
			     (spot::state_explicit*) s_dst->clone(),
			     (spot::state_explicit*)succ_dst->clone(),
			     cond, acc, visited};
	    alist->push_back(st);


	    // Destroy theses states that are now unused
	    succ_src->destroy();
	    succ_dst->destroy();
	  }
	delete si;

	// Reorder all transitions 
 	strategy_dispatcher (alist);

	// Now we just create all transitions (that are push back 
	// in the set of transitions in tgba
 	std::list<rebuild::sort_trans>::iterator it;
 	for (it = alist->begin(); it != alist->end();)
	  {
	    spot:: state_explicit::transition* trans = 0;
	    sort_trans st = *it;
	    trans = t->create_transition(st.sdst,
					 st.succdst);
	    t->add_conditions(trans, st.cond);
	    t->add_acceptance_conditions (trans, st.acc);
	    ++it;
	    alist->pop_front();

	    st.sdst->destroy();

	    // DEBUG
	    trace << "# in  : " << t->format_state(st.sdst) << std::endl;
	    trace << "# out : " << t->format_state(st.succdst) << std::endl;

	  }
  	assert(alist->empty());

	// CLEAN
	s_src->destroy();
	s_dst->destroy();
	trace << "DESTROY : " << s_src << std::endl;
	trace << "DESTROY : " << s_dst << std::endl;

	delete alist;
     }

    // Cleaning visited 
    std::set<spot::state *>::iterator it;
    for ( it=visited->begin() ; it != visited->end(); ++it )
      (*it)->destroy();
    delete visited;

    return t; 
  }

  bool
  rebuild::compare_hierarchy (sort_trans i1, sort_trans i2)
  {
    assert(i1.src == i2.src);
    spot::formula_emptiness_specifier *fes  =
      new spot::formula_emptiness_specifier (i1.src);
    const spot::ltl::formula * cf1 =
      fes->formula_from_state(i1.succdst);

    const spot::ltl::formula * cf2 =
      fes->formula_from_state(i2.succdst);

    // First guarantee
    if (cf1->is_syntactic_guarantee() &&
	!cf2->is_syntactic_guarantee())
      return true;

    // First persistence
    if (cf1->is_syntactic_persistence() &&
	!cf2->is_syntactic_persistence())
      return true;

     delete fes;
     return false;
  }

  bool
  rebuild::compare_shy (sort_trans first, sort_trans second)
  {
    if (first.visited->find (first.succdst) !=
	first.visited->end() &&
	second.visited->find (second.succdst) ==
	second.visited->end())
      return true;
    return false;
  }

  bool
  rebuild::compare_acc (sort_trans first, sort_trans second)
  {
    if (first.acc == all_cond && second.acc != all_cond)
      return true;
    if (first.acc != bddfalse &&  second.acc == bddfalse)
      return true;
    return false;
  }

  bool
  rebuild::compare_pessimistic (sort_trans i1, sort_trans i2)
  {
    return !compare_acc (i1, i2);
  }

  bool
  rebuild::compare_h_pessimistic (sort_trans i1, sort_trans i2)
  {
    return !compare_hierarchy (i1, i2);
  }

  void
  rebuild::strategy_dispatcher (std::list<sort_trans> *l)
  {
    //    assert(!l->empty());	// Why call this if empty?
    switch (is)
      {
      case DEFAULT :
	return;		// Should be optimized at the begining
      case ACC :
	l->sort(rebuild::compare_acc);
	return;
      case SHY :
	l->sort(rebuild::compare_shy);
	return;
      case HIERARCHY :
	l->sort(rebuild::compare_hierarchy);
	return;
      case PESSIMISTIC :
	l->sort(rebuild::compare_pessimistic);
	return;
      case H_PESSIMISTIC :
	l->sort(rebuild::compare_h_pessimistic);
	return;
      }
  }
}
