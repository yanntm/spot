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


#include <thread>
#include <vector>
#include "dead_share.hh"

namespace spot
{
  // ----------------------------------------------------------------------
  // Concurrent Dijkstra Algorithm with shared union find.
  // ======================================================================
  concur_opt_dijkstra_scc::concur_opt_dijkstra_scc(instanciator* i,
						   spot::uf* uf,
						   int tn,
						   int *stop,
						   std::string option)
    : opt_dijkstra_scc(i, option, true)
  {
    uf_ = uf;
    tn_ = tn;
    stop_ = stop;
  }

  bool
  concur_opt_dijkstra_scc::check()
  {
    start = std::chrono::system_clock::now();
    init();
    main();
    *stop_ = 1;
    end  = std::chrono::system_clock::now();
    return counterexample_found;
  }

  void concur_opt_dijkstra_scc::dfs_push(fasttgba_state* s)
  {
    ++states_cpt_;

    assert(H.find(s) == H.end());
    H.insert(std::make_pair(s, H.size()));

    uf_->make_set(s, tn_);

    // Count!
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    todo.push_back ({s, 0, H.size() -1});
    // Count!
    max_dfs_size_ = max_dfs_size_ > todo.size() ?
      max_dfs_size_ : todo.size();

    roots_stack_->push_trivial(todo.size() -1);

    int tmp_cost = 1*roots_stack_->size() + 2*H.size() + 1*live.size()
      + (deadstore_? deadstore_->size() : 0);
    if (tmp_cost > memory_cost_)
      memory_cost_ = tmp_cost;

  }

  bool concur_opt_dijkstra_scc::merge(fasttgba_state* d)
  {
    ++update_cpt_;
    assert(H.find(d) != H.end());
    int dpos = H[d];
    int rpos = roots_stack_->root_of_the_top();

    roots_stack_->pop();
    while ((unsigned)dpos < todo[rpos].position)
      {
	++update_loop_cpt_;
    	rpos = roots_stack_->root_of_the_top();
    	roots_stack_->pop();
	bool fast_backtrack = false;
	uf_->unite (d, todo[rpos].state, roots_stack_->top_acceptance(),
		    &fast_backtrack);
      }
    roots_stack_->push_non_trivial(rpos, *empty_, todo.size() -1);

    return false;
  }

  void concur_opt_dijkstra_scc::dfs_pop()
  {
    delete todo.back().lasttr;

    unsigned int rtop = roots_stack_->root_of_the_top();
    const fasttgba_state* last = todo.back().state;
    unsigned int steppos = todo.back().position;
    todo.pop_back();

    if (rtop == todo.size())
      {
	++roots_poped_cpt_;
	roots_stack_->pop();
	int trivial = 0;
	//deadstore_->add(last);
	uf_->make_dead(last);
	seen_map::const_iterator it1 = H.find(last);
	H.erase(it1);
	while (H.size() > steppos)
	  {
	    ++trivial;
	    //deadstore_->add(live.back());
	    seen_map::const_iterator it = H.find(live.back());
	    H.erase(it);
	    live.pop_back();
 	  }
	if (trivial == 0) // we just popped a trivial
	  ++trivial_scc_;
      }
    else
      {
	// This is the integration of Nuutila's optimisation.
	live.push_back(last);
      }
  }

