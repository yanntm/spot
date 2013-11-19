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
#include "concur_ec_stat.hh"

#include "fasttgbaalgos/ec/opt/opt_tarjan_scc.hh"
#include "fasttgbaalgos/ec/opt/opt_dijkstra_scc.hh"

#include <sstream>

namespace spot
{

  /// \brief The method that will be used by thread performing a Tarjan SCC
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
			  bool swarming,
			  std::string option = "");

    virtual bool check();

    virtual void dfs_push(fasttgba_state* q);

    virtual color get_color(const fasttgba_state* state);

    virtual void dfs_pop();

    virtual bool dfs_update (fasttgba_state* s);

    virtual void main ();

    virtual bool has_counterexample();

    virtual std::string csv()
    {
      return "tarjan," + extra_info_csv();
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
    int * stop_;		/// \brief stop the world varibale
    std::chrono::time_point<std::chrono::system_clock> start;
    std::chrono::time_point<std::chrono::system_clock> end;
    int make_cpt_;
  };

  class concur_opt_dijkstra_scc : public opt_dijkstra_scc, public concur_ec_stat
  {
  public:
    concur_opt_dijkstra_scc(instanciator* i,
			    spot::uf* uf,
			    int thread_number,
			    int *stop,
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
    int * stop_;		/// \brief stop the world varibale
    std::chrono::time_point<std::chrono::system_clock> start;
    std::chrono::time_point<std::chrono::system_clock> end;
    int make_cpt_;
  };


  class concur_opt_tarjan_ec : public concur_opt_tarjan_scc
  {
  public:
    concur_opt_tarjan_ec(instanciator* i,
			 spot::uf* uf,
			 int thread_number,
			 int *stop,
			 bool swarming,
			 std::string option = "")
      : concur_opt_tarjan_scc(i, uf, thread_number, stop, swarming, option)
    { }


    /// \brief  Pop states already explored
    virtual void dfs_pop();

    /// \brief the update for backedges
    virtual bool dfs_update (fasttgba_state* s);

    virtual void fastbacktrack()
    {
      //assert(!todo.empty());
      int ref = dstack_->top();
      int i = 0;
      while ( dstack_->top() >= ref)
	{
	  ++i;
	  seen_map::const_iterator it1 = H.find(todo.back().state);
	  H.erase(it1);
	  todo.pop_back();
	  dstack_->pop();

	  if (todo.empty())
	    break;
	}

      while (H.size() > (unsigned) ref)
	  {
	    seen_map::const_iterator it = H.find(live.back());
	    H.erase(it);
	    live.pop_back();
	  }


      std::cout << "FastBacktrack : " << i << std::endl;
    }

    /// \brief Display the csv of for this thread
    virtual std::string csv()
    {
      return "tarjan_ec," + extra_info_csv();
    }
  };


  /// \brief Wrapper Launch all threads
  class dead_share: public ec
  {
  public:

    /// \brief The walk policy to be used by threads
    enum  DeadSharePolicy
      {
	FULL_TARJAN = 0,	/// \brief All threads use Tarjan Algorithm
	FULL_DIJKSTRA = 1,	/// \brief All threads use Dijkstra Algorithm
	MIXED = 2,		/// \brief Combinaison of both previous
	FULL_TARJAN_EC = 3	/// \brief All treads use Tarjan Emptiness Check
      };

    dead_share(instanciator* i,
	       int thread_number = 1,
	       DeadSharePolicy policy = FULL_TARJAN,
	       std::string option = "") :
      itor_(i), tn_(thread_number), policy_(policy), max_diff(0)
    {
      assert(i && thread_number && !option.compare(""));
      uf_ = new spot::uf(tn_);
    }

    virtual ~dead_share();

    bool check();

    virtual std::string csv()
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


  protected:
    spot::uf* uf_;
    instanciator* itor_;
    int tn_;
    DeadSharePolicy policy_;
    std::chrono::milliseconds::rep max_diff;
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_CONCUREC_DEAD_SHARE_HH
