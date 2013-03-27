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
  cou99_uf::cou99_uf(const fasttgba* a) :
    counterexample_found(false), a_(a),
    uf(new union_find(a->get_acc()))
  {  }

  cou99_uf::~cou99_uf()
  {
    delete uf;
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
    dfs_push_scc(init);
    // dfs_push_classic(init);
  }

  void cou99_uf::dfs_push_classic(fasttgba_state* s)
  {
    trace << "Cou99_Uf::DFS_push "
    	  << std::endl;

    // Dead is inserted by default in the Union Find

    uf->add (s);
    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    todo.push_back (std::make_pair(s, si));
    last = true;
  }

  void cou99_uf::dfs_pop_classic()
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

  void cou99_uf::merge_classic(fasttgba_state* d)
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
  }

  void cou99_uf::dfs_push_scc(fasttgba_state* s)
  {
    trace << "Cou99_Uf::DFS_push "
    	  << std::endl;

    // Dead is inserted by default in the Union Find

    uf->add (s);
    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    todo.push_back (std::make_pair(s, si));
    last = true;
    scc.push (todo.size() -1);
  }

  void cou99_uf::dfs_pop_scc()
  {
    trace << "Cou99_Uf::DFS_pop " << std::endl;
    pair_state_iter pair = todo.back();
    delete pair.second;
    todo.pop_back();


    if (todo.empty() ||
	!uf->same_partition(pair.first, todo.back().first))
      {
	uf->make_dead(pair.first);
	scc.pop();
      }
  }

  void cou99_uf::merge_scc(fasttgba_state* d)
  {
    trace << "Cou99_Uf::merge " << std::endl;

    int i = todo.size() - 1;
    markset a = todo[i].second->current_acceptance_marks();

    while (!uf->same_partition(d, todo[i].first))
      {
 	uf->unite(d, todo[i].first);
	markset m = todo[i].second->current_acceptance_marks();
	a |= m;
	i =  scc.top() - 1;
	scc.pop();
      }
    uf->add_acc(d, a);

    assert(!uf->is_dead(todo.back().first));
    assert(!uf->is_dead(d));
  }

  void cou99_uf::main()
  {
    while (!todo.empty())
      {
	trace << "Main " << std::endl;
	assert(!uf->is_dead(todo.back().first));

	//  Is there any transitions left ?

	if (!last)
	    todo.back().second->next();
	else
	    last = false;

    	if (todo.back().second->done())
    	  {
	    dfs_pop_scc ();
	    // dfs_pop_classic ();
    	  }
    	else
    	  {
    	    fasttgba_state* d = todo.back().second->current_state();
    	    if (!uf->contains(d))
    	      {
		dfs_push_scc (d);
		// dfs_push_classic (d);
    	    	continue;
    	      }
    	    else if (!uf->is_dead(d))
    	      {
		merge_scc (d);
		// merge_classic (d);
    	    	if (uf->get_acc(d).all())
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
}
