// -*- coding: utf-8 -*-
// Copyright (C) 2011, 2014-2018 Laboratoire de Recherche et Developpement de
// l'EPITA (LRDE).
// Copyright (C) 2003, 2004, 2005 Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
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
#include <spot/twa/twa.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/couvreurnew.hh>
#include <spot/twaalgos/word.hh>
#include <spot/twaalgos/remfin.hh>
#include <spot/twaalgos/alternation.hh>
#include <spot/twa/twaproduct.hh>
#include <spot/twaalgos/dualize.hh>
#include <spot/twaalgos/postproc.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/product.hh>
#include <spot/twaalgos/genem.hh>
#include <utility>

namespace spot
{
  twa::twa(const bdd_dict_ptr& d)
    : iter_cache_(nullptr),
      dict_(d)
  {
    props = 0U;
    bddaps_ = bddtrue;
  }

  twa::~twa()
  {
    delete iter_cache_;
    release_named_properties();
    get_dict()->unregister_all_my_variables(this);
  }


  namespace
  {
    const_twa_graph_ptr is_twa_graph(const const_twa_ptr& a)
    {
      return std::dynamic_pointer_cast<const twa_graph>(a);
    }

    // Make sure automata using Fin acceptance are twa_graph_ptr
    const_twa_graph_ptr fin_to_twa_graph_maybe(const const_twa_ptr& a)
    {
      if (!a->acc().uses_fin_acceptance())
        return nullptr;
      auto aa = is_twa_graph(a);
      if (!aa)
        aa = make_twa_graph(a, twa::prop_set::all());
      return remove_alternation(aa);
    }

    // Remove Fin-acceptance and alternation.
    const_twa_ptr remove_fin_maybe(const const_twa_ptr& a)
    {
      auto aa = is_twa_graph(a);
      if ((!aa || aa->is_existential()) && !a->acc().uses_fin_acceptance())
        return a;
      if (!aa)
        aa = make_twa_graph(a, twa::prop_set::all());
      return remove_fin(remove_alternation(aa));
    }
  }

  state*
  twa::project_state(const state* s,
                      const const_twa_ptr& t) const
  {
    if (t.get() == this)
      return s->clone();
    return nullptr;
  }

  bool
  twa::is_empty() const
  {
    const_twa_ptr a = shared_from_this();
    if (const_twa_graph_ptr ag = fin_to_twa_graph_maybe(a))
      return generic_emptiness_check(ag);
    return !couvreur99_new_check(remove_fin_maybe(shared_from_this()));
  }

  twa_run_ptr
  twa::accepting_run() const
  {
    const_twa_ptr a = shared_from_this();
    if (const_twa_graph_ptr ag = fin_to_twa_graph_maybe(a))
      return generic_accepting_run(ag);
    auto res = couvreur99_new_check(a);
    if (!res)
      return nullptr;
    return res->accepting_run();
  }

  twa_word_ptr
  twa::accepting_word() const
  {
    if (auto run = shared_from_this()->accepting_run())
      {
        auto w = make_twa_word(run->reduce());
        w->simplify();
        return w;
      }
    else
      {
        return nullptr;
      }
  }

  bool
  twa::intersects(const_twa_ptr other) const
  {
    auto self = shared_from_this();
    // If the two operands are explicit automata (twa_graph_ptr) and one
    // of them uses Fin acceptance, make a regular product and check it
    // with the generic emptiness.
    if (acc().uses_fin_acceptance() || other->acc().uses_fin_acceptance())
      {
        const_twa_graph_ptr g1 = is_twa_graph(self);
        const_twa_graph_ptr g2 = is_twa_graph(other);
        if (g1 && g2 && g1->is_existential() && g2->is_existential())
          return !generic_emptiness_check(product(g1, g2));
      }
    auto a1 = remove_fin_maybe(self);
    auto a2 = remove_fin_maybe(other);
    return !otf_product(a1, a2)->is_empty();
  }

  twa_run_ptr
  twa::intersecting_run(const_twa_ptr other, bool from_other) const
  {
    auto a = shared_from_this();
    if (!from_other)
      other = remove_fin_maybe(other);
    else
      a = remove_fin_maybe(a);
    auto run = otf_product(a, other)->accepting_run();
    if (!run)
      return nullptr;
    return run->project(from_other ? other : a);
  }

  twa_word_ptr
  twa::intersecting_word(const_twa_ptr other) const
  {
    auto a1 = remove_fin_maybe(shared_from_this());
    auto a2 = remove_fin_maybe(other);
    return otf_product(a1, a2)->accepting_word();
  }

  namespace
  {
    static const_twa_graph_ptr
    ensure_deterministic(const const_twa_ptr& aut_in)
    {
      const_twa_graph_ptr aut =
        std::dynamic_pointer_cast<const twa_graph>(aut_in);
      if (!aut)
        aut = make_twa_graph(aut_in, twa::prop_set::all());

      if (is_deterministic(aut))
        return aut;
      postprocessor p;
      p.set_type(postprocessor::Generic);
      p.set_pref(postprocessor::Deterministic);
      p.set_level(postprocessor::Low);
      return p.run(std::const_pointer_cast<twa_graph>(aut));
    }
  }
  twa_run_ptr
  twa::exclusive_run(const_twa_ptr other) const
  {
    const_twa_ptr a = shared_from_this();
    const_twa_ptr b = other;

    // We have to find a run in A\B or in B\A.  When possible, let's
    // make sure the first automaton we complement is deterministic.
    if (auto aa = std::dynamic_pointer_cast<const twa_graph>(a))
      if (is_deterministic(aa))
        std::swap(a, b);
    if (auto run = a->intersecting_run(dualize(ensure_deterministic(b))))
      return run;
    return b->intersecting_run(dualize(ensure_deterministic(a)));
  }

  twa_word_ptr
  twa::exclusive_word(const_twa_ptr other) const
  {
    const_twa_ptr a = shared_from_this();
    const_twa_ptr b = other;

    // We have to find a word in A\B or in B\A.  When possible, let's
    // make sure the first automaton we complement is deterministic.
    if (auto aa = std::dynamic_pointer_cast<const twa_graph>(a))
      if (is_deterministic(aa))
        std::swap(a, b);
    if (auto word = a->intersecting_word(dualize(ensure_deterministic(b))))
      return word;
    return b->intersecting_word(dualize(ensure_deterministic(a)));
  }

  void
  twa::set_named_prop(std::string s, std::nullptr_t)
  {
    auto p = named_prop_.find(s);
    if (p == named_prop_.end())
      return;
    p->second.second(p->second.first);
    named_prop_.erase(p);
    return;
  }

  void
  twa::set_named_prop(std::string s,
                      void* val, std::function<void(void*)> destructor)
  {
    auto p = named_prop_.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(s),
                                 std::forward_as_tuple(val, destructor));
    if (!p.second)
      {
        p.first->second.second(p.first->second.first);
        p.first->second = std::make_pair(val, destructor);
      }
  }

  void*
  twa::get_named_prop_(std::string s) const
  {
    auto i = named_prop_.find(s);
    if (i == named_prop_.end())
      return nullptr;
    return i->second.first;
  }

  void
  twa::unregister_ap(int b)
  {
    auto d = get_dict();
    assert(d->bdd_map[b].type == bdd_dict::var);
    auto pos = std::find(aps_.begin(), aps_.end(), d->bdd_map[b].f);
    assert(pos != aps_.end());
    aps_.erase(pos);
    d->unregister_variable(b, this);
    bddaps_ = bdd_exist(bddaps_, bdd_ithvar(b));
  }



}
