// -*- coding: utf-8 -*-
// Copyright (C) 2019 Laboratoire de Recherche et Developpement de l'Epita
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

#include <iostream> // TODO remove

#include <vector>
#include <unordered_set>

#include <spot/kripke/kripke.hh>
#include <spot/mc/mpi/mc_mpi.hh>
#include <spot/mc/mpi/spot_mpi.hh>
#include <spot/mc/mpi/mpi_window.hh>

namespace spot
{
  template<typename State, typename SuccIterator, typename StateHash>
  class SPOT_API dfs_cep
  {
    const int BUFFER_SIZE = 8;
    const int STATE_HEADER = 2;
    spot::SpotMPI mpi_;
    kripkecube<State, SuccIterator>& sys_;
    std::unordered_set<State> q_, r_;
    std::vector<std::vector<State>> sbuf_;
    spot::mpi_window win_free_;
    std::vector<spot::mpi_window> win_buf_;

    size_t state_size_;

  public:
    dfs_cep(kripkecube<State, SuccIterator>& sys) : sys_(sys), mpi_()
    {

    }

    virtual ~dfs_cep()
    {
    }

    State setup()
    {
      State s0 = sys_.initial(/* mpi_.world_rank */0);
      state_size_ = s0[1] + STATE_HEADER;
      sbuf_.resize(mpi_.world_size);
      win_free_.init(mpi_.world_size);
      win_buf_.resize(mpi_.world_size);
      for (int rank = 0; rank < mpi_.world_size; ++rank)
        win_buf_[rank].init(BUFFER_SIZE * state_size_);
      return s0;
    }

    void finalize()
    {
    }

    bool termination()
    {
      return false;
    }

    bool incoming_states()
    {
      return !win_free_.get(mpi_.world_rank, mpi_.world_rank);
    }

    std::vector<State> get_states(mpi_window& win, int rank, int index, int size)
    {
        std::vector<int> raw = win.get(rank, 0, size * state_size_);
        std::vector<State> values(size);
        for (int i = 0; i < size; ++i)
            values[i] = raw.data() + (i * state_size_);
        return values;
    }

    void put_states(mpi_window& win, int rank, int index, std::vector<State> states, int size)
    {
        std::vector<int> raw(states.size() * state_size_);
        for (int i = 0; i < states.size(); ++i)
            for (int u = 0; u < state_size_; ++u)
                raw[i * state_size_ + u] = states[i][u];
        win.put(rank, index, raw, size * state_size_);
    }

    void process_in_states()
    {
      //std::cout << "process_in_states()" << std::endl;
      for (int rank = 0; rank < mpi_.world_size; ++rank)
      {
        if (rank == mpi_.world_rank)
          continue;
        std::vector<State> buf = get_states(win_buf_[rank], mpi_.world_rank, 0, BUFFER_SIZE);
        if (!buf[0][1])
          continue;
        win_free_.put(rank, mpi_.world_rank, true);
        for (int i = 0; i < BUFFER_SIZE && buf[i][1]; ++i)
          if (r_.find(buf[i]) == r_.end())
          {
            q_.insert(buf[i]);
            r_.insert(buf[i]);
          }
        // should I clear the buffer ?
        win_buf_[rank].put(mpi_.world_rank, 0, { 0, 0 }, 2);
      }
    }

    void flush_out_buffer(int rank)
    {
      //std::cout << "flush_out_buffer(" << rank << ")" << std::endl;
      while (!win_free_.get(mpi_.world_rank, rank))
        process_in_states();
      //std::cout << "flush_out_buffer(" << rank << ")" << std::endl;
      win_free_.put(mpi_.world_rank, rank, false);
      put_states(win_buf_[mpi_.world_rank], rank, 0, sbuf_[rank], BUFFER_SIZE);
      sbuf_[rank].clear();
    }

    void flush_out_buffers()
    {
      for (int rank = 0; rank < mpi_.world_size; ++rank)
        if (rank != mpi_.world_rank && sbuf_[rank].size())
          flush_out_buffer(rank);
    }

    void explore(State s0)
    {
      StateHash hash;
      size_t s0_hash = hash(s0);
      if (s0_hash % mpi_.world_size == mpi_.world_rank)
      {
        q_.insert(s0);
        r_.insert(s0);
      }
      while (!termination())
      {
        if (incoming_states() || true) //TODO improve incoming_states
          process_in_states();
        if (q_.empty())
          flush_out_buffers();
        else
          process_queue();
      }
    }

    void process_out_state(int rank, State state)
    {
      sbuf_[rank].push_back(state);
      if (sbuf_[rank].size() == BUFFER_SIZE)
        flush_out_buffer(rank);
    }

    bool check_invariant(State state)
    {
        //std::cout << "check_invariant(" << state[0] << ", " << state[1] << ")" << std::endl;
        std::cout << state[0] << state << std::endl;
        return true;
    }

    void process_queue()
    {
      State s = *q_.begin();
      q_.erase(s);
      auto i = sys_.succ(s, /* mpi_.world_rank */0);
      for (; !i->done(); i->next())
      {
        State ns = i->state();
        StateHash hash;
        size_t ns_hash = hash(ns);
        if (!check_invariant(ns))
          mpi_.abort(1);
        else if (ns_hash % mpi_.world_size != mpi_.world_rank)
          process_out_state(ns_hash % mpi_.world_size, ns);
        else if (r_.find(ns) == r_.end())
        {
          q_.insert(ns);
          r_.insert(ns);
        }
      }
    }

    void run()
    {
      State s0 = setup();
      explore(s0);
      finalize();
    }
  };
}
