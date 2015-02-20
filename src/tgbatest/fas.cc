// -*- coding: utf-8 -*-
// Copyright (C) 2014, 2015 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
// Copyright (C) 2003, 2004 Laboratoire d'Informatique de Paris 6 (LIP6),
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

#include <fstream>
#include <vector>
#include "graph/bidigraph.hh"
#include "hoaparse/public.hh"
#include "tgba/bdddict.hh"
#include "tgbaalgos/fas.hh"
#include "tgbaalgos/hoa.hh"
#include "tgbaalgos/mask.hh"

int main(int argc, char* argv[])
{
  argc = argc;
  if (argc <= 1)
    {
      std::cerr << "./fas [hoa_file]" << std::endl;
      return 1;
    }

  auto dict = spot::make_bdd_dict();
  spot::ltl::environment& env(spot::ltl::default_environment::instance());
  spot::hoa_parse_error_list pel;
  spot::hoa_aut_ptr h = spot::hoa_parse(argv[1], pel, dict, env);

  if (spot::format_hoa_parse_errors(std::cerr, argv[1], pel))
    return 2;

  spot::tgba_digraph_ptr a = h->aut;

  spot::fas* is_fas = new spot::fas(a);
  is_fas->build();

  // For each arc in the fas, set its acceptance mark
  // This makes it easier to see which arcs form the FAS
  {
    auto res = make_tgba_digraph(a->get_dict());
    res->copy_ap_of(a);
    res->prop_copy(a, { false, false, false, false });
    res->copy_acceptance_conditions_of(a);
    transform_copy(a, res, [&](unsigned src,
                                bdd& cond,
                                spot::acc_cond::mark_t&,
                                unsigned dst)
                   {
                     if ((*is_fas)(src, dst))
                       cond = bddfalse;
                   });
    hoa_reachable(std::cout, res, nullptr);
    std::cout << std::endl;
  }

  delete is_fas;
}
