// -*- coding: utf-8 -*-x
// Copyright (C) 2014, 2015, 2017, 2018 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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
#include <spot/tl/formula.hh>
#include <spot/misc/match_word.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twaalgos/word.hh>
#include <iostream>
#include <cstdlib>

int main(int argc, char** argv)
{
  if (argc != 4)
    return 1;
  std::string form(argv[1]);
  spot::formula f = spot::parse_formula(form);
  auto d = spot::make_bdd_dict();
  spot::twa_word_ptr w = spot::parse_word(argv[2], d);
  int b = std::atoi(argv[3]);
  return match_word(f, w) != b;
}
