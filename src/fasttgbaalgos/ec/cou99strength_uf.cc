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

// #define COU99STRENGTH_UFTRACE
#ifdef COU99STRENGTH_UFTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif


#include "fasttgba/fasttgba_product.hh"
#include <iostream>
#include "cou99strength_uf.hh"
#include <assert.h>

namespace spot
{
  cou99strength_uf::cou99strength_uf(const fasttgba* a) :
    counterexample_found(false), a_(a),
    uf(new union_find(a->get_acc())),
    state_cache_(0),
    strength_cache_(UNKNOWN_SCC)

  {  }

  cou99strength_uf::~cou99strength_uf()
  {
    delete uf;

    while (!todo.empty())
      {
    	delete todo.back().second;
    	todo.pop_back();
      }
  }

  bool
  cou99strength_uf::check()
  {
    init();
    if (counterexample_found)
      return counterexample_found;
    main();
    return counterexample_found;
  }

  enum scc_strength
  cou99strength_uf::get_strength (const fasttgba_state* input)
  {
    // In the cache
    if (input == state_cache_)
      return strength_cache_;

    // Otherwise we provide a cache
    const fast_product_state* s =
      down_cast<const fast_product_state*>(input);
    //    assert(s);
    const fast_explicit_state* s2 =
      down_cast<const fast_explicit_state*>(s->right());
    // assert(s2);

    state_cache_ = input;
    strength_cache_ = s2->get_strength();

    return strength_cache_;
  }

  void cou99strength_uf::init()
  {
    trace << "Cou99strength_Uf::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();

    // If terminal it's over
    if (get_strength(init) == TERMINAL_SCC)
      {
	init->destroy();
    	counterexample_found = true;
    	return;
      }

    //dfs_push_classic_uf(init);
    dfs_push_classic(init);
  }

  void cou99strength_uf::dfs_push_classic(fasttgba_state* s)
  {
    trace << "Cou99strength_Uf::DFS_push "
	  << a_->format_state(s)
    	  << std::endl;

    // If non accepting, no need to go further
    if (get_strength(s) == NON_ACCEPTING_SCC)
      {
	H[s] = false;
      }
    else  if (get_strength(s) == WEAK_SCC)
      {
	H[s] = true;
      }
    else  if (get_strength(s) == TERMINAL_SCC)
      {
	H[s] = true; // for easy clear
    	counterexample_found = true;
	return;
      }
    else
      {
	uf->add (s);
      }

    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    todo.push_back (std::make_pair(s, si));
    last = true;
  }

  void cou99strength_uf::dfs_pop_classic()
  {
    trace << "Cou99strength_Uf::DFS_pop " << std::endl;
    pair_state_iter pair = todo.back();
    delete pair.second;
    todo.pop_back();

    if ((get_strength(pair.first) == NON_ACCEPTING_SCC) ||
	(get_strength(pair.first) == WEAK_SCC))
      {
	H[pair.first] = false;
	return;
      }

    if (todo.empty() ||
	!uf->same_partition(pair.first, todo.back().first))
      {
	uf->make_dead(pair.first);
      }
  }

  void cou99strength_uf::merge_classic(fasttgba_state* d)
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



  void cou99strength_uf::dfs_push_classic_uf(fasttgba_state* s)
  {
    trace << "Cou99strength_Uf::DFS_push "
	  << a_->format_state(s)
    	  << std::endl;

    // If terminal must Stop
    if (get_strength(s) == TERMINAL_SCC)
      {
	counterexample_found = true;
	return;
      }

    uf->add (s);

    // Non accepting : mark as dead
    if (get_strength(s) == NON_ACCEPTING_SCC)
      {
	uf->make_dead(s);
      }

    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    todo.push_back (std::make_pair(s, si));
    last = true;
  }

  void cou99strength_uf::dfs_pop_classic_uf()
  {
    trace << "Cou99strength_Uf::DFS_pop " << std::endl;
    pair_state_iter pair = todo.back();
    delete pair.second;
    todo.pop_back();

    if (todo.empty() ||
	!uf->same_partition(pair.first, todo.back().first))
      {
	uf->make_dead(pair.first);
      }
  }

  void cou99strength_uf::merge_classic_uf(fasttgba_state* d)
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







  void cou99strength_uf::main()
  {
    while (!todo.empty())
      {
	trace << "Main " << std::endl;

	//  Is there any transitions left ?

	if (!last)
	    todo.back().second->next();
	else
	    last = false;

    	if (todo.back().second->done())
    	  {
	    dfs_pop_classic_uf ();
    	  }
    	else
    	  {
	    fasttgba_state* d = todo.back().second->current_state();
    	    if (!uf->contains(d))
    	      {
		dfs_push_classic_uf (d);
		if (counterexample_found)
		  return;
    	    	continue;
    	      }
    	    else if (!uf->is_dead(d))
    	      {
		merge_classic_uf (d);
    	    	if (uf->get_acc(d).all())
    	    	  {
    	    	    counterexample_found = true;
    	    	    d->destroy();
    	    	    return;
    	    	  }
    	      }
	    d->destroy();




    	    // fasttgba_state* d = todo.back().second->current_state();
	    // bool inuf = uf->contains(d);
	    // bool inh = H.find(d) != H.end();
    	    // if (!inh && !inuf)
    	    //   {
	    // 	dfs_push_classic (d);
	    // 	if (counterexample_found)
	    // 	  return;
    	    // 	continue;
    	    //   }
	    // else if (inh && H[d])
	    //   {
	    // 	counterexample_found = true;
	    // 	return;
	    //   }
    	    // else if (inuf && !uf->is_dead(d))
    	    //   {
	    // 	merge_classic (d);
    	    // 	if (uf->get_acc(d).all())
    	    // 	  {
    	    // 	    counterexample_found = true;
    	    // 	    d->destroy();
    	    // 	    return;
    	    // 	  }
    	    //   }
	    // d->destroy();
    	  }
      }
  }
}
