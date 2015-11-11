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

#include <string>
#include <vector>
#include <random>
#include <unordered_map>
#include <spot/twa/twa.hh>
#include <spot/misc/common.hh>
#include <spot/ltsmin/dfs_inspector.hh>

namespace spot
{
  /// \brief This class represent a proviso.
  class SPOT_API proviso
  {
  public:
    /// \brief This method is called for every closing edge but sometimes
    /// we cannot ditinguish between closing and forward edge: in this
    /// the method is also called.
    /// Return the position in the DFS stack of the state to expand.
    /// Note that according to the cycle proviso, every cycle must
    /// contain an expanded state. If  the method return -1, no expansion
    /// is needed.
    virtual unsigned maybe_closingedge(const state* src,
				       const state* dst,
				       const dfs_inspector& i) = 0;
    virtual void notify_push(const state* src, const dfs_inspector& i) = 0;

    virtual std::string name() = 0;
    virtual std::string dump() = 0;
    virtual std::string dump_csv() = 0;
    virtual ~proviso()
    { }
  };

  /// \brief Implementation of the source/destination family of
  /// provisos.
  template<bool BasicCheck>
  class SPOT_API src_dst_provisos: public proviso
  {
  public:
    enum class strategy
    {
      None,			// Nothing is expanded (incorrect regarding MC)
      All,			// All states are expanded (i.e. no proviso)
      Source,			// Source is expanded
      Destination,		// Destination is expanded
      Random,			// Choose randomly between source and dest.
      MinEnMinusRed,		// Choose minimal overhead in transitions
      MinNewStates		// Choose minimal overhead in new states
    };

    src_dst_provisos(strategy strat): strat_(strat), generator_(0),
      source_(0), destination_(0)
    { }

    virtual std::string name()
    {
      std::string res = BasicCheck? "basiccheck_" : "";
      switch (strat_)
	{
	case strategy::None:
	  return res +  "none";
	case strategy::All:
	  return res +  "all";
	case strategy::Source:
	  return res +  "source";
	case strategy::Destination:
	  return res +  "destination";
	case strategy::Random:
	  return res +  "random";
	case strategy::MinEnMinusRed:
	  return res +  "min_en_minus_red";
	case strategy::MinNewStates:
	  return res +  "min_new_states";
	default:
	  assert(false);
	  break;
	};
      return res;
    }

    virtual unsigned maybe_closingedge(const state* src,
				       const state* dst,
				       const dfs_inspector& i)
    {
      int src_pos = i.dfs_position(src);
      int dst_pos = i.dfs_position(dst);
      assert(src_pos != -1);

      // State not on the DFS
      if (dst_pos == -1)
	return -1;

      bool src_expanded = i.get_iterator(src_pos)->all_enabled();
      bool dst_expanded = i.get_iterator(dst_pos)->all_enabled();

      if (BasicCheck && (src_expanded || dst_expanded))
	return -1;

      return choose(src_pos, dst_pos, i);
    }

    virtual void notify_push(const state* src, const dfs_inspector& i)
    {
      if (strat_ == strategy::All)
	{
	  int src_pos = i.dfs_position(src);
	  i.get_iterator(src_pos)->fire_all();
	}
    }

    virtual std::string dump()
    {
      return
	" source_expanded  : " + std::to_string(source_)           + '\n' +
	" dest_expanded    : " + std::to_string(destination_)      + '\n';
    }
    virtual std::string dump_csv()
    {
      return
	std::to_string(source_)           + ',' +
	std::to_string(destination_);
    }

  private:
    int choose(int src, int dst, const dfs_inspector& i)
    {
      switch (strat_)
	{
	case strategy::None:
	  return -1;
	case strategy::All:
	  return -1; // already expanded.
	case strategy::Source:
	  ++source_;
	  return src;
	case strategy::Destination:
	  ++destination_;
	  return dst;
	case strategy::Random:
	  if (generator_()%2)
	    {
	      ++destination_;
	      return dst;
	    }
	  ++source_;
	  return src;
	case strategy::MinEnMinusRed:
	  {
	    unsigned enminred_src =
	      i.get_iterator(src)->enabled() - i.get_iterator(src)->reduced();
	    unsigned enminred_dst =
	      i.get_iterator(dst)->enabled() - i.get_iterator(dst)->reduced();
	    if (enminred_src < enminred_dst)
	      {
		++source_;
		return src;
	      }
	    ++destination_;
	    return dst;
	  }
	case strategy::MinNewStates:
	  {
	    // FIXME The use of static is an ugly hack but here we cannot
	    //  use lamdba capture [&] in expand_will_generate.
	    // I see two ways to resolve this hack:
	    //  - use std::function but it's too costly.
	    //  - replace expand_will_generate by a method ignored that
	    //    return an (auto)-iterator over all ignored states.
	    static unsigned new_src;
	    static unsigned new_dst;
	    static const dfs_inspector* i_ptr;
	    new_src = 0;
	    new_dst = 0;
	    i_ptr = &i;

	    i.get_iterator(src)->
	      expand_will_generate([](const state* s)
	    			{
				  if (i_ptr->visited(s))
	    			     ++new_src;
	    			});
	    i.get_iterator(dst)->
	      expand_will_generate([](const state* s)
	    			{
	    			  if (i_ptr->visited(s))
	    			    ++new_dst;
	    			});
	    if (new_src < new_dst)
	      {
		++source_;
		return src;
	      }
	    ++destination_;
	    return dst;
	  }
	default:
	  assert(false);
	  break;
	};
      return -1;
    }
    strategy strat_;
    std::mt19937 generator_;
    unsigned source_;
    unsigned destination_;
  };
}
