// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016 Laboratoire de Recherche
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

#pragma once

#include <vector>
#include <iosfwd>
#include <spot/graph/graph.hh>
#include <spot/twa/acc.hh>
#include <spot/twacube/cube.hh>

namespace spot
{
  class SPOT_API cstate
  {
  public:
    cstate() {}
    cstate(const cstate& s) = delete;
    cstate(cstate&& s) noexcept;
    cstate(unsigned int id);
    virtual ~cstate();
    unsigned int label();
  private:
    unsigned int id_;
  };

  class SPOT_API transition
  {
  public:
    cube cube_;
    acc_cond::mark_t acc_;
    transition();
    transition(const transition& t) = delete;
    transition(transition&& t) noexcept;
    transition(const cube& cube, acc_cond::mark_t acc);
    virtual ~transition();
  };

  class SPOT_API trans_index final
  {
  public:
    typedef digraph<cstate, transition> graph_t;
    typedef graph_t::edge_storage_t edge_storage_t;

    trans_index(trans_index& ci) = delete;
    trans_index(unsigned int state, graph_t& g):
      st_(g.state_storage(state))
    {
      reset();
    }

    trans_index(trans_index&& ci):
      idx_(ci.idx_),
      st_(ci.st_)
    {
    }

    inline void reset()
    {
      idx_ = st_.succ;
    }

    inline void next()
    {
      ++idx_;
    }

    inline bool done() const
    {
      return !idx_ || idx_ > st_.succ_tail;
    }

    inline unsigned int current(unsigned int seed = 0) const
    {
      // no-swarming : since twacube are dedicated for parallelism, i.e.
      // swarming, we expect swarming is activated.
      if (SPOT_UNLIKELY(!seed))
        return idx_;
      // swarming.
      return (((idx_-st_.succ+1)*seed) % (st_.succ_tail-st_.succ+1)) + st_.succ;
    }

  private:
    unsigned int idx_;
    const graph_t::state_storage_t& st_;
  };

  class SPOT_API twacube final
  {
  public:
    twacube() = delete;
    twacube(const std::vector<std::string>& aps);
    virtual ~twacube();
    const acc_cond& acc() const;
    acc_cond& acc();
    const std::vector<std::string>& get_ap();
    unsigned int new_state();
    void set_initial(unsigned int init);
    unsigned int get_initial();
    cstate* state_from_int(unsigned int i);
    void create_transition(unsigned int src, const cube& cube,
                           const acc_cond::mark_t& mark, unsigned int dst);
    const cubeset& get_cubeset() const;
    bool succ_contiguous() const;
    typedef digraph<cstate, transition> graph_t;
    const graph_t& get_graph()
    {
      return theg_;
    }
    typedef graph_t::edge_storage_t edge_storage_t;
    const graph_t::edge_storage_t&
      trans_storage(std::shared_ptr<trans_index> ci,
                    unsigned int seed = 0) const
    {
      return theg_.edge_storage(ci->current(seed));
    }
    const transition& trans_data(std::shared_ptr<trans_index> ci,
                                 unsigned int seed = 0) const
    {
      return theg_.edge_data(ci->current(seed));
    }

    std::shared_ptr<trans_index> succ(unsigned int i)
    {
      return std::make_shared<trans_index>(i, theg_);
    }

    friend SPOT_API std::ostream& operator<<(std::ostream& os,
                                             const twacube& twa);
  private:
    unsigned int init_;
    acc_cond acc_;
    const std::vector<std::string>& aps_;
    graph_t theg_;
    cubeset cubeset_;
  };
}
