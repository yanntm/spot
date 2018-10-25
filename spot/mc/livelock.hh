// -*- coding: utf-8 -*-
// Copyright (C) 2018 Laboratoire de Recherche et Developpement de l'Epita
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
#include <set>
#include <spot/misc/common.hh>
#include <spot/kripke/kripke.hh>
#include <spot/misc/fixpool.hh>
#include <spot/misc/timer.hh>
#include <spot/mc/shared_queue.hh>

namespace spot
{
  /// \brief This object is returned by the algorithm below
  struct SPOT_API livelock_stats
  {
    unsigned states;            ///< \brief Number of states visited
    unsigned transitions;       ///< \brief Number of transitions visited
    unsigned instack_dfs;       ///< \brief Maximum DFS stack
    bool has_livelock;          ///< \brief Does the model contains a livelock
    unsigned walltime;          ///< \brief Walltime for this thread in ms
  };

  /// \brief This class aims to explore a model to detect wether it
  /// contains a livelock. This livelock detection performs a DFS traversal
  /// sharing information shared among multiple threads.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_livelock
  {
    /// \brief Describes the status of a state
    enum st_status
      {
        UNKNOWN = 1,    // First time this state is discoverd by this thread
				FRONTIER = 2,   // The state is in the frontier
        VISITED = 4,    // The state has already been visited
      };

    /// \brief Describes the structure of a shared state
    struct livelock_pair
    {
      State st;                 ///< \brief the effective state
      int* colors;              ///< \brief the colors (one)
    };

    /// \brief The haser for the previous state.
    struct pair_hasher
    {
      pair_hasher(const livelock_pair&)
      { }

      pair_hasher() = default;

      brick::hash::hash128_t
      hash(const livelock_pair& lhs) const
      {
        StateHash hash;
        // Not modulo 31 according to brick::hashset specifications.
        unsigned u = hash(lhs.st) % (1<<30);
        return {u, u};
      }

      bool equal(const livelock_pair& lhs,
                 const livelock_pair& rhs) const
      {
        StateEqual equal;
        return equal(lhs.st, rhs.st);
      }
    };

  public:

		static void dump_state(State s)
		{
			int* sp = (int*) s;
			std::cout << "State " << sp << " :\n";
			std::cout << "  [0] (hash) = " << sp[0] << '\n';
			std::cout << "  [1] (size) = " << sp[1] << '\n';

			for (int i = 0; i < sp[1]; i++)
			  std::cout << "  [" << i + 2 << "]        = " << sp[i + 2] << '\n';
		}

    ///< \brief Shortcut to ease shared map manipulation
    using shared_map = brick::hashset::FastConcurrent <livelock_pair,
                                                       pair_hasher>;
		using shared_queue = s_queue<State>;

    swarmed_livelock(kripkecube<State, SuccIterator>& sys,
                     shared_map& map, shared_queue& frontier, unsigned tid,
										 std::atomic<bool>& stop):
      sys_(sys), tid_(tid), map_(map), frontier_(frontier),
      nb_th_(std::thread::hardware_concurrency()),
      p_(sizeof(int) * std::thread::hardware_concurrency()), stop_(stop)
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    virtual ~swarmed_livelock()
    {
/*      while (!todo_.empty())
        {
          sys_.recycle(todo_.back().it, tid_);
          todo_.pop_back(); TODO
        }
				*/
    }

    void setup()
    {
      tm_.start("DFS thread " + std::to_string(tid_));
    }

		void push_v(State s)
		{
      // Prepare data for a newer allocation
      int* ref = (int*) p_.allocate();
      for (unsigned i = 0; i < nb_th_; ++i)
        ref[i] = UNKNOWN;

      // Try to insert the new state in the shared map.
      auto it = map_.insert({s, ref});
      bool b = it.isnew();

      // Insertion failed, delete element
      // FIXME Should we add a local cache to avoid useless allocations?
      if (!b)
        p_.deallocate(ref);
			else
 			  ++states_;

			// Mark state visited by the current thread
 		  it->colors[tid_] |= VISITED;
		}

		void push_f(State s)
		{
      // Prepare data for a newer allocation
      int* ref = (int*) p_.allocate();
      for (unsigned i = 0; i < nb_th_; ++i)
        ref[i] = UNKNOWN;

      // Try to insert the new state in the shared map.
      auto it = map_.insert({s, ref});
      bool b = it.isnew();

      // Insertion failed, delete element
      // FIXME Should we add a local cache to avoid useless allocations?
      if (!b)
        p_.deallocate(ref);
			else
 			  ++states_;
			    
			// Mark state as in frontier.
 		  it->colors[tid_] |= FRONTIER;

			// if (!is_in_frontier(s)) TODO is the if useful ?
				frontier_.push(s);
		}

		bool is_in(State s, enum st_status set)
		{
			auto it = map_.template find<livelock_pair>({s, nullptr});

			if (!it.valid())
				return false;

			for (unsigned i = 0; i < nb_th_; ++i)
			  if (it->colors[i] & static_cast<int>(set))
					return true;
			
			return false;
		}

		bool is_visited(State s)
		{
			return is_in(s, VISITED);
		}

		bool is_in_frontier(State s)
		{
			return is_in(s, FRONTIER);
		}

		// Pick a state from the frontier queue, check if it is alredy visited
		// If yes, remove it from the queue and loop until a non visited is found
		// Else return it
		//
		// Return null if the queue is empty
		State pick_f_unvisited()
		{
			State s;

			do
				{
					s = frontier_.pop();
					if (!s)
						return nullptr;
				}
			while (is_visited(s));

			return s;
		}

		// Remove the state s from the frontier set
		void remove_f(State s)
		{
			auto it = map_.insert({s, nullptr});

			SPOT_ASSERT(!it.isnew());

     	it->colors[tid_] &= ~static_cast<int>(FRONTIER);
		}

    void finalize()
    {
      stop_ = true;
      tm_.stop("DFS thread " + std::to_string(tid_));
    }

    unsigned states()
    {
      return states_;
    }

    unsigned transitions()
    {
      return transitions_;
    }
			
    void run()
    {
      setup();
      State initial = sys_.initial(tid_);

			push_f(initial);

			State s;
			while ((s = pick_f_unvisited()) && !stop_.load(std::memory_order_relaxed))
			{
				if (dfs(s))
					break;
				remove_f(s);	
			}

      finalize();
    }

		static int get_state_hash(State s)
		{
			StateHash sh;
			return sh(s);
			//return (*reinterpret_cast<StateHash*>(&s))(); // TODO FIXME
			//return * ((int*) s);
		}

		bool dfs(State s)
		{
			auto e = todo__element({s, sys_.succ(s, tid_), transitions_}); 
			todo_.insert(get_state_hash(s));

			while (!e.it->done() && !stop_.load(std::memory_order_relaxed))
			{
				++transitions_;
				if (todo_.find(get_state_hash(e.it->state())) != todo_.end() &&
				    !sys_.is_progress_tr(e.it->get_transition_id()))
				{
					livelock_ = true;
					return true;
				}
				else if (!is_visited(e.it->state()))
				{
					if (!(sys_.is_progress_tr(e.it->get_transition_id())))
					{
						if (dfs(e.it->state()))
							return true;
					}
					else
						push_f(e.it->state());
				}
				e.it->next();
			}
			push_v(s);
			todo_.erase(get_state_hash(s));
			return false;
		}

    bool has_livelock()
    {
			std::cout << "has_livelock() : livelock_ = " << livelock_ << std::endl;
      return livelock_;
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(tid_)).walltime();
    }

    livelock_stats stats()
    {
      return {states(), transitions(), dfs_, has_livelock(), walltime()};
    }

  private:
    struct todo__element
    {
      State s;
      SuccIterator* it;
      unsigned current_tr;
    };

    kripkecube<State, SuccIterator>& sys_; ///< \brief The system to check
    std::set<int> todo_;      ///< \brief The DFS stack (stores hashs)
    unsigned transitions_ = 0;         ///< \brief Number of transitions
    unsigned tid_;                     ///< \brief Thread's current ID
    shared_map map_;                       ///< \brief Map shared by threads
    shared_queue& frontier_;                  ///< \brief Frontier queue
    spot::timer_map tm_;                   ///< \brief Time execution
    unsigned states_ = 0;                  ///< \brief Number of states
    unsigned dfs_ = 0;                     ///< \brief Maximum DFS stack size
    /// \brief Maximum number of threads that can be handled by this algorithm
    unsigned nb_th_ = 0;
    fixed_size_pool<pool_type::Unsafe> p_;  ///< \brief State Allocator
    bool livelock_ = false;                ///< \brief livelock detected?
    std::atomic<bool>& stop_;              ///< \brief Stop-the-world boolean
  };
}
