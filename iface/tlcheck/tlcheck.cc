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
#include "tgbaalgos/accpostproc.hh"
#include "tgbaalgos/postproc.hh"

#include <iostream>
#include <fstream>

#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#ifdef TRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

void
syntax(char* prog)
{
  // Display the supplied name unless it appears to be a libtool wrapper.
  char* slash = strrchr(prog, '/');
  if (slash && (strncmp(slash + 1, "lt-", 3) == 0))
    prog = slash + 4;

  std::cerr << "Usage: "<< prog << " [OPTIONS...] [-P|-B] formula"
	    << std::endl

	    << "  -Pfile        read a TGBA file" 	     << std::endl
	    << "  -Bfile        read a DVE file"	     << std::endl
	    << "  --minimize    minimise all automaton"      << std::endl
	    << "  --decompose   decompose formula automaton" << std::endl
	    << std::endl;
  exit(2);
}

///
/// This is a main wrapper to have an homogeneous
/// wrapper
///
void
print_results (const char* algo, std::string result,
	       std::string type, int ticks,
	       int pstates, int ptrans,
	       int pdepth, std::string input,
	       std::ostream& os = std::cout,
	       bool header = false)
{
  // We want to check the result to have a fast
  // comparison for the decomposition.
  if (header)
    os << result << std::endl;

  os << "# Algo,"
     << "Result,"
     << "Type,"
     << "Ticks,"
     << "Product states,"
     << "Product trans,"
     << "Product depth,"
     << "Formula"
     << std::endl;

  os << algo    << ","
     << result  << ","
     << type    << ","
     << ticks   << ","
     << pstates << ","
     << ptrans  << ","
     << pdepth  << ","
     << "\"" << input << "\""
     << std::endl;
  if (result == "VIOLATED")
    os << "an accepting run exists" << std::endl;
  else
    os << "no accepting run found" << std::endl;
};



///
/// A wrapper to call all emptiness checks
/// Warning this wrapper encompasses transformation
/// to degeneralize and so on
///
void
perform_emptiness_check (const spot::tgba* a,
			 const spot::tgba* system,
			 const char* algo,
			 std::string input,
			 std::ostream& os = std::cout,
			 bool header = false)

{
  // First we check if a is correct (i.e not NULL)
  // because decomposition can lead to NULL automaton
  // when no decomposition is empty
  if (!a)
    {
      print_results(algo,"NODECOMP", "BA",
		    0, 0, 0, 0, input, os, header);
      return;
    }

  // Perform the product
  const spot::tgba* product = new spot::tgba_product(system, a);

  // The timer for the emptiness check
  spot::timer_map tm_ec;
  const char* err;

  // the emptiness check instaniator
  spot::emptiness_check_instantiator* echeck_inst = 0;
  echeck_inst = spot::emptiness_check_instantiator::construct( algo, &err);

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
    const spot::ec_statistics* ecs =
      dynamic_cast<const spot::ec_statistics*>(ec);

    print_results(algo,(res ? "VIOLATED":"VERIFIED"), "BA",
		  tm_ec.timer("checking").utime() +
		  tm_ec.timer("checking").stime(),
		  ecs->states(), ecs->transitions(),
		  ecs->max_depth(),
		  input, os, header);
  }

  // Clear Memory
  delete product;
  delete es;
  delete ec;
}

