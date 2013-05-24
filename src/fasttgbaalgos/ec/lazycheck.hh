// Copyright (C) 2012 Laboratoire de Recherche et Développement
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

#ifndef SPOT_FASTTGBAALGOS_EC_LAZYCHECK_HH
# define SPOT_FASTTGBAALGOS_EC_LAZYCHECK_HH

#include <tuple>

#include <stack>
#include <map>
#include "misc/hash.hh"

#include "boost/tuple/tuple.hpp"

#include "fasttgba/fasttgba.hh"
#include "ec.hh"
#include "deadstore.hh"

namespace spot
{
  class lazycheck : public ec
  {
  public:

    /// A constructor taking the automaton to check
    /// Warning! This only work with B\¨uchi Automaton
    /// It means for TGBA that the number of acceptance set
    /// must be less or equal to one! Assertion fail otherwise
    lazycheck(instanciator* i);

    /// A destructor
    virtual ~lazycheck();

    /// The implementation of the interface
    bool check();

    /// Supply more information in a CSV way
    /// Informations are : Number of merge, number of states mark as dead.
    std::string stats_info();

  protected:

    /// \brief Fix set ups for the algo
    void init();

    /// \brief Push a new state to explore
    void dfs_push(fasttgba_state*);

    /// \brief  Pop states already explored
    void dfs_pop();

    /// \brief the main procedure
    void main();

    void lowlinkupdate(int f, int t, markset acc);

    /// \brief The color for a new State
    enum color {Alive, Dead, Unknown};

    /// \biref return the color of a state
    color get_color(const fasttgba_state*);

    ///< \brief Storage for counterexample found or not
    bool counterexample_found;

    //
    // Define the struct used in the paper
    //
    struct stack_entry
    {
      const fasttgba_state* s;		  // State stored in stack entry.
      fasttgba_succ_iterator* lasttr;     // Last transition explored.
      int lowlink;		          // Lowlink value if this entry.
      int pre;			          // DFS predecessor.
      markset* mark;			  // The associated markset
    };

  private:

    ///< \brief the automaton that will be used for the Emptiness check
    const fasttgba* a_;

    // Stack of visited states on the path.
    typedef std::vector<stack_entry> stack_type;
    stack_type stack;

    int top;		// Top of SCC stack.
    int dftop;		// Top of DFS stack.
    bool violation;	// True if a counterexample is found

    /// The map of visited states
    typedef Sgi::hash_map<const fasttgba_state*, size_t,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> seen_map;
    seen_map H;

    /// \brief The store for Deads states
    deadstore deadstore_;

    // The instance automaton
    const instance_automaton* inst;
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_LAZYCHECK_HH
