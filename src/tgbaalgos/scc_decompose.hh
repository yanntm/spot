// Copyright (C) 2009, 2010 Laboratoire de Recherche et Developpement
// de l'Epita (LRDE).
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


#ifndef SPOT_TGBAALGOS_SCC_DECOMPOSE_HH
# define SPOT_TGBAALGOS_SCC_DECOMPOSE_HH

#include "tgba/tgba.hh"
#include "tgbaalgos/scc.hh"

namespace spot
{
  /// This class provides the minimal wrapper to 
  /// decompose an automaton using strategies 
  class scc_decompose
  {
  protected:
    bool is_strong;
    bool is_weak;
    bool is_terminal;
    scc_map *sm;		// The map used to decompose
    const tgba* src_;		// The orginal automaton
    tgba* terminal_;		// The terminal extracted 
    tgba* weak_;		// The weak extracted 
    tgba* strong_;		// The strong extracted 
    bool minimize;		// Should we minimize

  public :
    scc_decompose(const tgba *a, bool minimize = false):
      src_(a), minimize (minimize)
    {
      terminal_ = weak_ = strong_ = 0;
      sm = new scc_map(src_);
      sm->build_map();
    }

    virtual
    ~scc_decompose()
    {
      delete sm;
      delete weak_;
      delete strong_;
      delete terminal_;
    }

    /// \brief Return the terminal automaton extracted of the 
    /// automaton provided to this class
    ///
    /// If there is no terminal automaton associated this function 
    /// will return 0
    ///
    /// This function may call decompose function 
    tgba*
    terminal_automaton ();

    /// \brief Return the weakterminal automaton extracted of the 
    /// automaton provided to this class
    ///
    /// If there is no weak automaton associated this function 
    /// will return 0
    ///
    /// This function may call decompose function 
    tgba*
    weak_automaton ();

    /// \brief Return the strong automaton extracted of the 
    /// automaton provided to this class
    ///
    /// If there is no strong automaton associated this function 
    /// will return 0
    ///
    /// This function may call decompose function 
    tgba*
    strong_automaton ();

    /// \brief This function perform a decomposition into 
    /// many automatons on which emptiness checks should be performed
    void
    decompose();


    tgba*
    recompose();


  protected:
    // Decompose into strong automaton
    void decompose_strong();
    // Decompose into weak automaton
    void decompose_weak();
    // Decompose into terminal
    void decompose_terminal();
  };
}

#endif // SPOT_TGBAALGOS_SCC_DECOMPOSE_HH
