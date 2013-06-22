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

// #define TARJAN_SCCTRACE
#ifdef TARJAN_SCCTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif


#include <iostream>
#include "tarjan_scc.hh"
#include <assert.h>

namespace spot
{
  tarjan_scc::tarjan_scc(instanciator* i, std::string option) :
    counterexample_found(false),
    inst (i->new_instance()),
    dfs_size_(0),
    max_live_size_(0),
    max_dfs_size_(0),
    update_cpt_(0),
    roots_poped_cpt_(0),
    states_cpt_(0),
    transitions_cpt_(0)
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

  tarjan_scc::~tarjan_scc()
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
  tarjan_scc::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void tarjan_scc::init()
  {
    trace << "Tarjan_Scc::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    top = dftop = -1;
    violation = false;
    dfs_push(init);
  }

  void tarjan_scc::dfs_push(fasttgba_state* s)
  {
    H[s] = ++top;
    ++dfs_size_;
    ++states_cpt_;

    stack_entry ss = { s, 0, top, dftop};
    trace << "    s.lowlink = " << top << std::endl;
    stack.push_back(ss);
    // Count !
    max_dfs_size_ = max_dfs_size_ > dfs_size_ ?
      max_dfs_size_ : dfs_size_;
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;
    dftop = top;
  }

  void
  tarjan_scc::lowlinkupdate(int f, int t)
  {
    ++update_cpt_;

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
  }

  void
  tarjan_scc::get_color(const fasttgba_state* state,
		       tarjan_scc::color_pair* cp)
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

  void tarjan_scc::dfs_pop()
  {
    --dfs_size_;
    int p = stack[dftop].pre;
    if (stack[dftop].lowlink == dftop)
      {
	++roots_poped_cpt_;

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
    else
      {
	lowlinkupdate(p, dftop);
      }
    dftop = p;
  }

  void tarjan_scc::main()
  {
    tarjan_scc::color_pair cp;
    while (!violation && dftop >= 0)
      {
	++transitions_cpt_;

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

	    get_color(s_prime, &cp);

	    if (cp.c == Unknown)
	      {
		trace << " is a new state." << std::endl;
		dfs_push(s_prime);
	      }
	    else
	      {
		if (cp.c == Alive)
		  {
		    trace << " is on stack." << std::endl;
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
  tarjan_scc::extra_info_csv()
  {
    // dfs max size
    // root max size
    // live max size
    // deadstore max size
    // number of UPDATE calls
    // number of loop inside UPDATE
    // Number of Roots poped
    // visited states
    // visited transitions

    return
      std::to_string(max_dfs_size_)
      + ","
      + std::to_string(0)
      + ","
      + std::to_string(max_live_size_)
      + ","
      + std::to_string(deadstore_? deadstore_->size() : 0)
      + ","
      + std::to_string(update_cpt_)
      + ","
      + std::to_string(0)
      + ","
      + std::to_string(roots_poped_cpt_)
      + ","
      + std::to_string(transitions_cpt_)
      + ","
      + std::to_string(states_cpt_)
      ;
  }
}
