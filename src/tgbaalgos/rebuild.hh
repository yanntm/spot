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

#ifndef SPOT_TGBAALGOS_REBUILD_HH
# define SPOT_TGBAALGOS_REBUILD_HH

#include <stack>
#include <set>

#include "tgba/tgba.hh"
#include "tgba/tgbaexplicit.hh"

namespace spot
{

  /// Allows to perform some changes on the tgba which
  /// This classe is used to try a speedup when performing 
  /// emptiness 
  class rebuild
  {
  public:

    /// This iterator is used to specify which stategy to apply 
    /// for reordering all transitions 
    enum iterator_strategy
      {
	DEFAULT, 		///< No changes
	ACC,			///< Accepting transitions are visited first
	SHY,			///< Transitions leading to a known state first
	HIERARCHY,		///< Order in term of terminal, weak, general
      };

  protected:
    // Internal structure which is used to store all transition to 
    // perform later the ordering considering the stategy
    struct sort_trans
    {
      const tgba *           src;     ///< The source automaton 
      spot::state_explicit * sdst;    ///< The From state of the transition
      spot::state_explicit * succdst; ///< The To state of the transition
      bdd                    cond;    ///< Condition over original transition
      bdd                    acc;     ///< Acceptance over original transition
      std::set <spot::state *> *visited;
    };

    tgba* src;		                ///< The original tgba
    std::set <spot::state *> *visited;	///< The visited staes
    iterator_strategy        is;	///< The current strategy

    /// The states to visit : first match original TGBA , second the 
    /// newly constructed
    std::stack<std::pair <spot::state *, spot::state *> > todo;

  public:

    rebuild (tgba* original, iterator_strategy i_s = DEFAULT) :
      src(original), is (i_s)
    { }

    virtual
    ~rebuild ()
    {
      assert (todo.empty());
    }
    /// Reorder transition of the TGBA considering the 
    /// hierarchy of temporal properties    
    tgba* reorder_transitions ();

  private :
    /// This function compare  in the sens of the relation of temporal 
    /// logic property. It means that we want to visit first states leading
    /// into Terminal sub-automaton, then Weak and at least General
    static bool
    compare_hierarchy (sort_trans i1, sort_trans i2);

    /// This function compare states giving preference to states that have 
    /// been already visited (chances to find a DFS). It means that 
    /// we first visit states already visited then the others
    static bool
    compare_shy (sort_trans i1, sort_trans i2);

    /// This method is used to perform the chosen strategy 
    /// It acts like a switch between all of these previously defined 
    /// methods 
    void
    strategy_dispatcher (std::list<sort_trans> *l);

  };
}
#endif // SPOT_TGBAALGO_REBUILD_HH

