// Copyright (C) 2012 Laboratoire de Recherche et Développement
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

#include <iostream>

// This part is for TGBA
#include "ltlast/allnodes.hh"
#include "ltlparse/public.hh"
#include "tgbaalgos/ltl2tgba_fm.hh"
#include "tgbaalgos/postproc.hh"

// This part is for FASTTGBA
#include "fasttgbaalgos/tgba2fasttgba.hh"
#include "fasttgbaalgos/dotty_dfs.hh"
#include "fasttgbaalgos/lbtt_dfs.hh"
#include "fasttgba/fasttgba_product.hh"

// Emptiness check for fasttgba
#include "fasttgbaalgos/ec/cou99.hh"


void syntax (char*)
{
  std::cout << "Syntax" << std::endl;
  exit (1);
}

int main(int argc, char **argv)
{

  //  The environement for LTL
  spot::ltl::environment& env(spot::ltl::default_environment::instance());
  // Option for the simplifier
  spot::ltl::ltl_simplifier_options redopt(false, false, false, false, false);

  //  The dictionnary
  spot::bdd_dict* dict = new spot::bdd_dict();

  // The automaton of the formula
  const spot::tgba* a = 0;

  // Should whether the formula be reduced
  bool simpltl = false;

  // The index of the formula
  int formula_index = 0;

  // Wether we want to check with Cou - default -
  bool opt_cou = true;

  for (;;)
    {
      //  Syntax check
      if (argc < formula_index + 2)
      	syntax(argv[0]);

      ++formula_index;

      // if (!strcmp(argv[formula_index], "-lbtt"))
      // 	{
      // 	  opt_lbtt = true;
      // 	  opt_dotty = false;
      // 	}
      // else
      if (argc == (formula_index + 1))
	break;
    }

  // The formula string
  std::string input =  argv[formula_index];
  // The formula in spot
  const spot::ltl::formula* f = 0;

  //
  // Building the formula from the input
  //
  spot::ltl::parse_error_list pel;
  f = spot::ltl::parse(input, pel, env, false);

  if (f)
    {
      //
      // Simplification for the formula among differents levels
      //
      {
	spot::ltl::ltl_simplifier* simp = 0;
	if (simpltl)
	  simp = new spot::ltl::ltl_simplifier(redopt, dict);

	if (simp)
	  {
	    const spot::ltl::formula* t = simp->simplify(f);
	    f->destroy();
	    f = t;
	  }
	delete simp;
      }

      std::cout << "===>   "
		<< spot::ltl::to_string(f) << std::endl;

      //
      // Building the TGBA of the formula
      //
      a = spot::ltl_to_tgba_fm(f, dict);
      assert (a);

      spot::postprocessor *pp = new spot::postprocessor();
      a = pp->run(a, f);
      delete pp;


      // -----------------------------------------------------
      // Start using fasttgba
      // -----------------------------------------------------

      // Decclare the dictionnary of atomic propositions that will be
      // used all along processing
      spot::ap_dict* aps = new spot::ap_dict();
      spot::acc_dict* accs = new spot::acc_dict();

      const spot::fasttgba* ftgba = spot::tgba_2_fasttgba(a, *aps, *accs);
      if (opt_cou)
	{
	  spot::cou99  checker(ftgba);
	  if (checker.check())
	    std::cout << "A counterexample has been found" << std::endl;
	  else
	    std::cout << "No counterexample has been found" << std::endl;
	}
      delete ftgba;
      delete aps;
      delete accs;
    }

  // Clean up
  f->destroy();
  delete a;
  delete dict;

  // Check effective clean up
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

  return 0;
}
