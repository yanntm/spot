// -*- coding: utf-8 -*-
// Copyright (C) 2013-2017 Laboratoire de Recherche et Développement
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

#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/ltl2tgba_fm.hh>
#include <spot/twaalgos/compsusp.hh>
#include <spot/twaalgos/product.hh>
#include <spot/twaalgos/parity.hh>
#include <spot/misc/optionmap.hh>

namespace spot
{
  namespace
  {
    static inline twa_graph_ptr product_aux(const const_twa_graph_ptr& left,
                                            const const_twa_graph_ptr& right)
    {
      return product(left, right);
    }

    static inline twa_graph_ptr product_or_aux(const const_twa_graph_ptr& left,
                                               const const_twa_graph_ptr& right)
    {
      return product_or(left, right);
    }

    template<typename T>
    twa_graph_ptr make_product(translator& translator, formula* f, T product)
    {
      twa_graph_ptr r = translator.run(formula(*f->begin()));
      for (size_t i = 1; i < f->size(); ++i)
        {
          auto translation = translator.run(formula((*f)[i]));
          if (r)
            r = product(r, translation);
          else
            r = translation;
        }
      return r;
    }
  }

  void translator::setup_opt(const option_map* opt)
  {
    comp_susp_ = early_susp_ = skel_wdba_ = skel_simul_ = split_
               = always_split_ = 0;

    if (!opt)
      return;

    comp_susp_ = opt->get("comp-susp", 0);
    if (comp_susp_ == 1)
      {
        early_susp_ = opt->get("early-susp", 0);
        skel_wdba_ = opt->get("skel-wdba", -1);
        skel_simul_ = opt->get("skel-simul", 1);
      }
    split_ = opt->get("split", 0);
    always_split_ = opt->get("always-split", 0);

  }

  void translator::build_simplifier(const bdd_dict_ptr& dict)
  {
    tl_simplifier_options options(false, false, false);
    switch (level_)
      {
      case High:
        options.containment_checks = true;
        options.containment_checks_stronger = true;
        SPOT_FALLTHROUGH;
      case Medium:
        options.synt_impl = true;
        SPOT_FALLTHROUGH;
      case Low:
        options.reduce_basics = true;
        options.event_univ = true;
      }
    simpl_owned_ = simpl_ = new tl_simplifier(options, dict);
  }

  twa_graph_ptr translator::run(formula* f)
  {
    formula r = simpl_->simplify(*f);
    *f = r;

    twa_graph_ptr product_aut = nullptr;
    if (!f->is_syntactic_obligation() && (split_ > 0 || always_split_ > 0))
      {
        if (f->is(op::Or))
          {
            if (this->postprocessor::type_ == Parity)
              product_aut = make_product(*this, f, parity_product_or);
            else
              product_aut = make_product(*this, f, product_or_aux);
          }
        else if (f->is(op::And))
          {
            if (this->postprocessor::type_ == Parity)
              product_aut = make_product(*this, f, parity_product);
            else
              product_aut = make_product(*this, f, product_aux);
          }
        if (product_aut)
        {
          product_aut = this->postprocessor::run(product_aut, r);
          if (always_split_ > 0)
            return product_aut;
        }
      }
    bool unambiguous = (pref_ & postprocessor::Unambiguous);
    if (unambiguous && type_ == postprocessor::Monitor)
      {
        // Deterministic monitor are unambiguous, so the unambiguous
        // option is not really relevant for monitors.
        unambiguous = false;
        set_pref(pref_ | postprocessor::Deterministic);
      }

    // This helps ltl_to_tgba_fm() to order BDD variables in a more
    // natural way (improving the degeneralization).
    simpl_->clear_as_bdd_cache();

    twa_graph_ptr aut;
    if (comp_susp_ > 0)
      {
        // FIXME: Handle unambiguous_ automata?
        int skel_wdba = skel_wdba_;
        if (skel_wdba < 0)
          skel_wdba = (pref_ == postprocessor::Deterministic) ? 1 : 2;

        aut = compsusp(r, simpl_->get_dict(), skel_wdba == 0,
                       skel_simul_ == 0, early_susp_ != 0,
                       comp_susp_ == 2, skel_wdba == 2, false);
      }
    else
      {
        bool exprop = unambiguous || level_ == postprocessor::High;
        aut = ltl_to_tgba_fm(r, simpl_->get_dict(), exprop,
                             true, false, false, nullptr, nullptr,
                             unambiguous);
      }
    aut = this->postprocessor::run(aut, r);
    if (product_aut && product_aut->num_states() < aut->num_states())
      return product_aut;
    return aut;
  }

  twa_graph_ptr translator::run(formula f)
  {
    return run(&f);
  }

  void translator::clear_caches()
  {
    simpl_->clear_caches();
  }

}
