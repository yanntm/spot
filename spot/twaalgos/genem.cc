// -*- coding: utf-8 -*-
// Copyright (C) 2017, 2018 Laboratoire de Recherche et Developpement
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

#include "config.h"
#include <spot/twaalgos/genem.hh>
#include <spot/twaalgos/cleanacc.hh>

namespace spot
{
  namespace
  {
    class temporary_acc_set
    {
      twa_graph_ptr aut_;
      acc_cond old_acc_;

    public:
      temporary_acc_set(const const_twa_graph_ptr& aut,
                        acc_cond new_acc)
        : aut_(std::const_pointer_cast<twa_graph>(aut)),
          old_acc_(aut->acc())
      {
        set(new_acc);
      }

      void set(acc_cond new_acc)
      {
        aut_->set_acceptance(new_acc);
      }

      ~temporary_acc_set()
      {
        aut_->set_acceptance(old_acc_);
      }
    };

    bool scc_split_check(const scc_info& si, unsigned scc, twa_run_ptr run,
                         acc_cond::mark_t tocut,
                         bool (*ec_scc)(const scc_info& si,
                                        unsigned scc,
                                        twa_run_ptr run,
                                        acc_cond::mark_t tocut))
    {
      struct filter_data_t {
        const scc_info& lower_si;
        unsigned lower_scc;
        acc_cond::mark_t cut_sets;
      }
      data = {si, scc, tocut};

      scc_info::edge_filter filter =
        [](const twa_graph::edge_storage_t& e, unsigned dst,
           void* filter_data) -> scc_info::edge_filter_choice
        {
          auto& data = *reinterpret_cast<filter_data_t*>(filter_data);
          if (data.lower_si.scc_of(dst) != data.lower_scc)
            return scc_info::edge_filter_choice::ignore;
          if (data.cut_sets & e.acc)
            return scc_info::edge_filter_choice::cut;
          return scc_info::edge_filter_choice::keep;
        };

      // We want to remove tocut from the acceptance condition right
      // now, because hopefully this will convert the acceptance
      // condition into a Fin-less one, and then we do not have to
      // recurse it.
      acc_cond::mark_t sets = si.acc_sets_of(scc) - tocut;
      auto& autacc = si.get_aut()->acc();
      acc_cond acc = autacc.restrict_to(sets);
      acc = acc.remove(si.common_sets_of(scc), false);
      temporary_acc_set tmp(si.get_aut(), acc);
      scc_info upper_si(si.get_aut(), si.one_state_of(scc), filter, &data,
                        scc_info_options::STOP_ON_ACC
                        | scc_info_options::TRACK_STATES);

      const int accepting_scc = upper_si.one_accepting_scc();
      if (accepting_scc >= 0)
      {
        if (run)
          upper_si.get_accepting_run(accepting_scc, run);
        return false;
      }
      if (!acc.uses_fin_acceptance())
        return true;
      unsigned nscc = upper_si.scc_count();
      for (unsigned scc = 0; scc < nscc; ++scc)
        if (!ec_scc(upper_si, scc, run, tocut))
          return false;
      return true;
    }

