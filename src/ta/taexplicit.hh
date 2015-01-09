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

#ifndef SPOT_TA_TAEXPLICIT_HH
# define SPOT_TA_TAEXPLICIT_HH

#include <list>
#include <set>
#include <cassert>
#include "misc/bddlt.hh"
#include "ta.hh"
#include "tgba/tgba.hh"
#include "tgba/tgba.hh"
#include "fwd.hh"
#include "graph/graph.hh"
#include "graph/ngraph.hh"
#include "misc/hash.hh"

namespace spot
{
  /// states used by spot::ta_explicit.
  /// \ingroup ta_representation
  class SPOT_API ta_graph_state : public spot::state
  {
#ifndef SWIG
  public:
    /// Explicit transitions.
    struct transition
    {
      bdd condition;
      acc_cond::mark_t acceptance_conditions;
      ta_graph_state* dest;
    };

    typedef std::list<transition*> transitions;

    ta_graph_state(const state* tgba_state,
		   const bdd tgba_condition,
		   bool is_initial_state = false,
		   bool is_accepting_state = false,
		   bool is_livelock_accepting_state = false) :
      tgba_state_(tgba_state),
      tgba_condition_(tgba_condition),
      is_initial_state_(is_initial_state),
      is_accepting_state_(is_accepting_state),
      is_livelock_accepting_state_(is_livelock_accepting_state)
    {
    }

    virtual ~ta_graph_state() noexcept
    {
    }

    virtual int compare(const spot::state* other) const;

    virtual size_t hash() const;

    virtual ta_graph_state* clone() const;

    virtual void destroy() const
    {
    }

    const state*
    get_tgba_state() const;
    const bdd
    get_tgba_condition() const;
    bool
    is_accepting_state() const;
    void
    set_accepting_state(bool is_accepting_state);
    bool
    is_livelock_accepting_state() const;
    void
    set_livelock_accepting_state(bool is_livelock_accepting_state);

    bool
    is_initial_state() const;
    void
    set_initial_state(bool is_initial_state);

    //ta_graph_state*
    unsigned stuttering_reachable_livelock;
  private:
    const state* tgba_state_;
    bdd tgba_condition_;
    bool is_initial_state_;
    bool is_accepting_state_;
    bool is_livelock_accepting_state_;
#endif // !SWIG
  };


  struct SPOT_API ta_graph_trans_data
  {
    bdd cond;
    acc_cond::mark_t acc;

    explicit ta_graph_trans_data()
      : cond(bddfalse), acc(0)
      {
      }

    ta_graph_trans_data(bdd cond, acc_cond::mark_t acc = 0U)
      : cond(cond), acc(acc)
    {
    }
  };

  /// Explicit representation of a spot::ta.
  /// \ingroup ta_representation
  class SPOT_API ta_digraph : public ta
  {
  public:
    typedef digraph<ta_graph_state, ta_graph_trans_data> graph_t;
  protected:
    graph_t g_;
    mutable unsigned init_number_;

  public:
    ta_digraph(const const_tgba_ptr& tgba,
	       unsigned n_acc,
	       bool add_artificial_state);

    const_tgba_ptr
    get_tgba() const;

    graph_t& get_graph()
    {
      return g_;
    }

    const graph_t& get_graph() const
    {
      return g_;
    }

    unsigned
    add_state(const state* tgba_state,
	      const bdd tgba_condition,
	      bool is_initial_state = false,
	      bool is_accepting_state = false,
	      bool is_livelock_accepting_state = false);

    ta_graph_state* add_state(ta_graph_state*)
    {
      assert(false);
      return nullptr;
    }

    int exist_state(const ta_graph_state* newstate)
    {
      assert(newstate);
      for(unsigned i = 1; i < num_states(); ++i)
	{
	  auto st = state_from_number(i);
	  if (newstate->compare(st) == 0)
	    return (int)i;
	}
      return -1;
    }

    void free_transitions(const state* s);

    void
    add_to_initial_states_set(unsigned s, bdd condition = bddfalse);

    void
    create_transition(unsigned source, bdd condition,
		      acc_cond::mark_t acceptance_conditions,
		      unsigned dest);

    // ta interface
    virtual
    ~ta_digraph();
    virtual const states_set_t
    get_initial_states_set() const;

    unsigned num_states() const
    {
      return g_.num_states();
    }

    unsigned num_transitions() const
    {
      return g_.num_transitions();
    }

    void set_init_state(graph_t::state s)
    {
      assert(s < num_states());
      init_number_ = s;
    }

    void set_init_state(const state* s)
    {
      set_init_state(state_number(s));
    }

    virtual state* get_init_state() const
    {
      if (num_states() == 0)
      	assert(false);
      return const_cast<ta_graph_state*>(state_from_number(init_number_));
    }

