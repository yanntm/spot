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
#include <vector>

namespace spot
{

  // transforms formula 'a S b' as 'Concat (Star(formula::tt), b, Star(a))'
  static formula
  since_translation(const formula& f)
  {
    if (f.kind() != op::S)
      {
        std::cerr << "since_translation cannot take any operator except S "
                  << std::endl;
        exit(1);
      }
    return formula::multop(op::Concat, {formula::bunop(op::Star,
       formula::tt(), 1), f[1], formula::bunop(op::Star, f[0], 1)});
  }

  // transforms formula 'a E b' as 'Concat (Star(formula::tt), b, Star(a))'
  static formula
  ergo_translation(const formula& f)
  {
    if (f.kind() != op::E)
      {
        std::cerr << "since_translation cannot take any operator except S "
                  << std::endl;
        exit(1);
      }
    return formula::multop(op::Or,
      {
        formula::multop(op::Concat,
          {
            formula::bunop(op::Star,f[1], 1)
          }),
          formula::multop(op::Concat,
          {
            formula::bunop(op::Star, formula::tt(), 1),
            formula::unop(op::Not, f[1]),
            formula::bunop(op::Star, formula::tt(), 1),
            f[0],
            formula::bunop(op::Star, formula::tt(), 1)
          })
      });
  }

  // recursive translation of a ltl+past formula using regular expression
  // formula should have a future operator before a past operator
  // ex: G(a S b)
  // formula should NOT have a future operator after a past operator
  // ex: G(a S (F b))
  static formula
  rec_translation(formula f, const bool can_be_past, const bool can_be_future)
  {
    std::cout << f.kindstr() << std::endl;
    switch (f.kind())
      {
        case op::M:
        case op::R:
        case op::W:
        case op::U:
          std::cout << "future " << f.kindstr() << std::endl;
          if (!can_be_future)
            {
              std::cerr << "Wrong format for the formula !" << std::endl;
              exit(1);
            }
          return formula::binop(f.kind(),\
              rec_translation(f[0], can_be_past, true),
              rec_translation(f[1], can_be_past, true));


      case op::F:
      case op::G:
        if (!can_be_future)
          {
            std::cerr << "Wrong format for the formula !" << std::endl;
            exit(1);
          }
        return formula::unop(f.kind(), rec_translation(f[0], true, true));


        case op::E:
          if (!can_be_past)
            {
              std::cerr << "Wrong format for the formula !" << std::endl;
              exit(1);
            }
          return ergo_translation(formula::binop(op::E,
              rec_translation(f[0], false, true),
              rec_translation(f[1], false, true)));


        case op::S:
          std::cout << "Since" << std::endl;
          if (!can_be_past)
            {
              std::cerr << "Wrong format for the formula !" << std::endl;
              exit(1);
            }
          f = since_translation(formula::binop(op::S,
             rec_translation(f[0], false, true),
             rec_translation(f[1], false, true)));
          std::cout << f << std::endl;
          return f;


        case op::Y:
          SPOT_UNIMPLEMENTED();


        // 'O a' translated as 'tt S a'
        case op::O:
          if (!can_be_past)
            {
              std::cerr << "Wrong format for the formula !" << std::endl;
              exit(1);
            }
          f = since_translation(formula::binop(op::S, formula::tt(),
                                rec_translation(f[0], true, false)));
          std::cout << f << std::endl;
          return f;


        //H a translated as 'not O not a' <-> not (formula::tt S not a)
        //therefore H a <-> formula::ff E a
        case op::H:
          if (!can_be_past)
            {
              std::cerr << "Wrong format for the formula !" << std::endl;
              exit(1);
            }
          return ergo_translation(formula::binop(op::E, formula::ff(),
                                 rec_translation(f[0], true, false)));


        case op::ap:
        case op::tt:
        case op::ff:
        case op::eword:
          return f;

        case op::Not:
        case op::Closure:
        case op::NegClosure:
        case op::NegClosureMarked:
          return formula::unop(f.kind(),
                          rec_translation(f[0], can_be_past, can_be_future));

        case op::Xor:
        case op::Implies:
        case op::Equiv:
        case op::EConcat:
        case op::EConcatMarked:
        case op::UConcat:
          return formula::binop(f.kind(),
                            rec_translation(f[0], can_be_past, can_be_future),
                            rec_translation(f[0], can_be_past, can_be_future));


      case op::Or:
      case op::OrRat:
      case op::And:
      case op::AndRat:
      case op::AndNLM:
      case op::Concat:
      case op::Fusion:
        {
          std::vector<formula> vect;
          for (unsigned i = 0; i < f.size(); ++i)
            {
              vect.push_back(rec_translation(f[i], can_be_past, can_be_future));
            }
          return formula::multop(f.kind(), vect);
        }


        default:
          std::cout << "unimplemented" << std::endl;
      }
    return f;
  }

  //exported function
  formula
  translate_past(const formula& f)
  {
    return rec_translation(f, false, true);
  }
}
