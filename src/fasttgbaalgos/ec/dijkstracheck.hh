// Copyright (C) 2013 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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

#ifndef SPOT_FASTTGBAALGOS_EC_DIJKSTRACHECK_HH
# define SPOT_FASTTGBAALGOS_EC_DIJKSTRACHECK_HH

#include <stack>
#include <tuple>
#include "misc/hash.hh"
#include "union_find.hh"
#include "fasttgba/fasttgba.hh"
#include "ec.hh"
#include "root_stack.hh"
#include "deadstore.hh"

namespace spot
{

  /// This class provides the adaptation of the emptiness
  /// check of couvreur using an Union Find structure and
  /// a specific dedicated root stack
  class dijkstracheck : public ec
  {
  public:

    /// A constructor taking the automaton to check
    dijkstracheck(instanciator* i);

    /// A destructor
    virtual ~dijkstracheck();

    /// The implementation of the interface
    bool check();

  protected:

    /// \brief Fix set ups for the algo
    inline void init();

    // ------------------------------------------------------------
    // For classic algorithm with stack and UF
    // ------------------------------------------------------------

    /// \brief Push a new state to explore
    virtual void dfs_push(fasttgba_state*);

    /// \brief  Pop states already explored
    virtual void dfs_pop();

    /// \brief merge multiple states
    virtual bool merge(fasttgba_state*);

    /// \brief the main procedure
    virtual void main();

    /// \brief The color for a new State
    enum color {Alive, Dead, Unknown};

    color get_color(const fasttgba_state*);

    ///< \brief Storage for counterexample found or not
    bool counterexample_found;

    ///< \brief the automaton that will be used for the Emptiness check
    const fasttgba* a_;

    // An element in Todo stack
    struct pair_state_iter
    {
      const spot::fasttgba_state* state;
      fasttgba_succ_iterator* lasttr;
      long unsigned int position;
    };

    /// \brief the todo stack
    std::vector<pair_state_iter> todo;
    std::vector<const spot::fasttgba_state*> live;

    /// Root of stack
    compressed_stack_of_roots *roots_stack_;

    /// The store of dead states
    deadstore* deadstore_;

    /// \brief the storage
    typedef Sgi::hash_map<const fasttgba_state*, int,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> seen_map;
    seen_map H;

    /// \brief The instance automaton
    const instance_automaton* inst;
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_DIJKSTRACHECK_HH
