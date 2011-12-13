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
    todo.push( std::pair <spot::state *, spot::state *> (s, sdst));

    trace << "# init  : " << t->format_state(sdst) << std::endl;

    spot:: state_explicit::transition* trans;
    do 
      {
	// Get the pair to work on
	std::pair <spot::state *, spot::state *> apair = todo.top();
	todo.pop();

	// Init all vars 
	s = apair.first;
	sdst = apair.second;
	visited.insert(sdst);

	std::list< rebuild::sort_trans > *alist = 
	  new std::list< rebuild::sort_trans >();

	// Iterator over the successor of the src
	spot::tgba_explicit_succ_iterator *si = 
	  (tgba_explicit_succ_iterator*) f->succ_iter (s);
	for (si->first(); !si->done(); si->next())
	  {
	    spot::state * succ = si->current_state();
	    spot::state * succdst =  t->add_state(f->get_label(succ));

	    if ( visited.find (succdst) == visited.end())
	      {
		std::cout << "New State  to visit\n";
		todo.push (std::pair <spot::state *, spot::state *> (succ, succdst));
	      }

	    bdd cond (si->current_condition());
 	    bdd  acc ( si->current_acceptance_conditions());

 	    // Get the info store them to rebuild an ordering set later
	    sort_trans st = {t, 
			     (spot::state_explicit*) sdst, 
			     (spot::state_explicit*)succdst, 
			     cond, acc};
	    alist->push_back(st);

 	    succ->destroy();
	  }
	delete (si);

	// Reorder all transitions 
	alist->sort(rebuild::compare_iter);
	//alist->reverse();

	// Now we just create all transitions (that are push back 
	// in the set of transitions in tgba
	std::list<rebuild::sort_trans>::iterator it;
	for (it = alist->begin(); it != alist->end(); )
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

    return t;
  }

  int
  rebuild::compare_iter (sort_trans i1, sort_trans i2)
  {
    assert(i1.src == i2.src);
    spot::formula_emptiness_specifier *fes  =
      new spot::formula_emptiness_specifier (i1.src);
    const spot::ltl::formula * cf1 = 
      fes->formula_from_state(i1.succdst);
  
    const spot::ltl::formula * cf2 = 
      fes->formula_from_state(i2.succdst);

    // Both are Terminal
    if (cf1->is_syntactic_guarantee() && 
	cf2->is_syntactic_guarantee())
      return 0;

    // Both are Weak
    if (cf1->is_syntactic_persistence() && 
	cf2->is_syntactic_persistence())
      return 0;

    // Both are Terminal
    if (!cf1->is_syntactic_persistence() && 
	!cf2->is_syntactic_persistence())
      return 0;

    // First guarantee
    if (cf1->is_syntactic_guarantee() && 
	!cf2->is_syntactic_guarantee())
      return -1;

    // First persistence
    if (cf1->is_syntactic_persistence() && 
	!cf2->is_syntactic_persistence())
      return -1;

   delete(fes);
    return 1;
  }
  
  void 
  rebuild::process_state (spot::tgba_explicit_formula *f,
			  spot::tgba_explicit_formula *tres)
  {
    assert( f && tres);





//     spot::state *s = todo.top();
//     todo.pop();
//     visited.insert (s);
//     spot::tgba_explicit_succ_iterator *si = 
//       (tgba_explicit_succ_iterator*) f->succ_iter (s);
//     std::list<spot::tgba_succ_iterator *> *list_of_succ = 
//       new std::list<spot::tgba_succ_iterator *>();
//     spot:: state_explicit::transition* t;

//     std::cout << "--------------START for a state"
// 	      << tres->format_state(s) << std::endl;
    
//     // Get all succ of the state
//     for (si->first(); !si->done(); si->next())
//     {
//       std::cout << "--------------here\n";
//       spot::tgba_succ_iterator *tmp =  
// 	new spot::tgba_explicit_succ_iterator(*si);
//       list_of_succ->push_back(tmp);
//     }  
//     delete (si);

    
//     // Sort all of these states considering the hierarchy
//     // list_of_succ->sort(rebuild::compare_iter);
  
//     // Construct the new automaton of the formula 
//     std::list<spot::tgba_succ_iterator *>::iterator it;
//     for (it=list_of_succ->begin(); it != list_of_succ->end(); it++)
//       {
// 	std::cout << "there\n";
// 	spot::state * stmp = (*it)->current_state();
// 	stmp = 	tres->add_state(f->get_label(stmp));


// 	if ( visited.find (stmp) == visited.end())
// 	  {
// 	    std::cout << "New State  to visit\n";
// 	    todo.push (stmp);
// 	  }
	

// 	t = tres->create_transition((spot::state_explicit *)s,
// 				    (spot::state_explicit *)stmp);

// 	std::cout << "# in  : " << tres->format_state(s) << std::endl;
// 	std::cout << "# out : " << tres->format_state(stmp) << std::endl;

// 	tres->add_conditions(t, (*it)->current_condition());
// 	tres->add_acceptance_conditions
// 	  (t, (*it)->current_acceptance_conditions());
// 	//stmp->destroy();
// 	//delete(*it);
// 	list_of_succ->pop_front();
//       }
    
//     assert(list_of_succ->empty());
  }  
  

