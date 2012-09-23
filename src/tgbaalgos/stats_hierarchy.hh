// Copyright (C) 2009, 2010, 2011 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
// Copyright (C) 2003, 2004, 2006 Laboratoire d'Informatique de
// Paris 6 (LIP6), département Systèmes Répartis Coopératifs (SRC),
// Université Pierre et Marie Curie.
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

#ifndef SPOT_TGBAALGOS_STATS_HIERARCHY_HH
# define SPOT_TGBAALGOS_STATS_HIERARCHY_HH

#include <stack>
#include <set>
#include <string.h>
#include "tgba/tgba.hh"
#include "tgba/tgbaexplicit.hh"
#include "emptiness_specifier.hh"
#include "tgba/tgbaexplicit.hh"
#include "ltlast/formula.hh"
#include "ltlast/constant.hh"
#include "tgbaalgos/reachiter.hh"

namespace spot
{
    class stats_bfs: public spot::tgba_reachable_iterator_breadth_first
    {
    public:
      int safety;	///< Number of safety states (syntax)
      int guarantee;	///< Number of gurantee states (syntax)
      int obligation;	///< Number of obligation states (syntax)
      int persistence;	///< Number of persistence states (syntax)
      int recurrence;	///< Number of reccurence states (syntax)
      int reactivity;	///< Number of reactivity states (syntax)
      int sum;		///< Sum is equal to all previous (syntax)
      int tgw;		///< Transition from general to weak (syntax)
      int tgg;		///< Transition from general to general (syntax)
      int tgt;		///< Transition from general to terminal (syntax)
      int twt;		///< Transition from weak to terminal (syntax)
      int tww;		///< Transition from weak to weak (syntax)
      int ttt;		///< Transition from terminal to terminal (syntax)
      int tsum;		///< The sum of all transition explored 

      int scc_terminal;		/// Number of terminal states 
      int scc_weak;		/// Number of weak states 
      int scc_strong;		/// Number of stong states

      formula_emptiness_specifier fes; ///< An acccess to formula informations

      stats_bfs(const spot::tgba* a)
	: tgba_reachable_iterator_breadth_first(a),
	  fes(a)
      {
	safety        = 0;
	guarantee     = 0;
	obligation    = 0;
	persistence   = 0;
	recurrence    = 0;
	reactivity    = 0;
	sum           = 0;
	tgw           = 0;
	tgt           = 0;
	twt           = 0;
	tsum          = 0;
	tgg           = 0;
	ttt           = 0;
	tww           = 0;

	scc_terminal = 0;
	scc_weak = 0;
	scc_strong = 0;

      }

      /// Called by run() to process a state.
      ///
      /// \param s The current state.
      void process_state(const spot::state* s, int, spot:: tgba_succ_iterator*)
      {
	const ltl::formula* f = 0;
	f = fes.formula_from_state(s);
	++sum;
	if (f->is_syntactic_guarantee ()  ||
	    (ltl::constant::true_instance() == f))
	  ++guarantee;
	else if (f->is_syntactic_safety ())
	  ++safety;
	else if (f->is_syntactic_obligation ())
	  ++obligation;
	else if (f->is_syntactic_persistence ())
	  ++persistence;
	else if (f->is_syntactic_recurrence ())
	  ++recurrence;
	else
	  ++reactivity;

	if (fes.is_guarantee(s))
	  ++scc_terminal;
	else if (fes.is_persistence(s))
	  ++ scc_weak;
	else
	  ++ scc_strong;
      }

