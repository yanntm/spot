// -*- coding: utf-8 -*-
// Copyright (C)  2015, 2017 Laboratoire de Recherche et
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
  porinfos_stats::porinfos_stats():
    red_(0), en_(0), cumul_(0),
    min_red_(0), max_red_(0), min_en_(0), max_en_(0)
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

    for (int i = 0; i < transitions_; ++i)
      m_read[i].resize(variables_);
    for (int i = 0; i < transitions_; ++i)
      m_write[i].resize(variables_);
    for (int i = 0; i < guards_; ++i)
      m_nes[i].resize(transitions_);
    for (int i = 0; i < guards_; ++i)
      m_mbc[i].resize(guards_);

    // Grab Read dependency Matrix
    for (int i = 0; i < transitions_; ++i)
      {
        int* read_dep = d_->get_transition_read_dependencies(i);
        for (int j = 0; j < variables_; ++j)
          m_read[i][j] = read_dep[j];
      }

    // Grab Write depency Matrix
    for (int i = 0; i < transitions_; ++i)
      {
        int* write_dep = d_->get_transition_write_dependencies(i);
        for (int j = 0; j < variables_; ++j)
          m_write[i][j] = write_dep[j];
      }

    // Grab NES matrix
    for (int i = 0; i < guards_; ++i)
      {
        int* nes = d_->get_guard_nes_matrix(i);
        for (int j = 0; j < transitions_; ++j)
          m_nes[i][j] = nes[j];
      }

    // Grab "may-be-coenabled" matrix
    for (int i = 0; i < guards_; ++i)
      {
        int* mbc = d_->get_guard_may_be_coenabled_matrix(i);
        for (int j = 0; j < guards_; ++j)
          m_mbc[i][j] = mbc[j];
      }


    // Grab guards matrix
    for (int i = 0; i < transitions_; ++i)
      {
        int* array_guards = d_->get_guards(i);
        m_guards[i].resize(array_guards[0]);
        for (int j = 0; j < array_guards[0]; ++j)
          m_guards[i][j] = array_guards[j+1];
      }

    // Setup dependency between transitions. This
    // is a cache for speeding up dependancy computation.
    m_dep_tr.resize(transitions_);
    for (int t1 = 0; t1 < transitions_; ++t1)
      m_dep_tr[t1].resize(transitions_);

    for (int t1 = 0; t1 < transitions_; ++t1)
      {
        for (int t2 = t1; t2 < transitions_; ++t2)
          {
            bool res = false;
            for (int i = 0; i < variables_; ++i)
              if ((m_read[t1][i] && m_read[t2][i])
                || (m_write[t1][i] && m_write[t2][i]))
                {
                  res = true;
                  break;
                }
            m_dep_tr[t1][t2] = res;
            m_dep_tr[t2][t1] = res;
          }
      }

    m_dep_guards.resize(transitions_);
    for (int t1 = 0; t1 < transitions_; ++t1)
      m_dep_guards[t1].resize(transitions_);
    for (int t1 = 0; t1 < transitions_; ++t1)
      {
        for (int t2 = t1; t2 < transitions_; ++t2)
          {
            bool res = false;
            for (unsigned i = 0; !res && i < m_guards[t1].size(); i++)
              for (unsigned j = 0; !res && j < m_guards[t2].size(); j++)
                if (m_guards[t1][i] == m_guards[t2][j])
                  res = true;

            m_dep_guards[t1][t2] = res;
            m_dep_guards[t2][t1] = res;
          }
      }

    // Compute the list of processes with their id.
    // Primitives types and processes are stored in the same place, so
    // we just have to keep only entries which are not a type.
    const std::vector<std::string> primitive_types = { "int", "byte" };
    std::vector<std::pair<std::string, int>> processes;
    unsigned type_count = d_->get_type_count();
    for (unsigned i = 0; i < type_count; ++i)
      {
        auto type = std::string(d_->get_type_name(i));
        if (std::find(primitive_types.begin(), primitive_types.end(), type)
           == primitive_types.end())
          processes.emplace_back(type, i);
      }

    // Compute the processus of each variable.
    // -1 means that the variable is global.
    std::vector<int> variable_proc(d_->get_state_size());
    int proc = -1;
    unsigned state_size = d_->get_state_size();
    for (unsigned i = 0; i < state_size; ++i)
      {
        auto var = std::string(d_->get_state_variable_name(i));
        auto pred = [&var](std::pair<std::string, int> p)
          { return p.first == var; };
        auto res = std::find_if(processes.begin(), processes.end(), pred);

        if (!(res == processes.end()))
          proc = res->second;
        variable_proc[i] = proc;
      }

    // Compute the processus of each transition, based on the modified
    // variables. -1 means that we don't know with this method.
    t_processes.resize(transitions_);
    for (int t = 0; t < transitions_; ++t)
      {
        t_processes[t] = -1;
        for (int v = 0; v < variables_; ++v)
          if (m_read[t][v] || m_write[t][v])
            {
              t_processes[t] = variable_proc[v];
              break;
            }
      }

    // Setup non maybe coenabled
    non_mbc_tr.resize(transitions_);
    for (int i = 0; i < transitions_; ++i)
      non_mbc_tr[i].resize(transitions_);

    for (int t1 = 0; t1 < transitions_; ++t1)
      for (int t2 = t1; t2 < transitions_; ++t2)
        {
          non_mbc_tr[t1][t2] = non_maybecoenabled(t1, t2);
          non_mbc_tr[t2][t1] = non_mbc_tr[t1][t2];
        }
  }

  std::vector<bool>
  porinfos::compute_reduced_set_ct(const std::vector<int>& enabled,
                                const int* for_spins_state)
  {
    (void) for_spins_state;
    std::vector<bool> res(enabled.size(), true);

    // State without succesors
    if (enabled.empty())
      {
        stats_.cumul(0, enabled.size());
        return res;
      }

    std::vector<int> t_work;
    std::vector<int> t;

    // Randomly take one enabled transition
    {
      int alpha = enabled[0];
      t_work.push_back(alpha);
      t.push_back(alpha);
    }

    auto are_conflicting = [&](int t1, int t2)
      {
        return ((t_processes[t1] != -1 && t_processes[t2] != -1
                 && t_processes[t1] != t_processes[t2]) && m_dep_tr[t1][t2])
                 || m_dep_guards[t1][t2];
      };

    // Compute the conflicting transitions algorithm as described by
    // Elwin Pater in Partial Order Reduction for PINS [2011] (page 15)

    while (!t_work.empty())
      {
        int alpha = t_work.back();
        t_work.pop_back();

        for (int beta = 0; beta < transitions_; ++beta)
          {
            if (alpha != beta && are_conflicting(alpha, beta))
              {
                if (std::find(enabled.begin(), enabled.end(), beta) ==
                    enabled.end())
                  return res;
                if (std::find(t.begin(), t.end(), beta) == t.end())
                  {
                    t.push_back(beta);
                    t_work.push_back(beta);
                  }
              }
          }
      }

    // Compute intersection between t and enabled
    for (unsigned i = 0; i < enabled.size(); ++i)
      if (std::find(t.begin(), t.end(), enabled[i]) == t.end())
        res[i] = false;

    return res;
  }

  std::vector<bool>
  porinfos::compute_reduced_set_ss(const std::vector<int>& enabled,
                                const int* for_spins_state)
  {
    (void) for_spins_state;
    std::vector<bool> res_(enabled.size());

    if (enabled.empty())
      {
        //std::cerr << "Warning, state without successors\n" << std::endl;
        stats_.cumul(0, enabled.size());
        return res_;
      }

    // Compute the stubborn set algorithm as described by Elwin Pater
    // in Partial Order Reduction for PINS [2011] (page 21)

    // Declare usefull variables
    std::vector<int> t_work;
    std::vector<int> t_s;

    std::unordered_set<int> cache;

    // Randomly take one enabled transition
    // FIXME here we choose the first enabled transition (as described
    // in the previous report) but better heuristics may exist
    {
      int alpha = enabled[0];
      t_work.push_back(alpha);
    }

    // Iteratively build the stubborn set
    while (!t_work.empty())
      {
        int beta = t_work.back();
        t_work.pop_back();
        t_s.push_back(beta);
        cache.insert(beta);

        // Computes guards used by beta
        int beta_guards_size = m_guards[beta].size();

        if (std::find(enabled.begin(), enabled.end(), beta) != enabled.end())
          {
            // Beta is an enabled transition.
            // Iterate over all transitions
            for (int i = 0; i < transitions_; ++i)
              {
                // Beta is already in t_s or transitions are independants ...
                // ... so we avoid extra computation.
                if (i == beta || !m_dep_tr[i][beta])
                  continue;

                // The transition must be processed!
                if (non_mbc_tr[beta][i] && cache.find(i) == cache.end())
                  {
                    t_work.push_back(i);
                    cache.insert(i);
                  }
              }
          }
        else
          {
            (void) beta_guards_size;
            // Beta is not an enabled transition.
            // Compute GNES(beta, for_spins_state)
            bool goon = true;
            for (int i = 0; i < beta_guards_size && goon; ++i)
              {
                if (!d_->get_guard(nullptr, m_guards[beta][i],
                                  for_spins_state))
                  {
                    unsigned guard_to_look = m_guards[beta][i];
                    for (int j = 0; j < transitions_ && goon; ++j)
                      {
                        if (cache.find(j) == cache.end() &&
                            m_nes[guard_to_look][j])
                        {
                          t_work.push_back(j);
                          cache.insert(j);
                          goon = false;
                        }
                      }
                  }
              }
          }
      }

    // Compute the intersection between T_S and enabled
    unsigned hidden = 0;
    for (unsigned i = 0; i < enabled.size(); ++i)
      {
        const auto idx = std::find(t_s.begin(), t_s.end(), enabled[i]);
        if (idx != t_s.end())
          res_[i] = true;
        else
          ++hidden;
      }

    stats_.cumul(hidden, enabled.size());

    // Here we activate a SPIN-like persistent set, i.e.
    // wheter a persistent set with one transition or all transitions
    if (spin_ && (enabled.size() - hidden != 1))
      {
        for (unsigned i = 0; i < res_.size(); ++i)
          res_[i] = true;
      }
    return res_;
  }

  std::vector<bool>
  porinfos::compute_reduced_set_ss_ns(const std::vector<int>& enabled,
                                  const int* for_spins_state)
  {
    (void) for_spins_state;
    std::vector<bool> res_(enabled.size(), false);

    if (enabled.empty())
      {
        //std::cerr << "Warning, state without successors\n" << std::endl;
        stats_.cumul(0, enabled.size());
        return res_;
      }

    // Compute the stubborn set algorithm as described by Elwin Pater
    // in Partial Order Reduction for PINS [2011] (page 21)

    // Declare usefull variables
    std::vector<int> t_work;
    std::vector<int> t_s;

    std::unordered_set<int> cache;

    // Randomly take one enabled transition
    // FIXME here we choose the first enabled transition (as described
    // in the previous report) but better heuristics may exist
    {
      int alpha = enabled[0];
      t_work.push_back(alpha);
    }

    while (!t_work.empty())
      {
        int beta = t_work.back();
        t_work.pop_back();
        t_s.push_back(beta);
        cache.insert(beta);

        // Computes guards used by beta
        // int beta_guards_size = m_guards[beta].size();

        if (std::find(enabled.begin(), enabled.end(), beta) != enabled.end())
          {
            // Beta is an enabled transition.
            // Iterate over all transitions
            for (int i = 0; i < transitions_; ++i)
              {
                // Beta is already in t_s or transitions are independants ...
                // ... so we avoid extra computation.
                if (i == beta || !m_dep_tr[i][beta])
                  continue;

                // The transition must be processed!
                if (non_mbc_tr[beta][i] && cache.find(i) == cache.end())
                  {
                    t_work.push_back(i);
                    cache.insert(i);
                  }
              }
          }
        else
          {
            // FIXME best nes
          }
      }

    // Compute the intersection between T_S and enabled
    unsigned hidden = 0;
    for (unsigned i = 0; i < enabled.size(); ++i)
      {
        const auto idx = std::find(t_s.begin(), t_s.end(), enabled[i]);
        if (idx != t_s.end())
          res_[i] = true;
        else
          ++hidden;
      }

    stats_.cumul(hidden, enabled.size());

    // Here we activate a SPIN-like persistent set, i.e.
    // wheter a persistent set with one transition or all transitions
    if (spin_ && (enabled.size() - hidden != 1))
      {
        for (unsigned i = 0; i < res_.size(); ++i)
          res_[i] = true;
      }

    return res_;
  }

  bool porinfos::non_maybecoenabled(int t1, int t2)
  {
    int t1_guards_size = m_guards[t1].size();
    int t2_guards_size = m_guards[t2].size();
    for (int i_t2 = 0; i_t2 < t2_guards_size; ++i_t2)
      for (int i_t1 = 0; i_t1 < t1_guards_size; ++i_t1)
        if (!m_mbc[m_guards[t2][i_t2]][m_guards[t1][i_t1]])
          return false;
    return true;
  }

  int porinfos::transitions()
  {
    return transitions_;
  }

  int porinfos::variables()
  {
    return variables_;
  }

  void porinfos::dump_read_dependency()
  {
    std::cout << "Read Dependency Matrix" << std::endl;
    for (int i = 0; i < transitions_; ++i)
      {
        std::cout << "   ";
        for (int j = 0; j < variables_; ++j)
          std::cout << (m_read[i][j]? "+" : "-");
        std::cout << std::endl;
      }
    std::cout << std::endl;
  }

  void porinfos::dump_write_dependency()
  {
    std::cout << "Write Dependency Matrix" << std::endl;
    for (int i = 0; i < transitions_; ++i)
      {
        std::cout << "   ";
        for (int j = 0; j < variables_; ++j)
          std::cout << (m_write[i][j]? "+" : "-");
        std::cout << std::endl;
      }
    std::cout << std::endl;
  }

  void porinfos::dump_nes_guards()
  {
    std::cout << "NES Matrix" << std::endl;
    for (int i = 0; i < guards_; ++i)
      {
        std::cout << "   ";
        for (int j = 0; j < transitions_; ++j)
          std::cout << (m_nes[i][j]? "+" : "-");
        std::cout << std::endl;
      }
    std::cout << std::endl;
  }

  void porinfos::dump_mbc_guards()
  {
    std::cout << "May be coenabled Matrix" << std::endl;
    for (int i = 0; i < guards_; ++i)
      {
        std::cout << "   ";
        for (int j = 0; j < guards_; ++j)
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
