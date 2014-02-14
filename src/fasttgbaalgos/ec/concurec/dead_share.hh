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
    int * stop_;		/// \brief stop the world varibale
    std::chrono::time_point<std::chrono::system_clock> start; /// \biref start!
    std::chrono::time_point<std::chrono::system_clock> end;   /// \biref stop!
    int make_cpt_;		/// \biref number of succed insertions
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
			 bool swarming,
			 std::string option = "-cs")
      : concur_opt_tarjan_scc(i, uf, thread_number, stop, swarming, option)
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
			 bool swarming,
			 std::string option = "-cs")
      : concur_opt_dijkstra_scc(i, uf, thread_number, stop, swarming, option)
    {
      fastb_cpt_ = 0;
    }

    /// \brief The update for backedges
    virtual bool merge(fasttgba_state* d);

    /// \brief Display the csv of for this thread
    virtual std::string csv();

  private:
    int fastb_cpt_;
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
	MIXED_EC = 5
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
    instanciator* itor_;	///< The instanciator
    int tn_;			///< The number of threads
    DeadSharePolicy policy_;	///< The current policy to use
    std::chrono::milliseconds::rep max_diff; ///< Elapse time between 2 stops
    std::vector<spot::concur_ec_stat*> chk;  ///< Local data for each threads
    int stop;				     ///< Stop the world variable
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_CONCUREC_DEAD_SHARE_HH
