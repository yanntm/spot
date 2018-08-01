// -*- coding: utf-8 -*-
// Copyright (C) 2018 Laboratoire de Recherche et Développement de
// l'Epita.
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
#include <spot/twaalgos/contains.hh>
#include <spot/twaalgos/postproc.hh>
#include <spot/twaalgos/ltl2tgba_fm.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/dualize.hh>

namespace spot
{
  namespace
  {
    static spot::const_twa_graph_ptr
    ensure_deterministic(const spot::const_twa_graph_ptr& aut)
    {
      if (spot::is_deterministic(aut))
        return aut;
      spot::postprocessor p;
      p.set_type(spot::postprocessor::Generic);
      p.set_pref(spot::postprocessor::Deterministic);
      p.set_level(spot::postprocessor::Low);
      return p.run(std::const_pointer_cast<twa_graph>(aut));
    }

    static spot::const_twa_graph_ptr
    translate(formula f, const bdd_dict_ptr& dict)
    {
      return ltl_to_tgba_fm(f, dict);
    }
  }

  bool contains(const_twa_graph_ptr left, const_twa_graph_ptr right)
  {
    return !right->intersects(dualize(ensure_deterministic(left)));
  }

  bool contains(const_twa_graph_ptr left, formula right)
  {
    auto right_aut = translate(right, left->get_dict());
    return !right_aut->intersects(dualize(ensure_deterministic(left)));
  }

  bool contains(formula left, const_twa_graph_ptr right)
  {
    return !right->intersects(translate(formula::Not(left), right->get_dict()));
  }

  bool contains(formula left, formula right)
  {
    auto dict = make_bdd_dict();
    auto right_aut = translate(right, dict);
    return !right_aut->intersects(translate(formula::Not(left), dict));
  }

  bool are_equivalent(const_twa_graph_ptr left, const_twa_graph_ptr right)
  {
    // Start with a deterministic automaton at right if possible to
    // avoid a determinization (in case the first containment check
    // fails).
    if (!is_deterministic(right))
      std::swap(left, right);
    return contains(left, right) && contains(right, left);
  }

  bool are_equivalent(const_twa_graph_ptr left, formula right)
  {
    // The first containement check does not involve a
    // complementation, the second might.
    return contains(left, right) && contains(right, left);
  }

  bool are_equivalent(formula left, const_twa_graph_ptr right)
  {
    // The first containement check does not involve a
    // complementation, the second might.
    return contains(right, left) && contains(left, right);
  }

  bool are_equivalent(formula left, formula right)
  {
    return contains(right, left) && contains(left, right);
  }
}
