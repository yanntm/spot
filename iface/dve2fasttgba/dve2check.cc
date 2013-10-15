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
#include "tgbaalgos/dotty.hh"


// This part is for FASTTGBA
#include "fasttgbaalgos/tgba2fasttgba.hh"
#include "fasttgbaalgos/dotty_dfs.hh"
#include "fasttgbaalgos/stats_dfs.hh"
#include "fasttgba/fasttgba_product.hh"

#include "fasttgbaalgos/ec/cou99.hh"
#include "fasttgbaalgos/ec/cou99_scc.hh"
#include "fasttgbaalgos/ec/cou99_uf.hh"
#include "fasttgbaalgos/ec/cou99_uf_swarm.hh"
#include "fasttgbaalgos/ec/cou99_uf_shared.hh"
#include "fasttgbaalgos/ec/cou99strength.hh"
#include "fasttgbaalgos/ec/cou99strength_uf.hh"
#include "fasttgbaalgos/ec/gv04.hh"
#include "fasttgbaalgos/ec/lazycheck.hh"
#include "fasttgbaalgos/ec/stackscheck.hh"
#include "fasttgbaalgos/ec/stats.hh"
#include "fasttgbaalgos/ec/tarjan_scc.hh"
#include "fasttgbaalgos/ec/unioncheck.hh"
#include "fasttgbaalgos/ec/union_scc.hh"
#include "fasttgbaalgos/ec/dijkstracheck.hh"
#include "fasttgbaalgos/ec/dijkstra_scc.hh"
#include "fasttgbaalgos/ec/cou99check.hh"

#include "fasttgbaalgos/ec/opt/opt_tarjan_scc.hh"
#include "fasttgbaalgos/ec/opt/opt_dijkstra_scc.hh"
#include "misc/timer.hh"