    template <bool deal_with_disjunct>
    bool generic_emptiness_check_for_scc_nocopy(const scc_info& si,
                                                unsigned scc,
                                                twa_run_ptr run,
                                                acc_cond::mark_t
                                                tocut = {})
    {
      if (si.is_rejecting_scc(scc))
        return true;
      acc_cond::mark_t sets = si.acc_sets_of(scc);
      auto& autacc = si.get_aut()->acc();
      acc_cond acc = autacc.restrict_to(sets);
      acc = acc.remove(si.common_sets_of(scc), false);
      if (acc.is_f())
        return true;
      if (acc.is_t())
      {
        if (run)
          si.get_accepting_run(scc, run);
        return false;
      }
      temporary_acc_set tmp(si.get_aut(), acc);
      if (acc_cond::mark_t fu = acc.fin_unit())
        return scc_split_check
          (si, scc, run, tocut | fu,
           generic_emptiness_check_for_scc_nocopy<deal_with_disjunct>);

      if (deal_with_disjunct)
        {
          acc_cond::acc_code& code = acc.get_acceptance();
          assert(!code.empty());
          auto pos = &code.back();
          auto start = &code.front();
          assert(pos - pos->sub.size == start);
          if (pos->sub.op == acc_cond::acc_op::Or)
            {
              do
                {
                  --pos;
                  if (pos->sub.op != acc_cond::acc_op::Inf)
                    {
                      acc_cond::acc_code one(pos);
                      tmp.set(one);
                      if (si.get_aut()->acc().uses_fin_acceptance())
                        {
                          acc_cond::mark_t plus = {};
                          if (acc_cond::mark_t fu = one.fin_unit())
                            plus = fu;
                          if (!scc_split_check
                              (si, scc, run, tocut | plus,
                               generic_emptiness_check_for_scc_nocopy
                               <deal_with_disjunct>))
                            return false;
                        }
                    }
                  pos -= pos->sub.size;
                }
              while (pos > start);
              return true;
            }
          if (pos->sub.op == acc_cond::acc_op::Fin)
            {
              tmp.set(acc_cond::acc_code());
              for (unsigned d: pos[-1].mark.sets())
                if (!scc_split_check
                    (si, scc, run,
                     tocut | acc_cond::mark_t({d}),
                     generic_emptiness_check_for_scc_nocopy
                     <deal_with_disjunct>))
                  return false;
              return true;
            }
        }

      int fo = acc.fin_one();
      assert(fo >= 0);
      // Try to accept when Fin(fo) == true
      if (!scc_split_check
          (si, scc, run, tocut | acc_cond::mark_t({(unsigned) fo}),
           generic_emptiness_check_for_scc_nocopy
           <deal_with_disjunct>))
        return false;
      // Try to accept when Fin(fo) == false
      tmp.set(acc.force_inf({(unsigned) fo}));
      return generic_emptiness_check_for_scc_nocopy
        <deal_with_disjunct>(si, scc, run, tocut);
    }

    template <bool deal_with_disjunct>
    bool generic_emptiness_check_main_nocopy(const twa_graph_ptr& aut,
                                             twa_run_ptr run)
    {
      cleanup_acceptance_here(aut, false);
      auto& aut_acc = aut->acc();
      if (aut_acc.is_f())
        return true;
      if (!aut->acc().uses_fin_acceptance())
      {
        if (run)
        {
          auto p = aut->accepting_run();
          if (p)
          {
            *run = *p;
            return false;
          }
          return true;
        }
        return aut->is_empty();
      }

      scc_info si(aut, scc_info_options::STOP_ON_ACC
                  | scc_info_options::TRACK_STATES);

      const int accepting_scc = si.one_accepting_scc();
      if (accepting_scc >= 0)
      {
        if (run)
          si.get_accepting_run(accepting_scc, run);
        return false;
      }

      unsigned nscc = si.scc_count();
      for (unsigned scc = 0; scc < nscc; ++scc)
        if (!generic_emptiness_check_for_scc_nocopy
            <deal_with_disjunct>(si, scc, run))
          return false;
      return true;
    }
  }

  bool generic_emptiness_check(const const_twa_graph_ptr& aut)
  {
    if (SPOT_UNLIKELY(!aut->is_existential()))
      throw std::runtime_error("generic_emptiness_check() "
                               "does not support alternating automata");
    auto aut_ = std::const_pointer_cast<twa_graph>(aut);
    acc_cond old = aut_->acc();
    bool res = generic_emptiness_check_main_nocopy<true>(aut_, nullptr);
    aut_->set_acceptance(old);
    return res;
  }

  twa_run_ptr generic_accepting_run(const const_twa_graph_ptr& aut)
  {
    if (SPOT_UNLIKELY(!aut->is_existential()))
      throw std::runtime_error("generic_accepting_run() "
                               "does not support alternating automata");
    auto aut_ = std::const_pointer_cast<twa_graph>(aut);
    acc_cond old = aut_->acc();

    twa_run_ptr run = std::make_shared<twa_run>(aut_);
    bool res = generic_emptiness_check_main_nocopy<true>(aut_, run);

    aut_->set_acceptance(old);

    if (!res)
      return run;
    return nullptr;
  }

  bool generic_emptiness_check_for_scc(const scc_info& si,
                                       unsigned scc)
  {
    if (SPOT_UNLIKELY(!si.get_aut()->is_existential()))
      throw std::runtime_error("generic_emptiness_check_for_scc() "
                               "does not support alternating automata");
    return generic_emptiness_check_for_scc_nocopy<true>(si, scc, nullptr);
  }
}
