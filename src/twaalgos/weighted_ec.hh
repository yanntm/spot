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
#include <stack>
#include <deque>

//#define TRACE 1
#ifdef TRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

namespace spot
{
  // -----------------------------------------------------------------
  // Emptiness check for weighted automata
  // -----------------------------------------------------------------

  /// \brief This class allows to perform an emptiness check
  /// on weighted graph. This emptiness check is based on
  /// Dijkstra's algorithm so it maintains a root stack.
  class SPOT_API weighted_ec
  {
  public:
    weighted_ec(w_aut& g) :  g_(g)
      { }

    virtual ~weighted_ec()
    {
      auto s = seen_.begin();
      while (s != seen_.end())
      	{
      	  // Advance the iterator before deleting the "key" pointer.
	  const w_state* ptr = s->first;
      	  ++s;
      	  ptr->destroy();
      	}
    }

    /// \brief This method push a new state on the DFS stack
    void push_state(const w_state* st)
    {
      trace << "PUSH " << st->weight() << std::endl;
      assert(seen_.find(st) == seen_.end());
      auto seen_size = seen_.size();
      seen_.insert({st, seen_size});
      roots_.push_back({todo.size(), {st}});
      todo.push_back({st, 0, seen_size});
    }

    /// \brief This method pop the element at the top of the DFS
    /// stack. When the associated element is a root all states
    /// that belong to the same SCC are marked as dead.
    int pop_state()
    {
      int rval = -1;
      auto& dfstop = todo.back();

      // We have detected a root
      if (roots_.back().dfs_number == todo.size()-1)
      	{
	  trace << "Root Detected!" << std::endl;
	  // If we have detected the root of an SCC s.t. |SCC| = 1
	  // We must check wether this state is transient or not!
	  if (roots_.back().same_scc.size() == 1)
	    {
	      dfstop.it->first();
	      while (!dfstop.it->done())
	  	{
	  	  auto ist = dfstop.it->current_state();
	  	  if (!ist->compare(roots_.back().same_scc[0]))
	  	    {
	  	      ist->destroy();
	  	      rval = roots_.back().same_scc[0]->weight();
	  	      break;
	  	    }
		  ist->destroy();
	  	  dfstop.it->next();
	  	}
	    }
	  else
	    {
	      // Otherwise just report the weight of this SCC
	      // FIXME : this assume that an SCC can only contains
	      // state with same weight
	      rval = roots_.back().same_scc[0]->weight();
	    }

	  // Mark all states as dead
	  spot::acc_cond::mark_t scc_acc = 0U;
      	  for (auto st: roots_.back().same_scc)
	    {
	      scc_acc |= st->acc();
	      seen_[st] = -1;
	    }

	  if (!g_.acceptance().accepting(scc_acc))
	    rval = -1;

	  roots_.pop_back();
	}

      // Clean
      delete dfstop.it;
      todo.pop_back();

      // Warning should return the weight of this SCC. This
      // to state with / without selfloops.
      return rval;
    }

    void update(const w_state* src, const w_state* dst)
    {
      trace << "UPDATE(" << src->weight() << ','
	    << dst->weight() << ')' << std::endl;

      int dpos = seen_[dst];
      int rpos = roots_.back().dfs_number;

      while ((unsigned)dpos < todo[rpos].position)
	{
	  int rsize = roots_.size();

	  // Here avoid a copy ...
	  auto& scc = roots_.back().same_scc;
	  // ... so do not pop until all states have been merged
	  roots_[rsize-2].same_scc.insert(roots_[rsize-2].same_scc.end(),
					  scc.begin(), scc.end());
	  roots_.pop_back();
	  rpos = roots_.back().dfs_number;
	}
    }

    /// \brief This method check wether the automaton is empty. If it
    /// is empty, then return -1, otherwise return the weight of the
    /// cycle with the maximum weight
    int check()
    {
      int rval = -1;
      push_state(g_.get_init());
      while (!todo.empty())
	{
	  auto& dfstop = todo.back();
	  if (dfstop.it == 0)
	    {
	      dfstop.it =  g_.succ_iter(dfstop.s);
	      dfstop.it->first();
	    }
	  else
	    {
	      dfstop.it->next();
	    }

	  // This state has been fully explored
	  if (dfstop.it->done())
	    {
	      int scc_weight = pop_state();
	      // If it's a trivial SCC, i.e an SCC composed of a unique
	      // state without selfloop, ignore it. Otherwise pick the
	      // maximum
	      if (scc_weight != -1)
		rval = (rval < scc_weight) ? scc_weight : rval;
	      continue;
	    }
	  else
	    {
	      auto dest = todo.back().it->current_state();
	      auto idx = seen_.find(dest);
	      if (idx == seen_.end())
		{
	    	  push_state (dest);
	    	  continue;
		}
	      else
		{
		  // If the state is not dead then we must consider this
		  // closing-egde
		  if (idx->second != -1)
		      update(dfstop.s, dest);
		}
	      dest->destroy();
	    }
	}
      return rval;
    }

  protected:
    const w_aut& g_;		///< \brief the automaton

    /// \brief Represents one element of the DFS stack
    struct stack_entry
    {
      const w_state* s;	       // State stored in stack entry.
      w_aut_succ_iterator* it; // The iterator for this state
      unsigned long position;
    };

    std::vector<stack_entry> todo; ///< \brief The effective DFS stack

    /// \brief Represents one element of the root stack
    ///
    /// FIXME : Here we should store the acceptance condition
    /// of the partial SCC. Since the exploration do not stop before
    /// visiting the whole SCC this information can be computed when
    /// states are tagged as dead
    struct root_entry
    {
      unsigned long dfs_number;

      // FIXME use a live stack as of nuutila's proposal (and LPAR'14)
      // Will avoid extra operations during merging partial SCCs
      std::vector<const w_state*> same_scc;
    };

    std::vector<root_entry> roots_; ///< \brief The root stack
    w_state_map seen_;              ///< \brief All visited states
  };
}
