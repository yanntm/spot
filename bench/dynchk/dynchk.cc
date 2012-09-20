// Copyright (C) 2012 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
// Copyright (C) 2003, 2004, 2005, 2006, 2007 Laboratoire d'Informatique de
// Paris 6 (LIP6), département Systèmes Répartis
// Coopératifs (SRC), Université Pierre et Marie Curie.
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include <iostream>
#include <iomanip>
#include <cassert>
#include <fstream>
#include <string>
#include <cstdlib>
#include "ltlvisit/tostring.hh"
#include "ltlvisit/apcollect.hh"
#include "ltlast/allnodes.hh"
#include "ltlparse/public.hh"
#include "tgbaalgos/ltl2tgba_lacim.hh"
#include "tgbaalgos/ltl2tgba_fm.hh"
#include "tgbaalgos/ltl2taa.hh"
#include "tgba/bddprint.hh"
#include "tgbaalgos/save.hh"
#include "tgbaalgos/dotty.hh"
#include "tgbaalgos/lbtt.hh"
#include "tgbaalgos/rebuild.hh"
#include "tgba/tgbatba.hh"
#include "tgba/tgbasgba.hh"
#include "tgba/tgbaproduct.hh"
#include "tgba/futurecondcol.hh"
#include "tgbaalgos/reducerun.hh"
#include "tgbaalgos/degen.hh"
#include "tgbaparse/public.hh"
#include "neverparse/public.hh"
#include "tgbaalgos/dupexp.hh"
#include "tgbaalgos/minimize.hh"
#include "tgbaalgos/neverclaim.hh"
#include "tgbaalgos/reductgba_sim.hh"
#include "tgbaalgos/replayrun.hh"
#include "tgbaalgos/rundotdec.hh"
#include "tgbaalgos/sccfilter.hh"
#include "tgbaalgos/safety.hh"
#include "tgbaalgos/eltl2tgba_lacim.hh"
#include "tgbaalgos/gtec/gtec.hh"
#include "eltlparse/public.hh"
#include "misc/timer.hh"
#include "tgbaalgos/stats_hierarchy.hh"
#include "tgbaalgos/stats.hh"
#include "tgbaalgos/scc.hh"
#include "tgbaalgos/emptiness_stats.hh"
#include "tgbaalgos/scc.hh"
#include "kripkeparse/public.hh"
#include "tgbaalgos/scc_decompose.hh"
#include "tgbaalgos/simulation.hh"
#include "../iface/dve2/dve2.hh"

void
syntax(char* prog)
{
  // Display the supplied name unless it appears to be a libtool wrapper.
  char* slash = strrchr(prog, '/');
  if (slash && (strncmp(slash + 1, "lt-", 3) == 0))
    prog = slash + 4;

  std::cerr << "Usage: "<< prog << "[OPTIONS...] [-P|-B]file [formula|file]"
	    << std::endl
	    << "  -Pfile  multiply the formula automaton with the TGBA read"
	    << " from `file'\n"
	    << "  -Bfile read a dve file"
	    << std::endl
	    << "  -r1   reduce formula using basic rewriting" << std::endl
	    << "  -r2   reduce formula using class of eventuality and "
	    << "universality" << std::endl
	    << "  -r3   reduce formula using implication between "
	    << "sub-formulae" << std::endl
	    << "  -r4   reduce formula using all above rules" << std::endl
	    << "  -r5   reduce formula using tau03" << std::endl
	    << "  -r6   reduce formula using tau03+" << std::endl
	    << "  -r7   reduce formula using tau03+ and -r4" << std::endl
	    << "  -rd   display the reduced formula" << std::endl
	    << std::endl

	    << "Automaton reduction:" << std::endl
	    << "  -dT  decompose the automaton into terminal"
	    << std::endl

	    << "Automaton reduction:" << std::endl
	    << "  -m  minimise the automaton"
	    << std::endl;
  exit(2);
}

