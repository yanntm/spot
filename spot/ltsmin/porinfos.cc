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

#include <vector>
#include <unordered_set>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <spot/ltsmin/porinfos.hh>

namespace spot
{
  porinfos_stats::porinfos_stats()
  { }

  void porinfos_stats::cumul(unsigned red, unsigned en)
  {
    red_ += red;
    en_ += en;
    ++cumul_;
    min_red_ = min_red_ < red ? min_red_ : red;
    max_red_ = max_red_ > red ? max_red_ : red;
    min_en_ = min_en_ < en ? min_en_ : en;
    max_en_ = max_en_ > en ? max_en_ : en;
  }

  void porinfos_stats::dump()
  {
    assert(cumul_);
    std::cout << "reduced_set_min: " << min_red_ << std::endl;
    std::cout << "reduced_set_avg: " << ((int) (red_/cumul_)) << std::endl;
    std::cout << "reduced_set_max: " << max_red_ << std::endl;
    std::cout << "enabled_set_min: " << min_en_ << std::endl;
    std::cout << "enabled_set_avg: " << ((int) (en_/cumul_)) << std::endl;
    std::cout << "enabled_set_max: " << max_en_ << std::endl;
  }

  porinfos::porinfos(const spins_interface* si)
  {
    // Prepare data
    d_ = si;
    transitions_ = si->get_transition_count();
    variables_ = si->get_state_size();
    guards_ = si->get_guard_count();
    m_read.resize(transitions_);
    m_write.resize(transitions_);
    m_nes.resize(guards_);
    m_mbc.resize(guards_);
    m_guards.resize(transitions_);

    for (unsigned i = 0; i < transitions_; ++i)
      m_read[i].resize(variables_);
    for (unsigned i = 0; i < transitions_; ++i)
      m_write[i].resize(variables_);
    for (unsigned i = 0; i < guards_; ++i)
      m_nes[i].resize(transitions_);
    for (unsigned i = 0; i < guards_; ++i)
      m_mbc[i].resize(guards_);

    // Grab Read dependency Matrix
    for (unsigned i = 0; i < transitions_; ++i)
      {
        int* read_dep = d_->get_transition_read_dependencies(i);
        for (unsigned j = 0; j < variables_; ++j)
          m_read[i][j] = read_dep[j];
      }

    // Grab Write depency Matrix
    for (unsigned i = 0; i < transitions_; ++i)
      {
        int* write_dep = d_->get_transition_write_dependencies(i);
        for (unsigned j = 0; j < variables_; ++j)
          m_write[i][j] = write_dep[j];
      }

    // Grab NES matrix
    for (unsigned i = 0; i < guards_; ++i)
      {
        int* nes = d_->get_guard_nes_matrix(i);
        for (unsigned j = 0; j < transitions_; ++j)
          m_nes[i][j] = nes[j];
      }

    // Grab "may-be-coenabled" matrix
    for (unsigned i = 0; i < guards_; ++i)
      {
        int* mbc = d_->get_guard_may_be_coenabled_matrix(i);
        for (unsigned j = 0; j < guards_; ++j)
          m_mbc[i][j] = mbc[j];
      }

    // Grab guards matrix
    for (unsigned i = 0; i < transitions_; ++i)
      {
        int* array_guards = d_->get_guards(i);
        m_guards[i].resize(array_guards[0]);
        for (unsigned j = 0; j < (unsigned) array_guards[0]; ++j)
          m_guards[i][j] = array_guards[j+1];
      }

    // Setup dependency between transitions This
    // is a cache for speeding up dependancy computation.
    m_dep_tr.resize(transitions_);
    for (unsigned t1 = 0; t1 < transitions_; ++t1)
      m_dep_tr[t1].resize(transitions_);
    for (unsigned t1 = 0; t1 < transitions_; ++t1)
      {
        for (unsigned t2 = t1; t2 < transitions_; ++t2)
          {
            for (unsigned i = 0; i < variables_; ++i)
              {
                if ((m_read[t1][i] && m_read[t2][i]) ||
                    (m_write[t1][i] && m_write[t2][i]))
                  {
                    m_dep_tr[t1][t2] = true;
                    m_dep_tr[t2][t1] = true;
                  }
              }
          }
      }

    // Setup non maybe coenabled
    non_mbc_tr.resize(transitions_);
    for (unsigned i = 0; i < transitions_; ++i)
      non_mbc_tr[i].resize(transitions_);

    for (unsigned t1 = 0; t1 < transitions_; ++t1)
      for (unsigned t2 = t1; t2 < transitions_; ++t2)
        {
          non_mbc_tr[t1][t2] = non_maybecoenabled(t1, t2);
          non_mbc_tr[t2][t1] = non_mbc_tr[t1][t2];
        }


    // Speedup Stubborn  set computation
    std::vector<int> tmp;
    for (unsigned i = 0; i < guards_; ++i)
      {
        unsigned guard_to_look = i;
        for (unsigned j = 0; j < transitions_; ++j)
          {
            if (m_nes[guard_to_look][j])
              tmp.emplace_back(j);
          }
        gnes_cache_.insert({guard_to_look, tmp});
        tmp.clear();
      }


    for (unsigned beta = 0; beta < transitions_; ++beta)
      {
        for (unsigned i = 0; i < transitions_; ++i)
          {
            // Beta is already in t_s or transitions are independants ...
            // ... so we avoid extra computation.
            if (i == (unsigned) beta || !m_dep_tr[i][beta])
              continue;

            // The transition must be processed!
            if (non_mbc_tr[beta][i])
              {
                tmp.emplace_back(i);
              }
          }
        gnes_cache_e_.insert({beta, tmp});
        tmp.clear();
      }
  }

