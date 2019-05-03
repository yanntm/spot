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

#include <numeric>
#include <chrono>
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
  class SPOT_API dfs_sync
  {
    const int STATE_HEADER = 2;

    spot::SpotMPI mpi_;
    kripkecube<State, SuccIterator>& sys_;
    std::unordered_set<State, StateHash, StateEqual> q_, r_;

    bool has_finish;
    mpi_window win_finish;
    std::chrono::time_point<std::chrono::steady_clock> finish_time
      = std::chrono::time_point<std::chrono::steady_clock>::max();

    size_t state_size_;

  public:
    dfs_sync(kripkecube<State, SuccIterator>& sys) : sys_(sys), mpi_()
    {

    }

    virtual ~dfs_sync()
    {
    }

    State setup()
    {
      State s0 = sys_.initial(0);
      state_size_ = s0[1] + STATE_HEADER;

      win_finish.init(mpi_.world_size, false);
      has_finish = false;

      return s0;
    }

    void finalize()
    {
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
        win_finish.put(0, mpi_.world_rank, 1);
        has_finish = true;
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

    void explore(State s0)
    {
      StateHash hash;
      size_t s0_hash = hash(s0);
      if (s0_hash % mpi_.world_size == mpi_.world_rank)
      {
        q_.insert(s0);
        r_.insert(s0);
      }
      bool work = true;
      while (work || !termination())
      {
        work = false;
        int state_received;
        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &state_received, &status);
        if (state_received)
        {
          work = true;
          int* state = (int *) malloc(state_size_ * sizeof (int));
          MPI_Recv(state, state_size_, MPI_INT, status.MPI_SOURCE, 0, 
                   MPI_COMM_WORLD, MPI_STATUS_IGNORE);
          if (r_.find(state) == r_.end())
          {
            q_.insert(state);
            r_.insert(state);
          }
        }
        if (!q_.empty())
        {
          work = true;
          process_queue();
        }
        if (work)
        {
          if (has_finish)
          {
            has_finish = false;
            win_finish.put(0, mpi_.world_rank, 0);
          }
          finish_time = std::chrono::time_point<std::chrono::steady_clock>
                        ::max();
        }
      }
    }

    bool check_invariant(State state)
    {
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
          MPI_Send(ns, state_size_, MPI_INT, ns_hash % mpi_.world_size, 0,
                   MPI_COMM_WORLD);
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
      check_invariant(s0);
      explore(s0);
      std::cout << "unique state explored: " << r_.size() << "\n";
      finalize();
    }
  };
}
