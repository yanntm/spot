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

#ifndef SPOT_FASTTGBAALGOS_EC_COU99_UF_HH
# define SPOT_FASTTGBAALGOS_EC_COU99_UF_HH

#include <stack>
#include "misc/hash.hh"
#include "union_find.hh"
#include "fasttgba/fasttgba.hh"
#include "ec.hh"

namespace spot
{
  class cou99_uf : public ec
  {
  public:

    /// A constructor taking the automaton to check
    cou99_uf(const fasttgba *);

    /// A destructor
    virtual ~cou99_uf();

    /// The implementation of the interface
    bool check();

  protected:

    /// \brief Fix set ups for the algo
    inline void init();

    // /// \brief Push a new state to explore
    // void (spot::cou99_uf::*dfs_push)(fasttgba_state*);

    // /// \brief  Pop states already explored
    // void (spot::cou99_uf::*dfs_pop)();


    // /// \brief merge multiple states
    // void (spot::cou99_uf::*merge)(fasttgba_state*);



    inline void dfs_push_classic(fasttgba_state*);

    inline void dfs_pop_classic();

    inline void merge_classic(fasttgba_state*);



    inline void dfs_push_scc(fasttgba_state*);

    inline void dfs_pop_scc();

    inline void merge_scc(fasttgba_state*);


    /// \brief the main procedure
    inline void main();

    ///< \brief Storage for counterexample found or not
    bool counterexample_found;

  private:

    ///< \brief the automaton that will be used for the Emptiness check
    const fasttgba* a_;

    /// \brief define the type that will be use for the todo stack
    typedef std::pair<const spot::fasttgba_state*,
		      fasttgba_succ_iterator*> pair_state_iter;

    /// \brief the todo stack
    std::vector<pair_state_iter> todo;

    /// \brief the union_find used for the storage
    union_find *uf;


    std::stack<int> scc;

  };
}

#endif // SPOT_FASTTGBAALGOS_EC_COU99_UF_HH
