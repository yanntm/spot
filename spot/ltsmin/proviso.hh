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
#include <unordered_map>
#include <spot/twa/twa.hh>
#include <spot/misc/common.hh>

namespace spot
{
  // ----------------------------------------------------------------------
  // \brief Interface for proviso
  class SPOT_API proviso
  {
  public:
    typedef std::unordered_map<const state*, int,
			       state_ptr_hash, state_ptr_equal> seen_map;

    virtual bool expand_new_state(int n, bool expanded,
				  // This part of the protoype is ugly but
				  // needed by some proviso
				  twa_succ_iterator*, seen_map&,
				  const state*, const const_twa_ptr&) = 0;

    virtual bool expand_src_closingedge(int src, int dst) = 0;

    virtual bool expand_before_pop(int n,
				   // This part of the protoype is ugly but
				   // needed by some proviso
				   twa_succ_iterator*, seen_map&) = 0;
    virtual ~proviso()
    { }
  };

  // ----------------------------------------------------------------------
  // \brief Implementation of an empty proviso : no states will be
  // expanded
  class SPOT_API no_proviso: public proviso
  {
  public:
    bool expand_new_state(int, bool, twa_succ_iterator*,
			  seen_map&, const state*, const const_twa_ptr&);
    bool expand_src_closingedge(int, int);
    bool expand_before_pop(int, twa_succ_iterator*, seen_map&);
    virtual ~no_proviso();
  };

  // \brief Implementation of an empty proviso : no states will be
  // expanded
  class SPOT_API fireall_proviso: public proviso
  {
  public:
    bool expand_new_state(int, bool, twa_succ_iterator*,
			  seen_map&, const state*, const const_twa_ptr&);
    bool expand_src_closingedge(int, int);
    bool expand_before_pop(int, twa_succ_iterator*, seen_map&);
    virtual ~fireall_proviso();
  };

  // ----------------------------------------------------------------------
  // \brief Implementation of the cycle proviso of SPIN. All states
  // with a backedge are expanded.
  class SPOT_API spin_proviso: public proviso
  {
  public:
    bool expand_new_state(int n, bool expanded,
			  twa_succ_iterator*, seen_map&,
			  const state*, const const_twa_ptr&);

    bool expand_src_closingedge(int, int dst);

    bool expand_before_pop(int n, twa_succ_iterator*, seen_map&);

    virtual ~spin_proviso();
  private:
    std::unordered_map<int, bool> on_dfs_;
  };

  // ----------------------------------------------------------------------
  // \brief Implementation of the optimized cycle proviso of SPIN. All states
  // with a backedge are expanded except one where destination is already
  // expanded.
  class SPOT_API source_proviso: public proviso
  {
  public:
    bool expand_new_state(int n, bool expanded,
			  twa_succ_iterator*, seen_map&,
			  const state*, const const_twa_ptr&);

    bool expand_src_closingedge(int, int dst);

    bool expand_before_pop(int n, twa_succ_iterator*, seen_map&);

    ~source_proviso();
  private:
    std::unordered_map<int, bool> on_dfs_;
  };

  // ----------------------------------------------------------------------
  // \brief This cycle proviso always expand the destination if the destination
  // is on the DFS. This proviso is the dual of the stack proviso. Note that
  // if the source is already expanded the destination is not marked as to
  // be expanded.
  class SPOT_API destination_proviso: public proviso
  {
  public:
    bool expand_new_state(int n, bool expanded,
			  twa_succ_iterator*, seen_map&,
			  const state*, const const_twa_ptr&);

    // Here we detect backegdes to mark the destination as to be expanded
    bool expand_src_closingedge(int, int dst);

    bool expand_before_pop(int n, twa_succ_iterator*, seen_map&);

    virtual ~destination_proviso();

  private:
    // Store foreach state wether it must be expanded or not
    std::unordered_map<int, bool> on_dfs_;
    std::vector<bool> expanded_;
  };


  // \brief this cycle proviso expand randomly one state between the source and
  // the destinationn iff they are both on DFS stack.
  class SPOT_API rnd_sd_proviso: public proviso
  {
  public:
    bool expand_new_state(int n, bool expanded,
			  twa_succ_iterator*, seen_map&,
			  const state*, const const_twa_ptr&);

    // Here we detect backegdes to mark the destination as to be expanded
    bool expand_src_closingedge(int, int dst);

    bool expand_before_pop(int n, twa_succ_iterator*, seen_map&);

    virtual ~rnd_sd_proviso();

  private:
    // Store foreach state wether it must be expanded or not
    struct dfs_element{
      unsigned long  pos;
      bool to_be_expanded;
      bool expanded;
    };
    std::unordered_map<int, dfs_element> on_dfs_;
  };

  // ----------------------------------------------------------------------
  // \brief This proviso always expand the destination but uses color
  // to avoid useless firing.
  class SPOT_API delayed_proviso: public proviso
  {
  public:

