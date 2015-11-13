// -*- coding: utf-8 -*-
// Copyright (C)  2015, 2017 Laboratoire de Recherche et
// Developpement de l'Epita (LRDE)
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

#include <spot/twaalgos/reachiter.hh>
#include <spot/twaalgos/stats.hh>
#include <spot/ltsmin/proviso.hh>
#include <random>


namespace spot
{
  /// \brief This class implements a simple DFS. This DFS is more complicated
  /// than the average since it allows to express many options that are useful
  /// for provisos.
  template<bool Anticipated, bool FullyAnticipated>
  class SPOT_API dfs_stats: public dfs_inspector
  {
  public:
    dfs_stats(const const_twa_ptr& a, proviso& proviso):
      aut_(a), proviso_(proviso)
    { }

    void push_state(const state* st)
    {
      ++states_;
      todo.push_back({st, aut_->succ_iter(st)});
      todo.back().it->first();

      max_dfssize_ = max_dfssize_ > todo.size()?
	max_dfssize_ : todo.size();

      if (todo.back().it->all_enabled())
	++already_expanded_;

      cumul_red_ += todo.back().it->reduced();
      cumul_en_ += todo.back().it->enabled();

      proviso_.notify_push(st, *this);

      if (Anticipated || FullyAnticipated)
	{
	  // FIXME bypass static by something faster than std::function
	  static seen_map* seen_ptr;
	  seen_ptr = &seen;
	  todo.back().it->
	    reorder_remaining([](const state* s)
			      {
				return seen_ptr->find(s) != seen_ptr->end();
			      });
	}
    }

    void pop_state()
    {
      if (!proviso_.before_pop(todo.back().src, *this))
	return;

      seen[todo.back().src].dfs_position = -1;
      aut_->release_iter(todo.back().it);
      todo.pop_back();

      if (!todo.empty() && FullyAnticipated)
	{
	  // FIXME bypass static by something faster than std::function
	  static seen_map* seen_ptr;
	  seen_ptr = &seen;
	  todo.back().it->
	    reorder_remaining([](const state* s)
			      {
				return seen_ptr->find(s) != seen_ptr->end();
			      });
	}
    }

    void run()
    {
      const state* initial =  aut_->get_init_state();
      seen[initial] = {++dfs_number, (int) todo.size(), {}};
      push_state(initial);

      while (!todo.empty())
      	{
       	  if (todo.back().it->done())
      	    {
	      pop_state();
      	    }
	  else
	    {
	      ++transitions_;
	      const state* dst = todo.back().it->dst();
	      todo.back().it->next();
	      state_info info = {dfs_number+1, (int)todo.size(), {}};
	      auto res = seen.emplace(dst, info);
	      if (res.second)
		{
		  // it's a new state
		  ++dfs_number;
		  push_state(dst);
		}
	      else
		{
		  // This may be a closing edge
		  int toexpand = proviso_.maybe_closingedge(todo.back().src,
							    dst, *this);

		  // Count the number of backedges
		  if (dfs_position(dst) != -1)
		    ++backedges_;

		  // ... and an expansion is needed.
		  if (toexpand != -1)
		    {
		      assert(toexpand < (int) todo.size());
		      todo[toexpand].it->fire_all();
		      ++expanded_;
		    }

		  dst->destroy();
		}
	    }
	}
    }

    std::string dump()
    {
      return
	" states           : " + std::to_string(states_)           + '\n' +
	" transitions      : " + std::to_string(transitions_)      + '\n' +
	" already_expanded : " + std::to_string(already_expanded_) + '\n' +
	" expanded         : " + std::to_string(expanded_)         + '\n' +
	" backedges        : " + std::to_string(backedges_)        + '\n' +
	" max_dfs_size     : " + std::to_string(max_dfssize_)      + '\n' +
	" cumul_reduced    : " + std::to_string(cumul_red_)        + '\n' +
	" cumul_enabled    : " + std::to_string(cumul_en_)         + '\n' +
	proviso_.dump();
    }

    std::string dump_csv()
    {
      return
	std::to_string(states_)           + ',' +
	std::to_string(transitions_)      + ',' +
	std::to_string(already_expanded_) + ',' +
	std::to_string(expanded_)         + ',' +
	std::to_string(backedges_)        + ',' +
	std::to_string(max_dfssize_)      + ',' +
	std::to_string(cumul_red_)        + ',' +
	std::to_string(cumul_en_)         + ',' +
	proviso_.dump_csv();
    }

    // ----------------------------------------------------------
    // Implement the dfs_inspector

    virtual int dfs_position(const state* st) const
    {
      if (seen.find(st) != seen.end())
	return seen[st].dfs_position;
      return -1;
    }
    virtual const state* dfs_state(int position) const
    {
      assert(position < (int)todo.size());
      return todo[position].src;
    }
    virtual twa_succ_iterator* get_iterator(unsigned dfs_position) const
    {
      assert(dfs_position < todo.size());
      return todo[dfs_position].it;
    }
    virtual bool visited(const state* s) const
    {
      return seen.find(s) != seen.end();
    }

    virtual std::vector<bool>& get_colors(const state* st) const
    {
      assert(seen.find(st) != seen.end());
      return seen[st].colors;
    }

  private:
    const_twa_ptr aut_;		///< The spot::tgba to explore.
    proviso& proviso_;
    unsigned dfs_number = 0;
    unsigned int states_ = 0;
    unsigned int transitions_ = 0;
    unsigned int already_expanded_ = 0;
    unsigned int expanded_ = 0;
    unsigned int backedges_ = 0;
    unsigned int max_dfssize_ = 0;
    unsigned int cumul_red_ = 0;
    unsigned int cumul_en_ = 0;

    struct state_info
    {
      unsigned live_number;
      int dfs_position;
      std::vector<bool> colors;
    };
    typedef std::unordered_map<const state*, state_info,
			       state_ptr_hash, state_ptr_equal> seen_map;
    mutable seen_map seen;		///< States already seen.
    struct stack_item
    {
      const state* src;
      twa_succ_iterator* it;
    };
    std::deque<stack_item> todo; ///< the DFS stack
  };
}
