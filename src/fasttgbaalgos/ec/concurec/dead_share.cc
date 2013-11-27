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
  // Concurrent Tarjan Algorithm with shared union find.
  // ======================================================================
  concur_opt_tarjan_scc::concur_opt_tarjan_scc(instanciator* i,
					       spot::uf* uf,
					       int tn,
					       int *stop,
					       bool swarming,
					       std::string option)
    : opt_tarjan_scc(i, option, swarming)
  {
    uf_ = uf;
    tn_ = tn;
    stop_ = stop;
    make_cpt_ = 0;
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

    if (uf_->make_set(q, tn_))
      {
	q->clone();
	++make_cpt_;
      }

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
	    auto toerase = live.back();
	    seen_map::const_iterator it = H.find(toerase);
	    H.erase(it);
	    toerase->destroy();
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


  std::chrono::milliseconds::rep concur_opt_tarjan_scc::get_elapsed_time()
  {
    auto elapsed_seconds = std::chrono::duration_cast
      <std::chrono::milliseconds>(end-start).count();
    return elapsed_seconds;
  }

  std::string concur_opt_tarjan_scc::csv()
  {
    return "tarjan," + extra_info_csv();
  }

  int concur_opt_tarjan_scc::nb_inserted()
  {
    return make_cpt_;
  }

  // ----------------------------------------------------------------------
  // Concurrent Dijkstra Algorithm with shared union find.
  // ======================================================================
  concur_opt_dijkstra_scc::concur_opt_dijkstra_scc(instanciator* i,
						   spot::uf* uf,
						   int tn,
						   int *stop,
						   bool swarming,
						   std::string option)
    : opt_dijkstra_scc(i, option, swarming)
  {
    uf_ = uf;
    tn_ = tn;
    stop_ = stop;
    make_cpt_ = 0;
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

    if (uf_->make_set(s, tn_))
      ++make_cpt_;

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
	    auto toerase = live.back();
	    seen_map::const_iterator it = H.find(toerase);
	    H.erase(it);
	    toerase->destroy();
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

	if (fast_backtrack)
	  fastbacktrack();

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

    bool rv = dstack_->top_acceptance().all();
    if (!rv && fast_backtrack)
      fastbacktrack();

    return rv;
  }

  void concur_opt_tarjan_ec::fastbacktrack()
  {
    ++fastb_cpt_;
    return;

    // int ref = dstack_->top();
    // int i = 0;


    // //live.push_back(todo.back().state);
    // 	seen_map::const_iterator it = H.find(todo.back().state);
    // 	H.erase(it);
    // 	todo.back().state->destroy();
    // delete todo.back().lasttr;
    // todo.pop_back();
    // dstack_->pop();

    // while(dstack_->top() > ref)
    //   {
    // 	seen_map::const_iterator it = H.find(todo.back().state);
    // 	H.erase(it);
    // 	todo.back().state->destroy();
    // 	// live.push_back(todo.back().state);
    // 	delete todo.back().lasttr;
    // 	todo.pop_back();
    // 	dstack_->pop();
    //   }

    // if (!todo.empty())
    //   {
    // 	unsigned int steppos = todo.back().position;
    // 	// printf("H:%zu todo:%zu live:%zu steppos:%d ref:%d\n", H.size(),
    // 	//        todo.size(), live.size(), steppos, ref);

    // 	while (H.size() > steppos+1)
    // 	  {
    // 	    ++i;
    // 	    // if (live.empty())
    // 	    //   {
    // 	    // 	printf("%zu %zu %d \n", H.size(), todo.size(), steppos);
    // 	    // 	assert(false);
    // 	    //   }
    // 	    auto toerase = live.back();
    // 	    seen_map::const_iterator it = H.find(toerase);
    // 	    H.erase(it);
    // 	    toerase->destroy();
    // 	    live.pop_back();
    // 	  }
    //   }
    // fastb_cpt_ = fastb_cpt_ > i ? fastb_cpt_ : i;
    // //      std::cout << "FastBacktrack : " << i << "\n";
  }

  std::string concur_opt_tarjan_ec::csv()
  {
    return "tarjan_ec," + extra_info_csv() + "," + std::to_string(fastb_cpt_);
  }

  // ----------------------------------------------------------------------
  // Definition of the core of Dead_share
  // ======================================================================

  dead_share::dead_share(instanciator* i,
			 int thread_number,
			 DeadSharePolicy policy,
			 std::string option) :
    itor_(i), tn_(thread_number), policy_(policy), max_diff(0)
  {
    assert(i && thread_number && !option.compare(""));
    uf_ = new spot::uf(tn_);

    // Must we stop the world? It is valid to use a non atomic variable
    // since it will only pass this variable to true once
    stop = 0;

    // Let us instanciate the checker according to the policy
    for (int i = 0; i < tn_; ++i)
      {
	bool s_ = i != 0;

	if (policy_ == FULL_TARJAN)
	  chk.push_back(new spot::concur_opt_tarjan_scc(itor_, uf_,
							i, &stop, s_));
	else if (policy_ == FULL_TARJAN_EC)
	  chk.push_back(new spot::concur_opt_tarjan_ec(itor_, uf_,
						       i, &stop, s_));
	else if (policy_ == FULL_DIJKSTRA)
	  chk.push_back(new spot::concur_opt_dijkstra_scc(itor_, uf_,
							  i, &stop, s_));
	else
	  {
	    assert(policy_ == MIXED);
	    if (i%2)
	      chk.push_back(new spot::concur_opt_tarjan_scc(itor_, uf_,
							    i, &stop, s_));
	    else
	      chk.push_back(new spot::concur_opt_dijkstra_scc(itor_, uf_,
							      i, &stop, s_));
	  }
      }
  }

  dead_share::~dead_share()
  {
    // Release memory
    for (int i = 0; i < tn_; ++i)
      	delete chk[i];

    delete uf_;
  }

  bool
  dead_share::check()
  {
    std::vector<std::thread> v;
    std::chrono::time_point<std::chrono::system_clock> start, end;

    // Start Global Timer
    std::cout << "Start threads ..." << std::endl;
    start = std::chrono::system_clock::now();

    // Launch all threads
    for (int i = 0; i < tn_; ++i)
      v.push_back(std::thread ([&](int tid){
	    srand (tid); // Unused if tid equal 0
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
    for (int i = 0; i < tn_; ++i)
      ctrexple |= chk[i]->has_counterexample();

    // Display results
    printf("\n[WF]  num of threads = %d insert = %d  elapsed time = %d\n",
	   tn_, uf_->size(), (int)elapsed_seconds);

    return ctrexple;
  }

  void dead_share::dump_threads()
  {
    std::cout << std::endl << "THREADS DUMP : " << std::endl;
    auto tmpi = 0;
    for (int i = 0; i < tn_; ++i)
      {
	if (max_diff < std::abs(chk[i]->get_elapsed_time()
				- chk[tmpi]->get_elapsed_time()))
	  {
	    max_diff = std::abs(chk[tmpi]->get_elapsed_time()
				- chk[i]->get_elapsed_time());
	    tmpi = i;
	  }

	std::cout << "      thread : " << i << "  csv : "
		  << (chk[i]->has_counterexample() ? "VIOLATED,"
		      : "VERIFIED,")
		  << chk[i]->get_elapsed_time() << ","
		  << chk[i]->csv() << ","
		  << chk[i]->nb_inserted()
		  << std::endl;
      }
    std::cout << std::endl;
  }

  std::string dead_share::csv()
  {
    std::stringstream res;
    switch (policy_)
      {
      case FULL_TARJAN:
	res << "FULL_TARJAN,";
	break;
      case FULL_DIJKSTRA:
	res << "FULL_DIJKSTRA,";
	break;
      case MIXED:
	res << "MIXED,";
	break;
      case FULL_TARJAN_EC:
	res << "FULL_TARJAN_EC,";
	break;
      default:
	std::cout << "Error undefined thread policy" << std::endl;
	assert(false);
      }

    res << tn_  << "," << max_diff;
    return res.str();
  }

}
