// -*- coding: utf-8 -*-
// Copyright (C)  2015, 2017 Laboratoire de Recherche et
// Developpement de l'Epita (LRDE)
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

#include <random>
#include "proviso.hh"

namespace spot
{
  // -----------------------------------------------------------------
  // NO PROVISO
  // -----------------------------------------------------------------
  bool no_proviso::expand_new_state(int, bool, twa_succ_iterator*,
				    seen_map&, const state*,
				    const const_twa_ptr&)
  {
    return false;
  }

  bool no_proviso::expand_src_closingedge(int, int)
  {
    return false;
  }

  bool no_proviso::expand_before_pop(int, twa_succ_iterator*, seen_map&)
  {
    return false;
  }

  no_proviso::~no_proviso()
  { }

  // -----------------------------------------------------------------
  // FIREALL PROVISO
  // -----------------------------------------------------------------
  bool fireall_proviso::expand_new_state(int, bool,
					 twa_succ_iterator*, seen_map&,
					 const state*,
					 const const_twa_ptr&)
  {
    return true;
  }

  bool fireall_proviso::expand_src_closingedge(int, int)
  {
    return false;
  }

  bool fireall_proviso::expand_before_pop(int, twa_succ_iterator*, seen_map&)
  {
    return false;
  }

  fireall_proviso::~fireall_proviso()
  { }

  // -----------------------------------------------------------------
  // STACK PROVISO
  // -----------------------------------------------------------------
  bool stack_proviso::expand_new_state(int n, bool expanded,
				       twa_succ_iterator*, seen_map&,
				       const state*, const const_twa_ptr&)
  {
    on_dfs_.insert(n);
    expanded_.push_back(expanded);
    return false;
  }

  bool stack_proviso::expand_src_closingedge(int, int dst)
  {
    if (expanded_.back() || on_dfs_.find(dst) == on_dfs_.end())
      return false;
    expanded_.back() = true;
    return true;
  }

  bool stack_proviso::expand_before_pop(int n, twa_succ_iterator*, seen_map&)
  {
    on_dfs_.erase(n);
    expanded_.pop_back();
    return false;
  }

  stack_proviso::~stack_proviso()
  { }

  // -----------------------------------------------------------------
  // DESTINATION PROVISO
  // -----------------------------------------------------------------
  bool destination_proviso::expand_new_state(int n, bool expanded,
					     twa_succ_iterator*,
					     seen_map&,
					     const state*,
					     const const_twa_ptr&)
  {
    on_dfs_.insert({n, false});
    expanded_.push_back(expanded);
    return false;
  }

  // Here we detect backegdes to mark the destination as to be expanded
  bool destination_proviso::expand_src_closingedge(int, int dst)
  {
    if (expanded_.back())
      return false;

    // Destination state is not on DFS
    auto idx =  on_dfs_.find(dst);
    if (idx == on_dfs_.end())
      return false;

    // Otherwise, mark it as to be  expanded
    idx->second = true;
    return false;
  }

  bool destination_proviso::expand_before_pop(int n,
					      twa_succ_iterator*, seen_map&)
  {
    if (!expanded_.back() && on_dfs_[n])
      {
	expanded_.back() = true;
	return true;
      }
    on_dfs_.erase(n);
    expanded_.pop_back();
    return false;
  }

  destination_proviso::~destination_proviso()
  { }

  // -----------------------------------------------------------------
  // RND SD PROVISO
  // -----------------------------------------------------------------
  bool rnd_sd_proviso::expand_new_state(int n, bool expanded,
					twa_succ_iterator*,
					seen_map&,
					const state*, const const_twa_ptr&)
  {
    on_dfs_.insert({n, false});
    expanded_.push_back(expanded);
    return false;
  }

  // Here we detect backegdes to mark the rnd_sd as to be expanded
  bool rnd_sd_proviso::expand_src_closingedge(int src, int dst)
  {
    if (expanded_.back())
      return false;

    // .. state is not on DFS
    auto idx =  on_dfs_.find(dst);
    if (idx == on_dfs_.end())
      return false;

    // Otherwise, randomly choose the state to expand.
    static std::mt19937 generator (0);

    if (generator()%2)
      idx->second = true;
    else
      on_dfs_[src] = true;
    return false;
  }

  bool rnd_sd_proviso::expand_before_pop(int n,
					      twa_succ_iterator*, seen_map&)
  {
    if (!expanded_.back() && on_dfs_[n])
      {
	expanded_.back() = true;
	return true;
      }
    on_dfs_.erase(n);
    expanded_.pop_back();
    return false;
  }

