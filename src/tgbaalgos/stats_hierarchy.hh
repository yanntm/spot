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
      int sum;		///< Sum is equal to all states
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
	sum++;
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
      void process_link(const spot::state*  s_src
			, int,
			const spot::state*  succ_src
			, int ,
			const spot::tgba_succ_iterator*)
      {
	tsum++;
	if (fes.is_guarantee(s_src) && fes.is_guarantee(succ_src))
	  ++ttt;
	else if (fes.is_persistence(s_src) && fes.is_guarantee(succ_src))
	  ++twt;
	else if (fes.is_persistence(s_src) && fes.is_persistence(succ_src))
	  ++tww;
	else if (fes.is_guarantee(succ_src))
	      ++tgt;
	else if (fes.is_persistence(succ_src))
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
    int sum;		///< Sum is equal to all states
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

    virtual
    ~stats_hierarchy  ()
    {
    }

    void stats_automaton ()
    {
      if (sum)
	return; 		// Avoid multiple computation

      processor.run();
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
    }

    // This function retrun true if the automaton is commuting
    // and is a weak automaton
    bool
    is_commuting_weak_automaton()
    {
      stats_automaton ();
      if (is_commuting_automaton () &&
	  scc_weak && !scc_strong)
	{
	  return true;
	}
      return false;
    }
  };

  std::ostream&
  operator<<(std::ostream& os, const stats_hierarchy& s)
  {
    int commut =
      (s.scc_terminal && s.scc_weak)
      ||
      (s.scc_weak && s.scc_strong)
      ||
      (s.scc_terminal && s.scc_strong);

    os << "init:";
    if (s.scc_strong)
      os << "G,";
    else if (s.scc_weak)
      os << "W,";
    else
      os << "T,";

    os << "Terminal:"   << s.scc_terminal    << ","
       << "Weak:"       << s.scc_weak  << ","
       << "General:"    << s.scc_strong  << ","
       << "commut:"     << commut << ","
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
}
#endif // SPOT_TGBAALGO_STATS_HIERARCHY
