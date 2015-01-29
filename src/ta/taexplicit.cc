// -*- coding: utf-8 -*-
// Copyright (C) 2010, 2011, 2012, 2013, 2014 Laboratoire de Recherche
// et Développement de l'Epita (LRDE).
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

//#define TRACE

#include <iostream>
#ifdef TRACE
#define trace std::clog
#else
#define trace while (0) std::clog
#endif

#include "ltlast/atomic_prop.hh"
#include "ltlast/constant.hh"
#include "taexplicit.hh"
#include "tgba/formula2bdd.hh"
#include <cassert>
#include "ltlvisit/tostring.hh"

#include "tgba/bddprint.hh"

namespace spot
{
  const state*
  ta_graph_state::get_tgba_state() const
  {
    return tgba_state_;
  }

  const bdd
  ta_graph_state::get_tgba_condition() const
  {
    return tgba_condition_;
  }

  bool
  ta_graph_state::is_accepting_state() const
  {
    return is_accepting_state_;
  }

  bool
  ta_graph_state::is_initial_state() const
  {
    return is_initial_state_;
  }

  void
  ta_graph_state::set_accepting_state(bool is_accepting_state)
  {
    is_accepting_state_ = is_accepting_state;
  }

  bool
  ta_graph_state::is_livelock_accepting_state() const
  {
    return is_livelock_accepting_state_;
  }

  void
  ta_graph_state::set_livelock_accepting_state(
      bool is_livelock_accepting_state)
  {
    is_livelock_accepting_state_ = is_livelock_accepting_state;
  }

  void
  ta_graph_state::set_initial_state(bool is_initial_state)
  {
    is_initial_state_ = is_initial_state;
  }

  int
  ta_graph_state::compare(const spot::state* other) const
  {
    const ta_graph_state* o = down_cast<const ta_graph_state*>(other);
    assert(o);
    int compare_value = tgba_state_->compare(o->tgba_state_);

    if (compare_value != 0)
      return compare_value;

    compare_value = tgba_condition_.id() - o->tgba_condition_.id();
    return compare_value;
  }

  size_t
  ta_graph_state::hash() const
  {
    return wang32_hash(tgba_state_->hash()) ^ wang32_hash(tgba_condition_.id());
  }

  ta_graph_state*
  ta_graph_state::clone() const
  {
    return new ta_graph_state(*this);
  }

  ////////////////////////////////////////
  // ta_explicit

  ta_digraph::ta_digraph(const const_tgba_ptr& tgba, unsigned n_acc):
    ta(tgba->get_dict()),
    init_number_(0),
    tgba_(tgba),
    artificial_initial_state_(0)
  {
    iter_cache_ = nullptr;
    get_dict()->register_all_variables_of(&tgba_, this);
    acc().add_sets(n_acc);
    assert(tgba_->get_init_state() != 0);
    artificial_initial_state_ =
      add_state(tgba_->get_init_state(), bddfalse, true);
  }

  ta_digraph::~ta_digraph()
  {
    ta::states_set_t::iterator it;
    for (it = states_set_.begin(); it != states_set_.end(); ++it)
      {
        ta_graph_state* s = down_cast<ta_graph_state*>(*it);

        free_transitions(s);
        s->get_tgba_state()->destroy();
        delete s;
      }
    get_dict()->unregister_all_my_variables(this);
  }

  unsigned
  ta_digraph::add_state(const state* tgba_state,
			const bdd tgba_condition,
			bool is_initial_state,
			bool is_accepting_state,
			bool is_livelock_accepting_state)
  {
    return g_.new_state(tgba_state->clone(),
			tgba_condition,
			is_initial_state,
			is_accepting_state,
			is_livelock_accepting_state);
  }

  void
  ta_digraph::add_to_initial_states_set(unsigned state, bdd condition)
  {
    if (condition == bddfalse)
      condition = get_state_condition(state_from_number(state));
    create_transition(artificial_initial_state_, condition, 0U, state);
  }

  bool
  ta_digraph::is_hole_state(const state* s) const
  {
    auto it = succ_iter(s);
    it->first();
    if (!it->done())
      return false;
    return true;
  }

  const ta::states_set_t
  ta_digraph::get_initial_states_set() const
  {
    return initial_states_set_;
  }

  bdd
  ta_digraph::get_state_condition(const spot::state* initial_state) const
  {
    const ta_graph_state* sta =
        down_cast<const ta_graph_state*>(initial_state);
    assert(sta);
    return sta->get_tgba_condition();
  }

  bool
  ta_digraph::is_accepting_state(const spot::state* s) const
  {
    const ta_graph_state* sta = down_cast<const ta_graph_state*>(s);
    assert(sta);
    return sta->is_accepting_state();
  }

