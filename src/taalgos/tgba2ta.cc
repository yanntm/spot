// -*- coding: utf-8 -*-
// Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015 Laboratoire de
// Recherche et Développement de l'Epita (LRDE).
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

// #define TRACE

#include <iostream>
#ifdef TRACE
#define trace std::clog
#else
#define trace while (0) std::clog
#endif

#include "ltlast/atomic_prop.hh"
#include "ltlast/constant.hh"
#include "tgba/formula2bdd.hh"
#include <cassert>
#include "ltlvisit/tostring.hh"
#include <iostream>
#include "tgba/bddprint.hh"
#include <stack>
#include "tgba2ta.hh"
#include "taalgos/statessetbuilder.hh"
#include "ta/tgtaexplicit.hh"

using namespace std;

namespace spot
{

  namespace
  {
    typedef std::pair<spot::state*, tgba_succ_iterator*> pair_state_iter;

    class infos
    {
    public :
      const spot::state* tgba_state_;
      bdd dest_condition_;
      bool initial_;
      bool accepting_;
      bool livelock_;

      infos(const spot::state* tgba_state,
	    bdd dest_condition,
	    bool initial,
	    bool accepting,
	    bool livelock = false):
	tgba_state_(tgba_state),
	dest_condition_(dest_condition),
	initial_(initial),
	accepting_(accepting),
	livelock_(livelock)
      { }

      infos(const infos&& other)
      {
	dest_condition_ = other.dest_condition_;
	initial_ = other.initial_;
	accepting_ = other.accepting_;
	livelock_ = other.livelock_;
      }
      infos(const infos& other)
      {
	dest_condition_ = other.dest_condition_;
	initial_ = other.initial_;
	accepting_ = other.accepting_;
	livelock_ = other.livelock_;
      }

    };


    struct infos_equal:
      public std::binary_function<const infos*,
				  const infos*, bool>
    {
      bool
      operator()(const infos* i1, const infos* i2) const
      {
	return i1->tgba_state_ == i2->tgba_state_ &&
	  i1->accepting_ == i2->accepting_ &&
	  i1->livelock_ == i2->livelock_ &&
	  i1->dest_condition_.id() == i2->dest_condition_.id();
      }
    };

    struct infos_hash:
      public std::unary_function<const infos*, size_t>
    {
      size_t
      operator()(const infos* that) const
      {
	return (size_t) that->tgba_state_->hash();
      }
    };


    typedef std::unordered_map<infos*, unsigned,
			       infos_hash, infos_equal> ta_infos;


    static void
    transform_to_single_pass_automaton(const ta_digraph_ptr& testing_automata,
				       bool artificial_livelock_state_mode,
				       bool target_tgta)
    {
      auto& g_ = testing_automata->get_graph();
      auto artificial_livelock_acc_state =
	testing_automata->add_state(testing_automata
				    ->get_tgba()->get_init_state(), bddtrue,
				    false, false, true);

      for (auto& source: g_.states())
      {
	  std::list<unsigned> transitions_to_livelock_states;

	  // Fill the above list.
      	  for (auto& trans: g_.out(source))
      	    {
	      auto dest = const_cast<ta_graph_state*>
		(testing_automata->state_from_number(trans.dst));


      	      // Indicate wether this state has successors.
      	      bool dest_trans_empty = true;
      	      for (auto& tmp: g_.out(trans.dst))
      	      	{
      	      	  (void) tmp;
      	      	  dest_trans_empty = false;
      	      	  break;
      	      	}

      	  	//select transitions where a destination is a livelock state
      	  	// which isn't a Buchi accepting state and has successors
      	  	if (dest->is_livelock_accepting_state()
      	  	    && (!dest->is_accepting_state()) && (!dest_trans_empty))
      	  	  transitions_to_livelock_states.
      	  	    push_back(g_.index_of_transition(trans));

      	  	// optimization to have, after minimization, an unique
      	  	// livelock state which has no successors
      	  	if (dest->is_livelock_accepting_state() && (dest_trans_empty))
		    dest->set_accepting_state(false);
	    }

      	  for (auto& trans_id: transitions_to_livelock_states)
      	    {
      	      ta_graph_trans_data* t = &g_.trans_data(trans_id);
	      auto the_src = testing_automata->state_number(&source.data());

	      // FIXME? Due to the presence of an initial artificial
	      if (the_src == testing_automata->get_initial_artificial_state()
		  && !target_tgta)
		continue;

	      unsigned the_dst =  artificial_livelock_state_mode?
		artificial_livelock_acc_state :
		testing_automata
		->state_from_number(g_.trans_storage(trans_id).dst)
		->stuttering_reachable_livelock;

	      testing_automata->create_transition(the_src, t->cond,
						  t->acc, the_dst);
	    }
       	}

      for (auto& source: g_.states())
	{
	  // Indicate wether this state has successors.
	  bool state_trans_empty = true;
      	  for (auto& tmp: g_.out(source))
      	    {
      	      (void) tmp;
      	      state_trans_empty = false;
      	      break;
      	    }

      	  if (source.data().is_livelock_accepting_state()
      	      && (!source.data().is_accepting_state()) && (!state_trans_empty))
	    source.data().set_livelock_accepting_state(false);
	}
    }