      /// Called by run() to process a transition.
      ///
      /// \param s_src The source state
      /// \param succ_src The destination state
      ///
      /// The in_s and out_s states are owned by the
      /// spot::tgba_reachable_iterator instance and destroyed when the
      /// instance is destroyed.
      void process_link(const spot::state* s_src, int,
			const spot::state* succ_src, int ,
			const spot::tgba_succ_iterator*)
      {
	const ltl::formula* fsrc = 0;
	const ltl::formula* fdst = 0;
	fsrc = fes.formula_from_state(s_src);
	fdst = fes.formula_from_state(succ_src);
	++tsum;

	if ((fsrc->is_syntactic_guarantee ()  ||
	     ltl::constant::true_instance() == fsrc) &&
	    (fdst->is_syntactic_guarantee ()  ||
	     ltl::constant::true_instance() == fdst))
		  ++ttt;
	else if ((fsrc->is_syntactic_safety() ||
		  fsrc->is_syntactic_obligation() ||
		  fsrc->is_syntactic_persistence()) &&
		 (fdst->is_syntactic_guarantee()   ||
		  ltl::constant::true_instance() == fdst))
	  ++twt;
	else if ((fsrc->is_syntactic_safety () ||
		  fsrc->is_syntactic_obligation () ||
		  fsrc->is_syntactic_persistence ()) &&
		 (fdst->is_syntactic_safety ()||
		  fdst->is_syntactic_obligation () ||
		  fdst->is_syntactic_persistence ()))
	  ++tww;
	else if (fdst->is_syntactic_guarantee ()  ||
		 ltl::constant::true_instance() == fdst)
	  ++tgt;
	else if  (fdst->is_syntactic_safety ()||
		  fdst->is_syntactic_obligation () ||
		  fdst->is_syntactic_persistence ())
	  ++tgw;
	else
	  ++tgg;
      }
    };


  /// This class is used to perform an evaluation of the 
  /// structure of the automaton of the formula 
  /// Get info about Weak, Terminal and General automata
  class stats_hierarchy
  {
  protected:
    const tgba* src;    ///< The original tgba

  public:
    int safety;		///< Number of safety states
    int guarantee;	///< Number of gurantee states
    int obligation;	///< Number of obligation states
    int persistence;	///< Number of persistence states
    int recurrence;	///< Number of reccurence states 
    int reactivity;	///< Number of reactivity states 
    int sum;		///< Sum is equal to all previous
    int tgw;		///< Transition from general to weak
    int tgg;		///< Transition from general to general
    int tgt;		///< Transition from general to terminal
    int twt;		///< Transition from weak to terminal
    int tww;		///< Transition from weak to weak
    int ttt;		///< Transition from terminal to terminal
    int tsum;		///< The sum of all transition explored 

    int scc_terminal;	/// Number of terminal states (by scc analysis)
    int scc_weak;	/// Number of weak states (by scc analysis)
    int scc_strong;	/// Number of stong states (by scc analysis)

    stats_bfs processor; 	///<  Use traditional bfs walk

    /// The constructor with a tgba formula
    stats_hierarchy (const tgba* original) :
      src(original),
      processor(original)
    {
      safety        = 0;
      guarantee     = 0;
      obligation    = 0;
      persistence   = 0;
      recurrence    = 0;
      reactivity    = 0;
      sum           = 0;
      tgw           = 0;
      tgt           = 0;
      twt           = 0;
      tsum          = 0;
      tgg           = 0;
      ttt           = 0;
      tww           = 0;
      scc_terminal  = 0;
      scc_weak      = 0;
      scc_strong    = 0;
    }

    void stats_automaton ()
    {
      if (sum)
	return; 		// Avoid multiple computation

      processor.run();
      safety        = processor.safety;
      guarantee     = processor.guarantee;
      obligation    = processor.obligation;
      persistence   = processor.persistence;
      recurrence    = processor.recurrence;
      reactivity    = processor.reactivity;
      sum           = processor.sum;
      tgw           = processor.tgw;
      tgt           = processor.tgt;
      twt           = processor.twt;
      tsum          = processor.tsum;
      tgg           = processor.tgg;
      ttt           = processor.ttt;
      tww           = processor.tww;

      scc_terminal  = processor.scc_terminal;
      scc_weak      = processor.scc_weak;
      scc_strong    = processor.scc_strong;

      // FIXME : direct access to processor variables 
      assert (safety + guarantee + obligation + persistence + recurrence
	      + reactivity == sum);
      assert (tgg + tgw + tgt + tww + twt + ttt == tsum);
    }

