// -*- coding: utf-8 -*-
// Copyright (C) 2015 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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

#pragma once

#include "twa/waut.hh"
#include "graph/graph.hh"

namespace spot
{
  // -----------------------------------------------------------------
  // Implement a weighted automaton based on digraph
  // -----------------------------------------------------------------

  template<class Graph>
  class SPOT_API w_aut_graph_succ_iterator final:
    public w_aut_succ_iterator
  {
  private:
    typedef typename Graph::transition transition;
    typedef typename Graph::state_data_t state;
    const Graph* g_;
    transition t_;
    transition p_;

  public:
    w_aut_graph_succ_iterator(const Graph* g, transition t)
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

    virtual w_state* current_state() const
    {
      assert(!done());
      return const_cast<w_state*>
	(&g_->state_data(g_->trans_storage(p_).dst));
    }

    transition pos() const
    {
      return p_;
    }
  };

  /// \brief A weighted automaton with a digraph underlying representation
  class SPOT_API w_aut_graph : public w_aut
  {
    typedef digraph<w_state, void> graph_t;

  public:
    w_aut_graph(const spot::digraph<spot::w_state, void>& g,
		spot::acc_cond& ac)
      : g_(g), init_(0), ac_(ac)
      { }

    const w_state* get_init() const
    {
      return &g_.state_data(init_);
    }

    virtual w_aut_succ_iterator*
    succ_iter(const w_state* st) const
    {
      auto idx =  state_number(st);
      return new w_aut_graph_succ_iterator<graph_t>(&g_, g_.states()[idx].succ);
    }

    virtual spot::acc_cond& acceptance() const
    {
      return ac_;
    }

    // Local Implementation for graph underlying structure

    // Facilities to get states
    graph_t::state state_number(const w_state* st) const
    {
      auto s = static_cast<const typename graph_t::state_storage_t*>(st);
      assert(s);
      return s - &g_.state_storage(0);
    }


    // FIXME : is it really needed since in this case we build
    // automaton by hand and thus can declare initial as the first
    // state?
    void set_init(unsigned n)
    {
      init_ = n;
    }

  private:
    const spot::digraph<spot::w_state, void>& g_;
    unsigned init_;
    spot::acc_cond& ac_;	    ///< \brief The acceptance of aut.
  };
}
