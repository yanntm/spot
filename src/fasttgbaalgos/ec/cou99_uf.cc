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

//#define COU99_UFTRACE
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
  namespace
  {
    // Create a classe that will represent dead states
    class dead_state : public fast_product_state
    {
    public:
      dead_state(): fast_product_state (0,0,0){}

      const fasttgba_state* left() const
      {
	return this;
      }
      const fasttgba_state* right() const
      {
	return this;
      }

      int compare(const fasttgba_state* other) const
      {
	if ((const fast_product_state*)other == this)
	  return 0;
	return -1;
      }

      size_t hash() const
      {
	return 0;
      }

      fasttgba_state* clone() const
      {
	assert(false);
      }

      void* external_information() const
      {
	assert(false);
      }

      virtual void destroy() const
      {
	delete this;
      }
    };
  }


  cou99_uf::cou99_uf(const fasttgba* a) :
    counterexample_found(false), a_(a),
    uf(new union_find(a->get_acc())),
    last(0), DEAD(new dead_state())
  {}

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

    // We consider 0 as a dead state since it cannot
    // be a valid adress for a state
    uf->add (DEAD);

    fasttgba_state* init = a_->get_init_state();
    dfs_push(init);
  }

  void cou99_uf::dfs_push(fasttgba_state* s)
  {
    trace << "Cou99_Uf::DFS_push "
    	  << a_->format_state(s)
    	  << std::endl;

    uf->add (s);
    fasttgba_succ_iterator* si = a_->succ_iter(s);
    si->first();
    todo.push_back (std::make_pair(s, si));
    last = s;
  }

  void cou99_uf::dfs_pop()
  {
    trace << "Cou99_Uf::DFS_pop" << std::endl;
    pair_state_iter pair = todo.back();
    delete pair.second;
    todo.pop_back();

    if (todo.empty() ||
	uf->same_partition(pair.first, 0))
      {
	uf->unite(pair.first, DEAD);
      }
  }


  void cou99_uf::merge(fasttgba_state* d)
  {
    trace << "Cou99_Uf::Merge" << std::endl;
    int i = todo.size() - 1;

    uf->same_partition(todo[i].first, d);
    while (!uf->same_partition(todo[i].first, d))
      {
 	uf->unite(d, todo[i].first);
	markset m = todo[i].second->current_acceptance_marks();
	uf->add_acc(d, m);
	--i;
      }
    markset m = todo[i].second->current_acceptance_marks();
    uf->add_acc(d, m);
  }

  void cou99_uf::main()
  {
    while (!todo.empty())
      {
	//  Is there any transitions left ?
	if (last)
	  last = 0;
	else
	  todo.back().second->next();

    	if (todo.back().second->done())
    	  {
    	    dfs_pop();
    	  }
    	else
    	  {
    	    fasttgba_state* d = todo.back().second->current_state();
	    trace << "Cou99_Uf::DFS_main "
		  << a_->format_state(d)
		  << "contains ? "
		  << uf->contains(d)
		  << std::endl;

    	    if (!uf->contains(d))
    	      {
    	    	dfs_push (d);
    	    	continue;
    	      }
    	    else if (!uf->same_partition(d, DEAD))
    	      {
    	    	merge(d);
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
