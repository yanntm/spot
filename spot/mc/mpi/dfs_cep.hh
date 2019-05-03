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

#include <iostream>
#include <vector>
#include <chrono>
#include <unordered_set>
#include <numeric>

#include <spot/kripke/kripke.hh>
#include <spot/mc/mpi/mc_mpi.hh>
#include <spot/mc/mpi/spot_mpi.hh>
#include <spot/mc/mpi/mpi_window.hh>

namespace spot
{
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class SPOT_API dfs_cep
  {
    const int STATE_HEADER_SIZE = 2;
    const int MTU = 1500;

    const int CHECK_ALL_BUFFERS = 10000;

    SpotMPI mpi_;
    mpi_window win_free_;
    std::vector<mpi_window> win_buf_;

    kripkecube<State, SuccIterator>& sys_;
    std::unordered_set<State, StateHash, StateEqual> q_, r_;
    std::vector<std::vector<std::vector<int>>> sbuf_;
    std::vector<size_t> sbuf_size_;

    size_t state_size_;
    int buffer_size_;
    size_t processed_states_;

    bool has_finish;
    mpi_window win_finish;
    std::chrono::time_point<std::chrono::steady_clock> finish_time
      = std::chrono::time_point<std::chrono::steady_clock>::max();

    std::vector<int> local_buf_;

    public:
      dfs_cep(kripkecube<State, SuccIterator>& sys)
        : sys_(sys),
          mpi_(),
          processed_states_(0)
      { }

      virtual ~dfs_cep() { }

      State setup()
      {
        State s0 = sys_.initial(0);
        state_size_ = s0[STATE_HEADER_SIZE - 1] + STATE_HEADER_SIZE;
        buffer_size_ = MTU / (state_size_ * sizeof (int));

        sbuf_.resize(mpi_.world_size,
                     std::vector<std::vector<int>>(buffer_size_,
                     std::vector<int>(state_size_)));
        sbuf_size_.resize(mpi_.world_size);
        has_finish = false;

        win_free_.init(mpi_.world_size, true);
        win_buf_.resize(mpi_.world_size);
        for (int rank = 0; rank < mpi_.world_size; ++rank)
          win_buf_[rank].init(buffer_size_ * state_size_, 0);

        win_finish.init(mpi_.world_size, false);

        local_buf_ = std::vector<int>(buffer_size_ * state_size_);

        return s0;
      }

      size_t get_hash(State state)
      {
        StateHash hash;
        return hash(state);
      }

      bool termination()
      {
        auto current = std::chrono::steady_clock::now();
        if (finish_time > current)
        {
          finish_time = current;
          return false;
        }
        auto milli = std::chrono::duration_cast<std::chrono::milliseconds>
          (current - finish_time).count();
        if (milli > 100)
        {
          has_finish = true;
          win_finish.put(0, mpi_.world_rank, true);
          if (mpi_.world_rank == 0)
          {
            std::vector<int> f(mpi_.world_size, 0);
            win_finish.get(0, 0, f);
            if (std::accumulate(f.cbegin(), f.cend(), 0) == mpi_.world_size)
            {
              for (int i = 1; i < mpi_.world_size; ++i)
                win_finish.put(i, 0, true);
              return true;
            }
            return false;
          }
          return win_finish.get(mpi_.world_rank, 0);
        }
        return false;
      }

      bool incoming_states(bool work)
      {
        if (processed_states_ % CHECK_ALL_BUFFERS && work)
          return false;
        for (int rank = 0; rank < mpi_.world_size; ++rank)
        {
          if (rank == mpi_.world_rank)
            continue;
          if (win_buf_[rank].get(mpi_.world_rank, STATE_HEADER_SIZE - 1))
            return true;
        }
        return false;
      }

      void explore(State s0)
      {
        size_t s0_hash = get_hash(s0);
        if (s0_hash % mpi_.world_size == mpi_.world_rank)
        {
          q_.insert(s0);
          r_.insert(s0);
        }
        bool work = true;
        while (work || !termination())
        {
          if (incoming_states(work))
            process_in_states();
          work = !q_.empty();
          if (q_.empty())
            flush_out_buffers();
          else
            process_queue();
          if (work)
          {
            if (has_finish)
            {
              win_finish.put(0, mpi_.world_rank, false);
              has_finish = false;
            }
            finish_time = std::chrono::time_point<std::chrono::steady_clock>
              ::max();
          }
        }
      }

      void process_queue()
      {
        State s = *q_.begin();
        q_.erase(s);
        auto i = sys_.succ(s, 0);
        for (; !i->done(); i->next())
        {
          State next = i->state();
          size_t hash = get_hash(next);
          if (!check_invariant(next))
            mpi_.abort(1);
          else if (hash % mpi_.world_size != mpi_.world_rank)
            process_out_state(hash % mpi_.world_size, next);
          else if (r_.find(next) == r_.end())
          {
            q_.insert(next);
            r_.insert(next);
          }
        }
      }

      void process_out_state(int target, State next)
      {
        for (int i = 0; i < state_size_; ++i)
          sbuf_[target][sbuf_size_[target]][i] = next[i];
        ++sbuf_size_[target];
        if (sbuf_size_[target] == buffer_size_)
          flush_out_buffer(target);
      }

      void flush_out_buffers()
      {
        for (int rank = 0; rank < mpi_.world_size; ++rank)
          if (rank != mpi_.world_rank && sbuf_size_[rank])
            flush_out_buffer(rank);
      }

      void flush_out_buffer(int target)
      {
        while (!win_free_.get(mpi_.world_rank, target))
          process_in_states();
        win_free_.put(mpi_.world_rank, target, false);
        std::vector<int> out_buf(state_size_ * sbuf_size_[target]);
        for (int i = 0; i < sbuf_size_[target]; ++i)
        {
          for (int u = 0; u < state_size_; ++u)
            out_buf[i * state_size_ + u] = sbuf_[target][i][u];
        }
        win_buf_[mpi_.world_rank].put(target, 0, out_buf,
                                      sbuf_size_[target] * state_size_);
        sbuf_size_[target] = 0;
      }

      void process_in_states()
      {
        for (int rank = 0; rank < mpi_.world_size; ++rank)
        {
          if (rank == mpi_.world_rank)
            continue;
          win_buf_[rank].get(mpi_.world_rank, 0, local_buf_);
          if (!local_buf_[STATE_HEADER_SIZE - 1])
            continue;
          win_buf_[rank].put(mpi_.world_rank, 1, 0);
          win_free_.put(rank, mpi_.world_rank, true);
          for (int i = 0; i < buffer_size_; ++i)
          {
            if (!local_buf_[i * state_size_ + STATE_HEADER_SIZE - 1])
              break;
            int *state = (int *) malloc(state_size_ * sizeof (int));
            for (int u = 0; u < state_size_; ++u)
              state[u] = local_buf_[i * state_size_ + u];
            if (r_.find(state) == r_.end())
            {
              q_.insert(state);
              r_.insert(state);
            }
          }
        }
      }

      void finalize()
      {
      }

      bool check_invariant(State state)
      {
        ++processed_states_;
        return true;
      }

      void run()
      {
        State s0 = setup();
        check_invariant(s0);
        explore(s0);
        std::cout << "unique state explored: " << r_.size() << "\n";
        finalize();
      }
  };
}
