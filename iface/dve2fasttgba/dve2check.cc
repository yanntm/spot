// Copyright (C) 2011, 2012 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE)
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

#include "dve2.hh"

#include <iostream>
#include <string.h>

// This part is for TGBA
#include "ltlast/allnodes.hh"
#include "tgba/tgba.hh"
#include "tgba/tgbaproduct.hh"
#include "ltlparse/public.hh"
#include "tgbaalgos/ltl2tgba_fm.hh"
#include "tgbaalgos/postproc.hh"
#include "tgbaalgos/emptiness.hh"
#include "kripke/kripkeexplicit.hh"
#include "../dve2/dve2.hh"
#include "tgbaalgos/stats.hh"
#include "tgba/tgbaproduct.hh"


// This part is for FASTTGBA
#include "fasttgbaalgos/tgba2fasttgba.hh"
#include "fasttgbaalgos/dotty_dfs.hh"
#include "fasttgbaalgos/stats_dfs.hh"
#include "fasttgba/fasttgba_product.hh"

#include "fasttgbaalgos/ec/cou99.hh"
#include "fasttgbaalgos/ec/cou99_uf.hh"
#include "fasttgbaalgos/ec/cou99strength.hh"


#include "misc/timer.hh"

static void
syntax(char* prog)
{
  // Display the supplied name unless it appears to be a libtool wrapper.
  char* slash = strrchr(prog, '/');
  if (slash && (strncmp(slash + 1, "lt-", 3) == 0))
    prog = slash + 4;

  std::cerr << "usage: " << prog << " formula file algo" << std::endl;
  exit(1);
}

