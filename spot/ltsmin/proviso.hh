// -*- coding: utf-8 -*-
// Copyright (C)  2015, 2016, 2017 Laboratoire de Recherche et
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
    virtual int maybe_closingedge(const state* src,
				       const state* dst,
				       const dfs_inspector& i) = 0;
   /// \brief Notify the proviso that a a new state has been pushed. This
    /// method return true only if the src has been expanded.
    virtual bool notify_push(const state* src, const dfs_inspector& i) = 0;
    virtual bool before_pop(const state* src, const dfs_inspector& i) = 0;

    virtual std::string name() = 0;
    virtual std::string dump() = 0;
    virtual std::string dump_csv() = 0;
    virtual void delete_extra(void*)
    { }
    virtual ~proviso()
    { }
  };


  /// \brief Implementation of the source/destination family of
  /// provisos.
  template<bool BasicCheck, bool Delayed>
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
      MinNewStates,		// Choose minimal overhead in new states
      OneThenDstElseSrc 	// Destination iff one
    };

    src_dst_provisos(strategy strat): strat_(strat), generator_(0),
      source_(0), destination_(0)
    { }

    virtual std::string name()
    {
      std::string res = BasicCheck? "basiccheck_" : "";
      if (Delayed)
	res = "delayed_" + res;

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
	case strategy::OneThenDstElseSrc:
	  return res + "one_then_dst_else_src";
	default:
	  assert(false);
	  break;
	};
      return res;
    }

    virtual int maybe_closingedge(const state* src,
				  const state* dst,
				  const dfs_inspector& i)
    {
      int src_pos = i.dfs_position(src);
      int dst_pos = i.dfs_position(dst);
      assert(src_pos != -1);

      // Delayed is activated,  we must update colors.
      if (Delayed)
	{
	  // The state is not on the DFS and Delayed is activated,
	  // we must propagate the dangerousness of successors
	  if (dst_pos == -1)
	    {
	      i.get_colors(src)[2] =
		i.get_colors(src)[2]  && i.get_colors(dst)[2];

	      // Nothing else todo in this case.
	      return -1;
	    }
	}

      // State not on the DFS
      if (dst_pos == -1)
	return -1;

      bool src_expanded = i.get_iterator(src_pos)->all_enabled();
      bool dst_expanded = i.get_iterator(dst_pos)->all_enabled();

      if (BasicCheck && (src_expanded || dst_expanded))
	return -1;

      return choose(src_pos, dst_pos, src, dst, i);
    }

    virtual bool notify_push(const state* src, const dfs_inspector& i)
    {
      int src_pos = i.dfs_position(src);
      if (strat_ == strategy::All)
	{
	  i.get_iterator(src_pos)->fire_all();
          return true;
	}
      if (Delayed)
	{
	  // vector of bool : d, e, g.
	  // d : is state is not expanded and on the DFS stack?
	  // e : is an extension required for state?
	  // g : state is not dangerous (i.e. green)
	  auto& colors = i.get_colors(src);
	  auto* it = i.get_iterator(src_pos);
	  colors.push_back(it->enabled() != it->reduced());
	  colors.push_back(false);
	  colors.push_back(true);
	}
       return false;
    }

    virtual bool before_pop(const state* st,
			    const dfs_inspector& i)
    {
      // Some work must be done when delayed is used.
      if (Delayed)
	{
	  int st_pos =  i.dfs_position(st);
	  auto& colors = i.get_colors(st);
	  // An extension is required.
	  if (colors[1] && !colors[2])
	    {
	      i.get_iterator(st_pos)->fire_all();
	      colors[1] = false;
	      colors[2] = true;
	      return false;
	    }
	  else
	    {
	      // Here since we will return true, we know that
	      // we will pop state from DFS, so we can already
	      // update colors of predecessor if some exits.
	      if (st_pos != 0) // state is not the initial one
		{
		  const state* newtop = i.dfs_state(st_pos-1);
		  auto& colors_newtop = i.get_colors(newtop);

		  // We are popping an SCC: we do not need to propagate
		  // dangerousness
		  if (i.is_root(st))
		    return true;

		  colors_newtop[2] = colors_newtop[2] && colors[2];
		}
	      return true;
	    }
	}

      // If Delayed is not activated , ther is nothing to do.
      return true;
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
    int choose(int src, int dst,
	       const state* src_st, const state* dst_st,
	       const dfs_inspector& i)
    {
      switch (strat_)
	{
	case strategy::None:
	  return -1;
	case strategy::All:
	  return -1; // already expanded.
	case strategy::Source:
	  ++source_;
	  if (Delayed)
	    {
	      update_delayed(src_st, src_st, i);
	      return -1;
	    }
	  return src;
	case strategy::Destination:
	  ++destination_;
	  if (Delayed)
	    {
	      update_delayed(src_st, dst_st, i);
	      return -1;
	    }
	  return dst;
	case strategy::Random:
	  if (generator_()%2)
	    {
	      ++destination_;
	      if (Delayed)
		{
		  update_delayed(src_st, dst_st, i);
		  return -1;
		}
	      return dst;
	    }
	  ++source_;
	  if (Delayed)
	    {
	      update_delayed(src_st, src_st, i);
	      return -1;
	    }
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
		if (Delayed)
		  {
		    update_delayed(src_st, src_st, i);
		    return -1;
		  }
		return src;
	      }
	    ++destination_;
	    if (Delayed)
	      {
		update_delayed(src_st, dst_st, i);
		return -1;
	      }
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
		if (Delayed)
		  {
		    update_delayed(src_st, src_st, i);
		    return -1;
		  }
		return src;
	      }
	    ++destination_;
	  if (Delayed)
	    {
	      update_delayed(src_st, dst_st, i);
	      return -1;
	    }
	    return dst;
	  }
	case strategy::OneThenDstElseSrc:
	  {
	    // In this proviso the source is expanded only iff it has more
	    // than one (dangerous, when basickcheck) backedge. We just
	    // have to walk successors of src and stop as soon 2
	    // backedges have been detected
	    int nb_backedges = 0;
	    auto* itp = i.automaton()->succ_iter(src_st);
	    itp->first();
	    while (!itp->done())
	      {
		auto* target = itp->dst();
		int target_pos = i.dfs_position(target);
		if (target_pos != -1)
		  {
		    if (!BasicCheck)
		      ++nb_backedges;
		    else if (!i.get_iterator(target_pos)->enabled())
		      ++nb_backedges;
		  }
		target->destroy();
		itp->next();
		if (nb_backedges == 2)
		  break;
	      }
	    i.automaton()->release_iter(itp);
	    if (nb_backedges == 2)
	      {
		++source_;
		if (Delayed)
		  {
		    update_delayed(src_st, src_st, i);
		    return -1;
		  }
		return src;
	      }
	    ++destination_;
	    if (Delayed)
	      {
		update_delayed(src_st, dst_st, i);
		return -1;
	      }
	    return dst;
	  }
	default:
	  assert(false);
	  break;
	};
      return -1;
    }

    void update_delayed(const state* dfstop,
			const state* chosen,
			const dfs_inspector& i)
    {
      if (Delayed)
	{
	  i.get_colors(chosen)[1] = true; // Expansion required
	  i.get_colors(dfstop)[2] = false; // top become dangerous
	}
    }

    strategy strat_;
    std::mt19937 generator_;
    unsigned source_;
    unsigned destination_;
  };


  /// \brief Implementation of the evangelista family of
  /// provisos.
  template<bool FullyColored>
    class SPOT_API evangelista10sttt: public proviso
  {

  public:
    evangelista10sttt()
      { }

    virtual int maybe_closingedge(const state* src,
				  const state* dst,
				  const dfs_inspector& i)
    {
      auto& src_colors = i.get_colors(src);
      auto& dst_colors = i.get_colors(dst);
      bool src_is_orange = src_colors[0] && !src_colors[1];
      bool dst_is_red = dst_colors[0] && dst_colors[1];

      // src is orange and dst is red  an expansion is required.
      if (src_is_orange && dst_is_red)
	{
	  ++expanded_;
	  src_colors[0] = false;
	  src_colors[1] = false;

	  int src_pos = i.dfs_position(src);

	  if (FullyColored)
	    {
	      // We can propagate the green color. Note that
	      // here the source is not yet expanded but has
	      // already the good color so we can propagate!
	      int p = src_pos-1;
	      while (0 <= p)
		{
		  const state* st = i.dfs_state(p);
		  auto& colors = i.get_colors(st);
		  bool is_orange = colors[0] && !colors[1];

		  if (is_orange && i.get_iterator(p)->done())
		    {
		      // Propagate green
		      colors[0] = false;
		      colors[1] = false;
		      --p;
		    }
		  else
		    break;
		}
	    }

	  // Require to expand the source of the edge.
	  ++source_;
	  return src_pos;
	}

      // This maybe closing-edge is safe.
      return -1;
    }

    virtual bool notify_push(const state* src, const dfs_inspector& i)
    {
      // Every state is associated to a color ORANGE, RED, and GREEN
      // We need two boolean to encode this information.
      // 10 -> ORANGE
      // 11 -> RED
      // 00 -> GREEN
      auto& colors = i.get_colors(src);
      colors.push_back(true);
      colors.push_back(false);

      // Actually Sami's algorithm do not have a weight but an expanded
      // field foreach state. We use the weight of the generic dfs to
      // store this information.
      i.get_weight(src) = expanded_;

      int src_pos = i.dfs_position(src);
      bool res = false;
      if (!i.get_iterator(src_pos)->all_enabled() && !is_c2cl(src, i))
	{
	  i.get_iterator(src_pos)->fire_all();
	  ++source_;
	  res = true;
	}

      // Set state to green
      if (i.get_iterator(src_pos)->all_enabled())
	{
	  ++expanded_;
	  colors[0] = false;
	  colors[1] = false; // Useless since this bit must already be false.
	}
      return res;
    }

    virtual bool before_pop(const state* src, const dfs_inspector& i)
    {
      int src_pos = i.dfs_position(src);
      if (i.get_iterator(src_pos)->all_enabled())
	--expanded_;

      auto& colors = i.get_colors(src);
      if (colors[0] && !colors[1]) // State is orange.
	{
	  auto* it = i.get_iterator(src_pos);

	  it->first();
	  bool isred = false;
	  while (!it->done())
	    {
	      auto* dst = it->dst();
	      auto& dst_colors = i.get_colors(dst);
	      bool dst_is_green = !dst_colors[0] && !dst_colors[1];

	      if (i.is_dead(dst))
		{
		  dst->destroy();
		  it->next();
		  continue;
		}

	      if (!dst_is_green) //dst is not green
		isred = true;
	      dst->destroy();
	      it->next();
	    }
	  if (isred) // set color to red
	    {
	      colors[0] = true;
	      colors[1] = true;
	    }
	  else // set color to green
	    {
	      colors[0] = false;
	      colors[1] = false;
	    }
	}

      // Never avoid a POP in Sami Evangelista and Pajault
      return true;
    }

    virtual std::string name()
    {
      if (FullyColored)
	return "fullycolored_evangelista10sttt";
      else
	return "evangelista10sttt";
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

    virtual ~evangelista10sttt()
    { }


  private:

    // In the algorithm of Evangelista, the procedure c2cl check
    // for every new state if an expansion is required according
    // to its reducet set of successors.
    bool is_c2cl(const state* st, const dfs_inspector& i)
    {
      // FIXME find another way to not use std::function
      static const dfs_inspector* i_ptr;
      static bool res;
      static int st_weight;

      // Initalize
      i_ptr = &i;
      res = true;
      st_weight = i.get_weight(st);

      // Grab the position of st.
      int st_pos = i.dfs_position(st);
      assert(st_pos != -1);

      // Here reorder remaining will walk remaining transitions
      // Since is_c2cl is only call during the PUSH remaining will
      // consist of the Reduced set.
      i.get_iterator(st_pos)->
	reorder_remaining([](const state* sp)
			  {
			    if (i_ptr->visited(sp))
			      {
				auto& colors = i_ptr->get_colors(sp);
				bool sp_is_red = colors[0] && colors[1];
				bool sp_is_orange = colors[0] && !colors[1];
				if (sp_is_red ||
				    (sp_is_orange &&
				     st_weight == i_ptr->get_weight(sp)))
				  res = false;
			      }

			    // do not reorder.
			    return false;
			  });
      return res;
    }

    int expanded_ = 0;
    unsigned source_ = 0;
    unsigned destination_ = 0; ///< stay to zero but to have homogeneous csv.
  };

  /// \brief This class implements the expanded list proviso.
  /// This is an adaptation of the proviso of Evangelista and Pajault
  /// but since we use an expanded list rather than weights we can adopt
  /// different stratgies for the expansion. If we decide to always expand
  /// the source then the algorithm is the one of Evangelista and Pajault
  template<bool FullyColored>
  class SPOT_API expandedlist_provisos: public proviso
  {
  public:
    enum class strategy
    {
      Source,			// Source is expanded
      Destination,		// Destination is expanded
      Random,			// Choose randomly between source and dest.
      MinEnMinusRed,		// Choose minimal overhead in transitions
      MinNewStates,		// Choose minimal overhead in new states
      OneThenDstElseSrc
    };

    expandedlist_provisos(strategy strat, unsigned power_of = 0,
			  bool highlinks = false):
      strat_(strat), generator_(0),
      Highlinks(highlinks), power_of_(power_of)
      { }

    virtual int maybe_closingedge(const state* src,
				  const state* dst,
				  const dfs_inspector& i)
    {
      auto& src_colors = i.get_colors(src);
      auto& dst_colors = i.get_colors(dst);
      bool src_is_orange = src_colors[0] && !src_colors[1];
      bool dst_is_red = dst_colors[0] && dst_colors[1];
      bool dst_is_orange = dst_colors[0] && !dst_colors[1];

      if (Highlinks)
	{
	  // The destination is on the DFS stack, just update
	  // highlink if needed.
	  if (i.dfs_position(dst) != -1)
	    {
	      const state* src_highlink = i.get_highlink(src);
	      if (src_highlink == nullptr ||
	  	  i.dfs_position(src_highlink) < i.dfs_position(dst))
	  	{
	  	  i.set_highlink(src,  dst);
	  	  assert(dst->compare(i.get_highlink(src)) == 0);
	  	}
	    }
	  // Otherwise we have to compute the actual highlink of the
	  // destination and compare it to the highlink of the source.
	  else
	    {
	      const state* dst_highlink = i.get_highlink(dst);

	      // if the highlink is not set, it means that we are not
	      // computing SCC but highlinks correctness rely on SCC
	      // computation.
	      assert(dst_highlink != nullptr);
	      assert(dst_highlink->compare(i.get_highlink(dst)) == 0);

	      std::vector<const state*> v;
	      v.push_back(dst);
	      while (i.dfs_position(dst_highlink) == -1)
	      	{
	      	  v.push_back(dst_highlink);
	      	  dst_highlink = i.get_highlink(dst_highlink);
	      	  assert(dst_highlink != nullptr);
	      	}
	      for (auto q: v)
	      	{
	      	  i.set_highlink(q, dst_highlink);
	      	  assert(dst_highlink->compare(i.get_highlink(q)) == 0);
	      	}

	      const state* src_highlink = i.get_highlink(src);
	      if (src_highlink == nullptr ||
	  	  i.dfs_position(src_highlink) < i.dfs_position(dst_highlink))
	  	{
	  	  i.set_highlink(src,  dst_highlink);
	  	}

	    }
	}



      // src is orange and dst is red  an expansion is required.
      if (src_is_orange && dst_is_red)
	{
	  // HERE we use entry points (aka highlinks)
	  // to detect the portion of the DFS stack in which
	  // an extension is required.
	  if (Highlinks)
	  {
	    assert(!i.is_dead(dst));
	    const state* high = i.get_highlink(dst);
	    assert(high != nullptr);
	    int src_pos = i.dfs_position(src);
	    int high_pos = i.dfs_position(high);
	    assert(high_pos != -1);

	    // Check is expansions are required.
	    if (!expanded_.empty() &&
		((int)expanded_.back()) >= high_pos)
	      return -1;

	    // The power of X is activated, choose one among X in the
	    // DFS stack. choose_powerof also change color and insert
	    // inside of the expanded list when needed.
	    // powerof can also propagate green
	    if (power_of_)
	      return choose_powerof(src_pos, high_pos, i);

	    high_pos = choose (src_pos, high_pos, i);
	    auto& high_colors = i.get_colors(high);
	    high_colors[0] = false;
	    high_colors[1] = false;
	    expanded_.push_back(high_pos);

	    // Do something only if FullyColored is activated.
	    propagate_green(high_pos, i);
	    return high_pos;
	  }

	  src_colors[0] = false;
	  src_colors[1] = false;

	  int src_pos = i.dfs_position(src);
	  expanded_.push_back(src_pos);

	  // Require to expand the source of the edge.
	  ++source_;
	  // Do something only if FullyColored is activated.
	  propagate_green(src_pos, i);
	  return src_pos;
	}
      else if (dst_is_orange && src_is_orange)
	{
	  int src_pos = i.dfs_position(src);
	  int dst_pos = i.dfs_position(dst);

	  if (!expanded_.empty() &&
	      ((int)expanded_.back()) >= dst_pos)
	    return -1;


	  // The power of X is activated, choose one among X in the
	  // DFS stack. choose_powerof also change color and insert
	  // inside of the expanded list when needed.
	  if (power_of_)
	    return choose_powerof(src_pos, dst_pos, i);

	  // Choose one state to expand, insert it into
	  // the expanded list, and finally change its
	  // color
	  int to_expand = choose (src_pos, dst_pos, i);
	  if (to_expand == src_pos)
	    {
	      expanded_.push_back(src_pos);
	      src_colors[0] = false;
	      src_colors[1] = false;
	      propagate_green(src_pos, i);
	      return src_pos;
	    }
	  else
	    {
	      expanded_.push_back(dst_pos);
	      dst_colors[0] = false;
	      dst_colors[1] = false;
	      propagate_green(dst_pos, i);
	      return dst_pos;
	    }
	}

      // This maybe closing-edge is safe.
      return -1;
    }

    virtual bool notify_push(const state* src, const dfs_inspector& i)
    {
      // Every state is associated to a color ORANGE, RED, and GREEN
      // We need two boolean to encode this information.
      // 10 -> ORANGE
      // 11 -> RED
      // 00 -> GREEN
      auto& colors = i.get_colors(src);
      colors.push_back(true);
      colors.push_back(false);

      // Set state to green if needed.
      int src_pos = i.dfs_position(src);
      if (i.get_iterator(src_pos)->all_enabled())
	{
	  expanded_.push_back(src_pos);
	  colors[0] = false;
	  colors[1] = false; // Useless since this bit must already be false.
	  propagate_green(src_pos, i);
	}

      // Here we do not perform an expansion even when we always expand the
      // source (i.e. for evangelista). The trick to obtain evangelista's
      // algorithm is to specify Anticipation in dfs_stat
      return false;
    }

    virtual bool before_pop(const state* src, const dfs_inspector& i)
    {
      int src_pos = i.dfs_position(src);

      if (Highlinks && !i.is_root(src) && src_pos)
	{
	  // We have to propagate highlinks. We must consider
	  // two cases:
	  //   (i)  highlink(src) == src, just set highlink(src) to
	  //        the predecessor of src in the DFS stack.
	  //   (ii) highlink(src) != src, just progate highlink(src)
	  //        w.r.t. highlink mecanism.

	  const state* newtop = i.dfs_state(src_pos-1);
	  const state* src_highlink = i.get_highlink(src);
	  if (src_highlink == nullptr ||
	      i.dfs_position(src_highlink) == src_pos)
	    {
	      i.set_highlink(src, newtop);
	    }
	  else
	    {
	      const state* newtop_highlink = i.get_highlink(newtop);
	      if (newtop_highlink == nullptr)
	      	i.set_highlink(newtop, src_highlink);

	      // Otherwise we must compare the highlink of src and pred
	      else if (i.dfs_position(src_highlink) <
		  i.dfs_position(newtop_highlink))
	  	{
	  	  i.set_highlink(newtop_highlink,  src_highlink);
	  	}

	    }
	}

      if (i.get_iterator(src_pos)->all_enabled()
	  && (int)expanded_.back() == src_pos)
	expanded_.pop_back();

      auto& colors = i.get_colors(src);
      if (colors[0] && !colors[1]) // State is orange.
	{
	  auto* it = i.get_iterator(src_pos);

	  it->first();
	  bool isred = false;
	  while (!it->done())
	    {
	      auto* dst = it->dst();
	      auto& dst_colors = i.get_colors(dst);
	      bool dst_is_green = !dst_colors[0] && !dst_colors[1];

	      if (i.is_dead(dst))
		{
		  dst->destroy();
		  it->next();
		  continue;
		}

	      if (!dst_is_green) //dst is not green
		isred = true;
	      dst->destroy();
	      it->next();
	    }
	  if (isred) // set color to red
	    {
	      colors[0] = true;
	      colors[1] = true;
	    }
	  else // set color to green
	    {
	      colors[0] = false;
	      colors[1] = false;
	    }
	}

      // Never avoid a POP in Sami Evangelista and Pajault
      return true;
    }

    virtual std::string name()
    {
      std::string res  = "expandedlist_";
      if (Highlinks)
	res += "highlink_";
      if (FullyColored)
	res += "fullycolored_";
      if (power_of_)
	res += "powerof" + std::to_string(power_of_) + "_";

      switch (strat_)
	{
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
	case strategy::OneThenDstElseSrc:
	  return res +  "one_then_dst_else_src";
	default:
	  assert(false);
	  break;
	};
      return res;
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
    virtual ~expandedlist_provisos()
    { }
  private:

    int choose(int src, int dst,
	       // const state* src_st, const state* dst_st,
	       const dfs_inspector& i)
    {
      switch (strat_)
	{
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
	case strategy::OneThenDstElseSrc:
	  {
	    // In this proviso the source is expanded only iff it has more
	    // than one dangerous backedge. We just have to walk
	    // successors of src and stop as soon 2 backedges have
	    // been detected
	    int nb_backedges = 0;
	    bool one_succ_is_red = false;
	    auto* itp = i.automaton()->succ_iter(i.dfs_state(src));
	    itp->first();
	    while (!itp->done())
	      {
		auto* target = itp->dst();
		int target_pos = i.dfs_position(target);
		if (target_pos != -1)
		  {
		    // This is a dangerous backedge
		    if (expanded_.empty() ||
			((int)expanded_.back()) < target_pos)
		      ++nb_backedges;
		  }
		// State is not on the DFS but already visited, so
		// it has some color.
		else if (i.visited(target))
		  {
		    auto& colors = i.get_colors(target);
		    // State is RED
		    if (colors[0]  && colors[1])
		      one_succ_is_red = true;
		  }
		target->destroy();
		itp->next();
		if (nb_backedges == 2 || one_succ_is_red)
		  break;
	      }
	    i.automaton()->release_iter(itp);
	    if (one_succ_is_red || nb_backedges == 2)
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

    int choose_powerof(int src_pos, int dst_pos,
		       const dfs_inspector& i)
    {
      unsigned range = src_pos - dst_pos+ 1;
      assert(range);
      std::vector<unsigned> positions;
      for (unsigned j = 0; j < power_of_; ++j)
	{
	  unsigned pos = dst_pos + generator_()%range;

	  // The selected position is expanded, no
	  // expansion is required.
	  if (i.get_iterator(pos)->all_enabled())
	    return -1;		// Should never happen

	  positions.push_back(pos);
	}

      switch (strat_)
	{
	case strategy::Random:
	  {
	    unsigned p  = generator_()%positions.size();
	    expanded_.push_back(positions[p]);
	    auto& colors = i.get_colors(i.dfs_state(positions[p]));
	    // Turn green the selected element
	    colors[0] = false;
	    colors[1] = false;
	    propagate_green(positions[p], i);
	    return positions[p];
	  }
	case strategy::MinEnMinusRed:
	  {
	    unsigned sel = 0;
	    for (unsigned j = 1; j < positions.size(); ++j)
	      {
		unsigned enminred_sel =
		  i.get_iterator(positions[sel])->enabled() -
		  i.get_iterator(positions[sel])->reduced();
		unsigned enminred_curr =
		  i.get_iterator(positions[j])->enabled() -
		  i.get_iterator(positions[j])->reduced();
		if (enminred_sel > enminred_curr)
		  sel = j;
	      }
	    expanded_.push_back(positions[sel]);
	    auto& colors = i.get_colors(i.dfs_state(positions[sel]));
	    // Turn green the selected element
	    colors[0] = false;
	    colors[1] = false;
	    propagate_green(positions[sel], i);
	    return positions[sel];
	  }
	case strategy::MinNewStates:
	  {
	    unsigned sel = 0;
	    static const dfs_inspector* i_ptr;
	    i_ptr = &i;

	    for (unsigned j = 1; j < positions.size(); ++j)
	      {
		static unsigned newstates_sel;
		newstates_sel = 0;
		i.get_iterator(positions[sel])->
		  expand_will_generate([](const state* s)
				       {
					 if (i_ptr->visited(s))
					   ++newstates_sel;
				       });
		static unsigned newstates_curr;
		newstates_sel = 0;
		i.get_iterator(positions[j])->
		  expand_will_generate([](const state* s)
				       {
					 if (i_ptr->visited(s))
					   ++newstates_curr;
				       });
		if (newstates_sel > newstates_curr)
		  sel = j;
	      }
	    // Turn green the selected element
	    expanded_.push_back(positions[sel]);
	    auto& colors = i.get_colors(i.dfs_state(positions[sel]));
	    colors[0] = false;
	    colors[1] = false;
	    propagate_green(positions[sel], i);
	    return positions[sel];
	  }
	default:
	  std::cerr << "Not Compatible with Power Of \n";
	  exit(1);
	}
    }


    void propagate_green(int expanded_pos, const dfs_inspector& i)
    {
      if (FullyColored)
	{
	  assert(expanded_pos != -1);
	  // We can propagate the green color. Note that
	  // here the source is not yet expanded but has
	  // already the good color so we can propagate!
	  int p = expanded_pos-1;
	  while (0 <= p)
	    {
	      const state* st = i.dfs_state(p);
	      auto& colors = i.get_colors(st);
	      bool is_orange = colors[0] && !colors[1];

	      if (is_orange && i.get_iterator(p)->done())
		{
		  auto* itp = i.get_iterator(p);
		  itp->first();
		  bool isgreen = true;
		  while (!itp->done())
		    {
		      auto* dst = itp->dst();
		      auto& dst_colors = i.get_colors(dst);
		      bool dst_is_green = !dst_colors[0] && !dst_colors[1];
		      assert(i.visited(dst));
		      if (!dst_is_green) //dst is not green
			isgreen = false;
		      dst->destroy();
		      itp->next();
		    }

		  if (!isgreen)
		    break;

		  // Propagate green
		  colors[0] = false;
		  colors[1] = false;
		  --p;
		}
	      else
		break;
	    }
	}
    }

    std::vector<unsigned> expanded_;
    unsigned source_ = 0;
    unsigned destination_ = 0; ///< stay to zero but to have homogeneous csv.
    strategy strat_;
    std::mt19937 generator_;
    bool Highlinks;
    unsigned power_of_;
  };













  //template<bool FullyColored>
  class SPOT_API summary_provisos: public proviso
  {
    enum class color{ ORANGE, GREEN, PURPLE, RED };
    struct data
    {
      color c;
      bool mark;
      int depth;
      int mx_d;
      const state* mx_s;
    };
    bool anticipated_;
    bool highlink_;
  public:
    summary_provisos(bool anticipated = false,
		     bool highlink = false): anticipated_(anticipated),
      highlink_(highlink)
      { }

    virtual bool notify_push(const state* src, const dfs_inspector& i)
    {
      ++d_;
      data* edata = new data({
	  i.get_iterator(d_)->all_enabled() ? color::GREEN : color::ORANGE,
	  false, d_, -1, nullptr});
      i.set_extra_data(src, edata);
      if (i.get_iterator(d_)->all_enabled())
	expanded_.push_back(d_);


      // During anticipation we first process all transitions
      // leading to already visited states
      if (anticipated_)
	{
	  data* data_src = (data*) i.get_extra_data(src);
	  twa_succ_iterator* it = i.get_iterator(d_);
	  while (!it->done())
	    {
	      const state* dst = it->dst();
	      if (i.visited(dst))
		{
		  // check if dst == src since src not yet inserted
		  // into the union find.
		  if (dst->compare(src) != 0 && i.is_dead(dst))
		    {
		      dst->destroy();
		      it->next();
		      continue;
		    }

		  data* data_dst = (data*) i.get_extra_data(dst);

		  if (data_src->c == color::ORANGE ||
		      data_src->c == color::PURPLE) // state is brown
		    {
		      if (data_dst->c != color::GREEN)
			{
			  data_src->c = color::PURPLE;
			  if (data_dst->c == color::RED)
			    {
			      if (highlink_)
				{
				  data* data_hi = (data*)
				    i.get_extra_data(i.get_highlink(dst));

				  if (data_src->mx_d < data_hi->depth)
				    {
				      data_src->mx_d = d_;
				      data_src->mx_s = i.get_highlink(dst);
				    }
				  dst->destroy();
				  it->next();
				  continue;
				}

			      data_src->mx_d = d_;
			      data_src->mx_s = src;
			    }
			  else if (data_dst->c == color::ORANGE ||
				   data_dst->c == color::PURPLE)
			    // state is brown
			    {
			      if (expanded_.empty() ||
				  data_dst->depth > (int)expanded_.back())
				{
				  if (data_src->mx_d < data_dst->depth)
				    {
				      data_src->mx_d = data_dst->depth;
				      data_src->mx_s = dst;
				    }
				}
			    }
			}
		    }
		}
	      dst->destroy();
	      it->next();
	    }
	  it->first();		// Reset the iterator for the main DFS procedure
	  if (data_src->mx_d == d_)//-1)
	    {
 	      // unsigned enminred_src =
	      // 	i.get_iterator(d_)->enabled() -
	      // 	i.get_iterator(d_)->reduced();
	      // unsigned enminred_mxs =
	      // 	i.get_iterator(data_src->mx_d)->enabled() -
	      // 	i.get_iterator(data_src->mx_d)->reduced();

	      // if (data_src->mx_d == d_ || enminred_src < enminred_mxs)
	      // 	{
		  expand(src, i);
		  return true;
		// }
	    }
	}
      return false;
    }

    virtual int maybe_closingedge(const state* src,
				  const state* dst,
				  const dfs_inspector& i)
    {
      if (i.is_dead(dst))
	return -1;


      if (highlink_)
	{
	  // The destination is on the DFS stack, just update
	  // highlink if needed.
	  if (i.dfs_position(dst) != -1)
	    {
	      const state* src_highlink = i.get_highlink(src);
	      if (src_highlink == nullptr ||
	  	  i.dfs_position(src_highlink) < i.dfs_position(dst))
	  	{
	  	  i.set_highlink(src,  dst);
	  	  assert(dst->compare(i.get_highlink(src)) == 0);
	  	}
	    }
	  // Otherwise we have to compute the actual highlink of the
	  // destination and compare it to the highlink of the source.
	  else
	    {
	      const state* dst_highlink = i.get_highlink(dst);

	      // if the highlink is not set, it means that we are not
	      // computing SCC but highlinks correctness rely on SCC
	      // computation.
	      assert(dst_highlink != nullptr);
	      assert(dst_highlink->compare(i.get_highlink(dst)) == 0);

	      std::vector<const state*> v;
	      v.push_back(dst);
	      while (i.dfs_position(dst_highlink) == -1)
	      	{
	      	  v.push_back(dst_highlink);
	      	  dst_highlink = i.get_highlink(dst_highlink);
	      	  assert(dst_highlink != nullptr);
	      	}
	      for (auto q: v)
	      	{
	      	  i.set_highlink(q, dst_highlink);
	      	  assert(dst_highlink->compare(i.get_highlink(q)) == 0);
	      	}

	      const state* src_highlink = i.get_highlink(src);
	      if (src_highlink == nullptr ||
	  	  i.dfs_position(src_highlink) < i.dfs_position(dst_highlink))
	  	{
	  	  i.set_highlink(src,  dst_highlink);
	  	}
	    }
	}



      data* data_src = (data*) i.get_extra_data(src);
      data* data_dst = (data*) i.get_extra_data(dst);


      // If anticipated two cases are of interrest:
      //    - state has been expanded so it's green and we don't care
      //      about any transitions
      //    - state has not been expanded. In this case, if the destination
      //      is red an expansion is required. Otherwise since,  we already have
      //      visited this transitions during notify_push, we can skip it.
      if (anticipated_)
	{
	  if (data_src->c == color::ORANGE && data_dst->c == color::RED)
	    {
	      if (highlink_)
		{
		  const state* hi = i.get_highlink(dst);
		  data* data_hi = (data*) i.get_extra_data(hi);

		 if (data_hi->depth != d_)
		   {
		     data_src->c = color::PURPLE;
		     if (data_src->mx_d < data_hi->depth)
		       {
			 data_src->mx_d = d_;
			 data_src->mx_s = i.get_highlink(dst);
		       }
		     return -1;
		   }
		 else
		   {
		      expand (src, i);
		      return d_;
		   }
		}
	      expand (src, i);
	      return d_;
	    }
	  return -1;
	}


      if (data_src->c == color::ORANGE ||
	  data_src->c == color::PURPLE) // state is brown
	{
	  if (data_dst->c != color::GREEN)
	    {
	      data_src->c = color::PURPLE;
	      if (data_dst->c == color::RED)
		{
		  if (highlink_)
		    {
		      data* data_hi =
			(data*) i.get_extra_data(i.get_highlink(dst));

		      if (data_src->mx_d < data_hi->depth)
			{
			  data_src->mx_d = d_;
			  data_src->mx_s = i.get_highlink(dst);
			}
		      return -1;
		    }

		  data_src->mx_d = d_;
		  data_src->mx_s = src;
		}
	      else if (data_dst->c == color::ORANGE ||
		       data_dst->c == color::PURPLE) // state is brown
		{
		  if (expanded_.empty() ||
		      data_dst->depth > (int)expanded_.back())
		    {
		      if (data_src->mx_d < data_dst->depth)
			{
			  data_src->mx_d = data_dst->depth;
			  data_src->mx_s = dst;
			}
		    }
		}
	    }
	}

      // This maybe closing-edge is "safe", i.e. expansions are not
      // performed here
      return -1;
    }

    virtual bool before_pop(const state* src, const dfs_inspector& i)
    {
      if (highlink_)
	{
	  int src_pos = i.dfs_position(src);
	  if (!i.is_root(src) && src_pos)
	    {
	      // We have to propagate highlinks. We must consider
	      // two cases:
	      //   (i)  highlink(src) == src, just set highlink(src) to
	      //        the predecessor of src in the DFS stack.
	      //   (ii) highlink(src) != src, just progate highlink(src)
	      //        w.r.t. highlink mecanism.

	      const state* newtop = i.dfs_state(src_pos-1);
	      const state* src_highlink = i.get_highlink(src);
	      if (src_highlink == nullptr ||
		  i.dfs_position(src_highlink) == src_pos)
		{
		  i.set_highlink(src, newtop);
		}
	      else
		{
		  const state* newtop_highlink = i.get_highlink(newtop);
		  if (newtop_highlink == nullptr)
		    i.set_highlink(newtop, src_highlink);

		  // Otherwise we must compare the highlink of src and pred
		  else if (i.dfs_position(src_highlink) <
			   i.dfs_position(newtop_highlink))
		    {
		      i.set_highlink(newtop_highlink,  src_highlink);
		    }

		}
	    }
	}



      data* data_src = (data*) i.get_extra_data(src);

      if (anticipated_)
	{
	  if (data_src->c == color::PURPLE)
	    {
	      if (data_src->mark)
		{
		  expand (src, i);
		  return false;
		}
	      else if (data_src->mx_d != -1)
		{
		  data* data_mxs = (data*) i.get_extra_data(data_src->mx_s);
		  data_mxs->mark = true;
		  data_src->c = color::RED;
		}
	    }
	  switch (data_src->c)
	    {
	    case color::GREEN:
	      expanded_.pop_back();
	      --d_;
	      return true;		// Do not avoid pop
	    case color::ORANGE:
	      data_src->c = color::GREEN;
	      --d_;
	      return true;		// Do not avoid pop
	    case color::PURPLE:
	      data_src->c = color::RED;
              SPOT_FALLTHROUGH;
              // Do not return now, some work todo at the end
	      // this function.
	    default:
	      ;
	    }
	}
      else
	{
	  switch (data_src->c)
	    {
	    case color::GREEN:
	      expanded_.pop_back();
	      --d_;
	      return true;		// Do not avoid pop
	    case color::ORANGE:
	      data_src->c = color::GREEN;
	      --d_;
	      return true;		// Do not avoid pop
	    case color::PURPLE:
	      {
		if (data_src->mark || data_src->mx_d == d_)
		  {
		    expand(src, i);
		    return false;
		  }
		else if (data_src->mx_d != -1)
		  {
		    // unsigned enminred_src =
		    //   i.get_iterator(d_)->enabled() -
		    //   i.get_iterator(d_)->reduced();
		    // unsigned enminred_mxs =
		    //   i.get_iterator(data_src->mx_d)->enabled() -
		    //   i.get_iterator(data_src->mx_d)->reduced();

		    if (data_src->mx_d == d_) //enminred_src < enminred_mxs)
		      {
			expand(src, i);
			return false;
		      }
		    else
		      {
			data* data_mxs =
			  (data*) i.get_extra_data(data_src->mx_s);
			data_mxs->mark = true;
			data_src->c = color::RED;
		      }
		  }
		else
		  {
		    data_src->c = color::RED;
		  }
	      }
              SPOT_FALLTHROUGH;
	    default: // state is red
	      ;
	    }
	}

      if (d_ > 0)
	{
	  data* data_pred = (data*)i.get_extra_data(i.dfs_state(d_-1));
	  if (data_src->c == color::RED  && data_pred->c == color::ORANGE)
	    data_pred->c = color::PURPLE;
	}

      --d_;

      // We are in the middle of a pop do not avoid it !
      return true;
    }


    void expand (const state* src, const dfs_inspector& i)
    {
      data* data_src = (data*) i.get_extra_data(src);
      data_src->c = color::GREEN;
      i.get_iterator(d_)->fire_all();
      expanded_.push_back(d_);
    }

    virtual std::string name()
    {
      std::string res = "";
      if (highlink_)
	res += "highlink_";
      if (anticipated_)
	res += "anticipated_";
      res += "summary";
      return res;
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
    virtual ~summary_provisos()
    { }

    virtual void delete_extra(void* edata)
    {
      data* d = (data*) edata;
      delete d;
    }
  private:
    std::vector<unsigned> expanded_;
    int d_ = -1;
    unsigned source_ = 0;
    unsigned destination_ = 0; ///< stay to zero but to have homogeneous csv.
    std::mt19937 generator_;
  };






















}
