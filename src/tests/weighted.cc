// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement de
// l'Epita.
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
#include "twaalgos/weighted_ec.hh"
#include "graph/graph.hh"
#include "twa/wautgraph.hh"


// --------------------------------------------------------------------------
// Specialization for printing graphs with weighted states
// --------------------------------------------------------------------------
void
dot_state(std::ostream& out, spot::digraph<spot::w_state, void>& g, unsigned n)
{
  out << " [label=\"" << g.state_data(n).weight()
      << ",acc=" << g.state_data(n).acc() << "\"]\n";
}

template <typename TR>
void
dot_trans(std::ostream& out, spot::digraph<spot::w_state, void>&, TR&)
{
  out << '\n';
}


template <typename SL, typename TL>
void
dot(std::ostream& out, spot::digraph<SL, TL>& g)
{
  out << "digraph {\n";
  unsigned c = g.num_states();
  for (unsigned s = 0; s < c; ++s)
    {
      out << ' ' << s;
      dot_state(out, g, s);
      for (auto& t: g.out(s))
	{
	  out << ' ' << s << " -> " << t.dst;
	  dot_trans(out, g, t);
	}
    }
  out << "}\n";
}


void f1()
{
  // First declare a digraph that will represent our weighted aut.
  spot::digraph<spot::w_state, void> g(3);

  // Now declare the number of acceptance conditions
  spot::acc_cond ac(1);
  ac.set_generalized_buchi();
  auto m1 = ac.marks({0}); // If many marks {0,1, ...}

  // Build the structure of the automaton
  auto s1 = g.new_state(200);
  auto s2 = g.new_state(100, m1);
  auto s3 = g.new_state(100);
  g.new_transition(s1, s1);
  g.new_transition(s1, s2);
  g.new_transition(s1, s3);
  g.new_transition(s2, s3);
  g.new_transition(s3, s2);

  // Print it !
  dot(std::cout, g);

  // Build the proxy automaton over this graph
  spot::w_aut_graph aut(g, ac);

  // Call the emptiness check and display the result
  auto wec = spot::weighted_ec(aut);
  int result = wec.check();
  if (result == -1)
    std::cout << "No counterexample found!" << std::endl;
  else
    std::cout << "Counterexample found of weight "
	      << result << std::endl;
}

int main()
{
  f1();
}
