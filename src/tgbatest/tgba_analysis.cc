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


void
syntax(char* prog)
{
  // Display the supplied name unless it appears to be a libtool wrapper.
  char* slash = strrchr(prog, '/');
  if (slash && (strncmp(slash + 1, "lt-", 3) == 0))
    prog = slash + 4;

  std::cerr << "Usage: "<< prog << "[OPTIONS...] formula"
	    << "Formula simplification (before translation):"
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

            << "Automaton conversion:" << std::endl
	    << std::endl
	    << "  -dfa  display formula automaton"
	    << std::endl
	    << "  -dsa  display the strong part of formula automaton"
	    << std::endl
	    << "  -dwa  display the weak part of formula automaton"
	    << std::endl
	    << "  -dta  display the terminal part of formula automaton"
	    << std::endl
	    << "  -dha  display stats extracted from hierarchical analysis"
	    << std::endl;

  exit(2);
}


int main(int argc, char **argv)
{
  //  The environement for LTL
  spot::ltl::environment& env(spot::ltl::default_environment::instance());
  // Option for the simplifier
  spot::ltl::ltl_simplifier_options redopt(false, false, false, false, false);

  // The reduction for the automaton 
  //int reduc_aut = spot::Reduce_None;
  bool scc_filter_all = true;

  //  The dictionnary
  spot::bdd_dict* dict = new spot::bdd_dict();

  // Timer for computing all steps 
  spot::timer_map tm;

  // The automaton of the formula
  const spot::tgba* a = 0;
  // Display option
  bool opt_dfa = false;
  bool opt_dsa = false;
  bool opt_dwa = false;
  bool opt_dta = false;
  bool opt_ha  = false;

  // Should whether the formula be reduced 
  bool simpltl = false;

  // The index of the formula
  int formula_index = 0;

  for (;;)
    {
      //  Syntax check
      if (argc < formula_index + 2)
	syntax(argv[0]);

      ++formula_index;

      if (!strcmp(argv[formula_index], "-dfa"))
	{
	  opt_dfa = true;
	}
      else if (!strcmp(argv[formula_index], "-dsa"))
	{
	  opt_dsa = true;
	}
      else if (!strcmp(argv[formula_index], "-dwa"))
	{
	  opt_dwa = true;
	}
      else if (!strcmp(argv[formula_index], "-dta"))
	{
	  opt_dta = true;
	}
      else if (!strcmp(argv[formula_index], "-dha"))
	{
	  opt_ha = true;
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
      else
	{
	  if (argc == (formula_index + 1))
	    break;
	  syntax(argv[0]);
	}
    }

  // The formula string 
  std::string input =  argv[formula_index];
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
	    // std::cout << spot::ltl::to_string(f) << std::endl;
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
      // Display options 
      // 
      if (opt_dfa)
	{
	  spot::dotty_reachable(std::cout, a);
	}
      if (opt_dsa)
	{
	  spot::scc_decompose sd (a);
	  spot::tgba* strong = sd.strong_automaton();
	  if (strong)
	    spot::dotty_reachable(std::cout, strong);
	  else
	    std::cerr << "No strong automaton associated"
		      << std::endl;
	}
      if (opt_dwa)
	{
	  spot::scc_decompose sd (a);
	  spot::tgba* weak = sd.weak_automaton();
	  if (weak)
	    spot::dotty_reachable(std::cout, weak);
	  else
	    std::cerr << "No weak automaton associated"
		      << std::endl;
	}
      if (opt_dta)
	{
	  spot::scc_decompose sd (a);
	  spot::tgba* term = sd.terminal_automaton();
	  if (term)
	    spot::dotty_reachable(std::cout, term);
	  else
	    std::cerr << "No terminal automaton associated"
		      << std::endl;
	}
      if (opt_ha)
	{
	  spot::stats_hierarchy sh (a);
	  sh.stats_automaton();
	  std::cout << sh << std::endl;
	}

    }

  // Clean up
  f->destroy();
  if (!a)
    delete dict;
  delete a;

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