    static void
    compute_livelock_acceptance_states(const ta_digraph_ptr& testing_aut,
				       bool single_pass_emptiness_check,
				       bool artificial_livelock_state_mode,
				       bool target_tgta = false)
    {
      // We use five main data in this algorithm:
      // * sscc: a stack of strongly stuttering-connected components (SSCC)
      scc_stack_ta sscc;

      // * arc, a stack of acceptance conditions between each of these SCC,
      std::stack<acc_cond::mark_t> arc;

      // * h: a hash of all visited nodes, with their order,
      //   (it is called "Hash" in Couvreur's paper)
      typedef std::unordered_map<const state*, int,
				 state_ptr_hash, state_ptr_equal> hash_type;
      hash_type h; ///< Heap of visited states.

      // * num: the number of visited nodes.  Used to set the order of each
      //   visited node,
      int num = 0;

      // * todo: the depth-first search stack.  This holds pairs of the
      //   form (STATE, ITERATOR) where ITERATOR is a tgba_succ_iterator
      //   over the successors of STATE.  In our use, ITERATOR should
      //   always be freed when TODO is popped, but STATE should not because
      //   it is also used as a key in H.
      std::stack<pair_state_iter> todo;

      // * init: the set of the depth-first search initial states
      std::stack<state*> init_set;

      init_set.push(testing_aut->get_artificial_initial_state());

      while (!init_set.empty())
	{
	  // Setup depth-first search from initial states.

	  {
	    ta_graph_state* init =
	      down_cast<ta_graph_state*> (init_set.top());
	    init_set.pop();

	    if (!h.emplace(init, num + 1).second)
	      {
		init->destroy();
		continue;
	      }

	    sscc.push(++num);
	    arc.push(0U);
	    sscc.top().is_accepting
              = testing_aut->is_accepting_state(init);
	    tgba_succ_iterator* iter = testing_aut->succ_iter(init);
	    iter->first();
	    todo.emplace(init, iter);
	  }

	  while (!todo.empty())
	    {
	      state* curr = todo.top().first;

	      auto i = h.find(curr);
	      // If we have reached a dead component, ignore it.
	      if (i != h.end() && i->second == -1)
		{
		  todo.pop();
		  continue;
		}

	      // We are looking at the next successor in SUCC.
	      tgba_succ_iterator* succ = todo.top().second;

	      // If there is no more successor, backtrack.
	      if (succ->done())
		{
		  // We have explored all successors of state CURR.

		  // Backtrack TODO.
		  todo.pop();

		  // fill rem with any component removed,
		  assert(i != h.end());
		  sscc.rem().push_front(curr);

		  // When backtracking the root of an SSCC, we must also
		  // remove that SSCC from the ROOT stacks.  We must
		  // discard from H all reachable states from this SSCC.
		  assert(!sscc.empty());
		  if (sscc.top().index == i->second)
		    {
		      // removing states
		      std::list<state*>::iterator i;
		      bool is_livelock_accepting_sscc = (sscc.rem().size() > 1)
			&& ((sscc.top().is_accepting) ||
			    (testing_aut->acc().
			     accepting(sscc.top().condition)));
		      trace << "*** sscc.size()  = ***" <<  sscc.size() << '\n';
		      for (auto j: sscc.rem())
			{
			  h[j] = -1;

			  if (is_livelock_accepting_sscc)
			    {
			      // if it is an accepting sscc add the state to
			      // G (=the livelock-accepting states set)
			      trace << "*** sscc.size() > 1: states: ***"
				    << testing_aut->format_state(j)
				    << '\n';
			      ta_graph_state* livelock_accepting_state =
				down_cast<ta_graph_state*>(j);

			      livelock_accepting_state->
				set_livelock_accepting_state(true);

			      if (single_pass_emptiness_check)
				{
				  livelock_accepting_state
				    ->set_accepting_state(true);

				  livelock_accepting_state
				    ->stuttering_reachable_livelock
				    = testing_aut->state_number
				    (livelock_accepting_state);
				}
			    }
			}

		      assert(!arc.empty());
		      sscc.pop();
		      arc.pop();

		    }

		  // automata reduction
		  testing_aut->delete_stuttering_and_hole_successors(curr);

		  delete succ;
		  // Do not delete CURR: it is a key in H.
		  continue;
		}

	      // Fetch the values destination state we are interested in...
	      state* dest = succ->current_state();
	      auto acc_cond = succ->current_acceptance_conditions();
	      // ... and point the iterator to the next successor, for
	      // the next iteration.
	      succ->next();
	      // We do not need SUCC from now on.

	      // Are we going to a new state through a stuttering transition?
	      bool is_stuttering_transition =
		testing_aut->get_state_condition(curr)
		== testing_aut->get_state_condition(dest);
	      auto id = h.find(dest);

	      // Is this a new state?
	      if (id == h.end())
		{
		  if (!is_stuttering_transition)
		    {
		      init_set.push(dest);
		      dest->destroy();
		      continue;
		    }

		  // Number it, stack it, and register its successors
		  // for later processing.
		  h[dest] = ++num;
		  sscc.push(num);
		  arc.push(acc_cond);
		  sscc.top().is_accepting =
		    testing_aut->is_accepting_state(dest);

		  tgba_succ_iterator* iter = testing_aut->succ_iter(dest);
		  iter->first();
		  todo.emplace(dest, iter);
		  continue;
		}
	      dest->destroy();

	      // If we have reached a dead component, ignore it.
	      if (id->second == -1)
		continue;

	      trace << "***compute_livelock_acceptance_states: CYCLE***\n";

	      if (!curr->compare(id->first))
		{
		  ta_graph_state* self_loop_state =
		    down_cast<ta_graph_state*> (curr);
		  assert(self_loop_state);

		  if (testing_aut->is_accepting_state(self_loop_state)
		      || (testing_aut->acc().accepting(acc_cond)))
		    {
		      self_loop_state->set_livelock_accepting_state(true);
		      if (single_pass_emptiness_check)
			{
			  self_loop_state->set_accepting_state(true);
			  self_loop_state->stuttering_reachable_livelock
			    =  testing_aut->state_number(self_loop_state);
			}
		    }

		  trace
		    << "***compute_livelock_acceptance_states: CYCLE: "
		    << "self_loop_state***\n";
		}

	      // Now this is the most interesting case.  We have reached a
	      // state S1 which is already part of a non-dead SSCC.  Any such
	      // non-dead SSCC has necessarily been crossed by our path to
	      // this state: there is a state S2 in our path which belongs
	      // to this SSCC too.  We are going to merge all states between
	      // this S1 and S2 into this SSCC.
	      //
	      // This merge is easy to do because the order of the SSCC in
	      // ROOT is ascending: we just have to merge all SSCCs from the
	      // top of ROOT that have an index greater to the one of
	      // the SSCC of S2 (called the "threshold").
	      int threshold = id->second;
	      std::list<state*> rem;
	      bool acc = false;

	      while (threshold < sscc.top().index)
		{
		  assert(!sscc.empty());
		  assert(!arc.empty());
		  acc |= sscc.top().is_accepting;
		  acc_cond |= sscc.top().condition;
		  acc_cond |= arc.top();
		  rem.splice(rem.end(), sscc.rem());
		  sscc.pop();
		  arc.pop();
		}

	      // Note that we do not always have
	      //  threshold == sscc.top().index
	      // after this loop, the SSCC whose index is threshold might have
	      // been merged with a lower SSCC.

	      // Accumulate all acceptance conditions into the merged SSCC.
	      sscc.top().is_accepting |= acc;
	      sscc.top().condition |= acc_cond;

	      sscc.rem().splice(sscc.rem().end(), rem);

	    }
	}

      if (single_pass_emptiness_check)
	{
	  transform_to_single_pass_automaton(testing_aut,
					     artificial_livelock_state_mode,
					     target_tgta);
	}
    }

