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

// #define STATSTRACE
#ifdef STATSTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif


#include <iostream>
#include "ingoing.hh"
#include <assert.h>
#include "fasttgba/fasttgba_explicit.hh"
#include "fasttgba/fasttgba_product.hh"

namespace spot
{
  ingoing::ingoing(instanciator* i, std::string option) :
    inst (i->new_instance())
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
  }

  ingoing::~ingoing()
  {
    while (!todo.empty())
      {
	delete todo.back().lasttr;
	todo.pop_back();
      }

    // Elements will be erase just afeter.
    in_.clear();

    seen_map::const_iterator s = H.begin();
    while (s != H.end())
      {
	// Advance the iterator before deleting the "key" pointer.
	const fasttgba_state* ptr = s->first;
	++s;
	ptr->destroy();
      }
  }

  bool
  ingoing::check()
  {
    init();
    main();
    return false;
  }

  void ingoing::init()
  {
    trace << "ingoing::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    dfs_push(init,0);

    // Always publish init.
    in_[init].to_publish = true;
  }

  void ingoing::dfs_push(fasttgba_state* q, const fasttgba_state* pred)
  {
    int position = live.size();
    live.push_back(q);
    H.insert(std::make_pair(q, position));
    dstack_.push_back({position});
    in_[q] = {false, pred, false};
    todo.push_back ({q, 0, live.size() -1});
  }

  ingoing::color
  ingoing::get_color(const fasttgba_state* state)
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

  void ingoing::dfs_pop()
  {
    int ll = dstack_.back().lowlink;
    dstack_.pop_back();
    unsigned int steppos = todo.back().position;
    const spot::fasttgba_state* state = todo.back().state;

    delete todo.back().lasttr;
    todo.pop_back();

    // It's a rooot
    if ((int) steppos == ll)
      {
	unsigned int lsize = live.size();
	int scc_size = 0;
	while (lsize > steppos)
	  {
	    --lsize;
	    ++scc_size;
	  }

	if (scc_size == 1 && !in_[state].to_publish)
	  {
	    //++trivial_to_publish;
	    in_[state].is_trivial = true;
	    //std::cout << "trivial\n";
	  }

    	while (live.size() > steppos)
	  {
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
      }
  }

  bool ingoing::dfs_update(fasttgba_state* d)
  {
    assert(d);

    if (!in_[d].to_publish &&
	(todo.back().state->compare(in_[d].pred) == 0))
      return false;

    in_[d].to_publish = true;

    // Skip dead state for updating lowlink
    if (get_color(d) == Dead)
      return false;

    if (H[d] < dstack_.back().lowlink)
      dstack_.back().lowlink = H[d];
    return false;
  }

  void ingoing::main()
  {
    ingoing::color c;
    while (!todo.empty())
      {
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
    	  }
    	else
    	  {
	    assert(todo.back().lasttr);
    	    fasttgba_state* d = todo.back().lasttr->current_state();
	    c = get_color (d);
    	    if (c == Unknown)
    	      {
		dfs_push (d, todo[todo.size() - 1].state);
    	    	continue;
    	      }
    	    else
	      // Alive or Dead
    	      {
		dfs_update (d);
    	      }
	    d->destroy();
    	  }
      }
  }

  std::string
  ingoing::extra_info_csv()
  {
    //    std::string dump_stats = "";
    in_map::const_iterator s = in_.begin();
    int how_many = 0;
    int nb_states = 0;
    int trivial_to_publish = 0;
    while (s != in_.end())
      {
	++nb_states;
	if (!s->second.to_publish)
	  {
	    ++how_many;
	    if (s->second.is_trivial)
	      ++trivial_to_publish;
	  }
	++s;
      }
    return std::to_string(how_many) + "/" + std::to_string(nb_states)
      + " (" +  std::to_string(trivial_to_publish) + ")";
  }
}
