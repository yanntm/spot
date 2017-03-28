// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita
// (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.

#include <spot/algebra/autalgebra.hh>
#include <spot/twaalgos/determinize.hh>
#include <spot/twaalgos/remfin.hh>
#include <spot/twaalgos/translate.hh>

#include <spot/tl/randomltl.hh>

constexpr size_t WILKE_MAX = 100;
constexpr size_t AUTOMATA_MAX = 50;

// returns 0 iff both automata have the same syntactic Wilke algebra
// return value
//    0     if both automata have the same syntatic Wilke algebra
//    < 0   if one Wilke algebra is too big (> WILKE_MAX)
//    > 0   if syntactic algebras differ
static
int
algebra_cross(const spot::const_twa_ptr& a, const spot::const_twa_ptr& b)
{
  auto sga = spot::aut_to_semigroup(a);
  auto sgb = spot::aut_to_semigroup(b);

  std::cerr << "\tND Wilke has " << sga->size() << " elements" << std::endl;
  std::cerr << "\tD Wilke has " << sgb->size() << " elements" << std::endl;

  if (sga->size() > WILKE_MAX || sgb->size() > WILKE_MAX)
    return -1;

  auto swa = sga->syntactic_wilke();
  auto swb = sgb->syntactic_wilke();

  bool res = !(swa == swb);

  if (res)
    {
      std::cerr << "Syntactic Wilke algebras mismatch\n";

      std::cerr << "ND morphism\n";
      sga->print_morphism(std::cerr);
      std::cerr << "D morphism\n";
      sgb->print_morphism(std::cerr);


      std::cerr << "\nTransition Wilke for the ND automaton is\n";
      sga->print(std::cerr);
      std::cerr << "\nTransition Wilke for the D automaton is\n";
      sgb->print(std::cerr);

      std::cerr << "\nSyntactic Wilke for the ND automaton is\n";
      swa.print(std::cerr);
      std::cerr << "\nSyntactic Wilke for the D automaton is\n";
      swb.print(std::cerr);
      std::cerr << std::endl;
    }

  return !(swa == swb);
}

static
int
algebra_cross(const spot::formula& f)
{
  spot::translator trans;
  // A TGBA that recognizes the formula.
  auto aut = trans.run(f);
  std::cerr << "\tND automaton has " << aut->num_states() << " states\n";
  // An equivalent deterministic parity automaton.
  auto daut = spot::tgba_determinize(aut);
  // Remove fin acceptance conditions for the algebra module
  daut = spot::remove_fin(daut);
  std::cerr << "\tD automaton has " << daut->num_states() << " states\n";

  if (aut->num_states() > AUTOMATA_MAX || daut->num_states() > AUTOMATA_MAX)
    return -1;

  return algebra_cross(aut, daut);
}

constexpr int NB_AP = 3;
constexpr unsigned NB_FORMULAE = 200;

// TODO before trying random formulae, we could try a fixed collection of 'easy'
// ones:
// True, False, GF(a)&GF(b) ...

int
main()
{
  spot::randltlgenerator gen(3, {});

  for (unsigned i = 0; i != NB_FORMULAE;)
    {
      auto f = gen.next();

      std::cerr << "\nWorking on " << (i+1) << "th formula " << f << std::endl;
      int res = algebra_cross(f);
      if (res < 0)
        {
          // an algebra was too big, syntactic Wilke were skipped
          continue;
        }
      else if (res == 0)
        {
          // syntactic Wilke are the same, continue
          ++i;
          continue;
        }
      else
        {
          // syntactic Wilke differ
          std::cerr << "wrong with " << f << std::endl;
          return 2;
        }
    }

  return 0;
}

