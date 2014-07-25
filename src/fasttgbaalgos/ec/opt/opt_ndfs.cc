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

// #define OPT_NDFS_SCCTRACE
#ifdef OPT_NDFS_SCCTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

#include <iostream>
#include "opt_ndfs.hh"
#include <assert.h>

namespace spot
{
  opt_ndfs::opt_ndfs(instanciator* i,
				 std::string option,
				 bool swarm,
				 int tid) :
    counterexample_found(false),
    inst (i->new_instance()),
    dfs_size_(0),
    max_live_size_(0),
    max_dfs_size_(0),
    update_cpt_(0),
    roots_poped_cpt_(0),
    states_cpt_(0),
    transitions_cpt_(0),
    memory_cost_(0),
    trivial_scc_(0),
    swarm_(swarm),
    tid_(tid)
  {
    assert(!option.compare("-cs")
	   || !option.compare("+cs")
	   || !option.compare(""));
    a_ = inst->get_ba_automaton();
    assert(a_->number_of_acceptance_marks() <= 1);
    deadstore_ = new deadstore();
  }

  opt_ndfs::~opt_ndfs()
  {
    while (!todo.empty())
      {
	delete todo.back().lasttr;
	todo.pop_back();
      }
    seen_map::const_iterator s = H.begin();
    while (s != H.end())
      {
	// Advance the iterator before deleting the "key" pointer.
	const fasttgba_state* ptr = s->first;
	++s;
	ptr->destroy();
      }
    delete deadstore_;
    //delete dstack_;
    //delete stack_;
    delete inst;
  }

  bool
  opt_ndfs::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void opt_ndfs::init()
  {
    trace << "Opt_Ndfs::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    dfs_push(init);
  }

  void opt_ndfs::dfs_push(fasttgba_state* q)
  {
    trace << "Push " << std::endl;

    H.insert(std::make_pair(q, Cyan));
    todo.push_back ({q, 0, true});
    ++dfs_size_;
    ++states_cpt_;

    max_dfs_size_ = max_dfs_size_ > dfs_size_ ?
      max_dfs_size_ : dfs_size_;
    // max_live_size_ = H.size() > max_live_size_ ?
    //   H.size() : max_live_size_;

    // int tmp_cost = 1*stack_->size() + 2*H.size() +1*live.size()
    //   + (deadstore_? deadstore_->size() : 0);
    // if (tmp_cost > memory_cost_)
    //   memory_cost_ = tmp_cost;
  }

  opt_ndfs::color
  opt_ndfs::get_color(const fasttgba_state* state)
  {
    trace << "Getcol " << std::endl;
    assert(state);
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return i->second == Red ? Dead : Alive;
    else
      return Unknown;
  }

  void opt_ndfs::dfs_pop()
  {
    trace << "Pop " << std::endl;

    --dfs_size_;
    auto pair = todo.back();
    todo.pop_back();

    if (pair.allred)
      {
    	H[pair.state] = Red;
      }
    else
      if (!todo.empty() &&
	     todo.back().lasttr->current_acceptance_marks().all())
      {
	H[pair.state] = Blue;
	nested_dfs(pair.state);
	H[pair.state] = Red;
      }
    else
      {
	H[pair.state] = Blue;
	if (!todo.empty())
	  {
	    todo.back().allred = false;
	  }
      }
    delete pair.lasttr;
  }

  bool opt_ndfs::dfs_update(fasttgba_state* d)
  {
    trace << "Update " << std::endl;

    ++update_cpt_;
    auto acc =  todo.back().lasttr->current_acceptance_marks();
    if (acc.all())
      {
	if (H[d] == Cyan)
	  {
	    return true;
	  }
	else
	  {
	    nested_dfs(todo.back().state);
	    H[d] = Red;
	    if (counterexample_found)
	      return true;
	  }
      }
    else
      {
	todo.back().allred = false;
      }

    return false;
  }

  void opt_ndfs::nested_dfs(const spot::fasttgba_state* s)
  {
    trace << "Nested " << std::endl;

    std::vector<std::pair<const spot::fasttgba_state*,
			  fasttgba_succ_iterator*>> nested_todo;
    nested_todo.push_back(std::make_pair(s, a_->succ_iter(s)));
    nested_todo.back().second->first();


    while(!nested_todo.empty())
      {
	// All successors have been visited.
    	if (nested_todo.back().second->done())
    	  {
	    delete (nested_todo.back().second);
	    nested_todo.pop_back();
	    continue;
    	  }

	// Grab current state
	assert(nested_todo.back().second);
	auto st = nested_todo.back().second->current_state();

	// Move iterator.
	nested_todo.back().second->next();

	// Check wheter an existing cycle exist.
	if (H[st] == Cyan)
	  {

	    std::cout << "------*\n";

	    counterexample_found = true;
	    return;
	  }
	if (H[st] == Blue)
	  {
	    H[st] = Red;
	    nested_todo.push_back(std::make_pair(st, a_->succ_iter(st)));
	    nested_todo.back().second->first();
	  }
      }
  }

  void opt_ndfs::main()
  {
    opt_ndfs::color c;
    while (!todo.empty())
      {
	trace << "Main " << std::endl;

	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = swarm_ ?
	      a_->swarm_succ_iter(todo.back().state, tid_) :
	      a_->succ_iter(todo.back().state);
	    todo.back().lasttr->first();
	  }
	else
	  {
	    assert(todo.back().lasttr);
	    todo.back().lasttr->next();
	  }

    	if (todo.back().lasttr->done())
    	  {
	    dfs_pop ();
	    if (counterexample_found)
	      return;
    	  }
    	else
    	  {
	    ++transitions_cpt_;
	    assert(todo.back().lasttr);
    	    fasttgba_state* d = todo.back().lasttr->current_state();
	    c = get_color (d);
    	    if (c == Unknown)
    	      {
		dfs_push (d);
    	    	continue;
    	      }
    	    else if (c == Alive)
    	      {
    	    	if (dfs_update (d))
    	    	  {
    	    	    counterexample_found = true;
    	    	    d->destroy();
    	    	    return;
    	    	  }
    	      }
    	    d->destroy();
    	  }
      }
  }

  std::string
  opt_ndfs::extra_info_csv()
  {
    // dfs max size
    // lowlink max size
    // live max size
    // deadstore max size
    // number of UPDATE calls
    // number of loop inside UPDATE
    // Number of Roots poped
    // visited states
    // visited transitions
    // Memory Cost
    // Trivial SCC

    return
      std::to_string(max_dfs_size_)
      + ","
      + std::to_string(0);//stack_->max_size())
      + ","
      + std::to_string(max_live_size_)
      + ","
      + std::to_string(deadstore_? deadstore_->size() : 0)
      + ","
      + std::to_string(update_cpt_)
      + ","
      + std::to_string(0)
      + ","
      + std::to_string(roots_poped_cpt_)
      + ","
      + std::to_string(transitions_cpt_)
      + ","
      + std::to_string(states_cpt_)
      + ","
      + std::to_string(memory_cost_)
      + ","
      + std::to_string(trivial_scc_);
  }
}
