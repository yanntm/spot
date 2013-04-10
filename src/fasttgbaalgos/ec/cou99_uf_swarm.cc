// Copyright (C) 2013 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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

#include "cou99_uf_swarm.hh"
#include "misc/timer.hh"

namespace spot
{
  namespace
  {
    std::atomic<bool> stop_world;


    class cou99_uf_swarm_impl : public cou99_uf
    {
    public :
      cou99_uf_swarm_impl (instanciator* i) :
	cou99_uf(i)
      {}

      void dfs_push_classic(fasttgba_state* s)
      {
	uf->add (s);
	fasttgba_succ_iterator* si = a_->swarm_succ_iter(s);
	si->first();
	todo.push_back (std::make_pair(s, si));
	last = true;
      }


      void main()
      {
	while (!stop_world && !todo.empty())
	  {
	    assert(!uf->is_dead(todo.back().first));

	    if (!last)
	      todo.back().second->next();
	    else
	      last = false;

	    if (todo.back().second->done())
	      {
		dfs_pop_classic ();
	      }
	    else
	      {
		fasttgba_state* d = todo.back().second->current_state();
		if (!uf->contains(d))
		  {
		    dfs_push_classic (d);
		    continue;
		  }
		else if (!uf->is_dead(d))
		  {
		    merge_classic (d);
		    if (uf->get_acc(d).all())
		      {
			counterexample_found = true;
			d->destroy();
			return;
		      }
		  }
		d->destroy();
	      }
	  }
      }
    };
  }

  cou99_uf_swarm::cou99_uf_swarm (instanciator* i,  int thread_number):
    itor_(i),
    nbthread_(thread_number),
    answer_found (false),
    counterexample_found(false)
  {
    if (nbthread_ > std::thread::hardware_concurrency())
      {
	std::cout << "Warning the hardware support only "
		  <<  std::thread::hardware_concurrency()
		  << " thread(s). Given "
		  <<  nbthread_ << " !" << std::endl;
      }

    assert(nbthread_ >0);

    stop_world = false;

    // Launch all threads ...
    for(unsigned int i = 0; i < nbthread_; ++i)
      {
	threads.push_back
	  (std::thread(&spot::cou99_uf_swarm::do_work, this, i));
      }
  }


  void cou99_uf_swarm::do_work(unsigned int)
  {
    // This is instanciation part that will be creating
    // at the creation of the swarm emptiness check
    spot::cou99_uf_swarm_impl* checker = new spot::cou99_uf_swarm_impl(itor_);

    // Then, Threads waits the GO signal that will become
    // true when the user will launch check
    {
      std::unique_lock<std::mutex> lk(mrun);
      run.wait(lk, [this]{
	  return go;
	});
    }

    // Launch the emptiness check ...
    bool res = checker->check();


    // And then notify the main thread that the result has
    // been found
    if (!stop_world)
      {
	std::lock_guard<std::mutex> lk(m);
	answer_found = true;
	if (res)
	  counterexample_found = res;
	data_cond.notify_one();
      }

    delete checker;
  }

    /// The implementation of the interface
    bool cou99_uf_swarm::check()
    {
      // All threads are already instanciated
      // So just set the flag to run
      {
	std::lock_guard<std::mutex> lk(mrun);
	go = true;
	run.notify_all();
      }

      // ... and wait them
      {
	std::unique_lock<std::mutex> lk(m);
	data_cond.wait(lk,
		       [this]{
			 return answer_found;
		       });

	stop_world = true;
      }

      // Create a dedicated thread waiting the end of all threads
      // It's safe to create one more thread here even if
      // the number of thread used is hardware limit since if this
      // point is reached this means that one thread is already dead
      // or about to die
      thread_finalize = new std::thread([this] {
	  std::for_each(threads.begin(),threads.end(),
			std::mem_fn(&std::thread::join));
	});

      return counterexample_found;
    }

  cou99_uf_swarm::~cou99_uf_swarm()
    {
      // Wait the trhead dedicated to wait all threads
      thread_finalize->join();
      delete thread_finalize;
    }
}
