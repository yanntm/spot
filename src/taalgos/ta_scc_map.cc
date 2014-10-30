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
	while (live_stack.size() != todo.size())
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
		state* st = si->current_state();
		int dst_scc = seen[st];

		// Warning! If two transitions go in the same SCC they will
		// be both in scc_reachable!
		if (-dst_scc != sccs_ /*current scc*/)
		  infos[sccs_].scc_reachable->push_back(-dst_scc);
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

  void ta_scc_map::dfs_update(const state* src, const state* dst)
  {
    int ll_src = seen[src];
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

  void ta_scc_map::dump_infos()
  {
    std::cout << "Transitions : " << transitions() << std::endl;
    std::cout << "States : " << states() << std::endl;
    std::cout << "SCCs : " << sccs() << std::endl;

    for (unsigned int i = 0; i < infos.size(); ++i)
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


  // This Can be more efficient because it can be do during the
  // computations of SCCs
  void ta_scc_map::scc_prune_livelock_buchi()
  {
    for (unsigned int i = 0; i < infos.size(); ++i)
      {
	if (infos[i].is_trivial &&
	    infos[i].contains_livelock_acc_state &&
	    infos[i].contains_livelock_acc_state)
	  {
	    infos[i].contains_buchi_acc_state = false;

	    // Now we wan to update the TA

	    // First, find the state
	    seen_map::const_iterator s = seen.begin();
	    while (s != seen.end())
	      {
		// Recall that scc numbers are stored in negated way
		int key = -i;
		if (s->second == key)
		  {
		    const state* state = s->first;
		    const state_ta_explicit* s =
		      dynamic_cast<const state_ta_explicit*> (state);
		    assert(s);

		    // Now we change the acceptance
		    s->set_accepting_state(false);

		    // There is only one state! Can break.
		    break;
		  }
		++s;
	      }
	  }
      }
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
	    if (infos[j].contains_livelock_acc_state)
	      infos[i].can_reach_livelock_acc_state = true;

	    if (infos[j].contains_buchi_acc_state)
	      infos[i].can_reach_buchi_acc_state = true;
	  }
      }
  }

  void ta_build_scc_map(const ta* t)
  {
    ta_scc_map* sccmap = new ta_scc_map(t);
    sccmap->dump_infos();
    delete sccmap;
  }
}
