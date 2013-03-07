// Copyright (C) 2013 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef SPOT_TGBAALGOS_STATS_HIERARCHY_HH
# define SPOT_TGBAALGOS_STATS_HIERARCHY_HH

#include <stack>
#include <set>
#include <string.h>
#include "tgba/tgba.hh"
#include "tgba/tgbaexplicit.hh"
//#include "emptiness_specifier.hh"
#include "tgba/tgbaexplicit.hh"
#include "ltlast/formula.hh"
#include "ltlast/constant.hh"
#include "tgbaalgos/reachiter.hh"

namespace spot
{

  /// This class is used to know if an automaton is an heterogeneous
  /// automaton i-e there is multiple strength of SCC inside of this
  /// automaton
  class stats_hierarchy
  {
  protected:
    const tgba* src;    ///< The original tgba

  public:
    bool scc_terminal;		// Is there any terminal scc
    bool scc_weak;		// Is there any weak scc
    bool scc_strong;		// Is there any strong scc

    /// The constructor with a tgba formula
    stats_hierarchy (const tgba* original) :
      src(original)
    {
      scc_terminal  = false;
      scc_weak      = false;
      scc_strong    = false;
    }

    virtual
    ~stats_hierarchy  ()
    {
    }

    void stats_automaton ()
    {
      if (scc_terminal || scc_weak || scc_strong)
	return;

      spot::scc_map* x = new spot::scc_map(src);
      x->build_map();
      scc_terminal = x->has_terminal_scc();
      scc_weak = x->has_weak_scc();
      scc_strong = x->has_strong_scc();
      delete x;
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
      os << "S,";
    else if (s.scc_weak)
      os << "W,";
    else
      os << "T,";

    os << "Terminal:"   << s.scc_terminal    << ",";
    os << "Weak:"       << s.scc_weak  << ",";
    os << "Strong:"     << s.scc_strong  << ",";
    os << "commut:" << commut;
    return os;
  }
}
#endif // SPOT_TGBAALGO_STATS_HIERARCHY
