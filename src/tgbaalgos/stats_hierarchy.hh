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


namespace spot
{
  /// This class is used to perform an evaluation of the 
  /// structure of the automaton of the formula 
  /// Get info about Weak, Terminal and General automata
  class stats_hierarchy
  {
  protected:
    const tgba* src;	        ///< The original tgba

  public:
    int safety;		///< Number of safety states
    int guarantee;	///< Number of gurantee states
    int obligation;	///< Number of obligation states
    int persistence;	///< Number of persistence states
    int recurrence;	///< Number of reccurence states 
    int reactivity;	///< Number of reactivity states 
    int sum;		///< sum is equal to all previous


    /// The constructor with a tgba formula
    stats_hierarchy (const tgba* original) :
      src(original)
    {
      safety        = 0;
      guarantee     = 0;
      obligation    = 0;
      persistence   = 0;
      recurrence    = 0;
      reactivity    = 0;
      sum           = 0;
    }

    void stats_automaton ()
    {
      // Here create an emptiness specifier to get the type
      // of the formula
      formula_emptiness_specifier fes (src);

      // Visited states
      std::set <spot::state *>*
	visited_tmp = new std::set< spot::state *>();

      // todo 
      std::stack
	<spot::state_explicit*> todo_tmp;

      // Initial state of the automaton
      spot::state_explicit *i_src =
	(spot::state_explicit *)src->get_init_state();
      visited_tmp->insert(i_src->clone());
      todo_tmp.push (i_src->clone());

      {
	const ltl::formula* f = 0;
	f = fes.formula_from_state(i_src->clone());
	++sum;
	if (f->is_syntactic_guarantee ())
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
      }

      while (!todo_tmp.empty())	// We have always an initial state 
	{
	  // Init all vars 
	  spot::state_explicit *s_src;
	  s_src = todo_tmp.top();
	  todo_tmp.pop();

	  // Iterator over the successor of the src
	  spot::tgba_explicit_succ_iterator *si =
	    (tgba_explicit_succ_iterator*) src->succ_iter (s_src);
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
		    f = fes.formula_from_state(succ_src->clone());
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
      assert (safety + guarantee + obligation + persistence + recurrence
	      + reactivity == sum);
    }
  };

  std::ostream&
  operator<<(std::ostream& os, const stats_hierarchy& s)
  {
    // os << dt.mo << '/' << dt.da << '/' << dt.yr;
    //       os << "guarantee:" << guarantee    << ","
    // 		<< "safety:" << safety       << "," 
    // 		<< "obligation:" << obligation   << ","
    // 		<< "persistence:" << persistence  << ","
    // 		<< "reccurence:" << recurrence   << ","
    // 		<< "reactivity:" << reactivity << std::endl; 
    int commut =
      (s.guarantee && (s.safety + s.obligation + s.persistence))
      ||
      (s.guarantee && (s.recurrence + s.reactivity))
      ||
      ((s.recurrence + s.reactivity) &&
	(s.safety + s.obligation + s.persistence));

    os << "Terminal:"   << s.guarantee    << ","
       << "Weak:"       << s.safety + s.obligation + s.persistence  << ","
       << "General:"    << s.recurrence + s.reactivity  << ","
       << "commut:"     << commut
       << std::endl;

      return os;
    }
}
#endif // SPOT_TGBAALGO_STATS_HIERARCHY
