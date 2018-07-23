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

    static bool generic_emptiness_check_main(const twa_graph_ptr& aut);

    static bool
    generic_emptiness_check_for_scc_copy(const scc_info& si, unsigned scc)
    {
      if (si.is_rejecting_scc(scc))
        return true;
      acc_cond::mark_t sets = si.acc_sets_of(scc);
      acc_cond acc = si.get_aut()->acc().restrict_to(sets);
      acc_cond::mark_t fu = acc.fin_unit();
      if (fu)
        {
          for (auto part: si.split_on_sets(scc, fu))
            if (!generic_emptiness_check_main(part))
              return false;
        }
      else
        {
          int fo = acc.fin_one();
          assert(fo >= 0);
          // Try to accept when Fin(fo) == true
          for (auto part: si.split_on_sets(scc, {(unsigned) fo}))
            if (!generic_emptiness_check_main(part))
              return false;
          // Try to accept when Fin(fo) == false
          twa_graph_ptr whole = si.split_on_sets(scc, {})[0];
          whole->set_acceptance(acc.remove({(unsigned) fo}, false));
          if (!generic_emptiness_check_main(whole))
            return false;
        }
      return true;
    }

    static bool generic_emptiness_check_main(const twa_graph_ptr& aut)
    {
      cleanup_acceptance_here(aut, false);
      if (aut->acc().is_f())
        return true;
      if (!aut->acc().uses_fin_acceptance())
        return aut->is_empty();

      scc_info si(aut, scc_info_options::STOP_ON_ACC
                  | scc_info_options::TRACK_STATES);
      if (si.one_accepting_scc() >= 0)
        return false;

      unsigned nscc = si.scc_count();
      for (unsigned scc = 0; scc < nscc; ++scc)
        if (!generic_emptiness_check_for_scc_copy(si, scc))
          return false;
      return true;
    }

    template <bool deal_with_slack>
    bool scc_split_check(const scc_info& si, unsigned scc,
                         acc_cond::mark_t tocut,
                         bool (*ec_scc)(const scc_info& si,
                                        unsigned scc,
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
      if (deal_with_slack)
        acc.slack_cleanup();
      temporary_acc_set tmp(si.get_aut(), acc);
      scc_info upper_si(si.get_aut(), si.one_state_of(scc), filter, &data,
                        scc_info_options::STOP_ON_ACC);
      if (upper_si.one_accepting_scc() >= 0)
        return false;
      if (!acc.uses_fin_acceptance() &&
          (!deal_with_slack || acc.slack_one() < 0))
        return true;
      unsigned nscc = upper_si.scc_count();
      for (unsigned scc = 0; scc < nscc; ++scc)
        if (!ec_scc(upper_si, scc, tocut))
          return false;
      return true;
    }

    template <bool deal_with_disjunct, bool deal_with_slack>
    bool generic_emptiness_check_for_scc_nocopy(const scc_info& si,
                                                unsigned scc,
                                                acc_cond::mark_t
                                                tocut = {})
    {
      if (si.is_rejecting_scc(scc))
        return true;
      acc_cond::mark_t sets = si.acc_sets_of(scc);
      auto& autacc = si.get_aut()->acc();
      acc_cond acc = autacc.restrict_to(sets);
      acc = acc.remove(si.common_sets_of(scc), false);
      if (deal_with_slack)
        acc.slack_cleanup();
      if (acc.is_f())
        return true;
      if (acc.is_t())
        return false;
      temporary_acc_set tmp(si.get_aut(), acc);
      if (acc_cond::mark_t fu = acc.fin_unit())
        return scc_split_check<deal_with_slack>
          (si, scc, tocut | fu,
           generic_emptiness_check_for_scc_nocopy
           <deal_with_disjunct, deal_with_slack>);

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
                          if (!scc_split_check<deal_with_slack>
                              (si, scc, tocut | plus,
                               generic_emptiness_check_for_scc_nocopy
                               <deal_with_disjunct, deal_with_slack>))
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
                if (!scc_split_check<deal_with_slack>
                    (si, scc,
                     tocut | acc_cond::mark_t({d}),
                     generic_emptiness_check_for_scc_nocopy
                     <deal_with_disjunct, deal_with_slack>))
                  return false;
              return true;
            }
        }

      int sl = acc.slack_one();
      if (sl >= 0)
        {
          for (bool val: {true, false})
            {
              tmp.set(acc.slack_set(sl, val));
              if (!generic_emptiness_check_for_scc_nocopy
                  <deal_with_disjunct, deal_with_slack>(si, scc, tocut))
                return false;
            }
          return true;
        }
      int fo = acc.fin_one();
      assert(fo >= 0);
      // Try to accept when Fin(fo) == true
      if (!scc_split_check<deal_with_slack>
          (si, scc, tocut | acc_cond::mark_t({(unsigned) fo}),
           generic_emptiness_check_for_scc_nocopy
           <deal_with_disjunct, deal_with_slack>))
        return false;
      // Try to accept when Fin(fo) == false
      tmp.set(acc.remove({(unsigned) fo}, false));
      return generic_emptiness_check_for_scc_nocopy
        <deal_with_disjunct, deal_with_slack>(si, scc, tocut);
      return true;
    }

    template <bool deal_with_disjunct, bool deal_with_slack>
    bool generic_emptiness_check_main_nocopy(const twa_graph_ptr& aut)
    {
      cleanup_acceptance_here(aut, false);
      auto& aut_acc = aut->acc();
      if (deal_with_slack)
        aut_acc.slack_cleanup();
      if (aut_acc.is_f())
        return true;
      if (!aut->acc().uses_fin_acceptance() && (!deal_with_slack
                                                || aut_acc.slack_one() < 0))
        return aut->is_empty();

      scc_info si(aut, scc_info_options::STOP_ON_ACC
                  | scc_info_options::TRACK_STATES);
      if (si.one_accepting_scc() >= 0)
        return false;

      unsigned nscc = si.scc_count();
      for (unsigned scc = 0; scc < nscc; ++scc)
        if (!generic_emptiness_check_for_scc_nocopy
            <deal_with_disjunct, deal_with_slack>(si, scc))
          return false;
      return true;
    }
  }

    static bool generic_emptiness_check_tseytin_rec(const twa_graph_ptr& aut)
    {
      cleanup_acceptance_here(aut, false);
      auto& aut_acc = aut->acc();
      aut_acc.slack_cleanup();
      if (aut_acc.is_f())
        return true;
      if ((!aut_acc.uses_fin_acceptance()) && aut_acc.slack_one() < 0)
        return aut->is_empty();

      scc_info si(aut, scc_info_options::STOP_ON_ACC
                  | scc_info_options::TRACK_STATES);
      if (si.one_accepting_scc() >= 0)
        return false;

      unsigned nscc = si.scc_count();
      for (unsigned scc = 0; scc < nscc; ++scc)
        {
          if (si.is_rejecting_scc(scc))
            continue;
          acc_cond::mark_t sets = si.acc_sets_of(scc);
          acc_cond acc = aut_acc.restrict_to(sets);
          acc.slack_cleanup();
          if (acc.is_f())
            continue;
          if (!acc.uses_fin_acceptance())
            {
              auto whole_scc = si.split_on_sets(scc, {})[0];
              if (!generic_emptiness_check_tseytin_rec(whole_scc))
                return false;
              continue;
            }
          acc_cond::mark_t fu = acc.fin_unit();
          if (fu)
            {
              for (auto part: si.split_on_sets(scc, fu))
                if (!generic_emptiness_check_tseytin_rec(part))
                  return false;
            }
          else
            {
              int sl = acc.slack_one();
              if (sl >= 0)
                {
                  for (bool val: {true, false})
                    {
                      auto saut = si.split_on_sets(scc, {})[0];
                      saut->set_acceptance(saut->acc().slack_set(sl, val));
                      if (!generic_emptiness_check_tseytin_rec(saut))
                        return false;
                    }
                }
              else
                {
                  int fo = acc.fin_one();
                  assert(fo >= 0);
                  // Try to accept when Fin(fo) == true
                  for (auto part: si.split_on_sets(scc, {(unsigned) fo}))
                    if (!generic_emptiness_check_tseytin_rec(part))
                      return false;
                  // Try to accept when Fin(fo) == false
                  twa_graph_ptr whole = si.split_on_sets(scc, {})[0];
                  whole->set_acceptance(acc.remove({(unsigned) fo}, false));
                  if (!generic_emptiness_check_tseytin_rec(whole))
                    return false;
                }
            }
        }
      return true;
    }

  bool generic_emptiness_check_copy(const const_twa_graph_ptr& aut)
  {
    auto aut_ = std::const_pointer_cast<twa_graph>(aut);
    acc_cond old = aut_->acc();
    bool res =  generic_emptiness_check_main(aut_);
    aut_->set_acceptance(old);
    return res;
  }

  bool generic_emptiness_check_nodj(const const_twa_graph_ptr& aut)
  {
    auto aut_ = std::const_pointer_cast<twa_graph>(aut);
    acc_cond old = aut_->acc();
    bool res = generic_emptiness_check_main_nocopy<false, false>(aut_);
    aut_->set_acceptance(old);
    return res;
  }

  bool generic_emptiness_check(const const_twa_graph_ptr& aut)
  {
    auto aut_ = std::const_pointer_cast<twa_graph>(aut);
    acc_cond old = aut_->acc();
    bool res = generic_emptiness_check_main_nocopy<true, false>(aut_);
    aut_->set_acceptance(old);
    return res;
  }

  bool generic_emptiness_check_for_scc(const scc_info& si,
                                       unsigned scc)
  {
    return generic_emptiness_check_for_scc_nocopy<true, false>(si, scc);
  }

  bool generic_emptiness_check_tseytin_copy(const const_twa_graph_ptr& aut)
  {
    twa_graph_ptr aut_ = std::const_pointer_cast<twa_graph>(aut);
    acc_cond old = aut_->acc();
    auto a = old.tseytin();
    a.slack_cleanup();
    aut_->set_acceptance(a);
    bool res =  generic_emptiness_check_tseytin_rec(aut_);
    aut_->set_acceptance(old);
    return res;
  }

  bool generic_emptiness_check_tseytin_nodj(const const_twa_graph_ptr& aut)
  {
    twa_graph_ptr aut_ = std::const_pointer_cast<twa_graph>(aut);
    acc_cond old = aut_->acc();
    auto a = old.tseytin();
    a.slack_cleanup();
    aut_->set_acceptance(a);
    bool res = generic_emptiness_check_main_nocopy<false, true>(aut_);
    aut_->set_acceptance(old);
    return res;
  }

  bool generic_emptiness_check_tseytin(const const_twa_graph_ptr& aut)
  {
    twa_graph_ptr aut_ = std::const_pointer_cast<twa_graph>(aut);
    acc_cond old = aut_->acc();
    auto a = old.tseytin();
    a.slack_cleanup();
    aut_->set_acceptance(a);
    bool res = generic_emptiness_check_main_nocopy<true, true>(aut_);
    aut_->set_acceptance(old);
    return res;
  }

}
