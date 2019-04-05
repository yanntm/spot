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
#include <unordered_set>

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
    const int BUFFER_SIZE = 16;
    const int STATE_HEADER_SIZE = 2;
    const int CHECK_ALL_BUFFERS = 1;

    SpotMPI mpi_;
    mpi_window win_free_;
    std::vector<mpi_window> win_buf_;

    kripkecube<State, SuccIterator>& sys_;
    std::unordered_set<State, StateHash, StateEqual> q_, r_;
    std::vector<std::vector<std::vector<int>>> sbuf_;
    std::vector<size_t> sbuf_size_;

    size_t state_size_;
    size_t processed_states_;

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

        sbuf_.resize(mpi_.world_size,
                     std::vector<std::vector<int>>(BUFFER_SIZE,
                     std::vector<int>(state_size_)));
        sbuf_size_.resize(mpi_.world_size);

        win_free_.init(mpi_.world_size, true);
        win_buf_.resize(mpi_.world_size);
        for (int rank = 0; rank < mpi_.world_size; ++rank)
          win_buf_[rank].init(BUFFER_SIZE * state_size_, 0);

        return s0;
      }

      size_t get_hash(State state)
      {
        StateHash hash;
        return hash(state);
      }

      bool termination()
      {
        return false;
      }

      bool incoming_states()
      {
        return true;
        /* FIXME
        if (processed_states_ % CHECK_ALL_BUFFERS)
          return false;
        for (int rank = 0; rank < mpi_.world_size; ++rank)
        {
          if (rank == mpi_.world_rank)
            continue;
          if (win_buf_[rank].get(mpi_.world_rank, STATE_HEADER_SIZE - 1))
            return true;
        }
        return false; */
      }

      void explore(State s0)
      {
        size_t s0_hash = get_hash(s0);
        if (s0_hash % mpi_.world_size == mpi_.world_rank)
        {
          q_.insert(s0);
          r_.insert(s0);
        }
        while (!termination())
        {
          if (incoming_states())
            process_in_states();
          if (q_.empty())
            flush_out_buffers();
          else
            process_queue();
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
        std::cout << "send " << next[3] << next[4] << std::endl;
        for (int i = 0; i < state_size_; ++i)
          sbuf_[target][sbuf_size_[target]][i] = next[i];
        ++sbuf_size_[target];
        if (sbuf_size_[target] == BUFFER_SIZE)
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
        win_free_.put(mpi_.world_rank, target, true);
        for (int i = 0; i < sbuf_size_[target]; ++i)
          win_buf_[mpi_.world_rank].put(target, i * state_size_,
                                        sbuf_[target][i], state_size_);
        sbuf_size_[target] = 0;
      }

      void process_in_states()
      {
        for (int rank = 0; rank < mpi_.world_size; ++rank)
        {
          if (rank == mpi_.world_rank)
            continue;
          std::vector<int> buf = win_buf_[rank].get(mpi_.world_rank, 0,
                                                    BUFFER_SIZE * state_size_);
          if (!buf[STATE_HEADER_SIZE - 1])
            continue;
          win_buf_[rank].put(mpi_.world_rank, 1, { 0 }, 1);
          win_free_.put(rank, mpi_.world_rank, true);
          for (int i = 0; i < BUFFER_SIZE && buf[i * state_size_
                                                 + STATE_HEADER_SIZE - 1]; ++i)
          {
            int *state = (int *) malloc(state_size_ * sizeof (int));
            for (int u = 0; u < state_size_; ++u)
              state[u] = buf[i * state_size_ + u];
            std::cout << "\t\trecv " << state[3] << state[4] << std::endl;
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

      void print_visited()
      {
        std::cout << "[";
        for (State state : r_)
          std::cout << state[3] << state[4] << ", ";
        std::cout << "]" << std::endl;
      }

      bool check_invariant(State state)
      {
        std::cout << r_.size() << " -> ";
        print_visited();
        ++processed_states_;
        return true;
      }

      void run()
      {
        State s0 = setup();
        check_invariant(s0);
        explore(s0);
        std::cout << "unique state explored : " << r_.size() << "\n";
        finalize();
      }
  };
}
