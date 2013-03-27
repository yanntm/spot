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

#ifndef SPOT_FASTTGBAALGOS_EC_COU99STRENGTH_HH
# define SPOT_FASTTGBAALGOS_EC_COU99STRENGTH_HH

#include <stack>
#include <map>
#include "boost/tuple/tuple.hpp"

#include "fasttgba/fasttgba.hh"
#include "fasttgba/fasttgba_explicit.hh"
#include "fasttgba/fasttgba_product.hh"
#include "ec.hh"

namespace spot
{
  class cou99strength : public ec
  {
  public:

    /// A constructor taking the automaton to check
    /// \a allsuccheuristic must be set only if post
    //  computationb is not expensive
    cou99strength(const fasttgba *,
		  bool allsuccheuristic  = false);

    /// A destructor
    virtual ~cou99strength();

    /// The implementation of the interface
    bool check();

  protected:

    /// \brief Fix set ups for the algo
    void init();

    /// \brief  Pop states already explored
    void dfs_pop();

    /// Take the merge fonction to use. There are 2 possibles :
    ///   - first that catches terminal states during the push (default one)
    ///   - second that visit all successors of a state to detect terminal
    /// The second heuristic must be used only when the Post operation is not
    /// expensive
    void (spot::cou99strength::*dfs_push)(markset, fasttgba_state*);

    /// \brief merge multiple states
    void merge(markset, int);

    /// \brief the main procedure
    void main();

    /// \brief the strength into the automaton of the prop.
    enum scc_strength
    get_strength (const fasttgba_state*);

    ///< \brief Storage for counterexample found or not
    bool counterexample_found;

  private:

    /// \brief Push a new state to explore and check wheter
    /// it's a terminal one or not
    void dfs_push_simple_heuristic(markset, fasttgba_state*);

    /// \brief Push a new state to explore and check wheter
    /// one of its successor is terminal or not
    void dfs_push_allsucc_heuristic(markset, fasttgba_state*);

    ///< \brief the automaton that will be used for the Emptiness check
    const fasttgba* a_;

    /// \brief define the type that will be use for the todo stack
    typedef std::pair<const spot::fasttgba_state*,
		      fasttgba_succ_iterator*> pair_state_iter;

    /// \brief the todo stack
    std::stack<pair_state_iter> todo;

    /// \brief define the type that will be use for the SCC stack
    typedef boost::tuple <int, markset, markset,
			  std::list<const fasttgba_state *>* > scc_tuple;

    /// \brief the stack of SCC
    std::stack<scc_tuple> scc;

    /// \brief the DFS number
    int max;


    /// The map of visited states
    typedef Sgi::hash_map<const fasttgba_state*, int,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> seen_map;
    seen_map H;

    // A cache
    const fasttgba_state* state_cache_; ///< State in cache

    enum scc_strength strength_cache_; ///< strength in cache

  };
}

#endif // SPOT_FASTTGBAALGOS_EC_COU99STRENGTH_HH