#include "fasttgbaalgos/ec/concur/uf.hh"

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
  if (argc != 4)
    syntax(argv[0]);

  // Choose the emptiness check algorithm
  bool opt_couuf = false;
  bool opt_couuf_swarm = false;
  bool opt_couuf_shared = false;
  bool opt_coustrengthuf = false;
  bool opt_cou99 = false;
  bool opt_dijkstra = false;
  bool opt_union = false;
  bool opt_couvreur = false;
  bool opt_stats = false;
  bool opt_tarjan = false;
  bool opt_opttarjan = false;
  bool opt_ecopttarjan = false;
  bool opt_optdijkstra = false;
  bool opt_ecoptdijkstra = false;
  bool opt_gv04 = false;
  bool opt_lc13 = false;
  bool opt_sc13 = false;
  bool opt_uc13 = false;
  bool opt_dc13 = false;
  bool opt_c99 = false;
  bool opt_cmp_cou_couuf = false;
  bool opt_all = false;
  bool opt_wait = false;

  std::string option_uc13 = "";
  std::string option_dc13 = "";
  std::string option_dijkstra = "";
  std::string option_union = "";
  std::string option_stats = "";
  std::string option_tarjan = "";
  std::string option_opttarjan = "";
  std::string option_ecopttarjan = "";
  std::string option_optdijkstra = "";
  std::string option_ecoptdijkstra = "";
  std::string option_couvreur = "";
  std::string option_lc13 = "";
  std::string option_sc13 = "";
  std::string option_gv04 = "";
  std::string option_c99  = "";
  std::string option_wait  = "";

  if (!strcmp("-cou99", argv[3]))
    opt_cou99 = true;
  else if (!strcmp("-cou99_uf", argv[3]))
    opt_couuf = true;
  else if (!strcmp("-cou99strength_uf", argv[3]))
    opt_coustrengthuf = true;
  else if (!strcmp("-cou99_uf_swarm", argv[3]))
    opt_couuf_swarm = true;
  else if (!strcmp("-cou99_uf_shared", argv[3]))
    opt_couuf_shared = true;
  else if (!strncmp("-gv04", argv[3], 5))
    {
      opt_gv04 = true;
      option_gv04 = std::string(argv[3]+5);
      // FIXME : For debug
      // opt_cou99 = true;
    }
  else if (!strncmp("-lc13", argv[3], 5))
    {
      opt_lc13 = true;
      option_lc13 = std::string(argv[3]+5);
      // FIXME : For debug
      // opt_cou99 = true;
    }
  else if (!strncmp("-c99", argv[3], 4))
    {
      opt_c99 = true;
      option_c99 = std::string(argv[3]+4);
      // FIXME : For debug
      // opt_cou99 = true;
    }
  else if (!strncmp("-sc13", argv[3], 5))
    {
      opt_sc13 = true;
      option_sc13 = std::string(argv[3]+5);
      // FIXME : For debug
      // opt_cou99 = true;
    }
  else if (!strncmp("-uc13", argv[3], 5))
    {
      opt_uc13 = true;
      option_uc13 = std::string(argv[3]+5);
      // FIXME : For debug
      // opt_cou99 = true;
    }
  else if (!strncmp("-dc13", argv[3], 5))
    {
      opt_dc13 = true;
      option_dc13 = std::string(argv[3]+5);
      // FIXME : For debug
      // opt_cou99 = true;
    }
  else if (!strncmp("-dijkstra", argv[3], 9))
    {
      opt_dijkstra = true;
      option_dijkstra = std::string(argv[3]+9);
    }
  else if (!strncmp("-union", argv[3], 6))
    {
      opt_union = true;
      option_union = std::string(argv[3]+6);
    }
  else if (!strncmp("-stats", argv[3], 6))
    {
      opt_stats = true;
      option_stats = std::string(argv[3]+6);
    }
  else if (!strncmp("-tarjan", argv[3], 7))
    {
      opt_tarjan = true;
      option_tarjan = std::string(argv[3]+7);
    }
  else if (!strncmp("-opttarjan", argv[3], 10))
    {
      opt_opttarjan = true;
      option_opttarjan = std::string(argv[3]+10);
    }
  else if (!strncmp("-ecopttarjan", argv[3], 12))
    {
      std::cout << argv[3] << std::endl;
      opt_ecopttarjan = true;
      option_ecopttarjan = std::string(argv[3]+12);
    }
  else if (!strncmp("-optdijkstra", argv[3], 12))
    {
      opt_optdijkstra = true;
      option_optdijkstra = std::string(argv[3]+12);
    }
  else if (!strncmp("-ecoptdijkstra", argv[3], 14))
    {
      opt_ecoptdijkstra = true;
      option_ecoptdijkstra = std::string(argv[3]+14);
    }
  else if (!strncmp("-couvreur", argv[3], 9))
    {
      opt_couvreur = true;
      option_couvreur = std::string(argv[3]+9);
    }
  else if (!strcmp("-cmp_cou_uf", argv[3]))
    {
      opt_couuf = true;
      opt_cou99 = true;
      opt_cmp_cou_couuf = true;
    }
  else if (!strncmp("-wait", argv[3], 5))
    {
      option_wait = std::string(argv[3]+5);
      opt_wait = true;
    }
  else if (!strcmp("-all", argv[3]))
    {
      opt_couuf = true;
      opt_couuf_swarm = true;
      opt_coustrengthuf = true;
      opt_cou99 = true;
      opt_all = true;
      opt_couuf_shared = true;
    }
  else
    syntax(argv[0]);

  // The formula string
  std::string input =  argv[1];

  // The file that must be load
  std::string file =  argv[2];

  // The formula in spot
  const spot::ltl::formula* f1 = 0;

  // A timer to compute
  spot::timer_map mtimer;

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


      if (!opt_wait)
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

      // spot::dotty_reachable(std::cout, af1);
      // exit(1);
      // -----------------------------------------------------
      // Start using fasttgba
      // -----------------------------------------------------

      {
	// The string that will contain all results
	std::ostringstream result;

	// Create the instanciator
	spot::instanciator* itor =
	  new spot::dve2product_instanciator(af1, file);

	bool res_cou99uf = true;
	if (opt_couuf)
	  {
	    std::ostringstream result;
	    result << "#cou99_uf,";

	    spot::cou99_uf* checker = new spot::cou99_uf(itor);
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
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << input;
	    std::cout << result.str() << std::endl;
	  }

	bool res_cou99ufswarm = true;
	if (opt_couuf_swarm)
	  {
	    std::ostringstream result;
	    result << "#cou99_uf_swarm,";

	    spot::cou99_uf_swarm* checker = new spot::cou99_uf_swarm (itor, 2);

	    mtimer.start("Checking cou99_uf_swarm");
	    if (checker->check())
	      {
		res_cou99ufswarm = false;
		result << "VIOLATED,";
	      }
	    else
	      {
		result << "VERIFIED,";
	      }
	    mtimer.stop("Checking cou99_uf_swarm");
	    delete checker;

	    spot::timer t = mtimer.timer("Checking cou99_uf_swarm");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << input;
	    std::cout << result.str() << std::endl;
	  }

	bool res_cou99ufshared = true;
	if (opt_couuf_shared)
	  {
	    std::ostringstream result;
	    result << "#cou99_uf_shared,";

	    spot::cou99_uf_shared* checker =
	      new spot::cou99_uf_shared (itor, 2);
	    mtimer.start("Checking cou99_uf_shared");
	    if (checker->check())
	      {
		res_cou99ufshared = false;
		result << "VIOLATED,";
	      }
	    else
	      {
		result << "VERIFIED,";
	      }
	    mtimer.stop("Checking cou99_uf_shared");
	    delete checker;

	    spot::timer t = mtimer.timer("Checking cou99_uf_shared");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << input;
	    std::cout << result.str() << std::endl;
	  }

	bool res_cou99strengthuf = true;
	if (opt_coustrengthuf)
	  {
	    std::ostringstream result;
	    result << "#cou99strength_uf,";

	    spot::cou99strength_uf* checker = new spot::cou99strength_uf(itor);
	    mtimer.start("Checking cou99str_uf");
	    if (checker->check())
	      {
		res_cou99strengthuf = false;
		result << "VIOLATED,";
	      }
	    else
	      {
		result << "VERIFIED,";
	      }
	    mtimer.stop("Checking cou99str_uf");
	    delete checker;

	    spot::timer t = mtimer.timer("Checking cou99str_uf");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << input;
	    std::cout << result.str() << std::endl;
	  }

	bool res_gv04 = true;
	if (opt_gv04)
	  {
	    std::ostringstream result;
	    result << "#gv04" << option_gv04 << ",";

	    spot::gv04* checker = new spot::gv04(itor, option_gv04);
	    mtimer.start("Checking gv04");
	    if (checker->check())
	      {
		res_gv04 = false;
		result << "VIOLATED,";
	      }
	    else
	      {
		result << "VERIFIED,";
	      }
	    mtimer.stop("Checking gv04");

	    spot::timer t = mtimer.timer("Checking gv04");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," <<  checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }

	bool res_lc13 = true;
	if (opt_lc13)
	  {
	    std::ostringstream result;
	    result << "#lc13" << option_lc13 << ",";

	    spot::lazycheck* checker = new spot::lazycheck(itor, option_lc13);
	    mtimer.start("Checking lc13");
	    if (checker->check())
	      {
		res_lc13 = false;
		result << "VIOLATED,";
	      }
	    else
	      {
		result << "VERIFIED,";
	      }
	    mtimer.stop("Checking lc13");


	    spot::timer t = mtimer.timer("Checking lc13");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }

	bool res_c99 = true;
	if (opt_c99)
	  {
	    std::ostringstream result;
	    result << "#c99" << option_c99 << ",";

	    spot::cou99check* checker = new spot::cou99check(itor, option_c99);
	    mtimer.start("Checking c99");
	    if (checker->check())
	      {
		res_c99 = false;
		result << "VIOLATED,";
	      }
	    else
	      {
		result << "VERIFIED,";
	      }
	    mtimer.stop("Checking c99");


	    spot::timer t = mtimer.timer("Checking c99");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }


	bool res_sc13 = true;
	if (opt_sc13)
	  {
	    std::ostringstream result;
	    result << "#sc13" << option_sc13 << ",";

	    spot::stackscheck* checker =
	      new spot::stackscheck(itor, option_sc13);
	    mtimer.start("Checking sc13");
	    if (checker->check())
	      {
		res_sc13 = false;
		result << "VIOLATED,";
	      }
	    else
	      {
		result << "VERIFIED,";
	      }
	    mtimer.stop("Checking sc13");

	    spot::timer t = mtimer.timer("Checking sc13");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }

	bool res_uc13 = true;
	if (opt_uc13)
	  {
	    std::ostringstream result;
	    result << "#uc13" << option_uc13 << ",";

	    spot::unioncheck* checker = new spot::unioncheck(itor, option_uc13);
	    mtimer.start("Checking uc13");
	    if (checker->check())
	      {
		res_uc13 = false;
		result << "VIOLATED,";
	      }
	    else
	      {
		result << "VERIFIED,";
	      }
	    mtimer.stop("Checking uc13");


	    spot::timer t = mtimer.timer("Checking uc13");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }


	bool res_dc13 = true;
	if (opt_dc13)
	  {
	    std::ostringstream result;
	    result << "#dc13" << option_dc13 << ",";

	    spot::dijkstracheck* checker =
	      new spot::dijkstracheck(itor, option_dc13);
	    mtimer.start("Checking dc13");
	    if (checker->check())
	      {
		res_dc13 = false;
		result << "VIOLATED,";
	      }
	    else
	      {
		result << "VERIFIED,";
	      }
	    mtimer.stop("Checking dc13");

	    spot::timer t = mtimer.timer("Checking dc13");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }


	if (opt_dijkstra)
	  {
	    std::ostringstream result;
	    result << "#dijkstra" << option_dijkstra << ",";

	    spot::dijkstra_scc* checker =
	      new spot::dijkstra_scc(itor, option_dijkstra);
	    mtimer.start("Checking dijkstra");
	    checker->check();
	    result << "SCC,";
	    mtimer.stop("Checking dijkstra");

	    spot::timer t = mtimer.timer("Checking dijkstra");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }


	if (opt_union)
	  {
	    std::ostringstream result;
	    result << "#union" << option_union << ",";

	    spot::union_scc* checker =
	      new spot::union_scc(itor, option_union);
	    mtimer.start("Checking union");
	    checker->check();
	    result << "SCC,";
	    mtimer.stop("Checking union");

	    spot::timer t = mtimer.timer("Checking union");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }

	if (opt_tarjan)
	  {
	    std::ostringstream result;
	    result << "#tarjan" << option_tarjan << ",";

	    spot::tarjan_scc* checker =
	      new spot::tarjan_scc(itor, option_tarjan);
	    mtimer.start("Checking tarjan");
	    checker->check();
	    result << "SCC,";
	    mtimer.stop("Checking tarjan");

	    spot::timer t = mtimer.timer("Checking tarjan");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }




	// -------------------------------------------------------------------

	if (opt_opttarjan)
	  {
	    std::ostringstream result;
	    result << "#opttarjan" << option_opttarjan << ",";

	    spot::opt_tarjan_scc* checker =
	      new spot::opt_tarjan_scc(itor, option_opttarjan);
	    mtimer.start("Checking opttarjan");
	    checker->check();
	    mtimer.stop("Checking opttarjan");
	    result << "SCC,";

	    spot::timer t = mtimer.timer("Checking opttarjan");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }

	if (opt_ecopttarjan)
	  {
	    std::ostringstream result;
	    result << "#ecopttarjan" << option_ecopttarjan << ",";

	    spot::opt_tarjan_ec* checker =
	      new spot::opt_tarjan_ec(itor, option_ecopttarjan);
	    mtimer.start("Checking ecopttarjan");
	    int b = checker->check();
	    mtimer.stop("Checking ecopttarjan");
	    result << (b ? "VIOLATED," :"VERIFIED,");

	    spot::timer t = mtimer.timer("Checking ecopttarjan");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }


	if (opt_optdijkstra)
	  {
	    std::ostringstream result;
	    result << "#optdijkstra" << option_optdijkstra << ",";

	    spot::opt_dijkstra_scc* checker =
	      new spot::opt_dijkstra_scc(itor, option_optdijkstra);
	    mtimer.start("Checking optdijkstra");
	    checker->check();
	    mtimer.stop("Checking optdijkstra");
	    result << "SCC,";

	    spot::timer t = mtimer.timer("Checking optdijkstra");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }

	if (opt_ecoptdijkstra)
	  {
	    std::ostringstream result;
	    result << "#ecoptdijkstra" << option_ecoptdijkstra << ",";

	    spot::opt_dijkstra_ec* checker =
	      new spot::opt_dijkstra_ec(itor, option_ecoptdijkstra);
	    mtimer.start("Checking ecoptdijkstra");
	    int b = checker->check();
	    mtimer.stop("Checking ecoptdijkstra");
	    result << (b ? "VIOLATED," :"VERIFIED,");

	    spot::timer t = mtimer.timer("Checking ecoptdijkstra");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }


	// ------------------------------------------------------------------


	if (opt_stats)
	  {
	    std::ostringstream result;
	    result << "#stats" << option_stats << ",";

	    spot::stats* checker =
	      new spot::stats(itor, option_stats);
	    mtimer.start("Checking stats");
	    checker->check();
	    result << "SCC,";
	    mtimer.stop("Checking stats");

	    spot::timer t = mtimer.timer("Checking stats");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }

	if (opt_couvreur)
	  {
	    std::ostringstream result;
	    result << "#couvreur" << option_couvreur << ",";

	    spot::cou99_scc* checker =
	      new spot::cou99_scc(itor, option_couvreur);
	    mtimer.start("Checking couvreur");
	    checker->check();
	    result << "SCC,";
	    mtimer.stop("Checking couvreur");

	    spot::timer t = mtimer.timer("Checking couvreur");
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << checker->extra_info_csv() << ","
		   << input;
	    delete checker;
	    std::cout << result.str() << std::endl;
	  }


	//
	// This option is used to construct a bench with
	// product exploration that takes at least a certain
	// amount of time and does not exeed another
	//
	if (opt_wait)
	  {
	    if (option_wait.compare("") == 0)
	      option_wait = "verified";
	    assert(option_wait.compare("verified") == 0||
		   option_wait.compare("violated") == 0);

	    //
	    // We just want to keep Strong automaton
	    //
	    const spot::dve2product_instance* inststr =
	      (const spot::dve2product_instance*)itor->new_instance();

	    const spot::fasttgbaexplicit* instaut =
	      dynamic_cast<const spot::fasttgbaexplicit*>
	      (inststr->get_formula_automaton());
	    assert(instaut);

	    if (instaut->get_strength() != spot::STRONG_AUT)
	      {
	    	exit(1);
	      }

	    delete inststr;


	    std::ostringstream result;

	    int opt_wait_min = 15000;
	    int opt_wait_max = 900000;
	    // std::chrono::milliseconds opt_wait_max( 2000 )

	    // Wheter the execution was stopped
	    std::atomic<bool> stop_;
	    stop_ = false;

	    std::thread worker  ([&](){
		// std::chrono::milliseconds dura( 2000 );
		// std::this_thread::sleep_for(dura);

		spot::lazycheck* checker =
		  new spot::lazycheck(itor, "");


		mtimer.start("Checking wait");
		if (checker->check())
		  {
		    mtimer.stop("Checking wait");

		    if (!(option_wait.compare("violated") == 0))
		      {
			// std::cout << "V\n";
			exit(1);
		      }
		    stop_ = true;
		    result << file.substr(1 + file.find_last_of("\\/")) << ",";
		    result << "VIOLATED,";
		  }
		else
		  {
		    mtimer.stop("Checking wait");

		    if (!(option_wait.compare("verified") == 0))
		      {
			// std::cout << "T\n";
			exit(1);
		      }
		    stop_ = true;
		    result << file.substr(1 + file.find_last_of("\\/")) << ",";
		    result << "VERIFIED,";
		  }
		spot::timer t = mtimer.timer("Checking wait");

		// here we check wether the time ellapsed is okay
		if (t.walltime()  < opt_wait_min ||
		    t.walltime()  > opt_wait_max)
		  {
		    // std::cout << "ERROR : TIME  "
		    // << t.walltime() << std::endl;
		    exit(1);
		  }

		result << t.walltime() << "," << t.utime()  << "," << t.stime();
		result << "," << checker->extra_info_csv() << ","
		       << input;
		delete checker;
		std::cout << result.str() << std::endl;

		// We can safely exit since the result is outputed
		exit(1);
	      });

	    // wait a little more
	    std::chrono::milliseconds dura(opt_wait_max + 1000);
	    std::this_thread::sleep_for(dura);
	    if (!stop_)
	      {
		// The thread has not finished in time Byebye!
		//std::cout << "Do not Join the thread" << std::endl;
		exit (1);
	      }
	    else
	      {
		// The thread is printing results
		//std::cout << "Join the thread" << std::endl;
		worker.join();
	      }
	  }





	bool res_cou99 = true;
	if (opt_cou99)
	  {
	    std::ostringstream result;
	    result << "#cou99,";

	    spot::cou99* checker = new spot::cou99(itor);
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
	    result << t.walltime() << "," << t.utime()  << "," << t.stime();
	    result << "," << input;
	    std::cout << result.str() << std::endl;

	    if (opt_all)
	      {
		assert (res_cou99uf == res_cou99);
		assert (res_cou99ufswarm == res_cou99);
		assert (res_cou99strengthuf == res_cou99);
		assert (res_cou99ufshared == res_cou99);
	      }

	    if (opt_cmp_cou_couuf)
	      {
		assert (res_cou99uf == res_cou99);
	      }

	    if (opt_gv04)
	      {
		assert (res_gv04 == res_cou99);
	      }
	    if (opt_lc13)
	      {
		assert (res_lc13 == res_cou99);
	      }
	    if (opt_sc13)
	      {
		assert (res_sc13 == res_cou99);
	      }
	    if (opt_uc13)
	      {
		assert (res_uc13 == res_cou99);
	      }
	    if (opt_dc13)
	      {
		assert (res_dc13 == res_cou99);
	      }
	    if (opt_c99)
	      {
		assert (res_c99 == res_cou99);
	      }


	  }

	// mtimer.print(std::cout);

	delete itor;
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
