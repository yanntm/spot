// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement de
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
// along with this program.  If not, see <http://www.gnu.org/licenses/>.



#include "ltlparse/public.hh"
#include "ltlast/allnodes.hh"
#include "tgba/univmodel.hh"
#include "tgbaalgos/dotty.hh"


void syntax(const char* prog)
{
  std::cerr << "Usage: " << prog << " RESTRICT AP...\n";
  exit(0);
}

int main(int argc, char** argv)
{
  if (argc <= 1)
    syntax(argv[0]);

  const spot::ltl::formula* restrict;
  spot::ltl::environment& env(spot::ltl::default_environment::instance());

  {
    spot::ltl::parse_error_list pel;
    restrict = spot::ltl::parse(argv[1], pel, env);
    if (spot::ltl::format_parse_errors(std::cerr, argv[1], pel))
      exit(1);
  }

  spot::ltl::atomic_prop_set ap;
  for (int i = 2; i < argc; ++i)
    ap.insert(static_cast<const spot::ltl::atomic_prop*>
	       (env.require(argv[i])));

  {
    spot::bdd_dict d;
    spot::tgba* univ = spot::universal_model(&d, &ap, restrict);

    spot::dotty_reachable(std::cout, univ);

    delete univ;
  }

  spot::ltl::atomic_prop_set::const_iterator i = ap.begin();
  while (i != ap.end())
   (*i++)->destroy();
  restrict->destroy();

  spot::ltl::atomic_prop::dump_instances(std::cerr);
  spot::ltl::unop::dump_instances(std::cerr);
  spot::ltl::binop::dump_instances(std::cerr);
  spot::ltl::multop::dump_instances(std::cerr);
  spot::ltl::automatop::dump_instances(std::cerr);
  assert(spot::ltl::atomic_prop::instance_count() == 0);
  assert(spot::ltl::unop::instance_count() == 0);
  assert(spot::ltl::binop::instance_count() == 0);
  assert(spot::ltl::multop::instance_count() == 0);
  assert(spot::ltl::automatop::instance_count() == 0);
}
