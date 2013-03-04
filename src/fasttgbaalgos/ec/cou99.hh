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

#ifndef SPOT_FASTTGBAALGOS_EC_COU99_HH
# define SPOT_FASTTGBAALGOS_EC_COU99_HH

#include <stack>
#include <map>
#include "misc/hash.hh"

#include "boost/tuple/tuple.hpp"

#include "fasttgba/fasttgba.hh"
#include "ec.hh"

namespace spot
{
  class cou99 : public ec
  {
  public:

    /// A constructor taking the automaton to check
    cou99(const fasttgba *);

    /// A destructor
    virtual ~cou99();

    /// The implementation of the interface
    bool check();

  protected:

    /// \brief Fix set ups for the algo
    void init();

    /// \brief Push a new state to explore
    void dfs_push(markset, fasttgba_state*);

    /// \brief  Pop states already explored
    void dfs_pop();

    /// \brief merge multiple states
    void merge(markset, int);

    /// \brief the main procedure
    void main();



    ///< \brief Storage for counterexample found or not
    bool counterexample_found;

  private:

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
    //std::map<const fasttgba_state*, int> H;
    typedef Sgi::hash_map<const fasttgba_state*, int,
			  fasttgba_state_ptr_hash, fasttgba_state_ptr_equal> seen_map;
    seen_map H;

  };
}

#endif // SPOT_FASTTGBAALGOS_EC_COU99_HH
