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
#include "stats.hh"
#include <assert.h>
#include "fasttgba/fasttgba_explicit.hh"

namespace spot
{
  stats::stats(instanciator* i, std::string option) :
    counterexample_found(false),
    inst (i->new_instance()),
    dfs_size_(0),
    max_live_size_(0),
    max_dfs_size_(0),
    update_cpt_(0),
    roots_poped_cpt_(0),
    states_cpt_(0),
    transitions_cpt_(0),
    strong_states(0),
    weak_states(0),
    terminal_states(0)
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

  stats::~stats()
  {
    while (!todo.empty())
      {
	delete todo.back().lasttr;
	todo.pop_back();
      }
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
  stats::check()
  {
    init();
    main();
    return counterexample_found;
  }

  void stats::init()
  {
    trace << "Stats::Init" << std::endl;
    fasttgba_state* init = a_->get_init_state();
    dfs_push(init);
  }

  void stats::dfs_push(fasttgba_state* q)
  {
    int position = live.size();
    live.push_back(q);
    H.insert(std::make_pair(q, position));
    dstack_.push_back({position});
    todo.push_back ({q, 0, live.size() -1});

    ++dfs_size_;
    ++states_cpt_;
    max_dfs_size_ = max_dfs_size_ > dfs_size_ ?
      max_dfs_size_ : dfs_size_;
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    if (auto t = dynamic_cast<fast_explicit_state*>(q))
      {
	if (t->get_strength() == STRONG_SCC)
	  ++strong_states;
	if (t->get_strength() == WEAK_SCC)
	  ++weak_states;
	if (t->get_strength() == TERMINAL_SCC)
	  ++terminal_states;
      }

  }

  stats::color
  stats::get_color(const fasttgba_state* state)
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

  void stats::dfs_pop()
  {
    --dfs_size_;
    int ll = dstack_.back().lowlink;
    dstack_.pop_back();

    unsigned int steppos = todo.back().position;
    delete todo.back().lasttr;
    todo.pop_back();
    assert(dstack_.size() == todo.size());

    if ((int) steppos == ll)
      {
	++roots_poped_cpt_;

	// Here compute stats about scc
	unsigned int lsize = live.size();
	int inttrans = 0;
	int outtrans = 0;
	int scc_size = 0;
	markset m (a_->get_acc());
	while (lsize > steppos)
	  {
	    --lsize;
	    ++scc_size;
	    fasttgba_succ_iterator* it = a_->succ_iter(live[lsize]);
	    it->first();
	    while (!it->done())
	      {
		fasttgba_state* d = it->current_state();
		if (get_color(d) == Alive)
		  {
		    ++inttrans;
		    m |= it->current_acceptance_marks();
		  }
		else
		  ++outtrans;
		d->destroy();
		it->next();
	      }
	    delete it;
	  }

	// Update Results
	if (hstats.find(scc_size) == hstats.end())
	  {
	    hstats[scc_size] = {1, inttrans, outtrans, m.all() ? 1 : 0};
	  }
	else
	  {
	    ++hstats[scc_size].count;
	    hstats[scc_size].ingoing_edge += inttrans;
	    hstats[scc_size].outgoing_edge += outtrans;
	    if (m.all())
	      ++hstats[scc_size].accepting;
	  }

	while (live.size() > steppos)
	  {
	    ++scc_size;
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
    else
      {
	if (ll < dstack_.back().lowlink)
	  dstack_.back().lowlink = ll;
      }
  }

  bool stats::dfs_update(fasttgba_state* d)
  {
    ++update_cpt_;
    if (H[d] < dstack_.back().lowlink)
      dstack_.back().lowlink = H[d];
    return false;
  }

  void stats::main()
  {
    stats::color c;
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
	    if (counterexample_found)
	      return;
    	  }
    	else
    	  {
	    ++transitions_cpt_;
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
    	    	if (dfs_update (d))
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
  stats::extra_info_csv()
  {
    // dfs max size
    // root max size
    // live max size
    // deadstore max size
    // number of UPDATE calls
    // number of loop inside UPDATE
    // Number of Roots poped
    // visited transitions
    // visited states

    std::string dump_stats = "";
    for (auto i = hstats.begin(); i != hstats.end(); ++i)
      {
	if (i != hstats.begin())
	  dump_stats += ",";
	dump_stats += "<" + std::to_string(i->first)
	  + "," + std::to_string(i->second.count)
	  + "," + std::to_string(i->second.ingoing_edge)
	  + "," + std::to_string(i->second.outgoing_edge)
	  + "," + std::to_string(i->second.accepting)
	  + ">";
      }

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
      + ","
      + std::to_string(strong_states)
      + ","
      + std::to_string(weak_states)
      + ","
      + std::to_string(terminal_states)
      + ","
      + dump_stats;
  }
}
