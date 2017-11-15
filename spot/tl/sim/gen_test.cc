// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016, 2017 Laboratoire de Recherche et Développement de
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



#include <spot/tl/sim/tree.hh>
#include <spot/tl/relabel.hh>

void generate_simple(spot::formula f, std::ostream& os)
{
  spot::relabeling_map m;
  str_map m2;
  spot::relabel(f, spot::Pnn, &m);
  for (auto& i: m)
  {
    std::string fir;
    auto name = i.second.ap_name();
    switch (name[0])
    {
      case 'e':
        fir = "F" + name;
        break;
      case 'u':
        fir = "G" + name;
        break;
      case 'q':
        fir = "GF" + name;
        break;
      //case '.':
      case 'b':
      default:
        fir = name;
    }
    m2.emplace(name, fir);
  }
  os << sar(spot::str_psl(chg_ap_name(f, m2)), "\"", "") << '\n';
}

