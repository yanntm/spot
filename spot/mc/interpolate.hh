// -*- coding: utf-8 -*-
// Copyright (C) 2016, 2017 Laboratoire de Recherche et
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
#include <stdlib.h>
#include <spot/mc/reachability.hh>
#include <spot/misc/timer.hh>
#include <chrono>

#include <bricks/brick-hashset>
#include <atomic>

namespace spot
{
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class count_valid  : public seq_reach_kripke<State, SuccIterator,
                                       StateHash, StateEqual,
                                       count_valid<State, SuccIterator,
                                                        StateHash, StateEqual>>
  {
  public:
    count_valid(kripkecube<State, SuccIterator>& sys,
                State new_initial,
                std::function<bool(const State)> count_valid_fun,
                std::function<int(State)>  get_depth,
                std::function<int(State)>  get_pos,
                unsigned id,
                unsigned tid, bool& stop, std::string algoname)
      : seq_reach_kripke<State, SuccIterator, StateHash, StateEqual,
                         count_valid<State, SuccIterator,
                                     StateHash, StateEqual>>(sys, tid, stop),
      new_initial_(new_initial), count_valid_fun_(count_valid_fun),
      get_depth_(get_depth), get_pos_(get_pos), id_(id), algoname_(algoname)
      {
      }

    virtual ~count_valid()
    {

    }

    void setup()
    {
    }

    bool push(State st, unsigned int dfsnum)
    {
      if (dfsnum == 0)
        {
          // Here Hack the reachability to specify startup
          this->visited.erase(st);
          this->sys_.recycle(this->todo.back().it, this->tid_);
          this->todo.pop_back();
          this->todo.push_back({new_initial_,
                this->sys_.succ(new_initial_, this->tid_)});
          this->visited[new_initial_] = this->dfs_number;
          this->dfs_number = 1;
          if (count_valid_fun_(new_initial_))
            ++counter_;
          return false;
        }

      if (count_valid_fun_(st))
        {
          ++counter_;
          if (first_depth_ == -1)
            {
              first_depth_ = get_depth_(st);
              first_pos_ = get_pos_(st);
            }
        }
      return true;
    }

    bool pop(State)
    {
      return true;
    }

    void edge(unsigned int, unsigned int)
    {
    }

    void finalize()
    {
      float x = (float) counter_ / this->states();
      std::cout << '@' << id_
                << ',' << algoname_
                << ',' << first_depth_
                << ',' << first_pos_
                << ',' << x
                << ',' << this->states()
                << ',' << this->trans()
                << ',' << counter_
                << (this->stop_? ",ABORTED" : ",FINISHED");
      this->stop_ = true;
    }

  private:
    State new_initial_;
    std::function<bool(const State)> count_valid_fun_;
    std::function<int(State)> get_depth_;
    std::function<int(State)> get_pos_;
    unsigned int counter_ = 0;
    int first_depth_ = -1;
    int first_pos_ = -1;
    unsigned id_ = 0;
    std::string algoname_;
  };

  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class interpolate  : public seq_reach_kripke<State, SuccIterator,
                                       StateHash, StateEqual,
                                       interpolate<State, SuccIterator,
                                                        StateHash, StateEqual>>
  {
  public:
    interpolate(kripkecube<State, SuccIterator>& sys,
        std::function<void(State, unsigned int)> display,
        std::function<std::vector<State>*(std::vector<State>&)> interpolate_fun,
                unsigned tid, bool& stop, std::string algoname)
      : seq_reach_kripke<State, SuccIterator, StateHash, StateEqual,
                         interpolate<State, SuccIterator,
                                     StateHash, StateEqual>>(sys, tid, stop),
      display_(display), interpolate_fun_(interpolate_fun), algoname_(algoname)
      {
        (void) display;
        depth.reserve(1000);
      }

    virtual ~interpolate()
    {
    }

    void setup()
    {
      tm_.start("original DFS");
    }

    bool push(State st, unsigned int dfsnum)
    {
      if (dfsnum <= 1000) // FIXME threshold
        sample_.push_back(st);

      depth.insert({st, (int) this->todo.size()});
      dfspos.insert({st, (int) this->dfs_number});

      return true;
    }

    bool pop(State)
    {
      return true;
    }

    void edge(unsigned int, unsigned int)
    {
    }

    void finalize()
    {
      using namespace std::literals::chrono_literals;
      tm_.stop("original DFS");

      std::cout << "STATES      : " << this->states() << std::endl;
      std::cout << "TRANSITIONS : " << this->trans() << std::endl;
      std::cout << "TIME        : " << tm_.timer("original DFS").walltime()
                << std::endl;
      tm_.start("Generation of states");
      auto* gen = interpolate_fun_(sample_);
      tm_.stop("Generation of states");

      std::cout << "GenPop  : "  << tm_.timer("Generation of states").walltime()
                << std::endl;

      for (unsigned i = 0; i < gen->size(); ++i)
        {
          bool stop = false;
          //SPOT_ASSERT(gen[i] != nullptr);
          count_valid<State, SuccIterator, StateHash, StateEqual>
            cv(this->sys_, (*gen)[i], [this](State s) -> bool
               {
                 return this->visited.find(s) != this->visited.end();
               },
               [this](State s) -> int
               {
                 return depth[s];
               },
               [this](State s) -> int
               {
                 return dfspos[s];
               },
               i,
               0, /* FIXME tid */
               stop, algoname_);
          std::thread th
            (&count_valid<State, SuccIterator, StateHash, StateEqual>::run,
             cv);

          // Sleep for 10 minutes
          for (unsigned t = 0; t < 600 && !stop; ++t)
            std::this_thread::sleep_for(1s);
          stop = true;
          th.join();
          std::cout << std::endl;
        }
      delete gen;
    }

  private:
    std::function<void(State, unsigned int)> display_;
    std::function<std::vector<State>*(std::vector<State>&)> interpolate_fun_;
    std::vector<State> sample_;
    spot::timer_map tm_;
    typedef std::unordered_map<const State, int,
                               StateHash, StateEqual> visited_map;
    visited_map depth;
    visited_map dfspos;
    std::string algoname_;
  };


  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_dfs
  {
    enum st_status     // Describe the status of a state
      {
        OPEN = 1,       // The state is currently processed by this thread
        CLOSED = 2,     // All the successors of this state have been visited
	UNKNOWN = 4     // First time this state is discoverd by this thread
      };

    struct my_pair
    {
      State st;
      int* colors;
    };

    struct inner_pair_hasher
    {
      inner_pair_hasher(const my_pair&)
      { }

      inner_pair_hasher() = default;

      brick::hash::hash128_t
      hash(const my_pair& lhs) const
      {
        StateHash hash;
	// FIXME without that insert/find fail !!
	unsigned u = hash(lhs.st) % (1<<30);
	return {u, u};
      }

      bool equal(const my_pair& lhs,
                 const my_pair& rhs) const
      {
        StateEqual equal;
        return equal(lhs.st, rhs.st);
      }
    };

  public:
    using shared_map = brick::hashset::FastConcurrent <my_pair,
						       inner_pair_hasher>;

    swarmed_dfs(kripkecube<State, SuccIterator>& sys,
                shared_map& map, unsigned tid):
      sys_(sys), tid_(tid), map_(map),
      nb_th_(std::thread::hardware_concurrency()),
      p_(sizeof(int)*std::thread::hardware_concurrency())
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    virtual ~swarmed_dfs()
    {
    }

    void setup()
    {
      tm_.start("DFS thread " + std::to_string(this->tid_));
    }

    bool push(State s, unsigned int tid)
    {
      (void) tid;
      // Prepare data for a newer allocation
      int* ref = (int*) p_.allocate();
      for (unsigned i = 0; i < nb_th_; ++i)
	ref[i] = UNKNOWN;

      // Try to insert the new state in the shared map.
      auto it = map_.insert({s, ref});
      bool b = it.isnew();
      inserted_ += b;

      // Insertion failed, delete element
      // FIXME add a cache to avoid useless allocations.
      if (!b)
	p_.deallocate(ref);

      // The state has been mark dead by another thread
      for (unsigned i = 0; !b && i < nb_th_; ++i)
	if (it->colors[i] == static_cast<int>(CLOSED))
	  return false;

      // The state has already been visited by the current thread
      if (it->colors[tid_] == static_cast<int>(OPEN))
	return false;

      // Keep a ptr over the array of colors
      refs_.push_back(it->colors);

      // Mark state as visited.
      it->colors[tid_] = OPEN;
      ++states_;
      return true;
    }

    bool pop(State s)
    {
      (void) s;
      // Don't avoid pop but modify the status of the state
      // during backtrack
      refs_.back()[tid_] = CLOSED;
      refs_.pop_back();
      return true;
    }

    void edge(unsigned int, unsigned int)
    {
      ++edges_;
    }

    void finalize()
    {
      tm_.stop("DFS thread " + std::to_string(this->tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(this->tid_))
        .walltime();
    }

    unsigned inserted()
    {
      return inserted_;
    }

    unsigned states()
    {
      return states_;
    }

    unsigned edges()
    {
      return edges_;
    }

    unsigned how_many_generations()
    {
      return nb_gens_;
    }

    void run()
    {
      setup();
      State initial = sys_.initial(tid_);
      if (SPOT_LIKELY(push(initial, dfs_number)))
        {
          todo.push_back({initial, sys_.succ(initial, tid_)});
	  ++dfs_number;
        }
      while (!todo.empty())
        {
          if (todo.back().it->done())
            {
              if (SPOT_LIKELY(pop(todo.back().s)))
                {
                  sys_.recycle(todo.back().it, tid_);
                  todo.pop_back();
                }
            }
          else
            {
              ++transitions;
              State dst = todo.back().it->state();

	      if (SPOT_LIKELY(push(dst, dfs_number)))
		{
		  ++dfs_number;
		  todo.back().it->next();
		  todo.push_back({dst, sys_.succ(dst, tid_)});
		}
              else
                {
                  todo.back().it->next();
                }
            }
        }
      finalize();
    }


  private:
    struct todo_element
    {
      State s;
      SuccIterator* it;
    };
    kripkecube<State, SuccIterator>& sys_;
    std::vector<todo_element> todo;
    unsigned int dfs_number = 0;
    unsigned int transitions = 0;
    unsigned int tid_;

    spot::timer_map tm_;
    shared_map map_;
    unsigned inserted_ = 0;
    unsigned nb_gens_ = 0;
    unsigned states_ = 0;
    unsigned edges_ = 0;
    unsigned nb_th_ = 0;
    fixed_size_pool p_;
    std::vector<int*> refs_;
  };

  std::mutex iomutex; // TOREMOVE

  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_dfs2
  {
    enum st_status     // Describe the status of a state
      {
        OPEN = 1,       // The state is currently processed by this thread
        CLOSED = 2,     // All the successors of this state have been visited
	UNKNOWN = 4,    // First time this state is discoverd by this thread
	UNKNOWN_OPEN = 8,
	PHASE_1 = 16
      };

    struct my_pair
    {
      State st;
      int* colors;
    };

    struct inner_pair_hasher
    {
      inner_pair_hasher(const my_pair&)
      { }

      inner_pair_hasher() = default;

      brick::hash::hash128_t
      hash(const my_pair& lhs) const
      {
        StateHash hash;
	// FIXME without that insert/find fail !!
	unsigned u = hash(lhs.st) % (1<<30);
	return {u, u};
      }

      bool equal(const my_pair& lhs,
                 const my_pair& rhs) const
      {
        StateEqual equal;
        return equal(lhs.st, rhs.st);
      }
    };

  public:
    st_status insert_status_;
    using shared_map = brick::hashset::FastConcurrent <my_pair,
						       inner_pair_hasher>;

    swarmed_dfs2(kripkecube<State, SuccIterator>& sys,
		 shared_map& map, unsigned tid,
		 std::function<std::vector<State>*(std::vector<State>&)> fun,
		 std::atomic<bool>& stop, unsigned initial_population = 1000):
      sys_(sys), tid_(tid), map_(map),
      nb_th_(std::thread::hardware_concurrency()),
      p_(sizeof(int)*std::thread::hardware_concurrency()),
      interpolate_fun_(fun), stop_(stop), THRESHOLD(initial_population)
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
      if (!(tid_%2)) // FIXME How many !
	phase1 = false;       // Reference, no GP for this thread
    }

    virtual ~swarmed_dfs2()
    {
      delete new_gen_;
    }

    void setup()
    {
      tm_.start("DFS thread " + std::to_string(this->tid_));
    }

    bool push(State s, unsigned int tid)
    {
      (void) tid;

      // Prepare data for a newer allocation
      int* ref = (int*) p_.allocate();
      for (unsigned i = 0; i < nb_th_; ++i)
	ref[i] = UNKNOWN;

      // Try to insert the new state in the shared map.
      auto it = map_.insert({s, ref});
      bool b = it.isnew();
      inserted_ += b;

      // Insertion failed, delete element
      // FIXME add a cache to avoid useless allocations.
      if (!b)
	p_.deallocate(ref);

      // The state has been mark dead by another thread
      for (unsigned i = 0; !b && i < nb_th_; ++i)
	if (it->colors[i] == static_cast<int>(CLOSED))
	  return false;

      // The state has already been visited by the current thread
      if (it->colors[tid_] == static_cast<int>(insert_status_))
	return false;

     // Keep a ptr over the array of colors
      refs_.push_back(it->colors);

      // Mark state as visited.
      it->colors[tid_] = insert_status_;// FIX for GP previous was OPEN;
      ++states_;
      return true;
    }

    bool pop(State s)
    {
      (void) s;
      // Don't avoid pop but modify the status of the state
      // during backtrack
      refs_.back()[tid_] = CLOSED;
      refs_.pop_back();
      return true;
    }

    void edge(unsigned int, unsigned int)
    {
      ++edges_;
    }

    void finalize()
    {
      tm_.stop("DFS thread " + std::to_string(this->tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(this->tid_))
        .walltime();
    }

    unsigned inserted()
    {
      return inserted_;
    }

    unsigned states()
    {
      return states_;
    }

    unsigned edges()
    {
      return edges_;
    }

    unsigned how_many_generations()
    {
      return nb_gens_;
    }

    void sampling()
    {
      State initial = sys_.initial(tid_);
      if (SPOT_LIKELY(push(initial, dfs_number)))
        {
          todo.push_back({initial, sys_.succ(initial, tid_)});
	  ++dfs_number;
	  sample_.push_back(initial);
        }
      while (!todo.empty() && sample_.size() < THRESHOLD)
        {
          if (todo.back().it->done())
            {
              if (SPOT_LIKELY(pop(todo.back().s)))
                {
                  sys_.recycle(todo.back().it, tid_);
                  todo.pop_back();
                }
            }
          else
            {
              ++transitions;
              State dst = todo.back().it->state();

	      if (SPOT_LIKELY(push(dst, dfs_number)))
		{
		  ++dfs_number;
		  todo.back().it->next();
		  todo.push_back({dst, sys_.succ(dst, tid_)});
		  sample_.push_back(dst);
		}
              else
                {
                  todo.back().it->next();
                }
            }
        }
    }

    void cleaning()
    {
      todo.clear();
      refs_.clear();
      dfs_number = 0;
    }

    void swarming(State initial)
    {
      if (SPOT_LIKELY(push(initial, dfs_number)))
        {
          todo.push_back({initial, sys_.succ(initial, tid_)});
	  ++dfs_number;
        }
      while (!todo.empty() && !stop_.load(std::memory_order_relaxed))
        {
          if (todo.back().it->done())
            {
              if (SPOT_LIKELY(pop(todo.back().s)))
                {
                  sys_.recycle(todo.back().it, tid_);
                  todo.pop_back();
                }
            }
          else
            {
              ++transitions;
              State dst = todo.back().it->state();

	      if (SPOT_LIKELY(push(dst, dfs_number)))
		{
		  ++dfs_number;
		  todo.back().it->next();
		  todo.push_back({dst, sys_.succ(dst, tid_)});
		}
              else
                {
                  todo.back().it->next();
                }
            }
        }
    }


    void run()
    {
      setup();
      if (phase1)
	{
	  insert_status_ = PHASE_1;
	  sampling();
	  cleaning();

	  // Generate the next geneation from the sample
	  new_gen_ = interpolate_fun_(sample_);

	  // Use all mutated states as initial state
	  unsigned i = 0;
	  while (i < new_gen_->size() && !stop_.load(std::memory_order_relaxed))
	    {
	      insert_status_ = UNKNOWN_OPEN;
	      swarming(new_gen_->at(i));
	      ++i;
	      ++nb_gens_;
	    }
	}
      else
	{
	  insert_status_ = OPEN;
	  swarming(sys_.initial(tid_));
	  stop_.store(true, std::memory_order_relaxed);
	}
      finalize();
    }



  private:
    struct todo_element
    {
      State s;
      SuccIterator* it;
    };
    kripkecube<State, SuccIterator>& sys_;
    std::vector<todo_element> todo;
    unsigned int dfs_number = 0;
    unsigned int transitions = 0;
    unsigned int tid_;

    spot::timer_map tm_;
    shared_map map_;
    unsigned inserted_ = 0;
    unsigned nb_gens_ = 0;
    unsigned states_ = 0;
    unsigned edges_ = 0;
    unsigned nb_th_ = 0;
    fixed_size_pool p_;
    std::vector<int*> refs_;
    std::function<std::vector<State>*(std::vector<State>&)> interpolate_fun_;
    std::atomic<bool>& stop_;
    std::vector<State> sample_;
    bool phase1 = true;
    std::vector<State>* new_gen_ = nullptr;
    unsigned THRESHOLD = 1000;
  };





  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_deadlock
  {
    enum st_status     // Describe the status of a state
      {
        OPEN = 1,       // The state is currently processed by this thread
        CLOSED = 2,     // All the successors of this state have been visited
	UNKNOWN = 4     // First time this state is discoverd by this thread
      };

    struct my_pair
    {
      State st;
      int* colors;
    };

    struct inner_pair_hasher
    {
      inner_pair_hasher(const my_pair&)
      { }

      inner_pair_hasher() = default;

      brick::hash::hash128_t
      hash(const my_pair& lhs) const
      {
        StateHash hash;
	// FIXME without that insert/find fail !!
	unsigned u = hash(lhs.st) % (1<<30);
	return {u, u};
      }

      bool equal(const my_pair& lhs,
                 const my_pair& rhs) const
      {
        StateEqual equal;
        return equal(lhs.st, rhs.st);
      }
    };

  public:
    using shared_map = brick::hashset::FastConcurrent <my_pair,
						       inner_pair_hasher>;

    swarmed_deadlock(kripkecube<State, SuccIterator>& sys,
                shared_map& map, unsigned tid):
      sys_(sys), tid_(tid), map_(map),
      nb_th_(std::thread::hardware_concurrency()),
      p_(sizeof(int)*std::thread::hardware_concurrency())
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
    }

    virtual ~swarmed_deadlock()
    {
    }

    void setup()
    {
      tm_.start("DFS thread " + std::to_string(this->tid_));
    }

    bool push(State s, unsigned int tid)
    {
      (void) tid;
      // Prepare data for a newer allocation
      int* ref = (int*) p_.allocate();
      for (unsigned i = 0; i < nb_th_; ++i)
	ref[i] = UNKNOWN;

      // Try to insert the new state in the shared map.
      auto it = map_.insert({s, ref});
      bool b = it.isnew();
      inserted_ += b;

      // Insertion failed, delete element
      // FIXME add a cache to avoid useless allocations.
      if (!b)
	p_.deallocate(ref);

      // The state has been mark dead by another thread
      for (unsigned i = 0; !b && i < nb_th_; ++i)
	if (it->colors[i] == static_cast<int>(CLOSED))
	  return false;

      // The state has already been visited by the current thread
      if (it->colors[tid_] == static_cast<int>(OPEN))
	return false;

      // Keep a ptr over the array of colors
      refs_.push_back(it->colors);

      // Mark state as visited.
      it->colors[tid_] = OPEN;
      ++states_;
      return true;
    }

    bool pop(State s)
    {
      (void) s;
      // Don't avoid pop but modify the status of the state
      // during backtrack
      refs_.back()[tid_] = CLOSED;
      refs_.pop_back();
      return true;
    }

    void edge(unsigned int, unsigned int)
    {
      ++edges_;
    }

    void finalize()
    {
      tm_.stop("DFS thread " + std::to_string(this->tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(this->tid_))
        .walltime();
    }

    unsigned inserted()
    {
      return inserted_;
    }

    unsigned states()
    {
      return states_;
    }

    unsigned edges()
    {
      return edges_;
    }

    unsigned how_many_generations()
    {
      return nb_gens_;
    }

    void run()
    {
      setup();
      State initial = sys_.initial(tid_);
      if (SPOT_LIKELY(push(initial, dfs_number)))
        {
          todo.push_back({initial, sys_.succ(initial, tid_), transitions});
	  ++dfs_number;
        }
      while (!todo.empty())
        {
          if (todo.back().it->done())
            {
              if (SPOT_LIKELY(pop(todo.back().s)))
                {
                  deadlock_ = todo.back().current_tr == transitions;
                  if (deadlock_)
                    break;
                  sys_.recycle(todo.back().it, tid_);
                  todo.pop_back();
                }
            }
          else
            {
              ++transitions;
              State dst = todo.back().it->state();

	      if (SPOT_LIKELY(push(dst, dfs_number)))
		{
		  ++dfs_number;
		  todo.back().it->next();
		  todo.push_back({dst, sys_.succ(dst, tid_), transitions});
		}
              else
                {
                  todo.back().it->next();
                }
            }
        }
      finalize();
    }

    bool has_deadlock()
    {
      return deadlock_;
    }

  private:
    struct todo_element
    {
      State s;
      SuccIterator* it;
      unsigned current_tr;
    };
    kripkecube<State, SuccIterator>& sys_;
    std::vector<todo_element> todo;
    unsigned int dfs_number = 0;
    unsigned int transitions = 0;
    unsigned int tid_;

    spot::timer_map tm_;
    shared_map map_;
    unsigned inserted_ = 0;
    unsigned nb_gens_ = 0;
    unsigned states_ = 0;
    unsigned edges_ = 0;
    unsigned nb_th_ = 0;
    fixed_size_pool p_;
    std::vector<int*> refs_;
    bool deadlock_ = false;
  };


  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_gp_deadlock
  {
    enum st_status     // Describe the status of a state
      {
        OPEN = 1,       // The state is currently processed by this thread
        CLOSED = 2,     // All the successors of this state have been visited
        CLOSED_DEADLOCK = 3,
	UNKNOWN = 4,    // First time this state is discoverd by this thread
	UNKNOWN_OPEN = 8,
	PHASE_1 = 16
      };

    struct my_pair
    {
      State st;
      int* colors;
    };

    struct inner_pair_hasher
    {
      inner_pair_hasher(const my_pair&)
      { }

      inner_pair_hasher() = default;

      brick::hash::hash128_t
      hash(const my_pair& lhs) const
      {
        StateHash hash;
	// FIXME without that insert/find fail !!
	unsigned u = hash(lhs.st) % (1<<30);
	return {u, u};
      }

      bool equal(const my_pair& lhs,
                 const my_pair& rhs) const
      {
        StateEqual equal;
        return equal(lhs.st, rhs.st);
      }
    };

  public:
    st_status insert_status_;
    using shared_map = brick::hashset::FastConcurrent <my_pair,
						       inner_pair_hasher>;

    swarmed_gp_deadlock(kripkecube<State, SuccIterator>& sys,
		 shared_map& map, unsigned tid,
		 std::function<std::vector<State>*(std::vector<State>&)> fun,
		 std::atomic<bool>& stop, unsigned initial_population = 1000):
      sys_(sys), tid_(tid), map_(map),
      nb_th_(std::thread::hardware_concurrency()),
      p_(sizeof(int)*std::thread::hardware_concurrency()),
      interpolate_fun_(fun), stop_(stop), THRESHOLD(initial_population)
    {
      SPOT_ASSERT(is_a_kripkecube(sys));
      if (!(tid_%2)) // FIXME How many !
	phase1 = false;       // Reference, no GP for this thread
    }

    virtual ~swarmed_gp_deadlock()
    {
      delete new_gen_;
    }

    void setup()
    {
      tm_.start("DFS thread " + std::to_string(this->tid_));
    }

    bool push(State s, unsigned int tid)
    {
      (void) tid;

      // Prepare data for a newer allocation
      int* ref = (int*) p_.allocate();
      for (unsigned i = 0; i < nb_th_; ++i)
	ref[i] = UNKNOWN;

      // Try to insert the new state in the shared map.
      auto it = map_.insert({s, ref});
      bool b = it.isnew();
      inserted_ += b;

      // Insertion failed, delete element
      // FIXME add a cache to avoid useless allocations.
      if (!b)
	p_.deallocate(ref);

      // The state has been mark dead by another thread
      for (unsigned i = 0; !b && i < nb_th_; ++i)
        {
          if (it->colors[i] == static_cast<int>(CLOSED))
            return false;
          if (it->colors[i] == static_cast<int>(CLOSED_DEADLOCK) &&
              insert_status_ == OPEN)
            {
              deadlock_ = true; // deaddlock detected by another thread
              stop_.store(true, std::memory_order_relaxed);
              return false;
            }
        }

      // The state has already been visited by the current thread
      if (it->colors[tid_] == static_cast<int>(insert_status_))
	return false;

     // Keep a ptr over the array of colors
      refs_.push_back(it->colors);

      // Mark state as visited.
      it->colors[tid_] = insert_status_;// FIX for GP previous was OPEN;
      ++states_;
      return true;
    }

    bool pop(State s)
    {
      (void) s;
      // Don't avoid pop but modify the status of the state
      // during backtrack
      refs_.back()[tid_] = CLOSED;
      refs_.pop_back();
      return true;
    }

    void edge(unsigned int, unsigned int)
    {
      ++edges_;
    }

    void finalize()
    {
      tm_.stop("DFS thread " + std::to_string(this->tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS thread " + std::to_string(this->tid_))
        .walltime();
    }

    unsigned inserted()
    {
      return inserted_;
    }

    unsigned states()
    {
      return states_;
    }

    unsigned edges()
    {
      return edges_;
    }

    unsigned how_many_generations()
    {
      return nb_gens_;
    }

    void sampling()
    {
      State initial = sys_.initial(tid_);
      if (SPOT_LIKELY(push(initial, dfs_number)))
        {
          todo.push_back({initial, sys_.succ(initial, tid_), transitions});
	  ++dfs_number;
	  sample_.push_back(initial);
        }
      while (!todo.empty() && sample_.size() < THRESHOLD)
        {
          if (todo.back().it->done())
            {
              if (SPOT_LIKELY(pop(todo.back().s)))
                {
                  if (todo.back().current_tr == transitions)
                    {
                      deadlock_ = true;
                      stop_.store(true, std::memory_order_relaxed);
                      break;
                    }
                  sys_.recycle(todo.back().it, tid_);
                  todo.pop_back();
                }
            }
          else
            {
              ++transitions;
              State dst = todo.back().it->state();

	      if (SPOT_LIKELY(push(dst, dfs_number)))
		{
		  ++dfs_number;
		  todo.back().it->next();
		  todo.push_back({dst, sys_.succ(dst, tid_), transitions});
		  sample_.push_back(dst);
		}
              else
                {
                  todo.back().it->next();
                }
            }
        }
    }

    void cleaning()
    {
      todo.clear();
      refs_.clear();
      dfs_number = 0;
    }

    void swarming(State initial)
    {
      if (SPOT_LIKELY(push(initial, dfs_number)))
        {
          todo.push_back({initial, sys_.succ(initial, tid_), transitions});
	  ++dfs_number;
        }
      while (!todo.empty() && !stop_.load(std::memory_order_relaxed))
        {
          if (todo.back().it->done())
            {
              if (SPOT_LIKELY(pop(todo.back().s)))
                {
                  // Handle deadlocks
                  if (todo.back().current_tr == transitions)
                    {
                      if (insert_status_ == OPEN)
                        {
                          stop_.store(true, std::memory_order_relaxed);
                          deadlock_ = true;
                          break;
                        }
                      else
                        {
                          while (!refs_.empty())
                            {
                              refs_.back()[tid_] = CLOSED_DEADLOCK;
                              refs_.pop_back();
                            }
                          break;
                        }
                    }
                  sys_.recycle(todo.back().it, tid_);
                  todo.pop_back();
                }
            }
          else
            {
              ++transitions;
              State dst = todo.back().it->state();

	      if (SPOT_LIKELY(push(dst, dfs_number)))
		{
		  ++dfs_number;
		  todo.back().it->next();
		  todo.push_back({dst, sys_.succ(dst, tid_), transitions});
		}
              else
                {
                  todo.back().it->next();
                }
            }
        }
    }


    void run()
    {
      setup();
      if (phase1)
	{
	  insert_status_ = PHASE_1;
	  sampling();
	  cleaning();

	  // Generate the next geneation from the sample
	  new_gen_ = interpolate_fun_(sample_);

	  // Use all mutated states as initial state
	  unsigned i = 0;
	  while (i < new_gen_->size() && !stop_.load(std::memory_order_relaxed))
	    {
	      insert_status_ = UNKNOWN_OPEN;
	      swarming(new_gen_->at(i));
	      ++i;
	      ++nb_gens_;
              cleaning();
	    }
	}
      else
	{
	  insert_status_ = OPEN;
	  swarming(sys_.initial(tid_));
	  stop_.store(true, std::memory_order_relaxed);
	}
      finalize();
    }


    bool has_deadlock()
    {
      return deadlock_;
    }

  private:
    struct todo_element
    {
      State s;
      SuccIterator* it;
      unsigned current_tr;
    };
    kripkecube<State, SuccIterator>& sys_;
    std::vector<todo_element> todo;
    unsigned int dfs_number = 0;
    unsigned int transitions = 0;
    unsigned int tid_;

    spot::timer_map tm_;
    shared_map map_;
    unsigned inserted_ = 0;
    unsigned nb_gens_ = 0;
    unsigned states_ = 0;
    unsigned edges_ = 0;
    unsigned nb_th_ = 0;
    fixed_size_pool p_;
    std::vector<int*> refs_;
    std::function<std::vector<State>*(std::vector<State>&)> interpolate_fun_;
    std::atomic<bool>& stop_;
    std::vector<State> sample_;
    bool phase1 = true;
    std::vector<State>* new_gen_ = nullptr;
    unsigned THRESHOLD = 1000;
    bool deadlock_ = false;
  };















































  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class swarmed_gp  :
    public seq_reach_kripke<State, SuccIterator,
                            StateHash, StateEqual,
                            swarmed_gp<State, SuccIterator,
                                        StateHash, StateEqual>>
  {
    struct my_pair
    {
      my_pair(): color(0){}
      my_pair(const my_pair& p): st(p.st), color(p.color.load()){}
      my_pair(const State st, int bar): st(st), color(bar) { }
      my_pair& operator=(my_pair& other)
      {
        if (this == &other)
          return *this;
        st = other.st;
        color = other.color.load();
        return *this;
      }
      State st;
      std::atomic<int> color;
    };

    struct inner_pair_hasher
    {
      inner_pair_hasher(const my_pair&)
      { }

      inner_pair_hasher() = default;

      brick::hash::hash128_t
      hash(const my_pair& lhs) const
      {
        StateHash hash;
        auto u = hash(lhs.st);
        return  {u, u}; // Just ignore the second part
      }

      bool equal(const my_pair& lhs,
                 const my_pair& rhs) const
      {
        StateEqual equal;
        return equal(lhs.st, rhs.st);
      }
    };

    enum st_status     // Describe the status of a state
      {
        OPEN = 0,       // The state is currently processed by a thread
        CLOSED = 1,     // All the successors of this state have been visited
        UNKNOWN = 2,
        UNKNOWN_CLOSED = 3
      };

  public:
    using shared_map = brick::hashset::FastConcurrent <my_pair,
                                                       inner_pair_hasher>;

    swarmed_gp(kripkecube<State, SuccIterator>& sys,
               std::function<std::vector<State>*(std::vector<State>&)>
                                                       interpolate_fun,
               shared_map& map,
               unsigned tid, bool& stop)
      : seq_reach_kripke<State, SuccIterator, StateHash, StateEqual,
                         swarmed_gp<State, SuccIterator,
                                    StateHash, StateEqual>>(sys, tid, stop),
      interpolate_fun_(interpolate_fun), map_(map)
      {
        if (!tid%2) // FIXME How many !
          phase1 = false;       // Reference, no GP for this thread
      }

    virtual ~swarmed_gp()
    {
    }

    void setup()
    {
      tm_.start("DFS GP thread " + std::to_string(this->tid_));
    }

    bool push(State s, unsigned int dfsnum)
    {
      ++states_;
      if (SPOT_UNLIKELY(dfsnum <= 1000 && phase1)) // FIXME threshold
        {
          sample_.push_back(s);

          // Here we decide to reset the current DFS using states
          // that have been mutated from GP.
          if (dfsnum == 1000)
            {
              phase1 = false;
              insert_status_ = UNKNOWN;
              new_gen = interpolate_fun_(sample_);

              // Recycle iterators
              for (auto& e: this->todo)
                this->sys_.recycle(e.it, this->tid_);

              // Clear structures.
              this->todo.clear();
              this->dfs_number = 0;
              this->visited.clear();

              // Select a "new" initial state among those generated
              this->todo.push_back({new_gen->at(new_gen_idx),
                    this->sys_.succ(new_gen->at(new_gen_idx), this->tid_)});
              this->visited[new_gen->at(new_gen_idx)] = this->dfs_number;
              this->dfs_number = 1;
              s = new_gen->at(new_gen_idx);
              ++new_gen_idx;
            }
        }

      auto it = map_.insert({s, insert_status_});

      // State has been marked as dead by another thread
      // just skip the insertion
      st_status status = (st_status) it->color.load(std::memory_order_relaxed);
      bool b = it.isnew();
      inserted_ += b;
      if (!b && status == CLOSED)
          //(status == CLOSED || status == UNKNOWN_CLOSED))
        return false;
      return true;
    }

    bool pop(State s)
    {
      // Don't avoid pop but modify the status of the state
      // during backtrack
      auto it = map_.insert({s, CLOSED}); // FIXME Find is enough

      // st_status status = (st_status) it->color.load(std::memory_order_relaxed);
      // if (status == OPEN)
        it->color.store(CLOSED, std::memory_order_relaxed);
      // else
      //   it->color.store(UNKNOWN_CLOSED, std::memory_order_relaxed);

      // Go ahead with another state iff popping the "initial" state
      // Do not worry about terminaison: the thread with tid = 0 will
      // stop the world as soon as it finishes
      if (SPOT_UNLIKELY(insert_status_ != OPEN  && this->todo.size() == 1))
        {
          ++nb_gens_;
          for (auto& e: this->todo)
            this->sys_.recycle(e.it, this->tid_);

          // Clear structures.
          this->todo.clear();
          this->dfs_number = 0;
          this->visited.clear();

          // When this condition holds it means that
          //    (1) all the state-spaces from mutated initial states
          //        have been explored
          //    (2) the "valid" state-space from the initial state
          //        has not yet finished
          // In this case, relaunch a DFS to help the other one.
          if (SPOT_UNLIKELY(new_gen_idx == new_gen->size()))
            {
              new_gen_idx = 0;
              new_gen->clear();
              new_gen->push_back(this->sys_.initial(this->tid_));
              insert_status_ = OPEN;
            }

          // Select a "new" initial state among those generated
          this->todo.push_back({new_gen->at(new_gen_idx),
                this->sys_.succ(new_gen->at(new_gen_idx), this->tid_)});
          this->visited[new_gen->at(new_gen_idx)] = this->dfs_number;
          this->dfs_number = 1;
          ++new_gen_idx;
          return false;
        }

      return true;
    }

    void edge(unsigned int, unsigned int)
    {
      ++edges_;
    }

    void finalize()
    {
      if (insert_status_ == OPEN)
        this->stop_ = true;
      tm_.stop("DFS GP thread " + std::to_string(this->tid_));
    }

    unsigned walltime()
    {
      return tm_.timer("DFS GP thread " + std::to_string(this->tid_))
        .walltime();
    }

    unsigned inserted()
    {
      return inserted_;
    }

    unsigned states()
    {
      return states_;
    }

    unsigned edges()
    {
      return edges_;
    }

    unsigned how_many_generations()
    {
      return nb_gens_;
    }

  private:
    spot::timer_map tm_;
    std::function<std::vector<State>*(std::vector<State>&)> interpolate_fun_;
    shared_map map_;
    std::vector<State> sample_;
    bool phase1 = true;
    std::vector<State>* new_gen = nullptr;
    unsigned new_gen_idx = 0;
    st_status insert_status_ = OPEN;
    unsigned inserted_ = 0;
    unsigned nb_gens_ = 0;
    unsigned states_ = 0;
    unsigned edges_ = 0;
  };
}