int
main(int argc, char **argv)
{
  //  The environement for LTL
  spot::ltl::environment& env(spot::ltl::default_environment::instance());

  // Option for the simplifier
  spot::ltl::ltl_simplifier_options redopt(false, false, false, false, false);

  //  The dictionary
  spot::bdd_dict* dict = new spot::bdd_dict();

  // The automaton of the  formula
  const spot::tgba* af1 = 0;

  // Should whether the formula be reduced
  bool simpltl = false;

  // Check arguments
  if (argc != 3)
    syntax(argv[0]);

  bool opt_couuf = true;
  bool opt_cou99 = true;

  // if (strcmp("-cou99", argv[3]))
  //   opt_cou99 = true;
  // else if (strcmp("-cou99_uf", argv[3]))
  //   opt_couuf = true;

  // The formula string
  std::string input =  argv[1];

  // The file that must be load
  std::string file =  argv[2];

  // The formula in spot
  const spot::ltl::formula* f1 = 0;

  // A timer to compute
  spot::timer_map mtimer;

  // Must compare to TGBA ?
  // This option is relevant if comparing to the classic implem
  // of TGBA
  bool compare_to_tgba = false;

  // Must compare emptiness checks algorithms ?
  // This option is relevant if comparing to the classic implem
  // of TGBA
  bool compare_to_emptchk = false;

  //
  // Building the formula from the input
  //
  spot::ltl::parse_error_list pel;
  f1 = spot::ltl::parse(input, pel, env, false);

  //
  // Check wether problems haven been detected during parsing.
  //
  int exit_code = spot::ltl::format_parse_errors(std::cerr, argv[1], pel);
  if (exit_code)
    goto safe_exit;

  if (f1)
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


      std::cout << "Checking the property : "
      		<< spot::ltl::to_string(f1)
      		<< std::endl << std::endl;

      //
      // Building the TGBA of the formula
      //
      af1 = spot::ltl_to_tgba_fm(f1, dict);
      assert (af1);
      spot::postprocessor *pp1 = new spot::postprocessor();
      af1 = pp1->run(af1, f1);
      delete pp1;

      // -----------------------------------------------------
      // Start using fasttgba
      // -----------------------------------------------------

      if (compare_to_tgba)
      	{
	  std::ostringstream result;
	  spot::timer t;

	  // Decclare the dictionnary of atomic propositions that will be
	  // used all along processing
	  spot::ap_dict* aps = new spot::ap_dict();
	  spot::acc_dict* accs = new spot::acc_dict();

	  const spot::fasttgba* kripke =
	    spot::load_dve2fast(file, *aps, *accs, true);
	  assert(kripke);
	  spot::stats_dfs *stats = new spot::stats_dfs(kripke);
	  std::cout << "Expl. the Kripke (new):" << std::flush;
	  mtimer.start("Expl. Kripke (new)");
	  stats->run();
	  mtimer.stop("Expl. Kripke (new)");
	  std::cout << " ---> done" << std::endl;
	  result << stats->dump();
	  t = mtimer.timer("Expl. Kripke (new)");
	  result << "," << t.utime() << "," << t.stime();
	  delete stats;

	  const spot::fasttgba* ftgba1 =
	    spot::tgba_2_fasttgba(af1, *aps, *accs);
	  std::cout << "Expl. the Property (new):";
	  mtimer.start("Expl. Property (new)");
	  spot::stats_dfs* stats1 = new spot::stats_dfs (ftgba1);
	  stats1->run();
	  mtimer.stop("Expl. Property (new)");
	  std::cout << " ---> done" << std::endl;
	  result << "," << stats1->dump();
	  t = mtimer.timer("Expl. Property (new)");
	  result << "," << t.utime() << "," << t.stime();
	  delete stats1;

	  const spot::fasttgba_kripke_product prod (kripke, ftgba1);
	  std::cout << "Expl. the Product (new):" << std::flush;
	  mtimer.start("Expl. Product (new)");
	  spot::stats_dfs* stats3 = new spot::stats_dfs(&prod);
	  stats3->run();
	  mtimer.stop("Expl. Product (new)");
	  std::cout << " ---> done" << std::endl;
	  result << "," << stats3->dump();
	  t = mtimer.timer("Expl. Product (new)");
	  result << "," << t.utime() << "," << t.stime();
	  delete stats3;



	  spot::ltl::atomic_prop_set ap;
	  atomic_prop_collect(f1, &ap);

	  spot::kripke* model = 0;
	  model = spot::load_dve2(file, dict, &ap,
				  spot::ltl::constant::true_instance(),
				  false, false);
	  std::cout << "Expl. the Kripke (old):" << std::flush;
	  mtimer.start("Expl. Kripke (old)");
	  spot::tgba_sub_statistics tgba_stats = sub_stats_reachable(model);
	  mtimer.stop("Expl. Kripke (old)");
	  std::cout << " ---> done" << std::endl;
	  result  << "," << tgba_stats.states
		  << "," << tgba_stats.transitions;
	  t = mtimer.timer("Expl. Kripke (old)");
	  result << "," << t.utime() << "," << t.stime();

	  std::cout << "Expl. the Property (old):" << std::flush;
	  mtimer.start("Expl. Property (old)");
	  tgba_stats = sub_stats_reachable(af1);
	  mtimer.stop("Expl. Property (old)");
	  std::cout << " ---> done" << std::endl;
	  result  << "," << tgba_stats.states
		  << "," << tgba_stats.transitions;
	  t = mtimer.timer("Expl. Property (old)");
	  result << "," << t.utime() << "," << t.stime();


	  spot::tgba* product = new spot::tgba_product(model, af1);
	  std::cout << "Expl. the Product (old):" << std::flush;
	  mtimer.start("Expl. Product (old)");
	  tgba_stats = sub_stats_reachable(product);
	  mtimer.stop("Expl. Product (old)");
	  std::cout << " ---> done" << std::endl;
	  result  << "," << tgba_stats.states
		  << "," << tgba_stats.transitions;
	  t = mtimer.timer("Expl. Product (old)");
	  result << "," << t.utime() << "," << t.stime();


	  delete product;
	  delete model;


	  mtimer.print (std::cout);


	  std::cout << std::endl;
	  std::cout << "#, nK. st, nK. tr, nK. utime, nK. stime, "
		    << "nF. st, nF. tr, nF. utime, nF. stime" << ", "
		    << "nK. x nF. st, nK. x nF. tr, nK. x nF. utime,"
		    << " nK. x nF. stime"
		    << ", oK. st, oK. tr, oK. utime, oK. stime, "
		    << "oF. st, oF. tr, oF. utime, oF. stime, "
		    << "oK. x oF. st, oK. x oF. tr, oK. x oF. utime,"
		    << " oK. x oF. stime"
		    << ", formula"
		    << std::endl;
	  std::cout << "STATS," <<  result.str()
		    << "," << spot::ltl::to_string(f1)
		    << std::endl;


	  delete ftgba1;
	  delete kripke;
	  delete aps;
	  delete accs;
      	}



      {
	    std::ostringstream result;

	    // Decclare the dictionnary of atomic propositions that will be
	    // used all along processing
	    spot::ap_dict* aps = new spot::ap_dict();
	    spot::acc_dict* accs = new spot::acc_dict();

	    const spot::fasttgba* kripke =
	      spot::load_dve2fast(file, *aps, *accs, true);

	    const spot::fasttgba* ftgba1 =
	      spot::tgba_2_fasttgba(af1, *aps, *accs);

	    const spot::fasttgba_kripke_product *prod =
	      new spot::fasttgba_kripke_product (kripke, ftgba1);

	    bool res_cou99uf = true;
	    if (opt_couuf)
	      {
		std::ostringstream result;
		result << "#cou99_uf,";

	  	spot::cou99_uf* checker = new spot::cou99_uf(prod);
	  	mtimer.start("Checking cou99_uf");
	  	if (checker->check())
	  	  {
		    res_cou99uf = false;
		    result << "VIOLATED,";
	  	  }
	  	else
		  {
		    result << "VERIFIED,";
		  }
	  	mtimer.stop("Checking cou99_uf");
	  	delete checker;

		spot::timer t = mtimer.timer("Checking cou99_uf");
		result << t.utime() + t.stime();
		result << "," << input;
		std::cout << result.str() << std::endl;
	      }

	    bool res_cou99 = true;
	    if (opt_cou99)
	      {
		std::ostringstream result;
		result << "#cou99," ;

	  	spot::cou99* checker = new spot::cou99(prod);
	  	mtimer.start("Checking cou99");
	  	if (checker->check())
	  	  {
		    res_cou99 = false;
		    result << "VIOLATED,";
	  	  }
	  	else
		  {
		    result << "VERIFIED,";
		  }
	  	mtimer.stop("Checking cou99");
	  	delete checker;

		spot::timer t = mtimer.timer("Checking cou99");
		result << t.utime() + t.stime();
		result << "," << input;
		std::cout << result.str() << std::endl;


		assert (res_cou99uf == res_cou99);
	      }
	    delete prod;
	    delete ftgba1;
	    delete kripke;
	    delete accs;
	    delete aps;


	    mtimer.print(std::cout);
	  }
















      if (compare_to_emptchk)
	{
	  int res_ = 0;
	  std::ostringstream result;
	  //spot::timer t;

	  // Decclare the dictionnary of atomic propositions that will be
	  // used all along processing
	  spot::ap_dict* aps = new spot::ap_dict();
	  spot::acc_dict* accs = new spot::acc_dict();

	  const spot::fasttgba* kripke =
	    spot::load_dve2fast(file, *aps, *accs, true);

	  const spot::fasttgba* ftgba1 =
	    spot::tgba_2_fasttgba(af1, *aps, *accs);

	  const spot::fasttgba_kripke_product *prod =
	    new spot::fasttgba_kripke_product (kripke, ftgba1);

	  spot::cou99 *checker = new spot::cou99(prod);
	  mtimer.start("Checking cou99 (new)");
    	  if (checker->check())
    	    {
	      mtimer.stop("Checking cou99 (new)");
	      res_ = 1;
    	      std::cout << "A counterexample has been found" << std::endl;
    	    }
    	  else
	    {
	      mtimer.stop("Checking cou99 (new)");
	      std::cout << "No counterexample has been found" << std::endl;
	    }

	  delete checker;


	  // ----------------------

	  spot::ltl::atomic_prop_set ap;
	  atomic_prop_collect(f1, &ap);

	  spot::kripke* model = 0;
	  model = spot::load_dve2(file, dict, &ap,
				  spot::ltl::constant::true_instance(),
				  false, false);

	  spot::tgba* product = new spot::tgba_product(model, af1);

	  // const char* err;
	  // spot::emptiness_check_instantiator* echeck_inst = 0;
	  // echeck_inst =
	  //   spot::emptiness_check_instantiator::construct("Cou99", &err);

	  // spot::emptiness_check* ec = echeck_inst->instantiate(product);
	  // assert(ec);
	  // mtimer.start("Checking cou99 (old)");

	  // spot::emptiness_check_result* ecres = ec->check();
	  // mtimer.stop("Checking cou99 (old)");

	  // // Check if the original algorithm is agree to ref
	  // if ((ecres && !res_) || (!ecres && res_))
	  //   {
	  //     std::cerr << "ERROR: "
	  //      << spot::ltl::to_string(f1) << std::endl;
	  //     assert(false);
	  //   }
	  // else
	  //   {
	  //     // Yes ! We can test other
	  //     std::cout << "checking Other " << std::endl;
	      // spot::cou99strength *chk = new spot::cou99strength(prod);
	      // mtimer.start("Checking cou99strength");
	      // if (chk->check())
	      // 	{
	      // 	  assert(res_);
	      // 	}
	      // else
	      // 	assert(!res_);
	      // mtimer.stop("Checking cou99strength");
	      // delete chk;

	      spot::cou99_uf *chk2 = new spot::cou99_uf(prod);
	      mtimer.start("Checking cou99_uf");
	      if (chk2->check())
		{
		  mtimer.stop("Checking cou99_uf");
		  assert(res_);
		}
	      else
		{
		  mtimer.stop("Checking cou99_uf");
		  assert(!res_);
		}
	      delete chk2;



	    //   std::cout << "End checking Other " << std::endl;

	    // }



	  // ---------------------------------------------------------
	  // This is emptiness check algorithms
	  // ---------------------------------------------------------



	  // delete ecres;
	  // delete echeck_inst;
	  // delete ec;
	  delete product;
	  delete model;



	  // ----------------------


	  mtimer.print(std::cout);



	  delete ftgba1;
	  delete kripke;
	  delete prod;
	  delete aps;
	  delete accs;
	}


    }

 safe_exit:
  // Clean up
  f1->destroy();
  delete af1;
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