    bool expand_new_state(int n, bool expanded, twa_succ_iterator*,
			  seen_map&, const state*, const const_twa_ptr&);

    bool expand_src_closingedge(int src, int dst);

    bool expand_before_pop(int n, twa_succ_iterator*, seen_map&);

    virtual ~delayed_proviso();

  private:
    // FIXME duplicated with dfs exploration, must be passed by reference?
    std::vector<int> todo_;
    std::unordered_map<int, bool> reach_;
    struct dfs_element
    {
      bool expanded;
      bool to_be_expanded;
    };
    std::unordered_map<int, dfs_element> on_dfs_;
  };

  // \brief the above delayed proviso but ignoring dead states.
  // This implies that we compute SCCs here
  class SPOT_API delayed_proviso_dead: public proviso
  {
  public:

    bool expand_new_state(int n, bool expanded, twa_succ_iterator*,
			  seen_map&, const state*, const const_twa_ptr&);

    bool expand_src_closingedge(int src, int dst);

    bool expand_before_pop(int n, twa_succ_iterator*, seen_map&);

    virtual ~delayed_proviso_dead();

  private:
    // FIXME duplicated with dfs exploration, must be passed by reference?
    std::vector<int> todo_;

    struct root_element
    {
      int n;
      std::vector<int> same_scc;
    };
    std::vector<root_element> root_stack_;
    struct reach_element
    {
      bool nongreen;
      bool dead;
    };
    std::unordered_map<int, reach_element> reach_;
    struct dfs_element
    {
      bool expanded;
      bool to_be_expanded;
    };
    std::unordered_map<int, dfs_element> on_dfs_;
  };


  // This proviso is the one described by Evangelista (2007) in
  // "Some Solutions to the Ignoring Problem"
  class SPOT_API color_proviso: public proviso
  {
  public:

    color_proviso();

    bool is_cl2l_persistent_set(int n, twa_succ_iterator* it, seen_map& map);

    bool expand_new_state(int n, bool expanded, twa_succ_iterator* it,
			  seen_map& map, const state*, const const_twa_ptr&);

    bool expand_src_closingedge(int src, int dst);

    bool expand_before_pop(int n, twa_succ_iterator* it, seen_map& map);

    virtual ~color_proviso();

  private:
    enum class color
    {
      Orange,
      Green,
      Red
    };
    struct element
    {
      bool in_stack;
      spot::color_proviso::color color;
      int  expanded;
    };
    int expanded_;
    std::unordered_map<int, element> reach_;
    std::vector<bool> st_expanded_;
  };

  // ----------------------------------------------------------------------
  // This proviso is the one described by Evangelista (2007) in
  // "Some Solutions to the Ignoring Problem"
  class SPOT_API color_proviso_dead: public proviso
  {
  public:

    color_proviso_dead();

    bool is_cl2l_persistent_set(int n, twa_succ_iterator* it, seen_map& map);

    bool expand_new_state(int n, bool expanded, twa_succ_iterator* it,
			  seen_map& map, const state*, const const_twa_ptr&);

    bool expand_src_closingedge(int src, int dst);

    bool expand_before_pop(int n, twa_succ_iterator* it, seen_map& map);

    virtual ~color_proviso_dead();

  private:
    enum class color
    {
      Orange,
      Green,
      Red
    };
    struct element
    {
      bool in_stack;
      spot::color_proviso_dead::color color;
      int  expanded;
      bool dead;
    };
    int expanded_;
    std::unordered_map<int, element> reach_;
    struct root_element
    {
      int n;
      std::vector<int> same_scc;
    };
    std::vector<root_element> root_stack_;
    std::vector<bool> st_expanded_;
  };

  // ----------------------------------------------------------------------
  // \brief expand the state with the smallest number of reduced
  // successors.
  class SPOT_API min_succ_sd_proviso: public proviso
  {
  public:

    bool expand_new_state(int n, bool expanded, twa_succ_iterator* it,
			  seen_map&, const state*, const const_twa_ptr&);

    bool expand_src_closingedge(int src, int dst);

    bool expand_before_pop(int n, twa_succ_iterator*, seen_map&);

    virtual ~min_succ_sd_proviso();

  private:
    struct dfs_element
    {
      int nb_succ;
      bool expanded;
      bool to_be_expanded;
    };
    std::unordered_map<int, dfs_element> on_dfs_;
  };

  // \brief expand the state with the smallest number of reduced
  // successors.
  class SPOT_API max_succ_sd_proviso: public proviso
  {
  public:

    bool expand_new_state(int n, bool expanded, twa_succ_iterator* it,
			  seen_map&, const state*, const const_twa_ptr&);

    bool expand_src_closingedge(int src, int dst);

    bool expand_before_pop(int n, twa_succ_iterator*, seen_map&);

    virtual ~max_succ_sd_proviso();

  private:
    struct dfs_element
    {
      int nb_succ;
      bool expanded;
      bool to_be_expanded;
    };
    std::unordered_map<int, dfs_element> on_dfs_;
  };
}