  rnd_sd_proviso::~rnd_sd_proviso()
  { }

  // -----------------------------------------------------------------
  // DELAYED PROVISO
  // -----------------------------------------------------------------
  bool delayed_proviso::expand_new_state(int n, bool expanded,
					 twa_succ_iterator*,
					 seen_map&,
					 const state*, const const_twa_ptr&)
  {
    reach_.insert({n, false});
    todo_.push_back(n);
    on_dfs_.insert({n, {expanded, false}});
    return false;
  }

  bool delayed_proviso::expand_src_closingedge(int src, int dst)
  {
    // In this proviso states are not expanded when a closing edge is
    // detected. This method only allows to detect closing edge to update
    // data structures.
    if (on_dfs_.find(dst) != on_dfs_.end() &&
	!on_dfs_[src].expanded && !on_dfs_[dst].expanded)
      {
	on_dfs_[dst].to_be_expanded = true;
	reach_[src] = true;
      }
    else if (reach_[dst] && !on_dfs_[src].expanded)
      {
	reach_[src] = true;
      }
    return false;
  }

  bool delayed_proviso::expand_before_pop(int n, twa_succ_iterator*, seen_map&)
  {
    //Here we must expand the state before popping it.
    auto tmp = on_dfs_[n];
    if (!tmp.expanded && tmp.to_be_expanded && reach_[n])
      {
	reach_[n] = false;
	on_dfs_[n].expanded = true;
	return true;
      }

    // Here it's an effective pop!
    bool nongreen = reach_[n];
    todo_.pop_back();
    on_dfs_.erase(n);
    if (!todo_.empty() && nongreen)
      reach_[todo_.back()] = true;

    // Effective POP no expansion needed
    return false;
  }

  delayed_proviso::~delayed_proviso()
  { }

  // -----------------------------------------------------------------
  // DELAYED+DEAD PROVISO
  // -----------------------------------------------------------------
  bool delayed_proviso_dead::expand_new_state(int n, bool expanded,
					      twa_succ_iterator*,
					      seen_map&,
					      const state*,
					      const const_twa_ptr&)
  {
    reach_.insert({n, {false, false}});
    todo_.push_back(n);
    root_stack_.push_back({n, {n}});
    on_dfs_.insert({n, {expanded, false}});
    return false;
  }

  bool delayed_proviso_dead::expand_src_closingedge(int src, int dst)
  {
    // dst belong to a dead SCC
    if (reach_[dst].dead)
      return false;

    while (dst < root_stack_.back().n)
      {
	auto tmp = root_stack_.back();
	root_stack_.pop_back();
	root_stack_.back().same_scc.insert(root_stack_.back().same_scc.end(),
					   tmp.same_scc.begin(),
					   tmp.same_scc.end());
      }

    // In this proviso states are not expanded when a closing edge is
    // detected. This method only allows to detect closing edge to update
    // data structures.
    if (on_dfs_.find(dst) != on_dfs_.end() &&
	!on_dfs_[src].expanded && !on_dfs_[dst].expanded)
      {
	on_dfs_[dst].to_be_expanded = true;
	reach_[src].nongreen = true;
      }
    else if (reach_[dst].nongreen && !on_dfs_[src].expanded)
      {
	reach_[src].nongreen = true;
      }
    return false;
  }

  bool delayed_proviso_dead::expand_before_pop(int n,
					       twa_succ_iterator*, seen_map&)
  {
    //Here we must expand the state before popping it.
    auto tmp = on_dfs_[n];
    if (!tmp.expanded && tmp.to_be_expanded && reach_[n].nongreen)
      {
	reach_[n].nongreen = false;
	on_dfs_[n].expanded = true;
	return true;
      }

    // Here it's an effective pop!
    bool nongreen = reach_[n].nongreen;
    todo_.pop_back();
    on_dfs_.erase(n);
    if (!todo_.empty() && nongreen)
      reach_[todo_.back()].nongreen = true;

    // We are popping a root
    if (root_stack_.back().n == n)
      {
	for (auto st: root_stack_.back().same_scc)
	  reach_[st].dead = true;
	root_stack_.pop_back();
      }

    // Effective POP no expansion needed
    return false;
  }

  delayed_proviso_dead::~delayed_proviso_dead()
  { }

