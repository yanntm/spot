// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016, 2017 Laboratoire de Recherche et
// Developpement de l'Epita
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

#include <atomic>
#include <chrono>
#include <spot/bricks/brick-hashset>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <spot/misc/common.hh>
#include <spot/kripke/kripke.hh>
#include <spot/misc/fixpool.hh>
#include <spot/misc/timer.hh>

namespace spot
{

  /// \brief This class print the states of the system using a DFS
  /// and one thread.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class state_printer
  {
    /// \brief Describes the status of a state
    enum st_status
      {
        UNKNOWN = 1,    // First time this state is discoverd by this thread
        OPEN = 2,       // The state is currently processed by this thread
        CLOSED = 4,     // All the successors of this state have been visited
      };

    /// \brief Describes the structure of a shared state
    struct st_pair
    {
      State st;                 ///< \brief the effective state
      int* colors;              ///< \brief the colors (one per thread)
    };

    /// \brief The haser for the previous state.
    struct pair_hasher
    {
      pair_hasher(const st_pair&)
      { }

      pair_hasher() = default;

      brick::hash::hash128_t
      hash(const st_pair& lhs) const
      {
        StateHash hash;
        // Not modulo 31 according to brick::hashset specifications.
        unsigned u = hash(lhs.st) % (1<<30);
        return {u, u};
      }

      bool equal(const st_pair& lhs,
                 const st_pair& rhs) const
      {
        StateEqual equal;
        return equal(lhs.st, rhs.st);
      }
    };

  public:

    ///< \brief Shortcut to ease shared map manipulation
    using shared_map = brick::hashset::FastConcurrent <st_pair,
                                                       pair_hasher>;

    state_printer(kripkecube<State, SuccIterator>& sys,
                     shared_map& map):
      sys_(sys), map_(map),
      p_(sizeof(int))
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    virtual ~state_printer()
    {
      while (!todo_.empty())
        {
          sys_.recycle(todo_.back().it, 0);
          todo_.pop_back();
        }
    }

    bool push(State s)
    {
      // Prepare data for a newer allocation
      int* ref = (int*) p_.allocate();
      ref[0] = UNKNOWN;

      // Try to insert the new state in the shared map.
      auto it = map_.insert({s, ref});
      bool b = it.isnew();

      // Insertion failed, delete element
      // FIXME Should we add a local cache to avoid useless allocations?
      if (!b)
        p_.deallocate(ref);

      // The state has been mark dead
      if (!b)
        if (it->colors[0] == static_cast<int>(CLOSED))
          return false;

      // The state has already been visited by the current thread
      if (it->colors[0] == static_cast<int>(OPEN))
        return false;

      // Keep a ptr over the array of colors
      refs_.push_back(it->colors);

      // Mark state as visited.
      it->colors[0] = OPEN;
      return true;
    }

    bool pop()
    {
      // Don't avoid pop but modify the status of the state
      // during backtrack
      refs_.back()[0] = CLOSED;
      refs_.pop_back();
      return true;
    }

    void run()
    {
      tm_.start("Print states");
      State initial = sys_.initial(0);
      if (SPOT_LIKELY(push(initial)))
        {
          todo_.push_back({initial, sys_.succ(initial, 0)});
          std::cout << todo_.back().it->size() << std::endl;
        }
      while (!todo_.empty())
        {
          if (todo_.back().it->done())
            {
              if (SPOT_LIKELY(pop()))
                {
                  sys_.recycle(todo_.back().it, 0);
                  todo_.pop_back();
                }
            }
          else
            {
              State dst = todo_.back().it->state();

              if (SPOT_LIKELY(push(dst)))
                {
                  todo_.back().it->next();
                  todo_.push_back({dst, sys_.succ(dst, 0)});
                  std::cout << todo_.back().it->size() << std::endl;
                }
              else
                {
                  todo_.back().it->next();
                }
            }
        }
      tm_.stop("Print states");
    }

    unsigned walltime()
    {
      return tm_.timer("Print states").walltime();
    }

  private:
    struct todo__element
    {
      State s;
      SuccIterator* it;
    };
    kripkecube<State, SuccIterator>& sys_; ///< \brief The system to check
    std::vector<todo__element> todo_;      ///< \brief The DFS stack
    shared_map map_;                       ///< \brief Map shared by threads
    spot::timer_map tm_;                   ///< \brief Time execution
    fixed_size_pool<pool_type::Unsafe> p_;  ///< \brief State Allocator
    /// \brief Stack that grows according to the todo stack. It avoid multiple
    /// concurent access to the shared map.
    std::vector<int*> refs_;
  };
}