  bool
  ta_digraph::is_initial_state(const spot::state* s) const
  {
    const ta_graph_state* sta = down_cast<const ta_graph_state*>(s);
    assert(sta);
    return sta->is_initial_state();
  }

  bool
  ta_digraph::is_livelock_accepting_state(const spot::state* s) const
  {
    const ta_graph_state* sta = down_cast<const ta_graph_state*>(s);
    assert(sta);
    return sta->is_livelock_accepting_state();
  }

  ta_succ_iterator*
  ta_digraph::succ_iter(const spot::state* st) const
  {
    {
      auto s = down_cast<const typename graph_t::state_storage_t*>(st);
      assert(s);
      assert(!s->succ || g_.valid_trans(s->succ));

      if (this->iter_cache_)
      	{
      	  auto it =
      	    down_cast<ta_explicit_succ_iterator<graph_t>*>(this->iter_cache_);
      	  it->recycle(s->succ);
      	  this->iter_cache_ = nullptr;
      	  return it;
      	}
      return new ta_explicit_succ_iterator<graph_t>(&g_, s->succ);
    }
  }

  ta_succ_iterator*
  ta_digraph::succ_iter(const spot::state* st, bdd cond) const
  {
    {
      auto s = down_cast<const typename graph_t::state_storage_t*>(st);
      assert(s);
      assert(!s->succ || g_.valid_trans(s->succ));

      if (this->iter_cache_)
	{
	  auto it = down_cast<ta_explicit_succ_iterator_cond<graph_t>*>
	    (this->iter_cache_);
	  it->recycle(s->succ);
	  this->iter_cache_ = nullptr;
	  return it;
	}
      return new ta_explicit_succ_iterator_cond<graph_t>(&g_, s->succ, cond);
    }
  }

  bdd_dict_ptr
  ta_digraph::get_dict() const
  {
    return tgba_->get_dict();
  }

  const_tgba_ptr
  ta_digraph::get_tgba() const
  {
    return tgba_;
  }

  std::string
  ta_digraph::format_state(const spot::state* s) const
  {
    const ta_graph_state* sta = down_cast<const ta_graph_state*>(s);
    assert(sta);

    if (sta->get_tgba_condition() == bddtrue)
      return tgba_->format_state(sta->get_tgba_state());

    return tgba_->format_state(sta->get_tgba_state()) + "\n"
        + bdd_format_formula(get_dict(), sta->get_tgba_condition());
  }

  void
  ta_digraph::free_transitions(const state* s)
  {
    auto t = g_.out_iteraser(state_number(s));
    while (t)
      {
    	t.erase();
      }
    g_.remove_dead_transitions_();
  }

  void
  ta_digraph::delete_stuttering_and_hole_successors(spot::state* s)
  {
    unsigned src = state_number(s);
    ta_graph_state* src_data = &g_.state_data(src);
    auto src_cond = src_data->get_tgba_condition();
    auto t = g_.out_iteraser(src);
    while (t)
      {
	ta_graph_state* dst_data = &g_.state_data(t->dst);
	auto dst_cond = dst_data->get_tgba_condition();
	bool is_stuttering_transition = (src_cond == dst_cond);
	bool dest_is_livelock_accepting =
	  dst_data->is_livelock_accepting_state();

	if (is_stuttering_transition)
	  {
	    if (!src_data->is_livelock_accepting_state() &&
		dest_is_livelock_accepting)
	      {
		src_data->set_livelock_accepting_state(true);
		src_data->stuttering_reachable_livelock =
		  dst_data->stuttering_reachable_livelock;
	      }
	    if (dst_data->is_initial_state())
	      {
		src_data->set_initial_state(true);
		add_to_initial_states_set(src);
	      }
	  }

	bool is_hole = true;
	for (auto tmp: g_.out(t->dst))
	  {
	    is_hole = false;
	    break;
	  }

	if (is_stuttering_transition ||
	    (is_hole && !dest_is_livelock_accepting))
	  {
	    t.erase();
	    continue;
	  }
	++t;
      }
  }

  void
  ta_digraph::create_transition(unsigned src,
				bdd condition,
				acc_cond::mark_t acceptance_conditions,
				unsigned dst)
  {
    bool transition_found = false;
    for (auto& t: g_.out(src))
    {
      // Restrict to transitions with the same condition
      if (condition != t.cond)
	continue;

      if (t.dst == dst)
	{
	  t.acc |= acceptance_conditions;
	  transition_found = true;
	}
    }

    if (!transition_found)
      g_.new_transition(src, dst, condition, acceptance_conditions);
  }

  void
  ta_digraph::free_state(const spot::state*) const
  {
  }

}