  // -----------------------------------------------------------------
  // COLOR PROVISO
  // -----------------------------------------------------------------
  color_proviso::color_proviso(): expanded_(0)
  { }

  bool color_proviso::is_cl2l_persistent_set(int n,
					     twa_succ_iterator* it,
					     seen_map& map)
  {
    it->first();
    while (!it->done())
      {
	auto state = it->dst();
	auto idx = map.find(state);
	if (idx != map.end() &&
	    (reach_[idx->second].color == color::Red ||
	     (reach_[idx->second].color == color::Orange &&
	      reach_[idx->second].expanded == reach_[n].expanded)))
	  break;
	it->next();
      }

    auto tmp = !it->done();
    it->first();
    if (tmp)
      return false;
    return true;
  }

  bool color_proviso::expand_new_state(int n,
				       bool expanded,
				       twa_succ_iterator* it,
				       seen_map& map,
				       const state*, const const_twa_ptr&)
  {
    if (expanded || !is_cl2l_persistent_set(n, it, map))
      {
	st_expanded_.push_back(true);
	reach_.insert({n, {true, color::Green, expanded_}});
	++expanded_;
	return !expanded;
      }
    else
      {
	st_expanded_.push_back(false);
	reach_.insert({n, {true, color::Orange, expanded_}});
      }
    return false;
  }

  bool color_proviso::expand_src_closingedge(int src, int dst)
  {
    if (reach_[src].color == color::Orange &&
	reach_[dst].color == color::Red)
      {
	reach_[src].color = color::Green;
	auto tmp = st_expanded_.back();
	st_expanded_.back() = true;
	++expanded_;
	return !tmp; // A state is fired only once!
      }
    return false;
  }

  bool color_proviso::expand_before_pop(int n,
					twa_succ_iterator* it, seen_map& map)
  {
    if (st_expanded_.back())
      --expanded_;

    st_expanded_.pop_back();
    reach_[n].in_stack = false;
    if (reach_[n].color == color::Orange)
      {
	it->first();
	bool isred = false;
	while (!it->done())
	  {
	    auto state = it->dst();
	    auto idx = map.find(state);
	    if (reach_[idx->second].color != color::Green)
	      isred = true;
	    it->next();
	  }
	if (isred)
	  reach_[n].color = color::Red;
	else
	  reach_[n].color = color::Green;
      }
    return false;
  }

  color_proviso::~color_proviso()
  { }

  // -----------------------------------------------------------------
  // COLOR+DEAD PROVISO
  // -----------------------------------------------------------------
  color_proviso_dead::color_proviso_dead(): expanded_(0)
  { }

  bool color_proviso_dead::is_cl2l_persistent_set(int n,
						  twa_succ_iterator* it,
						  seen_map& map)
  {
    it->first();
    while (!it->done())
      {
	auto state = it->dst();
	auto idx = map.find(state);
	// ignore dead states
	if (idx != map.end() && reach_[idx->second].dead)
	  {
	    it->next();
	    continue;
	  }
	if (idx != map.end() &&
	    (reach_[idx->second].color == color::Red ||
	     (reach_[idx->second].color == color::Orange &&
	      reach_[idx->second].expanded == reach_[n].expanded)))
	  break;
	it->next();
      }

    auto tmp = !it->done();
    it->first();
    if (tmp)
      return false;
    return true;
  }

  bool color_proviso_dead::expand_new_state(int n,
					    bool expanded,
					    twa_succ_iterator* it,
					    seen_map& map,
					    const state*, const const_twa_ptr&)
  {
    st_expanded_.push_back(expanded);
    root_stack_.push_back({n, {n}});
    if (expanded || !is_cl2l_persistent_set(n, it, map))
      {
	reach_.insert({n, {true, color::Green, expanded_, false}});
	++expanded_;
	return expanded? false : true;
      }
    else
      reach_.insert({n, {true, color::Orange, expanded_, false}});

    return false;
  }

  bool color_proviso_dead::expand_src_closingedge(int src, int dst)
  {
    // dst belong to a dead SCC
    if (reach_[dst].dead)
      return false;

    while (dst < root_stack_.back().n)
      {
	auto tmp = root_stack_.back();
	root_stack_.pop_back();
	root_stack_.back().same_scc.insert(root_stack_.back().same_scc.end(),
					   tmp.same_scc.begin(),
					   tmp.same_scc.end());
      }

    if (reach_[src].color == color::Orange &&
	reach_[dst].color == color::Red)
      {
	reach_[src].color = color::Green;
	auto tmp = st_expanded_.back();
	st_expanded_.back() = true;
	++expanded_;
	return !tmp; // A state is fired only once!
      }

    return false;
  }

