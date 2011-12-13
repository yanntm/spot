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
  protected:
    tgba* src;		                ///< The original tgba

    std::set <spot::state *> visited;	///< The visited staes

    /// The states to visit : first match original TGBA , second the 
    /// newly constructed
    std::stack<std::pair <spot::state *, spot::state *> > todo;	

    struct sort_trans 
    {
      const tgba *           src; 
      spot::state_explicit * sdst;
      spot::state_explicit * succdst;
      bdd                    cond;
      bdd                    acc;
    };
    
    
  public:
    rebuild (tgba* original) : src(original)
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
    /// This function compare two iterator in the sens of the relation 
    /// of temporal logic property. It means that a iterator whose current 
    /// state is a guarantee property is consider lesser than one which 
    /// has a persitence current state.
    static int
    compare_iter (sort_trans i1, sort_trans i2);

    /// This function process a state to construct the 
    /// identical tgba but where transitions has been 
    /// reordered 
    void 
    process_state (spot::tgba_explicit_formula *f,
		   spot::tgba_explicit_formula *tres);


  };
}
#endif // SPOT_TGBAALGO_REBUILD_HH

