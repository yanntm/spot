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
    /// Return the formula associated to the state which is provided as
    /// parameter.
    /// Return NULL if the state doesn't handle formula 
    virtual const ltl::formula *
    formula_from_state (const state *) const = 0;

    virtual bool
    is_part_of_weak_acc (const state *) const = 0;

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
  };

  /// This class represent a default specifier
  class emptiness_specifier_default : public  emptiness_specifier
  {
  public :

    // Return the formula associated to a specific state
    virtual const ltl::formula *
    formula_from_state (const state *) const
    {
      return 0;
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
    const tgba* sys_;		// The synchronized product automaton
    const tgba* f_;		// The formula automaton
    const tgba* s_;		// The system automaton
    bool  both_formula;  	// Boolean for 2 formula automaton
    scc_map * sm ;//= new scc_map(f_);
  public :

    /// Create a specifier for a system composed of a unique formula 
    /// Can work if there is no proxy around a 
    formula_emptiness_specifier (const tgba * a);

    /// Create a specifier for a system composed of a unique formula 
    /// and where this formula is wrapped into a proxy
    formula_emptiness_specifier (const tgba * a, const tgba * f);

    /// Create the specifier taking the automaton of the system and 
    /// the automaton of the formula 
    formula_emptiness_specifier (const tgba * a, const tgba * f, const tgba *s);

    virtual
    ~formula_emptiness_specifier();

    const ltl::formula *
    formula_from_state (const state *) const;

    /// Return true if the projection over the formula automata is 
    /// in a weak accepting SCC
    bool
    is_part_of_weak_acc (const state *) const;

    /// Return true if two states belong to the same weak accepting 
    ///  SCC
    bool
    same_weak_acc (const state *, const state *) const;

    /// Collect all nodes that are self loop terminal accepting 
    /// into the automaton
    /// self loop terminal non accepting have probably been removed 
    /// and all terminal acceptin SCC can be represent as a single 
    /// accepting state labelled by 1 and with a self loop
    std::list<const state *>*
    collect_self_loop_acc_terminal_nodes();

    bool
    is_guarantee (const state *) const;

    bool
    is_persistence (const state *) const;

    bool
    is_general (const state *) const;
  };
}

#endif // SPOT_TGBAALGOS_EMPTINESS_SPECIFIER_HH