    graph_t::state
    state_number(const state* st) const
    {
      auto s = down_cast<const typename graph_t::state_storage_t*>(st);
      assert(s);
      return s - &g_.state_storage(0);
    }

    const ta_graph_state*
    state_from_number(graph_t::state n) const
    {
      return &g_.state_data(n);
    }

    bool
    is_hole_state(const state*) const;

    virtual ta_succ_iterator*
    succ_iter(const spot::state* s) const;

    virtual ta_succ_iterator*
    succ_iter(const spot::state* s, bdd condition) const;

    virtual bdd_dict_ptr
    get_dict() const;

    virtual std::string
    format_state(const spot::state* s) const;

    virtual bool
    is_accepting_state(const spot::state* s) const;

    virtual bool
    is_livelock_accepting_state(const spot::state* s) const;

    virtual bool
    is_initial_state(const spot::state* s) const;

    virtual bdd
    get_state_condition(const spot::state* s) const;

    virtual void
    free_state(const spot::state* s) const;

    spot::state*
    get_artificial_initial_state() const
    {
      return const_cast<ta_graph_state*>
	(state_from_number(artificial_initial_state_));
    }

    unsigned
    get_initial_artificial_state() const
    {
      return artificial_initial_state_;
    }

    void
    set_artificial_initial_state(unsigned s)
    {
      artificial_initial_state_ = s;
    }

    virtual void
    delete_stuttering_and_hole_successors(spot::state* s);

    ta::states_set_t
    get_states_set()
    {
      assert(false);
      return states_set_;
    }

  private:
    // Disallow copy.
    ta_digraph(const ta_digraph& other) SPOT_DELETED;
    ta_digraph& operator=(const ta_digraph& other) SPOT_DELETED;

    const_tgba_ptr tgba_;
    unsigned  artificial_initial_state_;
    ta::states_set_t states_set_;
    ta::states_set_t initial_states_set_;
  };

  /// Successor iterators used by spot::ta_explicit.
  template<class Graph>
  class SPOT_API ta_explicit_succ_iterator : public ta_succ_iterator
  {
  private:
    typedef typename Graph::transition transition;
    typedef typename Graph::state_data_t state;
    const Graph* g_;
    transition t_;
    transition p_;

  public:
    ta_explicit_succ_iterator(const Graph* g, transition t)
      : g_(g), t_(t)
    {
    }

    virtual void recycle(transition t)
    {
      t_ = t;
    }

    virtual bool first()
    {
      p_ = t_;
      return p_;
    }

    virtual bool next()
    {
      p_ = g_->trans_storage(p_).next_succ;
      return p_;
    }

    virtual bool done() const
    {
      return !p_;
    }

    virtual ta_graph_state* current_state() const
    {
      assert(!done());
      return const_cast<ta_graph_state*>
	(&g_->state_data(g_->trans_storage(p_).dst));
    }

    virtual bdd current_condition() const
    {
      assert(!done());
      return g_->trans_data(p_).cond;
    }

    virtual acc_cond::mark_t current_acceptance_conditions() const
    {
      assert(!done());
      return g_->trans_data(p_).acc;
    }

    transition pos() const
    {
      return p_;
    }
  };


  template<class Graph>
  class SPOT_API ta_explicit_succ_iterator_cond : public ta_succ_iterator
  {
  private:
    typedef typename Graph::transition transition;
    typedef typename Graph::state_data_t state;
    const Graph* g_;
    transition t_;
    transition p_;
    bdd cond_;

  public:
    ta_explicit_succ_iterator_cond(const Graph* g, transition t, bdd cond)
      : g_(g), t_(t), cond_(cond)
    {
    }

    virtual void recycle(transition t)
    {
      t_ = t;
    }

    virtual bool first()
    {
      p_ = t_;
      select();
      return p_;
    }

    virtual bool next()
    {
      p_ = g_->trans_storage(p_).next_succ;
      select();
      return p_;
    }

    virtual bool done() const
    {
      return !p_;
    }

    virtual ta_graph_state* current_state() const
    {
      assert(!done());
      return const_cast<ta_graph_state*>
	(&g_->state_data(g_->trans_storage(p_).dst));
    }

    virtual bdd current_condition() const
    {
      assert(!done());
      return g_->trans_data(p_).cond;
    }

    virtual acc_cond::mark_t current_acceptance_conditions() const
    {
      assert(!done());
      return g_->trans_data(p_).acc;
    }

    transition pos() const
    {
      return p_;
    }

  private:
    void select()
    {
      while (!done() && current_condition() != cond_)
	{
	  next();
	}
    }
  };

  inline ta_digraph_ptr make_ta_explicit(const const_tgba_ptr& tgba,
					 unsigned n_acc,
					 bool add_artificial_init_state)
  {
    return std::make_shared<ta_digraph>(tgba, n_acc, add_artificial_init_state);
  }
}

#endif // SPOT_TA_TAEXPLICIT_HH
