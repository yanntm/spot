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

    // Return true if the state belong to a weak accepting SCC
    virtual bool
    is_part_of_weak_acc (const state *)
    {
      return false;
    }

    virtual bool
    same_weak_acc (const state *, const state *)
    {
      return false;
    }

    virtual bool
    is_terminal_accepting_scc (const state *)
    {
      return false;
    }

    /// Return the strneght of the sub automaton
    virtual strength
    typeof_subautomaton(const state *)
    {
      return StrongSubaut;
    }
  };

  /// This class represent a specifier which extract algorithm information
  /// from the TGBA. Here 2 types of systems must be consider to extract
  /// the information :
  ///     - single formula TGBA : the emptiness is performed on a single
  ///       automaton wich is a formula
  ///     - product TGBA : the system is the result of the product of a
  ///       formula and a system (Kripke)
  /// All theses system must be considered to reply to the fonction
  /// favorite_emptiness_type
  class  formula_emptiness_specifier : public  emptiness_specifier
  {
  protected:
    const tgba* sys_;		// The synchronized product automaton
    const tgba* f_;		// The formula automaton
    bool delete_sm;		// Should we delete the sm
    scc_map * sm;		// The map of scc
    const state *state_cache_;	// The state in cache
    const state *right_cache_;	// The right part of this state
    strength strength_cache_;	// The strength in the cache
    bool termacc_cache_;
    unsigned id_cache_;
    bool weak_accepting_cache_;
  public :

    /// Create a specifier for a system composed of a unique formula
    /// Can work if there is no proxy around a
    formula_emptiness_specifier (const tgba * a, scc_map* scm = 0);

    /// Create a specifier for a system composed of a unique formula
    /// and where this formula is wrapped into a proxy
    formula_emptiness_specifier (const tgba * a, const tgba * f, scc_map* scm = 0);

    virtual
    ~formula_emptiness_specifier()
    {
      //state_cache_->destroy();
      if (delete_sm)
	delete sm;
    }

    /// Return true if the projection over the formula automata is
    /// in a weak accepting SCC
    bool
    is_part_of_weak_acc (const state *);

    /// Return true if two states belong to the same weak accepting
    ///  SCC
    bool
    same_weak_acc (const state *, const state *);

    /// Return true if the state is in an accepting terminal
    /// SCC (i-e, a self lopp that accept all words)
    bool
    is_terminal_accepting_scc (const state *);

    /// Return the strneght of the sub automaton
    strength
    typeof_subautomaton(const state *);

  private :
    inline void update_cache(const state *);
  };
}

#endif // SPOT_TGBAALGOS_EMPTINESS_SPECIFIER_HH
