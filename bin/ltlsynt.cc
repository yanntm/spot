// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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

#include <memory>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "argmatch.h"

#include "common_aoutput.hh"
#include "common_finput.hh"
#include "common_setup.hh"
#include "common_sys.hh"

#include <spot/misc/bddlt.hh>
#include <spot/misc/game.hh>
#include <spot/misc/minato.hh>
#include <spot/tl/formula.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/aiger.hh>
#include <spot/twaalgos/complete.hh>
#include <spot/twaalgos/determinize.hh>
#include <spot/twaalgos/parity.hh>
#include <spot/twaalgos/sbacc.hh>
#include <spot/twaalgos/totgba.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twa/twagraph.hh>

enum
{
  OPT_ALGO = 256,
  OPT_INCREMENTAL,
  OPT_INPUT,
  OPT_OUTPUT,
  OPT_PRINT_AIGER,
  OPT_REAL
};

static const argp_option options[] =
  {
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Input options:", 1 },
    { "ins", OPT_INPUT, "PROPS", 0,
      "comma-separated list of uncontrollable (a.k.a. input) atomic"
      " propositions", 0},
    { "outs", OPT_OUTPUT, "PROPS", 0,
      "comma-separated list of controllable (a.k.a. output) atomic"
      " propositions", 0},
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Fine tuning:", 10 },
    { "algo", OPT_ALGO, "qp|rec", 0,
      "choose the parity game algorithm, valid ones are rec (Zielonka's"
      " recursive algorithm, default) and qp (Calude et al.'s quasi-polynomial"
      " time algorithm)", 0 },
    { "incremental", OPT_INCREMENTAL, nullptr, 0,
      "Start with the controller having only a subset of its transitions"
      " available, and incrementally add them back until a strategy was found"
      " or all of its transitions have been added", 0},
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Output options:", 20 },
    { "realizability", OPT_REAL, nullptr, 0,
      "realizability only, do not compute a winning strategy", 0},
    { "aiger", OPT_PRINT_AIGER, nullptr, 0,
      "prints the winning strategy as an AIGER circuit", 0},
    /**************************************************/
    { nullptr, 0, nullptr, 0, "Miscellaneous options:", -1 },
    { nullptr, 0, nullptr, 0, nullptr, 0 },
  };

static const struct argp_child children[] =
  {
    { &finput_argp_headless, 0, nullptr, 0 },
    { &aoutput_argp, 0, nullptr, 0 },
    //{ &aoutput_o_format_argp, 0, nullptr, 0 },
    { &misc_argp, 0, nullptr, 0 },
    { nullptr, 0, nullptr, 0 }
  };

const char argp_program_doc[] = "\
Synthesize a controller from its LTL specification.\v\
Exit status:\n\
  0   if the input problem is realizable\n\
  1   if the input problem is not realizable\n\
  2   if any error has been reported";

static std::vector<std::string> input_aps;
static std::vector<std::string> output_aps;

bool opt_print_pg(false);
bool opt_real(false);
bool opt_print_aiger(false);
bool opt_incremental(false);

// FIXME rename options to choose the algorithm
enum solver
{
  QP,
  REC
};

static char const *const solver_args[] =
{
  "qp", "quasi-polynomial",
  "recursive",
  nullptr
};
static solver const solver_types[] =
{
  QP, QP,
  REC,
};
ARGMATCH_VERIFY(solver_args, solver_types);

static solver opt_solver = REC;

namespace
{
  // Take an automaton and a set of atomic propositions I, and split each
  // transition
  //
  //     p -- cond --> q                cond in 2^2^AP
  //
  // into a set of transitions of the form
  //
  //     p -- i1 --> r1 -- o1 --> q     i1 in 2^I
  //                                    o1 in 2^O
  //
  //     p -- i2 --> r2 -- o2 --> q     i2 in 2^I
  //                                    o2 in 2^O
  //     ...
  //
  // where O = AP\I, and such that cond = (i1 & o1) | (i2 & o2) | ...
  //
  // When determinized, this encodes a game automaton that has a winning
  // strategy iff aut has an accepting run for any valuation of atomic
  // propositions in I.

  static spot::twa_graph_ptr
  split_automaton(const spot::twa_graph_ptr& aut,
                  bdd input_bdd)
  {
    auto tgba = spot::to_generalized_buchi(aut);
    auto split = spot::make_twa_graph(tgba->get_dict());
    split->copy_ap_of(tgba);
    split->copy_acceptance_of(tgba);
    split->new_states(tgba->num_states());
    split->set_init_state(tgba->get_init_state_number());

    for (unsigned src = 0; src < tgba->num_states(); ++src)
      for (const auto& e: tgba->out(src))
        {
          spot::minato_isop isop(e.cond);
          bdd cube;
          while ((cube = isop.next()) != bddfalse)
            {
              unsigned q = split->new_state();
              bdd in = bdd_existcomp(cube, input_bdd);
              bdd out = bdd_exist(cube, input_bdd);
              split->new_edge(src, q, in, 0);
              split->new_edge(q, e.dst, out, e.acc);
            }
        }
    split->prop_universal(spot::trival::maybe());
    return split;
  }

