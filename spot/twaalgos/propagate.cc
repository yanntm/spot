// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita.
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

#include <spot/twaalgos/propagate.hh>

#include <spot/twaalgos/sccinfo.hh>

namespace spot
{
  twa_graph_ptr
  propagate_acc(const const_twa_graph_ptr& a)
  {
    if (!a->is_existential())
      throw std::runtime_error("propagate_acc does not support alternation");

    scc_info scc = scc_info(a);
    // Marks that are common to all incoming or outgoing transitions
    const auto& allaccs = a->acc().all_sets();

    auto res = make_twa_graph(a, {true, true, true, true, true, true});

    bool cont;
    std::vector<acc_cond::mark_t> common(res->num_states(), 0U);
    do
      {
        std::vector<acc_cond::mark_t> newcommonin(res->num_states(), allaccs);
        std::vector<acc_cond::mark_t> newcommonout(res->num_states(), allaccs);

        for (auto& e : res->edges())
          {
            auto acc = e.acc;
            if (scc.scc_of(e.src) == scc.scc_of(e.dst) && e.src != e.dst)
              {
                acc |= common[e.src];
                acc |= common[e.dst];

                newcommonin[e.dst] &= acc;
                newcommonout[e.src] &= acc;
              }
            e.acc = acc;
          }

        for (unsigned s = 0; s != res->num_states(); ++s)
          newcommonout[s] |= newcommonin[s];
        cont = common != newcommonout;
        common = newcommonout;
      }
    while (cont);

    return res;
  }
}
