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

#ifndef SPOT_FASTTGBAALGOS_EC_STATS_HH
# define SPOT_FASTTGBAALGOS_EC_STATS_HH

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
  class SPOT_API stats : public ec
  {
  private:
    /// The map of visited states
    typedef Sgi::hash_map<const fasttgba_state*, int,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> seen_map;

  public:

    /// A constructor taking the automaton to check
    stats(instanciator* i, std::string option = "", bool iskripke = false);

    /// A destructor
    virtual ~stats();

    /// The implementation of the interface
    bool check();

    /// Supply more information in a CSV way
    /// Informations are : Number of merge, number of states mark as dead.
    std::string extra_info_csv();

  protected:

    /// \brief Fix set ups for the algo
    void init();

    /// \brief Push a new state to explore
    void dfs_push(fasttgba_state*);

    /// \brief  Pop states already explored
    void dfs_pop();

    /// \brief the main procedure
    void main();

    /// \brief the update for backedges
    bool dfs_update (fasttgba_state* s);

    /// \brief The color for a new State
    enum color {Alive, Dead, Unknown};

    // An element in Todo stack
    struct pair_state_iter
    {
      const spot::fasttgba_state* state;
      fasttgba_succ_iterator* lasttr;
      long unsigned int position;
    };

    /// \brief the todo stack
    std::vector<pair_state_iter> todo;

    struct stack_entry
    {
      int lowlink;
      int seq_trivial;
      int seq_trivial_sl;
      int out_degree;
    };
    typedef std::vector<stack_entry> dstack_type;
    dstack_type dstack_;


    /// \brief Access  the color of a state
    stats::color  get_color(const fasttgba_state*);

    ///< \brief Storage for counterexample found or not
    bool counterexample_found;

    /// structure to store stats about the product
    struct stats_entry
    {
      int count;		// The number of SCCs
      int ingoing_edge; 	// The number of internal transitions
      int outgoing_edge; 	// The number of external transitions
      int accepting;		// The number of accepting SCC
      int strong;
      int weak;
      int terminal;
    };

    Sgi::hash_map<int, stats_entry> hstats;
    Sgi::hash_map<int, int> histo;

  private:

    ///< \brief the automaton that will be used for the Emptiness check
    const fasttgba* a_;


    std::vector<const spot::fasttgba_state*> live;
    seen_map H;

    /// \brief The store for Deads states
    deadstore* deadstore_;

    /// \brief The instance automaton
    const instance_automaton* inst;

    unsigned int dfs_size_;
    unsigned int max_live_size_; ///< \brief keep peack size
    unsigned int max_dfs_size_;	 ///< \brief keep peack size
    int update_cpt_;		 ///< \brief count UPDATE calls
    int roots_poped_cpt_;	 ///< \brief count UPDATE loop iterations
    int states_cpt_;		 ///< \brief count states
    int transitions_cpt_;	 ///< \brief count transitions

    int strong_states;
    int weak_states;
    int terminal_states;
    int nonaccepting_states;

    int trivial_states;
    int trivial_states_sl;
    int trivial_states_sl_acc;
    int nonaccepting_sccs;
    int seq_trivials;
    int seq_trivials_sl;
    int max_seq_trivials;
    int max_seq_trivials_sl;
    bool last_was_trivial;
    bool iskripke_;
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_STATS_HH
