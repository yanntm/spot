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

//#define GV04TRACE
#ifdef GV04TRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif


#include <iostream>
#include "gv04.hh"
#include <assert.h>

namespace spot
{
  gv04::gv04(instanciator* i, std::string option) :
    counterexample_found(false),
    inst (i->new_instance()),
    max_live_size_(0)
  {
    if (!option.compare("-ds"))
      {
	a_ = inst->get_ba_automaton();
	deadstore_ = 0;
      }
    else
      {
	assert(!option.compare("+ds") || !option.compare(""));
	a_ = inst->get_ba_automaton();
	deadstore_ = new deadstore();
      }
    assert(a_->number_of_acceptance_marks() <= 1);
  }

  gv04::~gv04()
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
  gv04::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void gv04::init()
  {
    trace << "Gv04::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    top = dftop = -1;
    violation = false;
    dfs_push(false, init);
  }

  void gv04::dfs_push(bool acc, fasttgba_state* s)
  {
    H[s] = ++top;

    stack_entry ss = { s, 0, top, dftop, 0 };

    if (acc)
      ss.acc = top - 1;	// This differs from GV04 to support TBA.
    else if (dftop >= 0)
      ss.acc = stack[dftop].acc;
    else
      ss.acc = -1;

    trace << "    s.lowlink = " << top << std::endl
	  << "    s.acc = " << ss.acc << std::endl;

    stack.push_back(ss);
    dftop = top;
    max_live_size_ = max_live_size_ > H.size() ?
      max_live_size_ : H.size();
  }

  void
  gv04::lowlinkupdate(int f, int t)
  {
    trace << "  lowlinkupdate(f = " << f << ", t = " << t
	  << ")" << std::endl
	  << "    t.lowlink = " << stack[t].lowlink << std::endl
	  << "    f.lowlink = " << stack[f].lowlink << std::endl
	  << "    f.acc = " << stack[f].acc << std::endl;
    int stack_t_lowlink = stack[t].lowlink;
    if (stack_t_lowlink <= stack[f].lowlink)
      {
	if (stack_t_lowlink <= stack[f].acc)
	  violation = true;
	stack[f].lowlink = stack_t_lowlink;
	trace << "    f.lowlink updated to "
	      << stack[f].lowlink << std::endl;
      }
  }

  void
  gv04::get_color(const fasttgba_state* state,
		  gv04::color_pair* cp)
  {
    seen_map::const_iterator i = H.find(state);
    cp->it = i;
    if (i != H.end())
      {
	if (deadstore_)
	  cp->c = Alive;
	else
	  cp->c = (i->second == -1) ? Dead : Alive;
      }
    else if (deadstore_ && deadstore_->contains(state))
      cp->c = Dead;
    else
      cp->c = Unknown;
  }

  void gv04::dfs_pop()
  {
    int p = stack[dftop].pre;
    if (p >= 0)
      lowlinkupdate(p, dftop);
    if (stack[dftop].lowlink == dftop)
      {
	assert(static_cast<unsigned int>(top + 1) == stack.size());
	for (int i = top; i >= dftop; --i)
	  {
	    delete stack[i].lasttr;
	    if (deadstore_)
	      {
		deadstore_->add(stack[i].s);
		seen_map::const_iterator it = H.find(stack[i].s);
		H.erase(it);
		stack.pop_back();
	      }
	    else
	      {
		H[stack[i].s] = -1;
		stack.pop_back();
	      }
	  }
	top = dftop - 1;
      }
    dftop = p;
  }

  void gv04::main()
  {
    gv04::color_pair cp;
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
	    bool acc = iter->current_acceptance_marks().all();

	    trace << " Next successor: s_prime = "
		  << a_->format_state(s_prime)
		  << (acc ? " (with accepting link)" : "");


	    get_color(s_prime, &cp);

	    if (cp.c == Unknown)
	      {
		trace << " is a new state." << std::endl;
		dfs_push(acc, s_prime);
	      }
	    else
	      {
		if (cp.c == Alive)// i->second < stack.size()
		    // && stack[i->second].s->compare(s_prime) == 0
		    //)
		  {
		    // s_prime has a clone on stack
		    trace << " is on stack." << std::endl;
		    // This is an addition to GV04 to support TBA.
		    violation |= acc;
		    lowlinkupdate(dftop, cp.it->second);
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

  std::string
  gv04::extra_info_csv()
  {
    return  std::to_string(max_live_size_)
      + ","
      + std::to_string(deadstore_? deadstore_->size() : 0);
  }

}
