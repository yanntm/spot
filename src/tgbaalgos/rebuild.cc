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

#define REBUILD_TRACE
#ifdef REBUILD_TRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

namespace spot
{
  tgba*
  rebuild::reorder_transitions ()
  {
    // Get the dictionnary 
    spot::bdd_dict *dict = src->get_dict();

    // Convert once for all TGBA and initialize
    spot::tgba_explicit_formula *f =
      dynamic_cast<spot::tgba_explicit_formula*> (src);
    spot::tgba_explicit_formula *t =
      new spot::tgba_explicit_formula (dict);

    dict->register_all_variables_of(f, t);

    // Create the new initial state for the new TGBA
    spot::state *s = f->get_init_state();
    t->set_init_state(f->get_label(s));

    spot::state *sdst = t->get_init_state();
    todo.push(std::pair <spot::state *, spot::state *> (s, sdst));

    trace << "# init  : " << t->format_state(sdst) << std::endl;

    visited = new std::set< spot::state *>();

    spot:: state_explicit::transition* trans;
    do
      {
	// Get the pair to work on
	std::pair <spot::state *, spot::state *> apair = todo.top();
	todo.pop();

	// Init all vars 
	s = apair.first;
	sdst = apair.second;
	visited->insert(sdst);

	std::list< rebuild::sort_trans > *alist =
	  new std::list< rebuild::sort_trans >();

	// Iterator over the successor of the src
	spot::tgba_explicit_succ_iterator *si =
	  (tgba_explicit_succ_iterator*) f->succ_iter (s);
	for (si->first(); !si->done(); si->next())
	  {
	    spot::state * succ = si->current_state();
	    spot::state * succdst =  t->add_state(f->get_label(succ));

	    if (visited->find (succdst) == visited->end())
	      {
		std::cout << "New State  to visit\n";
		todo.push (std::pair <spot::state *, spot::state *>
			   (succ, succdst));
	      }

	    bdd cond (si->current_condition());
 	    bdd  acc (si->current_acceptance_conditions());

 	    // Get the info store them to rebuild an ordering set later
	    sort_trans st = {t,
			     (spot::state_explicit*) sdst,
			     (spot::state_explicit*)succdst,
			     cond, acc, visited};
	    alist->push_back(st);

 	    succ->destroy();
	  }
	delete si;

	// Reorder all transitions 
	strategy_dispatcher (alist);
	//alist->sort(rebuild::compare_iter);
	//alist->reverse();

	// Now we just create all transitions (that are push back 
	// in the set of transitions in tgba
	std::list<rebuild::sort_trans>::iterator it;
	for (it = alist->begin(); it != alist->end();)
	  {
	    sort_trans st = *it;
	    trans = t->create_transition(st.sdst,
					 st.succdst);
	    t->add_conditions(trans, st.cond);
	    t->add_acceptance_conditions (trans, st.acc);
	    st.succdst->destroy();

	    ++it;
	    alist->pop_front();

	    // DEBUG
	    trace << "# in  : " << t->format_state(st.sdst) << std::endl;
	    trace << "# out : " << t->format_state(st.succdst) << std::endl;
	  }
	assert(alist->empty());

	s->destroy();
	sdst->destroy();
      } while (!todo.empty());	// We have always an initial state 
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
    if (first.visited->find (first.succdst) ==
	first.visited->end() &&
	second.visited->find (second.succdst) !=
	second.visited->end())
      return true;
    return false;
  }

  void
  rebuild::strategy_dispatcher (std::list<sort_trans> *l)
  {
    assert(!l->empty());	// Why call this if empty?
    switch (is)
      {
      case DEFAULT :
	return;		// Should be optim at the begining
      case ACC :
	break;
      case SHY :
	l->sort(rebuild::compare_shy);
	break;
      case HIERARCHY :
	l->sort(rebuild::compare_hierarchy);
	break;
      }
  }
}
