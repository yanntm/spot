// -*- coding: utf-8 -*-
// Copyright (C) 2008, 2011, 2012, 2013, 2014 Laboratoire de Recherche
// et Développement de l'Epita (LRDE).
// Copyright (C) 2004 Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
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

#include <iostream>
#include "tgba/tgba.hh"
#include "stats.hh"
#include "reachiter.hh"
#include "ltlvisit/tostring.hh"
#include "tgbaalgos/isdet.hh"
#include "tgbaalgos/scc.hh"

namespace spot
{
  namespace
  {
    class stats_bfs: public tgba_reachable_iterator_breadth_first
    {
    public:
      stats_bfs(const tgba* a, tgba_statistics& s)
	: tgba_reachable_iterator_breadth_first(a), s_(s)
      {
      }

      void
      process_state(const state*, int, tgba_succ_iterator*)
      {
	++s_.states;
      }

      void
      process_link(const state*, int, const state*, int,
		   const tgba_succ_iterator*)
      {
	++s_.transitions;
      }

    private:
      tgba_statistics& s_;
    };

    class sub_stats_bfs: public stats_bfs
    {
    public:
      sub_stats_bfs(const tgba* a, tgba_sub_statistics& s)
	: stats_bfs(a, s), s_(s), seen_(bddtrue)
      {
      }

      void
      process_link(const state*, int, const state*, int,
		   const tgba_succ_iterator* it)
      {
	++s_.transitions;

	bdd cond = it->current_condition();
	bdd newvars = bdd_exist(bdd_support(cond), seen_);
	if (newvars != bddtrue)
	  {
	    seen_ &= newvars;
	    int count = 0;
	    while (newvars != bddtrue)
	      {
		++count;
		newvars = bdd_high(newvars);
	      }
	    // If we discover one new variable, that means that all
	    // transitions we counted so far are actually double
	    // subtransitions.  If we have two new variables, they where
	    // quadruple transitions, etc.
	    s_.sub_transitions <<= count;
	  }
	while (cond != bddfalse)
	  {
	    cond -= bdd_satoneset(cond, seen_, bddtrue);
	    ++s_.sub_transitions;
	  }
      }

    protected:
      tgba_sub_statistics& s_;
      bdd seen_;
    };


    class trim_sub_stats_bfs: public sub_stats_bfs
    {
      state_set& ignore_;
    public:
      trim_sub_stats_bfs(const tgba* a, tgba_sub_statistics& s,
			 state_set& ignore)
	: sub_stats_bfs(a, s), ignore_(ignore)
      {
      }

      void
      process_state(const state* s, int, tgba_succ_iterator*)
      {
	if (ignore_.find(s) == ignore_.end())
	  ++s_.states;
      }

      void
      process_link(const state* s, int ns, const state* d, int nd,
		   const tgba_succ_iterator* it)
      {
	if (ignore_.find(s) != ignore_.end())
	  return;
	this->sub_stats_bfs::process_link(s, ns, d, nd, it);
      }
    };

  } // anonymous


  std::ostream& tgba_statistics::dump(std::ostream& out) const
  {
    out << "transitions: " << transitions << std::endl;
    out << "states: " << states << std::endl;
    return out;
  }

  std::ostream& tgba_sub_statistics::dump(std::ostream& out) const
  {
    out << "sub trans.: " << sub_transitions << std::endl;
    this->tgba_statistics::dump(out);
    return out;
  }

  tgba_statistics
  stats_reachable(const tgba* g)
  {
    tgba_statistics s;
    stats_bfs d(g, s);
    d.run();
    return s;
  }

  tgba_sub_statistics
  sub_stats_reachable(const tgba* g)
  {
    tgba_sub_statistics s;
    sub_stats_bfs d(g, s);
    d.run();
    return s;
  }

  tgba_sub_statistics
  sub_stats_prefixtrim(const tgba* g)
  {
    scc_map m(g);
    m.build_map();
    unsigned scc_count = m.scc_count();

    // SCCs are topologically sorted, with
    // scc_count - 1 being the initial SCC.
    assert(m.initial() == scc_count - 1);

    // No prefix to trim?
    if (!m.trivial(scc_count - 1))
      return sub_stats_reachable(g);

    // Assume all SCC have to be trimmed.
    std::vector<bool> to_trim(scc_count, true);

    // All non-trivial SCCs and their descendants must be kept.
    for (unsigned s = scc_count - 1; s > 0; --s)
      {
	if (!m.trivial(s))
	  to_trim[s] = false;
	if (!to_trim[s])
	  {
	    const scc_map::succ_type& succ = m.succ(s);
	    for (scc_map::succ_type::const_iterator j = succ.begin();
		 j != succ.end(); ++j)
	      to_trim[j->first] = false;
	  }
      }
    // It's OK to not process s=0 in the above loop because SCC 0
    // cannot have any successor.
    assert(m.succ(0).empty());
    if (!m.trivial(0))
      to_trim[0] = false;

    state_set to_ignore;
    for (unsigned s = 0; s < scc_count; ++s)
      if (to_trim[s])
	{
	  const std::list<const state*>& l = m.states_of(s);
	  to_ignore.insert(l.begin(), l.end());
	}

    tgba_sub_statistics s;
    trim_sub_stats_bfs d(g, s, to_ignore);
    d.run();
    return s;
  }

  void printable_formula::print(std::ostream& os, const char*) const
  {
    ltl::to_string(val_, os);
  };

  stat_printer::stat_printer(std::ostream& os, const char* format)
    : format_(format)
  {
    declare('a', &acc_);
    declare('c', &scc_);
    declare('d', &deterministic_);
    declare('e', &edges_);
    declare('f', &form_);
    declare('n', &nondetstates_);
    declare('p', &complete_);
    declare('r', &run_time_);
    declare('s', &states_);
    declare('S', &scc_);	// Historical.  Deprecated.  Use %c instead.
    declare('t', &trans_);
    set_output(os);
    prime(format);
  }

  std::ostream&
  stat_printer::print(const tgba* aut,
		      const ltl::formula* f,
		      double run_time)
  {
    form_ = f;
    run_time_ = run_time;

    if (has('t'))
      {
	tgba_sub_statistics s = sub_stats_reachable(aut);
	states_ = s.states;
	edges_ = s.transitions;
	trans_ = s.sub_transitions;
      }
    else if (has('s') || has('e'))
      {
	tgba_sub_statistics s = sub_stats_reachable(aut);
	states_ = s.states;
	edges_ = s.transitions;
      }

    if (has('a'))
      acc_ = aut->number_of_acceptance_conditions();

    if (has('c') || has('S'))
      {
	scc_map m(aut);
	m.build_map();
	scc_ = m.scc_count();
      }

    if (has('n'))
      {
	nondetstates_ = count_nondet_states(aut);
	deterministic_ = (nondetstates_ == 0);
      }
    else if (has('d'))
      {
	// This is more efficient than calling count_nondet_state().
	deterministic_ = is_deterministic(aut);
      }

    if (has('p'))
      {
	complete_ = is_complete(aut);
      }

    return format(format_);
  }

}
