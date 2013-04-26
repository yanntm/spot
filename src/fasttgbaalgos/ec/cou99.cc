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
  cou99::cou99(instanciator* i) :
    counterexample_found(false),
    max(0), inst (i->new_instance())
  {
    a_ = inst->get_automaton();
  }

  cou99::~cou99()
  {
    while (!scc.empty())
      {
	delete std::get<3>(scc.top());
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
    delete inst;
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
    fasttgba_state* init = a_->get_init_state();
    dfs_push(markset(a_->get_acc()), init);
  }

  void cou99::dfs_push(markset acc, fasttgba_state* s)
  {
    trace << "Cou99::DFS_push "
	  << a_->format_state(s)
	  << std::endl;
    ++max;
    H[s] = max;
    scc.push
      (std::make_tuple
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
    if (std::get<3>(scc.top()))
      {
	std::get<3>(scc.top())->push_back(pair.first);
      }
    else
      {
	std::list<const fasttgba_state*> *r =
	  new  std::list<const fasttgba_state*>();
	r->push_back (pair.first);
	std::get<3>(scc.top()) = r;
      }


    if (H[pair.first] == std::get<0>(scc.top()))
      {
	std::list<const fasttgba_state*>::const_iterator i =
	  std::get<3>(scc.top())->begin();
	for (; i != std::get<3>(scc.top())->end(); ++i)
	  {
	    H[*i] = 0;
	  }
	delete std::get<3>(scc.top());
	scc.pop();
      }
  }

  void cou99::merge(markset a, int t)
  {
    trace << "Cou99::Merge" << std::endl;
    std::list<const fasttgba_state*> *r =
      new  std::list<const fasttgba_state*>();

    while (t < std::get<0>(scc.top()))
      {
	a |= std::get<1>(scc.top()) | std::get<2>(scc.top());

	if (std::get<3>(scc.top()))	/* Non empty list */
	  {
	    r->insert(r->end(),
		      std::get<3>(scc.top())->begin(),
		      std::get<3>(scc.top())->end());
	    delete std::get<3>(scc.top());
	  }
	scc.pop();
      }

    std::get<2>(scc.top()) |= a;

    if (std::get<3>(scc.top()))
      {
	std::get<3>(scc.top())->insert(std::get<3>(scc.top())->end(),
				       r->begin(),
				       r->end());
	delete r;
     }
    else
      {
	std::get<3>(scc.top()) = r;
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
		continue;
	      }
	    else if (H[d])
	      {
		merge(a, H[d]);
		if (std::get<2>(scc.top()).all())
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
