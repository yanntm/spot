// -*- coding: utf-8 -*-
// Copyright (C) 2014, 2015 Laboratoire de Recherche et Développement
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

#include <fstream>
#include "ltlast/formula.hh"
#include "ltlparse/public.hh"
#include "graph/bidigraph.hh"
#include "tgbaalgos/ltl2tgba_fm.hh"
#include "tgba/bdddict.hh"

int main(int argc, char* argv[])
{
  argc = argc;
  if (argc <= 1)
    {
      std::cerr << "./bidigraph [ltl_formula]" << std::endl;
      return 1;
    }

  spot::ltl::parse_error_list pel;
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const spot::ltl::formula* f = spot::ltl::parse(argv[1], pel);
  spot::tgba_digraph_ptr aut = spot::ltl_to_tgba_fm(f, dict);

  spot::graph::bidigraph* bdg = new spot::graph::bidigraph(aut, true);
  bdg->build();
  std::cout << *bdg;

  delete bdg;
  f->destroy();
}