    // This function return true if the automaton is 
    // commuting in any way possible
    bool
    is_commuting_automaton()
    {
      stats_automaton ();
      return
	(scc_terminal && scc_weak)
	||
	(scc_weak && scc_strong)
	||
	(scc_terminal && scc_strong);


// (guarantee && (safety + obligation + persistence))
// 	||
//       (guarantee && (recurrence + reactivity))
// 	||
//       ((recurrence + reactivity) &&
//        (safety + obligation + persistence));
    }

    // This function retrun true if the automaton is commuting
    // and is a general automaton
    bool
    is_commuting_general_automaton()
    {
      stats_automaton ();
      if (is_commuting_automaton () && scc_strong)
	{
	  return true;
	}
      return false;
//       if (is_commuting_automaton () && 
// 	  (reactivity || recurrence ))
// 	{
// 	  return true;
// 	}
//       return false;
    }

    // This function retrun true if the automaton is commuting
    // and is a weak automaton
    bool
    is_commuting_weak_automaton()
    {
      stats_automaton ();
      if (is_commuting_automaton () &&
	  scc_weak && !scc_strong)
	return true;
      return false;
//       if (is_commuting_automaton () && 
// 	  (persistence || obligation || safety)
// 	  && !reactivity && !recurrence)
// 	return true;
//       return false;
    }
  };

