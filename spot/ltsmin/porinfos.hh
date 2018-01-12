// -*- coding: utf-8 -*-
// Copyright (C)  2015, 2016, 2017 Laboratoire de Recherche et
// Developpement de l'Epita (LRDE)
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

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <assert.h>
#include <spot/ltsmin/spins_interface.hh>
#include <spot/misc/common.hh>

namespace spot
{
  // \brief Store statistics about partial order
  class SPOT_API porinfos_stats
  {
  public:
    porinfos_stats();

    void cumul(unsigned red, unsigned en);

    void dump();

  private:
    unsigned red_{0};
    unsigned en_{0};
    unsigned cumul_{0};
    unsigned min_red_{0};
    unsigned max_red_{0};
    unsigned min_en_{0};
    unsigned max_en_{0};
  };

  // \brief Store informations about partial order
  class SPOT_API porinfos
  {
  public:
    porinfos(const spins_interface* si);

    std::vector<bool> compute_reduced_set(const std::vector<int>& enabled,
					  const int* for_spins_state);

    inline bool non_maybecoenabled(int t1, int t2);

    unsigned transitions();

    unsigned variables();

    void dump_read_dependency();

    void dump_write_dependency();

    void dump_nes_guards();

    void dump_mbc_guards();

    porinfos_stats& stats();

    void spin_fashion()
    {
      spin_ = true;
    }

  private:
    const spins_interface* d_;
    unsigned transitions_{0};
    unsigned variables_{0};
    unsigned guards_{0};
    porinfos_stats stats_;
    std::vector<std::vector<bool>> m_read;
    std::vector<std::vector<bool>> m_write;
    std::vector<std::vector<bool>> m_nes;
    std::vector<std::vector<bool>> m_mbc;
    std::vector<std::vector<int>>  m_guards;

    // Develop caches to reduce computation time
    std::vector<std::vector<bool>> m_dep_tr;
    std::vector<std::vector<bool>> non_mbc_tr;
    bool spin_ = false;

    std::unordered_map<unsigned, std::vector<int>> gnes_cache_;
    std::unordered_map<unsigned, std::vector<int>> gnes_cache_e_;
  };
}
