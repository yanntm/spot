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

#ifndef SPOT_FASTTGBAALGOS_EC_COU99_UF_SWARM_HH
# define SPOT_FASTTGBAALGOS_EC_COU99_UF_SWARM_HH

#include <stack>
#include "misc/hash.hh"
#include "union_find.hh"
#include "fasttgba/fasttgba.hh"
#include "ec.hh"
#include "cou99_uf.hh"
#include "cou99.hh"

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace spot
{
  /// This class implements the SWARM algorithm for emptiness
  /// check. This is a parallel algorihtm with no sharing so
  /// as soon as one of the threads finish the result can be
  /// return
  class cou99_uf_swarm : public ec
  {
  public:

    /// A constructor taking the automaton to check
    cou99_uf_swarm (instanciator* i,  int thread_number);

    /// A destructor
    virtual ~cou99_uf_swarm();

    /// \brief The implementation of the interface
    bool check();

    /// \brief the main function of each thread
    void do_work(unsigned int);

  private:
    instanciator* itor_;	///< A safe thread instanciator
    unsigned int nbthread_;	///< Number of thread to instanciate
    std::vector<std::thread> threads; ///< All thread for emptiness check
    std::condition_variable data_cond; ///< Notify main that answer found
    bool answer_found;		       ///< Variable for storage
    std::mutex m;		       ///< Associted mutex
    std::condition_variable run; ///< Barrier for checking
    std::mutex mrun;		 ///< Associated Mutex
    bool go;			 ///< Associated variable.
    bool counterexample_found;	///< A counterexample found?
    std::thread *thread_finalize; ///< terminator thread
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_COU99_UF_SWARM_HH