    ta_digraph_ptr
    build_ta(const ta_digraph_ptr& ta, bdd atomic_propositions_set_,
	     bool degeneralized,
	     bool single_pass_emptiness_check,
	     bool artificial_livelock_state_mode,
	     bool target_tgta = false)
    {
      std::vector<unsigned> todo;
      ta_infos infos_;
      const_tgba_ptr tgba_ = ta->get_tgba();

      // build Initial states set:
      state* tgba_init_state = tgba_->get_init_state();

      bdd tgba_condition = tgba_->support_conditions(tgba_init_state);
      bool is_acc = false;
      if (degeneralized)
	{
	  tgba_succ_iterator* it = tgba_->succ_iter(tgba_init_state);
	  it->first();
	  if (!it->done())
	    is_acc = it->current_acceptance_conditions() != 0U;
	  delete it;
	}

      bdd satone_tgba_condition;
      while ((satone_tgba_condition = bdd_satoneset(tgba_condition,
						    atomic_propositions_set_,
						    bddtrue)) != bddfalse)
	{
	  tgba_condition -= satone_tgba_condition;
	  auto s = ta->add_state(tgba_init_state,
				 satone_tgba_condition,
				 true,
				 is_acc);
	  infos* i = new infos(tgba_init_state,
			       satone_tgba_condition, true, is_acc);
	  infos_.insert({i, s});
	  ta->add_to_initial_states_set(s);
	  todo.push_back(s);
	}
      tgba_init_state->destroy();

      while (!todo.empty())
	{
	  unsigned source = todo.back();
	  todo.pop_back();

	  tgba_succ_iterator* tgba_succ_it =
	    tgba_->succ_iter(ta->state_from_number(source)->get_tgba_state());
	  for (tgba_succ_it->first(); !tgba_succ_it->done();
	       tgba_succ_it->next())
	  {
	      const state* tgba_state = tgba_succ_it->current_state();
	      bdd tgba_condition = tgba_succ_it->current_condition();
	      acc_cond::mark_t tgba_acceptance_conditions =
                tgba_succ_it->current_acceptance_conditions();
	      bdd satone_tgba_condition;
	      while ((satone_tgba_condition =
		      bdd_satoneset(tgba_condition,
				    atomic_propositions_set_, bddtrue))
		     != bddfalse)
		{
		  tgba_condition -= satone_tgba_condition;

		  bdd all_props = bddtrue;
		  bdd dest_condition;

		  bool is_acc = false;
		  if (degeneralized)
		  {
		    tgba_succ_iterator* it = tgba_->succ_iter(tgba_state);
		    it->first();
		    if (!it->done())
		      is_acc = it->current_acceptance_conditions() != 0U;
		    tgba_->release_iter(it);
		  }

		  if (satone_tgba_condition ==
		      ta->state_from_number(source)->get_tgba_condition())
		    while ((dest_condition =
			    bdd_satoneset(all_props,
					  atomic_propositions_set_, bddtrue))
			   != bddfalse)
		      {
			all_props -= dest_condition;

			infos* inf = new infos(tgba_state, dest_condition,
					       false, is_acc);
			auto inf_iter = infos_.find(inf);

			unsigned upos;
			if (inf_iter != infos_.end())
			  {
			    upos = inf_iter->second;
			    delete inf;
			  }
			else
			  {
			    upos = ta->add_state(tgba_state,
						 dest_condition,
						 false, is_acc);
			    infos_.insert({inf, upos});
			    todo.push_back(upos);
			  }
			bdd cs = bdd_setxor(ta->state_from_number(source)
					    ->get_tgba_condition(),
					    ta->state_from_number(upos)
					    ->get_tgba_condition());

			ta->create_transition(source, cs,
					      tgba_acceptance_conditions,
					      upos);
		      }
		}
	    }
	  tgba_->release_iter(tgba_succ_it);
	}

      for (auto key: infos_)
	{
	  delete key.first;
	}

      trace << "*** build_ta: artificial_livelock_acc_state_mode = ***"
	    << artificial_livelock_state_mode << std::endl;

      if (artificial_livelock_state_mode)
	single_pass_emptiness_check = true;

      compute_livelock_acceptance_states(ta, single_pass_emptiness_check,
					 artificial_livelock_state_mode,
					 target_tgta);

      return ta;
    }
  }