// /// Temporary variable that is used to sort all transitions
// /// Needed by emptiness_specifier 
// const spot::tgba_explicit_formula *ftmp_;

// /// This function compare 2 iterators and compare them
// /// in a temporal property hierarchy way (w.r.t terminal, weak, general 
// /// automata)
// ///
// /// Returns : 
// ///     -1 : if i1 < i2
// ///      0 : if i1 = i2
// ///      1 : on other cases 
// int
// compare_iter (spot::tgba_succ_iterator *i1, 
// 	      spot::tgba_succ_iterator *i2 )
// {

//   spot::formula_emptiness_specifier *fes  =
//     new spot::formula_emptiness_specifier (ftmp_);
//   const spot::ltl::formula * cf1 = 
//     fes->formula_from_state(i1->current_state());
  
//   const spot::ltl::formula * cf2 = 
//     fes->formula_from_state(i2->current_state());

//   // Both are Terminal
//   if (cf1->is_syntactic_guarantee() && 
//       cf2->is_syntactic_guarantee())
//     return 0;

//   // Both are Weak
//   if (cf1->is_syntactic_persistence() && 
//       cf2->is_syntactic_persistence())
//     return 0;

//   // Both are Terminal
//   if (!cf1->is_syntactic_persistence() && 
//       !cf2->is_syntactic_persistence())
//     return 0;

//   // First guarantee
//   if (cf1->is_syntactic_guarantee() && 
//       !cf2->is_syntactic_guarantee())
//     return -1;

//   // First persistence
//   if (cf1->is_syntactic_persistence() && 
//       !cf2->is_syntactic_persistence())
//     return -1;

//   delete(fes);
//   return 1;
// }

// std::stack<spot::state *> todo;
// std::stack<spot::state *> visited;

// /// This function process a state to construct the 
// /// identical tgba but where transitions has been 
// /// reordered 
// void 
// process_state (const spot::tgba_explicit_formula *f, 
// 	       spot::tgba_explicit_formula *tres)
// {
//   spot::state *s = todo.top();
//   visited.push (s);
//   spot::tgba_succ_iterator *si =  f->succ_iter (s);
//   std::list<spot::tgba_succ_iterator *> *list_of_succ = 
//     new std::list<spot::tgba_succ_iterator *>();
//   spot:: state_explicit::transition* t;

//   // Get all succ of the state
//   for (si->first(); !si->done(); si->next())
//     {
//       list_of_succ->push_back(si);
//     }  
//   delete (si);

//   // Sort all of these states considering the hierarchy
//   ftmp_ = f;  
//   list_of_succ->sort(compare_iter);

//   // Construct the new automaton of the formula 
//   std::list<spot::tgba_succ_iterator *>::iterator it;
//   for (it=list_of_succ->begin(); it != list_of_succ->end(); it++)
//     {
//       spot::state * stmp = (*it)->current_state();
//       t = tres->create_transition((spot::state_explicit *)s, (spot::state_explicit *)stmp);
//       tres->add_conditions(t, (*it)->current_condition());
//       tres->add_acceptance_conditions(t, (*it)->current_acceptance_conditions());      
//       stmp->destroy();
//       delete(*it);
//     }
// }

// /// This function transform the tgba of the formula into an equivalent 
// /// tgba where transition have been sorted 
// const spot::tgba* 
// transform_formula(const spot::tgba*formula)
// {
//   assert(formula);
//   spot::bdd_dict *dict = formula->get_dict();
//   const spot::tgba_explicit_formula *f = 
//     dynamic_cast<const spot::tgba_explicit_formula*> (formula);
//   spot::tgba_explicit_formula *t = 
//     new spot::tgba_explicit_formula (dict);

//   spot::state *s = f->get_init_state();
//   t->set_init_state(f->get_label(s));
  
//   todo.push(s);
  
//   while (! todo.empty()) 
//     {
//       process_state (f,t);
//     }
//   return t;
// }
}


    

//     // Iterator over the src
//     spot::tgba_explicit_succ_iterator *si = 
//       (tgba_explicit_succ_iterator*) f->succ_iter (s);
//     for (si->first(); !si->done(); si->next())
//     {
//       std::cout << "--------------here\n";

//       spot::state * stmp = si->current_state();
      

//       stmp = t->add_state(f->get_label(stmp));
//       assert(stmp);
      
// //       if ( visited.find (stmp) == visited.end())
// // 	{
// // 	  std::cout << "New State  to visit\n";
// // 	    todo.push (stmp);
// // 	}
	

//       t->create_transition((spot::state_explicit *)t->get_init_state(),
// 			   (spot::state_explicit *)stmp);

//       //	std::cout << "# in  : " << t->format_state(s) << std::endl;
// 	std::cout << "# out : " << t->format_state(stmp) << std::endl;

//  	tres->add_conditions(t, si->current_condition());
//  	tres->add_acceptance_conditions
//  	  (t, si->current_acceptance_conditions());
//     }  
//     delete(si);

