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
#include "../iface/dve2/dve2.hh"


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
  bool is_commut = true;
  std::string dve_model_name;

  // Parse arguments
  if (argc == 3)
    {
      if (!strncmp(argv[1], "-B", 2))
	{
	  dve_model_name = argv[1]+2 ;
	}
      else
	assert (false);
    }
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

  int TOVERIFY = 100;		// For each type
  int current_verified = 0;
  int current_violated = 0;
  do
    {
      // If we use stdin
      if (use_stdin && !std::getline(inputcin, input))
	return 0;

      // The formula in spot
      const spot::ltl::formula* f = 0;
      const spot::tgba* a = 0;

      //
      // Building the formula from the input
      //
      f = spot::ltl::parse(input, pel, env, false);
      assert (f);

      //
      // Load the system
      //
      spot::ltl::atomic_prop_set ap;
      atomic_prop_collect(f, &ap);
      const spot::tgba *system = spot::load_dve2(dve_model_name,
						 dict, &ap,
						 spot::ltl::constant::true_instance(),
						 2 /* 0 or 1 or 2 */ , true);

      //
      // Building the TGBA of the formula
      //
      a = spot::ltl_to_tgba_fm(f, dict);
      assert (a);
      spot::postprocessor *pp = new spot::postprocessor();
      pp->set_type(spot::postprocessor::TGBA);
      pp->set_pref(spot::postprocessor::Any);
      pp->set_level(spot::postprocessor::Low);
      a = pp->run (a, f);

      /// Process
      if (is_commut)
	{
	  spot::stats_hierarchy sh (a);
	  sh.stats_automaton();
	  if (!sh.is_commuting_automaton())
	      continue;

	  int SIZE = 2000;
	  int dmt_trans = 0, dmw_trans = 0, dms_trans = 0, orig_trans = 0;
	  int dmt_strans = 0, dmw_strans = 0, dms_strans = 0, orig_strans = 0;
	  int dmt_states = 0, dmw_states = 0, dms_states = 0, orig_states = 0;
	  int dt_trans = 0, dw_trans = 0, ds_trans = 0;
	  int dt_strans = 0, dw_strans = 0, ds_strans = 0;
	  int dt_states = 0, dw_states = 0, ds_states = 0;

	  // Here we are in a favorable case
	  spot::scc_decompose sdm (a, true);
	  const spot::tgba* mstrong = sdm.strong_automaton();
	  if (mstrong)
	    {
	      const spot::tgba *p_mstrong = new spot::tgba_product(system, mstrong);
	      spot::tgba_statistics s_mstrong = stats_reachable(p_mstrong);
	      spot::tgba_statistics ss_mstrong = stats_reachable(p_mstrong);
	      dms_trans = s_mstrong.transitions;
	      dms_states = s_mstrong.states;
	      dms_strans = ss_mstrong.transitions;
	      delete p_mstrong;
	    }
	  const spot::tgba* mweak = sdm.weak_automaton();
	  if (mweak)
	    {
	      const spot::tgba *p_mweak = new spot::tgba_product(system, mweak);
	      spot::tgba_statistics s_mweak = stats_reachable(p_mweak);
	      spot::tgba_statistics ss_mweak = stats_reachable(p_mweak);
	      dmw_trans = s_mweak.transitions;
	      dmw_states = s_mweak.states;
	      dmw_strans = ss_mweak.transitions;
	      delete p_mweak;
	    }
	  const spot::tgba* mterminal = sdm.terminal_automaton();
	  if (mterminal)
	    {
	      const spot::tgba *p_mterminal = new spot::tgba_product(system, mterminal);
	      spot::tgba_statistics s_mterminal = stats_reachable(p_mterminal);
	      spot::tgba_statistics ss_mterminal = stats_reachable(p_mterminal);
	      dmt_trans = s_mterminal.transitions;
	      dmt_states = s_mterminal.states;
	      dmt_strans = ss_mterminal.transitions;
	      delete p_mterminal;
	    }

	  //
	  // It's a formula we want to keep!
	  //
	  if (dms_states >= SIZE || dmw_states >= SIZE || dmt_states >= SIZE)
	    {
	      spot::scc_decompose sd (a, false);
	      const spot::tgba* strong = sd.strong_automaton();
	      if (strong)
		{
		  const spot::tgba *p_strong = new spot::tgba_product(system, strong);
		  spot::tgba_statistics s_strong = stats_reachable(p_strong);
		  spot::tgba_statistics ss_strong = stats_reachable(p_strong);
		  ds_trans = s_strong.transitions;
		  ds_states = s_strong.states;
		  ds_strans = ss_strong.transitions;
		  delete p_strong;
		}
	      const spot::tgba* weak = sd.weak_automaton();
	      if (weak)
		{
		  const spot::tgba *p_weak = new spot::tgba_product(system, weak);
		  spot::tgba_statistics s_weak = stats_reachable(p_weak);
		  spot::tgba_statistics ss_weak = stats_reachable(p_weak);
		  dw_trans = s_weak.transitions;
		  dw_states = s_weak.states;
		  dw_strans = ss_weak.transitions;
		  delete p_weak;
		}
	      const spot::tgba* terminal = sd.terminal_automaton();
	      if (terminal)
		{
		  const spot::tgba *p_terminal =
		    new spot::tgba_product(system, terminal);
		  spot::tgba_statistics s_terminal = stats_reachable(p_terminal);
		  spot::tgba_statistics ss_terminal = stats_reachable(p_terminal);
		  dt_trans = s_terminal.transitions;
		  dt_states = s_terminal.states;
		  dt_strans = ss_terminal.transitions;
		  delete p_terminal;
		}

	      // And we perform the original product and verification
	      const spot::tgba *product =
		new spot::tgba_product(system, a);
	      spot::tgba_statistics stats  = stats_reachable(product);
	      spot::tgba_statistics sstats = stats_reachable(product);
	      orig_trans = stats.transitions;
	      orig_states = stats.states;
	      orig_strans = sstats.transitions;

	      // Emptiness check it !!!!!!!!
	      const char* err;
	      spot::emptiness_check_instantiator* echeck_inst =
		spot::emptiness_check_instantiator::construct( "Cou99", &err);
	      if (!echeck_inst)
		{
		  exit(2);
		}
	      // Instanciate the emptiness check
	      spot::emptiness_check* ec  =  echeck_inst->instantiate(product);
	      spot::emptiness_check_result* res = ec->check();
	      std::string verified =  res ? "VIOLATED": "VERIFIED";
	      delete product;

	      if (!res)
		{
		  if (current_violated >= TOVERIFY)
		    continue;
		  ++current_violated;
		}
	      else
		{
		  if (current_verified >= TOVERIFY)
		    continue;
		  ++current_verified;
		}


	      std::cout << dmt_trans    << ","
			<< dmt_strans   << ","
			<< dmt_states   << ","

			<< dmw_trans    << ","
			<< dmw_strans   << ","
			<< dmw_states   << ","

			<< dms_trans    << ","
			<< dms_strans   << ","
			<< dms_states   << ","

			<< dt_trans     << ","
			<< dt_strans    << ","
			<< dt_states    << ","

			<< dw_trans     << ","
			<< dw_strans    << ","
			<< dw_states    << ","

			<< ds_trans     << ","
			<< ds_strans    << ","
			<< ds_states    << ","

			<< orig_trans     << ","
			<< orig_strans    << ","
			<< orig_states    << ","
			<< a->number_of_acceptance_conditions() << ","

			<< verified      << ','

			<< input        << std::endl;
	    }
	}

      // Clean up
      f->destroy();
      delete a;
      a = 0;
      delete system;
      system = 0;
    } while (use_stdin && input != "");


  // Clean Up dictionnary
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