  // Generates a vector indicating the owner of each state, with the
  // convention that false is player 0 (the environment) and true is player 1
  // (the controller).  Starting with player 0 on the initial state, the owner
  // is switched after each transition.
  static std::vector<bool>
  make_alternating_owner(const spot::twa_graph_ptr& dpa,
                         bool init_owner = false)
  {
    std::vector<bool> seen(dpa->num_states(), false);
    std::vector<unsigned> todo({dpa->get_init_state_number()});
    std::vector<bool> owner(dpa->num_states());
    owner[dpa->get_init_state_number()] = init_owner;
    while (!todo.empty())
      {
        unsigned src = todo.back();
        todo.pop_back();
        seen[src] = true;
        for (auto& e: dpa->out(src))
          {
            if (!seen[e.dst])
              {
                owner[e.dst] = !owner[src];
                todo.push_back(e.dst);
              }
          }
      }
    return owner;
  }

  static spot::twa_graph_ptr
  to_dpa(const spot::twa_graph_ptr& split)
  {
    auto dpa = spot::tgba_determinize(split);
    dpa->merge_edges();
    spot::complete_here(dpa);
    spot::colorize_parity_here(dpa, true);
    spot::change_parity_here(dpa, spot::parity_kind_max,
                             spot::parity_style_odd);
    if (opt_print_pg)
      dpa = spot::sbacc(dpa);
    bool max, odd;
    dpa->acc().is_parity(max, odd);
    assert(max && odd);
    assert(spot::is_deterministic(dpa));
    return dpa;
  }

  // Construct a smaller automaton, filtering out states that are not
  // accessible.  Also merge back pairs of p --(i)--> q --(o)--> r
  // transitions to p --(i&o)--> r.
  static spot::twa_graph_ptr
  strat_to_aut(const spot::parity_game& pg,
               const spot::parity_game::strategy_t& strat,
               const spot::twa_graph_ptr& dpa,
               bdd all_outputs)
  {
    auto aut = spot::make_twa_graph(dpa->get_dict());
    aut->copy_ap_of(dpa);
    std::vector<unsigned> todo{pg.get_init_state_number()};
    std::vector<int> pg2aut(pg.num_states(), -1);
    aut->set_init_state(aut->new_state());
    pg2aut[pg.get_init_state_number()] = aut->get_init_state_number();
    while (!todo.empty())
      {
        unsigned s = todo.back();
        todo.pop_back();
        for (auto& e1: dpa->out(s))
          {
            unsigned i = 0;
            for (auto& e2: dpa->out(e1.dst))
              {
                bool self_loop = false;
                if (e1.dst == s || e2.dst == e1.dst)
                  self_loop = true;
                if (self_loop || strat.at(e1.dst) == i)
                  {
                    bdd out = bdd_satoneset(e2.cond, all_outputs, bddfalse);
                    if (pg2aut[e2.dst] == -1)
                      {
                        pg2aut[e2.dst] = aut->new_state();
                        todo.push_back(e2.dst);
                      }
                    aut->new_edge(pg2aut[s], pg2aut[e2.dst],
                                  (e1.cond & out), 0);
                    break;
                  }
                ++i;
              }
          }
      }
    aut->purge_dead_states();
    aut->set_named_prop("synthesis-outputs", new bdd(all_outputs));
    return aut;
  }

  using edge_iterator_t =
    spot::internal::edge_iterator<spot::twa_graph::graph_t>;

  spot::twa_graph_ptr filter_edges(const spot::twa_graph_ptr& aut,
                                   const std::vector<edge_iterator_t>& filter)
  {
    auto res = spot::make_twa_graph(aut->get_dict());
    res->copy_ap_of(aut);
    res->copy_acceptance_of(aut);
    for (unsigned s = 0; s < aut->num_states(); ++s)
      res->new_state();
    res->set_init_state(aut->get_init_state_number());
    for (unsigned s = 0; s < aut->num_states(); ++s)
      for (auto it = aut->out(s).begin(); it != filter[s]; ++it)
        res->new_edge(it->src, it->dst, it->cond, it->acc);
    return res;
  }

  bool add_one_trans(const spot::twa_graph_ptr& aut,
                     std::vector<edge_iterator_t>& filter)
  {
    for (unsigned s = 0; s < filter.size(); ++s)
      {
        if (filter[s] != aut->out(s).end())
          {
            ++filter[s];
            return true;
          }
      }
    return false;
  }

  class ltl_processor final : public job_processor
  {
  private:
    spot::translator& trans_;
    std::vector<std::string> input_aps_;
    std::vector<std::string> output_aps_;

  public:

    ltl_processor(spot::translator& trans,
                  std::vector<std::string> input_aps_,
                  std::vector<std::string> output_aps_)
      : trans_(trans), input_aps_(input_aps_), output_aps_(output_aps_)
    {
    }