/// This program perform emptiness check over formula
int main(int argc, char **argv)
{
  //  The environement for LTL
  spot::ltl::environment& env(spot::ltl::default_environment::instance());
  // Option for the simplifier
  spot::ltl::ltl_simplifier_options redopt(true, true, true, true, true);

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

  // automaton reduction, we always want to do it...
  bool opt_minimize = false;

  // Option for the decomposition
  bool opt_decompose = false;
  spot::scc_decompose *sd = 0;

  bool opt_SE05opt = false;

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

      if (!strcmp(argv[formula_index], "--minimize"))
  	{
  	  opt_minimize = true;
  	}
      else if (!strcmp(argv[formula_index], "--decompose"))
  	{
	  opt_decompose = true;
  	}
      else if (!strcmp(argv[formula_index], "-SE05opt"))
  	{
  	  opt_SE05opt = true;
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
  if (!system && !load_dve_model)
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

      ///
      /// Building the formula from the input
      ///
      spot::ltl::parse_error_list pel;
      tm.start("parsing formula");
      f = spot::ltl::parse(input, pel, env, false);
      tm.stop("parsing formula");

      if (!f)
	{
	  std::cerr << "Error parsing formula" << std::endl;
	}

      ///
      /// Simplification for the formula among differents levels
      ///
      {
	spot::ltl::ltl_simplifier* simp = 0;
	simp = new spot::ltl::ltl_simplifier(redopt, dict);

	if (simp)
	  {
	    tm.start("reducing formula");
	    const spot::ltl::formula* t = simp->simplify(f);
	    f->destroy();
	    tm.stop("reducing formula");
	    f = t;
	  }
	delete simp;
      }

      //
      // Building the TGBA of the formula
      //
      a = spot::ltl_to_tgba_fm(f, dict);
      assert (a);

      ///
      /// We use postproc to reduce the automaton
      ///
      spot::postprocessor *pp = new spot::postprocessor();
      a = pp->run(a, f);
      delete pp;


      /// The system has not yet be build  we have to create it
      /// For dve models.
      ///
      /// If other formalism  are supported and computed "on-the-fly"
      /// just follow the same scheme for integrate them here
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
      /// Perform the terminal decomposition
      ///
      const spot::tgba* term = 0;
      const spot::tgba* weak = a;
      const spot::tgba* strong = a;
      if (opt_decompose)
	{
	  pid_t term_pid = 0;
	  pid_t weak_pid = 0;
	  pid_t strong_pid = 0;

	  // Decompose the automaton
	  sd = new spot::scc_decompose (a, opt_minimize);

	  // Test for the creation of a son
	  if ((term_pid = fork()) == -1)
	    {
	      std::cerr << "Fork Fail for terminal decomposition"
			<< std::endl;
	    }

	  // We are in the son (Terminal)
	  if (!term_pid)
	    {
	      std::cout << " -->Terminal Compute (pid: "
			<< getpid() << ")" << std::endl;

	      // Grab the terminal automaton
	      term = sd->terminal_automaton();

	      // Create the output name
	      std::ostringstream oss;
	      oss << "pid."<< (int) getpid();
	      std::ofstream os(oss.str().c_str());
	      perform_emptiness_check (term, system, "REACHABILITY",
				       input, os, true);
	      os.close();
	      goto finalize;
	    }

	  // Test for the creation of a new son
	  if ((weak_pid = fork()) == -1)
	    {
	      std::cerr << "Fork Fail for Weak decomposition"
	  		<< std::endl;
	    }

	  // We are in the son (Weak)
	  if (!weak_pid)
	    {
	      std::cout << " -->Weak Compute (pid: "
	  		<< getpid() << ")" << std::endl;

	      // Grab the weak automaton
	      weak = sd->weak_automaton();

	      // Create the output name
	      std::ostringstream oss;
	      oss << "pid."<< (int) getpid();
	      std::ofstream os(oss.str().c_str());
	      perform_emptiness_check (weak, system, "DFS",
				       input, os, true);
	      os.close();
	      goto finalize;
	    }

	  // Test for the creation of a son
	  if ((strong_pid = fork()) == -1)
	    {
	      std::cerr << "Fork Fail for terminal decomposition"
	  		<< std::endl;
	    }

	  // We are in the son
	  if (!strong_pid)
	    {
	      std::cout << " -->Strong Compute (pid: "
	  		<< getpid() << ")" << std::endl;

	      // Grab the strong automaton
	      strong = sd->strong_automaton();

	      // Create the output name
	      std::ostringstream oss;
	      oss << "pid."<< (int) getpid();
	      std::ofstream os(oss.str().c_str());
	      perform_emptiness_check (strong, system, "Cou99",
				       input, os, true);
	      os.close();
	      goto finalize;
	    }

	  // The father wait for all son and only keep the faster
	  int waited = 0;
	  std::string line;
	  while (true)
	    {
	      pid_t pid;
	      pid = wait (NULL);
	      std::cout << "    -->Answer From " << pid << std::endl;

	      // Grab information if violated or not
	      std::ostringstream oss;
	      oss << "pid."<< (int) pid;
	      std::ifstream ifs (oss.str().c_str());
	      getline (ifs, line);

	      // The property is violated , can kill others
	      if (line == "VIOLATED")
		{
		  // Display results for violated properties
		  while (getline(ifs, line))
		    std::cout << line << std::endl;

		  // The faster is terminal
		  if (pid == term_pid)
		    {
		      kill (9, weak_pid);
		      kill (9, strong_pid);
		    }
		  // The faster is weak
		  else if (pid == weak_pid)
		    {
		      kill (9, term_pid);
		      kill (9, strong_pid);
		    }
		  // The faster is strong
		  else
		    {
		      kill (9, term_pid);
		      kill (9, weak_pid);
		    }
		  ifs.close();
		  break;
		}

	      // Conditionnal Stop
	      ++waited;
	      if (waited == 3)
		{
		  // Display results for verified properties
		  while (getline(ifs, line))
		    std::cout << line << std::endl;
		  break;
		}
	      ifs.close();
	    }
	}
      else
	perform_emptiness_check (strong, system, "Cou99", input);


    finalize:
      // Clean up
      f->destroy();
      delete sd;

      // Delete formula automaton
      delete a;
      delete system;
      system = 0;
      delete product;
      product = 0;
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











 // if (opt_dT)
 // 	{
 // 	  sd = new spot::scc_decompose (a, opt_m);
 // 	  const spot::tgba* term_a = sd->terminal_automaton();
 // 	  if (term_a)
 // 	    {
 // 	      a = term_a;
 // 	    }
 // 	  else
 // 	    {
 // 	      // Output in standard way then exit
 // 	      // suppose that degeneralisation doesn't introduce
 // 	      // weak SCC
 // 	      int i = 0;
 // 	      while (*(dyn_degen+i)  && opt_BA)
 // 	  	{
 // 	  	  const char* algo = dyn_degen[i];
 // 	  	  print_results(algo, "NODECOMP", "BA", 0, 0, 0, 0, input);
 // 	  	  ++i;
 // 	  	}
 // 	      i= 0;
 // 	      while (*(dyn_gen+i) && opt_TGBA)
 // 	      	{
 // 	      	  const char* algo = dyn_gen[i];
 // 	      	  print_results(algo, "NODECOMP", "TGBA", 0, 0, 0, 0, input);
 // 	      	  ++i;
 // 	      	}
 // 	      goto finalize;
 // 	    }
 // 	  assert (a);
 // 	}

 // if (opt_dW)
 // 	{
 // 	  sd = new spot::scc_decompose (a, opt_m);
 // 	  const spot::tgba* weak_a = sd->weak_automaton();
 // 	  if (weak_a)
 // 	    {
 // 	      a = weak_a;
 // 	    }
 // 	  else
 // 	    {
 // 	      // Output in standard way then exit
 // 	      // suppose that degeneralisation doesn't introduce
 // 	      // weak SCC
 // 	      int i = 0;
 // 	      while (*(dyn_degen+i)  && opt_BA)
 // 		{
 // 		  const char* algo = dyn_degen[i];
 // 		  print_results(algo, "NODECOMP", "BA", 0, 0, 0, 0, input);
 // 		  ++i;
 // 		}
 // 	      i= 0;
 // 	      while (*(dyn_gen+i)  && opt_TGBA)
 // 		{
 // 		  const char* algo = dyn_gen[i];
 // 		  print_results(algo, "NODECOMP", "TGBA", 0, 0, 0, 0, input);
 // 		  ++i;
 // 		}
 // 	      goto finalize;
 // 	    }
 // 	  assert (a);
 // 	}

 // if (opt_dS)
 // 	{
 // 	  sd = new spot::scc_decompose (a, opt_m);
 // 	  const spot::tgba* strong_a = sd->strong_automaton();
 // 	  if (strong_a)
 // 	    {
 // 	      a = strong_a;
 // 	    }
 // 	  else
 // 	    {
 // 	      // Output in standard way then exit
 // 	      // suppose that degeneralisation doesn't introduce
 // 	      // weak SCC
 // 	      int i = 0;
 // 	      while (*(dyn_degen+i) && opt_BA)
 // 	  	{
 // 	  	  const char* algo = dyn_degen[i];
 // 	  	  print_results(algo, "NODECOMP", "BA", 0, 0, 0, 0, input);
 // 	  	  ++i;
 // 	  	}
 // 	      i= 0;
 // 	      while (*(dyn_gen+i) && opt_TGBA)
 // 	      	{
 // 	      	  const char* algo = dyn_gen[i];
 // 	      	  print_results(algo, "NODECOMP", "TGBA", 0, 0, 0, 0, input);
 // 	      	  ++i;
 // 	      	}
 // 	      goto finalize;
 // 	    }
 // 	  assert (a);
 // 	}

 // /// The system has not yet be build  we have to create it
 // /// For dve models.
 // ///
 // /// If other formalism  are supported and computed "on-the-fly"
 // /// just follow the same scheme for integrate them into this test_suite
 // {
 // 	if (load_dve_model)
 // 	  {
 // 	    spot::ltl::atomic_prop_set ap;
 // 	    atomic_prop_collect(f, &ap);
 // 	    system = spot::load_dve2(dve_model_name,
 // 				     dict, &ap,
 // 				     spot::ltl::constant::true_instance(),
 // 				     2 /* 0 or 1 or 2 */ , true);
 // 	  }
 // }


 // // ------------> NASTY TRICK
 // ///
 // /// Build the composed system
 // /// and perform the most efficient emptiness
 // ///
 // if (opt_dT || opt_dW || (opt_dS && opt_BA))
 // 	{
 // 	  const spot::tgba* degen = a;

 // 	  // The environment
 // 	  spot::ltl::declarative_environment *envacc =  spot::create_env_acc();

 // 	  if (opt_dS)
 // 	    {
 // 	      // Degeneralize product
 // 	      tm.start("degeneralization");
 // 	      if (a->number_of_acceptance_conditions() > 1)
 // 		degen = spot::degeneralize(a);
 // 	      tm.stop("degeneralization");

 // 	      // Add fake acceptance condition
 // 	      const spot::tgba* tmp = add_fake_acceptance_condition (degen, envacc);
 // 	      if (degen != a)
 // 		delete degen;
 // 	      degen = tmp;
 // 	    }


 // 	  // Perform the product
 // 	  product = new spot::tgba_product(system, degen);

 // 	  // The timer for the emptiness check
 // 	  spot::timer_map tm_ec;

 // 	  // Create the emptiness check procedure
 // 	  const char* err;
 // 	  const char* algo = "SE05_OPT";
 // 	  if (opt_dT)
 // 	    algo = "REACHABILITY";
 // 	  else if (opt_dW)
 // 	    algo = "DFS";
 // 	  else if (opt_dS)
 // 	    algo = "SE05_OPT";

 // 	  echeck_inst =
 // 	    spot::emptiness_check_instantiator::construct( algo, &err);
 // 	  if (!echeck_inst)
 // 	    {
 // 	      exit(2);
 // 	    }

 // 	  // Create a specifier
 // 	  spot::formula_emptiness_specifier *es  = 0;
 // 	  es = new spot::formula_emptiness_specifier (product, degen);

 // 	  // Instanciate the emptiness check
 // 	  spot::emptiness_check* ec  =  echeck_inst->instantiate(product, es, envacc);

 // 	  tm_ec.start("checking");
 // 	  spot::emptiness_check_result* res = ec->check();
 // 	  tm_ec.stop("checking");

 // 	  //
 // 	  // Now output results
 // 	  //
 // 	  const spot::ec_statistics* ecs =
 // 	    dynamic_cast<const spot::ec_statistics*>(ec);

 // 	  print_results(algo,(res ? "VIOLATED":"VERIFIED"), "BA",
 // 			tm_ec.timer("checking").utime() +
 // 			tm_ec.timer("checking").stime(),
 // 			  ecs->states(), ecs->transitions(),
 // 			ecs->max_depth(),
 // 			input);
 // 	  delete echeck_inst;
 // 	  delete es;
 // 	  delete envacc;
 // 	  if (degen != a)
 // 	    delete degen;

 // 	  goto decompfinal;
 // 	}


 // if (opt_SE05opt)
 // 	{
 // 	  // The degeneralized automaton
 // 	  const spot::tgba* degen = a;
 // 	  spot::timer_map tm_ec;

 // 	  // Degeneralize product
 // 	  tm.start("degeneralization");
 // 	  if (a->number_of_acceptance_conditions() != 1)
 // 	    degen = spot::degeneralize(a);
 // 	  tm.stop("degeneralization");

 // 	  // The environment
 // 	  spot::ltl::declarative_environment *envacc =  spot::create_env_acc();

 // 	  // Add fake acceptance condition
 // 	  const spot::tgba* tmp = add_fake_acceptance_condition (degen, envacc);
 // 	  if (degen != a)
 // 	    delete degen;
 // 	  degen = tmp;

 // 	  // Perform the product
 // 	  product = new spot::tgba_product(system, degen);

 // 	  // Create the emptiness check procedure
 // 	  const char* err;
 // 	  const char* algo = "SE05_OPT";
 // 	  echeck_inst =
 // 	    spot::emptiness_check_instantiator::construct (algo, &err);
 // 	  if (!echeck_inst)
 // 	    {
 // 	      exit(2);
 // 	    }

 // 	  // Create a specifier
 // 	  spot::formula_emptiness_specifier *es  = 0;
 // 	  es = new spot::formula_emptiness_specifier (product, degen);

 // 	  // Instanciate the emptiness check
 // 	  spot::emptiness_check* ec  =  echeck_inst->instantiate(product, es, envacc);

 // 	  tm_ec.start("checking");
 // 	  spot::emptiness_check_result* res = ec->check();
 // 	  tm_ec.stop("checking");

 // 	  //
 // 	  // Now output results
 // 	  //
 // 	  {
 // 	    const spot::ec_statistics* ecs =
 // 	      dynamic_cast<const spot::ec_statistics*>(ec);

 // 	    print_results(algo, (res ? "VIOLATED":"VERIFIED"), "BA",
 // 			      tm_ec.timer("checking").utime() +
 // 			  tm_ec.timer("checking").stime(),
 // 			  ecs->states(), ecs->transitions(),
 // 			  ecs->max_depth(),
 // 			  input);
 // 	  }

 // 	  delete echeck_inst;
 // 	  delete es;
 // 	  delete envacc;

 // 	  if (degen != a)
 // 	    delete degen;

 // 	  goto decompfinal;
 // 	}


 // ///
 // /// Build the composed system
 // /// and perform emptiness on the degeneralized formula
 // ///
 // if (opt_BA)
 // 	{
 // 	  // The degeneralized automaton
 // 	  const spot::tgba* degen = a;

 // 	  // Degeneralize product
 // 	  tm.start("degeneralization");
 // 	  // degen = spot::degeneralize(a);
 // 	  if (a->number_of_acceptance_conditions() > 1)
 // 	    degen = spot::degeneralize(a);
 // 	  tm.stop("degeneralization");

 // 	  // Perform the product
 // 	  product = new spot::tgba_product(system, degen);

 // 	  // Walk all emptiness checks
 // 	  int i = 0;
 // 	  while (*(dyn_degen+i))
 // 	    {
 // 	      // The timer for the emptiness check
 // 	      spot::timer_map tm_ec;

 // 	      // Create the emptiness check procedure
 // 	      const char* err;
 // 	      const char* algo = dyn_degen[i];
 // 	      echeck_inst =
 // 		spot::emptiness_check_instantiator::construct( algo, &err);
 // 	      if (!echeck_inst)
 // 		{
 // 		  exit(2);
 // 		}

 // 	      // Create a specifier
 // 	      spot::formula_emptiness_specifier *es  = 0;
 // 	      es = new spot::formula_emptiness_specifier (product, degen);

 // 	      // Instanciate the emptiness check
 // 	      spot::emptiness_check* ec  =  echeck_inst->instantiate(product, es);

 // 	      tm_ec.start("checking");
 // 	      spot::emptiness_check_result* res = ec->check();
 // 	      tm_ec.stop("checking");

 // 	      //
 // 	      // Now output results
 // 	      //
 // 	      {
 // 		const spot::ec_statistics* ecs =
 // 		  dynamic_cast<const spot::ec_statistics*>(ec);

 // 		print_results(algo,(res ? "VIOLATED":"VERIFIED"), "BA",
 // 			      tm_ec.timer("checking").utime() +
 // 			      tm_ec.timer("checking").stime(),
 // 			      ecs->states(), ecs->transitions(),
 // 			      ecs->max_depth(),
 // 			      input);
 // 	      }

 // 	      i++;
 // 	      delete echeck_inst;
 // 	      delete es;
 // 	    }
 // 	  if (degen != a)
 // 	    delete degen;
 // 	}

 // ///
 // /// Build the composed system
 // /// and perform emptiness on the  formula not degeneralized
 // ///
 // if (opt_TGBA)
 // 	{
 // 	  // Perform the product
 // 	  product = new spot::tgba_product(system, a);

 // 	  // Walk all emptiness checks
 // 	  int i = 0;
 // 	  while (*(dyn_gen+i))
 // 	    {
 // 	      // The timer for the emptiness check
 // 	      spot::timer_map tm_ec;

 // 	      // Create the emptiness check procedure
 // 	      const char* err;
 // 	      const char* algo = dyn_gen[i];
 // 	      echeck_inst =
 // 		spot::emptiness_check_instantiator::construct( algo, &err);
 // 	      if (!echeck_inst)
 // 		{
 // 		  exit(2);
 // 		}

 // 	      // Create a specifier
 // 	      spot::formula_emptiness_specifier *es  = 0;
 // 	      es = new spot::formula_emptiness_specifier (product, a);

 // 	      // Instanciate the emptiness check
 // 	      spot::emptiness_check* ec  =  echeck_inst->instantiate(product, es);

 // 	      tm_ec.start("checking");
 // 	      spot::emptiness_check_result* res = ec->check();
 // 	      tm_ec.stop("checking");

 // 	      //
 // 	      // Now output results
 // 	      //
 // 	      {
 // 		const spot::ec_statistics* ecs =
 // 		  dynamic_cast<const spot::ec_statistics*>(ec);

 // 		print_results(algo,(res ? "VIOLATED":"VERIFIED"), "BA",
 // 			      tm_ec.timer("checking").utime() +
 // 			      tm_ec.timer("checking").stime(),
 // 			      ecs->states(), ecs->transitions(),
 // 			      ecs->max_depth(),
 // 			      input);
 // 	      }

 // 	      i++;
 // 	      delete echeck_inst;
 // 	      delete es;
 // 	    }
 // 	}

 //    decompfinal:
 // refix decomposition
 // if (opt_dT)
 // 	a = term;
 // if (opt_dW)
 // 	a = weak;
 // if (opt_dS)
 // 	a = strong;