  opt_dijkstra_scc::color
  concur_opt_dijkstra_scc::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (uf_->is_dead(state))
      return Dead;
    else
      return Unknown;
  }

  bool
  concur_opt_dijkstra_scc::has_counterexample()
  {
    return counterexample_found;
  }

  void concur_opt_dijkstra_scc::main()
  {
    opt_dijkstra_scc::color c;
    while (!todo.empty() && !*stop_)
      {
	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = swarm_ ?
	      a_->swarm_succ_iter(todo.back().state) :
	      a_->succ_iter(todo.back().state);
	    todo.back().lasttr->first();
	  }
	else
	  {
	    assert(todo.back().lasttr);
	    todo.back().lasttr->next();
	  }

    	if (todo.back().lasttr->done())
    	  {
	    dfs_pop ();
    	  }
    	else
    	  {
	    ++transitions_cpt_;
	    assert(todo.back().lasttr);
    	    fasttgba_state* d = todo.back().lasttr->current_state();
	    c = get_color (d);
    	    if (c == Unknown)
    	      {
		dfs_push (d);
    	    	continue;
    	      }
    	    else if (c == Alive)
    	      {
    	    	if (merge (d))
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

  // ----------------------------------------------------------------------
  // Concurrent Tarjan Algorithm with shared union find.
  // ======================================================================
  concur_opt_tarjan_scc::concur_opt_tarjan_scc(instanciator* i,
					       spot::uf* uf,
					       int tn,
					       int *stop,
					       std::string option)
    : opt_tarjan_scc(i, option, true)
  {
    uf_ = uf;
    tn_ = tn;
    stop_ = stop;
  }

  bool
  concur_opt_tarjan_scc::check()
  {
    start = std::chrono::system_clock::now();
    init();
    main();
    *stop_ = 1;
    end = std::chrono::system_clock::now();
    return counterexample_found;
  }

  void
  concur_opt_tarjan_scc::dfs_push(fasttgba_state* q)
  {
    int position = H.size();
    H.insert(std::make_pair(q, position));
    dstack_->push(position);
    todo.push_back ({q, 0, H.size() -1});

    uf_->make_set(q, tn_);

    ++dfs_size_;
    ++states_cpt_;
    max_dfs_size_ = max_dfs_size_ > dfs_size_ ?
      max_dfs_size_ : dfs_size_;
    max_live_size_ = H.size() > max_live_size_ ?
      H.size() : max_live_size_;

    int tmp_cost = 1*dstack_->size() + 2*H.size() +1*live.size()
      + (deadstore_? deadstore_->size() : 0);
    if (tmp_cost > memory_cost_)
      memory_cost_ = tmp_cost;
  }

  bool concur_opt_tarjan_scc::dfs_update(fasttgba_state* d)
  {
    ++update_cpt_;
    // Warning !  Do not optimize to '<'
    // Here we check '<=' since the information "==" is
    // needed to the compressed stack of lowlink.
    if (H[d] <= dstack_->top())
      dstack_->set_top(H[d]);

    bool fast_backtrack = false;
    uf_->unite (d, todo.back().state, dstack_->top_acceptance(),
		&fast_backtrack);
    return false;
  }

  opt_tarjan_scc::color
  concur_opt_tarjan_scc::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = H.find(state);
    if (i != H.end())
      return Alive;
    else if (uf_->is_dead(state))
      return Dead;
    else
      return Unknown;
  }

  void
  concur_opt_tarjan_scc::dfs_pop()
  {
    --dfs_size_;
    int ll = dstack_->pop();

    unsigned int steppos = todo.back().position;
    const fasttgba_state* last = todo.back().state;
    delete todo.back().lasttr;
    todo.pop_back();

    if ((int) steppos == ll)
      {
	++roots_poped_cpt_;
	int trivial = 0;

	// Delete the root that is not inside of live Stack
	uf_->make_dead(last);
	seen_map::const_iterator it1 = H.find(last);
	H.erase(it1);
	while (H.size() > steppos)
	  {
	    ++trivial;
	    seen_map::const_iterator it = H.find(live.back());
	    H.erase(it);
	    live.pop_back();
	  }

	// This change regarding original algorithm
	if (trivial == 0)
	  ++trivial_scc_;
      }
    else
      {
	// Warning !  Do not optimize to '<'
	// Here we check '<=' since the information "==" is
	// needed to the compressed stack of lowlink.
	if (ll <= dstack_->top())
	  dstack_->set_top(ll);
	live.push_back(last);

	bool fast_backtrack = false;
	uf_->unite (last, todo.back().state, dstack_->top_acceptance(),
		    &fast_backtrack);
      }
  }

  bool
  concur_opt_tarjan_scc::has_counterexample()
  {
    return counterexample_found;
  }


  void concur_opt_tarjan_scc::main()
  {
    opt_tarjan_scc::color c;
    while (!todo.empty() && !*stop_)
      {

	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = swarm_ ?
	      a_->swarm_succ_iter(todo.back().state) :
	      a_->succ_iter(todo.back().state);
	    todo.back().lasttr->first();
	  }
	else
	  {
	    assert(todo.back().lasttr);
	    todo.back().lasttr->next();
	  }

    	if (todo.back().lasttr->done())
    	  {
	    dfs_pop ();
	    if (counterexample_found)
	      return;
    	  }
    	else
    	  {
	    ++transitions_cpt_;
	    assert(todo.back().lasttr);
    	    fasttgba_state* d = todo.back().lasttr->current_state();
	    c = get_color (d);
    	    if (c == Unknown)
    	      {
		dfs_push (d);
    	    	continue;
    	      }
    	    else if (c == Alive)
    	      {
    	    	if (dfs_update (d))
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

  // ----------------------------------------------------------------------
  // Concurrent Tarjan Emptiness check  with shared union find.
  // ======================================================================
  void concur_opt_tarjan_ec::dfs_pop()
  {
    --dfs_size_;
    const markset acc = dstack_->top_acceptance();
    int ll = dstack_->pop();

    unsigned int steppos = todo.back().position;
    const fasttgba_state* last = todo.back().state;
    delete todo.back().lasttr;
    todo.pop_back();

    if ((int) steppos == ll)
      {
	++roots_poped_cpt_;
	int trivial = 0;

	// Delete the root that is not inside of live Stack
	uf_->make_dead(last);
	seen_map::const_iterator it1 = H.find(last);
	H.erase(it1);
	while (H.size() > steppos)
	  {
	    ++trivial;
	    //deadstore_->add(live.back());
	    seen_map::const_iterator it = H.find(live.back());
	    H.erase(it);
	    live.pop_back();
	  }

	// This change regarding original algorithm
	if (trivial == 0)
	  ++trivial_scc_;
      }
    else
      {
	// Warning !  Do not optimize to '<'
	// Here we check '<=' since the information "==" is
	// needed to the compressed stack of lowlink.
	if (ll <= dstack_->top())
	  dstack_->set_top(ll , acc | dstack_->top_acceptance() |
			   todo.back().lasttr->current_acceptance_marks());
	else
	  dstack_->set_top(dstack_->top(), acc | dstack_->top_acceptance() |
			   todo.back().lasttr->current_acceptance_marks());

	live.push_back(last);
	bool fast_backtrack = false;
	dstack_->set_top(dstack_->top(),
			 uf_->unite (last, todo.back().state,
				     dstack_->top_acceptance(),
				     &fast_backtrack));
	if (dstack_->top_acceptance().all())
	  counterexample_found = true;

	// if (fast_backtrack)
	//   assert(false);//fastbacktrack();
      }
  }




  bool concur_opt_tarjan_ec::dfs_update(fasttgba_state* d)
  {
    ++update_cpt_;
    // Warning !  Do not optimize to '<'
    // Here we check '<=' since the information "==" is
    // needed to the compressed stack of lowlink.
    if (H[d] <= dstack_->top())
      dstack_->set_top(H[d] ,
		       todo.back().lasttr->current_acceptance_marks() |
		       dstack_->top_acceptance());
    else
      dstack_->set_top(dstack_->top(),
		       todo.back().lasttr->current_acceptance_marks() |
		       dstack_->top_acceptance());

    bool fast_backtrack = false;
    dstack_->set_top(dstack_->top(),
    		     uf_->unite (d, todo.back().state,
    				 dstack_->top_acceptance(),
    				 &fast_backtrack));

    // if (fast_backtrack)
    //   assert(false);//fastbacktrack();

    return dstack_->top_acceptance().all();
  }

  // ----------------------------------------------------------------------
  // Definition of the core of Dead_share
  // ======================================================================

  dead_share::~dead_share()
  {
    delete uf_;
  }

  bool
  dead_share::check()
  {
    std::vector<std::thread> v;
    std::chrono::time_point<std::chrono::system_clock> start, end;

    // Must we stop the world? It is valid to use a non atomic variable
    // since it will only pass this variable to true once
    int stop = 0;

    // Let us instanciate the checker according to the policy
    std::vector<spot::concur_ec_stat*> chk;
    for (int i = 0; i < tn_; ++i)
      {
	if (policy_ == FULL_TARJAN)
	  chk.push_back(new spot::concur_opt_tarjan_scc(itor_, uf_,
							i, &stop));
	else if (policy_ == FULL_TARJAN_EC)
	  chk.push_back(new spot::concur_opt_tarjan_ec(itor_, uf_,
							i, &stop));
	else if (policy_ == FULL_DIJKSTRA)
	  chk.push_back(new spot::concur_opt_dijkstra_scc(itor_, uf_,
							  i, &stop));
	else
	  {
	    assert(policy_ == MIXED);
	    if (i%2)
	      chk.push_back(new spot::concur_opt_tarjan_scc(itor_, uf_,
							    i, &stop));
	    else
	      chk.push_back(new spot::concur_opt_dijkstra_scc(itor_, uf_,
							      i, &stop));
	  }
      }

    // Start Global Timer
    std::cout << "Start threads ..." << std::endl;
    start = std::chrono::system_clock::now();

    // Launch all threads
    for (int i = 0; i < tn_; ++i)
      v.push_back( std::thread ([&](int tid){
	    chk[tid]->check();
	  }, i));

    // Wait all threads
    for (int i = 0; i < tn_; ++i)
      {
	v[i].join();
      }

    // Stop Global Timer
    end  = std::chrono::system_clock::now();

    auto elapsed_seconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

    // Clean up checker and construct CSV
    bool ctrexple = false;
    auto tmpi = 0;
    for (int i = 0; i < tn_; ++i)
      {
	ctrexple |= chk[i]->has_counterexample();

	if (max_diff < std::abs(chk[i]->get_elapsed_time()
			   - chk[tmpi]->get_elapsed_time()))
	  {
	    max_diff = std::abs(chk[tmpi]->get_elapsed_time()
				- chk[i]->get_elapsed_time());
	    tmpi = i;
	  }

	std::cout << "      thread : " << i << "  csv : "
		  << (chk[i]->has_counterexample() ? "VIOLATED," : "VERIFIED,")
		  << chk[i]->get_elapsed_time() << ","
      		  << chk[i]->csv() << std::endl;
      }

    // Release memrory
    for (int i = 0; i < tn_; ++i)
      	delete chk[i];


    // Display results
    printf("[WF]  num of threads = %d insert = %d  elapsed time = %d\n\n",
	   tn_, uf_->size(), (int)elapsed_seconds);

    return ctrexple;
  }
}