  ta_digraph_ptr
  tgba_to_ta(const const_tgba_ptr& tgba_, bdd atomic_propositions_set_,
      bool degeneralized,
      bool single_pass_emptiness_check, bool artificial_livelock_state_mode)
  {
    ta_digraph_ptr ta;
    ta = make_ta_explicit(tgba_, tgba_->acc().num_sets());

    // build ta automaton
    build_ta(ta, atomic_propositions_set_, degeneralized,
	     single_pass_emptiness_check, artificial_livelock_state_mode);

    // (degeneralized=true) => TA
    if (degeneralized)
      {
	return ta;
      }

    // (degeneralized=false) => GTA
    // adapt a GTA to remove acceptance conditions from states
    auto& g_ = ta->get_graph();
    for (unsigned src = 0; src < g_.num_states(); ++src)
    {
      ta_graph_state* src_data  =
        const_cast<ta_graph_state*>(&g_.state_data(src));

      if (src_data->is_accepting_state())
	{
	  for (auto& t: g_.out(src))
	    {
	      t.acc =  ta->acc().all_sets();
	    }
	  src_data->set_accepting_state(false);
	}
    }
    return ta;
  }

  tgta_explicit_ptr
  tgba_to_tgta(const const_tgba_ptr& tgba_, bdd atomic_propositions_set_)
  {
    auto tgta = make_tgta_explicit(tgba_, tgba_->acc().num_sets());

    // build a Generalized TA automaton involving a single_pass_emptiness_check
    // (without an artificial livelock state):
    auto ta = tgta->get_ta();
    build_ta(ta, atomic_propositions_set_, false, true, false, true);

    trace << "***tgba_to_tgbta: POST build_ta***" << std::endl;

    // adapt a ta automata to build tgta automata :
    tgba_succ_iterator* initial_states_iter =
      ta->succ_iter(ta->get_artificial_initial_state());
    initial_states_iter->first();
    if (initial_states_iter->done())
      {
	delete initial_states_iter;
	return tgta;
      }
    bdd first_state_condition = initial_states_iter->current_condition();
    delete initial_states_iter;

    bdd bdd_stutering_transition = bdd_setxor(first_state_condition,
					      first_state_condition);

    auto& g_ = ta->get_graph();
    for (unsigned src = 0; src < g_.num_states(); ++src)
    {
      ta_graph_state* src_data  =
        const_cast<ta_graph_state*>(&g_.state_data(src));

      if (src_data->is_livelock_accepting_state())
	{
	  bool trans_empty = true;
	  for (auto& t: g_.out(src))
	    {
	      (void) t;
	      trans_empty = false;
	      break;
	    }
	  if (trans_empty || src_data->is_accepting_state())
	      ta->create_transition(src, bdd_stutering_transition,
				    ta->acc().all_sets(), src);
	}

      if (src != ta->get_initial_artificial_state())
	{
	  ta->create_transition(src, bdd_stutering_transition, 0U, src);
	}

      src_data->set_livelock_accepting_state(false);
      src_data->set_accepting_state(false);
      trace << "***tgba_to_tgbta: POST create_transition ***" << std::endl;
    }

    return tgta;
  }
}
