// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
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

#include <iostream>
#include <vector>
#include <algorithm>
#include "ltlvisit/apcollect.hh"
#include "ltlvisit/randomltl.hh"
#include "tgbaalgos/ltl2tgba_fm.hh"
#include "tgbaalgos/translate.hh"
#include "tgbaalgos/postproc.hh"
#include "tgbaalgos/randomgraph.hh"
#include "misc/timer.hh"

template<typename C, typename D>
void timeit(const char* descr, C code, D destr)
{
  spot::stopwatch w;
  double times[5];
  for (unsigned i = 0; i < 5; ++i)
    {
      w.start();
      code();
      times[i] = w.stop();
      destr();
    }
  std::sort(times, times + 5);
  std::cout << descr << ",\t" << times[2] << "\t(+" << times[4] - times[2]
	    << ",\t-" << times[2] - times[0] << ")\n";
}

template<typename C>
void timeit(const char* descr, C code)
{
  timeit(descr, code, [](){});
}

void sequence_1()
{
  auto ap = spot::ltl::create_atomic_prop_set(5);
  std::vector<const spot::ltl::formula*> formulas;
  formulas.reserve(40000);

  spot::ltl::random_ltl r(&ap);

  srand(0);

  timeit("generate 40000 random LTL formulas (size 20) + destroy",
	 [&]() {
	   for (unsigned i = 0; i < 40000; ++i)
	     formulas.push_back(r.generate(20));
	   for (auto f: formulas)
	     f->destroy();
	 },
	 [&]() {
	   formulas.clear();
	   srand(0);
	 });

  // generate just 1000 formulas.
  for (unsigned i = 0; i < 1000; ++i)
    formulas.push_back(r.generate(20));

  auto d = spot::make_bdd_dict();

  timeit("ltl_to_tgba_fm() on 1000 random LTL formulas (size 20)",
	 [&]() {
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d);
	 });

  timeit("ltl_to_tgba_fm(exprop) on 1000 random LTL formulas",
	 [&]() {
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d, true);
	 });

  timeit("translate(TGBA,Small,High) on 1000 random LTL formulas",
	 [&]() {
	   spot::translator t;
	   t.set_type(spot::translator::output_type::TGBA);
	   t.set_pref(spot::postprocessor::Small);
	   t.set_level(spot::translator::optimization_level::High);
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d, true);
	 });

  timeit("translate(TGBA,Small,Low) on 1000 random LTL formulas",
	 [&]() {
	   spot::translator t;
	   t.set_type(spot::translator::output_type::TGBA);
	   t.set_pref(spot::postprocessor::Small);
	   t.set_level(spot::translator::optimization_level::Low);
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d, true);
	 });

  timeit("translate(TGBA,Det,High) on 1000 random LTL formulas",
	 [&]() {
	   spot::translator t;
	   t.set_type(spot::translator::output_type::TGBA);
	   t.set_pref(spot::postprocessor::Deterministic);
	   t.set_level(spot::translator::optimization_level::High);
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d, true);
	 });

  timeit("translate(TGBA,Det,Low) on 1000 random LTL formulas",
	 [&]() {
	   spot::translator t;
	   t.set_type(spot::translator::output_type::TGBA);
	   t.set_pref(spot::postprocessor::Deterministic);
	   t.set_level(spot::translator::optimization_level::Low);
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d, true);
	 });

  timeit("translate(BA,Small,High) on 1000 random LTL formulas",
	 [&]() {
	   spot::translator t;
	   t.set_type(spot::translator::output_type::BA);
	   t.set_pref(spot::postprocessor::Small);
	   t.set_level(spot::translator::optimization_level::High);
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d, true);
	 });

  timeit("translate(BA,Small,Low) on 1000 random LTL formulas",
	 [&]() {
	   spot::translator t;
	   t.set_type(spot::translator::output_type::BA);
	   t.set_pref(spot::postprocessor::Small);
	   t.set_level(spot::translator::optimization_level::Low);
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d, true);
	 });

  timeit("translate(BA,Det,High) on 1000 random LTL formulas",
	 [&]() {
	   spot::translator t;
	   t.set_type(spot::translator::output_type::BA);
	   t.set_pref(spot::postprocessor::Deterministic);
	   t.set_level(spot::translator::optimization_level::High);
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d, true);
	 });

  timeit("translate(BA,Det,Low) on 1000 random LTL formulas",
	 [&]() {
	   spot::translator t;
	   t.set_type(spot::translator::output_type::BA);
	   t.set_pref(spot::postprocessor::Deterministic);
	   t.set_level(spot::translator::optimization_level::Low);
	   for (auto f: formulas)
	     spot::ltl_to_tgba_fm(f, d, true);
	 });

  std::vector<spot::tgba_digraph_ptr> randaut;
  randaut.reserve(100);
  auto aprops = spot::ltl::create_atomic_prop_set(4);

  timeit("100 x random_graphs(states=500, density=0.1, ap=4)",
	 [&] () {
	   for (unsigned i = 0; i < 100; ++i)
	     randaut.push_back(spot::random_graph(500, 0.1, &aprops, d));
	 },
	 [&] () {
	   randaut.clear();
	 });

  for (unsigned i = 0; i < 100; ++i)
    randaut.push_back(spot::random_graph(500, 0.1, &aprops, d));

  timeit("merge_transition() on above 100 random graphs (0.1)",
	 [&] () {
	   for (auto& a: randaut)
	     a->merge_transitions();
	 });
  randaut.clear();

  timeit("100 x random_graphs(states=500, density=0.2, ap=4)",
	 [&] () {
	   for (unsigned i = 0; i < 100; ++i)
	     randaut.push_back(spot::random_graph(500, 0.2, &aprops, d));
	 },
	 [&] () {
	   randaut.clear();
	 });

  for (unsigned i = 0; i < 100; ++i)
    randaut.push_back(spot::random_graph(500, 0.2, &aprops, d));

  timeit("merge_transition() on above 100 random graphs (0.2)",
	 [&] () {
	   for (auto& a: randaut)
	     a->merge_transitions();
	 });
  randaut.clear();
}



int main()
{
  sequence_1();
}
