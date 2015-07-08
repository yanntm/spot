// Copyright (C) 2012 Laboratoire de Recherche et Développement
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

#ifndef SPOT_FASTTGBAALGOS_EC_CONCUREC_DEAD_SHARE_HH
# define SPOT_FASTTGBAALGOS_EC_CONCUREC_DEAD_SHARE_HH

#include <tuple>

#include <stack>
#include <map>

#include "fasttgbaalgos/ec/ec.hh"
#include "fasttgbaalgos/ec/deadstore.hh"
#include "fasttgbaalgos/ec/lowlink_stack.hh"
#include "fasttgbaalgos/ec/lazycheck.hh"

#include "fasttgbaalgos/ec/concur/uf.hh"
#include "fasttgbaalgos/ec/concur/queue.hh"
#include "fasttgbaalgos/ec/concur/sharedhashtable.hh"
#include "fasttgbaalgos/ec/concur/openset.hh"
#include "concur_ec_stat.hh"

#include "fasttgbaalgos/ec/opt/opt_tarjan_scc.hh"
#include "fasttgbaalgos/ec/opt/opt_dijkstra_scc.hh"
#include "fasttgbaalgos/ec/opt/opt_ndfs.hh"
#include "fasttgbaalgos/ec/unioncheck.hh"

#include <sstream>
#include <iostream>

namespace spot
{
  // ----------------------------------------------------------------------
  // Tarjan Concurrent SCC Computation
  // ======================================================================

  /// \brief The class that will be used by thread performing a Tarjan SCC
  ///
  /// This class only redefine methods that are specific to multi threading env.
  /// The deadstore is remove and the thread use a shared union find to mark
  /// states  dead.
  class concur_opt_tarjan_scc : public opt_tarjan_scc, public concur_ec_stat
  {
  public:
    concur_opt_tarjan_scc(instanciator* i,
			  spot::uf* uf,
			  int thread_number,
			  int *stop,
			  int *stop_strong,
			  bool swarming,
			  std::string option = "");

    virtual bool check();

    virtual void dfs_push(fasttgba_state* q);

    virtual color get_color(const fasttgba_state* state);

    virtual void dfs_pop();

    virtual bool dfs_update (fasttgba_state* s);

    virtual void main ();

    virtual bool has_counterexample();

    virtual std::string csv();

    virtual std::chrono::milliseconds::rep  get_elapsed_time();

    virtual int nb_inserted();

  protected:
    spot::uf* uf_;		/// \brief a reference to shared union find
    int tn_;			/// \brief the thread identifier
    int * stop_;		/// \brief stop the world variable
    int * stop_strong_;		/// \brief stop strong variable
    std::chrono::time_point<std::chrono::system_clock> start; /// \brief start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \brief stop!
    int make_cpt_;		/// \brief number of succed insertions
  };

  // ----------------------------------------------------------------------
  // Disjkstra Concurrent SCC Computation
  // ======================================================================

  /// \brief The class that will be used by thread performing a Dijkstra SCC
  ///
  /// This class only redefine methods that are specific to multi threading env.
  /// The deadstore is remove and the thread use a shared union find to mark
  /// states  dead.
  class concur_opt_dijkstra_scc : public opt_dijkstra_scc, public concur_ec_stat
  {
  public:
    concur_opt_dijkstra_scc(instanciator* i,
			    spot::uf* uf,
			    int thread_number,
			    int *stop,
			    int *stop_strong,
			    bool swarming,
			    std::string option = "");

    virtual bool check();

    virtual void dfs_push(fasttgba_state* q);

    virtual color get_color(const fasttgba_state* state);

    virtual bool merge(fasttgba_state* d);

    virtual void dfs_pop();

    virtual void main ();

    virtual bool has_counterexample();

    virtual std::string csv()
    {
      return "dijkstra," + extra_info_csv();
    }

    virtual std::chrono::milliseconds::rep
    get_elapsed_time()
    {
      auto elapsed_seconds = std::chrono::duration_cast
	<std::chrono::milliseconds>(end-start).count();
      return elapsed_seconds;
    }

    virtual int nb_inserted()
    {
      return make_cpt_;
    }

