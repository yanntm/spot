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

// #define LAZYCHECKTRACE
#ifdef LAZYCHECKTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif


#include <iostream>
#include "lazycheck.hh"
#include <assert.h>

namespace spot
{
  lazycheck::lazycheck(instanciator* i, std::string option) :
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
    trivial_scc_(0)
  {
    if (!option.compare("-ds"))
      {
	a_ = inst->get_automaton();
	deadstore_ = 0;
      }
    else
      {
	assert(!option.compare("+ds") || !option.compare(""));
	a_ = inst->get_automaton();
	deadstore_ = new deadstore();
      }
    empty_ = new markset(a_->get_acc());
  }

  lazycheck::~lazycheck()
  {
    for (dstack_type::iterator i = dstack_.begin(); i != dstack_.end(); ++i)
      {
	if (i->mark != empty_)
	  delete i->mark;
      }
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
    delete empty_;
    delete deadstore_;
    delete inst;
  }

  bool
  lazycheck::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void lazycheck::init()
  {
    trace << "Lazycheck::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    dfs_push(init);
  }

  void lazycheck::dfs_push(fasttgba_state* q)
  {
    int position = live.size();
    live.push_back(q);
    H.insert(std::make_pair(q, position));
    dstack_.push_back({position, empty_});
    todo.push_back ({q, 0, live.size() -1});

    ++dfs_size_;
    ++states_cpt_;
    max_dfs_size_ = max_dfs_size_ > dfs_size_ ?
      max_dfs_size_ : dfs_size_;
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    int tmp_cost = 2*dstack_.size() + 2*H.size() +1*live.size()
      + (deadstore_? deadstore_->size() : 0);
    if (tmp_cost > memory_cost_)
      memory_cost_ = tmp_cost;
  }

  lazycheck::color
  lazycheck::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      {
	if (deadstore_)
	  return Alive;
	else
	  return i->second == -1 ? Dead : Alive;
      }
    else if (deadstore_ && deadstore_->contains(state))
      return Dead;
    else
      return Unknown;
  }

  void lazycheck::dfs_pop()
  {
    --dfs_size_;
    int ll = dstack_.back().lowlink;
    markset* acc = dstack_.back().mark;
    dstack_.pop_back();

    unsigned int steppos = todo.back().position;
    delete todo.back().lasttr;
    todo.pop_back();
    assert(dstack_.size() == todo.size());

    if ((int) steppos == ll)
      {
	++roots_poped_cpt_;
	int trivial = 0;
	while (live.size() > steppos)
	  {
	    ++trivial;
	    if (deadstore_)	// There is a deadstore
	      {
		deadstore_->add(live.back());
		seen_map::const_iterator it = H.find(live.back());
		H.erase(it);
		live.pop_back();
	      }
	    else
	      {
		H[live.back()] = -1;
		live.pop_back();
	      }
	  }
	if (trivial == 1) // we just popped a trivial
	  ++trivial_scc_;
      }
    else
      {
	if (ll < dstack_.back().lowlink)
	  dstack_.back().lowlink = ll;

	markset m = (*acc | todo.back().lasttr->current_acceptance_marks());

	// This is an optimisation : When a state that have a non empty
	// markset is backtracted and the predecessor have an empty
	// markset we just transfer the ownership
	bool fastret = false;
	if (!(m == *empty_))
	  {
	    if (dstack_.back().mark == empty_)
	      {
		dstack_.back().mark = acc;
		fastret = true;
	      }
	    dstack_.back().mark->operator|= (m);
	  }
	if (dstack_.back().mark->all())
	  counterexample_found = true;
	if (fastret)
	  return;
      }

    // Delete only if it's not empty.
    if (acc != empty_)
	delete acc;
  }

  bool lazycheck::dfs_update(fasttgba_state* d)
  {
    ++update_cpt_;
    if ( H[d] < dstack_.back().lowlink)
      dstack_.back().lowlink = H[d];

    if (!((todo.back().lasttr->current_acceptance_marks()) == *empty_))
      {
	if (dstack_.back().mark == empty_)
	  dstack_.back().mark = new markset(a_->get_acc());
	dstack_.back().mark->operator|=
	  (todo.back().lasttr->current_acceptance_marks());
      }
    return dstack_.back().mark->all();
  }

  void lazycheck::main()
  {
    lazycheck::color c;
    while (!todo.empty())
      {
	++transitions_cpt_;
	trace << "Main " << std::endl;

	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = a_->succ_iter(todo.back().state);
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
  lazycheck::extra_info_csv()
  {
    // dfs max size
    // root max size
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
      + std::to_string(0)
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
      + std::to_string(trivial_scc_)
      ;
  }
}
