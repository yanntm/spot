// Copyright (C) 2004, 2005  Laboratoire d'Informatique de Paris 6 (LIP6),
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

#ifndef SPOT_TGBAALGOS_EMPTINESS_SPECIFIER_HH
# define SPOT_TGBAALGOS_EMPTINESS_SPECIFIER_HH

#include "tgbaalgos/scc.hh"
#include "tgba/state.hh"
#include "tgba/tgba.hh"
#include "tgba/tgbaexplicit.hh"
#include "misc/hash.hh"
#include "ltlast/formula.hh"
#include "ltlast/multop.hh"
#include "tgba/tgbaproduct.hh"

namespace spot
{
  /// This class provides a specifier for emptiness check algorithm
  /// Indeed some algorithms (especially dynamic ones) needs more 
  /// informations to perform efficient check. This information can 
  /// be extract from a state
  class emptiness_specifier
  {
  public :
    virtual
    ~emptiness_specifier()
    { }

    /// \return true if the state belong to a weak accepting 
    /// SCC 
    virtual bool
    is_part_of_weak_acc (const state *) const = 0;

    /// \return true if this two states belong to the 
    /// same weak accepting SCC
    virtual bool
    same_weak_acc (const state *, const state *) const = 0;

    /// \return true if the state can be considered as a guarantee 
    /// state (i.e. leads to terminal state for all path that can be 
    /// general non accepting or terminal non accepting)
    virtual bool
    is_guarantee (const state *) const = 0;

    /// \return true if the state can be considered as a persitence 
    /// state (i.e. belongs to a weak accepting or not SCC and all 
    /// paths leads to weak SCC) 
    virtual bool
    is_persistence (const state *) const = 0;

    /// \return true for all other cases
    virtual bool
    is_general (const state *) const = 0;

    /// \return true if the state belongs to a terminal accepting 
    /// Scc. Here terminal is used in the meaning of terminal automaton
    /// (i.e all paths are fully accepting and the SCC is complete) 
    virtual bool
    is_terminal_accepting_scc (const state *) const = 0;
  };

  /// This class represent a default specifier
  class emptiness_specifier_default : public  emptiness_specifier
  {
  public :
    virtual ~emptiness_specifier_default()
    {
    }

    // Return true if the state belong to a weak accepting SCC
    virtual bool
    is_part_of_weak_acc (const state *) const
    {
      return false;
    }

    virtual bool
    same_weak_acc (const state *, const state *) const
    {
      return false;
    }

    virtual bool
    is_guarantee (const state *) const
    {
      return false;
    }

    virtual bool
    is_persistence (const state *) const
    {
      return false;
    }

    virtual bool
    is_general(const state *) const
    {
      return false;
    }

    virtual bool
    is_terminal_accepting_scc (const state *) const
    {
      return false;
    }
  };

  /// This class represent a specifier which extract algorithm information 
  /// from the system. Here 3 types of systems must be consider to extract 
  /// the information : 
  ///     - single formula system : the emptiness is performed on a single 
  ///       automaton wich is a formula
  ///     - product system : the system is the result of the product of a 
  ///       formula and a system (Kripke)
  ///     - double formula system : the system is the result of the product 
  ///       of two formula tgba 
  /// All theses system must be considered to reply to the fonction 
  /// favorite_emptiness_type 
  class  formula_emptiness_specifier : public  emptiness_specifier
  {
  protected:
    const tgba* sys_;		// The synchronized product automaton
    const tgba* f_;		// The formula automaton
    scc_map * sm;		// The map of scc 
  public :

    /// Create a specifier for a system composed of a unique formula 
    /// Can work if there is no proxy around a 
    formula_emptiness_specifier (const tgba * a);

    /// Create a specifier for a system composed of a unique formula 
    /// and where this formula is wrapped into a proxy
    formula_emptiness_specifier (const tgba * a, const tgba * f);

    virtual
    ~formula_emptiness_specifier()
    {
      delete sm;
    }

    /// Return true if the projection over the formula automata is 
    /// in a weak accepting SCC
    bool
    is_part_of_weak_acc (const state *) const;

    /// Return true if two states belong to the same weak accepting 
    ///  SCC
    bool
    same_weak_acc (const state *, const state *) const;

    /// Check wether the state is a guarantee state i-e it belongs to 
    /// a sub-automaton which is terminal
    bool
    is_guarantee (const state *) const;

    /// Check wether the state is a persistence state i-e it belongs to 
    /// a sub-automaton which is weak
    bool
    is_persistence (const state *) const;

    /// Check if the state is a general state i-e it belongs to 
    /// a sub-automaton which is strong
    bool
    is_general (const state *) const;

    /// Return true if the state is in an accepting terminal 
    /// SCC (i-e, a self lopp that accept all words)
    bool
    is_terminal_accepting_scc (const state *) const;
  };
}

#endif // SPOT_TGBAALGOS_EMPTINESS_SPECIFIER_HH