  protected:
    spot::uf* uf_;		/// \brief a reference to shared union find
    int tn_;
    int * stop_;		/// \brief stop the world variable
    int * stop_strong_;		/// \brief stop strong variable
    std::chrono::time_point<std::chrono::system_clock> start;
    std::chrono::time_point<std::chrono::system_clock> end;
    int make_cpt_;
  };

  // ----------------------------------------------------------------------
  // Tarjan Concurrent Emptiness Check
  // ======================================================================

  /// \brief An emptiness based on the tarjan parallel computation algorithm
  /// above
  class concur_opt_tarjan_ec : public concur_opt_tarjan_scc
  {
  public:
    /// \brief A constuctor
    concur_opt_tarjan_ec(instanciator* i,
			 spot::uf* uf,
			 int thread_number,
			 int *stop,
			 int *stop_strong,
			 bool swarming,
			 std::string option = "-cs")
      : concur_opt_tarjan_scc(i, uf, thread_number,
			      stop, stop_strong, swarming, option)
    {
      fastb_cpt_ = 0;
    }

    /// \brief  Pop states already explored
    virtual void dfs_pop();

    /// \brief the update for backedges
    virtual bool dfs_update (fasttgba_state* s);

    /// \brief Speed up the backtrack when the current state as been
    /// already marked dead by another thread.
    virtual void fastbacktrack();

    /// \brief Display the csv of for this thread
    virtual std::string csv();

  private:
    int fastb_cpt_;
  };


  // ----------------------------------------------------------------------
  // Dijkstra Concurrent Emptiness Check
  // ======================================================================

  /// \brief An emptiness based on the dijkstra parallel computation algorithm
  /// above
  class concur_opt_dijkstra_ec : public concur_opt_dijkstra_scc
  {
  public:
    /// \brief A constuctor
    concur_opt_dijkstra_ec(instanciator* i,
			   spot::uf* uf,
			   int thread_number,
			   int *stop,
			   int *stop_strong,
			   bool swarming,
			   std::string option = "-cs")
      : concur_opt_dijkstra_scc(i, uf, thread_number, stop,
				stop_strong,  swarming, option)
    {
      fastb_cpt_ = 0;
    }

    /// \brief The update for backedges
    virtual bool merge(fasttgba_state* d);

    /// \brief Display the csv of for this thread
    virtual std::string csv();

    /// \brief Speed up the backtrack when the current state as been
    /// already marked dead by another thread.
    virtual void fastbacktrack();

  private:
    int fastb_cpt_;
  };


  // ----------------------------------------------------------------------
  // Reachability Concurrent Emptiness Check
  // ======================================================================

  class concur_reachability_ec : public concur_ec_stat
  {
  public:
    /// \brief A constuctor
    concur_reachability_ec(instanciator* i,
			   spot::openset* os,
			   int thread_number,
			   int total_threads,
			   int *stop,
			   int *stop_terminal,
			   std::atomic<int>& giddle,
			   std::string option = "");

    /// \brief A simple destructor
    virtual ~concur_reachability_ec();

    bool check();

    /// \brief check wether a state is synchronised with a terminal
    /// state of the property automaton
    bool is_terminal(const fasttgba_state* );

    virtual bool has_counterexample();

    virtual std::string csv();

    virtual std::chrono::milliseconds::rep get_elapsed_time();

    virtual int nb_inserted();

  private:
    int tn_;			///< The id of the thread
    int tt_;			///< Total number of threads
    int insert_cpt_;		///< The number of successfull insert
    int fail_cpt_;		///< The number of failled insert
    bool iddle_;		///< Is this thread iddle
    int * stop_;		///< stop the world variable
    int * stop_terminal_;	///< stop the terminal variable
    spot::openset* os_;		///< The shared Open Set
    const fasttgba* a_;         ///< The automaton to check
    const instance_automaton* inst; ///< The instanciator
    bool counterexample_;	    ///< Wether a counterexample exist

    /// \brief the store that will keep states discovered by this thread
    std::unordered_set<const fasttgba_state*,
		       fasttgba_state_ptr_hash,
		       fasttgba_state_ptr_equal> store;

    /// \brief to avoid terminaison problem
    std::atomic<int>& giddle_;

