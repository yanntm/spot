// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement
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

#include "ta/ta.hh"
#include "ta/taexplicit.hh"
#include "ta_scc_map.hh"
#include <iosfwd>

namespace spot
{
  ta_scc_map::ta_scc_map(const ta* t,
			 bool remove_trivial_livelock_buchi_acc):
    t_(t),
    transitions_(0),
    states_(0),
    sccs_(0),
    id_(0)
  {
    build_map();
    if (remove_trivial_livelock_buchi_acc)
      scc_prune_livelock_buchi();
    scc_reach();
  }

  ta_scc_map::~ta_scc_map()
  {
    seen_map::const_iterator s = seen.begin();
    while (s != seen.end())
      {
        // Advance the iterator before deleting the "key" pointer.
        const state* ptr = s->first;
        ++s;
        t_->free_state(ptr);
      }
    for (unsigned int i = 0; i < infos.size(); ++i)
      delete infos[i].scc_reachable;
  }

  void ta_scc_map::dfs_push(const spot::state* st)
  {
    dfs_element dfs_elt = {st, 0};
    todo.push_back(dfs_elt);
    ++id_;
    seen[st] = id_;
    lowlink_stack.push_back(id_);
    ++states_;
    live_stack.push_back(st);
  }

  void ta_scc_map::dfs_pop()
  {
    const state* st = todo.back().st;
    delete todo.back().si;
    todo.pop_back();

    int  ll = lowlink_stack.back();
    lowlink_stack.pop_back();
    if (ll == seen[st])
      {
	// Root !
	scc_info i = {false, false, false, false, true, new std::vector<int>()};
	infos.push_back(i);

	int scc_size = 0;
	std::vector<const state*> in_states;
	while (!live_stack.empty() && seen[live_stack.back()]  >= ll)
	  {
	    ++scc_size;
	    const state* state = live_stack.back();
	    live_stack.pop_back();
	    in_states.push_back(state);

	    // Now a state is associate to its (negated) scc number
	    seen[state] = -sccs_;

	    // Check wheter the state is livelock
	    if (t_->is_livelock_accepting_state(state))
	      infos.back().contains_livelock_acc_state = true;

	    if (t_->is_accepting_state(state))
	      infos.back().contains_buchi_acc_state = true;
	  }

	// Check wether the SCC is trivial
	if (scc_size > 1)
	  infos.back().is_trivial = false;

	// And now compute SCC that can be reached using the
	// set of all states previously computed
	// It's easy because when a root is popped all reachable SCCs have
	// already been computed!
	for (unsigned int i = 0; i < in_states.size(); ++i)
	  {
	    ta_succ_iterator* si = t_->succ_iter(in_states[i]);

	    for (si->first(); !si->done(); si->next())
	      {
		state* tst = si->current_state();
		int dst_scc = seen[tst];
		assert(dst_scc <= 0);

		if (dst_scc != -sccs_ /*current scc*/)
		  {
		    bool toinsert = true;
		    for (unsigned int k = 0;
			 k < infos[sccs_].scc_reachable->size(); ++k)
		      if ((*infos[sccs_].scc_reachable)[k] == -dst_scc)
			toinsert = false;
		    if (toinsert)
		      infos[sccs_].scc_reachable->push_back(-dst_scc);
		  }
		st->destroy();
	      }
	    delete si;
	  }

	// Finally increment the scc number!
	++sccs_;
      }
    else
      {
	lowlink_stack.back() = std::min(ll, lowlink_stack.back());
      }
  }

  void ta_scc_map::dfs_update(const state* /*src*/, const state* dst)
  {
    int ll_src = lowlink_stack.back();
    int ll_dst = seen[dst];
    lowlink_stack.back() = std::min(ll_src, ll_dst);
  }

  void ta_scc_map::build_map()
  {
    // Get the artificial initial state
    spot::state* artificial_initial_state = t_->get_artificial_initial_state();

    // Support only TA with artificial state
    if (artificial_initial_state == 0)
      assert(false);

    dfs_push(artificial_initial_state);

    // Perform the DFS
    while (!todo.empty())
      {
	if (todo.back().si == 0)
	  {
	    todo.back().si = t_->succ_iter(todo.back().st);
	    todo.back().si->first();
	  }
	else
	  {
	    todo.back().si->next();
	  }

	if (todo.back().si->done())
	  {
	    dfs_pop();
	  }
	else
	  {
	    ++transitions_;
	    const state* current = todo.back().si->current_state();
	    seen_map::const_iterator s = seen.find(current);
            if (s == seen.end())
              {
	    	dfs_push(current);
	    	continue;
	      }
	    else
	      {
	    	// Check if alive!
		if (seen[current] > 0)
		  dfs_update(todo.back().st, current);
	      }
	    current->destroy();
	  }
      }
  }

