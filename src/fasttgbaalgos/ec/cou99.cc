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

//#define COU99TRACE
#ifdef COU99TRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif


#include <iostream>
#include "cou99.hh"
#include <assert.h>

namespace spot
{
  cou99::cou99(const fasttgba* a) :
    counterexample_found(false), a_(a),
    max(0)
  {}

  cou99::~cou99()
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

    std::map<const fasttgba_state*, int>::const_iterator i;
    for (i = H.begin(); i != H.end(); ++i)
      {
    	i->first->destroy();
      }
    H.clear();
  }

  bool
  cou99::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void cou99::init()
  {
    trace << "Cou99::Init" << std::endl;
    dfs_push(markset(a_->get_acc()),
	     a_->get_init_state());

  }

  void cou99::dfs_push(markset acc, fasttgba_state* s)
  {
    trace << "Cou99::DFS_push" << std::endl;
    ++max;
    s->clone();
    H[s] = max;
    scc.push
      (boost::make_tuple
       (max, acc,
	markset(a_->get_acc()),
	(std::list<const fasttgba_state *>*) 0)); /* empty list */
    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    todo.push (std::make_pair(s, si));
  }

  void cou99::dfs_pop()
  {
    trace << "Cou99::DFS_pop" << std::endl;
    pair_state_iter pair = todo.top();
    delete pair.second;
    todo.pop();


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

  void cou99::merge(markset a, int t)
  {
    trace << "Cou99::Merge" << std::endl;
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

  void cou99::main()
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
	    	dfs_push (a, d);
	      }
	    else if (H[d])
	      {
	    	merge(a, H[d]);
	    	if (scc.top().get<2>().any())
	    	  {
	    	    counterexample_found = true;
	    	    return;
	    	  }
	      }
	  }

      }
  }
}
