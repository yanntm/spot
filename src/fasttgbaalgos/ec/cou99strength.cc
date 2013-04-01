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

// #define COU99STRENGTHTRACE
#ifdef COU99STRENGTHTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif


#include <iostream>
#include <assert.h>

#include "cou99strength.hh"

namespace spot
{
  cou99strength::cou99strength(const fasttgba* a,
			       bool allsuccheuristic) :
    counterexample_found(false), a_(a),
    max(0),
    state_cache_(0),
    strength_cache_(UNKNOWN_SCC)
  {
    if (allsuccheuristic)
      dfs_push  = &spot::cou99strength::dfs_push_allsucc_heuristic;
    else
      dfs_push  = &spot::cou99strength::dfs_push_simple_heuristic;
  }

  cou99strength::~cou99strength()
  {
    while (!scc.empty())
      {
    	delete scc.top().get<3>();
    	scc.pop();
      }
    while (!todo.empty())
      {
    	delete todo.top().second;
    	todo.pop();
      }

    seen_map::const_iterator s = H.begin();
    while (s != H.end())
      {
	s->first->destroy();
	++s;
      }
    H.clear();
  }

  bool
  cou99strength::check()
  {
    init();
    if (counterexample_found)
      return counterexample_found;
    main();
    return counterexample_found;
  }

  enum scc_strength
  cou99strength::get_strength (const fasttgba_state* input)
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

  void cou99strength::init()
  {
    trace << "Cou99strength::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();

    // If terminal it's over
    if (get_strength(init) == TERMINAL_SCC)
      {
	init->destroy();
    	counterexample_found = true;
    	return;
      }

    (this->*dfs_push)(markset(a_->get_acc()), init);
  }

  void
  cou99strength::dfs_push_simple_heuristic(markset acc, fasttgba_state* s)
  {
    trace << "Cou99strength::DFS_push_simple_heuristic" << std::endl;

    // If non accepting, no need to go further
    if (get_strength(s) == NON_ACCEPTING_SCC)
      {
	H[s] = 0;
      }
    else
      {
	++max;
	H[s] = max;

	// Push only for Strong SCC
	if (get_strength(s) == STRONG_SCC)
	  {
	    scc.push
	      (boost::make_tuple
	       (max, acc,
		markset(a_->get_acc()),
		(std::list<const fasttgba_state *>*) 0)); /* empty list */
	  }

	else if (get_strength(s) == TERMINAL_SCC)
	  {
	    counterexample_found = true;
	    return;
	  }

      }

    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    todo.push (std::make_pair(s, si));
  }

  void
  cou99strength::dfs_push_allsucc_heuristic(markset acc, fasttgba_state* s)
  {
    trace << "Cou99strength::DFS_push_simple_heuristic" << std::endl;

    // If non accepting, no need to go further
    if (get_strength(s) == NON_ACCEPTING_SCC)
      {
	H[s] = 0;
      }
    else
      {
	++max;
	H[s] = max;

	// Push only for Strong SCC
	if (get_strength(s) == STRONG_SCC)
	  {
	    scc.push
	      (boost::make_tuple
	       (max, acc,
		markset(a_->get_acc()),
		(std::list<const fasttgba_state *>*) 0)); /* empty list */
	  }
      }

    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    // Check if one successor is terminal
    while (!si->done())
      {
    	fasttgba_state* curr = si->current_state();
    	if (get_strength(curr) == TERMINAL_SCC)
    	  {
    	    curr->destroy();
    	    counterexample_found = true;
    	    return;
    	  }
    	curr->destroy();
    	si->next();
      }

    // This line full the cache with the original state:
    // Indeed as curr was destroy and since the cache doesn't
    // clone states to be faster, curr is no longer an available
    // state for cache. This operation solves the problem.
    (void) get_strength(s);

    // Reset the iterator to insert into todo.
    si->first();
    todo.push (std::make_pair(s, si));
  }




  void cou99strength::dfs_pop()
  {
    trace << "Cou99strength::DFS_pop" << std::endl;
    pair_state_iter pair = todo.top();
    delete pair.second;
    todo.pop();

    // It's weak mark as dead
    if (get_strength(pair.first) == WEAK_SCC)
      {
    	H[pair.first] = 0;
      }
    else if (get_strength(pair.first) == STRONG_SCC)
      {
	// insert into rem.
	if (scc.top().get<3>())
	  {
	    scc.top().get<3>()->push_back(pair.first);
	  }
	else
	  {
	    std::list<const fasttgba_state*> *r =
	      new  std::list<const fasttgba_state*>();
	    r->push_back (pair.first);
	    scc.top().get<3>() = r;
	  }

    	if (H[pair.first] == scc.top().get<0>())
    	  {

    	    std::list<const fasttgba_state*>::const_iterator i =
    	      scc.top().get<3>()->begin();
    	    for (; i != scc.top().get<3>()->end(); ++i)
    	      {
    		H[*i] = 0;
    	      }
    	    delete scc.top().get<3>();
    	    scc.pop();
    	  }
      }
  }

  void cou99strength::merge(markset a, int t)
  {
    trace << "Cou99strength::Merge" << std::endl;
    std::list<const fasttgba_state*> *r =
      new  std::list<const fasttgba_state*>();

    while (t < scc.top().get<0>())
      {
	a |= scc.top().get<1>() | scc.top().get<2>();

	if (scc.top().get<3>())	/* Non empty list */
	  {
	    r->insert(r->end(),
		      scc.top().get<3>()->begin(),
		      scc.top().get<3>()->end());
	    delete scc.top().get<3>();
	  }
	scc.pop();
      }

    scc.top().get<2>() |= a;

    if (scc.top().get<3>())
      {
	scc.top().get<3>()->insert(scc.top().get<3>()->end(),
				   r->begin(),
				   r->end());
	delete r;
     }
    else
      {
	scc.top().get<3>() = r;
      }
  }

  void cou99strength::main()
  {
    while (!todo.empty())
      {
	if (todo.top().second->done())
	  {
	    dfs_pop();
	  }
	else
	  {
	    markset a = todo.top().second->current_acceptance_marks();
	    fasttgba_state* d = todo.top().second->current_state();
	    todo.top().second->next();
	    if (H.find(d) == H.end())
	      {
	    	(this->*dfs_push) (a, d);
		if (counterexample_found)
		  return;
		continue;
	      }
	    else if (H[d])
	      {
		if (get_strength(d) == WEAK_SCC)
		  {
		    d->destroy();
		    counterexample_found = true;
		    return;
		  }

	    	merge(a, H[d]);
	    	if (scc.top().get<2>().all())
	    	  {
		    d->destroy();
	    	    counterexample_found = true;
	    	    return;
	    	  }
	      }
	    d->destroy();
	  }
      }
  }
}