  unsigned ta_scc_map::transitions()
  {
    return transitions_;
  }

  unsigned ta_scc_map::states()
  {
    return states_;
  }

  int ta_scc_map::sccs()
  {
    return sccs_;
  }

  void ta_scc_map::dump_infos(bool verbose)
  {
    std::cout << "Transitions : " << transitions() << std::endl;
    std::cout << "States : " << states() << std::endl;
    std::cout << "SCCs : " << sccs() << std::endl;

    for (unsigned int i = 0; verbose && i < infos.size(); ++i)
      {
    	std::cout << "SCC " << i << ":" <<  std::endl;
    	std::cout << "    contains_livelock_acc_state : "
    		  << infos[i].contains_livelock_acc_state << std::endl;
    	std::cout << "    contains_buchi_acc_state    : "
    		  << infos[i].contains_buchi_acc_state << std::endl;
    	std::cout << "    is_trivial : " << infos[i].is_trivial << std::endl;

    	std::cout << "    can_reach_livelock_acc_state : "
    		  << infos[i].can_reach_livelock_acc_state << std::endl;
    	std::cout << "    can_reach_buchi_acc_state    : "
    		  << infos[i].can_reach_buchi_acc_state << std::endl;

    	std::cout << "    Can reach  : ";
    	for (unsigned int j = 0; j < infos[i].scc_reachable->size(); ++j)
    	  std::cout << (*infos[i].scc_reachable)[j] << "  ";
    	std::cout << std::endl;
      }
  }

  bool ta_scc_map::decomposable()
  {
    for (unsigned int i = 0; i < infos.size(); ++i)
      {
	if (infos[i].can_reach_livelock_acc_state &&
	    !infos[i].can_reach_buchi_acc_state)
	  return true;
	if (!infos[i].can_reach_livelock_acc_state &&
	    infos[i].can_reach_buchi_acc_state)
	  return true;
      }
    return false;
  }

  bool ta_scc_map::can_reach_livelock(const spot::state* s)
  {
    int id = -(seen[s]);
    return infos[id].can_reach_livelock_acc_state;
  }

  bool ta_scc_map::can_reach_buchi(const spot::state* s)
  {
    int id = -(seen[s]);
    return infos[id].can_reach_buchi_acc_state;
  }


  // This Can be more efficient because it can be do during the
  // computations of SCCs.
  void ta_scc_map::scc_prune_livelock_buchi()
  {
    seen_map::const_iterator s = seen.begin();
    while (s != seen.end())
      {
	const state* state = s->first;
	int key = -(s->second);

	// std::cout << t_->format_state(state) << " " << key << std::endl;
    	if (infos[key].is_trivial &&
    	    infos[key].contains_livelock_acc_state &&
    	    infos[key].contains_buchi_acc_state)
	  {
	    const state_ta_explicit* sexpl =
	      dynamic_cast<const state_ta_explicit*> (state);

	    ta_succ_iterator* si = t_->succ_iter(state);
	    bool okay_remove = true;
	    for (si->first(); !si->done(); si->next())
	      {
	    	const spot::state* st = si->current_state();
	    	int dst_scc = -seen[st];
	    	if (dst_scc == key)
	    	  {
	    	    okay_remove = false;
	    	    st->destroy();
	    	    break;
	    	  }
	    	st->destroy();
	      }
	    delete si;

	    if (okay_remove)
	      {
		sexpl->set_accepting_state(false);
		infos[key].contains_buchi_acc_state = false;
	      }
	  }
	++s;
      }
  }

  void ta_scc_map::dump_scc_graph()
  {
    std::cout << "digraph G {" << std::endl;

    // NODES
    for (unsigned int i = 0; i < infos.size(); ++i)
      {
	std::string opt = "";

	if (infos[i].contains_livelock_acc_state)
	  opt += ",shape=box";
	if (infos[i].contains_buchi_acc_state)
	  opt += ",peripheries=2";

	std::cout << i << "[label=\"" << i << "\"" << opt << "]" << std::endl;
      }

    std::cout << "xx [label=\"\", style=invis, height=0]" << std::endl;
    std::cout << "xx -> " <<  infos.size()-1  << std::endl;


    //TRANSITIONS
    for (unsigned int i = 0; i < infos.size(); ++i)
      {
	for (unsigned int j = 0; j < infos[i].scc_reachable->size(); ++j)
	  {
	    std::cout << i << " -> " << (*infos[i].scc_reachable)[j]
		      << std::endl;
	  }
      }
    std::cout << "}" << std::endl;
  }

