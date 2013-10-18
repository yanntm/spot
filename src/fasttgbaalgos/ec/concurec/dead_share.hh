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
			  std::string option = "");

    virtual bool check();

    virtual void dfs_push(fasttgba_state* q);

    virtual color get_color(const fasttgba_state* state);

    virtual void dfs_pop();

    virtual bool has_counterexample();

    virtual std::string csv()
    {
      return "tarjan," + extra_info_csv();
    }

  private:
    spot::uf* uf_;		/// \brief a reference to shared union find
    int tn_;
    int * stop_;		/// \brief stop the world varibale
  };

  class concur_opt_dijkstra_scc : public opt_dijkstra_scc, public concur_ec_stat
  {
  public:
    concur_opt_dijkstra_scc(instanciator* i,
			    spot::uf* uf,
			    int thread_number,
			    int *stop,
			    std::string option = "");

    virtual bool check();

    virtual void dfs_push(fasttgba_state* q);

    virtual color get_color(const fasttgba_state* state);

    virtual bool merge(fasttgba_state* d);

    virtual void dfs_pop();

    virtual bool has_counterexample();

    virtual std::string csv()
    {
      return "dijkstra," + extra_info_csv();
    }

  private:
    spot::uf* uf_;		/// \brief a reference to shared union find
    int tn_;
    int * stop_;		/// \brief stop the world varibale
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
	MIXED = 2		/// \brief Combinaison of both previous
      };

    dead_share(instanciator* i,
	       int thread_number = 1,
	       DeadSharePolicy policy = FULL_TARJAN,
	       std::string option = "") :
      itor_(i), tn_(thread_number), policy_(policy)
    {
      assert(i && thread_number && !option.compare(""));
      uf_ = new spot::uf(tn_);
    }

    virtual ~dead_share();

    bool check();

    virtual std::string csv()
    {
      return "FIXME";
    }


  protected:
    spot::uf* uf_;
    instanciator* itor_;
    int tn_;
    DeadSharePolicy policy_;
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_CONCUREC_DEAD_SHARE_HH
