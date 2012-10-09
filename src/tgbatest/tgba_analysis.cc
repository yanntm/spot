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
#include "tgbaalgos/scc.hh"
#include "tgbaalgos/postproc.hh"
#include "tgbaalgos/accpostproc.hh"

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

	    << "Miscalenous :" << std::endl
	    << std::endl
	    << "  -df   display formula"
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
	    << std::endl

	    << "Automaton reduction:" << std::endl
	    << "  -m  minimise the automaton"
	    << std::endl
	    << "  -dmr  decompose minimise and recompose the automaton"
	    << std::endl

	    << "Reduction effects:" << std::endl
    	    << "  -csma  compare the strong part of the automaton "
	    << "   and its minimized version"
	    << std::endl
    	    << "  -cwma  compare the weak part of the automaton "
	    << "   and its minimized version"
	    << std::endl
    	    << "  -ctma  compare the weak part of the automaton "
	    << "   and its minimized version"
	    << std::endl

	    << "Misc:"
	    << std::endl
	    << "-opt_decomp_eval   Evaluate the effect of the decomposition"
	    << std::endl

	    << "Transformation:"
	    << std::endl
	    << "-opt_swt_acc   Tag all SCC with an acceptance set"
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
  //bool scc_filter_all = true;

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
  bool opt_df  = false;

  // automaton reduction
  bool opt_m   = false;
  bool opt_dmr = false;

  // Comparison of reductions
  bool opt_ctma = false;
  bool opt_cwma = false;
  bool opt_csma = false;

  // For the paper
  bool opt_decomp_eval = false;

  // If we want to tag all accepting SCC
  bool opt_swt_acc = false;

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
      else if (!strcmp(argv[formula_index], "-df"))
	{
	  opt_df = true;
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
      else if (!strcmp(argv[formula_index], "-ctma"))
	{
	  opt_ctma = true;
	}
      else if (!strcmp(argv[formula_index], "-cwma"))
	{
	  opt_cwma = true;
	}
      else if (!strcmp(argv[formula_index], "-csma"))
	{
	  opt_csma = true;
	}
      else if (!strcmp(argv[formula_index], "-dha"))
	{
	  opt_ha = true;
	}
      else if (!strcmp(argv[formula_index], "-m"))
	{
	  opt_m = true;
	}
       else if (!strcmp(argv[formula_index], "-dmr"))
	{
	  opt_dmr = true;
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
      else if (!strcmp(argv[formula_index], "-opt_decomp_eval"))
	{
	  opt_decomp_eval = true;
	}
      else if (!strcmp(argv[formula_index], "-opt_swt_acc"))
	{
	  opt_swt_acc = true;
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

  // The declarative environnment for S/W/T
  spot::ltl::declarative_environment* envacc  = 0;

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

      if (opt_df)
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

      //
      // Display SCC with acceptance set for S/W/T
      //
      if (opt_swt_acc)
	{
	  envacc = spot::create_env_acc();
	  const spot::tgba* tmp = add_fake_acceptance_condition (a, envacc);
	  delete a;
	  a = tmp;
	  if (opt_dfa)
	    {
	      spot::dotty_reachable(std::cout, a);
	    }
	  goto cleanup;
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
	  spot::scc_decompose sd (a, opt_m);
	  const spot::tgba* strong = sd.strong_automaton();

	  if (strong)
	    spot::dotty_reachable(std::cout, strong);
	  else
	    std::cerr << "No strong automaton associated"
		      << std::endl;
	}
      if (opt_dwa)
	{
	  spot::scc_decompose sd (a, opt_m);
	  const spot::tgba* weak = sd.weak_automaton();

	  if (weak)
	    spot::dotty_reachable(std::cout, weak);
	  else
	    std::cerr << "No weak automaton associated"
		      << std::endl;
	}
      if (opt_dta)
	{
	  spot::scc_decompose sd (a, opt_m);
	  const spot::tgba* term = sd.terminal_automaton();

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
      if (opt_ctma)
	{
	  spot::scc_decompose sd (a);
	  const spot::tgba* term = sd.terminal_automaton();
	  if (!term)
	    std::cerr << "No terminal automaton associated"
		      << std::endl;
	  else
	    {
	      // Stats for term
	      spot::tgba_statistics a_size =
	  	spot::stats_reachable(term);

	      // Stat for minimised
	      spot::scc_decompose sd1 (a, true);
	      const spot::tgba* minimized = sd1.terminal_automaton();
	      spot::tgba_statistics m_size =
	       	spot::stats_reachable(minimized);

	      if ((a_size.transitions != m_size.transitions) ||
		  (a_size.states != m_size.states) ||
		  (minimized->number_of_acceptance_conditions() !=
		   term->number_of_acceptance_conditions()))
		{

		  std::cout << "Terminal:Original("
			    << a_size.states << ", "
			    << a_size.transitions << ", "
			    << term->number_of_acceptance_conditions()
			    <<")#";
		  std::cout << "Minimized("
			    << m_size.states << ", "
			    << m_size.transitions << ", "
			    << minimized->number_of_acceptance_conditions()
			    <<")" << std::endl;
		}
	      else
		{
		  std::cout << "Terminal:No difference" << std::endl;
		}
	    }
	}
      if (opt_cwma)
	{
	  spot::scc_decompose sd (a);
	  const spot::tgba* weak = sd.weak_automaton();
	  if (!weak)
	    std::cerr << "No weak automaton associated"
		      << std::endl;
	  else
	    {
	      // Stats for weak
	      spot::tgba_statistics a_size =
	  	spot::stats_reachable(weak);

	      // Stat for minimised
	      spot::scc_decompose sd1 (a, true);
	      const spot::tgba* minimized = sd1.weak_automaton();
	      spot::tgba_statistics m_size =
	       	spot::stats_reachable(minimized);

	      if ((a_size.transitions != m_size.transitions) ||
		  (a_size.states != m_size.states) ||
		  (minimized->number_of_acceptance_conditions() !=
		   weak->number_of_acceptance_conditions()))
		{

		  std::cout << "Weak:Original("
			    << a_size.states << ", "
			    << a_size.transitions << ", "
			    << weak->number_of_acceptance_conditions()
			    <<")#";
		  std::cout << "Minimized("
			    << m_size.states << ", "
			    << m_size.transitions << ", "
			    << minimized->number_of_acceptance_conditions()
			    <<")" << std::endl;
		}
	      else
		{
		  std::cout << "Weak:No difference" << std::endl;
		}
	    }
	}
      if (opt_csma)
	{
	  spot::scc_decompose sd (a);
	  const spot::tgba* strong = sd.strong_automaton();
	  if (!strong)
	    std::cerr << "No terminal automaton associated"
		      << std::endl;
	  else
	    {
	      // Stats for term
	      spot::tgba_statistics a_size =
	  	spot::stats_reachable(strong);

	      // Stat for minimised
	      spot::scc_decompose sd1 (a, true);
	      const spot::tgba* minimized = sd1.strong_automaton();
	      spot::tgba_statistics m_size =
	       	spot::stats_reachable(minimized);

	      if ((a_size.transitions != m_size.transitions) ||
		  (a_size.states != m_size.states) ||
		  (minimized->number_of_acceptance_conditions() !=
		   strong->number_of_acceptance_conditions()))
		{

		  std::cout << "Strong:Original("
			    << a_size.states << ", "
			    << a_size.transitions << ", "
			    << strong->number_of_acceptance_conditions()
			    <<")#";
		  std::cout << "Minimized("
			    << m_size.states << ", "
			    << m_size.transitions << ", "
			    << minimized->number_of_acceptance_conditions()
			    <<")" << std::endl;
		}
	      else
		{
		  std::cout << "Strong:No difference" << std::endl;
		}
	    }
	}
      if (opt_dmr)
	{
	  spot::scc_decompose sd (a, true);
	  sd.recompose();
	}
      if (opt_decomp_eval)
	{
	  int orig_states = 0, orig_trans =0, orig_acc = 0;
	  int term_states = 0, term_trans =0, term_acc = 0;
	  int weak_states = 0, weak_trans =0, weak_acc = 0;
	  int strong_states = 0, strong_trans =0, strong_acc = 0;
	  int mterm_states = 0, mterm_trans =0, mterm_acc = 0;
	  int mweak_states = 0, mweak_trans =0, mweak_acc = 0;
	  int mstrong_states = 0, mstrong_trans =0, mstrong_acc = 0;

	  // Compute stats for original
	  spot::tgba_statistics orig_stat =
	    spot::stats_reachable(a);
	  orig_states = orig_stat.states;
	  orig_trans  = orig_stat.transitions;
	  orig_acc    = a->number_of_acceptance_conditions();

	  {
	    // Compute stats for terminal
	    spot::scc_decompose sd_term (a);
	    const spot::tgba* term = sd_term.terminal_automaton();
	    if (!term)
	      {
		term_states = 0;
		term_trans = 0;
		term_acc = 0;
	      }
	    else
	      {
		spot::scc_map* x = new spot::scc_map(term);
		x->build_map();
		const spot::state* s = term->get_init_state();
		//spot::strength str = x->typeof_subautomaton(x->scc_of_state(s));
		//assert(str == spot::TerminalSubaut);
		s->destroy();
		delete x;
		spot::tgba_statistics term_stat =
		  spot::stats_reachable(term);
		term_states = term_stat.states;
		term_trans  = term_stat.transitions;
		term_acc    = term->number_of_acceptance_conditions();
	      }
	  }

	  {
	    // Compute stats for mterminal
	    spot::scc_decompose sd_mterm (a, true);
	    const spot::tgba* mterm = sd_mterm.terminal_automaton();
	    if (!mterm)
	      {
		mterm_states = 0;
		mterm_trans = 0;
		mterm_acc = 0;
	      }
	    else
	      {
		spot::scc_map* x = new spot::scc_map(mterm);
		x->build_map();
		const spot::state* s = mterm->get_init_state();
		//spot::strength str = x->typeof_subautomaton(x->scc_of_state(s));
		//assert(str == spot::TerminalSubaut);
		s->destroy();
		delete x;
		spot::tgba_statistics mterm_stat =
		  spot::stats_reachable(mterm);
		mterm_states = mterm_stat.states;
		mterm_trans  = mterm_stat.transitions;
		mterm_acc    = mterm->number_of_acceptance_conditions();
	      }
	  }

	  {
	    // Compute stats for weak
	    spot::scc_decompose sd_weak (a);
	    const spot::tgba* weak = sd_weak.weak_automaton();
	    if (!weak)
	      {
		weak_states = 0;
		weak_trans = 0;
		weak_acc = 0;
	      }
	    else
	      {
		spot::scc_map* x = new spot::scc_map(weak);
		x->build_map();
		const spot::state* s = weak->get_init_state();
		//spot::strength str = x->typeof_subautomaton(x->scc_of_state(s));
		//assert(str == spot::WeakSubaut);
		s->destroy();
		delete x;
		spot::tgba_statistics weak_stat =
		  spot::stats_reachable(weak);
		weak_states = weak_stat.states;
		weak_trans  = weak_stat.transitions;
		weak_acc    = weak->number_of_acceptance_conditions();
	      }
	  }

	  {
	    // Compute stats for mweak
	    spot::scc_decompose sd_mweak (a, true);
	    const spot::tgba* mweak = sd_mweak.weak_automaton();
	    if (!mweak)
	      {
		mweak_states = 0;
		mweak_trans = 0;
		mweak_acc = 0;
	      }
	    else
	      {
		spot::scc_map* x = new spot::scc_map(mweak);
		x->build_map();
		const spot::state* s = mweak->get_init_state();
		//spot::strength str = x->typeof_subautomaton(x->scc_of_state(s));
		//assert(str == spot::WeakSubaut);
		s->destroy();
		delete x;
		spot::tgba_statistics mweak_stat =
		  spot::stats_reachable(mweak);
		mweak_states = mweak_stat.states;
		mweak_trans  = mweak_stat.transitions;
		mweak_acc    = mweak->number_of_acceptance_conditions();
	      }
	  }

	  {
	    // Compute stats for strong
	    spot::scc_decompose sd_strong (a);
	    const spot::tgba* strong = sd_strong.strong_automaton();
	    if (!strong)
	      {
		strong_states = 0;
		strong_trans = 0;
		strong_acc = 0;
	      }
	    else
	      {
		spot::scc_map* x = new spot::scc_map(strong);
		x->build_map();
		const spot::state* s = strong->get_init_state();
		//spot::strength str = x->typeof_subautomaton(x->scc_of_state(s));
		//assert(str == spot::StrongSubaut);
		s->destroy();
		delete x;
		spot::tgba_statistics strong_stat =
		  spot::stats_reachable(strong);
		strong_states = strong_stat.states;
		strong_trans  = strong_stat.transitions;
		strong_acc    = strong->number_of_acceptance_conditions();
	      }
	  }

	  {
	    // Compute stats for mstrong
	    spot::scc_decompose sd_mstrong (a);
	    const spot::tgba* mstrong = sd_mstrong.strong_automaton();
	    if (!mstrong)
	      {
		mstrong_states = 0;
		mstrong_trans = 0;
		mstrong_acc = 0;
	      }
	    else
	      {
		spot::scc_map* x = new spot::scc_map(mstrong);
		x->build_map();
		const spot::state* s = mstrong->get_init_state();
		//spot::strength str = x->typeof_subautomaton(x->scc_of_state(s));
		//assert(str == spot::StrongSubaut);
		s->destroy();
		delete x;
		spot::tgba_statistics mstrong_stat =
		  spot::stats_reachable(mstrong);
		mstrong_states = mstrong_stat.states;
		mstrong_trans  = mstrong_stat.transitions;
		mstrong_acc    = mstrong->number_of_acceptance_conditions();
	      }
	  }


	  std::cout << "#State Orig,"
		    << "Trans Orig,"
		    << "Acc Orig,"
		    << "T States,"
		    << "T Trans,"
		    << "T Acc,"
		    << "W States,"
		    << "W Trans,"
		    << "W Acc,"
		    << "S States,"
		    << "S Trans,"
		    << "S Acc,"
		    << "T (min)States,"
		    << "T (min)Trans,"
		    << "T (min)Acc,"
		    << "W (min)States,"
		    << "W (min)Trans,"
		    << "W (min)Acc,"
		    << "S (min)States,"
		    << "S (min)Trans,"
		    << "S (min)Acc,"
		    << "formula"
		    << std::endl;
	  std::cout << orig_states << ","
		    << orig_trans  << ","
		    << orig_acc    << ","
		    << term_states << ","
		    << term_trans  << ","
		    << term_acc    << ","
		    << weak_states << ","
		    << weak_trans  << ","
		    << weak_acc    << ","
		    << strong_states << ","
		    << strong_trans  << ","
		    << strong_acc  << ","
		    << mterm_states << ","
		    << mterm_trans  << ","
		    << mterm_acc    << ","
		    << mweak_states << ","
		    << mweak_trans  << ","
		    << mweak_acc    << ","
		    << mstrong_states << ","
		    << mstrong_trans  << ","
		    << mstrong_acc  << ","
		    << input
		    << std::endl;
	}
    }

 cleanup:
  // Clean up
  f->destroy();
  //  if (!a)
  delete a;
  delete dict;
  delete envacc;

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
