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

#ifndef SPOT_FASTTGBAALGOS_EC_UNION_SCC_HH
# define SPOT_FASTTGBAALGOS_EC_UNION_SCC_HH

#include <stack>
#include <tuple>
#include "misc/hash.hh"
#include "union_find.hh"
#include "fasttgba/fasttgba.hh"
#include "ec.hh"
#include "root_stack.hh"

namespace spot
{

  /// This class provides the adaptation of the emptiness
  /// check of couvreur using an Union Find structure and
  /// a specific dedicated root stack
  class union_scc : public ec
  {
  public:

    /// A constructor taking the automaton to check
    union_scc(instanciator* i, std::string otpion = "");

    /// A destructor
    virtual ~union_scc();

    /// The implementation of the interface
    bool check();

    /// \brief Get extra informations
    std::string extra_info_csv();

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

    ///< \brief Storage for counterexample found or not
    bool counterexample_found;

    ///< \brief the automaton that will be used for the Emptiness check
    const fasttgba* a_;

    // An element in Todo stack
    struct pair_state_iter
    {
      const spot::fasttgba_state* state;
      fasttgba_succ_iterator* lasttr;
    };

    /// \brief the todo stack
    std::vector<pair_state_iter> todo;

    /// Root of stack
    stack_of_roots *roots_stack_;

    /// \brief the union_find used for the storage
    union_find *uf;

    /// \brief The instance automaton
    const instance_automaton* inst;

    markset* empty_;
    unsigned int max_dfs_size_;	 ///< \brief keep peack size
    int update_cpt_;		 ///< \brief count UPDATE calls
    int update_loop_cpt_;	 ///< \brief count UPDATE loop iterations
    int roots_poped_cpt_;	 ///< \brief count UPDATE loop iterations
    int states_cpt_;		 ///< \brief count states
    int transitions_cpt_;	 ///< \brief count transitions
    int memory_cost_;		 ///< \brief evaluates memory
    int trivial_scc_;            ///< \brief count trivial SCCs
    int K;			 ///< \brief constant to evaluate cost
    int memory_usage_;		 ///< \brief memory consumption
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_UNION_SCC_HH
