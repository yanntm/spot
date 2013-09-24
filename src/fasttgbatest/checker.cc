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
#include "ltlparse/public.hh"
#include "tgbaalgos/ltl2tgba_fm.hh"
#include "tgbaalgos/postproc.hh"
#include "tgbaalgos/randomgraph.hh"

// This part is for FASTTGBA
#include "fasttgbaalgos/tgba2fasttgba.hh"
#include "fasttgbaalgos/dotty_dfs.hh"
#include "fasttgbaalgos/lbtt_dfs.hh"
#include "fasttgba/fasttgba_product.hh"

// Emptiness check for fasttgba
#include "fasttgbaalgos/ec/cou99.hh"
#include "fasttgbaalgos/ec/unioncheck.hh"
#include "fasttgbaalgos/ec/opt/opt_dijkstra_scc.hh"
#include "fasttgbaalgos/ec/opt/opt_tarjan_scc.hh"


void syntax (char*)
{
  std::cout << std::endl;
  std::cout << "Syntax : [-rndgraph] phi" << std::endl;
  std::cout << std::endl;
  std::cout << "    phi :      the formula to check "
	    << "if opt -rndgraph is set then phi "
	    << "must be \"a|b|c....\" to declare AP" << std::endl
	    << std::endl;
  exit (1);
}

int main(int argc, char **argv)
{
  int return_value = 0;

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

  bool opt_random = false;

  // Wether we want to check with Cou - default -
  bool opt_cou = true;

  for (;;)
    {
      //  Syntax check
      if (argc < formula_index + 2)
      	syntax(argv[0]);

      ++formula_index;

      if (!strcmp(argv[formula_index], "-rndgraph"))
      	{
	  opt_random = true;
	  assert(argc == (formula_index + 2));
      	}

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

  // Parse formula
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
    }


  if (opt_random)
    {
      //  Generate a random graph
      spot::ltl::atomic_prop_set* apf =
	spot::ltl::atomic_prop_collect(f);
      a = spot::random_graph(1000, 0.01, apf, dict, 0.0001);

      //  Now use  New TGBA !
      spot::ap_dict* aps = new spot::ap_dict();
      spot::acc_dict* accs = new spot::acc_dict();
      const spot::fasttgba* ftgba = spot::tgba_2_fasttgba(a, *aps, *accs);

      //  Create instanciator
      spot::simple_instanciator si (ftgba);

      // Multi - emptiness check it
      bool cou99_result = false;
      spot::cou99  cou99_checker(&si);
      if (cou99_checker.check())
	cou99_result = true;

      bool unioncheck_result = false;
      spot::unioncheck unioncheck_checker(&si);
      if (unioncheck_checker.check())
	unioncheck_result = true;

      bool optdijkstracheck_result = false;
      spot::opt_dijkstra_ec opt_dijkstra_ec_checker(&si, "-cs");
      if (opt_dijkstra_ec_checker.check())
	optdijkstracheck_result = true;

      bool optdijkstracheckcs_result = false;
      spot::opt_dijkstra_ec opt_dijkstra_ec_cs_checker(&si, "+cs");
      if (opt_dijkstra_ec_cs_checker.check())
	optdijkstracheckcs_result = true;

      bool opttarjancheck_result = false;
      spot::opt_tarjan_ec opt_tarjan_ec_checker(&si, "-cs");
      if (opt_tarjan_ec_checker.check())
	opttarjancheck_result = true;

      bool opttarjancheckcs_result = false;
      spot::opt_tarjan_ec opt_tarjan_ec_cs_checker(&si, "+cs");
      if (opt_tarjan_ec_cs_checker.check())
	opttarjancheckcs_result = true;

      // Check the result
      if (cou99_result != unioncheck_result ||
	  cou99_result != optdijkstracheck_result ||
	  cou99_result != optdijkstracheckcs_result ||
	  cou99_result != opttarjancheck_result ||
	  cou99_result != opttarjancheckcs_result)
	{
	  spot::dotty_dfs dotty(ftgba);
	  dotty.run();

	  std::cout <<  "ERROR : Sum Up Results " << std::endl
		    <<  "   cou99                :  "
		    << (cou99_result? "true" : "false")
		    << std::endl
		    <<  "   opt_dijkstra_ec      :  "
		    << (optdijkstracheck_result? "true" : "false")
		    << std::endl
		    <<  "   opt_dijkstra_ec+cs   :  "
		    << (optdijkstracheckcs_result? "true" : "false")
		    << std::endl
		    <<  "   opt_tarjan_ec      :  "
		    << (opttarjancheck_result? "true" : "false")
		    << std::endl
		    <<  "   opt_tarjan_ec+cs   :  "
		    << (opttarjancheckcs_result? "true" : "false")
		    << std::endl
		    <<  "   unioncheck           :  "
		    << (unioncheck_result? "true" : "false")
		    << std::endl;
	  assert(false);
	}

      delete ftgba;
      delete accs;
      delete aps;
      delete apf;
    }
  else
    {
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
	  spot::simple_instanciator si (ftgba);
    	  spot::cou99  checker(&si);
    	  if (checker.check())
    	    {
    	      return_value = 1;
    	      std::cout << "A counterexample has been found" << std::endl;
    	    }
    	  else
    	    std::cout << "No counterexample has been found" << std::endl;
    	}
      delete ftgba;
      delete accs;
      delete aps;
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

  return return_value;
}