    std::chrono::time_point<std::chrono::system_clock> start; /// \brief start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \brief stop!
  };


  // ----------------------------------------------------------------------
  // Weak Concurrent Emptiness Check
  // ======================================================================

  class concur_weak_ec : public concur_ec_stat
  {
  public:
    /// \brief A constuctor
    concur_weak_ec(instanciator* i,
		   spot::sharedhashtable* sht,
		   int thread_number,
		   int *stop,
		   int *stop_weak,
		   std::string option = "");
    /// \brief A simple destructor
    virtual ~concur_weak_ec();
    void push_state(const spot::fasttgba_state* state);

    enum color {Alive, Dead, Unknown};

    color get_color(const spot::fasttgba_state* state);
    void dfs_pop();
    bool check();
    void accepting_cycle_check(const fasttgba_state* left,
			       const fasttgba_state* right);
    virtual bool has_counterexample();
    virtual std::string csv();
    virtual std::chrono::milliseconds::rep get_elapsed_time();
    virtual int nb_inserted();

  private:
    const instance_automaton* inst;
    const fasttgba* a_;
    sharedhashtable* sht_;

    int tn_;			///< The id of the thread
    int insert_cpt_;		///< The number of successfull insert
    int * stop_;		///< stop the world variable
    int * stop_weak_;	        ///< stop the terminal variable
    bool counterexample_;	    ///< Wether a counterexample exist

    struct pair_state_iter
    {
      const spot::fasttgba_state* state;
      fasttgba_succ_iterator* lasttr;
    };

    std::vector<pair_state_iter> todo;

    /// The map of visited states
    typedef std::unordered_set<const fasttgba_state*,
		       fasttgba_state_ptr_hash,
		       fasttgba_state_ptr_equal> seen_map;
    seen_map H;

    /// \brief the store that will keep states discovered by this thread
    std::unordered_set<const fasttgba_state*,
    		       fasttgba_state_ptr_hash,
    		       fasttgba_state_ptr_equal> store;

    std::chrono::time_point<std::chrono::system_clock> start; /// \bref start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \bref stop!
  };

  // ======================================================================
  // Fake Ec when needed.
  // ======================================================================


  class fake_ec : public concur_ec_stat
  {
  public:
    /// \brief A constuctor
    fake_ec(int thread_number): tn_(thread_number)
    {
      (void) tn_;
    }

    virtual
    bool has_counterexample()
    {
      return false;
    }

    virtual
    std::string csv()
    {
      return "no_ec,0,0,0,0,0,0,0,0,0,0,0,0";
    }

    virtual std::chrono::milliseconds::rep get_elapsed_time()
    {
      return 0;
    }

    virtual int nb_inserted()
    {
      return 0;
    }

    virtual bool check()
    {
      return false;
    }

  private:
    int tn_;
  };

  // ======================================================================
  // Reachability for only one thread.
  // ======================================================================
  class reachability_ec : public concur_ec_stat
  {
  public:
    /// \brief A constuctor
    reachability_ec(instanciator* i,
		    int thread_number,
		    int *stop);

    /// \brief A simple destructor
    virtual ~reachability_ec();
    bool check();
    void push_state(const spot::fasttgba_state* state);
    enum color {Alive, Dead, Unknown};
    color get_color(const spot::fasttgba_state* state);

    /// \brief check wether a state is synchronised with a terminal
    /// state of the property automaton
    bool is_terminal(const fasttgba_state* s);
    virtual bool has_counterexample();
    virtual std::string csv();
    virtual std::chrono::milliseconds::rep get_elapsed_time();
    virtual int nb_inserted();

  private:
    int tn_;			///< The id of the thread
    int insert_cpt_;		///< The number of successfull insert
    int * stop_;		///< stop the world variable
    const fasttgba* a_;         ///< The automaton to check
    const instance_automaton* inst; ///< The instanciator
    bool counterexample_;	    ///< Wether a counterexample exist

    struct pair_state_iter
    {
      const spot::fasttgba_state* state;
      fasttgba_succ_iterator* lasttr;
    };

    std::vector<pair_state_iter> todo;
    /// \brief the store that will keep states discovered by this thread
    typedef std::unordered_set<const fasttgba_state*,
		       fasttgba_state_ptr_hash,
		       fasttgba_state_ptr_equal> seen_map;
    seen_map H;