///
///
///
void print_results (const char* algo, std::string result, 
		    std::string type, int ticks,
		    int pstates, int ptrans,
		    int pdepth, std::string input)
{
  std::cout << "# Algo,"
	    << "Result,"
	    << "Type,"
	    << "Ticks,"
	    << "Product states,"
	    << "Product trans,"
	    << "Product depth,"
	    << "Formula"
	    << std::endl;

  std::cout << algo    << ","
	    << result  << ","
	    << type    << ","
	    << ticks   << "," 
	    << pstates << ","
	    << ptrans  << ","
	    << pdepth  << ","
	    << "\"" << input << "\""
	    << std::endl;
};


// Algos that will be used using a single B.A.
const char *dyn_degen [] =
  {
    "SE05_dyn",
    "SE05",
    // "CVWY90_dyn",
    // "CVWY90",
    "Cou99_dyn",
    "Cou99",
    0
  };

// Algos that will be used using a TGBA
const char *dyn_gen [] =
  {
    "Cou99_dyn",
    "Cou99",
    0
  };

/// This program perform emptiness over BA and TGBA 
/// using previous algorithms
int main(int argc, char **argv)
{
  //  The environement for LTL
  spot::ltl::environment& env(spot::ltl::default_environment::instance());
  // Option for the simplifier
  spot::ltl::ltl_simplifier_options redopt(false, false, false, false, false);

  // The reduction for the automaton 
  bool scc_filter_all = true;

  //  The dictionnary
  spot::bdd_dict* dict = new spot::bdd_dict();

  // Timer for computing all steps 
  spot::timer_map tm;

  // The automaton of the formula
  const spot::tgba* a = 0;

  // The system 
  spot::tgba* system = 0;

  // The product automaton
  const spot::tgba* product = 0;

  // the emptiness check instaniator 
  spot::emptiness_check_instantiator* echeck_inst = 0;

  // automaton reduction 
  bool opt_m = false;

  // Should whether the formula be reduced 
  bool simpltl = false;

  // Option for the decomposition
  bool opt_dT = false;
  bool opt_dW = false;
  bool opt_dS = false;
  spot::scc_decompose *sd = 0;

  // The index of the formula
  int formula_index = 0;

  // Variables for loading DVE models 
  bool load_dve_model = false;
  std::string dve_model_name;

  for (;;)
    {
      //  Syntax check
      if (argc < formula_index + 2)
	syntax(argv[0]);

      ++formula_index;

      if (!strcmp(argv[formula_index], "-m"))
	{
	  opt_m = true;
	}
      else if (!strcmp(argv[formula_index], "-r1"))
	{
	  simpltl = true;
	  redopt.reduce_basics = true;
	}
      else if (!strcmp(argv[formula_index], "-r2"))
	{
	  simpltl = true;
	  redopt.event_univ = true;
	}
      else if (!strcmp(argv[formula_index], "-r3"))
	{
	  simpltl = true;
	  redopt.synt_impl = true;
	}
      else if (!strcmp(argv[formula_index], "-r4"))
	{
	  simpltl = true;
	  redopt.reduce_basics = true;
	  redopt.event_univ = true;
	  redopt.synt_impl = true;
	}
      else if (!strcmp(argv[formula_index], "-r5"))
	{
	  simpltl = true;
	  redopt.containment_checks = true;
	}
      else if (!strcmp(argv[formula_index], "-r6"))
	{
	  simpltl = true;
	  redopt.containment_checks = true;
	  redopt.containment_checks_stronger = true;
	}
      else if (!strcmp(argv[formula_index], "-r7"))
	{
	  simpltl = true;
	  redopt.reduce_basics = true;
	  redopt.event_univ = true;
	  redopt.synt_impl = true;
	  redopt.containment_checks = true;
	  redopt.containment_checks_stronger = true;
	}
      else if (!strcmp(argv[formula_index], "-dT"))
	{
	  assert (!opt_dW);
	  assert (!opt_dS);
	  opt_dT = true;
	}
      else if (!strcmp(argv[formula_index], "-dW"))
	{
	  assert (!opt_dT);
	  assert (!opt_dS);
	  opt_dW = true;
	}
      else if (!strcmp(argv[formula_index], "-dS"))
	{
	  assert (!opt_dW);
	  assert (!opt_dT);
	  opt_dS = true;
	}
      else if (!strncmp(argv[formula_index], "-P", 2))
	{
	  tm.start("reading -P's argument");
	  spot::tgba_parse_error_list pel;
	  spot::tgba_explicit_string* s;
	  s = spot::tgba_parse(argv[formula_index] + 2,
			       pel, dict, env, env, false);
	  if (spot::format_tgba_parse_errors(std::cerr,
					     argv[formula_index] + 2, pel))
	    return 2;
	  s->merge_transitions();
	  tm.stop("reading -P's argument");
	  system = s;
	}
      else if (!strncmp(argv[formula_index], "-B", 2))
	{
	  // When using DVE model can't be preload as in case
	  // of spin..
	  // Set a flag that indicate it should be load 
	  load_dve_model = true;
	  dve_model_name = argv[formula_index] + 2;
	}
      else
	{
	  if (argc == (formula_index + 1))
	    break;
	  syntax(argv[0]);
	}
    }

  // We always need a system when it is a promela system
  if(!system && !load_dve_model)
    syntax(argv[0]);

  // and this system must not have acceptance conditions
  // for dynamic algorithms and Promela models
  if (system)
    assert(system->number_of_acceptance_conditions() == 0);


  // To work with files
  bool isfile = false;
  std::ifstream ifs(argv[formula_index]);
  if (ifs)
    {
      isfile = true;		// The passed argument is a file
    }

  //
  // Process all formula of a file: this allow to 
  // avoid loading of the system
  //
  bool again = true;
  int formula_number = 0;
  while (again) 
    {
      again = false;
      ++formula_number;

      // The formula string 
      std::string input;

      // Track the cases where input is a file
      if (!isfile)
	input =  argv[formula_index];
      else 
	{
	  again = true;
	  if (!getline(ifs, input))
	    break;
	}

      // The formula in spot 
      const spot::ltl::formula* f = 0;

      //
      // Building the formula from the input 
      //
      spot::ltl::parse_error_list pel;
      tm.start("parsing formula");
      f = spot::ltl::parse(input, pel, env, false);
      tm.stop("parsing formula");

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
		tm.start("reducing formula");
		const spot::ltl::formula* t = simp->simplify(f);
		f->destroy();
		tm.stop("reducing formula");
		f = t;
		//std::cout << spot::ltl::to_string(f) << std::endl;
	      }
	    delete simp;
	  }

	  // 
	  // Building the TGBA of the formula
	  // 
	  a = spot::ltl_to_tgba_fm(f, dict);
	  assert (a);

	  // 
	  // Remove unused SCC
	  // 
	  {
	    const spot::tgba* aut_scc = 0;
	    tm.start("reducing A_f w/ SCC");
	    aut_scc = spot::scc_filter(a, scc_filter_all);
	    delete a;
	    a = aut_scc;
	    tm.stop("reducing A_f w/ SCC");
	  }


	  //
	  // Minimized 
	  //
	  {
	    spot::tgba* minimized = 0;
	    if (opt_m)
	      {
		minimized = minimize_obligation(a);
		if (minimized)
		  {
		    delete a;
		    a = minimized;
		  }
		else{
		  minimized = spot::simulation(a);
		  if (minimized)
		    {
		      delete a;
		      a = minimized;
		    }
		}
	      }
	  }
	}

      /// Perform the terminal decomposition 
      /// 
      const spot::tgba* term = a;
      const spot::tgba* weak = a;
      const spot::tgba* strong = a;
      if (opt_dT)
	{
	  sd = new spot::scc_decompose (a, true);
      	  const spot::tgba* term_a = sd->terminal_automaton();
      	  if (term_a)
      	    {
      	      a = term_a;
      	    }
	  else
	    {
	      // Output in standard way then exit
	      // suppose that degeneralisation doesn't introduce 
	      // weak SCC
	      int i = 0;
	      while (*(dyn_degen+i))
	  	{
	  	  const char* algo = dyn_degen[i];
	  	  print_results(algo, "NODECOMP", "BA", 0, 0, 0, 0, input);
	  	  ++i;
	  	}
	      i= 0;
	      while (*(dyn_gen+i))
	      	{
	      	  const char* algo = dyn_gen[i];
	      	  print_results(algo, "NODECOMP", "TGBA", 0, 0, 0, 0, input);
	      	  ++i;
	      	}
	      goto finalize;
	    }
      	  assert (a);
	}

      if (opt_dW)
	{
	  sd = new spot::scc_decompose (a, true);
      	  const spot::tgba* weak_a = sd->weak_automaton();
      	  if (weak_a)
      	    {
      	      a = weak_a;
      	    }
	  else
	    {
	      // Output in standard way then exit
	      // suppose that degeneralisation doesn't introduce 
	      // weak SCC
	      int i = 0;
	      while (*(dyn_degen+i))
		{
		  const char* algo = dyn_degen[i];
		  print_results(algo, "NODECOMP", "BA", 0, 0, 0, 0, input);
		  ++i;
		}
	      i= 0;
	      while (*(dyn_gen+i))
		{
		  const char* algo = dyn_gen[i];
		  print_results(algo, "NODECOMP", "TGBA", 0, 0, 0, 0, input);
		  ++i;
		}
	      goto finalize;
	    }
      	  assert (a);
	}

      if (opt_dS)
	{
	  sd = new spot::scc_decompose (a, true);
      	  const spot::tgba* strong_a = sd->strong_automaton();
      	  if (strong_a)
      	    {
      	      a = strong_a;
      	    }
	  else
	    {
	      // Output in standard way then exit
	      // suppose that degeneralisation doesn't introduce 
	      // weak SCC
	      int i = 0;
	      while (*(dyn_degen+i))
	  	{
	  	  const char* algo = dyn_degen[i];
	  	  print_results(algo, "NODECOMP", "BA", 0, 0, 0, 0, input);
	  	  ++i;
	  	}
	      i= 0;
	      while (*(dyn_gen+i))
	      	{
	      	  const char* algo = dyn_gen[i];
	      	  print_results(algo, "NODECOMP", "TGBA", 0, 0, 0, 0, input);
	      	  ++i;
	      	}
	      goto finalize;
	    }
      	  assert (a);
	}



      /// The system has not yet be build  we have to create it 
      /// For dve models.
      ///
      /// If other formalism  are supported and computed "on-the-fly"
      /// just follow the same scheme for integrate them into this test_suite
      {
	if (load_dve_model)
	  {
	    spot::ltl::atomic_prop_set ap;
	    atomic_prop_collect(f, &ap);
	    system = spot::load_dve2(dve_model_name,
				     dict, &ap, 
				     spot::ltl::constant::true_instance(),
				     2 /* 0 or 1 or 2 */ , true);
	  }
      }

      ///
      /// Build the composed system
      /// and perform emptiness on the degeneralized formula
      ///
      {
	// The degeneralized automaton
	spot::tgba* degen = 0;

	// Degeneralize product
	tm.start("degeneralization");
	degen = spot::degeneralize(a);
	tm.stop("degeneralization");

	// Perform the product
	product = new spot::tgba_product(system, degen);

	// Walk all emptiness checks
	int i = 0;
	while (*(dyn_degen+i))
	  {
	    // The timer for the emptiness check
	    spot::timer_map tm_ec;

	    // Create the emptiness check procedure
	    const char* err;
	    const char* algo = dyn_degen[i];
	    echeck_inst =
	      spot::emptiness_check_instantiator::construct( algo, &err);
	    if (!echeck_inst)
	      {
		exit(2);
	      }

	    // Create a specifier 
	    spot::formula_emptiness_specifier *es  = 0;
	    es = new spot::formula_emptiness_specifier (product, degen);

	    // Instanciate the emptiness check
	    spot::emptiness_check* ec  =  echeck_inst->instantiate(product, es);

	    tm_ec.start("checking");
	    spot::emptiness_check_result* res = ec->check();
	    tm_ec.stop("checking");

	    //
	    // Now output results
	    //
	    {
	      // spot::tgba_statistics a_size =
	      // 	spot::stats_reachable(ec->automaton());
	      const spot::ec_statistics* ecs =
		dynamic_cast<const spot::ec_statistics*>(ec);
  
	      std::cout << "# Algo,"
			<< "Result,"
			<< "Type,"
			<< "Ticks,"
			<< "Product states,"
			<< "Product trans,"
			<< "Product depth,"
			// << "System states," 
			// << "System trans,"
			<< "Formula"
			<< std::endl;
	      std::cout << algo << ","
			<< (res ? "VIOLATED,":"VERIFIED,")
			<< "BA,"
			<< tm_ec.timer("checking").utime() + tm_ec.timer("checking").stime() << "," 
			<< ecs->states() << ","
			<< ecs->transitions() << ","
			<< ecs->max_depth() << ","
			// << a_size.states << ","
			// << a_size.transitions << ","
			<< "\"" << input << "\""
			<< std::endl;
	    }

	    i++;
	    delete echeck_inst;
	    delete es;
	  }
	if (degen)
	  delete degen;
      }

      ///
      /// Build the composed system
      /// and perform emptiness on the  formula not degeneralized
      ///
      {
	// Perform the product
	product = new spot::tgba_product(system, a);

	// Walk all emptiness checks
	int i = 0;
	while (*(dyn_gen+i))
	  {
	    // The timer for the emptiness check
	    spot::timer_map tm_ec;

	    // Create the emptiness check procedure
	    const char* err;
	    const char* algo = dyn_gen[i];
	    echeck_inst =
	      spot::emptiness_check_instantiator::construct( algo, &err);
	    if (!echeck_inst)
	      {
		exit(2);
	      }

	    // Create a specifier 
	    spot::formula_emptiness_specifier *es  = 0;
	    es = new spot::formula_emptiness_specifier (product, a);

	    // Instanciate the emptiness check
	    spot::emptiness_check* ec  =  echeck_inst->instantiate(product, es);

	    tm_ec.start("checking");
	    spot::emptiness_check_result* res = ec->check();
	    tm_ec.stop("checking");

	    //
	    // Now output results
	    //
	    {
	      // spot::tgba_statistics a_size =
	      // 	spot::stats_reachable(ec->automaton());
	      const spot::ec_statistics* ecs =
		dynamic_cast<const spot::ec_statistics*>(ec);
  
	      std::cout << "# Algo,"
			<< "Result,"
			<< "Type,"
			<< "Ticks,"
			<< "Product states,"
			<< "Product trans,"
			<< "Product depth,"
			// << "System states," 
			// << "System trans,"
			<< "Formula"
			<< std::endl;
	      std::cout << algo << ","
			<< (res ? "VIOLATED,":"VERIFIED,")
			<< "TGBA,"
			<< tm_ec.timer("checking").utime() + tm_ec.timer("checking").stime() << "," 
			<< ecs->states() << ","
			<< ecs->transitions() << ","
			<< ecs->max_depth() << ","
			// << a_size.states << ","
			// << a_size.transitions << ","
			<< "\"" << input << "\""
			<< std::endl;
	    }

	    i++;
	    delete echeck_inst;
	    delete es;
	  }
      }

      // refix decomposition
      if (opt_dT)
	a = term;
      if (opt_dW)
	a = weak;
      if (opt_dS)
	a = strong;

    finalize:
      // Clean up
      f->destroy();
      delete product;
      delete sd;

      // Delete formula automaton
      delete a;

      if (load_dve_model)
	delete system;
      
    }

  if (!(load_dve_model))
    delete system;
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
