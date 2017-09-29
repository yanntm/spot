// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016, 2017 Laboratoire de Recherche et
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
#include <tuple>
#include <vector>
#include <spot/kripke/kripke.hh>
#include <spot/mc/ec.hh>
#include <spot/misc/common.hh>

namespace spot
{
  /// \brief Check for the emptiness between a system and a twa.
  /// Return a pair containing a boolean indicating wether a counterexample
  /// has been found and a string representing the counterexample if the
  /// computation have been required
  template<typename kripke_ptr, typename State,
           typename Iterator, typename Hash, typename Equal>
  static std::tuple<bool, std::string, std::vector<istats>>
  modelcheck(kripke_ptr sys, spot::twacube_ptr twa, bool compute_ctrx = false)
  {
    // Must ensure that the two automata are working on the same
    // set of atomic propositions.
    SPOT_ASSERT(sys->get_ap().size() == twa->get_ap().size());
    for (unsigned int i = 0; i < sys->get_ap().size(); ++i)
      SPOT_ASSERT(sys->get_ap()[i].compare(twa->get_ap()[i]) == 0);

    bool stop = false;
    std::vector<ec_renault13lpar<State, Iterator, Hash, Equal>> ecs;
    for (unsigned i = 0; i < sys->get_threads(); ++i)
      ecs.push_back({*sys, twa, i, stop});

    std::vector<std::thread> threads;
    for (unsigned i = 0; i < sys->get_threads(); ++i)
      threads.push_back
        (std::thread(&ec_renault13lpar<State, Iterator, Hash, Equal>::run,
                     &ecs[i]));

    for (unsigned i = 0; i < sys->get_threads(); ++i)
      threads[i].join();

    bool has_ctrx = false;
    std::string trace = "";
     std::vector<istats> stats;
    for (unsigned i = 0; i < sys->get_threads(); ++i)
      {
        has_ctrx |= ecs[i].counterexample_found();
        if (compute_ctrx && ecs[i].counterexample_found()
            && trace.compare("") == 0)
          trace = ecs[i].trace(); // Pick randomly one !
        stats.push_back(ecs[i].stats());
      }
    return std::make_tuple(has_ctrx, trace, stats);
  }
}