    std::chrono::time_point<std::chrono::system_clock> start; /// \brief start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \brief stop!
    deadstore* deadstore_;
    unsigned int transitions_cpt_;
    unsigned int max_dfs_size_;
    unsigned int memory_cost_;
    unsigned int max_live_size_;
    unsigned int update_cpt_;
  };


  // ----------------------------------------------------------------------
  // Single Weak Emptiness Check
  // ======================================================================

  class weak_ec : public concur_ec_stat
  {
  public:
    /// \brief A constuctor
    weak_ec(instanciator* i,
	    int thread_number,
	    int *stop);
    /// \brief A simple destructor
    virtual ~weak_ec();
    void push_state(const spot::fasttgba_state* state);

    enum color {Alive, Dead, Unknown};

    color get_color(const spot::fasttgba_state* state);
    void dfs_pop();
    bool check();
    void accepting_cycle_check(const fasttgba_state* left,
			       const fasttgba_state* right);
    virtual bool has_counterexample();
    virtual std::string csv();
    virtual std::chrono::milliseconds::rep get_elapsed_time();
    virtual int nb_inserted();

  private:
    const instance_automaton* inst;
    const fasttgba* a_;
    deadstore* deadstore_;


    int tn_;			///< The id of the thread
    int insert_cpt_;		///< The number of successfull insert
    int * stop_;		///< stop the world variable
    bool counterexample_;	    ///< Wether a counterexample exist

    struct pair_state_iter
    {
      const spot::fasttgba_state* state;
      fasttgba_succ_iterator* lasttr;
    };

    std::vector<pair_state_iter> todo;

    /// The map of visited states
    typedef std::unordered_set<const fasttgba_state*,
		       fasttgba_state_ptr_hash,
		       fasttgba_state_ptr_equal> seen_map;
    seen_map H;

    std::chrono::time_point<std::chrono::system_clock> start; /// \bref start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \bref stop!
    unsigned int transitions_cpt_;
    unsigned int max_dfs_size_;
    unsigned int memory_cost_;
    unsigned int max_live_size_;
    unsigned int update_cpt_;
  };




  class single_opt_tarjan_ec : public opt_tarjan_ec, public concur_ec_stat
  {
  public:
    single_opt_tarjan_ec(instanciator* i,
			 int thread_number,
			 int *stop,
			 std::string option = "");
    virtual void main ();
    virtual bool check();
    virtual bool has_counterexample();
    virtual std::string csv();
    virtual std::chrono::milliseconds::rep  get_elapsed_time();
    virtual int nb_inserted();
  protected:
    int tn_;			/// \brief the thread identifier
    int * stop_;		/// \brief stop the world variable
    std::chrono::time_point<std::chrono::system_clock> start; /// \brief start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \brief stop!
    int make_cpt_;		/// \brief number of succed insertions
  };

  class single_opt_dijkstra_ec : public opt_dijkstra_ec, public concur_ec_stat
  {
  public:
    single_opt_dijkstra_ec(instanciator* i,
			 int thread_number,
			 int *stop,
			 std::string option = "");
    virtual void main ();
    virtual bool check();
    virtual bool has_counterexample();
    virtual std::string csv();
    virtual std::chrono::milliseconds::rep  get_elapsed_time();
    virtual int nb_inserted();
  protected:
    int tn_;			/// \brief the thread identifier
    int * stop_;		/// \brief stop the world variable
    std::chrono::time_point<std::chrono::system_clock> start; /// \brief start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \brief stop!
    int make_cpt_;		/// \brief number of succed insertions
  };


  class single_opt_ndfs_ec : public opt_ndfs, public concur_ec_stat
  {
  public:
    single_opt_ndfs_ec(instanciator* i,
			 int thread_number,
			 int *stop,
			 std::string option = "");
    virtual void main ();
    virtual bool check();
    virtual bool has_counterexample();
    virtual std::string csv();
    virtual std::chrono::milliseconds::rep  get_elapsed_time();
    virtual int nb_inserted();
  protected:
    int tn_;			/// \brief the thread identifier
    int * stop_;		/// \brief stop the world variable
    std::chrono::time_point<std::chrono::system_clock> start; /// \brief start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \brief stop!
    int make_cpt_;		/// \brief number of succed insertions
  };


