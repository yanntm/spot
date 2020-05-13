// -*- coding: utf-8 -*-
// Copyright (C)  2019, 2020 Laboratoire de Recherche et
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

#include <string>
#include <thread>
#include <vector>
#include <utility>
#include <spot/kripke/kripke.hh>
#include <spot/mc/mc.hh>
#include <spot/mc/lpar13.hh>
#include <spot/mc/deadlock.hh>
#include <spot/mc/cndfs.hh>
#include <spot/mc/bloemen.hh>
#include <spot/mc/bloemen_ec.hh>
#include <spot/misc/common.hh>
#include <spot/misc/timer.hh>

namespace spot
{

  template<typename algo_name, typename kripke_ptr, typename State,
           typename Iterator, typename Hash, typename Equal>
  static SPOT_API ec_stats instanciate(kripke_ptr sys,
                                       spot::twacube_ptr prop = nullptr,
                                       bool trace = false)
  {
    // FIXME ensure that algo_name contains all methods

    spot::timer_map tm;
    std::atomic<bool> stop(false);
    unsigned  nbth = sys->get_threads();

    typename algo_name::shared_map map;
    std::vector<algo_name*> swarmed(nbth);

    // The shared structure requires sometime one instance per thread
    using struct_name = typename algo_name::shared_struct;
    std::vector<struct_name*> ss(nbth);

    tm.start("Initialisation");
    for (unsigned i = 0; i < nbth; ++i)
      {
        ss[i] = algo_name::make_shared_structure(map, i);
        swarmed[i] = new algo_name(*sys, prop, map, ss[i], i, stop);
      }
    tm.stop("Initialisation");

    // Spawn Threads
    std::mutex iomutex;
    std::atomic<bool> barrier(true);
    std::vector<std::thread> threads(nbth);
    for (unsigned i = 0; i < nbth; ++i)
      {
        threads[i] = std::thread ([&swarmed, &iomutex, i, &barrier]
        {
#if defined(unix) || defined(__unix__) || defined(__unix)
            {
              std::lock_guard<std::mutex> iolock(iomutex);
              std::cout << "Thread #" << i
                        << ": on CPU " << sched_getcpu() << '\n';
            }
#endif

            // Wait all threads to be instanciated.
            while (barrier)
              continue;
            swarmed[i]->run();
         });

#if defined(unix) || defined(__unix__) || defined(__unix)
        //  Pins threads to a dedicated core.
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        int rc = pthread_setaffinity_np(threads[i].native_handle(),
                                        sizeof(cpu_set_t), &cpuset);
        if (rc != 0)
          {
            std::lock_guard<std::mutex> iolock(iomutex);
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << '\n';
          }
#endif
      }

    tm.start("Run");
    barrier.store(false);

    for (auto& t: threads)
      t.join();
    tm.stop("Run");

    // Build the result
    ec_stats result;
    for (unsigned i = 0; i < nbth; ++i)
      {
        result.name.emplace_back(swarmed[i]->name());
        result.walltime.emplace_back(swarmed[i]->walltime());
        result.states.emplace_back(swarmed[i]->states());
        result.transitions.emplace_back(swarmed[i]->transitions());
        result.sccs.emplace_back(swarmed[i]->sccs());
        result.value.emplace_back(swarmed[i]->result());
      }

    if (trace)
      {
        bool go_on = true;
        for (unsigned i = 0; i < nbth && go_on; ++i)
          {
            // Enumerate cases where a trace can be extraced
            // Here we use a switch so that adding new algortihm
            // with new return status will trigger an error that
            // should the be fixed here.
            switch (result.value[i])
              {
                // A (partial?) trace has been computed
              case mc_rvalue::DEADLOCK:
              case mc_rvalue::NOT_EMPTY:
                result.trace = swarmed[i]->trace();
                go_on = false;
                break;

                // Nothing to do here.
              case mc_rvalue::NO_DEADLOCK:
              case mc_rvalue::EMPTY:
              case mc_rvalue::SUCCESS:
              case mc_rvalue::FAILURE:
                break;
              }
          }
      }

    for (unsigned i = 0; i < nbth; ++i)
      {
        delete swarmed[i];
        delete ss[i];
      }

    return result;
  }

  template<typename kripke_ptr, typename State,
           typename Iterator, typename Hash, typename Equal>
  static ec_stats ec_instanciator(const mc_algorithm algo, kripke_ptr sys,
                                  spot::twacube_ptr prop = nullptr,
                                  bool trace = false)
  {
    if (algo == mc_algorithm::BLOEMEN_EC || algo == mc_algorithm::CNDFS ||
        algo == mc_algorithm::SWARMING)
      {
        SPOT_ASSERT(prop != nullptr);
        SPOT_ASSERT(sys->ap().size() == prop->ap().size());
        for (unsigned int i = 0; i < sys->ap().size(); ++i)
          SPOT_ASSERT(sys->ap()[i].compare(prop->ap()[i]) == 0);
      }

    switch (algo)
      {
      case mc_algorithm::BLOEMEN_SCC:
        return instanciate<spot::swarmed_bloemen<State, Iterator, Hash, Equal>,
             kripke_ptr, State, Iterator, Hash, Equal> (sys, prop, trace);

      case mc_algorithm::BLOEMEN_EC:
        return
          instanciate<spot::swarmed_bloemen_ec<State, Iterator, Hash, Equal>,
          kripke_ptr, State, Iterator, Hash, Equal> (sys, prop, trace);

      case mc_algorithm::CNDFS:
        return instanciate<spot::swarmed_cndfs<State, Iterator, Hash, Equal>,
            kripke_ptr, State, Iterator, Hash, Equal> (sys, prop, trace);

      case mc_algorithm::DEADLOCK:
        return instanciate<spot::swarmed_deadlock<State, Iterator, Hash, Equal>,
            kripke_ptr, State, Iterator, Hash, Equal> (sys, prop, trace);

      case mc_algorithm::SWARMING:
        return instanciate<spot::lpar13<State, Iterator, Hash, Equal>,
            kripke_ptr, State, Iterator, Hash, Equal> (sys, prop, trace);
      }
  }
}
