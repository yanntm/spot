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

// #define COU99_UFTRACE
#ifdef COU99_UFTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

#include "fasttgba/fasttgba_product.hh"
#include <iostream>
#include "cou99_uf.hh"
#include <assert.h>

namespace spot
{
  cou99_uf::cou99_uf(instanciator* i) :
    counterexample_found(false),
    inst(i->new_instance())
  {
    a_ = inst->get_automaton ();
    uf  = new SetOfDisjointSetsIPC_LRPC_MS (a_->get_acc());
    roots_stack_ = new stack_of_roots (a_->get_acc());
  }

  cou99_uf::~cou99_uf()
  {
    delete roots_stack_;
    delete uf;
    while (!todo.empty())
      {
    	delete todo.back().second;
    	todo.pop_back();
      }

    delete inst;
  }

  bool
  cou99_uf::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void cou99_uf::init()
  {
    trace << "Cou99_Uf::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    dfs_push(init);
  }

  void cou99_uf::dfs_push(fasttgba_state* s)
  {
    trace << "Cou99_Uf::DFS_push "
    	  << std::endl;

    uf->add (s);
    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    todo.push_back (std::make_pair(s, si));
    last = true;
    roots_stack_->push_trivial(todo.size()-1);
  }

  void cou99_uf::dfs_pop()
  {
    trace << "Cou99_Uf::DFS_pop " << std::endl;
    pair_state_iter pair = todo.back();
    delete pair.second;
    todo.pop_back();

    if (todo.size() == roots_stack_->root_of_the_top())
      {
	uf->make_dead(pair.first);
	roots_stack_->pop();
      }
  }

  bool cou99_uf::merge(fasttgba_state* d)
  {
    trace << "Cou99_Uf::merge " << std::endl;

    int i = roots_stack_->root_of_the_top();
    markset a = todo[i].second->current_acceptance_marks();
    roots_stack_->pop();

    while (!uf->same_partition(d, todo[i].first))
      {
 	uf->unite(d, todo[i].first);
	markset m = todo[i].second->current_acceptance_marks();
	a |= m | roots_stack_->top_acceptance();
	i = roots_stack_->root_of_the_top();
	roots_stack_->pop();
      }
    roots_stack_->push_non_trivial(i, a, todo.size() -1);

    return a.all();
  }

  void cou99_uf::main()
  {
    while (!todo.empty())
      {
	trace << "Main " << std::endl;
	assert(!uf->is_dead(todo.back().first));

	// Is there any transitions left?
	if (!last)
	    todo.back().second->next();
	else
	    last = false;

    	if (todo.back().second->done())
    	  {
	    dfs_pop ();
    	  }
    	else
    	  {
    	    fasttgba_state* d = todo.back().second->current_state();
    	    if (!uf->contains(d))
    	      {
		dfs_push (d);
    	    	continue;
    	      }
    	    else if (!uf->is_dead(d))
    	      {
    	    	if (merge (d))
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

  void cou99_uf_original::dfs_push(fasttgba_state* s)
  {
    trace << "Cou99_Uf::DFS_push "
    	  << std::endl;

    uf->add (s);
    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    todo.push_back (std::make_pair(s, si));
    last = true;
  }

  void cou99_uf_original::dfs_pop()
  {
    trace << "Cou99_Uf::DFS_pop " << std::endl;
    pair_state_iter pair = todo.back();
    delete pair.second;
    todo.pop_back();

    if (todo.empty() ||
	!uf->same_partition(pair.first, todo.back().first))
      {
	uf->make_dead(pair.first);
      }
  }

  bool cou99_uf_original::merge(fasttgba_state* d)
  {
    int i = todo.size() - 1;
    markset a (a_->get_acc());

    while (!uf->same_partition(d, todo[i].first))
      {
	int ref = i;
	while (uf->same_partition(todo[ref].first, todo[i].first))
	 --ref;

 	uf->unite(d, todo[i].first);
	markset m = todo[i].second->current_acceptance_marks();
	a |= m;
	i = ref;
      }

    markset m = todo[i].second->current_acceptance_marks();
    uf->add_acc(d, m|a);

    assert(!uf->is_dead(todo.back().first));
    assert(!uf->is_dead(d));
    return uf->get_acc(d).all();
  }

}
