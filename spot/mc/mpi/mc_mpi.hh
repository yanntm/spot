// -*- coding: utf-8 -*-
// Copyright (C) 2019 Laboratoire de Recherche et Developpement de l'Epita
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

#include <spot/mc/mpi/dfs_cep.hh>
#include <spot/mc/mpi/dfs_sync.hh>

namespace spot
{
  template<typename kripke_ptr, typename State, typename Iterator,
           typename Hash, typename Equal>
   bool distribute_dfs(kripke_ptr sys)
   {
     using algo_name = spot::dfs_cep<State, Iterator, Hash, Equal>;
     algo_name *dfs = new algo_name(*sys);
     dfs->run();
     delete dfs;
     return true;
   }

  template<typename kripke_ptr, typename State, typename Iterator,
           typename Hash, typename Equal>
   bool sync_dfs(kripke_ptr sys)
   {
     using algo_name = spot::dfs_sync<State, Iterator, Hash, Equal>;
     algo_name *dfs = new algo_name(*sys);
     dfs->run();
     delete dfs;
     return true;
   }
}
