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

#ifndef SPOT_FASTTGBAALGOS_EC_OPT_OPT_DIJKSTRA_SCC_HH
# define SPOT_FASTTGBAALGOS_EC_OPT_OPT_DIJKSTRA_SCC_HH

#include <stack>
#include <tuple>
#include "misc/hash.hh"
#include "fasttgba/fasttgba.hh"
#include "fasttgbaalgos/ec/ec.hh"
#include "fasttgbaalgos/ec/root_stack.hh"
#include "fasttgbaalgos/ec/deadstore.hh"

namespace spot
{

  /// \brief This is the Dijkstra SCC computation algorithm
  /// This class also include the optimisation for the live
  /// stack (provided by Nuutila for Tarjan's algorithm but
  /// adapted here) and a possibly stack compression technique
  class SPOT_API opt_dijkstra_scc : public ec
  {
  public:

    /// A constructor taking the automaton to check
    opt_dijkstra_scc(instanciator* i, std::string option = "",
		     bool swarm = false, int tid = 0);

    /// A destructor
    virtual ~opt_dijkstra_scc();

    /// The implementation of the interface
    bool check();

    /// \brief Get extra informations
    std::string extra_info_csv();

  protected:

    /// \brief Fix set ups for the algo
    virtual void init();

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

    virtual color get_color(const fasttgba_state*);

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
    stack_of_roots *roots_stack_;
    generic_stack *stack_;

    /// The store of dead states
    deadstore* deadstore_;

    /// \brief the storage
    typedef Sgi::hash_map<const fasttgba_state*, int,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> seen_map;
    seen_map H;

    /// \brief The instance automaton
    const instance_automaton* inst;

    markset* empty_;
    unsigned int max_live_size_; ///< \brief keep peack size
    unsigned int max_dfs_size_;	 ///< \brief keep peack size
    int update_cpt_;		 ///< \brief count UPDATE calls
    int update_loop_cpt_;	 ///< \brief count UPDATE loop iterations
    int roots_poped_cpt_;	 ///< \brief count UPDATE loop iterations
    int states_cpt_;		 ///< \brief count states
    int transitions_cpt_;	 ///< \brief count transitions
    int memory_cost_;		 ///< \brief evaluates memory
    int trivial_scc_;            ///< \brief count trivial SCCs
    bool swarm_;		 ///< \brief shall use swarming?
    int tid_;                    ///< \brief the thread id
  };

  /// \brief transform the previous algorithm into an emptiness
  /// check. Only refine specific methods
  class SPOT_API opt_dijkstra_ec : public opt_dijkstra_scc
  {
  public :
    opt_dijkstra_ec(instanciator* i, std::string option = "")
      : opt_dijkstra_scc(i, option)
    {}


    /// \brief Add acceptance set deal
    virtual bool merge(fasttgba_state*);
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_OPT_OPT_DIJKSTRA_SCC_HH
