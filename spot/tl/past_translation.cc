// -*- coding: utf-8 -*-
// Copyright (C) 2018 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
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
// along with this program.  If not, see <hformula::ttp://www.gnu.org/licenses/>.

#include "config.h"
#include <spot/tl/past_translation.hh>
#include <spot/misc/common.hh>
#include <iostream>

namespace spot
{

  // transforms formula 'a S b' as 'Concat (Star(formula::tt), b, Star(a))'
  static formula
  since_translation(const formula& f)
  {
    return formula::multop(op::Concat, {formula::unop(op::Star,
            formula::tt()), f[1], formula::unop(op::Star, f[0])});
  }


  // recursive translation of a ltl+past formula using regular expression
  // formula should have a future operator before a past operator
  // ex: G(a S b)
  // formula should NOT have a future operator after a past operator
  // ex: G(a S (F b))
  static formula
  rec_translation(formula *f, const bool can_be_past, const bool can_be_future)
  {
    switch (f->kind())
      {
        case op::M:
        case op::R:
        case op::W:
        case op::U:
        case op::F:
        case op::G:
          if (!can_be_future)
            {
              std::cerr << "Wrong format for the formula !" << std::endl;
              exit(1);
            }
          for (unsigned index = 0; index < f->size(); ++index)
            rec_translation(&f[index], f->kind() == op::F ||
                f->kind() == op::G || can_be_past, true);
          break;
        case op::S:
          if (!can_be_past)
            {
              std::cerr << "Wrong format for the formula !" << std::endl;
              exit(1);
            }
          for (unsigned index = 0; index < f->size(); ++index)
            rec_translation(&f[index], true, false);
          *f = since_translation(*f);
          break;
        case op::Y:
          SPOT_UNIMPLEMENTED();
        //O a translated as formula::tt S a
        case op::O:
          if (!can_be_past)
            {
              std::cerr << "Wrong format for the formula !" << std::endl;
              exit(1);
            }
          for (unsigned index = 0; index < f->size(); ++index)
            rec_translation(&f[index], true, false);
          *f = since_translation(formula::binop(op::S, formula::tt(), (*f)[0]));
          break;
        //H a translated as 'not O not a' -> not (formula::tt S not a)
        case op::H:
          if (!can_be_past)
            {
              std::cerr << "Wrong format for the formula !" << std::endl;
              exit(1);
            }
          for (unsigned index = 0; index < f->size(); ++index)
            rec_translation(&f[index], true, false);
          *f = since_translation(formula::unop(op::Not, formula::binop(op::S,
                  formula::tt(), formula::unop(op::Not, (*f)[0]))));
          break;
        default:
          for (unsigned index = 0; index < f->size(); ++index)
              rec_translation(&f[index], can_be_past, can_be_future);
      }
    return *f;
  }

  //exported function
  formula
  translate_past(formula& f)
  {
    return rec_translation(&f, false, true);
  }
}