  bool color_proviso_dead::expand_before_pop(int n,
					     twa_succ_iterator* it,
					     seen_map& map)
  {
    if (st_expanded_.back())
      --expanded_;

    st_expanded_.pop_back();
    reach_[n].in_stack = false;
    if (reach_[n].color == color::Orange)
      {
	it->first();
	bool isred = false;
	while (!it->done())
	  {
	    auto state = it->dst();
	    auto idx = map.find(state);

	    // ignore dead states
	    if (reach_[idx->second].dead)
	      {
		it->next();
		continue;
	      }

	    if (reach_[idx->second].color != color::Green)
	      isred = true;
	    it->next();
	  }
	if (isred)
	  reach_[n].color = color::Red;
	else
	  reach_[n].color = color::Green;
      }

    // We are popping a root
    if (root_stack_.back().n == n)
      {
	for (auto st: root_stack_.back().same_scc)
	  reach_[st].dead = true;
	root_stack_.pop_back();
      }

    return false;
  }

  color_proviso_dead::~color_proviso_dead()
  { }

  // -----------------------------------------------------------------
  // MIN SUCC SD PROVISO
  // -----------------------------------------------------------------
  bool min_succ_sd_proviso::expand_new_state(int n, bool expanded,
					     twa_succ_iterator* it,
					     seen_map&, const state*,
					     const const_twa_ptr&)
  {
    // Compute the number of reduced successors
    int nb_succ = 0;
    it->first();
    while (!it->done())
      {
	++nb_succ;
	it->next();
      }
    it->first();

    // Initialize data
    on_dfs_.insert({n, {nb_succ, expanded, false}});
    return false;
  }

  bool min_succ_sd_proviso::expand_src_closingedge(int src, int dst)
  {
    // Already expanded !
    if (on_dfs_[src].expanded || on_dfs_.find(dst) == on_dfs_.end() ||
	on_dfs_[dst].expanded)
      return false;

    // Otherwise, mark it as expanded
    if (on_dfs_[src].nb_succ < on_dfs_[dst].nb_succ)
      on_dfs_[src].to_be_expanded = true;
    else
      on_dfs_[dst].to_be_expanded = true;
    return false;
  }

  bool min_succ_sd_proviso::expand_before_pop(int n,
					      twa_succ_iterator*, seen_map&)
  {
    if (!on_dfs_[n].expanded && on_dfs_[n].to_be_expanded)
      {
	on_dfs_[n].expanded = true;
	return true;
      }

    on_dfs_.erase(n);
    return false;
  }

  min_succ_sd_proviso::~min_succ_sd_proviso()
  { }

  // -----------------------------------------------------------------
  // MAX SUCC SD PROVISO
  // -----------------------------------------------------------------
  bool max_succ_sd_proviso::expand_new_state(int n, bool expanded,
					     twa_succ_iterator* it,
					     seen_map&, const state*,
					     const const_twa_ptr&)
  {
    // Compute the number of reduced successors
    int nb_succ = 0;
    it->first();
    while (!it->done())
      {
	++nb_succ;
	it->next();
      }
    it->first();

    // Initialize data
    on_dfs_.insert({n, {nb_succ, expanded, false}});
    return false;
  }

  bool max_succ_sd_proviso::expand_src_closingedge(int src, int dst)
  {
    // Already expanded !
    if (on_dfs_[src].expanded || on_dfs_.find(dst) == on_dfs_.end() ||
	on_dfs_[dst].expanded)
      return false;

    // Otherwise, mark it as expanded
    if (on_dfs_[src].nb_succ > on_dfs_[dst].nb_succ)
      on_dfs_[src].to_be_expanded = true;
    else
      on_dfs_[dst].to_be_expanded = true;
    return false;
  }

  bool max_succ_sd_proviso::expand_before_pop(int n,
					      twa_succ_iterator*, seen_map&)
  {
    if (!on_dfs_[n].expanded && on_dfs_[n].to_be_expanded)
      {
	on_dfs_[n].expanded = true;
	return true;
      }

    on_dfs_.erase(n);
    return false;
  }

  max_succ_sd_proviso::~max_succ_sd_proviso()
  { }
}
