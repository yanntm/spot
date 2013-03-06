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
#include "tgbaalgos/postproc.hh"
#include "tgbaalgos/cycles.hh"
#include "tgbaalgos/isweakscc.hh"


void
syntax(char* prog)
{
  // Display the supplied name unless it appears to be a libtool wrapper.
  char* slash = strrchr(prog, '/');
  if (slash && (strncmp(slash + 1, "lt-", 3) == 0))
    prog = slash + 4;

  std::cerr << "Usage: "<< prog << "formula"
	    << std::endl;

  exit(2);
}

int main(int argc, char **argv)
{
  assert(argc >= 2);

  // Which is the chosen approach ?
  bool exact = false; 		// WARNING Should decoment something
  bool structural = false;//true;
  bool syntaxic = false;//true;
  bool original = false;

  // Parse arguments
  if (argc == 3)
    {
      std::string arg = argv[1];
      if (arg == "--exact")
	exact = true;
      else if (arg == "--structural")
	structural = true;
      else if (arg == "--syntaxic")
	syntaxic = true;
      else if (arg == "--original")
	original = true;
      else
	assert (false);
    }

  assert (structural ||syntaxic || exact || original);

  spot::ltl::environment& env(spot::ltl::default_environment::instance());
  spot::ltl::parse_error_list pel;
  //  The dictionnary
  spot::bdd_dict* dict = new spot::bdd_dict();

  // The formula string
  bool use_stdin = false;
  std::string input =  argv[argc -1];
  std::istream &inputcin = std::cin;
  if (input == "-")
      use_stdin = true;

  do
    {
      if (use_stdin && !std::getline(inputcin, input))
	return 0;

      const spot::ltl::formula* f = 0;

      //
      // Building the formula from the input
      //
      f = spot::ltl::parse(input, pel, env, false);
      const spot::tgba *a = spot::ltl_to_tgba_fm(f, dict);
      assert (a);

      // Process the formula
      spot::postprocessor *pp = new spot::postprocessor();
      pp->set_type(spot::postprocessor::TGBA);
      pp->set_pref(spot::postprocessor::Any);
      pp->set_level(spot::postprocessor::Low);
      a = pp->run (a, f);

      // create a timer
      spot::timer_map tm;
      int weaks = 0, non_accepting = 0, terminals = 0, strongs = 0;

      ///
      ///
      ///
      int computation;
      int maxcomputation = 10;
      if (original)
	maxcomputation = 1;
      for (computation = 0; computation != maxcomputation; ++computation)
      {
	// Build a map
	spot::scc_map map(a);
	map.build_map();

	tm.start("Strength computation");
	// Walk accross all SCC
	unsigned max = map.scc_count();
	int number_of_fully_weak = 0;
	int number_of_cycle_weak = 0;
	int number_of_syntactic_weak = 0;
	int number_of_terminal = 0;
	int number_of_strong = 0;
	int number_of_nonaccepting = 0;
	weaks = 0;
	non_accepting = 0;
	terminals = 0;
	strongs = 0;
	int bw = false, bt = false;
	for (unsigned n = 0; n < max; ++n)
	  {
	    if (exact)
	      {
		// Check non accepting SCC
		if (!map.accepting(n))
		  {
		    ++non_accepting;
		    continue;
		  }
		// Check weak
		if (!is_weak_scc(map, n, false))
		  {
		    ++strongs;
		    continue;
		  }
		if (is_complete_scc(a, map, n))
		  {
		    ++terminals;
		  }
		else
		  {
		    ++weaks;
		  }
		continue;
	      }

	    if (structural)
	      {
		// Check non accepting SCC
		if (!map.accepting(n))
		  {
		    ++non_accepting;
		    continue;
		  }
		// Check weak
		if (!is_weak_heuristic(map, n))
		  {
		    ++strongs;
		    continue;
		  }
		if (is_complete_scc(a, map, n))
		  {
		    ++terminals;
		  }
		else
		  {
		    ++weaks;
		  }
		continue;
	      }

	    if (syntaxic)
	      {
		// Check non accepting SCC
		if (!map.accepting(n))
		  {
		    ++non_accepting;
		    continue;
		  }
		if (bt)
		  {
		    ++terminals;
		    continue;
		  }
		if (bw)
		  {
		    if (is_syntactic_terminal_scc(a, map, n))
		      {
			bt = true;
			++terminals;
		      }
		    else
		      ++weaks;
		    continue;
		  }

		// Check weak
		if (!is_syntactic_weak_scc(a, map, n))
		  {
		    ++strongs;
		    continue;
		  }
		if (is_syntactic_terminal_scc(a, map, n))
		  {
		    bt = true;
		    ++terminals;
		  }
		else
		  {
		    ++weaks;
		  }
	      }

	    if (original)
	      {

		// if(is_complete_scc(a, map, n) && is_weak_scc(map, n))
		//   assert(map.terminal_accepting (n));

		// DO not consider terminal SCC
		if (map.terminal_accepting (n))
		  {
		    ++number_of_terminal;
		    continue;
		  }

		// We need to check the acceptance not in term of Pelaned

		// If all transitions use all acceptance conditions, the SCC is weak.
		bool fully_weak = map.accepting(n) &&
		  (map.useful_acc_of(n) ==
		   bdd_support(map.get_aut()->neg_acceptance_conditions()));
		if (fully_weak)
		  ++number_of_fully_weak;

		// if (fully_weak)
		//   assert(map.weak_accepting(n));

		// A SCC can be weak considering  cycles
		bool cycle_weak = map.accepting(n) && is_weak_scc(map, n, false);
		if (cycle_weak)
		  ++number_of_cycle_weak;

		// A SCC can be syntactically weak
		// bool syntactic_weak = map.accepting(n) &&
		//   is_syntactic_weak_scc(a, map, n);
		// if (syntactic_weak)
		++number_of_syntactic_weak;

		// Otherwise
		if (!cycle_weak)
		  ++number_of_strong;
	      }
	  }

	if (original)
	  {
	    std::cout << "#U,SW,FW,CW,T,S,"  << input  << std::endl
		      << number_of_nonaccepting    << ","
		      << number_of_syntactic_weak  << ","
		      << number_of_fully_weak      << ","
		      << number_of_cycle_weak      << ","
		      << number_of_terminal        << ","
		      << number_of_strong          << std::endl;

	    if ((number_of_fully_weak != number_of_cycle_weak))
	      std::cout << "--BAD concording structural:" << input << std::endl;
	    assert(number_of_fully_weak <= number_of_cycle_weak);

	    // if (number_of_cycle_weak != number_of_syntactic_weak)
	    //   std::cout << "--BAD concording syntactic:" << input << std::endl;
	    // assert(number_of_fully_weak >= number_of_syntactic_weak);
	    // assert(number_of_cycle_weak >= number_of_syntactic_weak);
	  }
	tm.stop("Strength computation");
      }
      std::cout << non_accepting << ','
		<< weaks         << ','
		<< terminals     << ','
		<< strongs       << ','
		<< tm.timer("Strength computation").utime() +
	tm.timer("Strength computation").stime() << ","
		<< input
		<< std::endl;


      // Clean up
      delete pp;
      f->destroy();
      delete a;
    } while (use_stdin && input != "");

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





// {
// 	// Build a map
// 	spot::scc_map map(a);
// 	map.build_map();

// 	// Walk accross all SCC
// 	unsigned max = map.scc_count();
// 	int number_of_fully_weak = 0;
// 	int number_of_cycle_weak = 0;
// 	int number_of_syntactic_weak = 0;
// 	int number_of_terminal = 0;
// 	int number_of_strong = 0;
// 	int number_of_nonaccepting = 0;
// 	for (unsigned n = 0; n < max; ++n)
// 	  {
// 	    // Check non accepting SCC
// 	    if (!map.accepting(n))
// 	      {
// 		++number_of_nonaccepting;
// 		continue;
// 	      }

// 	    // if(is_complete_scc(a, map, n) && is_weak_scc(map, n))
// 	    //   assert(map.terminal_accepting (n));


// 	    // DO not consider terminal SCC
// 	    if (map.terminal_accepting (n))
// 	      {
// 		++number_of_terminal;
// 		continue;
// 	      }

// 	    // We need to check the acceptance not in term of Pelaned

// 	    // If all transitions use all acceptance conditions, the SCC is weak.
// 	    bool fully_weak = map.accepting(n) &&
// 	      (map.useful_acc_of(n) ==
// 	       bdd_support(map.get_aut()->neg_acceptance_conditions()));
// 	    if (fully_weak)
// 	      ++number_of_fully_weak;

// 	    // if (fully_weak)
// 	    //   assert(map.weak_accepting(n));

// 	    // A SCC can be weak considering  cycles
// 	    bool cycle_weak = map.accepting(n) && is_weak_scc(map, n);
// 	    if (cycle_weak)
// 	      ++number_of_cycle_weak;

// 	    // A SCC can be syntactically weak
// 	    bool syntactic_weak = map.accepting(n) &&
// 	      is_syntactic_weak_scc(a, map, n);
// 	    if (syntactic_weak)
// 	      ++number_of_syntactic_weak;

// 	    // Otherwise
// 	    if (!cycle_weak)
// 	      ++number_of_strong;
// 	  }

// 	std::cout << "#U,SW,FW,CW,T,S,"  << input  << std::endl
// 		  << number_of_nonaccepting    << ","
// 		  << number_of_syntactic_weak  << ","
// 		  << number_of_fully_weak      << ","
// 		  << number_of_cycle_weak      << ","
// 		  << number_of_terminal        << ","
// 		  << number_of_strong          << std::endl;

// 	if ((number_of_fully_weak != number_of_cycle_weak))
// 	  std::cout << "--BAD concording structural:" << input << std::endl;
// 	assert(number_of_fully_weak <= number_of_cycle_weak);

// 	if (number_of_cycle_weak != number_of_syntactic_weak)
// 	  std::cout << "--BAD concording syntactic:" << input << std::endl;
// 	assert(number_of_fully_weak >= number_of_syntactic_weak);
// 	assert(number_of_cycle_weak >= number_of_syntactic_weak);
//       }
