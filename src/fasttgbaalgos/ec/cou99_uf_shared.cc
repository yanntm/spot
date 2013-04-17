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

#include "cou99_uf_shared.hh"
#include "cou99_uf.hh"
#include "union_find_shared.hh"
#include "misc/timer.hh"

namespace spot
{
  namespace
  {
    std::atomic<bool> stop_world;


    class cou99_uf_shared_impl : public cou99_uf
    {
    private:
      union_find* ufshared_;
      int tn_;
    public :
      cou99_uf_shared_impl (instanciator* i,
			    union_find* u,
			    int threadnumber) :
	cou99_uf(i), tn_(threadnumber)
      {
	ufshared_ = u;
      }

      virtual
      ~cou99_uf_shared_impl()
      {
	//std::cout << "DESTROY" << std::endl;
      }

  void dfs_push_classic(fasttgba_state* s)
  {
    //std::cout << tn_ << " : dfs_push_classic" << std::endl;
    uf->add (s);
    if (ufshared_->add (s))
      {
	s->clone();
      }
    fasttgba_succ_iterator* si = a_->swarm_succ_iter(s);
    si->first();
    todo.push_back (std::make_pair(s, si));
    last = true;
  }

  void dfs_pop_classic()
  {
    //std::cout << tn_ << " : dfs_pop_classic" << std::endl;
    pair_state_iter pair = todo.back();
    delete pair.second;
    todo.pop_back();


    if (todo.empty() ||
	!uf->same_partition(pair.first, todo.back().first))
      {
	uf->make_dead(pair.first);
	ufshared_->make_dead(pair.first);
      }
  }

  void merge_classic(fasttgba_state* d)
  {
    //std::cout << tn_ << " : merge_classic" << std::endl;
    int i = todo.size() - 1;
    markset a (a_->get_acc());

    while (!uf->same_partition(d, todo[i].first))
      {
	int ref = i;
	while (uf->same_partition(todo[ref].first, todo[i].first))
	 --ref;

 	uf->unite(d, todo[i].first);
	ufshared_->unite(d, todo[i].first);
	markset m (a_->get_acc());
	m = todo[i].second->current_acceptance_marks();
	a |= m;
	i = ref;
      }

    markset m = todo[i].second->current_acceptance_marks();
    //uf->add_acc(d, m|a);
    ufshared_->add_acc(d, m|a);

    assert(!uf->is_dead(todo.back().first));
    assert(!uf->is_dead(d));
  }



  void main()
  {
    while (!stop_world && !todo.empty())
      {
	assert(!uf->is_dead(todo.back().first));

	//  Is there any transitions left ?

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
    	    else if (!uf->is_dead(d)  && !ufshared_->is_dead(d))
    	      {
	    	merge_classic (d);
    	    	if (ufshared_->get_acc(d).all())
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


  void init()
  {
    fasttgba_state* init = a_->get_init_state();
    dfs_push_classic(init);
  }

  bool check()
  {
    init();
    main();
    //std::cout << tn_ << " : END" << std::endl;
    return counterexample_found;
  }

    };
  }

  cou99_uf_shared::cou99_uf_shared (instanciator* i,  int thread_number):
    itor_(i),
    nbthread_(thread_number),
    counterexample_found(false),
    nb_end(0)
  {
    if (nbthread_ > std::thread::hardware_concurrency())
      {
	std::cout << "Warning the hardware support only "
		  <<  std::thread::hardware_concurrency()
		  << " thread(s). Given "
		  <<  nbthread_ << " !" << std::endl;
      }

    assert(nbthread_ >0);

    inst = i->new_instance();
    const fasttgba* a_ = inst->get_automaton ();
    uf_ = new union_find_shared (a_->get_acc(), nbthread_);

    stop_world = false;
    go = false;
    answer_found = false;

    // Launch all threads ...
    for(unsigned int i = 0; i < nbthread_; ++i)
      {
	threads.push_back
	  (std::thread(&spot::cou99_uf_shared::do_work, this, i));
	chk.push_back(new spot::cou99_uf_shared_impl(itor_, uf_, i));
      }
  }


  void cou99_uf_shared::do_work(unsigned int i)
  {
    // This is instanciation part that will be creating
    // at the creation of the shared emptiness check
    // spot::cou99_uf_shared_impl* checker =
    //   new spot::cou99_uf_shared_impl(itor_, uf_, i);

    // Then, Threads waits the GO signal that will become
    // true when the user will launch check
    {
      //std::cout << "T_" << i << " waiting..." << std::endl;
      std::unique_lock<std::mutex> lk(mrun);
      run.wait(lk, [this]{
      	  return go;
      	});
      //std::cout << "        T_" << i << " going..." << std::endl;
    }

    // Launch the emptiness check ...
    bool res = chk[i]->check();//checker->check();

    // And then notify the main thread that the result has
    // been found
      {
	std::lock_guard<std::mutex> lk(m);
	if (res)
	  {
	    counterexample_found = res;
	    answer_found = true;
	  }
	++nb_end;
	if (nb_end == nbthread_)
	  answer_found = true;
	data_cond.notify_all();
      }
  }

    /// The implementation of the interface
    bool cou99_uf_shared::check()
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

      thread_finalize = new std::thread([this] {
	  std::for_each(threads.begin(),threads.end(),
			std::mem_fn(&std::thread::join));
	});


      return counterexample_found;
    }

  cou99_uf_shared::~cou99_uf_shared()
    {
      // Wait the trhead dedicated to wait all threads
      thread_finalize->join();
      delete thread_finalize;
      delete uf_;
      for(unsigned int i = 0; i < nbthread_; ++i)
	{
	  delete chk[i];
	}
      delete inst;
    }
}