  class single_opt_uc13_ec : public unioncheck, public concur_ec_stat
  {
  public:
    single_opt_uc13_ec(instanciator* i,
			 int thread_number,
			 int *stop,
			 std::string option = "");
    virtual void main ();
    virtual bool check();
    virtual bool has_counterexample();
    virtual std::string csv();
    virtual std::chrono::milliseconds::rep  get_elapsed_time();
    virtual int nb_inserted();
  protected:
    int tn_;			/// \brief the thread identifier
    int * stop_;		/// \brief stop the world variable
    std::chrono::time_point<std::chrono::system_clock> start; /// \brief start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \brief stop!
    int make_cpt_;		/// \brief number of succed insertions
  };


  class single_opt_tuc13_ec : public tarjanunioncheck, public concur_ec_stat
  {
  public:
    single_opt_tuc13_ec(instanciator* i,
			 int thread_number,
			 int *stop,
			 std::string option = "");
    virtual void main ();
    virtual bool check();
    virtual bool has_counterexample();
    virtual std::string csv();
    virtual std::chrono::milliseconds::rep  get_elapsed_time();
    virtual int nb_inserted();
  protected:
    int tn_;			/// \brief the thread identifier
    int * stop_;		/// \brief stop the world variable
    std::chrono::time_point<std::chrono::system_clock> start; /// \brief start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \brief stop!
    int make_cpt_;		/// \brief number of succed insertions
  };


  // ----------------------------------------------------------------------
  // Async Emptiness check
  // ======================================================================

  class async_worker: public concur_ec_stat
  {
  public:
    async_worker(instanciator* i,
		 spot::uf* uf,
		 spot::queue* queue,
		 int thread_number,
		 int *stop,
		 int *stop_strong,
		 std::string option = "")
    {
      uf_ = uf;
      tn_ = thread_number;
      queue_ = queue;
      stop_ = stop;
      inst = i->new_instance();
      counterexample = false;
      nbunite_ = 0;
      (void) option;
      (void) stop_strong;
      a_  = inst->get_automaton ();
    }

    virtual ~async_worker()
    {
      delete inst;
    }
    const fasttgba* a_;
    virtual bool check()
    {
      markset m1(a_->get_acc());
      start = std::chrono::system_clock::now();
      while (!*stop_)
	{
	  // Do not destroy !
	  shared_op* op = queue_->get(stop_);
	  if (!op)
	    break;
	  if (op->op_ == op_type::the_end)
	    break;
	  else if (op->op_ == op_type::unite)
	    {
	      ++nbunite_;
	      bool tmp; /* useless in this special case*/
	      m1 = (m1 & 0U) |
		uf_->make_and_unite ((const fasttgba_state*)op->arg1_,
				     (const fasttgba_state*)op->arg2_,
				     op->acc_,
				     &tmp, tn_);
	      if (m1.all())
	      	{
		  counterexample = true;
	      	  break;
	      	}
	    }
	  else if (op->op_ == op_type::makedead)
	    uf_->make_and_makedead((const fasttgba_state*)op->arg1_, tn_);
	}
      end = std::chrono::system_clock::now();
      *stop_ = 1;
      return counterexample;
    }

    virtual bool has_counterexample()
    {
      return counterexample;
    }

    virtual std::string csv()
    {
      return std::to_string(nbunite_) + "," + "consumer";
    }

    virtual std::chrono::milliseconds::rep  get_elapsed_time()
    {
      auto elapsed_seconds = std::chrono::duration_cast
	<std::chrono::milliseconds>(end-start).count();
      return elapsed_seconds;
    }

    virtual int nb_inserted()
    {
      return 0;
    }

  protected:
    spot::uf* uf_;
    spot::queue* queue_;
    const instance_automaton* inst; ///< The instanciator
    int tn_;			    /// \brief the thread identifier
    int * stop_;		    /// \brief stop the world variable
    std::chrono::time_point<std::chrono::system_clock> start; /// \brief start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \brief stop!
    bool counterexample;
    int nbunite_;
  };


