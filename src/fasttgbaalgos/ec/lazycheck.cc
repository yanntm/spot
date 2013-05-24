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

// #define LAZYCHECKTRACE
#ifdef LAZYCHECKTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif


#include <iostream>
#include "lazycheck.hh"
#include <assert.h>

namespace spot
{
  lazycheck::lazycheck(instanciator* i) :
    counterexample_found(false),
    inst (i->new_instance())
  {
    a_ = inst->get_automaton();
    deadstore_ = new deadstore();
  }

  lazycheck::~lazycheck()
  {
    for (stack_type::iterator i = stack.begin(); i != stack.end(); ++i)
      delete i->lasttr;
    seen_map::const_iterator s = H.begin();
    while (s != H.end())
      {
	// Advance the iterator before deleting the "key" pointer.
	const fasttgba_state* ptr = s->first;
	++s;
	ptr->destroy();
      }
    delete deadstore_;
    delete inst;
  }

  bool
  lazycheck::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void lazycheck::init()
  {
    trace << "Lazycheck::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    top = dftop = -1;
    violation = false;
    dfs_push(init);
  }

  void lazycheck::dfs_push(fasttgba_state* s)
  {
    H[s] = ++top;

    stack_entry ss = { s, 0, top, dftop, new markset(a_->get_acc()) };
    trace << "    s.lowlink = " << top << std::endl;
    stack.push_back(ss);
    dftop = top;
  }

  void
  lazycheck::lowlinkupdate(int f, int t, markset acc)
  {
    trace << "  lowlinkupdate(f = " << f << ", t = " << t
	  << ")" << std::endl
	  << "    t.lowlink = " << stack[t].lowlink << std::endl
	  << "    f.lowlink = " << stack[f].lowlink << std::endl;
    int stack_t_lowlink = stack[t].lowlink;
    if (stack_t_lowlink <= stack[f].lowlink)
      {
	stack[f].lowlink = stack_t_lowlink;
	trace << "    f.lowlink updated to "
	      << stack[f].lowlink << std::endl;
      }
    stack[f].mark->operator |=(acc | *stack[t].mark);
    if (stack[f].mark->all())
      violation = true;
  }

  lazycheck::color
  lazycheck::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (deadstore_->contains(state))
      return Dead;
    return Unknown;
  }

  void lazycheck::dfs_pop()
  {
    int p = stack[dftop].pre;
    if (stack[dftop].lowlink == dftop)
      {
	assert(static_cast<unsigned int>(top + 1) == stack.size());
	for (int i = top; i >= dftop; --i)
	  {
	    delete stack[i].lasttr;
	    delete stack[i].mark;
	    deadstore_->add(stack[i].s);
	    seen_map::const_iterator it = H.find(stack[i].s);
	    H.erase(it);
	    stack.pop_back();
	  }
	top = dftop - 1;
      }
    else
      {
	lowlinkupdate(p, dftop, stack[p].lasttr->current_acceptance_marks());
      }
    dftop = p;
  }

  void lazycheck::main()
  {
    while (!violation && dftop >= 0)
      {
	trace << "Main iteration (top = " << top
	      << ", dftop = " << dftop
	      << ", s = " << a_->format_state(stack[dftop].s)
	      << ")" << std::endl;

	fasttgba_succ_iterator* iter = stack[dftop].lasttr;
	if (!iter)
	  {
	    iter = stack[dftop].lasttr = a_->succ_iter(stack[dftop].s);
	    iter->first();
	  }
	else
	  {
	    iter->next();
	  }

	if (iter->done())
	  {
	    trace << " No more successors" << std::endl;
	    dfs_pop();
	  }
	else
	  {
	    fasttgba_state* s_prime = iter->current_state();

	    trace << " Next successor: s_prime = "
		  << a_->format_state(s_prime)
		  << std::endl;

	    color c = get_color(s_prime);

	    if (c == Unknown)
	      {
		trace << " is a new state." << std::endl;
		dfs_push(s_prime);
	      }
	    else
	      {
		if (c == Alive)
		  {
		    seen_map::const_iterator i = H.find(s_prime);
		    trace << " is on stack." << std::endl;
		    lowlinkupdate(dftop, i->second,
				  iter->current_acceptance_marks());
		  }
		else
		  {
		    trace << " has been seen, but is no longer on stack."
			  << std::endl;
		  }

		s_prime->destroy();
	      }
	  }
      }
    if (violation)
      counterexample_found = true;
  }
}