  std::ostream&
  operator<<(std::ostream& os, const stats_hierarchy& s)
  {
    int scommut =
      (s.guarantee && (s.safety + s.obligation + s.persistence))
      ||
      (s.guarantee && (s.recurrence + s.reactivity))
      ||
      ((s.recurrence + s.reactivity) &&
	(s.safety + s.obligation + s.persistence));

    int commut =
      (s.scc_terminal && s.scc_weak)
      ||
      (s.scc_weak && s.scc_strong)
      ||
      (s.scc_terminal && s.scc_strong);

    os << "init:";
    if (s.recurrence + s.reactivity)
      os << "G,";
    else if (s.safety + s.obligation + s.persistence)
      os << "W,";
    else
      os << "T,";

    os << "Terminal:"   << s.scc_terminal    << ","
       << "Weak:"       << s.scc_weak  << ","
       << "General:"    << s.scc_strong  << ","
       << "commut:"     << commut << ","
       << "STerminal:"  << s.guarantee    << ","
       << "SWeak:"      << s.safety + s.obligation + s.persistence  << ","
       << "SGeneral:"   << s.recurrence + s.reactivity  << ","
       << "Scommut:"    << scommut << ","
       << "states:"     << s.sum << ","
       << "trans:"      << s.tsum << ","
       << "tgg:"        << s.tgg  << ","
       << "tgw:"        << s.tgw  << ","
       << "tgt:"        << s.tgt  << ","
       << "tww:"        << s.tww  << ","
       << "twt:"        << s.twt  << ","
       << "ttt:"        << s.ttt
       << std::endl;

      return os;
    }

//       // Here create an emptiness specifier to get the type
//       // of the formula
//       formula_emptiness_specifier fes (src);

//       // Visited states
//       std::set <spot::state *>*
// 	visited_tmp = new std::set< spot::state *>();

//       // todo 
//       std::stack
// 	<spot::state_explicit*> todo_tmp;

//       // Initial state of the automaton
//       spot::state_explicit *i_src =
// 	(spot::state_explicit *)src->get_init_state();
//       visited_tmp->insert(i_src->clone());
//       todo_tmp.push (i_src->clone());

//       {
// 	const ltl::formula* f = 0;
// 	f = fes.formula_from_state(i_src->clone());
// 	++sum;
// 	if (f->is_syntactic_guarantee ())
// 	  ++guarantee;
// 	else if (f->is_syntactic_safety ())
// 	  ++safety;
// 	else if (f->is_syntactic_obligation ())
// 	  ++obligation;
// 	else if (f->is_syntactic_persistence ())
// 	  ++persistence;
// 	else if (f->is_syntactic_recurrence ())
// 	  ++recurrence;
// 	else
// 	  ++reactivity;
//       }

//       while (!todo_tmp.empty())	// We have always an initial state 
// 	{
// 	  // Init all vars 
// 	  spot::state_explicit *s_src;
// 	  s_src = todo_tmp.top();
// 	  todo_tmp.pop();

// 	  // Iterator over the successor of the src
// 	  spot::tgba_explicit_succ_iterator *si =
// 	    (tgba_explicit_succ_iterator*) src->succ_iter (s_src);
// 	  for (si->first(); !si->done(); si->next())
// 	    {
// 	      // Get successor of the src
// 	      spot::state_explicit * succ_src = si->current_state();

// 	      // It's a new state we have to visit it
// 	      if (visited_tmp->find (succ_src) == visited_tmp->end())
// 		{
// 		  // Mark as visited 
// 		  visited_tmp->insert(succ_src);

// 		  todo_tmp.push (succ_src);

// 		  {
// 		    const ltl::formula* f = 0;
// 		    f = fes.formula_from_state(succ_src->clone());
// 		    ++sum;
// 		    if (f->is_syntactic_guarantee ()  ||
// 			(ltl::constant::true_instance() == f))
// 		      ++guarantee;
// 		    else if (f->is_syntactic_safety ())
// 		      ++safety;
// 		    else if (f->is_syntactic_obligation ())
// 		      ++obligation;
// 		    else if (f->is_syntactic_persistence ())
// 		      ++persistence;
// 		    else if (f->is_syntactic_recurrence ())
// 		      ++recurrence;
// 		    else
// 		      ++reactivity;
// 		  }
// 		}

// 	      // Here provide some stats about transition comutatation
// 	      {
// 		const ltl::formula* fsrc = 0;
// 		const ltl::formula* fdst = 0;
// 		fsrc = fes.formula_from_state(s_src->clone());
// 		fdst = fes.formula_from_state(succ_src->clone());
// 		tsum ++;

//  		if ((fsrc->is_syntactic_guarantee ()  ||
// 		     ltl::constant::true_instance() == fsrc) &&
// 		    (fdst->is_syntactic_guarantee ()  ||
// 		     ltl::constant::true_instance() == fdst))
// 		  ++ttt;
// 		else if ((fsrc->is_syntactic_safety() ||
// 			  fsrc->is_syntactic_obligation() || 
// 			  fsrc->is_syntactic_persistence()) &&
// 			 (fdst->is_syntactic_guarantee()   ||
// 			  ltl::constant::true_instance() == fdst))
// 		  ++twt;
// 		else if ((fsrc->is_syntactic_safety () ||
// 			  fsrc->is_syntactic_obligation () || 
// 			  fsrc->is_syntactic_persistence ()) &&
// 			 (fdst->is_syntactic_safety ()||
// 			  fdst->is_syntactic_obligation () || 
// 			  fdst->is_syntactic_persistence ()))
// 		  ++tww;
// 		else if (fdst->is_syntactic_guarantee ()  ||
// 			 ltl::constant::true_instance() == fdst)
// 		  ++tgt;
// 		else if  (fdst->is_syntactic_safety ()||
// 			  fdst->is_syntactic_obligation () || 
// 			  fdst->is_syntactic_persistence ())
// 		  ++tgw;
// 		else 
// 		  ++tgg;
// 	      }


// 	      // Destroy theses states that are now unused
// 	      succ_src->destroy();
// 	    }
// 	  delete si;

// 	  // CLEAN
// 	  s_src->destroy();
// 	}

//       // No more in use
//       i_src->destroy();

//       std::set<spot::state *>::iterator it;
//       for (it=visited_tmp->begin(); it != visited_tmp->end(); ++it)
// 	(*it)->destroy();
//       delete visited_tmp;
}
#endif // SPOT_TGBAALGO_STATS_HIERARCHY
