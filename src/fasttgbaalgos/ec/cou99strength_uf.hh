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

#ifndef SPOT_FASTTGBAALGOS_EC_COU99STRENGTH_UF_HH
# define SPOT_FASTTGBAALGOS_EC_COU99STRENGTH_UF_HH

#include <stack>
#include "misc/hash.hh"
#include "union_find.hh"
#include "fasttgba/fasttgba.hh"
#include "fasttgba/fasttgba.hh"
#include "fasttgba/fasttgba_explicit.hh"
#include "ec.hh"

namespace spot
{
  class cou99strength_uf : public ec
  {
  public:

    /// A constructor taking the automaton to check
    cou99strength_uf(const fasttgba *);

    /// A destructor
    virtual ~cou99strength_uf();

    /// The implementation of the interface
    bool check();

  protected:

    /// \brief Fix set ups for the algo
    inline void init();

    /// \brief Push a new state to explore
     void dfs_push_classic(fasttgba_state*);

    /// \brief  Pop states already explored
     void dfs_pop_classic();

    /// \brief merge multiple states
     void merge_classic(fasttgba_state*);

    /// \brief the main procedure
     void main();

    ///< \brief Storage for counterexample found or not
    bool counterexample_found;

    /// \brief the strength into the automaton of the prop.
    enum scc_strength
    get_strength (const fasttgba_state*);

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

    /// \brief to detect if an iterator has already be once incremented
    bool last;


    typedef Sgi::hash_map<const fasttgba_state*, bool,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> seen_map;
    seen_map H;

    // A cache
    const fasttgba_state* state_cache_; ///< State in cache

    enum scc_strength strength_cache_; ///< strength in cache
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_COU99STRENGTH_UF_HH
