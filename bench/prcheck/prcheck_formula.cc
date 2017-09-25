// -*- coding: utf-8 -*-
// Copyright (C) 2014, 2015, 2016 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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

#include <config.h>
#include <spot/misc/timer.hh>
#include <spot/tl/hierarchy.hh>
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/simplify.hh>
#include <spot/twa/fwd.hh>
#include <spot/twa/acc.hh>
#include <spot/twaalgos/ltl2tgba_fm.hh>
#include <spot/twaalgos/cobuchi.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/postproc.hh>
#include <spot/twaalgos/remfin.hh>
#include <spot/twaalgos/sccfilter.hh>
#include <spot/twaalgos/totgba.hh>
#include <spot/misc/memusage.hh>

# define val(X) X / clocks_per_sec

static bool
cobuchi_realizable(const spot::const_twa_graph_ptr& aut,
                   const spot::const_twa_graph_ptr& aut_neg)
{
  // Find which algorithm must be performed between nsa_to_nca() and
  // dnf_to_nca(). Throw an exception if none of them can be performed.
  std::vector<spot::acc_cond::rs_pair> pairs;
  bool streett_or_parity = aut->acc().is_streett_like(pairs)
                           || aut->acc().is_parity();
  if (!streett_or_parity && !aut->get_acceptance().is_dnf())
    throw std::runtime_error("cobuchi_realizable() only works with "
                             "Streett-like, Parity or any "
                             "acceptance condition in DNF");

  spot::process_timer p1;
  p1.start();
  spot::twa_graph_ptr cobuchi = streett_or_parity ? spot::nsa_to_nca(aut)
                                                  : spot::dnf_to_nca(aut);
  p1.stop();
  std::cout << p1.walltime() << ',';

  p1.start();
  bool res = !cobuchi->intersects(aut_neg);
  p1.stop();
  std::cout << p1.walltime() << ',';

  return res;
}

static bool
detbuchi_realizable(const spot::twa_graph_ptr& aut)
{
  if (is_universal(aut))
    {
      std::cout << ",,,";
      return true;
    }

  // if aut is a non-deterministic TGBA, we do
  // TGBA->DPA->DRA->(D?)BA.  The conversion from DRA to
  // BA will preserve determinism if possible.
  spot::process_timer p1;
  p1.start();
  spot::postprocessor p;
  p.set_type(spot::postprocessor::Generic);
  p.set_pref(spot::postprocessor::Deterministic);
  p.set_level(spot::postprocessor::Low);
  auto dpa = p.run(aut);
  p1.stop();
  std::cout << p1.walltime() << ',';

  if (dpa->acc().is_generalized_buchi())
    {
      std::cout << ",,";
      assert(spot::is_deterministic(dpa));
      return true;
    }
  else
    {
      p1.start();
      spot::twa_graph_ptr dra = spot::to_generalized_rabin(dpa);
      p1.stop();
      std::cout << p1.walltime() << ',';

      p1.start();
      bool res = spot::rabin_is_buchi_realizable(dra);
      p1.stop();
      std::cout << p1.walltime() << ',';
      return res;
    }
}

enum class prcheck
  {
    PR_Auto,
    PR_via_CoBuchi,
    PR_via_Rabin,
    PR_via_Rabin_Opti
  };

static
bool
is_persistence(spot::formula f, int algo, spot::bdd_dict_ptr d)
{
  if (f.is_syntactic_persistence())
    {
      std::cout << ",,,,";
      return true;
    }

  spot::twa_graph_ptr trans1 = nullptr;
  spot::twa_graph_ptr trans2 = nullptr;
  spot::process_timer p1;
  switch (algo)
  {
    case 1:
      p1.start();
      trans2 = spot::ltl_to_tgba_fm(spot::formula::Not(f), d, true);
      p1.stop();
      std::cout << p1.walltime() << ',';

      trans2 = scc_filter(trans2);
      if (is_universal(trans2))
        {
          std::cout << ",,,";
          return true;
        }

      p1.start();
      trans1 = spot::ltl_to_tgba_fm(f, trans2->get_dict(), true);
      p1.stop();
      std::cout << p1.walltime() << ',';
      return cobuchi_realizable(trans1, trans2);

    case 2:
      p1.start();
      trans1 = spot:: ltl_to_tgba_fm(spot::formula::Not(f), d, true);
      p1.stop();
      std::cout << p1.walltime() << ',';
      return detbuchi_realizable(trans1);

    default:
      SPOT_UNREACHABLE();
      break;
  }
}

static
bool
is_recurrence(spot::formula f, int algo, spot::bdd_dict_ptr d)
{
  if (f.is_syntactic_recurrence())
    {
      std::cout << ",,,,";
      return true;
    }

  spot::twa_graph_ptr trans1 = nullptr;
  spot::twa_graph_ptr trans2 = nullptr;
  spot::process_timer p1;
  switch (algo)
  {
    case 1:
      p1.start();
      trans2 = spot::ltl_to_tgba_fm(f, d, true);
      p1.stop();
      std::cout << p1.walltime() << ',';

      trans2 = scc_filter(trans2);
      if (is_universal(trans2))
        {
          std::cout << ",,,";
          return true;
        }

      p1.start();
      trans1 =
        spot::ltl_to_tgba_fm(spot::formula::Not(f), trans2->get_dict(), true);
      p1.stop();
      std::cout << p1.walltime() << ',';
      return cobuchi_realizable(trans1, trans2);

    case 2:
      p1.start();
      trans1 = spot::ltl_to_tgba_fm(f, d, true);
      p1.stop();
      std::cout << p1.walltime() << ',';
      return detbuchi_realizable(trans1);

    default:
      SPOT_UNREACHABLE();
      break;
  }
}

int
main(int argc, char** argv)
{
#ifdef _SC_CLK_TCK
  const long clocks_per_sec = sysconf(_SC_CLK_TCK);
#else
#  ifdef CLOCKS_PER_SEC
  const long clocks_per_sec = CLOCKS_PER_SEC;
#  else
  const long clocks_per_sec = 100;
#  endif
#endif

  // Handle arguments.
  if (argc != 4)
    {
      std::cerr << "usage: <ltl> <persistence|recurrence> <algo>\n";
      exit(2);
    }
  std::string ltl(argv[1]);
  std::string method(argv[2]);
  if (method != "persistence" && method != "recurrence")
    {
      std::cerr << "invalid method " << argv[2] << '\n';
      exit(2);
    }
  std::istringstream ss(argv[3]);
  int algo = 0;
  if (!(ss >> algo))
    {
      std::cerr << "Invalid number " << argv[1] << '\n';
      exit(2);
    }

  bool res = false;
  spot::bdd_dict_ptr d = spot::make_bdd_dict();
  spot::formula f = spot::parse_formula(ltl);
  // Perform a quick simplification of the formula taking into account the
  // following simplification's parameters: basics, synt_impl, event_univ.
  spot::tl_simplifier simpl(spot::tl_simplifier_options(true, true, true));
  f = simpl.simplify(f);

  spot::process_timer p;
  p.start();
  if (method == "recurrence")
    res = is_recurrence(f, algo, d);
  else if (method == "persistence")
    res = is_persistence(f, algo, d);
  else
    SPOT_UNREACHABLE();
  p.stop();
  clock_t cpusys = p.cputime(false, true, true, true);
  clock_t cpuusr = p.cputime(true, false, true, true);
  double wall = p.walltime();

  int pages = spot::memusage();

  std::cout << res << ','
            << val(cpusys) << ','
            << val(cpuusr) << ','
            << wall << ','
            << pages;

  return 0;
}