  class dijkstra_async : public opt_dijkstra_scc, public concur_ec_stat
  {
  public:
    dijkstra_async(instanciator* i,
		   spot::uf* uf,
		   spot::queue* queue,
		   int thread_number,
		   int *stop,
		   int *stop_strong,
		   bool swarming,
		   std::string option = "")
      : opt_dijkstra_scc(i, option, swarming, thread_number)
    {
      uf_ = uf;
      queue_ = queue;
      tn_ = thread_number;
      stop_ = stop;
      stop_strong_ = stop_strong; /* Useless? */
      make_cpt_ = 0;
    }


    virtual bool check()
    {
      start = std::chrono::system_clock::now();
      init();
      main();
      *stop_strong_ = 1;
      end  = std::chrono::system_clock::now();
      queue_->put(the_end, 0, 0, 0, tn_);
      *stop_ = 1; // Ok since this algo is exact!
      return counterexample_found;
    }

    virtual void dfs_push(fasttgba_state* s)
    {
      ++states_cpt_;
      assert(H.find(s) == H.end());
      H.insert(std::make_pair(s, H.size()));

      // Count!
      max_live_size_ = H.size() > max_live_size_ ?
	H.size() : max_live_size_;

      stack_->push_transient(todo.size());

      todo.push_back ({s, 0, H.size() -1});
      // Count!
      max_dfs_size_ = max_dfs_size_ > todo.size() ?
	max_dfs_size_ : todo.size();


      int tmp_cost = 1*stack_->size() + 2*H.size() + 1*live.size()
	+ (deadstore_? deadstore_->size() : 0);
      if (tmp_cost > memory_cost_)
	memory_cost_ = tmp_cost;

    }

    virtual color get_color(const fasttgba_state* state)
    {
      seen_map::const_iterator i = H.find(state);
      if (i != H.end())
	return Alive;
      else if (deadstore_->contains(state) ||
	       uf_->is_maybe_dead(state))
      	return Dead;
      else
	return Unknown;

    }

    virtual bool merge(fasttgba_state* d)
    {
      ++update_cpt_;
      assert(H.find(d) != H.end());

      int dpos = H[d];

      auto top = stack_->pop(todo.size()-1);
      top.acc |= todo.back().lasttr->current_acceptance_marks();
      int r = top.pos;
      assert(todo[r].state);

      while ((unsigned)dpos < todo[r].position)
	{

	  ++update_loop_cpt_;
	  assert(todo[r].lasttr);
	  auto newtop = stack_->top(r-1);
	  int oldr = r;
	  r = newtop.pos;

	  // [r-1] Because acceptances are stored in the predecessor!
	  top.acc |= newtop.acc |
	    todo[oldr-1].lasttr->current_acceptance_marks();

	  // FIXME Do something
	  //uf_->unite (d, todo[r].state,  top.acc, &fast_backtrack);
	  todo[r].state->clone();
	  d->clone(); // FIXME
	  queue_->put(unite, d, todo[r].state,
		top.acc.to_ulong(), tn_);
	  stack_->pop(oldr -1);
	}
      stack_->push_non_transient(r, top.acc);
      return top.acc.all();
    }

    virtual void dfs_pop()
    {
      auto pair = todo.back();
      delete pair.lasttr;
      todo.pop_back();

      if (todo.size() == stack_->top(todo.size()).pos)
	{
	  ++roots_poped_cpt_;
	  stack_->pop(todo.size());
	  int trivial = 0;
	  //uf_->make_dead(pair.state);
	  pair.state->clone();
	  assert(deadstore_);
	  deadstore_->add(pair.state);
	  queue_->put(makedead, pair.state, 0, 0, tn_);
	  seen_map::const_iterator it1 = H.find(pair.state);
	  H.erase(it1);
	  while (H.size() > pair.position)
	    {
	      ++trivial;
	      auto toerase = live.back();
	      deadstore_->add(toerase);
	      seen_map::const_iterator it = H.find(toerase);
	      H.erase(it);
	      live.pop_back();
	    }
	  if (trivial == 0) // we just popped a trivial
	    ++trivial_scc_;
	}
      else
	{
	  // This is the integration of Nuutila's optimisation.
	  live.push_back(pair.state);
	}
    }

