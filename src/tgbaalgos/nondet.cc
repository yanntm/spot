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

#include "nondet.hh"
#include "powerset.hh"
#include <armadillo>

namespace spot
{
  double nondet_metric(const tgba* aut, unsigned iter)
  {
    power_map pm;
    tgba_explicit_number* det = tgba_powerset(aut, pm);
    unsigned ns = det->num_states();

    arma::sp_mat a(ns, ns);

    for (unsigned s = 1; s <= ns; ++s)
      {
	const state* ss = det->get_state(s);

	tgba_succ_iterator* it = det->succ_iter(ss);

	// Get all used variables by outgoing transition
	bdd vars = bddtrue;
	for (it->first(); !it->done(); it->next())
	  vars &= bdd_support(it->current_condition());

	// Fill a row in the matrix
	for (it->first(); !it->done(); it->next())
	  {
	    bdd cond = it->current_condition();

	    unsigned count = 0;
	    while (cond != bddfalse)
	      {
		cond -= bdd_satoneset(cond, vars, bddtrue);
		++count;
	      }
	    state_explicit_number* dest =
	      down_cast<state_explicit_number*>(it->current_state());
	    a(s - 1, dest->label() - 1) = count;
	  }
	delete it;

        unsigned nc = bdd_nodecount(vars);

	// Fix the row
	if (nc > 0)
	  {
	    double total = pow(2, nc);
	    for (unsigned d = 0; d < ns; ++d)
	      a(s - 1, d) /= total;
	  }
      }

    //std::cout << a << "\n";

    arma::mat ak(ns, ns, arma::fill::eye);
    arma::mat as(ns, ns, arma::fill::zeros);

    for (unsigned k = 1; k <= iter; ++k)
      {
	ak *= a;
	//std::cout << "A" << k << "\n" << ak << "\n";
	as += ak;
	//std::cout << "AS\n" << as << "\n";
      }

    arma::mat i(1, ns, arma::fill::zeros);
    i(0, down_cast<state_explicit_number*>(det->get_init_state())->label() - 1)
      = 1.0;

    //std::cout << "i: " << i;

    i *= as;
    i /= iter;

    //std::cout << "i: " << i;

    double result = 0.0;
    power_map::power_map_data::const_iterator it;

    for (it = pm.map_.begin(); it != pm.map_.end(); ++it)
      {
	// power_map::power_state::const_iterator j;
	// std::cerr << i(it->first - 1) << " { ";
	// for (j = it->second.begin(); j != it->second.end(); ++j)
	//   std::cerr << aut->format_state(*j) << " ";
	// std::cerr << "}\n";
	result += i(it->first - 1) * it->second.size();
      }
    delete det;
    return result;
  }
}
