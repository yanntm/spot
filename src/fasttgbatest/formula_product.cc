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
#include <string.h>

// This part is for TGBA
#include "ltlast/allnodes.hh"
#include "tgba/tgba.hh"
#include "tgba/tgbaproduct.hh"
#include "ltlparse/public.hh"
#include "tgbaalgos/ltl2tgba_fm.hh"
#include "tgbaalgos/postproc.hh"

// This part is for FASTTGBA
#include "fasttgbaalgos/tgba2fasttgba.hh"
#include "fasttgbaalgos/dotty_dfs.hh"
#include "fasttgbaalgos/lbtt_dfs.hh"
#include "fasttgba/fasttgba_product.hh"

void syntax (char* argv)
{
  // Display the supplied name unless it appears to be a libtool wrapper.
  char* slash = strrchr(argv, '/');
  if (slash)
    argv = slash + 1;

  std::cout << "Syntax: "
	    << std::endl << "\t" << argv << "  "
	    << "-f1 formula1 -f2 formula2"
	    << std::endl;
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

  // The automaton of the first formula
  const spot::tgba* af1 = 0;

  // The automaton of the second formula
  const spot::tgba* af2 = 0;

  // Should whether the formula be reduced
  bool simpltl = false;

  // The index of the formula
  int f1_index = 2;
  int f2_index = 4;

  //------------------------------------------------------------
  // This part is for the invocation
  //------------------------------------------------------------
  std::string opt1 = "f1";
  std::string opt2 = "f2";
  if (argc != 5)
    syntax(argv[0]);
  if (!opt1.compare(argv[f1_index - 1]))
    syntax(argv[0]);
  if (!opt2.compare(argv[f2_index - 1]))
    syntax(argv[0]);
  //------------------------------------------------------------

  // The formula string
  std::string input1 =  argv[f1_index];
  std::string input2 =  argv[f2_index];

  // The formula in spot
  const spot::ltl::formula* f1 = 0;
  const spot::ltl::formula* f2 = 0;

  //
  // Building the formula from the input
  //
  spot::ltl::parse_error_list pel;
  f1 = spot::ltl::parse(input1, pel, env, false);
  f2 = spot::ltl::parse(input2, pel, env, false);

  if (f1 && f2)
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
	    const spot::ltl::formula* t = simp->simplify(f1);
	    f1->destroy();
	    f1 = t;
	  }
	delete simp;
      }
      {
	spot::ltl::ltl_simplifier* simp = 0;
	if (simpltl)
	  simp = new spot::ltl::ltl_simplifier(redopt, dict);

	if (simp)
	  {
	    const spot::ltl::formula* t = simp->simplify(f2);
	    f2->destroy();
	    f2 = t;
	  }
	delete simp;
      }

      std::cout << "===>   "
		<< spot::ltl::to_string(f1) << std::endl;
      std::cout << "===>   "
		<< spot::ltl::to_string(f2) << std::endl;

      //
      // Building the TGBA of the formula
      //
      af1 = spot::ltl_to_tgba_fm(f1, dict);
      assert (af1);
      spot::postprocessor *pp1 = new spot::postprocessor();
      af1 = pp1->run(af1, f1);
      delete pp1;

      af2 = spot::ltl_to_tgba_fm(f2, dict);
      assert (af2);
      spot::postprocessor *pp2 = new spot::postprocessor();
      af2 = pp2->run(af2, f2);
      delete pp2;

      // -----------------------------------------------------
      // Start using fasttgba
      // -----------------------------------------------------

      // Decclare the dictionnary of atomic propositions that will be
      // used all along processing
      spot::ap_dict* aps = new spot::ap_dict();
      spot::acc_dict* accs = new spot::acc_dict();

      const spot::fasttgba* ftgba1 = spot::tgba_2_fasttgba(af1, *aps, *accs);
      spot::dotty_dfs dotty1(ftgba1);
      dotty1.run();
      const spot::fasttgba* ftgba2 = spot::tgba_2_fasttgba(af2, *aps, *accs);
      spot::dotty_dfs dotty2(ftgba2);
      dotty2.run();
      const spot::fasttgba_product prod (ftgba1, ftgba2);
      spot::dotty_dfs dotty3(&prod);
      dotty3.run();

      std::cout << "-------------> PRODUCT ! " << std::endl;
      const spot::tgba *prodbis = new spot::tgba_product(af1, af2);
      const spot::fasttgba* ftgbabis = spot::tgba_2_fasttgba(prodbis, *aps, *accs);
      spot::dotty_dfs dottybis(ftgbabis);
      dottybis.run();
      delete prodbis;

      delete ftgba1;
      delete ftgba2;
      delete aps;
      delete accs;
    }

  // Clean up
  f1->destroy();
  f2->destroy();
  delete af1;
  delete af2;
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
