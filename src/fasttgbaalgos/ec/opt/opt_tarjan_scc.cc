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

// #define OPT_TARJAN_SCCTRACE
#ifdef OPT_TARJAN_SCCTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

#include <iostream>
#include "opt_tarjan_scc.hh"
#include <assert.h>

namespace spot
{
  opt_tarjan_scc::opt_tarjan_scc(instanciator* i,
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
    a_ = inst->get_automaton();
    deadstore_ = new deadstore();
    if (!option.compare("-cs"))
      {
	stack_ = new generic_stack(a_->get_acc());
      }
    else
      {
	stack_ = new compressed_generic_stack(a_->get_acc());
      }
  }

  opt_tarjan_scc::~opt_tarjan_scc()
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
    delete stack_;
    delete inst;
  }

  bool
  opt_tarjan_scc::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void opt_tarjan_scc::init()
  {
    trace << "Opt_Tarjan_Scc::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    dfs_push(init);
  }

  void opt_tarjan_scc::dfs_push(fasttgba_state* q)
  {
    int position = H.size();
    H.insert(std::make_pair(q, position));
    stack_->push_transient(position);
    todo.push_back ({q, 0, H.size() -1});

    ++dfs_size_;
    ++states_cpt_;
    max_dfs_size_ = max_dfs_size_ > dfs_size_ ?
      max_dfs_size_ : dfs_size_;
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    int tmp_cost = 1*stack_->size() + 2*H.size() +1*live.size()
      + (deadstore_? deadstore_->size() : 0);
    if (tmp_cost > memory_cost_)
      memory_cost_ = tmp_cost;
  }

  opt_tarjan_scc::color
  opt_tarjan_scc::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (deadstore_->contains(state))
      return Dead;
    else
      return Unknown;
  }

  void opt_tarjan_scc::dfs_pop()
  {
    --dfs_size_;
    auto top = stack_->pop(todo.back().position);

    auto pair = todo.back();
    delete pair.lasttr;
    todo.pop_back();

    if (pair.position == (unsigned) top.pos)
      {
	++roots_poped_cpt_;
	int trivial = 0;

	// Delete the root that is not inside of live Stack
	deadstore_->add(pair.state);
	seen_map::const_iterator it1 = H.find(pair.state);
	H.erase(it1);
	while (H.size() > pair.position)
	  {
	    ++trivial;
	    deadstore_->add(live.back());
	    seen_map::const_iterator it = H.find(live.back());
	    H.erase(it);
	    live.pop_back();
	  }

	// This change regarding original algorithm
	if (trivial == 0)
	  ++trivial_scc_;
      }
    else
      {
	auto newtop = stack_->pop(todo.back().position);

	if (top.pos <= newtop.pos)
	  stack_->push_non_transient(top.pos, newtop.acc);
	else
	  stack_->push_non_transient(newtop.pos, newtop.acc);

	live.push_back(pair.state);
      }
  }

  void opt_tarjan_ec::dfs_pop()
  {
    --dfs_size_;
    auto top = stack_->pop(todo.back().position);

    auto pair = todo.back();
    delete pair.lasttr;
    todo.pop_back();

    if (pair.position == (unsigned) top.pos)
      {
	++roots_poped_cpt_;
	int trivial = 0;

	// Delete the root that is not inside of live Stack
	deadstore_->add(pair.state);
	seen_map::const_iterator it1 = H.find(pair.state);
	H.erase(it1);
	while (H.size() > pair.position)
	  {
	    ++trivial;
	    deadstore_->add(live.back());
	    seen_map::const_iterator it = H.find(live.back());
	    H.erase(it);
	    live.pop_back();
	  }

	// This change regarding original algorithm
	if (trivial == 0)
	  ++trivial_scc_;
      }
    else
      {
	auto newtop = stack_->pop(todo.back().position);
	newtop.acc |= top.acc | todo.back().lasttr->current_acceptance_marks();

	if (top.pos <= newtop.pos)
	  stack_->push_non_transient(top.pos, newtop.acc);
	else
	  stack_->push_non_transient(newtop.pos, newtop.acc);

	live.push_back(pair.state);
	if (newtop.acc.all())
	  counterexample_found = true;
      }
  }

  bool opt_tarjan_scc::dfs_update(fasttgba_state* d)
  {
    ++update_cpt_;
    auto top = stack_->pop(todo.back().position);

    if (H[d] <= (int) top.pos)
      stack_->push_non_transient(H[d], top.acc/*empty*/);
    else
      stack_->push_non_transient(top.pos, top.acc/*empty*/);

    return false;
  }

  bool opt_tarjan_ec::dfs_update(fasttgba_state* d)
  {
    ++update_cpt_;
    auto top = stack_->pop(todo.back().position);
    top.acc |= todo.back().lasttr->current_acceptance_marks();

    if (H[d] <= (int) top.pos)
      stack_->push_non_transient(H[d], top.acc);
    else
      stack_->push_non_transient(top.pos, top.acc);

    return top.acc.all();
  }

  void opt_tarjan_scc::main()
  {
    opt_tarjan_scc::color c;
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
  opt_tarjan_scc::extra_info_csv()
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
      + std::to_string(stack_->max_size())
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
