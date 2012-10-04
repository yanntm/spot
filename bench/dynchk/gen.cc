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

  // The formula string
  bool use_stdin = false;
  std::string input =  argv[argc -1];

  //  The dictionnary
  spot::bdd_dict* dict = new spot::bdd_dict();

  std::istream &inputcin = std::cin;
  if (input == "-")
      use_stdin = true;

  int TOVERIFY = 100;		// For each type
  int current_verified = 0;
  int current_violated = 0;


  std::cout << "A.st"  << ","
	    << "A.tr"  << ","
	    << "A.str" << ","
	    << "A.acc" << ","
	    << "A.aps" << ","

	    << "S.st" << ","
	    << "S.tr"  << ","
	    << "S.str" << ","
	    << "S.acc" << ","
	    << "S.aps" << ","

	    << "W.st" << ","
	    << "W.tr"  << ","
	    << "W.str" << ","
	    << "W.acc" << ","
	    << "W.aps" << ","

	    << "T.st" << ","
	    << "T.tr"  << ","
	    << "T.str" << ","
	    << "T.acc" << ","
	    << "T.aps" << ","

	    << "Smin.st" << ","
	    << "Smin.tr"  << ","
	    << "Smin.str" << ","
	    << "Smin.acc" << ","
	    << "Smin.aps" << ","

	    << "Wmin.st" << ","
	    << "Wmin.tr"  << ","
	    << "Wmin.str" << ","
	    << "Wmin.acc" << ","
	    << "Wmin.aps" << ","

	    << "Tmin.st" << ","
	    << "Tmin.tr"  << ","
	    << "Tmin.str" << ","
	    << "Tmin.acc" << ","
	    << "Tmin.aps" << ","

	    << "AxK.st" << ","
	    << "AxK.tr"  << ","

	    << "SxK.st" << ","
	    << "SxK.tr"  << ","

	    << "WxK.st" << ","
	    << "WxK.tr"  << ","

	    << "TxK.st" << ","
	    << "TxK.tr"  << ","

	    << "SminxK.st" << ","
	    << "SminxK.tr"  << ","

	    << "WminxK.st" << ","
	    << "WminxK.tr"  << ","

	    << "TminxK.st" << ","
	    << "TminxK.tr"  << ","

	    << "Verif/Viol" << ","
	    << "Formula"  << std::endl;
  do
    {
      //
      if (current_verified == TOVERIFY &&
	  current_violated == TOVERIFY)
	break;

      // If we use stdin
      if (use_stdin && !std::getline(inputcin, input))
	break;

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
      const spot::kripke *system = spot::load_dve2(dve_model_name,
						 dict, &ap,
						 spot::ltl::constant::true_instance(),
						 2 /* 0 or 1 or 2 */ , true);
      assert(system);

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
      	aut_scc = spot::scc_filter(a, true);
      	delete a;
      	a = aut_scc;
      }


      //
      // Postprocessing
      //
      spot::postprocessor *pp = new spot::postprocessor();
      a = pp->run (a, f);
      delete pp;

      /// Process
      if (is_commut)
      	{
      	  spot::stats_hierarchy sh (a);
      	  sh.stats_automaton();
      	  if (!sh.is_commuting_automaton())
	    {
	      goto clean;
	    }

	  // NUMBERS of states minimums required
      	  int SIZE = 2000;

	  int A_st = 0, A_tr = 0, A_str = 0, A_acc = 0, A_aps;
	  int AxK_st = 0, AxK_tr = 0;

	  int Smin_st = 0, Smin_tr = 0, Smin_str = 0, Smin_acc = 0,
	    Smin_aps = 0;
	  int SminxK_st = 0, SminxK_tr = 0;
	  int S_st = 0, S_tr = 0, S_str = 0, S_acc = 0, S_aps = 0;
	  int SxK_st = 0, SxK_tr = 0;

	  int Wmin_st = 0, Wmin_tr = 0, Wmin_str = 0, Wmin_acc = 0,
	     Wmin_aps = 0;
	  int WminxK_st = 0, WminxK_tr = 0;
	  int W_st = 0, W_tr = 0, W_str = 0, W_acc = 0, W_aps = 0;
	  int WxK_st = 0, WxK_tr = 0;

	  int Tmin_st = 0, Tmin_tr = 0, Tmin_str = 0, Tmin_acc = 0,
	    Tmin_aps = 0;
	  int TminxK_st = 0, TminxK_tr = 0;
	  int T_st = 0, T_tr = 0, T_str = 0, T_acc = 0, T_aps = 0;
	  int TxK_st = 0, TxK_tr = 0;

	  /// Compute stats for the original automaton
	  if (a)
	    {
      	      spot::tgba_sub_statistics sstat_a = sub_stats_reachable(a);
      	      A_st = sstat_a.states;
      	      A_tr = sstat_a.transitions;
      	      A_str = sstat_a.sub_transitions;
	      A_acc = a->number_of_acceptance_conditions();

	      // Compute the set of atomics props used
	      spot::scc_map* x = new spot::scc_map(a);
	      x->build_map();
	      bdd aps = x->aprec_set_of(x->initial());
	      A_aps = bdd_nodecount (aps);
	      delete x;
	    }
	  assert(A_st || A_tr || A_str || A_acc);

      	  // Here we are in a favorable case
      	  spot::scc_decompose sdm (a, true);
      	  const spot::tgba* mstrong = sdm.strong_automaton();
      	  if (mstrong)
      	    {
	      // Compute stats for smin
      	      spot::tgba_sub_statistics sstat_smin = sub_stats_reachable(mstrong);
      	      Smin_st = sstat_smin.states;
      	      Smin_tr = sstat_smin.transitions;
      	      Smin_str = sstat_smin.sub_transitions;
	      Smin_acc = mstrong->number_of_acceptance_conditions();
	      assert(Smin_st || Smin_tr || Smin_str || Smin_acc);

	      // Compute the set of atomics props used
	      spot::scc_map* x = new spot::scc_map(mstrong);
	      x->build_map();
	      bdd aps = x->aprec_set_of(x->initial());
	      Smin_aps = bdd_nodecount (aps);
	      delete x;

	      // Compute stats for Smin product
      	      const spot::tgba *p_mstrong = new spot::tgba_product(system, mstrong);
      	      spot::tgba_statistics s_mstrong = stats_reachable(p_mstrong);
      	      SminxK_st = s_mstrong.states;
      	      SminxK_tr = s_mstrong.transitions;
	      assert(SminxK_st || SminxK_tr);
      	      delete p_mstrong;
      	    }
      	  const spot::tgba* mweak = sdm.weak_automaton();
      	  if (mweak)
      	    {
	      // Compute stats for Wmin
      	      spot::tgba_sub_statistics sstat_wmin = sub_stats_reachable(mweak);
      	      Wmin_st = sstat_wmin.states;
      	      Wmin_tr = sstat_wmin.transitions;
      	      Wmin_str = sstat_wmin.sub_transitions;
	      Wmin_acc = mweak->number_of_acceptance_conditions();
	      assert(Wmin_st || Wmin_tr || Wmin_str || Wmin_acc);

	      // Compute the set of atomics props used
	      spot::scc_map* x = new spot::scc_map(mweak);
	      x->build_map();
	      bdd aps = x->aprec_set_of(x->initial());
	      Wmin_aps = bdd_nodecount (aps);
	      delete x;

	      // Compute stats for Wmin product
      	      const spot::tgba *p_mweak = new spot::tgba_product(system, mweak);
      	      spot::tgba_statistics s_mweak = stats_reachable(p_mweak);
      	      WminxK_st = s_mweak.states;
      	      WminxK_tr = s_mweak.transitions;
	      assert(WminxK_st || WminxK_tr);
      	      delete p_mweak;
      	    }
      	  const spot::tgba* mterminal = sdm.terminal_automaton();
      	  if (mterminal)
      	    {
	      // Compute stats for Tmin
      	      spot::tgba_sub_statistics sstat_tmin = sub_stats_reachable(mterminal);
      	      Tmin_st = sstat_tmin.states;
      	      Tmin_tr = sstat_tmin.transitions;
      	      Tmin_str = sstat_tmin.sub_transitions;
	      Tmin_acc = mterminal->number_of_acceptance_conditions();
	      assert(Tmin_st || Tmin_tr || Tmin_str || Tmin_acc);

	      // Compute the set of atomics props used
	      spot::scc_map* x = new spot::scc_map(mterminal);
	      x->build_map();
	      bdd aps = x->aprec_set_of(x->initial());
	      Tmin_aps = bdd_nodecount (aps);
	      delete x;

	      // Compute stats for Tmin product
      	      const spot::tgba *p_mterminal = new spot::tgba_product(system, mterminal);
      	      spot::tgba_statistics s_mterminal = stats_reachable(p_mterminal);
      	      TminxK_st = s_mterminal.states;
      	      TminxK_tr = s_mterminal.transitions;
	      assert(TminxK_st || TminxK_tr);
      	      delete p_mterminal;
      	    }

      	  //
      	  // It's a formula we want to keep!
      	  //
      	  if (SminxK_st >= SIZE || WminxK_st >= SIZE || TminxK_st >= SIZE)
      	    {
      	      spot::scc_decompose sd (a, false);
      	      const spot::tgba* strong = sd.strong_automaton();
      	      if (strong)
      		{
		  // Compute stats for S
		  spot::tgba_sub_statistics sstat_s = sub_stats_reachable(strong);
		  S_st = sstat_s.states;
		  S_tr = sstat_s.transitions;
		  S_str = sstat_s.sub_transitions;
		  S_acc = strong->number_of_acceptance_conditions();
		  assert(S_st || S_tr || S_str || S_acc);

		  // Compute the set of atomics props used
		  spot::scc_map* x = new spot::scc_map(strong);
		  x->build_map();
		  bdd aps = x->aprec_set_of(x->initial());
		  S_aps = bdd_nodecount (aps);
		  delete x;


		  // Compute stats for S product
      		  const spot::tgba *p_strong = new spot::tgba_product(system, strong);
      		  spot::tgba_statistics s_strong = stats_reachable(p_strong);
		  SxK_st = s_strong.states;
		  SxK_tr = s_strong.transitions;
		  assert(SxK_st || SxK_tr);
      		  delete p_strong;
      		}
      	      const spot::tgba* weak = sd.weak_automaton();
      	      if (weak)
      		{
		  // Compute stats for W
		  spot::tgba_sub_statistics sstat_w = sub_stats_reachable(weak);
		  W_st = sstat_w.states;
		  W_tr = sstat_w.transitions;
		  W_str = sstat_w.sub_transitions;
		  W_acc = weak->number_of_acceptance_conditions();
		  assert(W_st || W_tr || W_str || W_acc);

		  // Compute the set of atomics props used
		  spot::scc_map* x = new spot::scc_map(weak);
		  x->build_map();
		  bdd aps = x->aprec_set_of(x->initial());
		  W_aps = bdd_nodecount (aps);
		  delete x;


		  // Compute stats for W product
      		  const spot::tgba *p_weak = new spot::tgba_product(system, weak);
      		  spot::tgba_statistics s_weak = stats_reachable(p_weak);
		  WxK_st = s_weak.states;
		  WxK_tr = s_weak.transitions;
		  assert(WxK_st || WxK_tr);
      		  delete p_weak;
      		}
      	      const spot::tgba* terminal = sd.terminal_automaton();
      	      if (terminal)
      		{
		  // Compute stats for T
		  spot::tgba_sub_statistics sstat_t = sub_stats_reachable(terminal);
		  T_st = sstat_t.states;
		  T_tr = sstat_t.transitions;
		  T_str = sstat_t.sub_transitions;
		  T_acc = terminal->number_of_acceptance_conditions();
		  assert(T_st || T_tr || T_str || T_acc);

		  // Compute the set of atomics props used
		  spot::scc_map* x = new spot::scc_map(terminal);
		  x->build_map();
		  bdd aps = x->aprec_set_of(x->initial());
		  T_aps = bdd_nodecount (aps);
		  delete x;


		  // Compute stats for T product
      		  const spot::tgba *p_terminal =
      		    new spot::tgba_product(system, terminal);
      		  spot::tgba_statistics s_terminal = stats_reachable(p_terminal);
		  TxK_st = s_terminal.states;
		  TxK_tr = s_terminal.transitions;
		  assert(TxK_st || TxK_tr);
      		  delete p_terminal;
      		}

      	      // And we perform the original product and verification
      	      const spot::tgba *product =
      		new spot::tgba_product(system, a);

	      spot::emptiness_check* ec  =  0;
	      spot::emptiness_check_result* res = 0;
	      spot::emptiness_check_instantiator* echeck_inst = 0;
	      if (product)
		{
      		  spot::tgba_statistics s_product = stats_reachable(product);
		  AxK_st = s_product.states;
		  AxK_tr = s_product.transitions;
		  assert(AxK_st || AxK_tr);

		  // Emptiness check it !!!!!!!!
		  const char* err;
		  echeck_inst =
		    spot::emptiness_check_instantiator::construct( "Cou99", &err);
		  if (!echeck_inst)
		    {
		      std::cerr << "ERROR\n";
		      exit(2);
		    }
		  // Instanciate the emptiness check
		  ec  =  echeck_inst->instantiate(product);
		  res = ec->check();
		}
	      std::string verified =  res ? "VIOLATED": "VERIFIED";

      	      if (!res)
      		{
      		  if (current_violated == TOVERIFY)
		    {
		      delete res;
		      delete ec;
		      delete product;
		      delete echeck_inst;
		      goto clean;
		    }
      		  ++current_violated;
      		}
      	      else
      		{
      		  if (current_verified == TOVERIFY)
		    {
		      delete res;
		      delete ec;
		      delete product;
		      delete echeck_inst;
		      goto clean;
		    }
      		  ++current_verified;
      		}
	      std::cout << A_st  << ","
			<< A_tr  << ","
			<< A_str << ","
			<< A_acc << ","
			<< A_aps << ","

			<< S_st << ","
			<< S_tr  << ","
			<< S_str << ","
			<< S_acc << ","
			<< S_aps << ","

			<< W_st << ","
			<< W_tr  << ","
			<< W_str << ","
			<< W_acc << ","
			<< W_aps << ","

			<< T_st << ","
			<< T_tr  << ","
			<< T_str << ","
			<< T_acc << ","
			<< T_aps << ","

			<< Smin_st << ","
			<< Smin_tr  << ","
			<< Smin_str << ","
			<< Smin_acc << ","
			<< Smin_aps << ","

			<< Wmin_st << ","
			<< Wmin_tr  << ","
			<< Wmin_str << ","
			<< Wmin_acc << ","
			<< Wmin_aps << ","

			<< Tmin_st << ","
			<< Tmin_tr  << ","
			<< Tmin_str << ","
			<< Tmin_acc << ","
			<< Tmin_aps << ","

			<< AxK_st << ","
			<< AxK_tr  << ","

			<< SxK_st << ","
			<< SxK_tr  << ","

			<< WxK_st << ","
			<< WxK_tr  << ","

			<< TxK_st << ","
			<< TxK_tr  << ","

			<< SminxK_st << ","
			<< SminxK_tr  << ","

			<< WminxK_st << ","
			<< WminxK_tr  << ","

			<< TminxK_st << ","
			<< TminxK_tr  << ","

			<< verified << ","
			<< input    << std::endl;
      	      delete res;
      	      delete ec;
      	      delete product;
      	      delete echeck_inst;
      	    }
      	}

    clean:
      // Clean up
      f->destroy();
      f = 0;
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