    int process_formula(spot::formula f,
                        const char*, int) override
    {
      spot::process_timer timer;
      timer.start();

      auto aut = trans_.run(&f);
      bdd all_inputs = bddtrue;
      bdd all_outputs = bddtrue;
      for (unsigned i = 0; i < input_aps_.size(); ++i)
        {
          std::ostringstream lowercase;
          for (char c: input_aps_[i])
            lowercase << (char)std::tolower(c);
          unsigned v = aut->register_ap(spot::formula::ap(lowercase.str()));
          all_inputs &= bdd_ithvar(v);
        }
      for (unsigned i = 0; i < output_aps_.size(); ++i)
        {
          std::ostringstream lowercase;
          for (char c: output_aps_[i])
            lowercase << (char)std::tolower(c);
          unsigned v = aut->register_ap(spot::formula::ap(lowercase.str()));
          all_outputs &= bdd_ithvar(v);
        }
      auto split = split_automaton(aut, all_inputs);
      auto owner = make_alternating_owner(split);

      // filter[s] = max transition iterator considered for state s.
      // Start with a deterministic subautomaton by allowing at most 1
      // outgoing transition for each state
      std::vector<edge_iterator_t> filter(split->num_states());
      if (opt_incremental)
      {
        for (unsigned s = 0; s < split->num_states(); ++s)
          {
            if (owner[s])
              {
                bdd all = bddfalse;
                auto it = split->out(s).begin();
                for (; it != split->out(s).end(); ++it)
                  {
                    if ((all & it->cond) != bddfalse)
                      break;
                    all |= it->cond;
                  }
                filter[s] = it;
              }
            else
              filter[s] = split->out(s).end();
          }
      }

      for (int it = 0; ; ++it)
        {
          spot::twa_graph_ptr dpa;
          if (opt_incremental)
            {
              auto constrained = filter_edges(split, filter);
              dpa = to_dpa(constrained);
            }
          else
            dpa = to_dpa(split);
          auto owner = make_alternating_owner(dpa);
          auto pg = spot::parity_game(dpa, owner);
          switch (opt_solver)
            {
              case REC:
                {
                  spot::parity_game::region_t winning_region[2];
                  spot::parity_game::strategy_t strategy[2];
                  pg.solve(winning_region, strategy);
                  if (winning_region[1].count(pg.get_init_state_number()))
                    {
                      std::cout << "REALIZABLE\n";
                      if (!opt_real)
                        {
                          auto strat_aut =
                            strat_to_aut(pg, strategy[1], dpa, all_outputs);

                          // output the winning strategy
                          if (opt_print_aiger)
                            spot::print_aiger(std::cout, strat_aut);
                          else
                            {
                              automaton_printer printer;
                              printer.print(strat_aut, timer);
                            }
                        }
                      return 0;
                    }
                  else
                    {
                      if (opt_incremental && add_one_trans(aut, filter))
                        continue;
                      else
                        {
                          std::cout << "UNREALIZABLE\n";
                          return 1;
                        }
                    }
                }
                break;
              case QP:
                if (!opt_real)
                  {
                    std::cout << "The quasi-polynomial time algorithm does not"
                      " implement synthesis yet, use --realizability\n";
                    return 1;
                  }
                else if (pg.solve_qp())
                  {
                    std::cout << "REALIZABLE";
                    return 0;
                  }
                else if (opt_incremental && add_one_trans(aut, filter))
                  continue;
                else
                  std::cout << "UNREALIZABLE";
                break;
              default:
                SPOT_UNREACHABLE();
                return 0;
            }
        }
    }
  };
}

static int
parse_opt(int key, char* arg, struct argp_state*)
{
  switch (key)
    {
    case OPT_INPUT:
      {
        std::istringstream aps(arg);
        std::string ap;
        while (std::getline(aps, ap, ','))
          {
            ap.erase(remove_if(ap.begin(), ap.end(), isspace), ap.end());
            input_aps.push_back(ap);
          }
        break;
      }
    case OPT_OUTPUT:
      {
        std::istringstream aps(arg);
        std::string ap;
        while (std::getline(aps, ap, ','))
          {
            ap.erase(remove_if(ap.begin(), ap.end(), isspace), ap.end());
            output_aps.push_back(ap);
          }
        break;
      }
    case OPT_ALGO:
      opt_solver = XARGMATCH("--algo", arg, solver_args, solver_types);
      break;
    case OPT_REAL:
      opt_real = true;
      break;
    case OPT_PRINT_AIGER:
      opt_print_aiger = true;
      break;
    case OPT_INCREMENTAL:
      opt_incremental = true;
      break;
    }
  return 0;
}

int
main(int argc, char **argv)
{
  setup(argv);
  const argp ap = { options, parse_opt, nullptr,
                    argp_program_doc, children, nullptr, nullptr };
  if (int err = argp_parse(&ap, argc, argv, ARGP_NO_HELP, nullptr, nullptr))
    exit(err);
  check_no_formula();

  spot::translator trans;
  ltl_processor processor(trans, input_aps, output_aps);
  return processor.run();
}
