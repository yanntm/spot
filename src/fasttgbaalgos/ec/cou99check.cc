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

// #define COU99CHECKTRACE
#ifdef COU99CHECKTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

#include "fasttgba/fasttgba_product.hh"
#include <iostream>
#include "cou99check.hh"
#include <assert.h>

namespace spot
{
  cou99check::cou99check(instanciator* i, std::string option) :
    counterexample_found(false),
    inst(i->new_instance()),
    max_live_size_(0)
  {
    a_ = inst->get_automaton ();
    if (!option.compare("-cs-ds"))
      {
	deadstore_ = 0;
	roots_stack_ = new stack_of_roots (a_->get_acc());
      }
    else if (!option.compare("-cs+ds"))
      {
	roots_stack_ = new stack_of_roots (a_->get_acc());
	deadstore_ = new deadstore();
      }
    else if (!option.compare("+cs-ds"))
      {
	deadstore_ = 0;
	roots_stack_ = new compressed_stack_of_roots (a_->get_acc());
      }
    else
      {
	assert(!option.compare("+cs+ds") || !option.compare(""));
	roots_stack_ = new compressed_stack_of_roots (a_->get_acc());
	deadstore_ = new deadstore();
      }
  }

  cou99check::~cou99check()
  {
    delete roots_stack_;
    while (!todo.empty())
      {
    	delete todo.back().lasttr;
    	todo.pop_back();
      }

    delete inst;
  }

  bool
  cou99check::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void cou99check::init()
  {
    trace << "Cou99check::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    dfs_push(init);
  }

  void cou99check::dfs_push(fasttgba_state* s)
  {
    trace << "Cou99check::DFS_push "
    	  << std::endl;

    live.push_back(s);
    H[s] = live.size() -1;
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;
    todo.push_back ({s, 0});
    roots_stack_->push_trivial(todo.size() -1);

  }

  void cou99check::dfs_pop()
  {
    trace << "Cou99check::DFS_pop " << std::endl;
    const fasttgba_state* s = todo.back().state;
    delete todo.back().lasttr;
    todo.pop_back();

    unsigned int rtop = roots_stack_->root_of_the_top();
    if (rtop == todo.size())
      {
	roots_stack_->pop();
	fasttgba_succ_iterator* i;
	i = a_->succ_iter(s);


	std::stack<fasttgba_succ_iterator*> kill;
	if (deadstore_)
	  {
	    deadstore_->add(s);
	    seen_map::const_iterator it = H.find(s);
	    H.erase(it);
	  }
	else
	  H[s] = -1;
	live.pop_back();
	kill.push(i);

	while (!kill.empty())
	  {
	    i = kill.top();
	    kill.pop();
	    for(i->first(); !i->done(); i->next())
	      {
		s = i->current_state();
		seen_map::const_iterator it = H.find(s);
		if (it != H.end())
		  {
		    if (it->second != -1)
		      {
			kill.push (a_->succ_iter(s));
			if (deadstore_)
			  {
			    deadstore_->add(s);
			    seen_map::const_iterator it = H.find(s);
			    H.erase(it);
			  }
			else
			  H[s] = -1;
			live.pop_back();
		      }
		  }
	      }
	    delete i;
	  }
      }
  }

  bool cou99check::merge(fasttgba_state* d)
  {
    trace << "Cou99check::merge " << std::endl;

    int dpos = H[d];
    int rpos = roots_stack_->root_of_the_top();
    markset a = todo[rpos].lasttr->current_acceptance_marks();

    roots_stack_->pop();
    while (dpos < H[todo[rpos].state])
      {
    	markset m = todo[rpos].lasttr->current_acceptance_marks();
    	a |= m | roots_stack_->top_acceptance();
    	rpos = roots_stack_->root_of_the_top();
    	roots_stack_->pop();
      }
    roots_stack_->push_non_trivial(rpos, a, todo.size() -1);

    return a.all();
  }

  cou99check::color
  cou99check::get_color(const fasttgba_state* state)
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


  void cou99check::main()
  {
    cou99check::color c;
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
		dfs_push (d);
    	    	continue;
    	      }
    	    else if (c == Alive)
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

  std::string
  cou99check::extra_info_csv()
  {
    return std::to_string(roots_stack_->max_size())
      + ","
      + std::to_string(max_live_size_)
      + ","
      + std::to_string(deadstore_? deadstore_->size() : 0)
      ;
  }
}