  std::vector<bool>
  porinfos::compute_reduced_set(const std::vector<int>& enabled,
                                const int* for_spins_state)
  {
    // Compute the stubborn set algorithm as described by Elwin Pater
    // in Partial Order Reduction for PINS [2011] (page 21)

    // The result, bit = true means that the corresponding transition
    // belongs to the reduced set
    std::vector<bool> res_(enabled.size());

    if (SPOT_UNLIKELY(enabled.empty()))
      {
        stats_.cumul(0, enabled.size());
        return res_;
      }

    // Declare usefull variables
    thread_local std::vector<int> t_work;
    thread_local std::unordered_set<int> cache;
    t_work.clear();
    cache.clear();

    // Store enable transitions in a map. Every time such a transition is
    // discovered we remove it from the map. When the map is empty we know
    // that the state is naturally expanded.
    thread_local std::unordered_map<int, unsigned> enabled_to_remove;
    thread_local std::unordered_map<int, unsigned> enabled_cache;
    enabled_to_remove.clear();
    unsigned enabled_size = enabled.size();
    for (unsigned i = 1; i < enabled_size; ++i) // skip first transition
      enabled_to_remove.insert({enabled[i], i});

    // Randomly take one enabled transition
    // FIXME here we choose the first enabled transition (as described
    // in the previous report) but better heuristics may exist
    {
      int alpha = enabled[0];
      res_[0] = true;
      t_work.emplace_back(alpha);
      cache.insert(alpha);
    }

    // Iteratively build the stubborn set
    unsigned idx = 0;
    std::vector<int>* to_add;
    while (idx < t_work.size() && !enabled_to_remove.empty())
      {
        int beta = t_work[idx++];

        // Transition beta is enabled
        if (std::find(enabled.begin(), enabled.end(), beta) != enabled.end())
          {
            to_add = &gnes_cache_e_[beta];
          }
        else
          {
            // Beta is not an enabled, build GNES(beta, for_spins_state)

            // Computes guards used by beta
            unsigned beta_guards_size = m_guards[beta].size();

            // Pick only one guard that is not enabled
            for (unsigned i = 0; i < beta_guards_size; ++i)
              {
                unsigned beta_guard = m_guards[beta][i];
                if (!d_->get_guard(nullptr, beta_guard, for_spins_state))
                  {
                    to_add = &gnes_cache_[beta_guard];
                    break;
                  }
              }
          }

        // Process all transitions to add to t_work
        for (const int tr: *to_add)
          {
            const auto& it = cache.insert(tr);
            if (it.second)
              {
                t_work.emplace_back(tr);

                // Speedup computation
                const auto& iterator = enabled_to_remove.find(tr);
                if (iterator != enabled_to_remove.end())
                  {
                    res_[iterator->second] = true;
                    enabled_to_remove.erase(iterator);
                  }
              }
          }
      }
    return res_;
  }

  bool porinfos::non_maybecoenabled(int t1, int t2)
  {
    unsigned t1_guards_size = m_guards[t1].size();
    unsigned t2_guards_size = m_guards[t2].size();
    for (unsigned i_t2 = 0; i_t2 < t2_guards_size; ++i_t2)
      for (unsigned i_t1 = 0; i_t1 < t1_guards_size; ++i_t1)
        if (!m_mbc[m_guards[t2][i_t2]][m_guards[t1][i_t1]])
          return false;
    return true;
  }

  unsigned porinfos::transitions()
  {
    return transitions_;
  }

  unsigned porinfos::variables()
  {
    return variables_;
  }

  void porinfos::dump_read_dependency()
  {
    std::cout << "Read Dependency Matrix" << std::endl;
    for (unsigned i = 0; i < transitions_; ++i)
      {
        std::cout << "   ";
        for (unsigned j = 0; j < variables_; ++j)
          std::cout << (m_read[i][j]? "+" : "-");
        std::cout << std::endl;
      }
    std::cout << std::endl;
  }

  void porinfos::dump_write_dependency()
  {
    std::cout << "Write Dependency Matrix" << std::endl;
    for (unsigned i = 0; i < transitions_; ++i)
      {
        std::cout << "   ";
        for (unsigned j = 0; j < variables_; ++j)
          std::cout << (m_write[i][j]? "+" : "-");
        std::cout << std::endl;
      }
    std::cout << std::endl;
  }

  void porinfos::dump_nes_guards()
  {
    std::cout << "NES Matrix" << std::endl;
    for (unsigned i = 0; i < guards_; ++i)
      {
        std::cout << "   ";
        for (unsigned j = 0; j < transitions_; ++j)
          std::cout << (m_nes[i][j]? "+" : "-");
        std::cout << std::endl;
      }
    std::cout << std::endl;
  }

  void porinfos::dump_mbc_guards()
  {
    std::cout << "May be coenabled Matrix" << std::endl;
    for (unsigned i = 0; i < guards_; ++i)
      {
        std::cout << "   ";
        for (unsigned j = 0; j < guards_; ++j)
          std::cout << (m_mbc[i][j]? "+" : "-");
        std::cout << std::endl;
      }
    std::cout << std::endl;
  }

  porinfos_stats& porinfos::stats()
  {
    return stats_;
  }
}
