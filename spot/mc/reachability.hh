// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016 Laboratoire de Recherche et
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

#include <functional>
#include <spot/kripke/kripke.hh>

namespace spot
{
  /// \brief This template class provide a sequential reachability
  /// of a kripkecube. The algorithm uses a single DFS since it
  /// is the most efficient in a sequential setting
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual,
           typename Visitor>
  class SPOT_API seq_reach_kripke
  {
  public:
    seq_reach_kripke(kripkecube<State, SuccIterator>& sys, unsigned tid,
                     bool& stop):
      sys_(sys), tid_(tid), stop_(stop)
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
      visited.reserve(2000000);
      todo.reserve(100000);
    }

    ~seq_reach_kripke()
      {
        // States will be destroyed by the system, so just clear map
        visited.clear();
      }

    Visitor& self()
    {
      return static_cast<Visitor&>(*this);
    }

    void run()
    {
      self().setup();
      State initial = sys_.initial(tid_);
      if (SPOT_LIKELY(self().push(initial, dfs_number)))
        {
          todo.push_back({initial, sys_.succ(initial, tid_)});
          visited[initial] = ++dfs_number;
        }
      while (!todo.empty() && !stop_)
        {
          if (todo.back().it->done())
            {
              if (SPOT_LIKELY(self().pop(todo.back().s)))
                {
                  sys_.recycle(todo.back().it, tid_);
                  todo.pop_back();
                }
            }
          else
            {
              ++transitions;
              State dst = todo.back().it->state();
              auto it  = visited.insert({dst, dfs_number+1});
              if (it.second)
                {
                  ++dfs_number;
                  if (SPOT_LIKELY(self().push(dst, dfs_number)))
                    {
                      todo.back().it->next();
                      todo.push_back({dst, sys_.succ(dst, tid_)});
                    }
                }
              else
                {
                  self().edge(visited[todo.back().s], visited[dst]);
                  todo.back().it->next();
                }
            }
        }
      self().finalize();
    }

    unsigned int states()
    {
      return dfs_number;
    }

    unsigned int trans()
    {
      return transitions;
    }

  protected:
    struct todo_element
    {
      State s;
      SuccIterator* it;
    };
    kripkecube<State, SuccIterator>& sys_;
    std::vector<todo_element> todo;
    // FIXME: The system already handle a set of visited states so
    // this map is redundant: an we avoid this new map?
    typedef std::unordered_map<const State, int,
                               StateHash, StateEqual> visited_map;
    visited_map visited;
    unsigned int dfs_number = 0;
    unsigned int transitions = 0;
    unsigned int tid_;
    bool& stop_; // Do not need to be atomic.
  };
}
