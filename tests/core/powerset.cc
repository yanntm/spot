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

#include <iostream>
#include <set>

#include "spot/misc/powerset.hh"

int main()
{
  std::set<int> s;
  for (unsigned i = 0; i <= 4; ++i)
    {
      std::cout << "==P({";
      if (!s.empty())
        {
          auto it = s.begin();
          std::cout << *it;
          for (++it; it != s.end(); ++it)
            std::cout << ", " << *it;
        }
      std::cout << "})==\n";
      for (auto subset: spot::make_powerset(s))
        {
          std::cout << "{ ";
          if (!subset.empty())
            {
              auto it = subset.begin();
              std::cout << *it;
              for (++it; it != subset.end(); ++it)
                std::cout << ", " << *it;
            }
          std::cout << " }\n";
        }
      s.insert(i);
    }
}