    virtual void main ()
    {
    opt_dijkstra_scc::color c;
    while (!todo.empty() && !*stop_)
      {
	if (!todo.back().lasttr)
	  {
	    todo.back().lasttr = swarm_ ?
	      a_->swarm_succ_iter(todo.back().state, tn_) :
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
		    *stop_ = 1;
    	    	    counterexample_found = true;
    	    	    d->destroy();
    	    	    return;
    	    	  }
    	      }
    	    d->destroy();
    	  }
      }
    }

    virtual bool has_counterexample()
    {
      return counterexample_found;
    }


    virtual std::string csv()
    {
      return "dijkstra_async," + extra_info_csv();
    }

    virtual std::chrono::milliseconds::rep
    get_elapsed_time()
    {
      auto elapsed_seconds = std::chrono::duration_cast
	<std::chrono::milliseconds>(end-start).count();
      return elapsed_seconds;
    }

    virtual int nb_inserted()
    {
      return make_cpt_;
    }

  protected:
    spot::uf* uf_;		/// \brief a reference to shared union find
    spot::queue* queue_;
    int tn_;
    int * stop_;		/// \brief stop the world variable
    int * stop_strong_;		/// \brief stop strong variable
    std::chrono::time_point<std::chrono::system_clock> start;
    std::chrono::time_point<std::chrono::system_clock> end;
    int make_cpt_;
  };






















  /// \brief Wrapper Launch all threads
  class SPOT_API dead_share: public ec
  {
  public:

    /// \brief The walk policy to be used by threads
    enum  DeadSharePolicy
      {
	FULL_TARJAN = 0,	/// \brief All threads use Tarjan Algorithm
	FULL_DIJKSTRA = 1,	/// \brief All threads use Dijkstra Algorithm
	MIXED = 2,		/// \brief Combinaison of both previous
	FULL_TARJAN_EC = 3,	/// \brief All treads use Tarjan Emptiness Check
	FULL_DIJKSTRA_EC = 4,
	MIXED_EC = 5,
	DECOMP_EC = 6,
	REACHABILITY_EC = 7,
	DECOMP_EC_SEQ = 8,
	DECOMP_TACAS13_TARJAN = 9,
	DECOMP_TACAS13_DIJKSTRA = 10,
	DECOMP_TACAS13_NDFS = 11,
	DECOMP_TACAS13_UC13 = 12,
	DECOMP_TACAS13_TUC13 = 13,
	ASYNC_DIJKSTRA = 14,
	W2_ASYNC_DIJKSTRA = 15
      };

    /// \brief Constructor for the multithreaded emptiness check
    ///
    /// This emptiness check is a wrapper for many and the policy to
    /// use can be defined using the \policy parameter.
    ///
    /// \param thread_number the number of thread to use
    dead_share(instanciator* i,
	       int thread_number = 1,
	       DeadSharePolicy policy = FULL_TARJAN,
	       std::string option = "");

    /// \brief Release all memory
    virtual ~dead_share();

    /// \brief launch every thread with associated data and
    /// wait them to end.
    bool check();

    /// \brief A CSV containing :
    ///    - the Name of the algo used
    ///    - the number of threads in use
    ///    - the maximum elapsed time between 2 threads stop!
    virtual std::string csv();

    /// Additionnal information threads by threads
    void dump_threads();

  protected:
    spot::uf* uf_;		///< The shared Union Find
    spot::queue* queue_;	///< The shared Queue
    spot::openset* os_;		///< The shared Open Set
    sharedhashtable* sht_;	///< The shared Hash Table
    instanciator* itor_;	///< The instanciator
    int tn_;			///< The number of threads
    DeadSharePolicy policy_;	///< The current policy to use
    std::chrono::milliseconds::rep max_diff; ///< Elapse time between 2 stops
    std::vector<spot::concur_ec_stat*> chk;  ///< Local data for each threads
    int stop;				     ///< Stop the world variable
    int stop_terminal;			     ///< Stop terminal variable
    int stop_weak;			     ///< Stop terminal variable
    std::atomic<int> term_iddle_;
    int stop_strong;			     ///< Stop strong variable
    std::string option_;		     ///< option to pass to ec.
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_CONCUREC_DEAD_SHARE_HH