  void ta_scc_map::scc_reach()
  {
    for (unsigned int i = 0; i < infos.size(); ++i)
      {
	if (infos[i].contains_livelock_acc_state)
	  infos[i].can_reach_livelock_acc_state = true;

	if (infos[i].contains_buchi_acc_state)
	  infos[i].can_reach_buchi_acc_state = true;

	for (unsigned int j = 0; j < infos[i].scc_reachable->size(); ++j)
	  {
	    if (infos[(*infos[i].scc_reachable)[j]]
		.can_reach_livelock_acc_state)
	      infos[i].can_reach_livelock_acc_state = true;

	    if (infos[(*infos[i].scc_reachable)[j]].can_reach_buchi_acc_state)
	      infos[i].can_reach_buchi_acc_state = true;
	  }
      }
  }

  ta_scc_map* ta_build_scc_map(const ta* t, bool b)
  {
    ta_scc_map* sccmap = new ta_scc_map(t, b);
    return  sccmap;
  }

  const ta* ta_build_decomp(const ta* t,
			    ta_scc_map* map,
			    bool livelock,
			    bool buchi)
  {
    // Since we have livelock_aut xor buchi_aut only one of the
    // two boolean can be used to choose what is the kind of automata
    // we build. Let's choose livelock_aut!
    (void) buchi;
    assert((buchi && !livelock) || (!buchi && livelock));

    // Cast arguments to ensure a correct parsing.
    const ta_explicit* t_ = dynamic_cast<const ta_explicit*>(t);
    ta_explicit* ret_ta_ =
      new spot::ta_explicit(t_->get_tgba(), t_->all_acceptance_conditions());

    // Grab initial state ! Only support artificial initial state
    spot::state_ta_explicit* artificial_initial_state =
      dynamic_cast<state_ta_explicit*>
      (t_->get_artificial_initial_state());
    if (artificial_initial_state == 0)
      assert(false);

    // Create an artificial state for the new autoaton.
    spot::state_ta_explicit* ret_art_init =
      new state_ta_explicit
      (t_->get_tgba()->get_init_state()->clone(),
       artificial_initial_state->get_tgba_condition(),
       artificial_initial_state->is_initial_state(),
       livelock? false : artificial_initial_state->is_accepting_state(),
       !livelock? false :
       artificial_initial_state->is_livelock_accepting_state(),
       0);
    ret_ta_->set_artificial_initial_state(ret_art_init);
    ret_ta_->add_state(ret_art_init);

    // Just create the new automaton. To do this a BFS is enough
    std::vector<const state*> todo;
    typedef Sgi::hash_map<const state*, state*, state_ptr_hash,
			  state_ptr_equal>  seen_map2;
    seen_map2 seen;		// Original -- New
    todo.push_back(artificial_initial_state);
    seen[artificial_initial_state] = ret_art_init;

    while (!todo.empty())
      {
	const state* src = todo.back();
	todo.pop_back();
	ta_succ_iterator* si = t_->succ_iter(src);
	si->first();
	while (!si->done())
	  {
	    const state* current = si->current_state();
	    seen_map2::const_iterator s = seen.find(current);
	    spot::state_ta_explicit* dst = 0;

	    bool want_state = false;

	    // Fix want_state according to the automaton we build.
	    if (livelock)
	      want_state = map->can_reach_livelock(current);
	    else
	      want_state = map->can_reach_buchi(current);

	    if (want_state)
	      {
		if (s == seen.end())
		  {
		    const state_ta_explicit* current_ta =
		      dynamic_cast<const state_ta_explicit*>(current);
		    dst =
		      new state_ta_explicit
		      (current_ta->get_tgba_state(),
		       current_ta->get_tgba_condition(),
		       current_ta->is_initial_state(),
		       livelock? false : current_ta->is_accepting_state(),
		       !livelock? false :
		       current_ta->is_livelock_accepting_state(),
		       0);

		    ret_ta_->add_state(dst);
		    todo.push_back(current);
		    seen[current] = dst;
		  }
		else
		  {
		    dst = dynamic_cast<state_ta_explicit*>(s->second);
		  }
		ret_ta_->
		  create_transition(dynamic_cast<state_ta_explicit*>(seen[src]),
				    si->current_condition(),
				    si->current_acceptance_conditions(), dst);
	      }
	    si->next();
	  }
	delete si;
      }
    return ret_ta_;
  }
}
